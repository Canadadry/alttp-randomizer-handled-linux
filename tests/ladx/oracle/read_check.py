"""Usage: python3 -I tests/ladx/oracle/read_check.py [--ladxr <dir>] [--inventory <file>] [--traces <dir>] [--scope-cuts <file>] [--print]

The check of notes/ladx-inventory.md (--inventory) against the read traces of real runs
(--traces, default tmp/ladx-inventory/, written by inventory_trace.sh: `<case>/seed-<k>/` holding
oracle_run.py --reads' reads.txt and writes.txt, and run.txt for the outcome). The seeds of one
case share its settings. Proves itself first, on every run, on planted traces and inventories.

Each traced site is keyed to its inventory row; a site on a `for`/`if`/`elif`/`else`/`while`/
`try`/`except` header goes to the first row of the body it governs, a continuation line to its
statement (the sources under --ladxr, default tests/ladx/oracle/ladxr). One line per finding:
  UNTAGGED          a read or write with no inventory row
  UNCOVERED         a read in a `C` row that no Read coverage entry matches
  BAD-COVER         a coverage entry whose conditions fail (below)
  NONE-ACCESS       a read or write in a `none` row outside the group `parse`
  UNUSED-REACHED    a read or write in an `unused` row
  SEED-VARIES       a `table` row whose writes differ between the `ok` seeds of one case
  TABLE-READS-SEED  a `table` row reading bytes a `C` group wrote (written: or mixed:)
  DEPENDS           a group's Depends on differs from the writer groups of its reads (itself
                    and `-` left out)
  READS-COLUMN      a Reads (trace) cell differs from what the traces give
Information: `STABLE <row>` (a `C` row whose writes are identical under the `ok` seeds of every
case that compares them), `UNREACHED <row>` (a `table` or `C` row no trace reads or writes in).
Only runs whose outcome is `ok` are compared for SEED-VARIES and STABLE: an error or a crash
stops part-way. Last line `read-check: <cases> cases, <reads> reads, <groups> groups covered`
(groups: the `C` groups with reads), else `read-check: <n> findings`. Exit 1 on a finding, 2 on
`setup:` (no checkout, inventory or traces; a malformed trace; a trace whose group of a row's
own site differs from the inventory's: traces older than the inventory).

Coverage entry (Group, Row `*` or a row key, Target `*` or a trace target, Range `*` or
`0xLO..0xHI` holding the read's range; a read with no range, a whole-target read, needs `*`).
Every entry: a listed `C` group, a row of that group, at least one matched read. Kind `copy`:
a Reason; `written`: every matched read `written:` by groups listed under Groups; `table`: the
same, by `table` groups; `computed`: group `header`, a named row whose text names `checksum`
(the checksum rows, whose reads decide only 0x14D..0x14F); `rom`: a Reason (the bytes are read
from the user's ROM at runtime, verified by rom-check, never committed); `cut:<id>`: a heading's or a list item's first word in --scope-cuts (default
notes/plan3-scope-cuts.md).

Reads (trace) cell: per target and provenance, `<target> <provenance> <range>` (`0xLO..0xHI`,
`all` when the target was also read whole), entries `; `-separated, banks first then tables,
consecutive banks of the same provenance and range merged (`bank 0x00..0x3F`); `-` for no read.
`--print` writes the inventory with the Reads (trace) cells and the Depends on column taken from
the traces, to refresh the file by hand; no finding is checked.
"""

import argparse
import importlib.util
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
DEFAULT_LADXR = os.path.join(HERE, "ladxr")
DEFAULT_INVENTORY = os.path.join(TOP, "notes", "ladx-inventory.md")
DEFAULT_TRACES = os.path.join(TOP, "tmp", "ladx-inventory")
DEFAULT_SCOPE_CUTS = os.path.join(TOP, "notes", "plan3-scope-cuts.md")

sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("inventory_check",
                                               os.path.join(HERE, "inventory_check.py"))
inventory_check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(inventory_check)

PARSE_GROUP = "parse"
HEADER_GROUP = "header"
COMPUTED_ROW = re.compile(r"\bchecksum\b")
ROW_CELL = re.compile(r"`(.+)`(?: #(\d+))?")
READS_COLUMN = inventory_check.COLUMNS.index("Reads (trace)")
DEPENDS_COLUMN = inventory_check.GROUP_COLUMNS.index("Depends on")


class SetupError(Exception):
    pass


class Read:
    __slots__ = ("group", "site", "caller", "channel", "target", "prov", "n", "span")

    def __init__(self, group, site, caller, channel, target, prov, n, span):
        self.group, self.site, self.caller, self.channel = group, site, caller, channel
        self.target, self.prov, self.n, self.span = target, prov, n, span


