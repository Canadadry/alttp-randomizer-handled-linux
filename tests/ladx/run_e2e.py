"""LADX's e2e driver: runs the C generator (ladx_gen) on every case of a directory and compares,
per case, the golden's and C's stage projection of run.txt at the stage claims.txt names, then
each claimed group's write-log projection (write_log.py's). --projections compares C's projection
of every golden (ladx_gen --project) with write_log.py's, group by group. --rom runs the same with
the source ROM (e2e-rom): C's KS span list against known_source.build's, then every group and
rom-group claimed. Contract in tests/ladx/README.md, "Stage projection", "E2E driver and claims"
and "ROM-present e2e". Standard library only; run with python3 -I -B.
"""

import argparse
import concurrent.futures
import hashlib
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

LADX_SHA1 = "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"
WORK = os.path.join(TOP, "tmp", "ladx-e2e-rom")

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
# A claimed C group that draws also claims its trace lines: those of these LADXR frames.
GROUP_TRACE = {
    "marin": ("patches.aesthetics:getMarinStartingText", "utils:randomNumber"),
    "hints": ("hints:addHints",),
    "music": ("patches.music:randomizeMusic",),
    "enemizer": ("patches.enemies:randomizeEnemies",),
    "endscreen": ("patches.endscreen:updateEndScreen",),
}
PORTED_STAGES = set().union(*STAGE_TRACE.values(), *GROUP_TRACE.values())


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


def group_trace(lines, group, renumber=False):
    """The trace lines of run.txt's lines drawn in group's frames (GROUP_TRACE), whole, in order.
    With renumber (a golden's), each line's number less the trace lines before it drawn in a
    frame C does not port (in neither STAGE_TRACE nor GROUP_TRACE), which C never draws."""
    stages = GROUP_TRACE.get(group, ())
    out, unported = [], 0
    for line in lines:
        fields = line.split(" ")
        if not line.startswith("trace ") or len(fields) < 5:
            continue
        if fields[3] not in PORTED_STAGES:
            unported += 1
        elif fields[3] in stages:
            if renumber and unported and fields[1].isdigit():
                fields[1] = "%d" % (int(fields[1]) - unported)
            out.append(" ".join(fields))
    return out


