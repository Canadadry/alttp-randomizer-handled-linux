"""Usage: python3 gen_fill_outcomes.py [--ladxr <dir>] [--out <file>] [--check]

Writes fill_outcomes.txt (default tests/ladx/oracle/fill_outcomes.txt): LADXR's fill projection
(run_e2e.py's, stage fill) on seeds that are not case seeds, through oracle_run.py's driver in
SKIP_ROM_GENERATION mode (no ROM), PYTHONHASHSEED=0. --check renders into
tmp/ladx-fill-outcomes/ with PYTHONHASHSEED=1 on the same checkout and compares. Contract in
tests/ladx/README.md, "Fill-outcome fixture". Run python3 without -I or -E (both drop
PYTHONHASHSEED); needs the LADXR checkout at the pin and python3 >= 3.11, else a `setup:` line.
"""

import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.dirname(HERE))

import ladx_cases  # noqa: E402
import ladx_oracle  # noqa: E402
import run_e2e  # noqa: E402

TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
FIXTURE = os.path.join(HERE, "fill_outcomes.txt")
CHECK_OUT = os.path.join(TOP, "tmp", "ladx-fill-outcomes", "fill_outcomes.txt")
LADXR_PIN = "4325207e4e55076e05f209fbdf16b860a90e42ef"
REFUSED_CANDIDATES = 8
INPUTS = ("tier1.txt", "settings.txt", "settings_validate.txt", "combos.txt")

HEADER = """\
# LADXR's fill outcomes on seeds that are not case seeds; written by gen_fill_outcomes.py, never
# by hand. Runs, in order: for every case and combo (tests/ladx/cases/ then combos/, in the
# expected order), every candidate derive(<name>, k) below the case's own k (the candidates
# ladx_cases.py pick refused); candidates k = 0..%d of each refused single; then the lines of
# determinism_cases.txt whose values are all tier-1 values, labelled det-<name>. Each runs
# LADXR's Randomizer in SKIP_ROM_GENERATION mode (oracle_run.py's driver, no ROM). One line each:
#   <label> <k or -> <seed hex> <ok|error|crash> <lines> <digest> [<k=v> ...]
# <digest>: the first 16 bytes, in hex, of SHA-512 of the run's fill projection (run_e2e.py's
# project at stage fill: lines joined by \\n, final newline); <lines>: its line count.
"""


class SetupError(Exception):
    pass


def projection_text(lines):
    return "".join(line + "\n" for _, line in run_e2e.project(lines, "fill"))


def outcome_kind(lines):
    last = lines[-1] if lines else ""
    kind = last.split(" ")[1] if last.startswith("outcome ") else None
    if kind not in ("ok", "error", "crash"):
        raise ladx_oracle.OracleError("fill-outcomes: no outcome line: %r" % last)
    return kind


def fixture_line(label, k, seed, assignments, lines):
    text = projection_text(lines)
    digest = hashlib.sha512(text.encode("utf-8")).hexdigest()[:32]
    return " ".join([label, "-" if k is None else str(k), seed.hex(), outcome_kind(lines),
                     str(text.count("\n")), digest] + list(assignments))


def _read(directory, name):
    with open(os.path.join(directory, name), encoding="utf-8") as f:
        return f.read()


def runs(root, inputs):
    texts = [_read(inputs, name) for name in INPUTS]
    e = ladx_cases.expected_list(*texts)
    problems = ladx_cases.mismatches(e, root)
    if problems:
        raise ladx_oracle.OracleError("fill-outcomes: the case tree is not the expected list: "
                                      + "; ".join(problems))
    out = []
    for line in ladx_cases.listing(e, root):
        name, seed_hex, *assignments = line.split(" ")
        k = ladx_cases.seed_index(name, seed_hex + "\n")
        out += [(name, j, ladx_cases.derive(name, j), assignments) for j in range(k)]
    for name, assignments, _ in e.refused:
        out += [(name, j, ladx_cases.derive(name, j), assignments) for j in range(REFUSED_CANDIDATES)]
    tier1 = dict(ladx_oracle.read_tier1(texts[0], ladx_cases.read_settings(texts[1])))
    for line in _read(inputs, "determinism_cases.txt").splitlines():
        if not line or line.startswith("#"):
            continue
        name, seed_hex, *assignments = line.split(" ")
        if all(v in tier1.get(k, ()) for k, _, v in (a.partition("=") for a in assignments)):
            out.append(("det-" + name, None, bytes.fromhex(seed_hex), assignments))
    return out


