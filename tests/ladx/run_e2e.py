"""LADX's e2e driver: runs the C generator (ladx_gen) on every case of a directory and compares,
per case, the golden's and C's stage projection of run.txt at the stage claims.txt names.
Contract in tests/ladx/README.md, "Stage projection" and "E2E driver and claims". Standard
library only; run with python3 -I -B.
"""

import argparse
import concurrent.futures
import io
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TIMEOUT = 120

STAGES = ("init", "ws", "pool", "fill")
STAGE_TRACE = {
    "init": {"randomizer:Randomizer.__init__"},
    "ws": {"worldSetup:WorldSetup.randomize", "patches.maze:buildMaze"},
    "pool": {"itempool:ItemPool.__randomizeRupees"},
    "fill": {"randomizer:RandomItemPlacer.run", "randomizer:RandomItemPlacer.placeSpecificItem",
             "randomizer:RandomItemPlacer.__placeItem"},
}
STAGE_SECTIONS = {
    "init": ("seed", "settings"),
    "ws": ("ws",),
    "pool": ("words_before", "pool", "words_after", "placer_pool"),
    "fill": ("place",),
}
# A stage is complete once its marker's section is present; until then LADXR stopped in it.
STAGE_MARKER = {"init": "ws", "ws": "words_before", "pool": "placer_pool", "fill": "place"}
OUT_OF_STAGE = ("rom_sha1", "spoiler_sha1", "rom", "spoiler")


class FormatError(Exception):
    pass


class Usage(Exception):
    pass


def _upto(stage, table):
    out = []
    for s in STAGES[:STAGES.index(stage) + 1]:
        out.extend(table[s])
    return out


def project(lines, stage):
    """The [(section, line)] projection of run.txt's lines at stage; FormatError when the
    text is malformed or a fill-stage trace line follows the fill-stage prefix."""
    sections = set(_upto(stage, STAGE_SECTIONS))
    allowed = set(_upto(stage, STAGE_TRACE))
    fill = set(_upto("fill", STAGE_TRACE))
    known = set(_upto("fill", STAGE_SECTIONS)) | {"trace", "outcome"} | set(OUT_OF_STAGE)
    present = set()
    out = []
    in_prefix = True
    fill_prefix = True
    outcome = None
    for n, line in enumerate(lines, 1):
        kind = line.split(" ", 1)[0]
        if kind not in known:
            raise FormatError("line %d: unknown line kind %r" % (n, kind))
        present.add(kind)
        if kind == "outcome":
            if outcome is not None:
                raise FormatError("line %d: a second outcome line" % n)
            outcome = line
        elif kind == "trace":
            fields = line.split(" ")
            if len(fields) < 5:
                raise FormatError("line %d: malformed trace line" % n)
            where = fields[3]
            if fill_prefix and where not in fill:
                fill_prefix = False
            elif not fill_prefix and where in fill:
                raise FormatError("line %d: fill-stage trace line after the fill-stage prefix" % n)
            if in_prefix and where not in allowed:
                in_prefix = False
            if in_prefix:
                out.append(("trace", line))
        elif kind in sections:
            out.append((kind, line))
    if outcome is not None and STAGE_MARKER[stage] not in present:
        out.append(("outcome", outcome))
    return out