class Run:
    def __init__(self, case, seed, outcome, reads, writes):
        self.case, self.seed, self.outcome = case, seed, outcome
        self.reads, self.writes = reads, writes


def _site(fields, path):
    if fields[0] == "-":
        return None, fields[1:]
    if len(fields) < 2:
        raise SetupError("malformed line in %s" % path)
    return (fields[0], fields[1]), fields[2:]


def _detail(text, path):
    m = re.fullmatch(r"n=(\d+)(?: 0x([0-9A-F]+)\.\.0x([0-9A-F]+))?", text)
    if not m:
        raise SetupError("malformed detail %r in %s" % (text, path))
    span = None if m.group(2) is None else (int(m.group(2), 16), int(m.group(3), 16))
    return int(m.group(1)), span


def _lines(path):
    with open(path, "rb") as f:
        return [line for line in f.read().decode("utf-8").split("\n") if line]


def read_run(case, seed, folder):
    reads, writes = [], []
    for name in ("reads.txt", "writes.txt"):
        if not os.path.isfile(os.path.join(folder, name)):
            raise SetupError("no %s in %s (run inventory_trace.sh)" % (name, folder))
    path = os.path.join(folder, "reads.txt")
    for line in _lines(path):
        fields = line.split("\t")
        site, rest = _site(fields[1:], path)
        if len(rest) != 5:
            raise SetupError("malformed line in %s: %r" % (path, line))
        caller, channel, target, prov, detail = rest
        n, span = _detail(detail, path)
        reads.append(Read(fields[0], site, caller, channel, target, prov, n, span))
    path = os.path.join(folder, "writes.txt")
    for line in _lines(path):
        fields = line.split("\t")
        site, rest = _site(fields[1:], path)
        if len(rest) != 2 or not rest[0].startswith("n=") or not rest[1].startswith("sha1="):
            raise SetupError("malformed line in %s: %r" % (path, line))
        writes.append((fields[0], site, int(rest[0][2:]), rest[1][5:]))
    outcome = "none"
    run_path = os.path.join(folder, "run.txt")
    if os.path.isfile(run_path):
        last = (_lines(run_path) or [""])[-1].split()
        if len(last) >= 2 and last[0] == "outcome":
            outcome = last[1]
    return Run(case, seed, outcome, reads, writes)


def read_traces(root):
    runs = []
    if os.path.isdir(root):
        for case in sorted(os.listdir(root)):
            folder = os.path.join(root, case)
            if not os.path.isdir(folder):
                continue
            for seed in sorted(os.listdir(folder)):
                if seed.startswith("seed-") and os.path.isdir(os.path.join(folder, seed)):
                    runs.append(read_run(case, seed, os.path.join(folder, seed)))
    if not runs:
        raise SetupError("no traces in %s (run tests/ladx/oracle/inventory_trace.sh)" % root)
    return runs


class Resolver:
    def __init__(self, root, rows):
        self._sources = inventory_check.Sources(root)
        self._rows = rows
        self._bodies = {}
        self._cache = {}

    def _body(self, section):
        if section not in self._bodies:
            file, _, function = section.partition(" ")
            lines = self._sources.lines(file)
            body = None
            if lines is not None:
                body = inventory_check._function_body(lines, function,
                                                      self._sources.continuations(file))
            keyed, seen = [], {}
            for _, line, continues in body or []:
                text = line.strip()
                key = None
                if text:
                    seen[text] = seen.get(text, 0) + 1
                    key = text if seen[text] == 1 else "%s #%d" % (text, seen[text])
                keyed.append((key, line, continues))
            self._bodies[section] = keyed
        return self._bodies[section]

    def row(self, site):
        if site is None:
            return None
        if site in self._rows:
            return site
        if site not in self._cache:
            self._cache[site] = self._resolve(*site)
        return self._cache[site]

    def _resolve(self, section, key):
        body = self._body(section)
        at = next((i for i, (k, _, _) in enumerate(body) if k == key), None)
        if at is None:
            return None
        while at > 0 and body[at][2]:
            at -= 1
        text = body[at][1].strip()
        if (section, body[at][0]) in self._rows:
            return section, body[at][0]
        if not inventory_check.HEADER.match(text):
            return None
        indent = inventory_check._indent(body[at][1])
        for k, line, continues in body[at + 1:]:
            if k is None or continues or line.strip().startswith("#"):
                continue
            if inventory_check._indent(line) <= indent:
                return None
            if (section, k) in self._rows:
                return section, k
        return None


def _writers(prov):
    kind, _, names = prov.partition(":")
    return [] if kind == "source" else names.split(",")


def _range(lo, hi):
    return "0x%04X..0x%04X" % (lo, hi)


