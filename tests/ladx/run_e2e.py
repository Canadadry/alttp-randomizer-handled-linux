"""LADX's e2e driver: runs the C generator (ladx_gen) on every case of a directory and compares,
per case, the golden's and C's stage projection of run.txt at the stage claims.txt names, then
each claimed group's write-log projection (write_log.py's). --projections compares C's projection
of every golden (ladx_gen --project) with write_log.py's, group by group. Contract in
tests/ladx/README.md, "Stage projection" and "E2E driver and claims". Standard library only; run
with python3 -I -B.
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
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(HERE, "oracle"))
import write_log  # noqa: E402

TIMEOUT = 120
DETAIL_MAX = 200

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


def read_claims(path, inventory):
    """(stage, [group]) of claims.txt; Usage on a malformed file. The inventory (a path) is
    read only when a group is claimed."""
    try:
        lines = read_lines(path)
    except OSError as e:
        raise Usage("cannot read claims %s: %s" % (path, e.strerror))
    stage = None
    groups = []
    sides = None
    for n, line in enumerate(lines, 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        fields = text.split()
        if len(fields) != 2 or fields[0] not in ("stage", "group"):
            raise Usage("%s:%d: malformed line" % (path, n))
        if fields[0] == "group":
            if sides is None:
                sides = read_sides(inventory)
            if sides.get(fields[1]) not in ("table", "C") or fields[1] == "spoiler":
                raise Usage("%s:%d: not a claimable group %r" % (path, n, fields[1]))
            if fields[1] in groups:
                raise Usage("%s:%d: group %s claimed twice" % (path, n, fields[1]))
            groups.append(fields[1])
            continue
        if fields[1] not in STAGES:
            raise Usage("%s:%d: unknown stage %r" % (path, n, fields[1]))
        if stage is not None:
            raise Usage("%s:%d: a second stage line" % (path, n))
        stage = fields[1]
    if stage is None:
        raise Usage("%s: no stage line" % path)
    return stage, groups


def read_sides(inventory):
    try:
        with open(inventory, encoding="utf-8") as f:
            return write_log.sides_of(f.read())
    except OSError as e:
        raise Usage("cannot read inventory %s: %s" % (inventory, e.strerror))
    except write_log.WriteLogError as e:
        raise Usage("%s: %s" % (inventory, e))


def log_projections(text, groups):
    """{group: projection lines} of a write log's text for each of groups; WriteLogError when
    the log is malformed."""
    tables, segments = write_log.parse(text)
    net = {g: ({}, {}) for g in groups}
    for name, banks, entries in segments:
        if name in net:
            net[name][0].update(banks)
            net[name][1].update(entries)
    return {g: write_log.render(tables, [(g, b, e)] if b or e else []).splitlines()
            for g, (b, e) in net.items()}


def _cut(line):
    if len(line) <= DETAIL_MAX:
        return line
    return "%s ... (%d chars)" % (line[:DETAIL_MAX], len(line))


def compare(golden, c):
    """None when equal, else the detail lines of the first difference."""
    for i in range(max(len(golden), len(c))):
        g = golden[i] if i < len(golden) else None
        x = c[i] if i < len(c) else None
        if g != x:
            section = (g or x)[0]
            return ["%s: line %d:" % (section, i + 1),
                    "  " + _cut("golden: %s" % (g[1] if g else "<end>")),
                    "  " + _cut("c: %s" % (x[1] if x else "<end>"))]
    return None


def compare_group(group, golden, c):
    """None when the projections' lines are equal, else the detail lines of the first
    difference."""
    return compare([(group, line) for line in golden], [(group, line) for line in c])


def run_case(gen, tables, case_dir, stage, golden, groups, golden_logs):
    """The detail lines of a DIVERGENT case, or None when it passes."""
    if not os.path.isfile(gen) or not os.access(gen, os.X_OK):
        return ["no C generator: %s" % gen]
    with tempfile.TemporaryDirectory(prefix="ladx-e2e-") as out_dir:
        try:
            proc = subprocess.run([gen, "--tables", tables, case_dir, out_dir], stdin=subprocess.DEVNULL,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            return ["c generator timed out after %d s" % TIMEOUT]
        if proc.returncode != 0:
            err = proc.stderr.decode("utf-8", "replace").splitlines()[:5]
            return ["c generator exited %d" % proc.returncode] + ["  " + _cut(line) for line in err]
        path = os.path.join(out_dir, "run.txt")
        if not os.path.isfile(path):
            return ["c wrote no run.txt"]
        try:
            c = project(read_lines(path), stage)
        except FormatError as e:
            return ["c run.txt malformed: %s" % e]
        detail = compare(golden, c) or []
        if groups:
            path = os.path.join(out_dir, "rom_writes.txt")
            if not os.path.isfile(path):
                return detail + ["c wrote no rom_writes.txt"]
            try:
                with open(path, encoding="utf-8", errors="replace") as f:
                    c_logs = log_projections(f.read(), groups)
            except write_log.WriteLogError as e:
                return detail + [_cut("c rom_writes.txt malformed: %s" % e)]
            for group in groups:
                detail += compare_group(group, golden_logs[group], c_logs[group]) or []
    return detail or None


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


def run(cases_dir, claims, gen, names, out, tables, inventory):
    """Prints the verdicts to out; returns the exit code."""
    try:
        stage, groups = read_claims(claims, inventory)
        cases = list_cases(cases_dir, names)
        goldens = {}
        golden_logs = {}
        for case in cases:
            path = os.path.join(cases_dir, case, "run.txt")
            try:
                goldens[case] = project(read_lines(path), stage)
            except OSError:
                raise Usage("%s: no golden run.txt" % case)
            except FormatError as e:
                raise Usage("%s: golden run.txt: %s" % (case, e))
            if not groups:
                continue
            path = os.path.join(cases_dir, case, "rom_writes.txt")
            try:
                with open(path, encoding="utf-8", errors="replace") as f:
                    golden_logs[case] = log_projections(f.read(), groups)
            except OSError:
                raise Usage("%s: no golden rom_writes.txt" % case)
            except write_log.WriteLogError as e:
                raise Usage("%s: golden rom_writes.txt: %s" % (case, e))
    except Usage as e:
        print("run_e2e: %s" % e, file=out)
        return 2
    if not cases:
        print("e2e: no cases", file=out)
        return 2
    print("e2e: claims stage %s, groups %s" % (stage, " ".join(groups) or "none"), file=out)
    workers = min(len(cases), os.cpu_count() or 1)
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        results = list(pool.map(lambda c: run_case(gen, tables, os.path.join(cases_dir, c), stage, goldens[c],
                                                   groups, golden_logs.get(c)), cases))
    divergent = 0
    claimed = "%s + %d groups" % (stage, len(groups)) if groups else stage
    for case, detail in zip(cases, results):
        if detail is None:
            print("PASS %s (%s)" % (case, claimed), file=out)
            continue
        divergent += 1
        print("DIVERGENT %s" % case, file=out)
        for line in detail:
            print("  " + line, file=out)
    print("e2e: %d pass, %d divergent" % (len(cases) - divergent, divergent), file=out)
    return 1 if divergent else 0


# --- C's projections of the goldens ----------------------------------------------------------

def project_case(gen, path):
    """(the golden's group count, [(group or "-", detail)] of the differences between ladx_gen
    --project's files for the log at path and write_log.py's projections); WriteLogError when the
    golden is malformed."""
    with open(path, encoding="utf-8", errors="replace") as f:
        want = write_log.projections(f.read())
    if not os.path.isfile(gen) or not os.access(gen, os.X_OK):
        return len(want), [("-", "no C generator: %s" % gen)]
    with tempfile.TemporaryDirectory(prefix="ladx-e2e-project-") as out_dir:
        try:
            proc = subprocess.run([gen, "--project", path, out_dir], stdin=subprocess.DEVNULL,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            return len(want), [("-", "c generator timed out after %d s" % TIMEOUT)]
        if proc.returncode != 0:
            err = proc.stderr.decode("utf-8", "replace").splitlines()[:1]
            return len(want), [("-", _cut("c generator exited %d%s" % (proc.returncode,
                                                                        "".join(": " + e for e in err))))]
        got = {}
        for name in sorted(os.listdir(out_dir)):
            with open(os.path.join(out_dir, name), encoding="utf-8", errors="replace") as f:
                got[name[:-4] if name.endswith(".txt") else name] = f.read()
    diffs = []
    for group in sorted(set(want) | set(got)):
        if group not in got:
            diffs.append((group, "missing"))
        elif group not in want:
            diffs.append((group, "extra"))
        elif got[group] != want[group]:
            a, b = want[group].split("\n"), got[group].split("\n")
            n = next(i for i in range(max(len(a), len(b))) if i >= len(a) or i >= len(b) or a[i] != b[i])
            diffs.append((group, "line %d" % (n + 1)))
    return len(want), diffs


def run_projections(cases_dir, gen, names, out):
    """Prints one PROJECTION line per difference, then the summary; returns the exit code."""
    try:
        cases = list_cases(cases_dir, names)
        for case in cases:
            if not os.path.isfile(os.path.join(cases_dir, case, "rom_writes.txt")):
                raise Usage("%s: no golden rom_writes.txt" % case)
    except Usage as e:
        print("run_e2e: %s" % e, file=out)
        return 2
    if not cases:
        print("projections: no cases", file=out)
        return 2
    paths = [os.path.join(cases_dir, case, "rom_writes.txt") for case in cases]
    workers = min(len(cases), os.cpu_count() or 1)
    try:
        with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as pool:
            results = list(pool.map(project_case, [gen] * len(paths), paths))
    except write_log.WriteLogError as e:
        print("run_e2e: a golden rom_writes.txt: %s" % e, file=out)
        return 2
    different = 0
    for case, (_, diffs) in zip(cases, results):
        for group, detail in diffs:
            print("PROJECTION %s %s: %s" % (case, group, detail), file=out)
        different += len(diffs)
    if different:
        print("projections: %d goldens, %d differences from write_log.py's" % (len(cases), different), file=out)
        return 1
    print("projections: %d goldens, %d group projections equal to write_log.py's"
          % (len(cases), sum(total for total, _ in results)), file=out)
    return 0


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


def _merged(segments):
    """segments with consecutive segments of one group merged, as a log allows them."""
    out = []
    for name, banks, entries in segments:
        if out and out[-1][0] == name:
            out[-1][1].update(banks)
            out[-1][2].update(entries)
        else:
            out.append((name, dict(banks), dict(entries)))
    return out


def _render(tables, segments):
    return write_log.render(tables, _merged(segments))


def _copy(segments):
    return [(name, dict(banks), dict(entries)) for name, banks, entries in segments]


def _changed(tables, segments, index):
    """The log with one bank byte of segments[index] changed."""
    out = _copy(segments)
    banks = out[index][1]
    if not banks:
        raise AssertionError("self-test: segment %d has no bank byte" % index)
    p = max(banks)
    a = banks[p]
    banks[p] = -1 - ((-1 - a + 1) % 256) if a < 0 else -1 - ((a + 1) % 256)
    return _render(tables, out)


def _planted_logs(golden_log):
    """[(name, claimed groups, golden log, c log or None, expected)]: the write-log claims."""
    tables, segments = write_log.parse(golden_log)
    names = [name for name, _, _ in segments]
    table_groups = ["fixed", "frame-counter", "goal-fixed", "music-fixed", "options"]
    only_tables = _render(tables, [seg for seg in segments if seg[0] in table_groups])
    frame = names.index("frame-counter")
    before = max(i for i in range(frame) if names[i] != "frame-counter" and segments[i][1])
    after = min(i for i in range(frame + 1, len(names)) if names[i] != "frame-counter" and segments[i][1])
    last_fixed = max(i for i, name in enumerate(names) if name == "fixed")
    moved = _copy(segments)
    p = max(moved[last_fixed][1])
    moved[last_fixed + 1][1][p] = moved[last_fixed][1].pop(p)
    last_options = max(i for i, name in enumerate(names) if name == "options")
    head = "ladx-rom-writes 1\ntables texts\nG fixed\n"
    return [
        ("identical logs", table_groups, golden_log, golden_log, "PASS"),
        ("c log of the table groups alone, merged across C segments", table_groups, golden_log, only_tables,
         "PASS"),
        ("changed byte in a claimed group", table_groups, golden_log, _changed(tables, segments, last_options),
         "DIVERGENT"),
        ("changed byte in an unclaimed group before the claimed one", ["frame-counter"], golden_log,
         _changed(tables, segments, before), "PASS"),
        ("changed byte in an unclaimed group after the claimed one", ["frame-counter"], golden_log,
         _changed(tables, segments, after), "PASS"),
        # The bytes at 2000 are stated to be 0a0b0c0d: the driver has no ROM to tell.
        ("literal for a reference to the same bytes", ["fixed"], head + "B 100 @2000+4\n",
         head + "B 100 0a0b0c0d\n", "DIVERGENT"),
        ("claimed group's byte in a segment of another group", ["fixed"], golden_log, _render(tables, moved),
         "DIVERGENT"),
        ("claimed group absent from the c log", ["options"], golden_log,
         _render(tables, [seg for seg in segments if seg[0] != "options"]), "DIVERGENT"),
        ("no c rom_writes.txt", table_groups, golden_log, None, "DIVERGENT"),
        ("malformed c log", table_groups, golden_log,
         golden_log.replace("ladx-rom-writes 1", "ladx-rom-writes 2", 1), "DIVERGENT"),
        ("golden I lines, c log without them", table_groups, golden_log, write_log.render(tables, segments),
         "PASS"),
        ("unknown group claimed", ["nosuch"], golden_log, golden_log, "exit 2"),
        ("group claimed twice", ["fixed", "fixed"], golden_log, golden_log, "exit 2"),
        ("spoiler claimed", ["spoiler"], golden_log, golden_log, "exit 2"),
        ("golden rom_writes.txt missing", ["fixed"], None, golden_log, "exit 2"),
    ]


def _verdict(code, text):
    if code == 2:
        return "exit 2"
    if code == 0 and "\nPASS " in "\n" + text and "DIVERGENT" not in text:
        return "PASS"
    if code == 1 and "\nDIVERGENT " in "\n" + text:
        return "DIVERGENT"
    return "exit %d" % code


def _write(path, text):
    with open(path, "w") as f:
        f.write(text)


def _planted_run(root, config, stage, groups, gold, c, kind, gold_log, c_log):
    """run's (code, text) on one planted case tree and fake generator."""
    case = os.path.join(root, "cases", "planted")
    os.makedirs(case)
    shutil.copy(config, case)
    _write(os.path.join(case, "run.txt"), "".join(line + "\n" for line in gold))
    if gold_log is not None:
        _write(os.path.join(case, "rom_writes.txt"), gold_log)
    gen = os.path.join(root, "gen")
    script = "#!%s\nimport os, shutil, sys\n" % sys.executable
    if kind == "fail":
        script += "sys.stderr.write('planted failure\\n')\nsys.exit(3)\n"
    for name, text in (("run.txt", c and "".join(line + "\n" for line in c)), ("rom_writes.txt", c_log)):
        if kind != "fail" and text is not None:
            planted = os.path.join(root, "c-" + name)
            _write(planted, text)
            script += "shutil.copy(%r, os.path.join(sys.argv[-1], %r))\n" % (planted, name)
    _write(gen, script)
    os.chmod(gen, 0o755)
    if kind == "missing":
        gen = os.path.join(root, "no-such-gen")
    claims = os.path.join(root, "claims.txt")
    _write(claims, "# planted\nstage %s\n" % stage + "".join("group %s\n" % g for g in groups))
    out = io.StringIO()
    code = run(os.path.join(root, "cases"), claims, gen, [], out, os.path.join(TOP, "data", "ladx"),
               os.path.join(TOP, "notes", "ladx-inventory.md"))
    return code, out.getvalue()