def render(ladxr, root, inputs):
    import oracle_run
    plan = runs(root, inputs)
    driver = oracle_run.Driver(ladxr)
    try:
        lines = [fixture_line(label, k, seed, a, driver.run(seed, a)) for label, k, seed, a in plan]
    finally:
        driver.close()
    return HEADER % (REFUSED_CANDIDATES - 1) + "".join(line + "\n" for line in lines)


def compare(reference, got):
    if reference == got:
        return []
    a, b = reference.split("\n"), got.split("\n")
    out = []
    for i in range(max(len(a), len(b))):
        x = a[i] if i < len(a) else None
        y = b[i] if i < len(b) else None
        if x != y:
            where = " ".join((x if x is not None else y).split(" ")[:2])
            out.append("DIFFERENT line %d (%s): committed %s, regenerated %s"
                       % (i + 1, where, "<end>" if x is None else x, "<end>" if y is None else y))
    return out


def check_setup(ladxr):
    if sys.version_info < (3, 11):
        raise SetupError("python3 is older than 3.11 (run tests/ladx/oracle/setup.sh)")
    try:
        head = subprocess.run(["git", "-C", ladxr, "rev-parse", "HEAD"], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, check=False).stdout.decode().strip()
    except OSError:
        head = ""
    if not os.path.isfile(os.path.join(ladxr, "randomizer.py")) or head != LADXR_PIN:
        raise SetupError("no LADXR checkout at the pin %s in %s (run tests/ladx/oracle/setup.sh)"
                         % (LADXR_PIN[:8], ladxr))


def _worker(ladxr, out, hashseed):
    done = subprocess.run([sys.executable, os.path.abspath(__file__), "--render", ladxr, out],
                          env=ladx_oracle.oracle_env(hashseed), stdin=subprocess.DEVNULL, check=False)
    if done.returncode != 0:
        raise ladx_oracle.OracleError("fill-outcomes: the render exited %d" % done.returncode)


def main(argv):
    if argv[:1] == ["--render"] and len(argv) == 3:
        text = render(argv[1], os.path.dirname(HERE), HERE)
        os.makedirs(os.path.dirname(os.path.abspath(argv[2])), exist_ok=True)
        with open(argv[2], "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        return 0
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
        check_setup(ladxr)
    except SetupError as e:
        print("setup: fill-outcomes: %s" % e, file=sys.stderr)
        return 2
    if not check:
        _worker(ladxr, os.path.abspath(options["--out"]), 0)
        with open(options["--out"], encoding="utf-8") as f:
            n = sum(1 for line in f if not line.startswith("#"))
        print("fill-outcomes: %d runs written to %s" % (n, os.path.relpath(options["--out"], TOP)))
        return 0
    reference = os.path.abspath(options["--out"])
    shown = os.path.relpath(reference, TOP)
    if os.path.exists(CHECK_OUT):
        os.remove(CHECK_OUT)
    _worker(ladxr, CHECK_OUT, 1)
    if not os.path.isfile(reference):
        print("MISSING %s" % shown)
        return 1
    with open(reference, encoding="utf-8") as f:
        ref = f.read()
    with open(CHECK_OUT, encoding="utf-8") as f:
        got = f.read()
    lines = compare(ref, got)
    for line in lines:
        print(line)
    if lines:
        return 1
    print("fill-outcomes: %d runs byte-identical to %s"
          % (sum(1 for line in ref.splitlines() if not line.startswith("#")), shown))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ladx_oracle.OracleError as e:
        sys.exit(str(e))