def reads_cell(reads):
    acc = {}
    for r in reads:
        entry = acc.setdefault((r.target, r.prov), [None, None, False])
        if r.span is None:
            entry[2] = True
        else:
            entry[0] = r.span[0] if entry[0] is None else min(entry[0], r.span[0])
            entry[1] = r.span[1] if entry[1] is None else max(entry[1], r.span[1])
    banks, others = {}, []
    for (target, prov), (lo, hi, whole) in acc.items():
        text = "all" if whole else _range(lo, hi)
        m = re.fullmatch(r"bank 0x([0-9A-F]+)", target)
        if m:
            banks.setdefault((prov, text), []).append(int(m.group(1), 16))
        else:
            others.append((target, prov, text))
    entries = []
    for (prov, text), numbers in banks.items():
        numbers.sort()
        start = prev = numbers[0]
        for n in numbers[1:] + [None]:
            if n is not None and n == prev + 1:
                prev = n
                continue
            name = "bank 0x%02X" % start + ("" if prev == start else "..0x%02X" % prev)
            entries.append(((0, start, prov), "%s %s %s" % (name, prov, text)))
            start = prev = n
    for target, prov, text in others:
        entries.append(((1, target, prov), "%s %s %s" % (target, prov, text)))
    return "; ".join(text for _, text in sorted(entries)) or "-"


def scope_cut_ids(text):
    ids = set()
    for line in (text or "").split("\n"):
        m = re.match(r"\s*(?:#{1,6}|[-*])\s+(\S+)", line)
        if m:
            ids.add(m.group(1).strip("`*").rstrip(":").strip("`*"))
    return ids


def _row_text(site):
    return "%s: %s" % site