def _planted_projection(root, golden_log, change):
    """run_projections' (code, text) on one planted golden, the fake generator writing
    write_log.py's projections, change(projections) applied first."""
    case = os.path.join(root, "cases", "planted")
    os.makedirs(case)
    _write(os.path.join(case, "rom_writes.txt"), golden_log)
    planted = change(write_log.projections(golden_log))
    files = os.path.join(root, "projections")
    os.makedirs(files)
    for group, text in planted.items():
        _write(os.path.join(files, group + ".txt"), text)
    gen = os.path.join(root, "gen")
    _write(gen, "#!%s\nimport os, shutil, sys\nassert sys.argv[1] == '--project'\n"
                "for name in os.listdir(%r):\n    shutil.copy(os.path.join(%r, name), sys.argv[3])\n"
           % (sys.executable, files, files))
    os.chmod(gen, 0o755)
    out = io.StringIO()
    code = run_projections(os.path.join(root, "cases"), gen, [], out)
    return code, out.getvalue()


def _wrong_projection(projections):
    out = dict(projections)
    group = sorted(out)[0]
    out[group] = out[group].replace("G ", "G x", 1)
    return out


def self_test():
    golden = read_lines(os.path.join(HERE, "cases", "baseline", "run.txt"))
    with open(os.path.join(HERE, "cases", "baseline", "rom_writes.txt"), encoding="utf-8") as f:
        golden_log = f.read()
    config = os.path.join(HERE, "cases", "baseline", "config.txt")
    planted = [(name, stage, [], gold, c, kind, None, None, expected)
               for name, stage, gold, c, kind, expected in _planted(golden)]
    planted += [(name, "fill", groups, golden, golden, "copy", gold_log, c_log, expected)
                for name, groups, gold_log, c_log, expected in _planted_logs(golden_log)]
    failed = []
    with tempfile.TemporaryDirectory(prefix="ladx-e2e-self-") as tmp:
        for k, (name, stage, groups, gold, c, kind, gold_log, c_log, expected) in enumerate(planted):
            code, text = _planted_run(os.path.join(tmp, str(k)), config, stage, groups, gold, c, kind, gold_log,
                                      c_log)
            got = _verdict(code, text)
            if got != expected:
                failed.append("%s (expected %s, got %s)" % (name, expected, got))
        projected = [("projections equal to write_log.py's", lambda p: p, 0, "projections: 1 goldens"),
                     ("a wrong projection", _wrong_projection, 1, "PROJECTION planted "),
                     ("a projection missing", lambda p: dict(list(p.items())[1:]), 1, "PROJECTION planted ")]
        for k, (name, change, code, line) in enumerate(projected):
            got_code, text = _planted_projection(os.path.join(tmp, "p%d" % k), golden_log, change)
            if got_code != code or not ("\n" + text).count("\n" + line):
                failed.append("%s (expected exit %d and %r, got exit %d)" % (name, code, line, got_code))
        planted += projected
    for case in failed:
        print("e2e: self-test failed: %s" % case)
    if failed:
        return 1
    print("e2e: self-test, %d planted runs as expected" % len(planted))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(prog="run_e2e.py")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--projections", action="store_true")
    parser.add_argument("--cases", default=os.path.join(HERE, "cases"))
    parser.add_argument("--claims", default=os.path.join(HERE, "claims.txt"))
    parser.add_argument("--gen", default=os.path.join(HERE, "build", "ladx_gen"))
    parser.add_argument("--tables", default=os.path.join(TOP, "data", "ladx"))
    parser.add_argument("--inventory", default=os.path.join(TOP, "notes", "ladx-inventory.md"))
    parser.add_argument("case", nargs="*")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if args.self_test:
        return self_test()
    if args.projections:
        return run_projections(args.cases, os.path.abspath(args.gen), args.case, sys.stdout)
    return run(args.cases, args.claims, os.path.abspath(args.gen), args.case, sys.stdout,
               os.path.abspath(args.tables), args.inventory)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