def read_lines(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read().splitlines()


def read_claims(path):
    """The claimed stage; Usage on a malformed file."""
    try:
        lines = read_lines(path)
    except OSError as e:
        raise Usage("cannot read claims %s: %s" % (path, e.strerror))
    stage = None
    for n, line in enumerate(lines, 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        fields = text.split()
        if fields[0] != "stage" or len(fields) != 2:
            raise Usage("%s:%d: malformed line" % (path, n))
        if fields[1] not in STAGES:
            raise Usage("%s:%d: unknown stage %r" % (path, n, fields[1]))
        if stage is not None:
            raise Usage("%s:%d: a second stage line" % (path, n))
        stage = fields[1]
    if stage is None:
        raise Usage("%s: no stage line" % path)
    return stage


def compare(golden, c):
    """None when equal, else the detail lines of the first difference."""
    for i in range(max(len(golden), len(c))):
        g = golden[i] if i < len(golden) else None
        x = c[i] if i < len(c) else None
        if g != x:
            section = (g or x)[0]
            return ["%s: line %d:" % (section, i + 1),
                    "  golden: %s" % (g[1] if g else "<end>"),
                    "  c: %s" % (x[1] if x else "<end>")]
    return None


def run_case(gen, case_dir, stage, golden):
    """The detail lines of a DIVERGENT case, or None when it passes."""
    if not os.path.isfile(gen) or not os.access(gen, os.X_OK):
        return ["no C generator: %s" % gen]
    with tempfile.TemporaryDirectory(prefix="ladx-e2e-") as out_dir:
        try:
            proc = subprocess.run([gen, case_dir, out_dir], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            return ["c generator timed out after %d s" % TIMEOUT]
        if proc.returncode != 0:
            err = proc.stderr.decode("utf-8", "replace").splitlines()[:5]
            return ["c generator exited %d" % proc.returncode] + ["  " + line for line in err]
        path = os.path.join(out_dir, "run.txt")
        if not os.path.isfile(path):
            return ["c wrote no run.txt"]
        try:
            c = project(read_lines(path), stage)
        except FormatError as e:
            return ["c run.txt malformed: %s" % e]
    return compare(golden, c)


def list_cases(cases_dir, names):
    if not os.path.isdir(cases_dir):
        raise Usage("no cases directory: %s" % cases_dir)
    found = sorted(d for d in os.listdir(cases_dir) if os.path.isdir(os.path.join(cases_dir, d)))
    if not names:
        return found
    for name in names:
        if name not in found:
            raise Usage("unknown case: %s" % name)
    return sorted(set(names))


def run(cases_dir, claims, gen, names, out):
    """Prints the verdicts to out; returns the exit code."""
    try:
        stage = read_claims(claims)
        cases = list_cases(cases_dir, names)
        goldens = {}
        for case in cases:
            path = os.path.join(cases_dir, case, "run.txt")
            try:
                goldens[case] = project(read_lines(path), stage)
            except OSError:
                raise Usage("%s: no golden run.txt" % case)
            except FormatError as e:
                raise Usage("%s: golden run.txt: %s" % (case, e))
    except Usage as e:
        print("run_e2e: %s" % e, file=out)
        return 2
    if not cases:
        print("e2e: no cases", file=out)
        return 2
    workers = min(len(cases), os.cpu_count() or 1)
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        results = list(pool.map(lambda c: run_case(gen, os.path.join(cases_dir, c), stage, goldens[c]), cases))
    divergent = 0
    for case, detail in zip(cases, results):
        if detail is None:
            print("PASS %s (%s)" % (case, stage), file=out)
            continue
        divergent += 1
        print("DIVERGENT %s" % case, file=out)
        for line in detail:
            print("  " + line, file=out)
    print("e2e: %d pass, %d divergent" % (len(cases) - divergent, divergent), file=out)
    return 1 if divergent else 0


# --- self-test -------------------------------------------------------------------------------

def _trace_where(line):
    return line.split(" ")[3] if line.startswith("trace ") else None


def _replace(lines, index, old, new):
    out = list(lines)
    if old not in out[index]:
        raise AssertionError("self-test: %r not in line %d" % (old, index))
    out[index] = out[index].replace(old, new, 1)
    return out


def _planted(golden):
    """[(name, stage, golden lines, c lines or None, gen kind, expected)], expected 'PASS',
    'DIVERGENT' or 'exit 2'."""
    fill = set(_upto("fill", STAGE_TRACE))
    first = {}
    for i, line in enumerate(golden):
        first.setdefault(line.split(" ", 1)[0], i)
    traces = [i for i, line in enumerate(golden) if line.startswith("trace ")]
    fill_end = next(i for i in traces if _trace_where(golden[i]) not in fill)
    last_fill = fill_end - 1
    rom_line = fill_end
    place = first["place"]
    ws = first["ws"]
    init_only = [line for line in golden if line.split(" ", 1)[0] in ("seed", "settings")]
    init_only += [golden[i] for i in traces[:1]]
    no_place = [line for line in golden[:fill_end]
                if not line.startswith("place ")] + ["outcome error Failed to place an item for a bunch of retries"]
    extra = golden[:fill_end] + [golden[last_fill].replace("trace ", "trace 9", 1)] + golden[fill_end:]

    def field_change(line):
        head, _, tail = line.rpartition(" ")
        return head + " [999]" if tail.startswith("[") else line + "x"

    return [
        ("identical", "fill", golden, golden, "copy", "PASS"),
        ("changed place line", "fill", golden, _replace(golden, place, " ", "  "), "copy", "DIVERGENT"),
        ("changed fill-stage trace line", "fill", golden,
         golden[:last_fill] + [field_change(golden[last_fill])] + golden[fill_end:], "copy", "DIVERGENT"),
        ("c trace one line short", "fill", golden, golden[:last_fill] + golden[fill_end:], "copy", "DIVERGENT"),
        ("one extra c fill-stage trace line", "fill", golden, extra, "copy", "DIVERGENT"),
        ("changed ws line", "fill", golden, _replace(golden, ws, " ", "  "), "copy", "DIVERGENT"),
        ("non-zero exit", "fill", golden, golden, "fail", "DIVERGENT"),
        ("no run.txt", "fill", golden, None, "copy", "DIVERGENT"),
        ("no generator", "fill", golden, golden, "missing", "DIVERGENT"),
        ("changed generateRom trace line", "fill", golden,
         golden[:rom_line] + [field_change(golden[rom_line])] + golden[rom_line + 1:], "copy", "PASS"),
        ("changed rom_sha1", "fill", golden, [l + "0" if l.startswith("rom_sha1 ") else l for l in golden],
         "copy", "PASS"),
        ("changed spoiler_sha1", "fill", golden,
         [l + "0" if l.startswith("spoiler_sha1 ") else l for l in golden], "copy", "PASS"),
        ("claim at ws, changed place line", "ws", golden, _replace(golden, place, " ", "  "), "copy", "PASS"),
        ("claim at init, c stops after the seeding", "init", golden, init_only, "copy", "PASS"),
        ("claim at init, c outcome ok with no ws line", "init", golden, init_only + ["outcome ok"], "copy",
         "DIVERGENT"),
        ("golden outcome error and no place, c ok with places", "fill", no_place, golden, "copy", "DIVERGENT"),
        ("golden fill-stage line after a generateRom line", "fill", golden + [golden[last_fill]], golden, "copy",
         "exit 2"),
        ("unknown stage claimed", "filled", golden, golden, "copy", "exit 2"),
    ]


def _verdict(code, text):
    if code == 2:
        return "exit 2"
    if code == 0 and "\nPASS " in "\n" + text and "DIVERGENT" not in text:
        return "PASS"
    if code == 1 and "\nDIVERGENT " in "\n" + text:
        return "DIVERGENT"
    return "exit %d" % code


def self_test():
    golden = read_lines(os.path.join(HERE, "cases", "baseline", "run.txt"))
    config = os.path.join(HERE, "cases", "baseline", "config.txt")
    failed = []
    planted = _planted(golden)
    with tempfile.TemporaryDirectory(prefix="ladx-e2e-self-") as tmp:
        for k, (name, stage, gold, c, kind, expected) in enumerate(planted):
            root = os.path.join(tmp, str(k))
            case = os.path.join(root, "cases", "planted")
            os.makedirs(case)
            shutil.copy(config, case)
            with open(os.path.join(case, "run.txt"), "w") as f:
                f.write("".join(line + "\n" for line in gold))
            planted_c = os.path.join(root, "c.txt")
            if c is not None:
                with open(planted_c, "w") as f:
                    f.write("".join(line + "\n" for line in c))
            gen = os.path.join(root, "gen")
            with open(gen, "w") as f:
                f.write("#!%s\nimport os, shutil, sys\n" % sys.executable)
                if kind == "fail":
                    f.write("sys.stderr.write('planted failure\\n')\nsys.exit(3)\n")
                elif c is not None:
                    f.write("shutil.copy(%r, os.path.join(sys.argv[2], 'run.txt'))\n" % planted_c)
            os.chmod(gen, 0o755)
            if kind == "missing":
                gen = os.path.join(root, "no-such-gen")
            claims = os.path.join(root, "claims.txt")
            with open(claims, "w") as f:
                f.write("# planted\nstage %s\n" % stage)
            out = io.StringIO()
            code = run(os.path.join(root, "cases"), claims, gen, [], out)
            got = _verdict(code, out.getvalue())
            if got != expected:
                failed.append("%s (expected %s, got %s)" % (name, expected, got))
    for case in failed:
        print("e2e: self-test failed: %s" % case)
    if failed:
        return 1
    print("e2e: self-test, %d planted runs as expected" % len(planted))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(prog="run_e2e.py")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--cases", default=os.path.join(HERE, "cases"))
    parser.add_argument("--claims", default=os.path.join(HERE, "claims.txt"))
    parser.add_argument("--gen", default=os.path.join(HERE, "build", "ladx_gen"))
    parser.add_argument("case", nargs="*")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if args.self_test:
        return self_test()
    return run(args.cases, args.claims, os.path.abspath(args.gen), args.case, sys.stdout)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