class Checker:
    def __init__(self, ladxr, text, runs, cuts_text):
        tables, bad = inventory_check.parse_inventory(text)
        named, named_bad = inventory_check.parse_named(text)
        if bad or named_bad:
            raise SetupError("the inventory does not parse (run inventory_check.py): %s"
                             % (bad + named_bad)[0])
        self.rows = {(section, row["key"]): row for section, rows in tables.items()
                     for row in rows}
        self.order = [(section, row["key"]) for section, rows in tables.items() for row in rows]
        self.groups = {row["Group"]: row for row in named["Groups"]}
        self.named = named
        self.cuts = scope_cut_ids(cuts_text)
        self.runs = runs
        self.resolver = Resolver(ladxr, self.rows)
        self.findings, self.infos = [], []
        self.total = 0
        self._row_reads = {}
        self._reached = set()
        self._writers = {g: set() for g in self.groups}
        self._read_groups = set()
        self._digests = {}
        self._check()

    def side(self, group):
        return self.groups.get(group, {}).get("Side")

    def _own_row(self, site, traced_group):
        row = self.resolver.row(site)
        if row is not None and row == site and traced_group != self.rows[row]["Group"]:
            raise SetupError("%s traced in group %s, the inventory says %s: the traces are older "
                             "than the inventory (run inventory_trace.sh)"
                             % (_row_text(site), traced_group, self.rows[row]["Group"]))
        return row

    def _check(self):
        untagged, none_access, unused, table_seed, uncovered = {}, {}, {}, set(), {}
        entries = [dict(e, matched=[]) for e in self.named["Read coverage"]]
        for run in self.runs:
            for r in run.reads:
                self.total += r.n
                row = self._own_row(r.site, r.group)
                if row is None:
                    where = _row_text(r.site) if r.site else "-"
                    untagged.setdefault("%s: read %s via %s" % (where, r.target, r.caller), 0)
                    continue
                self._reached.add(row)
                self._row_reads.setdefault(row, []).append(r)
                info = self.rows[row]
                group, side = info["Group"], info["Side"]
                self._read_groups.add(group)
                writers = set(_writers(r.prov)) - {group, "-"}
                self._writers.setdefault(group, set()).update(writers)
                if side == "none" and group != PARSE_GROUP:
                    none_access.setdefault(row, [0, 0])[0] += r.n
                elif side == "unused":
                    unused.setdefault(row, [0, 0])[0] += r.n
                elif side == "table":
                    if any(self.side(g) == "C" for g in _writers(r.prov)):
                        table_seed.add((row, r.target, r.prov))
                elif side == "C":
                    matched = [e for e in entries if self._matches(e, row, r)]
                    for e in matched:
                        e["matched"].append(r)
                    if not matched:
                        acc = uncovered.setdefault((row, r.target), [None, None, False, set()])
                        acc[3].add(r.prov)
                        if r.span is None:
                            acc[2] = True
                        else:
                            acc[0] = r.span[0] if acc[0] is None else min(acc[0], r.span[0])
                            acc[1] = r.span[1] if acc[1] is None else max(acc[1], r.span[1])
            for group, site, n, sha1 in run.writes:
                row = self._own_row(site, group)
                if row is None:
                    where = _row_text(site) if site else "-"
                    untagged.setdefault("%s: write" % where, 0)
                    continue
                self._reached.add(row)
                side = self.rows[row]["Side"]
                if side == "none" and self.rows[row]["Group"] != PARSE_GROUP:
                    none_access.setdefault(row, [0, 0])[1] += n
                elif side == "unused":
                    unused.setdefault(row, [0, 0])[1] += n
                per_run = self._digests.setdefault(run.case, {}).setdefault(row, {})
                per_run.setdefault(run.seed, []).append((site, n, sha1))
        for where in sorted(untagged):
            self.findings.append("UNTAGGED " + where)
        for (row, target), (lo, hi, whole, provs) in sorted(uncovered.items()):
            self.findings.append("UNCOVERED %s %s: %s %s %s" % (
                self.rows[row]["Group"], _row_text(row), target,
                "all" if whole else _range(lo, hi), " ".join(sorted(provs))))
        for e in entries:
            for reason in self._cover_faults(e):
                self.findings.append("BAD-COVER %s %s %s %s: %s" % (
                    e["Group"], e["Row"], e["Target"], e["Range"], reason))
        for kind, found in (("NONE-ACCESS", none_access), ("UNUSED-REACHED", unused)):
            for row in sorted(found):
                self.findings.append("%s %s: %d reads, %d writes"
                                     % ((kind, _row_text(row)) + tuple(found[row])))
        self._seeds()
        for row, target, prov in sorted(table_seed):
            self.findings.append("TABLE-READS-SEED %s: %s %s" % (_row_text(row), target, prov))
        self.depends = {g: ", ".join(sorted(self._writers.get(g, ()))) or "-"
                        for g in self.groups}
        for group in sorted(self.groups):
            listed = ", ".join(sorted(inventory_check._items(self.groups[group]["Depends on"])))
            if (listed or "-") != self.depends[group]:
                self.findings.append("DEPENDS %s: inventory %s, trace %s"
                                     % (group, listed or "-", self.depends[group]))
        self.cells = {row: reads_cell(self._row_reads.get(row, [])) for row in self.order}
        for row in self.order:
            if self.rows[row]["Reads (trace)"] != self.cells[row]:
                self.findings.append("READS-COLUMN %s: inventory %s, trace %s" % (
                    _row_text(row), self.rows[row]["Reads (trace)"], self.cells[row]))
        for row in self.order:
            if self.rows[row]["Side"] in ("table", "C") and row not in self._reached:
                self.infos.append("UNREACHED " + _row_text(row))

    def _matches(self, entry, row, read):
        if entry["Group"] != self.rows[row]["Group"]:
            return False
        if entry["Row"] != "*":
            m = ROW_CELL.fullmatch(entry["Row"])
            if not m or (m.group(1) if m.group(2) is None else "%s #%s" % m.groups()) != row[1]:
                return False
        if entry["Target"] not in ("*", read.target):
            return False
        if entry["Range"] == "*":
            return True
        m = re.fullmatch(r"0x([0-9A-Fa-f]+)\.\.0x([0-9A-Fa-f]+)", entry["Range"])
        return (m is not None and read.span is not None and int(m.group(1), 16) <= read.span[0]
                and read.span[1] <= int(m.group(2), 16))

    def _cover_faults(self, e):
        faults = []
        group, kind, matched = e["Group"], e["Kind"], e["matched"]
        if self.side(group) != "C":
            faults.append("group %s is not a C group" % group)
        row_key = None
        if e["Row"] != "*":
            m = ROW_CELL.fullmatch(e["Row"])
            row_key = m and (m.group(1) if m.group(2) is None else "%s #%s" % m.groups())
            if not any(k == row_key and self.rows[(s, k)]["Group"] == group
                       for s, k in self.rows):
                faults.append("no row %s in group %s" % (e["Row"], group))
        if not matched:
            faults.append("matches no read")
        provs = sorted({r.prov for r in matched})
        if kind == "copy":
            if not e["Reason"]:
                faults.append("a copy needs a Reason saying where the bytes go")
        elif kind == "rom":
            if not e["Reason"]:
                faults.append("a rom read needs a Reason saying what its value decides")
        elif kind in ("written", "table"):
            for prov in provs:
                writers = _writers(prov)
                if not prov.startswith("written:"):
                    faults.append("%s read of provenance %s" % (kind, prov))
                elif kind == "written" and any(g not in self.groups for g in writers):
                    faults.append("written by a group not under Groups: %s" % prov)
                elif kind == "table" and any(self.side(g) != "table" for g in writers):
                    faults.append("table read written by a group that is not table: %s" % prov)
        elif kind == "computed":
            if group != HEADER_GROUP or row_key is None or not COMPUTED_ROW.search(row_key):
                faults.append("computed is only for the %s group's checksum rows" % HEADER_GROUP)
        elif kind.startswith("cut:"):
            if kind[len("cut:"):] not in self.cuts:
                faults.append("cut %s is not in the scope-cuts note" % kind[len("cut:"):])
        else:
            faults.append("unknown kind %s" % kind)
        return faults

    def _seeds(self):
        ok = {}
        for run in self.runs:
            if run.outcome == "ok":
                ok.setdefault(run.case, []).append(run.seed)
        varies, stable = {}, {}
        for case, rows in sorted(self._digests.items()):
            seeds = ok.get(case, [])
            if len(seeds) < 2:
                continue
            for row, per_seed in rows.items():
                values = {tuple(sorted(per_seed.get(s, []))) for s in seeds}
                if values == {()}:
                    continue
                differs = len(values) > 1
                if differs:
                    varies.setdefault(row, []).append(case)
                stable[row] = stable.get(row, True) and not differs
        for row in sorted(varies):
            if self.rows[row]["Side"] == "table":
                self.findings.append("SEED-VARIES %s: cases %s"
                                     % (_row_text(row), ", ".join(varies[row])))
        for row in self.order:
            if self.rows[row]["Side"] == "C" and stable.get(row):
                self.infos.append("STABLE " + _row_text(row))

    def summary(self):
        if self.findings:
            return "read-check: %d findings" % len(self.findings)
        covered = sum(1 for g in self._read_groups if self.side(g) == "C")
        return "read-check: %d cases, %d reads, %d groups covered" % (
            len({run.case for run in self.runs}), self.total, covered)

    def refreshed(self, text):
        sections = {inventory_check.section_name(f, fn) for f, fn, _ in inventory_check.SECTIONS}
        out, heading = [], None
        for line in text.split("\n"):
            if line.startswith("## "):
                heading = line[3:].strip()
            cells = inventory_check._split_cells(line) if line.startswith("|") else None
            if heading in sections and cells and len(cells) == len(inventory_check.COLUMNS):
                m = ROW_CELL.fullmatch(cells[0])
                key = m and (m.group(1) if m.group(2) is None else "%s #%s" % m.groups())
                if key and (heading, key) in self.cells:
                    cells[READS_COLUMN] = self.cells[(heading, key)]
                    line = self._join(line, cells)
            elif heading == "Groups" and cells and len(cells) == len(
                    inventory_check.GROUP_COLUMNS) and cells[0] in self.depends:
                cells[DEPENDS_COLUMN] = self.depends[cells[0]]
                line = self._join(line, cells)
            out.append(line)
        return "\n".join(out)

    @staticmethod
    def _join(line, cells):
        if inventory_check._split_cells(line) == cells:
            return line
        return "| " + " | ".join(inventory_check._escape(c) for c in cells) + " |"


