"""Usage: python3 tests/ladx/oracle/gen_header_vectors.py [--ladxr <dir>] [--out <file>] [--check]

Writes header_vectors.txt (default tests/ladx/oracle/header_vectors.txt): LADXR's own
ROM.fixHeader (rom.py at the pin) on synthetic banks, for C's ladx_header_fix to replay. No ROM,
no network; the LADXR checkout at the pin. --check renders it again in memory and compares with
the file. Contract in tests/ladx/README.md, "Header fixture"; the format is in HEADER below.
"""

import io
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gen_fill_outcomes  # noqa: E402
import gen_store_vectors  # noqa: E402

TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
FIXTURE = os.path.join(HERE, "header_vectors.txt")
VERSION_LINE = "ladx-header-vectors 1"
BANK = 0x4000
BANKS = 0x40
SIZE = BANK * BANKS
RANDOM_SEED = 0x4EAD5E7
RANDOM_VECTORS = 40

HEADER = """\
# LADXR's ROM.fixHeader (rom.py at the pin) on synthetic banks; written by gen_header_vectors.py,
# never by hand. No byte of any real ROM: every image is 64 zero banks of 0x4000 bytes with
# synthetic fills and plants, hand-written or drawn from random.Random. Lines, hex lowercase,
# offsets into the 1 MiB image:
#   ladx-header-vectors 1
#   V <name>                     a vector: hand-written ones, then random-<nn>
#   F <offset> <length> <byte>   a fill of the zero image, in order
#   P <offset> <hex>             planted bytes, after the fills, in order
#   name <hex>|-                 fixHeader's name as UTF-8 bytes, or None
#   W <offset> <hex>             every assignment fixHeader made to bank 0, in order
#   end                          the vector's last line
"""

FixtureError = gen_store_vectors.FixtureError


class Vector:
    def __init__(self, name, fills=(), plants=(), title="LADXR"):
        self.name, self.fills, self.plants, self.title = name, list(fills), list(plants), title


def image(vector):
    data = bytearray(SIZE)
    for offset, length, value in vector.fills:
        data[offset:offset + length] = bytes([value]) * length
    for offset, data_bytes in vector.plants:
        data[offset:offset + len(data_bytes)] = data_bytes
    if len(data) != SIZE:
        raise FixtureError("%s: a plant past the image" % vector.name)
    return bytes(data)


def run_vector(rom_class, vector):
    """The vector's lines (without V), LADXR's own fixHeader on the vector's image."""
    rom = rom_class(io.BytesIO(image(vector)))
    banks = []
    for n, bank in enumerate(rom.banks):
        recording = gen_store_vectors.RecBank(bank)
        recording.number = n
        banks.append(recording)
    rom.banks = banks
    log = []
    gen_store_vectors.RecBank.log = log
    try:
        rom.fixHeader(name=vector.title)
    finally:
        gen_store_vectors.RecBank.log = None
    lines = ["F %x %x %02x" % fill for fill in vector.fills]
    lines += ["P %x %s" % (offset, data.hex()) for offset, data in vector.plants]
    lines.append("name %s" % ("-" if vector.title is None else vector.title.encode("utf-8").hex()))
    for bank, addr, data in log:
        if bank != 0:
            raise FixtureError("%s: fixHeader wrote bank %d" % (vector.name, bank))
        lines.append("W %x %s" % (addr, data.hex()))
    return lines + ["end"]


def hand_vectors():
    return [
        Vector("zero"),
        Vector("no-name", plants=[(0x134, b"\x51\x52\x53\x01\x02\x03\x04")], title=None),
        Vector("empty-name", plants=[(0x134, b"\x11" * 15)], title=""),
        Vector("long-name", title="ABCDEFGHIJKLMNOPQRST"),
        # 14 ASCII bytes then a two-byte character: the cut keeps its first byte.
        Vector("utf8-cut", title="ABCDEFGHIJKLMNé"),
        Vector("all-ff", fills=[(0, SIZE, 0xFF)]),
        # the old checksum bytes are zeroed before the sum; the new 0x14D is counted.
        Vector("old-checksums", plants=[(0x14D, b"\x77\x88\x99")]),
        Vector("header-wraps", plants=[(0x143, b"\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6")]),
        Vector("last-byte", plants=[(SIZE - 1, b"\x01")]),
        Vector("sum-wraps", fills=[(0x4000, 0x8000, 0x81), (0xF8000, 0x8000, 0x3C)]),
    ]


