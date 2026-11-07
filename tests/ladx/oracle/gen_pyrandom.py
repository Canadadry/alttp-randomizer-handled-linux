"""Usage: python3 gen_pyrandom.py <ladxr dir> <out dir>

Writes <out dir>/pyrandom_vectors.txt: CPython random.Random values for every method LADXR calls.
Plain random.Random, no LADXR (<ladxr dir> is taken for gen_fixtures.sh's uniform call only).
"""

import os
import random
import sys

import ladx_oracle
from ladx_oracle import render, render_list, seed_hex

SEEDS = (b"", b"\x00", b"\x01", bytes(range(16)), b"\xff" * 16, b"LADXR", bytes(range(64)),
         bytes(n % 256 for n in range(300)))
BITS = (1, 31, 32, 33, 64)
RANGES = (1, 2, 3, 5, 17, 100, 2**31, 2**31 + 1, 2**32, 2**32 + 1)
EXTRA_RANGES = ((-1, 9), (4, 37))
CHOICE_LENGTHS = (1, 2, 7, 64, 300)
SHUFFLE_LENGTHS = (0, 1, 2, 9, 64)
WEIGHTS = ((10, 5, 10, 10, 1), (20, 20, 20, 50, 50, 20, 10, 5, 5, 20, 10, 10), (1, 1), (3, 17), (9, 4),
           (1.0,), (1.0, 1.0), (1.0,) * 7, (1.0,) * 40)
SCRIPTS = (
    "random;getrandbits:33;randrange:0:100;randint:-1:8;choice:7;shuffle:9;choices:10,5,10,10,1:1;"
    "uniform:0:100;getrandbits:1;random",
    "getstate;randrange:0:4294967297;choice:300;random;setstate;shuffle:64;"
    "choices:20,20,20,50,50,20,10,5,5,20,10,10:3;randint:0:2147483647;getrandbits:64",
    "choices:1.0,1.0,1.0:2;randint:4:36;shuffle:2;getstate;uniform:0:100;choice:1;randrange:0:1;"
    "setstate;getrandbits:32;choice:64;randrange:-1:9;shuffle:0;random",
)
FIRST = 8

HEADER = """\
# CPython random.Random vectors for every method LADXR calls; written by gen_pyrandom.py
# (gen_fixtures.sh). Each line starts a new generator: random.Random(<seed>), seeded from bytes
# (version 2, SHA-512). <seed>: lower-case hex bytes, - for the empty seed. Integers in decimal,
# floats as float.hex, index lists comma-separated (- when empty), fields separated by one space.
#   words <seed> <first 16 getrandbits(32)>
#   random <seed> <first 8 random()>
#   getrandbits <seed> <k> <first 4 getrandbits(k)>
#   randrange <seed> <start> <stop> <first 8 randrange(start, stop)>
#   randint <seed> <a> <b> <first 8 randint(a, b)>
#   choice <seed> <len> <first 8 indexes picked by choice(range(len))>
#   shuffle <seed> <n> <list(range(n)) after one shuffle>
#   choices <seed> <weights> <indexes picked by choices(range(len(weights)), weights, k=8)>
#   uniform <seed> <a> <b> <first 8 uniform(a, b)>
#   script <seed> <op;op;...> -> <result;result;...> <words>
# A script runs its ops in order on one generator: random, getrandbits:<k>, randrange:<start>:<stop>,
# randint:<a>:<b>, choice:<len>, shuffle:<n>, choices:<weights>:<k>, uniform:<a>:<b>, getstate
# (saves the state, result -), setstate (restores the last saved state, result -); an op's weights
# are decimal, a float written with a point. <words>: the MT position at the end, as the number of
# getrandbits(32) that take a newly seeded generator there.
"""


def number(text):
    return float(text) if "." in text else int(text)


def run_op(rng, op, saved):
    name, *args = op.split(":")
    if name == "random":
        return render(rng.random())
    if name == "getrandbits":
        return render(rng.getrandbits(int(args[0])))
    if name == "randrange":
        return render(rng.randrange(int(args[0]), int(args[1])))
    if name == "randint":
        return render(rng.randint(int(args[0]), int(args[1])))
    if name == "choice":
        return render(rng.choice(range(int(args[0]))))
    if name == "shuffle":
        perm = list(range(int(args[0])))
        rng.shuffle(perm)
        return render_list(perm)
    if name == "choices":
        weights = [number(w) for w in args[0].split(",")]
        return render_list(rng.choices(range(len(weights)), weights, k=int(args[1])))
    if name == "uniform":
        return render(rng.uniform(number(args[0]), number(args[1])))
    if name == "getstate":
        saved[:] = [rng.getstate()]
        return "-"
    if name == "setstate":
        rng.setstate(saved[0])
        return "-"
    raise ladx_oracle.OracleError("gen_pyrandom: unknown op %r" % op)


def position(seed, state):
    rng = random.Random(seed)
    for words in range(100000):
        if rng.getstate() == state:
            return words
        rng.getrandbits(32)
    raise ladx_oracle.OracleError("gen_pyrandom: script state not found")


def lines():
    for seed in SEEDS:
        s = seed_hex(seed)
        rng = random.Random(seed)
        yield "words %s %s" % (s, " ".join(render(rng.getrandbits(32)) for _ in range(16)))
        rng = random.Random(seed)
        yield "random %s %s" % (s, " ".join(render(rng.random()) for _ in range(FIRST)))
        for k in BITS:
            rng = random.Random(seed)
            yield "getrandbits %s %d %s" % (s, k, " ".join(render(rng.getrandbits(k)) for _ in range(4)))
        bounds = [(0, n) for n in RANGES] + list(EXTRA_RANGES)
        for start, stop in bounds:
            rng = random.Random(seed)
            values = " ".join(render(rng.randrange(start, stop)) for _ in range(FIRST))
            yield "randrange %s %d %d %s" % (s, start, stop, values)
        for start, stop in bounds:
            rng = random.Random(seed)
            values = " ".join(render(rng.randint(start, stop - 1)) for _ in range(FIRST))
            yield "randint %s %d %d %s" % (s, start, stop - 1, values)
        for n in CHOICE_LENGTHS:
            rng = random.Random(seed)
            yield "choice %s %d %s" % (s, n, " ".join(render(rng.choice(range(n))) for _ in range(FIRST)))
        for n in SHUFFLE_LENGTHS:
            rng = random.Random(seed)
            perm = list(range(n))
            rng.shuffle(perm)
            yield "shuffle %s %d %s" % (s, n, render_list(perm))
        for weights in WEIGHTS:
            rng = random.Random(seed)
            picks = rng.choices(range(len(weights)), weights, k=FIRST)
            yield "choices %s %s %s" % (s, render_list(weights), render_list(picks))
        rng = random.Random(seed)
        yield "uniform %s 0 100 %s" % (s, " ".join(render(rng.uniform(0, 100)) for _ in range(FIRST)))
        for script in SCRIPTS:
            rng = random.Random(seed)
            saved = []
            results = [run_op(rng, op, saved) for op in script.split(";")]
            yield "script %s %s -> %s %d" % (s, script, ";".join(results), position(seed, rng.getstate()))


def main(argv):
    if len(argv) != 3:
        print("usage: gen_pyrandom.py <ladxr dir> <out dir>", file=sys.stderr)
        return 2
    with open(os.path.join(argv[2], "pyrandom_vectors.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write(HEADER)
        for line in lines():
            out.write(line + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