def read_lines(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read().splitlines()


class Claims:
    """claims.txt: the stage, the group lines and the rom-group lines, in file order, and whether
    the spoiler and net lines are there."""

    def __init__(self, stage, groups, rom_groups, spoiler=False, net=False):
        self.stage, self.groups, self.rom_groups, self.spoiler, self.net = stage, groups, rom_groups, spoiler, net


def read_claims(path, inventory):
    """The Claims of claims.txt; Usage on a malformed file or a group line only the ROM can
    verify (Evidence `rom`). The inventory (a path) is read only when a group is claimed."""
    try:
        lines = read_lines(path)
    except OSError as e:
        raise Usage("cannot read claims %s: %s" % (path, e.strerror))
    stage = None
    groups, rom_groups = [], []
    spoiler = net = False
    table = None
    for n, line in enumerate(lines, 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        fields = text.split()
        if fields == ["spoiler"]:
            if spoiler:
                raise Usage("%s:%d: a second spoiler line" % (path, n))
            spoiler = True
            continue
        if fields == ["net"]:
            if net:
                raise Usage("%s:%d: a second net line" % (path, n))
            net = True
            continue
        if len(fields) != 2 or fields[0] not in ("stage", "group", "rom-group"):
            raise Usage("%s:%d: malformed line" % (path, n))
        if fields[0] in ("group", "rom-group"):
            if table is None:
                table = read_groups_table(inventory)
            group = fields[1]
            side, evidence = table.get(group, (None, ()))
            sides = ("table", "C") if fields[0] == "group" else ("C",)
            if side not in sides or group == "spoiler":
                raise Usage("%s:%d: not a claimable %s %r" % (path, n, fields[0], group))
            if group in groups or group in rom_groups:
                raise Usage("%s:%d: group %s claimed twice" % (path, n, group))
            if fields[0] == "group" and "rom" in evidence:
                raise Usage("%s:%d: group %s needs the ROM (its Evidence lists rom): claim it as a rom-group"
                            % (path, n, group))
            (groups if fields[0] == "group" else rom_groups).append(group)
            continue
        if fields[1] not in STAGES:
            raise Usage("%s:%d: unknown stage %r" % (path, n, fields[1]))
        if stage is not None:
            raise Usage("%s:%d: a second stage line" % (path, n))
        stage = fields[1]
    if stage is None:
        raise Usage("%s: no stage line" % path)
    return Claims(stage, groups, rom_groups, spoiler, net)


def read_groups_table(inventory):
    """{group: (side, (evidence item, ...))} of the inventory's Groups table."""
    import inventory_check
    try:
        with open(inventory, encoding="utf-8") as f:
            named, bad = inventory_check.parse_named(f.read())
    except OSError as e:
        raise Usage("cannot read inventory %s: %s" % (inventory, e.strerror))
    if bad:
        raise Usage("%s: %s" % (inventory, bad[0]))
    return {row["Group"]: (row["Side"], tuple(item.strip() for item in row["Evidence"].split(",")))
            for row in named["Groups"]}


def source_references(segments, group):
    """The source offsets the group's records reference (keeps and copies), in log order."""
    refs = []
    for name, banks, entries in segments:
        if name != group:
            continue
        refs += [a for a in banks.values() if a >= 0]
        for atoms in entries.values():
            if isinstance(atoms, list):
                refs += [a for a in atoms if a >= 0]
    return refs


def log_projections(text, groups):
    """{group: projection lines} of a write log's text for each of groups; WriteLogError when
    the log is malformed."""
    return parsed_projections(write_log.parse(text), groups)


def parsed_projections(parsed, groups):
    """log_projections of write_log.parse's (tables, segments)."""
    tables, segments = parsed
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


def record_head(line):
    """A write-log line with its tokens left out: the record's head and its token count."""
    fields = line.split(" ")
    head = {"B": 2, "T": 3}.get(fields[0])
    if head is None or len(fields) < head:
        return line if fields[0] in ("G", "tables", "ladx-rom-writes") else "%s (%d fields)" % (fields[0], len(fields))
    return "%s (%d tokens)" % (" ".join(fields[:head]), len(fields) - head)


def compare(golden, c, show=None):
    """None when equal, else the detail lines of the first difference, each line shown through
    show (default whole)."""
    show = show or (lambda line: line)
    for i in range(max(len(golden), len(c))):
        g = golden[i] if i < len(golden) else None
        x = c[i] if i < len(c) else None
        if g != x:
            section = (g or x)[0]
            return ["%s: line %d:" % (section, i + 1),
                    "  " + _cut("golden: %s" % (show(g[1]) if g else "<end>")),
                    "  " + _cut("c: %s" % (show(x[1]) if x else "<end>"))]
    return None


def compare_group(group, golden, c, redact=False):
    """None when the projections' lines are equal, else the detail lines of the first
    difference; redact: record heads only (the ROM is present)."""
    return compare([(group, line) for line in golden], [(group, line) for line in c],
                   record_head if redact else None)


def golden_spoiler(lines):
    """run.txt's spoiler line: the spoiler_sha1 digest, "none" for `spoiler none`, None when absent."""
    for line in lines:
        fields = line.split(" ")
        if fields[0] == "spoiler_sha1" and len(fields) == 2:
            return fields[1]
        if fields == ["spoiler", "none"]:
            return "none"
    return None


def _first_line_difference(a, b):
    a, b = a.split(b"\n"), b.split(b"\n")
    return next(i for i in range(max(len(a), len(b))) if i >= len(a) or i >= len(b) or a[i] != b[i]) + 1


def compare_spoiler(want, out_dir, case_dir):
    """The spoiler claim's detail lines (digests and a line number only), or [] when C's
    spoiler.json matches the golden's spoiler line want (a SHA-1, or "none")."""
    path = os.path.join(out_dir, "spoiler.json")
    if not os.path.isfile(path):
        return [] if want == "none" else ["spoiler: c wrote no spoiler.json"]
    if want == "none":
        return ["spoiler: c wrote a spoiler.json, golden spoiler none"]
    with open(path, "rb") as f:
        data = f.read()
    got = hashlib.sha1(data).hexdigest()
    if got == want:
        return []
    line = "spoiler: sha1 %s, golden %s" % (got, want)
    golden = os.path.join(case_dir, "spoiler.json")
    if os.path.isfile(golden):
        with open(golden, "rb") as f:
            line += ", first difference at line %d" % _first_line_difference(f.read(), data)
    return [line]


def net_effect(segments):
    """{offset: atom} of a parsed log's bank records replayed in log order: the final value of every
    written output offset, a literal (negative) or a source reference."""
    out = {}
    for _, banks, _ in segments:
        out.update(banks)
    return out


def _atom_kind(atom):
    return "unwritten" if atom is None else "reference" if atom >= 0 else "literal"


def compare_net(golden, c):
    """The net claim's detail lines (offsets and kinds only), [] when the two net effects are equal."""
    differ = sorted(p for p in set(golden) | set(c) if golden.get(p) != c.get(p))
    if not differ:
        return []
    p = differ[0]
    return ["net: %d offsets differ, first at %x: golden %s, c %s"
            % (len(differ), p, _atom_kind(golden.get(p)), _atom_kind(c.get(p)))]


def run_case(gen, tables, case_dir, stage, golden, groups, golden_logs, rom=None, out_root=None, golden_traces=None,
             spoiler=None, net=None):
    """The detail lines of a DIVERGENT case, or None when it passes. With rom, the generator gets
    --rom and writes into out_root/<case>/, the detail lines redacted. spoiler: the golden's spoiler
    line (golden_spoiler) when the spoiler is claimed, else None."""
    if not os.path.isfile(gen) or not os.access(gen, os.X_OK):
        return ["no C generator: %s" % gen]
    if rom is None:
        with tempfile.TemporaryDirectory(prefix="ladx-e2e-") as out_dir:
            return _run_case(gen, tables, case_dir, stage, golden, groups, golden_logs, None, out_dir,
                             golden_traces, spoiler, net)
    out_dir = os.path.join(out_root, os.path.basename(case_dir))
    os.makedirs(out_dir)
    return _run_case(gen, tables, case_dir, stage, golden, groups, golden_logs, rom, out_dir, golden_traces, spoiler,
                     net)


def _run_case(gen, tables, case_dir, stage, golden, groups, golden_logs, rom, out_dir, golden_traces=None,
              spoiler=None, net=None):
    args = [gen, "--tables", tables] + (["--rom", rom] if rom else []) + [case_dir, out_dir]
    try:
        proc = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return ["c generator timed out after %d s" % TIMEOUT]
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", "replace").splitlines()[:5]
        return ["c generator exited %d" % proc.returncode] + ["  " + _cut(line) for line in err]
    path = os.path.join(out_dir, "run.txt")
    if not os.path.isfile(path):
        return ["c wrote no run.txt"]
    try:
        c_lines = read_lines(path)
        c = project(c_lines, stage)
    except FormatError as e:
        return ["c run.txt malformed: %s" % e]
    detail = compare(golden, c) or []
    for group in groups:
        if group in GROUP_TRACE:
            label = "trace %s" % group
            detail += compare([(label, line) for line in (golden_traces or {}).get(group, [])],
                              [(label, line) for line in group_trace(c_lines, group)]) or []
    if spoiler is not None:
        detail += compare_spoiler(spoiler, out_dir, case_dir)
    if groups or net is not None:
        path = os.path.join(out_dir, "rom_writes.txt")
        if not os.path.isfile(path):
            return detail + ["c wrote no rom_writes.txt"]
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                parsed = write_log.parse(f.read())
            c_logs = parsed_projections(parsed, groups)
        except write_log.WriteLogError as e:
            # With the ROM present a parse message may quote a token: its line number only.
            reason = str(e).split(":")[0] if rom else str(e)
            return detail + [_cut("c rom_writes.txt malformed: %s" % reason)]
        for group in groups:
            detail += compare_group(group, golden_logs[group], c_logs[group], rom is not None) or []
        if net is not None:
            detail += compare_net(net, net_effect(parsed[1]))
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


class RomMode:
    """e2e-rom's inputs: the ROM's path, the work root, whether KS is compared, and the two
    injectable parts the self-test plants (the accepted SHA-1, the oracle's span list)."""

    def __init__(self, rom, work=WORK, ks=True, sha1=LADX_SHA1, ks_oracle=None):
        self.rom, self.work, self.ks, self.sha1 = rom, work, ks, sha1
        self.ks_oracle = ks_oracle or oracle_spans


def oracle_spans(rom):
    """known_source.build's span list text of the ROM's bytes."""
    import known_source
    return known_source.spans(known_source.build(rom))


def read_rom(path, sha1):
    """The ROM's bytes; Usage (a setup line) when missing or of another SHA-1."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        raise Usage("setup: e2e-rom: no ROM at %s (tests/ladx/oracle/fetch_rom.sh fetches it)" % path)
    digest = hashlib.sha1(data).hexdigest()
    if digest != sha1:
        raise Usage("setup: e2e-rom: %s is not the LADX English 1.0 ROM (SHA-1 %s)" % (path, digest))
    return data


def _span_label(line):
    fields = line.split(" ")
    if fields[0] == "failure" or len(fields) < 3:
        return line
    return " ".join(fields[2:])


def compare_ks(oracle, c):
    """The `KS DIFFERENT: line <n>: <label>` lines of two span lists, labels only."""
    a, b = oracle.splitlines(), c.splitlines()
    out = []
    for i in range(max(len(a), len(b))):
        x = a[i] if i < len(a) else None
        y = b[i] if i < len(b) else None
        if x != y:
            label = _span_label(x) if x is not None else "<end>, c has %s" % _span_label(y)
            out.append("KS DIFFERENT: line %d: %s" % (i + 1, label))
    return out


def ks_counts(text):
    """(spans, distinct bytes, failures) of a span list."""
    spans, failures, covered = 0, 0, set()
    for line in text.splitlines()[1:]:
        fields = line.split(" ")
        if fields[0] == "failure":
            failures += 1
            continue
        start, length = int(fields[0], 16), int(fields[1], 16)
        spans += 1
        covered.update(range(start, start + length))
    return spans, len(covered), failures


def run_ks(gen, mode, data, out):
    """e2e-rom's KS comparison; returns 0 when equal, else 1."""
    oracle = mode.ks_oracle(data)
    oracle_path = os.path.join(mode.work, "ks-oracle.txt")
    c_path = os.path.join(mode.work, "ks-c.txt")
    with open(oracle_path, "w") as f:
        f.write(oracle)
    if not os.path.isfile(gen) or not os.access(gen, os.X_OK):
        print("e2e-rom: KS: no C generator: %s" % gen, file=out)
        return 1
    try:
        proc = subprocess.run([gen, "--rom", mode.rom, "--ks-spans", c_path], stdin=subprocess.DEVNULL,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        print("e2e-rom: KS: c generator timed out after %d s" % TIMEOUT, file=out)
        return 1
    if proc.returncode != 0 or not os.path.isfile(c_path):
        print("e2e-rom: KS: c generator exited %d" % proc.returncode, file=out)
        return 1
    with open(c_path, encoding="utf-8", errors="replace") as f:
        diffs = compare_ks(oracle, f.read())
    for line in diffs:
        print(line, file=out)
    if diffs:
        return 1
    print("e2e-rom: KS equal, %d spans, %d bytes, %d not rebuilt" % ks_counts(oracle), file=out)
    return 0


def _empty(path):
    if os.path.isdir(path):
        shutil.rmtree(path)
    os.makedirs(path)


def load_goldens(cases_dir, cases, stage, with_logs, raw=None):
    """({case: run.txt projection}, {case: write_log.parse of rom_writes.txt}) (logs only
    with_logs); raw, a dict, gets {case: run.txt's lines}; Usage on a missing or malformed golden."""
    goldens, logs = {}, {}
    for case in cases:
        path = os.path.join(cases_dir, case, "run.txt")
        try:
            lines = read_lines(path)
            if raw is not None:
                raw[case] = lines
            goldens[case] = project(lines, stage)
        except OSError:
            raise Usage("%s: no golden run.txt" % case)
        except FormatError as e:
            raise Usage("%s: golden run.txt: %s" % (case, e))
        if not with_logs:
            continue
        path = os.path.join(cases_dir, case, "rom_writes.txt")
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                logs[case] = write_log.parse(f.read())
        except OSError:
            raise Usage("%s: no golden rom_writes.txt" % case)
        except write_log.WriteLogError as e:
            raise Usage("%s: golden rom_writes.txt: %s" % (case, e))
    return goldens, logs


def check_needs_rom(claims, inventory, cases, logs):
    """Usage when a C group claimed by a group line holds a source reference in a golden."""
    if not claims.groups:
        return
    table = read_groups_table(inventory)
    for group in claims.groups:
        if table[group][0] != "C":
            continue
        for case in cases:
            if source_references(logs[case][1], group):
                raise Usage("group %s holds source references in %s's golden: it needs the ROM to encode, "
                            "claim it as a rom-group" % (group, case))


def run(cases_dir, claims, gen, names, out, tables, inventory, rom=None):
    """Prints the verdicts to out; returns the exit code. rom: a RomMode (e2e-rom)."""
    prefix = "e2e-rom" if rom else "e2e"
    try:
        claimed = read_claims(claims, inventory)
        stage = claimed.stage
        groups = claimed.groups + (claimed.rom_groups if rom else [])
        cases = list_cases(cases_dir, names)
        raw = {}
        goldens, logs = load_goldens(cases_dir, cases, stage,
                                     bool(claimed.groups or claimed.rom_groups or claimed.net and rom), raw)
        check_needs_rom(claimed, inventory, cases, logs)
        golden_logs = {case: parsed_projections(logs[case], groups) for case in logs}
        golden_traces = {case: {g: group_trace(raw[case], g, renumber=True) for g in groups} for case in cases}
        spoilers = {}
        if claimed.spoiler:
            for case in cases:
                spoilers[case] = golden_spoiler(raw[case])
                if spoilers[case] is None:
                    raise Usage("%s: golden run.txt: no spoiler_sha1 or spoiler none line" % case)
        data = read_rom(rom.rom, rom.sha1) if rom else None
    except Usage as e:
        print(str(e) if str(e).startswith("setup:") else "run_e2e: %s" % e, file=out)
        return 2
    if not cases:
        print("%s: no cases" % prefix, file=out)
        return 2
    out_root = None
    if rom:
        _empty(rom.work if rom.ks else os.path.join(rom.work, "runs", os.path.basename(cases_dir.rstrip("/"))))
        if rom.ks and run_ks(gen, rom, data, out):
            return 1
        out_root = os.path.join(rom.work, "runs", os.path.basename(cases_dir.rstrip("/")))
        os.makedirs(out_root, exist_ok=True)
    header = "%s: claims stage %s, groups %s" % (prefix, stage, " ".join(claimed.groups) or "none")
    if claimed.rom_groups:
        header += ", rom-groups %s%s" % (" ".join(claimed.rom_groups), "" if rom else " skipped")
    if claimed.spoiler:
        header += ", spoiler%s" % ("" if rom else " skipped")
    if claimed.net:
        header += ", net%s" % ("" if rom else " skipped")
    print(header, file=out)
    nets = {case: net_effect(logs[case][1]) for case in cases} if claimed.net and rom else {}
    workers = min(len(cases), os.cpu_count() or 1)
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        results = list(pool.map(lambda c: run_case(gen, tables, os.path.join(cases_dir, c), stage, goldens[c],
                                                   groups, golden_logs.get(c), rom and os.path.abspath(rom.rom),
                                                   out_root, golden_traces[c],
                                                   spoilers.get(c) if rom else None, nets.get(c)), cases))
    divergent = 0
    label = "%s + %d groups" % (stage, len(groups)) if groups else stage
    if claimed.spoiler and rom:
        label += " + spoiler"
    if claimed.net and rom:
        label += " + net"
    for case, detail in zip(cases, results):
        if detail is None:
            print("PASS %s (%s)" % (case, label), file=out)
            continue
        divergent += 1
        print("DIVERGENT %s" % case, file=out)
        for line in detail:
            print("  " + line, file=out)
    print("%s: %d pass, %d divergent" % (prefix, len(cases) - divergent, divergent), file=out)
    return 1 if divergent else 0


def run_ks_groups(cases_dir, names, out, inventory, rom=None):
    """--ks-groups: per C group without rom in its Evidence, the goldens holding source
    references (inside and outside KS when rom, a RomMode, is given); returns the exit code."""
    try:
        table = read_groups_table(inventory)
        cases = list_cases(cases_dir, names)
        _, logs = load_goldens(cases_dir, cases, "fill", True)
        data = read_rom(rom.rom, rom.sha1) if rom else None
    except Usage as e:
        print(str(e) if str(e).startswith("setup:") else "run_e2e: %s" % e, file=out)
        return 2
    ks = None
    if data is not None:
        import known_source
        ks = known_source.build(data)
    found = []
    for group in sorted(g for g, (side, evidence) in table.items()
                        if side == "C" and g != "spoiler" and "rom" not in evidence):
        holding = [case for case in cases if source_references(logs[case][1], group)]
        if not holding:
            print("ks-groups: %s: no golden of %d holds a source reference" % (group, len(cases)), file=out)
            continue
        found.append(group)
        line = "ks-groups: %s: %d of %d goldens hold source references, first %s" % (
            group, len(holding), len(cases), holding[0])
        if ks is not None:
            inside = sum(1 for case in holding if any(q in ks for q in source_references(logs[case][1], group)))
            outside = sum(1 for case in holding
                          if any(q not in ks for q in source_references(logs[case][1], group)))
            line += " (%d with references into KS, %d outside)" % (inside, outside)
        print(line, file=out)
    print("ks-groups: %d groups without rom in their Evidence hold source references: %s"
          % (len(found), " ".join(found) or "none"), file=out)
    return 0


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


def _drawn_before(lines, index, stage):
    """lines with one rnd choice traced in stage inserted before line index, later traces renumbered."""
    out, number = list(lines[:index]), int(lines[index].split(" ")[1])
    out.append("trace %d rnd %s choice 2 -> 0 [1]" % (number, stage))
    for line in lines[index:]:
        fields = line.split(" ")
        if fields[0] == "trace":
            fields[1] = "%d" % (int(fields[1]) + 1)
        out.append(" ".join(fields))
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


PLANTED_SPANS = "ladx-ks-spans 1\n10 4 texts 1\n12 1 boss item 3\nfailure texts bank 1\n"


def _planted_rom_run(root, config, golden, gold_log, c_log, claims, inventory=None, rom=None, rom_log=None,
                     spans=PLANTED_SPANS, c_run=None, gold_run=None, gold_spoiler=None, c_spoiler=None):
    """run's (code, text) on one planted case tree whose fake generator writes c_log, or rom_log
    when it receives --rom, the planted spans for --ks-spans, and c_run's lines (default the
    golden's) as run.txt. rom: "ok", "other" (a ROM of another SHA-1) or "missing" for e2e-rom
    mode, None for e2e. gold_run: the golden's run.txt lines instead of golden. gold_spoiler: the
    case folder's spoiler.json bytes; c_spoiler: the spoiler.json bytes the generator writes (None:
    none)."""
    c_run = c_run if c_run is not None else golden
    golden = gold_run if gold_run is not None else golden
    case = os.path.join(root, "cases", "planted")
    os.makedirs(case)
    shutil.copy(config, case)
    run_path = os.path.join(case, "run.txt")
    _write(run_path, "".join(line + "\n" for line in golden))
    _write(os.path.join(case, "rom_writes.txt"), gold_log)
    if gold_spoiler is not None:
        with open(os.path.join(case, "spoiler.json"), "wb") as f:
            f.write(gold_spoiler)
    paths = {"spoiler": os.path.join(root, "c-spoiler")}
    if c_spoiler is not None:
        with open(paths["spoiler"], "wb") as f:
            f.write(c_spoiler)
    for name, text in (("log", c_log), ("rom-log", rom_log or c_log), ("spans", spans),
                       ("run", "".join(line + "\n" for line in (c_run if c_run is not None else golden)))):
        paths[name] = os.path.join(root, "c-" + name)
        _write(paths[name], text)
    gen = os.path.join(root, "gen")
    _write(gen, "#!%s\nimport os, shutil, sys\nargs = sys.argv[1:]\n"
                "if '--ks-spans' in args:\n    shutil.copy(%r, args[args.index('--ks-spans') + 1])\n    sys.exit(0)\n"
                "shutil.copy(%r, os.path.join(args[-1], 'run.txt'))\n"
                "shutil.copy(%r if '--rom' in args else %r, os.path.join(args[-1], 'rom_writes.txt'))\n"
                "if os.path.isfile(%r):\n    shutil.copy(%r, os.path.join(args[-1], 'spoiler.json'))\n"
           % (sys.executable, paths["spans"], paths["run"], paths["rom-log"], paths["log"], paths["spoiler"],
              paths["spoiler"]))
    os.chmod(gen, 0o755)
    claims_path = os.path.join(root, "claims.txt")
    _write(claims_path, claims)
    mode = None
    if rom:
        rom_path = os.path.join(root, "planted.gbc")
        if rom != "missing":
            _write(rom_path, "planted rom")
        sha1 = hashlib.sha1(b"planted rom").hexdigest() if rom == "ok" else LADX_SHA1
        mode = RomMode(rom_path, os.path.join(root, "work"), True, sha1, lambda data: PLANTED_SPANS)
    out = io.StringIO()
    code = run(os.path.join(root, "cases"), claims_path, gen, [], out, os.path.join(TOP, "data", "ladx"),
               inventory or os.path.join(TOP, "notes", "ladx-inventory.md"), mode)
    return code, out.getvalue()


def _rom_checks(tmp, config, golden, golden_log, golden_spoiler_json):
    """[(name, failure or None)]: rom-group lines, the two group refusals, e2e-rom mode, the spoiler
    claim and the KS comparator."""
    tables, segments = write_log.parse(golden_log)
    bank3e = max(i for i, (name, _, _) in enumerate(segments) if name == "bank3e")
    changed = _changed(tables, segments, bank3e)
    plain = write_log.render(tables, segments)
    with_copy = write_log.render(tables, segments + [("hints", {0x100 + i: 0x2000 + i for i in range(4)}, {})])
    with open(os.path.join(TOP, "notes", "ladx-inventory.md"), encoding="utf-8") as f:
        inventory = f.read()
    row = next(line for line in inventory.splitlines() if line.startswith("| hints | C |"))
    planted_inventory = os.path.join(tmp, "inventory.md")
    _write(planted_inventory, inventory.replace(row, row[:-2] + ", rom |", 1))
    fill = "stage fill\n"
    marin = [i for i, line in enumerate(golden) if line.split(" ")[3:4] == [GROUP_TRACE["marin"][0]]]
    redrawn = _replace(golden, marin[0], "-> ", "-> 1") if marin else None
    undrawn = [line for i, line in enumerate(golden) if i not in marin]
    end = [i for i, line in enumerate(golden) if line.split(" ")[3:4] == [GROUP_TRACE["endscreen"][0]]]
    unported = _drawn_before(golden, end[0], "patches.planted:unported") if end else None
    hinted = _drawn_before(golden, end[0], GROUP_TRACE["hints"][0]) if end else None
    sj = golden_spoiler_json
    changed_sj = sj.replace(b"\n", b"\n ", 1)
    spoiler_none = [l for l in golden if not l.startswith("spoiler_sha1 ")]
    spoiler_none.insert(len(spoiler_none) - 1, "spoiler none")
    no_spoiler_line = [l for l in golden if not l.startswith("spoiler_sha1 ")]
    claims_sp = fill + "spoiler\n"
    spot = 0x7fff0
    hints_seg, bank3e_seg = ("hints", {spot: write_log._lit(0x11)}, {}), ("bank3e", {spot: write_log._lit(0x22)}, {})
    overlap = write_log.render(tables, segments + [hints_seg, bank3e_seg])
    swapped = write_log.render(tables, segments + [bank3e_seg, hints_seg])
    claims_net = fill + "group hints\nrom-group bank3e\nnet\n"
    marin_seg = ("marin", {spot: write_log._lit(0x33)}, {})
    first_same = write_log.render(tables, segments + [hints_seg, bank3e_seg, marin_seg])
    first_same_c = write_log.render(tables, segments + [hints_seg, marin_seg, bank3e_seg])
    runs = [
        ("e2e-rom, three writers of one offset, the first in place, the last two swapped",
         dict(gold_log=first_same, c_log=first_same, rom_log=first_same_c, claims=fill + "net\n", rom="ok"),
         "DIVERGENT", "  net: 1 offsets differ"),
        ("net claimed, an overlap-order swap, skipped by e2e",
         dict(gold_log=overlap, c_log=swapped, claims=claims_net), "PASS", "rom-groups bank3e skipped, net skipped"),
        ("the same claimed by e2e-rom: every projection equal, the net effect divergent",
         dict(gold_log=overlap, c_log=overlap, rom_log=swapped, claims=claims_net, rom="ok"), "DIVERGENT",
         "  net: 1 offsets differ, first at %x: golden literal, c literal\n" % spot),
        ("net claimed by e2e-rom, the logs in one order",
         dict(gold_log=overlap, c_log=overlap, claims=claims_net, rom="ok"), "PASS", " + net)"),
        ("net claimed by e2e-rom, c writing one offset more",
         dict(gold_log=golden_log, c_log=golden_log, rom_log=overlap, claims=fill + "net\n", rom="ok"), "DIVERGENT",
         "golden unwritten, c literal"),
        ("net line twice", dict(gold_log=golden_log, c_log=golden_log, claims=fill + "net\nnet\n"), "exit 2",
         "a second net line"),
        ("spoiler claimed, c's spoiler.json different, skipped by e2e",
         dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp, gold_spoiler=sj, c_spoiler=changed_sj),
         "PASS", "groups none, spoiler skipped"),
        ("spoiler claimed by e2e-rom, c's spoiler.json the golden's",
         dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp, rom="ok", gold_spoiler=sj, c_spoiler=sj),
         "PASS", "PASS planted (fill + spoiler)"),
        ("spoiler claimed by e2e-rom, c's spoiler.json one line different",
         dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp, rom="ok", gold_spoiler=sj,
              c_spoiler=changed_sj), "DIVERGENT", ", first difference at line 2"),
        ("spoiler claimed by e2e-rom, c wrote none",
         dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp, rom="ok", gold_spoiler=sj), "DIVERGENT",
         "spoiler: c wrote no spoiler.json"),
        ("spoiler claimed by e2e-rom, golden spoiler none, c wrote one",
         dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp, rom="ok", gold_run=spoiler_none,
              c_spoiler=sj), "DIVERGENT", "golden spoiler none"),
        ("spoiler claimed by e2e-rom, golden spoiler none, c wrote none",
         dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp, rom="ok", gold_run=spoiler_none), "PASS",
         None),
        ("spoiler claimed, golden run.txt with no spoiler line",
         dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp, gold_run=no_spoiler_line), "exit 2",
         "no spoiler_sha1 or spoiler none line"),
        ("spoiler line twice", dict(gold_log=golden_log, c_log=golden_log, claims=claims_sp + "spoiler\n"),
         "exit 2", "a second spoiler line"),
        ("spoiler line with a name", dict(gold_log=golden_log, c_log=golden_log, claims=fill + "spoiler json\n"),
         "exit 2", "malformed line"),
        # (name, kwargs, expected verdict, a line the output must hold)
        ("rom-group divergent in its group, skipped by e2e",
         dict(gold_log=golden_log, c_log=changed, claims=fill + "rom-group bank3e\n"), "PASS",
         "e2e: claims stage fill, groups none, rom-groups bank3e skipped"),
        ("the same run claimed by e2e-rom, the generator given --rom",
         dict(gold_log=golden_log, c_log=golden_log, rom_log=changed, claims=fill + "rom-group bank3e\n",
              rom="ok"), "DIVERGENT", "e2e-rom: KS equal, 2 spans, 4 bytes, 1 not rebuilt"),
        ("e2e-rom's divergent line shows the record's head only",
         dict(gold_log=golden_log, c_log=golden_log, rom_log=changed, claims=fill + "rom-group bank3e\n",
              rom="ok"), "DIVERGENT", " tokens)"),
        ("a rom-group's trace line changed, skipped by e2e",
         dict(gold_log=golden_log, c_log=golden_log, claims=fill + "rom-group marin\n", c_run=redrawn), "PASS",
         "rom-groups marin skipped"),
        ("the same claimed by e2e-rom", dict(gold_log=golden_log, c_log=golden_log, claims=fill + "rom-group marin\n",
                                             rom="ok", c_run=redrawn), "DIVERGENT", "trace marin: line 1:"),
        ("a rom-group's trace lines missing from C's run.txt under e2e-rom",
         dict(gold_log=golden_log, c_log=golden_log, claims=fill + "rom-group marin\n", rom="ok", c_run=undrawn),
         "DIVERGENT", "c: <end>"),
        ("a golden draw in a frame C does not port, before a claimed one, renumbered away",
         dict(gold_log=golden_log, c_log=golden_log, claims=fill + "rom-group endscreen\n", rom="ok",
              gold_run=unported), "PASS", "rom-groups endscreen"),
        ("a golden draw in a ported frame before a claimed one still shifts its number",
         dict(gold_log=golden_log, c_log=golden_log, claims=fill + "rom-group endscreen\n", rom="ok",
              gold_run=hinted), "DIVERGENT", "trace endscreen: line 1:"),
        ("e2e-rom with KS different", dict(gold_log=golden_log, c_log=golden_log, claims=fill, rom="ok",
                                          spans=PLANTED_SPANS.replace("12 1", "12 2")),
         "exit 1", "KS DIFFERENT: line 3: boss item 3"),
        ("e2e-rom with a ROM of another SHA-1", dict(gold_log=golden_log, c_log=golden_log, claims=fill,
                                                     rom="other"), "exit 2", "setup: e2e-rom: "),
        ("e2e-rom with no ROM", dict(gold_log=golden_log, c_log=golden_log, claims=fill, rom="missing"),
         "exit 2", "setup: e2e-rom: no ROM"),
        ("group on a C group with no source reference",
         dict(gold_log=golden_log, c_log=plain, claims=fill + "group hints\n"), "PASS", None),
        ("group on a group whose Evidence lists rom",
         dict(gold_log=golden_log, c_log=plain, claims=fill + "group hints\n", inventory=planted_inventory),
         "exit 2", "needs the ROM (its Evidence lists rom)"),
        ("group on a C group whose golden holds a copy",
         dict(gold_log=with_copy, c_log=with_copy, claims=fill + "group hints\n"), "exit 2",
         "group hints holds source references in planted's golden"),
        ("rom-group on a table group", dict(gold_log=golden_log, c_log=golden_log,
                                            claims=fill + "rom-group fixed\n"), "exit 2", None),
        ("group and rom-group on one group", dict(gold_log=golden_log, c_log=golden_log,
                                                  claims=fill + "group marin\nrom-group marin\n"), "exit 2", None),
        ("rom-group twice", dict(gold_log=golden_log, c_log=golden_log,
                                 claims=fill + "rom-group marin\nrom-group marin\n"), "exit 2", None),
    ]
    out = []
    for k, (name, kwargs, expected, line) in enumerate(runs):
        code, text = _planted_rom_run(os.path.join(tmp, "r%d" % k), config, golden, **kwargs)
        got = _verdict(code, text)
        if got == expected:
            ok = line is None or line in text
            out.append((name, None if ok else "expected the line %r" % line))
        else:
            out.append((name, "expected %s, got %s" % (expected, got)))
    lines = PLANTED_SPANS.splitlines(True)
    for name, c, want in (
            ("KS comparator, equal span lists", PLANTED_SPANS, []),
            ("KS comparator, one span changed", PLANTED_SPANS.replace("10 4", "10 5"),
             ["KS DIFFERENT: line 2: texts 1"]),
            ("KS comparator, one span missing", "".join(lines[:1] + lines[2:]),
             ["KS DIFFERENT: line 2: texts 1", "KS DIFFERENT: line 3: boss item 3",
              "KS DIFFERENT: line 4: failure texts bank 1"])):
        got = compare_ks(PLANTED_SPANS, c)
        out.append((name, None if got == want else "got %r" % got))
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
        with open(os.path.join(HERE, "cases", "baseline", "spoiler.json"), "rb") as f:
            golden_spoiler_json = f.read()
        for name, failure in _rom_checks(tmp, config, golden, golden_log, golden_spoiler_json):
            planted.append(name)
            if failure:
                failed.append("%s (%s)" % (name, failure))
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
    parser.add_argument("--rom")
    parser.add_argument("--work", default=WORK)
    parser.add_argument("--no-ks", action="store_true")
    parser.add_argument("--ks-groups", action="store_true")
    parser.add_argument("case", nargs="*")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if args.self_test:
        return self_test()
    if args.projections:
        return run_projections(args.cases, os.path.abspath(args.gen), args.case, sys.stdout)
    rom = RomMode(args.rom, os.path.abspath(args.work), not args.no_ks) if args.rom else None
    if args.ks_groups:
        return run_ks_groups(args.cases, args.case, sys.stdout, args.inventory, rom)
    return run(args.cases, args.claims, os.path.abspath(args.gen), args.case, sys.stdout,
               os.path.abspath(args.tables), args.inventory, rom)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