def check(ladxr, text, traces, cuts_text):
    return Checker(ladxr, text, read_traces(traces), cuts_text)


PLANTED_SOURCES = {
    "generator.py": """\
def generateRom(args, settings, seed, logic, *, rnd=None):
    rom = ROMWithTables(open(args.input_filename, 'rb'))
    patches.p.fixed(rom)
    patches.p.items(rom,
                    logic)
    for c in rom.banks[0][0x10:0x12]:
        # the body's first row
        patches.p.summed(rom, c)
    patches.p.later(rom)
    assembler.const("X", 1)
    patches.p.debug(rom)
    patches.p.never(rom)
    while rom.banks[0][0x20]:
        return rom
    return rom
""",
    "rom.py": """\
class ROM:
    def fixHeader(self, *, name=None):
        checksum = 0
        for c in self.banks[0][0x134:0x14D]:
            checksum -= c + 1
        self.banks[0][0x14D] = checksum & 0xFF
""",
}

GEN = "generator.py generateRom"
HDR = "rom.py ROM.fixHeader"
PLANTED_ROWS = [
    # (section, key, side, group, Reads (trace))
    (GEN, "rom = ROMWithTables(open(args.input_filename, 'rb'))", "none", "parse",
     "bank 0x3F source 0x0147..0x0147"),
    (GEN, "patches.p.fixed(rom)", "table", "fixed", "-"),
    (GEN, "patches.p.items(rom,", "C", "items",
     "bank 0x03 source 0x0000..0x0000; bank 0x04 written:fixed 0x0000..0x0001; "
     "table entities source 0x0001..0x0003"),
    (GEN, "patches.p.summed(rom, c)", "C", "sum",
     "bank 0x00 source 0x0010..0x0011; bank 0x01 written:items 0x0000..0x0003; "
     "bank 0x05..0x06 source all"),
    (GEN, "patches.p.later(rom)", "table", "later", "bank 0x02 written:fixed 0x0000..0x0001"),
    (GEN, 'assembler.const("X", 1)', "none", "consts", "-"),
    (GEN, "patches.p.debug(rom)", "unused", "debug", "-"),
    (GEN, "patches.p.never(rom)", "table", "never", "-"),
    (HDR, "checksum = 0", "C", "header", "-"),
    (HDR, "checksum -= c + 1", "C", "header", "bank 0x00 source 0x0134..0x014C"),
    (HDR, "self.banks[0][0x14D] = checksum & 0xFF", "C", "header", "-"),
]
PLANTED_GROUPS = [
    # (group, side, depends on, evidence)
    ("parse", "none", "-", "-"), ("fixed", "table", "-", "-"),
    ("items", "C", "fixed", "items, rom"), ("sum", "C", "items", "sum"),
    ("later", "table", "fixed", "-"), ("consts", "none", "-", "-"),
    ("debug", "unused", "-", "-"), ("never", "table", "-", "-"), ("header", "C", "-", "header"),
]
PLANTED_COVERAGE = [
    "| items | `patches.p.items(rom,` | table entities | 0x0000..0x00FF | rom | "
    "decides the sprite sets |",
    "| items | * | bank 0x03 | * | cut:no-x | read only under x |",
    "| items | * | bank 0x04 | 0x0000..0x0001 | table | the fixed patch's bytes |",
    "| sum | * | bank 0x00 | 0x0010..0x0011 | copy | moved to bank 0x01 |",
    "| sum | `patches.p.summed(rom, c)` | bank 0x01 | * | written | the items' bytes |",
    "| sum | * | * | * | copy | summed into bank 0x07 |",
    "| header | `checksum -= c + 1` | bank 0x00 | * | computed | the header checksum |",
]
PLANTED_CUTS = "# Scope cuts\n\n## no-x\n\nSetting x=1 is cut.\n"