def random_vector(r, n):
    fills, plants = [], []
    for _ in range(r.randint(0, 5)):
        offset = r.randrange(SIZE)
        fills.append((offset, r.randint(1, SIZE - offset), r.randrange(256)))
    for _ in range(r.randint(1, 6)):
        length = r.randint(1, 6)
        if r.random() < 0.5:
            offset = r.randrange(0x130, 0x150)
        else:
            offset = r.randrange(SIZE - length)
        plants.append((offset, bytes(r.randrange(256) for _ in range(length))))
    roll = r.random()
    if roll < 0.1:
        title = None
    elif roll < 0.4:
        title = "LADXR"
    else:
        title = "".join(r.choice("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ") for _ in range(r.randint(0, 18)))
    return Vector("random-%02d" % n, fills, plants, title)


def vectors():
    r = random.Random(RANDOM_SEED)
    return hand_vectors() + [random_vector(r, n) for n in range(RANDOM_VECTORS)]


def render(rom_class):
    out = [HEADER, VERSION_LINE + "\n"]
    for vector in vectors():
        out.append("V %s\n" % vector.name)
        out += [line + "\n" for line in run_vector(rom_class, vector)]
    return "".join(out)


def parse(text):
    """[(name, [line])] of a fixture's vectors; FixtureError on a malformed text."""
    lines = [line for line in text.split("\n") if line and not line.startswith("#")]
    if not lines or lines[0] != VERSION_LINE:
        raise FixtureError("no version line")
    out, current = [], None
    for line in lines[1:]:
        if line.startswith("V "):
            if current is not None:
                raise FixtureError("vector %s has no end" % current[0])
            current = (line[2:], [])
        elif current is None:
            raise FixtureError("a line outside a vector: %s" % line)
        elif line == "end":
            if sum(1 for row in current[1] if row.startswith("name ")) != 1:
                raise FixtureError("vector %s has not one name line" % current[0])
            out.append(current)
            current = None
        else:
            current[1].append(line)
    if current is not None:
        raise FixtureError("vector %s has no end" % current[0])
    return out


def load_rom_class(ladxr):
    sys.path.insert(0, ladxr)
    import rom
    return rom.ROM


def main(argv):
    options = {"--ladxr": os.environ.get("LADXR_DIR", os.path.join(HERE, "ladxr")), "--out": FIXTURE}
    check = False
    while argv:
        if argv[0] == "--check":
            check, argv = True, argv[1:]
        elif argv[0] in options and len(argv) >= 2:
            options[argv[0]], argv = argv[1], argv[2:]
        else:
            print(__doc__.splitlines()[0], file=sys.stderr)
            return 2
    ladxr = os.path.abspath(options["--ladxr"])
    try:
        gen_fill_outcomes.check_setup(ladxr)
    except gen_fill_outcomes.SetupError as e:
        print("setup: header-vectors: %s" % e, file=sys.stderr)
        return 2
    text = render(load_rom_class(ladxr))
    out = os.path.abspath(options["--out"])
    shown = os.path.relpath(out, TOP)
    count = len(parse(text))
    if not check:
        with open(out, "w", encoding="ascii", newline="\n") as f:
            f.write(text)
        print("header-vectors: %d vectors written to %s" % (count, shown))
        return 0
    if not os.path.isfile(out):
        print("MISSING %s" % shown)
        return 1
    with open(out, encoding="ascii") as f:
        committed = f.read()
    lines = gen_store_vectors.compare(committed, text)
    for line in lines[:20]:
        print(line)
    if lines:
        print("header-vectors: %d lines differ from %s" % (len(lines), shown))
        return 1
    print("header-vectors: %d vectors byte-identical to %s" % (count, shown))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