def planted_inventory(rows=None, groups=None, coverage=None):
    by_section = {}
    for section, key, side, group, cell in PLANTED_ROWS if rows is None else rows:
        m = re.fullmatch(r"(.*) #(\d+)", key)
        call = "`%s` #%s" % m.groups() if m else "`%s`" % key
        by_section.setdefault(section, []).append(
            "| %s | - | - | - | - | %s |  | %s | %s |" % (call, cell, side, group))

    def table(name, columns, lines):
        return ["## " + name, "", "| " + " | ".join(columns) + " |",
                "|" + "---|" * len(columns)] + lines + [""]
    out = ["# Planted inventory", ""]
    for section, lines in by_section.items():
        out += table(section, inventory_check.COLUMNS, lines)
    out += table("Groups", inventory_check.GROUP_COLUMNS,
                 ["| %s | %s | - | %s | %s |" % g for g in
                  (PLANTED_GROUPS if groups is None else groups)])
    out += table("Read coverage", inventory_check.COVERAGE_COLUMNS,
                 PLANTED_COVERAGE if coverage is None else coverage)
    out += table("Blind spots", inventory_check.BLIND_COLUMNS, [])
    return "\n".join(out)


def read_line(group, section, key, target, prov, detail, channel="slice", caller="p.py:f"):
    site = "-" if section is None else "%s\t%s" % (section, key)
    return "\t".join([group, site, caller, channel, target, prov, detail])


def write_line(group, section, key, n, sha1):
    site = "-" if section is None else "%s\t%s" % (section, key)
    return "%s\t%s\tn=%d\tsha1=%s" % (group, site, n, sha1)


def planted_run(seed_index, outcome="ok"):
    reads = [
        read_line("parse", GEN, PLANTED_ROWS[0][1], "bank 0x3F", "source", "n=1 0x0147..0x0147",
                  channel="index"),
        read_line("items", GEN, "patches.p.items(rom,", "bank 0x03", "source",
                  "n=1 0x0000..0x0000"),
        read_line("items", GEN, "logic)", "bank 0x04", "written:fixed", "n=2 0x0000..0x0001"),
        read_line("items", GEN, "patches.p.items(rom,", "table entities", "source",
                  "n=3 0x0001..0x0003", channel="table"),
        read_line("-", GEN, "for c in rom.banks[0][0x10:0x12]:", "bank 0x00", "source",
                  "n=1 0x0010..0x0011"),
        read_line("sum", GEN, "patches.p.summed(rom, c)", "bank 0x01", "written:items",
                  "n=2 0x0000..0x0003"),
        read_line("sum", GEN, "patches.p.summed(rom, c)", "bank 0x05", "source", "n=1",
                  channel="iter"),
        read_line("sum", GEN, "patches.p.summed(rom, c)", "bank 0x06", "source", "n=1",
                  channel="iter"),
        read_line("later", GEN, "patches.p.later(rom)", "bank 0x02", "written:fixed",
                  "n=1 0x0000..0x0001"),
        read_line("-", HDR, "for c in self.banks[0][0x134:0x14D]:", "bank 0x00", "source",
                  "n=1 0x0134..0x014C"),
    ]
    writes = [
        write_line("fixed", GEN, "patches.p.fixed(rom)", 4, "f" * 40),
        write_line("items", GEN, "patches.p.items(rom,", 2, "%x" % seed_index * 40),
        write_line("sum", GEN, "patches.p.summed(rom, c)", 1, "5" * 40),
        write_line("later", GEN, "patches.p.later(rom)", 1, "6" * 40),
        write_line("header", HDR, "self.banks[0][0x14D] = checksum & 0xFF", 1,
                   "%x" % (seed_index + 7) * 40),
    ]
    return reads, writes, outcome


def write_traces(root, runs):
    for (case, seed), (reads, writes, outcome) in runs.items():
        folder = os.path.join(root, case, seed)
        os.makedirs(folder, exist_ok=True)
        for name, lines in (("reads.txt", reads), ("writes.txt", writes),
                            ("run.txt", ["seed -", "outcome " + outcome])):
            with open(os.path.join(folder, name), "w") as f:
                f.write("".join(line + "\n" for line in lines))


def planted_runs():
    runs = {("a", "seed-%d" % k): planted_run(k) for k in range(3)}
    runs.update({("b", "seed-%d" % k): planted_run(k) for k in range(3)})
    reads, writes, _ = runs[("b", "seed-1")]
    writes = [w.replace("f" * 40, "e" * 40) for w in writes]
    runs[("b", "seed-1")] = (reads, writes, "error")
    return runs


def _with_read(line):
    def mutate(runs):
        reads, writes, outcome = runs[("a", "seed-0")]
        runs[("a", "seed-0")] = (reads + [line], writes, outcome)
    return mutate


def _with_write(line):
    def mutate(runs):
        reads, writes, outcome = runs[("a", "seed-0")]
        runs[("a", "seed-0")] = (reads, writes + [line], outcome)
    return mutate


def _replace_in_runs(old, new, seed="seed-1"):
    def mutate(runs):
        reads, writes, outcome = runs[("a", seed)]
        runs[("a", seed)] = ([r.replace(old, new) for r in reads],
                             [w.replace(old, new) for w in writes], outcome)
    return mutate


def _cover(old, new):
    return lambda c: [line.replace(old, new) for line in c]


def _rows(index, **changes):
    def mutate(rows):
        rows = list(rows)
        section, key, side, group, cell = rows[index]
        rows[index] = (section, key, changes.get("side", side), changes.get("group", group),
                       changes.get("cell", cell))
        return rows
    return mutate


SELF_TEST_CASES = [
    # (name, runs mutation, rows mutation, coverage mutation, groups mutation, expected kinds)
    ("clean", None, None, None, None, []),
    ("untagged read", _with_read(read_line("-", GEN, "nowhere(rom)", "bank 0x09", "source",
                                           "n=1 0x0000..0x0000")), None, None, None,
     ["UNTAGGED"]),
    ("header governing no row", _with_read(read_line("-", GEN, "while rom.banks[0][0x20]:",
                                                     "bank 0x00", "source",
                                                     "n=1 0x0020..0x0020")),
     None, None, None, ["UNTAGGED"]),
    ("uncovered read", None, None, lambda c: c[:1] + c[2:], None, ["UNCOVERED"]),
    ("written over a source read", None, None,
     _cover("| bank 0x00 | 0x0010..0x0011 | copy |", "| bank 0x00 | 0x0010..0x0011 | written |"),
     None, ["BAD-COVER"]),
    ("table kind over a C group's bytes", None, None,
     _cover("| bank 0x01 | * | written |", "| bank 0x01 | * | table |"), None, ["BAD-COVER"]),
    ("computed outside the header checksum", None, None,
     _cover("| copy | moved to bank 0x01 |", "| computed | moved |"), None, ["BAD-COVER"]),
    ("rom read without a Reason", None, None,
     _cover("| rom | decides the sprite sets |", "| rom |  |"), None, ["BAD-COVER"]),
    ("unknown cut", None, None, _cover("cut:no-x", "cut:no-y"), None, ["BAD-COVER"]),
    ("entry matching no read", None, None, lambda c: c + [
        "| items | * | bank 0x30 | * | copy | nowhere |"], None, ["BAD-COVER"]),
    ("none row reading", _with_read(read_line("consts", GEN, 'assembler.const("X", 1)',
                                              "bank 0x09", "source", "n=1 0x0000..0x0000")),
     _rows(5, cell="bank 0x09 source 0x0000..0x0000"), None, None, ["NONE-ACCESS"]),
    ("unused row writing", _with_write(write_line("debug", GEN, "patches.p.debug(rom)", 1,
                                                  "1" * 40)), None, None, None,
     ["UNUSED-REACHED"]),
    ("table row varying with the seed", _replace_in_runs("f" * 40, "d" * 40), None, None, None,
     ["SEED-VARIES"]),
    ("table row reading a C group's bytes",
     _replace_in_runs("written:fixed\tn=1", "written:items\tn=1", "seed-0"),
     _rows(4, cell="bank 0x02 written:fixed 0x0000..0x0001; "
                   "bank 0x02 written:items 0x0000..0x0001"), None, None,
     ["TABLE-READS-SEED", "DEPENDS"]),
    ("wrong Depends on", None, None, None,
     lambda g: [("sum", "C", "fixed", "sum") if x[0] == "sum" else x for x in g], ["DEPENDS"]),
    ("wrong Reads (trace)", None, _rows(4, cell="-"), None, None, ["READS-COLUMN"]),
]


SELF_TEST_SETUP = [
    ("traces older than the inventory", _replace_in_runs("later\tgenerator.py",
                                                         "fixed\tgenerator.py", "seed-0")),
]


def _run_planted(root, runs_mutation, rows_mutation, coverage_mutation, groups_mutation, tag):
    traces = os.path.join(root, "traces-" + tag)
    runs = planted_runs()
    if runs_mutation:
        runs_mutation(runs)
    write_traces(traces, runs)
    rows = rows_mutation(PLANTED_ROWS) if rows_mutation else None
    coverage = coverage_mutation(PLANTED_COVERAGE) if coverage_mutation else None
    groups = groups_mutation(PLANTED_GROUPS) if groups_mutation else None
    return check(os.path.join(root, "ladxr"), planted_inventory(rows, groups, coverage), traces,
                 PLANTED_CUTS)


def self_test(out=None):
    out = out or sys.stdout
    failures = []
    with tempfile.TemporaryDirectory(prefix="read-check-") as root:
        for rel, text in PLANTED_SOURCES.items():
            path = os.path.join(root, "ladxr", rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as f:
                f.write(text)
        for n, (name, runs_m, rows_m, cover_m, groups_m, expected) in enumerate(SELF_TEST_CASES):
            try:
                checker = _run_planted(root, runs_m, rows_m, cover_m, groups_m, str(n))
                found = sorted({line.split()[0] for line in checker.findings})
                if name == "clean":
                    infos = sorted(checker.infos)
                    want = sorted(["STABLE %s: patches.p.summed(rom, c)" % GEN,
                                   "UNREACHED %s: patches.p.never(rom)" % GEN,
                                   "UNREACHED %s: checksum = 0" % HDR])
                    if infos != want:
                        failures.append("clean: information %s, expected %s" % (infos, want))
            except SetupError as e:
                found = ["setup: %s" % e]
            if found != sorted(set(expected)):
                failures.append("%s: %s, expected %s" % (name, " ".join(found) or "no finding",
                                                         " ".join(expected) or "no finding"))
        for n, (name, runs_m) in enumerate(SELF_TEST_SETUP):
            try:
                _run_planted(root, runs_m, None, None, None, "setup-%d" % n)
                failures.append("%s: no setup error" % name)
            except SetupError:
                pass
    for failure in failures:
        print("read-check: self-test failed: " + failure, file=out)
    if failures:
        return 1
    print("read-check: self-test, %d planted trace sets as expected"
          % (len(SELF_TEST_CASES) + len(SELF_TEST_SETUP)), file=out)
    return 0


def _read_text(path):
    if not os.path.isfile(path):
        return None
    with open(path, "rb") as f:
        return f.read().decode("utf-8")


def main(argv):
    parser = argparse.ArgumentParser(prog="read_check.py")
    parser.add_argument("--ladxr", default=DEFAULT_LADXR)
    parser.add_argument("--inventory", default=DEFAULT_INVENTORY)
    parser.add_argument("--traces", default=DEFAULT_TRACES)
    parser.add_argument("--scope-cuts", default=DEFAULT_SCOPE_CUTS)
    parser.add_argument("--print", action="store_true", dest="print_")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if self_test(sys.stderr if args.print_ else sys.stdout):
        return 1
    if not os.path.isfile(os.path.join(args.ladxr, "generator.py")):
        print("setup: read-check: no LADXR checkout in %s, run tests/ladx/oracle/setup.sh"
              % args.ladxr)
        return 2
    text = _read_text(args.inventory)
    if text is None:
        print("setup: read-check: no inventory %s" % args.inventory)
        return 2
    try:
        checker = check(args.ladxr, text, args.traces, _read_text(args.scope_cuts))
    except SetupError as e:
        print("setup: read-check: %s" % e)
        return 2
    if args.print_:
        sys.stdout.write(checker.refreshed(text))
        return 0
    for line in checker.findings + checker.infos + [checker.summary()]:
        print(line)
    return 1 if checker.findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
