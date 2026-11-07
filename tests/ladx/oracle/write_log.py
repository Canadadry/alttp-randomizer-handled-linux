"""Usage: python3 -I tests/ladx/oracle/write_log.py encode --events <file> --rom <file> [--inventory <file>] [--invariant <file>] [--out <file>]
       python3 -I tests/ladx/oracle/write_log.py decode --rom <file> <rom_writes.txt>
       python3 -I tests/ladx/oracle/write_log.py project <rom_writes.txt> <group>
       python3 -I tests/ladx/oracle/write_log.py scrambled --events <file> --rom <file> [--inventory <file>]
       python3 -I tests/ladx/oracle/write_log.py --self-test

LADX's write log, `rom_writes.txt`: what LADXR writes into the ROM, in its write order, tagged by
inventory group, every byte that comes from the source ROM a reference, never its value.

`encode` turns read_trace.py's event stream (oracle_run.py --reads' events.txt) and the source
ROM into the log (to --out, else stdout), with the groups' sides from the inventory's Groups
table (--inventory, default notes/ladx-inventory.md) and the C-knowable source map of
known_source.py; with --invariant, the oracle's re-run record (oracle_run.py --reads'
invariant.txt, parse_invariant) gives its `I` lines. `decode` replays the log's bank records onto the ROM and prints the result's
SHA-1. `project` prints one group's projection (itself a log). `scrambled` re-encodes the stream
with every ROM byte outside the C-knowable map scrambled, the table groups' encoding kept from
the real ROM (C copies it from step 6's tables), and prints one `SCRAMBLED <group>` line per C
group whose projection changed, then a summary; exit 1 on any. `--self-test` runs a
synthetic encode, decode, projection and scramble.

Format, version 1: line-based ASCII, lowercase hex without 0x.
  ladx-rom-writes 1
  tables <table>...                   ROMWithTables' tables in their order
  G <group>                           opens a segment: a maximal run of one group's writes
  B <offset> <tokens>                 a maximal run of the segment's net bank bytes at that
                                      absolute ROM offset, records by offset
  T <table> <key> <tokens>            a table entry's net new value, key an index in hex or an
                                      alt key, records in table order then index then alt key
  I <line> <start>+<n>...             after every segment, by line: the B or T record on that
                                      line holds invariant literal bytes at those indices
Tokens, single spaces between: `=<n>` keep (n source bytes at the record's own offsets, bank
records only), `@<q>+<n>` copy (n source bytes from absolute offset q), a run of literal hex,
`p<v>` an integer-pointer entry (alone in its record). Continuing references are merged and
adjacent literals joined; the parser refuses anything else, so equal logs are equal text.
`I` lines are evidence for the Nintendo-bytes check only: decode and projections ignore them, and
C never writes them. A range is a maximal run of at least 4 literal bytes of a C group's record,
each the net byte of a write the re-run flagged unchanged (invariant_run.py), holding a 4-byte
window of the source ROM; ranges in order, never touching.

Encoding rule. A byte's origin in the stream is `src(q)` (an unchanged copy of source byte q) or
the write identity of the computed byte it copies unchanged; a carried origin is the reference q,
or the encoding that byte got when it was first written. Table groups (side `table`), per byte:
keep when it equals the source at its own offset (bank records); else its carried origin; else
the longest source match of at least 4 bytes among the bytes left, lowest offset; else the
fragment rule below (not a longest match: gen_tables.py's couplings never see it); else a
literal. C groups, per byte: a carried origin; else a byte equal to the byte it replaces
inherits that byte's encoding when
C can know it (a literal, or a reference into the C-knowable map), else is a literal; then, once
the segment ends, each maximal run of its net literal bytes (bank bytes at contiguous offsets,
each entry alone) is matched, left to right, longest first, against the C-knowable map taken as
maximal runs of contiguous source offsets: a match of at least 4 bytes inside one run becomes a
copy of its lowest offset (KS-match; C parses those bytes from the user's ROM at runtime); then
the fragment rule, the writes its pieces name renamed as KS-match does. The fragment rule, on a
record's net atoms: a `T` entry is cut against the source bytes of its own KS entry (KnownSource
`entries`, alt keys included; none for texts or a key outside KS), bank bytes against the KS span
holding each offset with the lowest start (`span_at`; runs cut where it changes, offsets outside
KS skipped); each maximal literal run is walked left to right, the longest prefix of at least 1
byte occurring in those source bytes becoming a copy of its lowest offset there, the walk moving
on one byte where no byte occurs; nothing applies when known has no `entries`. The
replaced byte is the same offset's for a bank write; for a table entry the previous value, the
new one aligned to it by common prefix, then common suffix, each stopping at a byte C cannot
know; between them, left to right, the longest run of at least 4 bytes without a carried origin
equal to knowable bytes of the previous value, at its lowest index there, inherits their
encoding. C bytes are encoded event by event
against the state before; table groups' net bytes once their segment ends. Within a segment the
writes collapse to their net effect.

Projection: one group's segments replayed in order into its own bank and table maps, other
groups ignored, re-encoded as one canonical segment; no ROM needed.
"""

import argparse
import bisect
import hashlib
import os
import random
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
DEFAULT_INVENTORY = os.path.join(TOP, "notes", "ladx-inventory.md")

VERSION_LINE = "ladx-rom-writes 1"
EVENTS_LINE = "ladx-events 1"
KNOWN = -1
MIN_MATCH = 4
HEX = re.compile(r"[0-9a-f]+")
NAME = re.compile(r"[A-Za-z0-9_.-]+")


class WriteLogError(Exception):
    pass


class Events:
    """An event stream: table order, parsed entries (table, key, value, origins) and events as
    read_trace.Trace.events() gives them."""

    def __init__(self, tables, initial, events, source=None):
        self.tables, self.initial, self.events, self.source = tables, initial, events, source


# Atoms: a byte's encoding, q >= 0 a reference to source byte q, -1 - v the literal v.

def _lit(value):
    return -1 - value


def _value(atom, rom):
    return rom[atom] if atom >= 0 else -1 - atom


def _knowable(atom, known):
    return atom < 0 or atom in known


# Event stream ----------------------------------------------------------------------------------

def _hex_int(text, what):
    if not HEX.fullmatch(text):
        raise WriteLogError("bad %s %r" % (what, text))
    return int(text, 16)


def _key(text):
    return int(text, 16) if HEX.fullmatch(text) else text


def _origins(text):
    if text == "-":
        return ()
    out = []
    for run in text.split(","):
        if run.startswith("k"):
            out += [KNOWN] * _hex_int(run[1:], "origin run")
        elif run.startswith("v"):
            i, _, n = run[1:].partition("+")
            i, n = _hex_int(i, "write identity"), _hex_int(n, "origin run")
            out += [-2 - j for j in range(i, i + n)]
        else:
            q, _, n = run.partition("+")
            q, n = _hex_int(q, "origin"), _hex_int(n, "origin run")
            out += range(q, q + n)
    return tuple(out)


def _payload(value_text, origin_text):
    if value_text.startswith("p"):
        return _hex_int(value_text[1:], "pointer"), None
    value = b"" if value_text == "-" else bytes.fromhex(value_text)
    origins = _origins(origin_text)
    if len(origins) != len(value):
        raise WriteLogError("origins out of step with the value")
    return value, origins


def parse_events(text):
    """The Events of an event stream's text; WriteLogError on a malformed one."""
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if len(lines) < 3 or lines[0] != EVENTS_LINE or not lines[1].startswith("source ") \
            or not lines[2].startswith("tables"):
        raise WriteLogError("not an event stream (no `%s`, source and tables lines)" % EVENTS_LINE)
    stream = Events(lines[2].split()[1:], [], [], lines[1].split()[1])
    for number, line in enumerate(lines[3:], 4):
        fields = line.split("\t")
        try:
            if fields[0] == "I" and len(fields) == 5:
                stream.initial.append((fields[1], _key(fields[2])) + _payload(fields[3], fields[4]))
            elif fields[0] == "B" and len(fields) >= 6:
                value, origins = _payload(fields[3], fields[4])
                stream.events.append(("B", fields[1], "\t".join(fields[5:]),
                                      _hex_int(fields[2], "offset"), value, origins))
            elif fields[0] == "T" and len(fields) >= 7:
                stream.events.append(("T", fields[1], "\t".join(fields[6:]), fields[2],
                                      _key(fields[3])) + _payload(fields[4], fields[5]))
            else:
                raise WriteLogError("unknown event line")
        except (WriteLogError, ValueError) as e:
            raise WriteLogError("event stream line %d: %s" % (number, e))
    return stream


INVARIANT_LINE = "ladx-invariant-events 1"


def parse_invariant(text):
    """{event index: per-byte flags} of the oracle's re-run record (invariant.txt: the
    INVARIANT_LINE, then `<event index> <0 or 1 per byte>`, indices increasing, hex)."""
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or lines[0] != INVARIANT_LINE:
        raise WriteLogError("not a re-run record (no `%s` line)" % INVARIANT_LINE)
    flags, last = {}, -1
    for number, line in enumerate(lines[1:], 2):
        index, _, bits = line.partition(" ")
        if not HEX.fullmatch(index) or not re.fullmatch(r"[01]+", bits) or int(index, 16) <= last:
            raise WriteLogError("re-run record line %d: bad line" % number)
        last = int(index, 16)
        flags[last] = tuple(b == "1" for b in bits)
    return flags


def invariant_lines(flags):
    """parse_invariant's text, as lines, for {event index: per-byte flags}."""
    return [INVARIANT_LINE] + ["%x %s" % (n, "".join("1" if f else "0" for f in flags[n]))
                               for n in sorted(flags)]


# Encoding --------------------------------------------------------------------------------------

def _longest_match(rom, data, start, stop):
    """(offset, length) of the longest match in rom of data[start:stop]'s prefix, at least
    MIN_MATCH bytes, at its lowest offset; None when there is none."""
    room = stop - start
    if room < MIN_MATCH or rom.find(data[start:start + MIN_MATCH]) < 0:
        return None
    low, high = MIN_MATCH, None
    while low < room:
        trial = min(room, low * 2)
        if rom.find(data[start:start + trial]) < 0:
            high = trial
            break
        low = trial
    while high is not None and high - low > 1:
        mid = (low + high) // 2
        if rom.find(data[start:start + mid]) >= 0:
            low = mid
        else:
            high = mid
    return rom.find(data[start:start + low]), low


def _table_atoms(rom, values, origins, offset=None, matches=None):
    """A table group's atoms for a run of net bytes (at offset when they are bank bytes);
    origins: per byte a source offset, an atom from a write identity, or None; matches, when
    given, gets the (index, n) of every run the longest-match rule chose."""
    atoms = [None] * len(values)
    for i, v in enumerate(values):
        if offset is not None and offset + i < len(rom) and rom[offset + i] == v:
            atoms[i] = offset + i
        elif origins[i] is not None:
            atoms[i] = origins[i]
    i = 0
    while i < len(values):
        if atoms[i] is not None:
            i += 1
            continue
        stop = i
        while stop < len(values) and atoms[stop] is None:
            stop += 1
        while i < stop:
            match = _longest_match(rom, values, i, stop)
            if match is None:
                atoms[i] = _lit(values[i])
                i += 1
            else:
                at, length = match
                atoms[i:i + length] = range(at, at + length)
                if matches is not None:
                    matches.append((i, length))
                i += length
    return atoms


def _fragment_atoms(rom, known, where, atoms):
    """atoms with the fragment rule applied: within the source bytes each literal replaces (a
    table entry's KS entry at where = (table, key); for bank bytes at offset where, the
    lowest-start KS span of each offset, runs cut where it changes), each maximal literal run
    walked left to right, the longest prefix of at least 1 byte occurring there a copy of its
    lowest offset there; atoms itself when known has no `entries` and `span_at`."""
    entries, span_at = getattr(known, "entries", None), getattr(known, "span_at", None)
    if entries is None or span_at is None or isinstance(atoms, int):
        return atoms
    if isinstance(where, tuple):
        cuts = [(0, len(atoms), entries[where])] if where in entries else []
    else:
        cuts = []
        for i in range(len(atoms)):
            span = span_at(where + i)
            if span is None:
                continue
            if cuts and cuts[-1][1] == i and cuts[-1][2] == span:
                cuts[-1] = (cuts[-1][0], i + 1, span)
            else:
                cuts.append((i, i + 1, span))
    out = list(atoms)
    for start, stop, (low, high) in cuts:
        source = rom[low:high]
        j = start
        while j < stop:
            n = 0
            while j + n < stop and out[j + n] < 0 \
                    and bytes(-1 - a for a in out[j:j + n + 1]) in source:
                n += 1
            if n:
                at = low + source.find(bytes(-1 - a for a in out[j:j + n]))
                out[j:j + n] = range(at, at + n)
                j += n
            else:
                j += 1
    return out


class _KnownRuns:
    """The C-knowable source map as maximal runs of contiguous offsets: `match` finds the
    longest run of at least MIN_MATCH bytes inside one of them, at its lowest offset."""

    def __init__(self, rom, known):
        mask = getattr(known, "mask", None)
        offsets = ([q for q in range(len(rom)) if q < len(mask) and mask[q] == 1]
                   if mask is not None else sorted(q for q in known if 0 <= q < len(rom)))
        self.starts, pieces = [], []
        for start, stop in _spans(offsets):
            self.starts.append(start)
            pieces.append(rom[start:stop])
        self.bases, at = [], 0
        for piece in pieces:
            self.bases.append(at)
            at += len(piece)
        self.joined = b"".join(pieces)

    def find(self, data):
        """The lowest source offset of data inside one run, -1 when there is none."""
        pos = self.joined.find(data)
        while pos >= 0:
            i = bisect.bisect_right(self.bases, pos) - 1
            end = self.bases[i + 1] if i + 1 < len(self.bases) else len(self.joined)
            if pos + len(data) <= end:
                return self.starts[i] + pos - self.bases[i]
            pos = self.joined.find(data, pos + 1)
        return -1

    def match(self, data, start, stop):
        """(offset, length) of the longest match of data[start:stop]'s prefix, None when it is
        under MIN_MATCH bytes."""
        if stop - start < MIN_MATCH or self.find(data[start:start + MIN_MATCH]) < 0:
            return None
        low, high = MIN_MATCH, stop - start + 1
        while high - low > 1:
            mid = (low + high) // 2
            if self.find(data[start:start + mid]) >= 0:
                low = mid
            else:
                high = mid
        return self.find(data[start:start + low]), low

    def atoms(self, atoms):
        """atoms with every run of at least MIN_MATCH literals that matches the map made copies,
        greedily left to right, longest first."""
        out, i = list(atoms), 0
        while i < len(out):
            if out[i] >= 0:
                i += 1
                continue
            stop = i
            while stop < len(out) and out[stop] < 0:
                stop += 1
            values = bytes(-1 - a for a in out[i:stop])
            j = 0
            while j < len(values):
                found = self.match(values, j, len(values))
                if found is None:
                    j += 1
                else:
                    out[i + j:i + j + found[1]] = range(found[0], found[0] + found[1])
                    j += found[1]
            i = stop
        return out


class _Encoder:
    """The running state: every ROM position's atom, every table entry's atoms (or int
    pointer), and the atom each write identity got when its byte was first written."""

    def __init__(self, events, rom, sides, known, fixed=None):
        """fixed: the table groups' encodings of an earlier run, reused segment by segment."""
        self.rom, self.sides, self.known = rom, sides, known
        self._known_runs = None
        self._windows = None
        self.fixed = None if fixed is None else list(fixed)
        self.table_segments = []
        self.table_details = []
        self.tables = list(events.tables)
        self.state = {}
        self.written = {}
        self.entries = {}
        for table, key, value, origins in events.initial:
            self.entries[(table, key)] = value if origins is None else [
                q if q >= 0 else _lit(v) for q, v in zip(origins, value)]

    def carried(self, origin):
        """The atom an origin gives by itself: a source reference, or the atom of the write it
        names; None for KNOWN and for a write identity not met yet."""
        if origin >= 0:
            return origin
        return self.written.get(origin)

    def name(self, origins, atoms):
        """Records the atoms of the bytes whose write identity is met for the first time."""
        for q, atom in zip(origins, atoms):
            if q < KNOWN and q not in self.written:
                self.written[q] = atom

    def knowable(self, atom):
        return _knowable(atom, self.known)

    def known_runs(self):
        if self._known_runs is None:
            self._known_runs = _KnownRuns(self.rom, self.known)
        return self._known_runs

    def bank_atom(self, p):
        return self.state.get(p, p)

    def inherited(self, new, old, origins):
        """A C group's atoms for a table entry's new value replacing the atoms old (None when
        unknown): carried origins, then the common prefix and suffix, which stop at an old byte
        C cannot know so that where they stop never depends on the ROM; else literals."""
        old = old or []
        rom = self.rom
        same = lambda i, j: self.knowable(old[j]) and new[i] == _value(old[j], rom)
        prefix = 0
        while prefix < min(len(new), len(old)) and same(prefix, prefix):
            prefix += 1
        suffix = 0
        while suffix < min(len(new), len(old)) - prefix and same(-1 - suffix, -1 - suffix):
            suffix += 1
        atoms = []
        for i, v in enumerate(new):
            atom = self.carried(origins[i])
            if atom is None and i < prefix:
                atom = old[i]
            elif atom is None and i >= len(new) - suffix:
                atom = old[len(old) - (len(new) - i)]
            atoms.append(atom)
        self._inherit_middle(new, old, atoms, prefix, len(new) - suffix)
        return [_lit(v) if atom is None else atom for v, atom in zip(new, atoms)]

    def _inherit_middle(self, new, old, atoms, start, stop):
        """Fills atoms[start:stop] left to right: where no atom is set, the longest run of at
        least MIN_MATCH new bytes, none with an atom, equal to knowable old bytes, at its lowest
        old index, inherits their encoding."""
        rom = self.rom
        values = [_value(a, rom) if self.knowable(a) else None for a in old]
        i = start
        while i < stop:
            best, at = 0, None
            for j in range(len(values)):
                n = 0
                while i + n < stop and j + n < len(values) and atoms[i + n] is None \
                        and values[j + n] is not None and values[j + n] == new[i + n]:
                    n += 1
                if n > best:
                    best, at = n, j
            if best >= MIN_MATCH:
                atoms[i:i + best] = old[at:at + best]
                i += best
            else:
                i += 1

    def segment(self, group, events, flags=None):
        """(lines, marks) of one segment; flags: per event, the re-run's per-byte flags or
        None; marks: {index in lines: [(start, n)]} of the `I` ranges."""
        side = self.sides.get(group)
        if side not in ("table", "C"):
            raise WriteLogError("group %r writes but its side is %r, not table or C" % (group,
                                                                                       side))
        if side == "table":
            return self._table_segment(group, events), {}
        banks, tables, named = {}, {}, {}
        bank_flags, table_flags = {}, {}
        for event, flag in zip(events, flags or [None] * len(events)):
            if event[0] == "B":
                _, _, _, offset, values, origins = event
                for i, v in enumerate(values):
                    bank_flags[offset + i] = bool(flag and flag[i])
                    p = offset + i
                    atom = self.carried(origins[i])
                    if atom is None:
                        replaced = self.bank_atom(p)
                        atom = (replaced if _value(replaced, self.rom) == v
                                and self.knowable(replaced) else _lit(v))
                    self.name(origins[i:i + 1], [atom])
                    banks[p] = self.state[p] = atom
                    named[("B", p)] = [origins[i]]
                continue
            _, _, _, table, key, value, origins = event
            self._check_table(table)
            if isinstance(value, int):
                atoms = value
            else:
                previous = self.entries.get((table, key))
                atoms = self.inherited(value, previous if isinstance(previous, list) else None,
                                       origins)
                self.name(origins, atoms)
                named[("T", table, key)] = list(origins)
                table_flags[(table, key)] = [bool(flag and flag[i]) for i in range(len(value))]
            tables[(table, key)] = self.entries[(table, key)] = atoms
        self._match_known(banks, tables, named)
        self._match_fragments(banks, tables, named)
        lines = _records(group, banks, tables, self.tables)
        marks = {}
        if flags is not None:
            rows = [[(banks[p], bank_flags[p]) for p in range(start, stop)]
                    for start, stop in _spans(sorted(banks))]
            rows += [list(zip(tables[k], table_flags[k])) if isinstance(tables[k], list) else []
                     for k in sorted(tables, key=_key_order(self.tables))]
            for n, row in enumerate(rows, 1):
                ranges = self._marks(row)
                if ranges:
                    marks[n] = ranges
        return lines, marks

    def _marks(self, row):
        """[(start, n)] of a record's maximal runs of invariant literal atoms, at least MIN_MATCH
        long, that hold a MIN_MATCH-byte window of the source ROM; row: [(atom, flag)]."""
        out, i = [], 0
        while i < len(row):
            if not (row[i][0] < 0 and row[i][1]):
                i += 1
                continue
            stop = i
            while stop < len(row) and row[stop][0] < 0 and row[stop][1]:
                stop += 1
            values = bytes(-1 - a for a, _ in row[i:stop])
            windows = self.rom_windows()
            if any(values[j:j + MIN_MATCH] in windows
                   for j in range(len(values) - MIN_MATCH + 1)):
                out.append((i, stop - i))
            i = stop
        return out

    def rom_windows(self):
        if self._windows is None:
            rom = self.rom
            self._windows = {rom[i:i + MIN_MATCH] for i in range(len(rom) - MIN_MATCH + 1)}
        return self._windows

    def _match_known(self, banks, tables, named):
        """A C segment's net literal runs matching the C-knowable map become copies (bank runs
        by contiguous offsets, entries each alone); the writes they name follow."""
        if not any(a < 0 for a in banks.values()) and not any(
                isinstance(v, list) and any(a < 0 for a in v) for v in tables.values()):
            return
        runs = self.known_runs()
        for start, stop in _spans(sorted(banks)):
            old = [banks[p] for p in range(start, stop)]
            new = runs.atoms(old)
            for i, (a, b) in enumerate(zip(old, new)):
                if a != b:
                    p = start + i
                    banks[p] = self.state[p] = b
                    self._rename(named[("B", p)], [b])
        for (table, key), value in tables.items():
            if isinstance(value, list):
                new = runs.atoms(value)
                if new != value:
                    tables[(table, key)] = self.entries[(table, key)] = new
                    self._rename(named[("T", table, key)], new)

    def _match_fragments(self, banks, tables, named):
        """A C segment's net literal pieces of the source bytes they replace become copies (the
        fragment rule, _fragment_atoms); the writes they name follow."""
        for start, stop in _spans(sorted(banks)):
            old = [banks[p] for p in range(start, stop)]
            new = _fragment_atoms(self.rom, self.known, start, old)
            for i, (a, b) in enumerate(zip(old, new)):
                if a != b:
                    p = start + i
                    banks[p] = self.state[p] = b
                    self._rename(named[("B", p)], [b])
        for (table, key), value in tables.items():
            if isinstance(value, list):
                new = _fragment_atoms(self.rom, self.known, (table, key), value)
                if new != value:
                    tables[(table, key)] = self.entries[(table, key)] = new
                    self._rename(named[("T", table, key)], new)

    def _rename(self, origins, atoms):
        for q, atom in zip(origins, atoms):
            if q < KNOWN and atom >= 0:
                self.written[q] = atom

    def _check_table(self, table):
        if table not in self.tables:
            raise WriteLogError("table %r is not in the stream's table order" % table)

    def _table_segment(self, group, events):
        """A table group's net bytes and entries, encoded once its segment ends (or taken from
        the fixed encoding)."""
        net, entries = {}, {}
        for event in events:
            if event[0] == "B":
                _, _, _, offset, values, origins = event
                for i, v in enumerate(values):
                    net[offset + i] = (v, origins[i])
            else:
                self._check_table(event[3])
                entries[(event[3], event[4])] = (event[5], event[6])
        matches = []
        if self.fixed is not None:
            banks, tables = self.fixed.pop(0)
        else:
            banks = {}
            for start, stop in _spans(sorted(net)):
                values = bytes(net[p][0] for p in range(start, stop))
                origins = [self.carried(net[p][1]) for p in range(start, stop)]
                found = []
                atoms = _fragment_atoms(self.rom, self.known, start,
                                        _table_atoms(self.rom, values, origins, start, found))
                for i, atom in enumerate(atoms):
                    banks[start + i] = atom
                matches += [(start + i, n) for i, n in found]
            tables = {k: v if isinstance(v, int) else _fragment_atoms(
                          self.rom, self.known, k,
                          _table_atoms(self.rom, v, [self.carried(q) for q in o]))
                      for k, (v, o) in entries.items()}
        self.table_segments.append((banks, tables))
        self.table_details.append((group, list(events), banks, tables, matches))
        for p, (v, origin) in sorted(net.items()):
            self.name([origin], [banks[p]])
        for k, (v, o) in entries.items():
            if not isinstance(v, int):
                self.name(o, tables[k])
        self.state.update(banks)
        self.entries.update(tables)
        return _records(group, banks, tables, self.tables)


def _spans(positions):
    """[(start, stop)] of the maximal runs of sorted positions."""
    spans = []
    for p in positions:
        if spans and spans[-1][1] == p:
            spans[-1][1] = p + 1
        else:
            spans.append([p, p + 1])
    return [tuple(s) for s in spans]


def _key_order(tables):
    order = {t: n for n, t in enumerate(tables)}
    return lambda item: (order[item[0]], isinstance(item[1], str), item[1])


def _tokens(atoms, offset=None):
    """Canonical tokens of atoms, keep tokens only when offset is given."""
    out, i = [], 0
    while i < len(atoms):
        a = atoms[i]
        j = i + 1
        if a < 0:
            while j < len(atoms) and atoms[j] < 0:
                j += 1
            out.append(bytes(-1 - b for b in atoms[i:j]).hex())
        else:
            while j < len(atoms) and atoms[j] == a + j - i:
                j += 1
            if offset is not None and a == offset + i:
                out.append("=%x" % (j - i))
            else:
                out.append("@%x+%x" % (a, j - i))
        i = j
    return out


def _key_text(key):
    return "%x" % key if isinstance(key, int) else key


def _records(group, banks, tables, order):
    """The lines of a segment from its net bank atoms {offset: atom} and table entries
    {(table, key): atoms or int pointer}."""
    lines = ["G " + group]
    for start, stop in _spans(sorted(banks)):
        lines.append("B %x %s" % (start, " ".join(_tokens([banks[p] for p in range(start, stop)],
                                                         start))))
    for table, key in sorted(tables, key=_key_order(order)):
        value = tables[(table, key)]
        tokens = ["p%x" % value] if isinstance(value, int) else _tokens(value)
        lines.append(" ".join(["T", table, _key_text(key)] + tokens))
    return lines


def _header(tables):
    return [VERSION_LINE, " ".join(["tables"] + list(tables))]


def encode(events, rom, sides, known, invariant=None):
    """rom_writes.txt's text for Events on the source rom; sides {group: side}; known: the
    C-knowable source offsets (any container); invariant: {event index: per-byte flags} of the
    oracle's re-run (read_invariant), which gives the `I` lines, none when None."""
    return _encode(events, rom, sides, known, invariant=invariant)[0]


def table_segments(events, rom, sides, known):
    """The table groups' segments of encode's run, in order: (group, the segment's events, net
    bank atoms {offset: atom}, net entries {(table, key): atoms or int}, [(offset, n)] of the bank
    runs the longest-match rule chose)."""
    return _encode(events, rom, sides, known)[2]


def _encode(events, rom, sides, known, fixed=None, invariant=None):
    """(text, the table groups' encodings segment by segment, table_segments' details)."""
    encoder = _Encoder(events, rom, sides, known, fixed)
    lines = _header(events.tables)
    marks = []
    run, flags = [], []
    for n, event in enumerate(events.events + [None]):
        if run and (event is None or event[1] != run[0][1]):
            got, got_marks = encoder.segment(run[0][1], run,
                                             None if invariant is None else flags)
            marks += [(len(lines) + k + 1, ranges) for k, ranges in sorted(got_marks.items())]
            lines += got
            run, flags = [], []
        if event is not None:
            run.append(event)
            flags.append(None if invariant is None else invariant.get(n))
    lines += [_mark_line(line, ranges) for line, ranges in marks]
    return "".join(line + "\n" for line in lines), encoder.table_segments, encoder.table_details


def _mark_line(line, ranges):
    return " ".join(["I %x" % line] + ["%x+%x" % r for r in ranges])


# Parsing, decode, projection -------------------------------------------------------------------

def _parse_tokens(fields, offset, number):
    """[atom or int pointer] of a record's tokens; offset None for a table record."""
    atoms, previous = [], None
    for token in fields:
        kind = token[:1]
        if kind == "p" and offset is None and len(fields) == 1:
            pointer = _hex_int(token[1:], "pointer")
            if "%x" % pointer != token[1:]:
                raise WriteLogError("line %d: pointer %r not canonical" % (number, token))
            return pointer
        if kind == "=" and offset is not None:
            n = _hex_int(token[1:], "length")
            run = list(range(offset + len(atoms), offset + len(atoms) + n))
            kind = "ref"
        elif kind == "@":
            m = re.fullmatch(r"@([0-9a-f]+)\+([0-9a-f]+)", token)
            if not m:
                raise WriteLogError("line %d: bad copy token %r" % (number, token))
            q, n = int(m.group(1), 16), int(m.group(2), 16)
            run = list(range(q, q + n))
            kind = "ref"
        elif HEX.fullmatch(token) and len(token) % 2 == 0:
            run = [_lit(b) for b in bytes.fromhex(token)]
            kind = "lit"
        else:
            raise WriteLogError("line %d: bad token %r" % (number, token))
        if not run:
            raise WriteLogError("line %d: empty token %r" % (number, token))
        if previous == "lit" and kind == "lit" or previous == "ref" and kind == "ref" \
                and atoms[-1] + 1 == run[0]:
            raise WriteLogError("line %d: token %r continues the one before" % (number, token))
        if token != _tokens(run, None if offset is None else offset + len(atoms))[0]:
            raise WriteLogError("line %d: token %r not canonical" % (number, token))
        atoms += run
        previous = kind
    if not atoms and offset is not None:
        raise WriteLogError("line %d: a bank record without bytes" % number)
    return atoms


def parse(text):
    """(tables, [(group, {offset: atom}, {(table, key): atoms or int})]) of a log's text, every
    rule of the format checked; WriteLogError names the first broken one."""
    return _parse(text)[:2]


def invariants(text):
    """{line number: [(start, n)]} of a log's `I` lines, the log checked as parse does."""
    return _parse(text)[2]


_MARK_RANGE = re.compile(r"([0-9a-f]+)\+([0-9a-f]+)")


def _parse_mark(fields, number, records):
    """(line, [(start, n)]) of an `I` line's fields, checked against records {line: atoms}."""
    if len(fields) < 3:
        raise WriteLogError("line %d: an I line without a range" % number)
    line = _hex_int(fields[1], "line")
    atoms = records.get(line)
    if "%x" % line != fields[1] or not isinstance(atoms, list):
        raise WriteLogError("line %d: I line %s names no record" % (number, fields[1]))
    ranges, end = [], -1
    for field in fields[2:]:
        m = _MARK_RANGE.fullmatch(field)
        if not m:
            raise WriteLogError("line %d: bad range %r" % (number, field))
        start, n = int(m.group(1), 16), int(m.group(2), 16)
        if "%x+%x" % (start, n) != field or n == 0 or start <= end:
            raise WriteLogError("line %d: range %r not canonical" % (number, field))
        if start + n > len(atoms) or any(a >= 0 for a in atoms[start:start + n]):
            raise WriteLogError("line %d: range %r is not all literal" % (number, field))
        ranges.append((start, n))
        end = start + n
    return line, ranges


def _parse(text):
    """parse's (tables, segments) and invariants' marks."""
    lines = text.split("\n")
    if not lines or lines[-1] != "":
        raise WriteLogError("the log does not end with a newline")
    lines.pop()
    if not lines or lines[0] != VERSION_LINE:
        raise WriteLogError("line 1: not `%s`" % VERSION_LINE)
    if len(lines) < 2 or lines[1].split(" ")[0] != "tables":
        raise WriteLogError("line 2: no tables line")
    tables = lines[1].split(" ")[1:]
    if any(not NAME.fullmatch(t) for t in tables) or len(set(tables)) != len(tables):
        raise WriteLogError("line 2: bad table names")
    order = _key_order(tables)
    segments = []
    last_bank = last_table = None
    records, marks = {}, {}
    for number, line in enumerate(lines[2:], 3):
        fields = line.split(" ")
        if fields[0] == "I":
            mark, ranges = _parse_mark(fields, number, records)
            if marks and mark <= max(marks):
                raise WriteLogError("line %d: I lines out of order" % number)
            marks[mark] = ranges
            continue
        if marks:
            raise WriteLogError("line %d: a line after the I lines" % number)
        if fields[0] == "G" and len(fields) == 2 and NAME.fullmatch(fields[1]):
            if segments and not (segments[-1][1] or segments[-1][2]):
                raise WriteLogError("line %d: segment %s has no record" % (number - 1,
                                                                          segments[-1][0]))
            if segments and segments[-1][0] == fields[1]:
                raise WriteLogError("line %d: segment %s continues the one before" % (number,
                                                                                     fields[1]))
            segments.append((fields[1], {}, {}))
            last_bank = last_table = None
            continue
        if not segments:
            raise WriteLogError("line %d: a record before any G line" % number)
        group, banks, entries = segments[-1]
        if fields[0] == "B" and len(fields) >= 2:
            offset = _hex_int(fields[1], "offset")
            if "%x" % offset != fields[1]:
                raise WriteLogError("line %d: offset not canonical" % number)
            if entries or last_bank is not None and offset <= last_bank:
                raise WriteLogError("line %d: bank record out of order or not maximal" % number)
            atoms = _parse_tokens(fields[2:], offset, number)
            records[number] = atoms
            banks.update((offset + i, a) for i, a in enumerate(atoms))
            last_bank = offset + len(atoms)
        elif fields[0] == "T" and len(fields) >= 3:
            if fields[1] not in tables:
                raise WriteLogError("line %d: unknown table %r" % (number, fields[1]))
            key = _key(fields[2])
            if not (HEX.fullmatch(fields[2]) and "%x" % key == fields[2]
                    or not HEX.fullmatch(fields[2]) and NAME.fullmatch(fields[2])):
                raise WriteLogError("line %d: bad key %r" % (number, fields[2]))
            if last_table is not None and order((fields[1], key)) <= order(last_table):
                raise WriteLogError("line %d: table record out of order" % number)
            entries[(fields[1], key)] = _parse_tokens(fields[3:], None, number)
            records[number] = entries[(fields[1], key)]
            last_table = (fields[1], key)
        else:
            raise WriteLogError("line %d: unknown line %r" % (number, line[:40]))
    if segments and not (segments[-1][1] or segments[-1][2]):
        raise WriteLogError("segment %s has no record" % segments[-1][0])
    return tables, segments, marks


def decode(text, rom):
    """The ROM the log's bank records give when replayed in order onto the source rom."""
    _, segments = parse(text)
    out = bytearray(rom)
    for group, banks, _ in segments:
        for p, atom in banks.items():
            if p >= len(out) or atom >= len(rom):
                raise WriteLogError("segment %s writes or copies beyond the ROM" % group)
            out[p] = _value(atom, rom)
    return bytes(out)


def render(tables, segments):
    """The log text of parse's (tables, segments)."""
    lines = _header(tables)
    for group, banks, entries in segments:
        lines += _records(group, banks, entries, tables)
    return "".join(line + "\n" for line in lines)


def projections(text):
    """{group: project(text, group)} for every group of the log, from one parse."""
    tables, segments = parse(text)
    net = {}
    for name, seg_banks, seg_entries in segments:
        banks, entries = net.setdefault(name, ({}, {}))
        banks.update(seg_banks)
        entries.update(seg_entries)
    return {name: render(tables, [(name, banks, entries)])
            for name, (banks, entries) in net.items()}


def project(text, group):
    """One group's projection of a log: its segments replayed in order, as a log of at most one
    segment."""
    return projections(text).get(group) or render(parse(text)[0], [])


def groups(text):
    """The groups of a log's segments, each once, in first-write order."""
    seen = []
    for name, _, _ in parse(text)[1]:
        if name not in seen:
            seen.append(name)
    return seen


# Fragments -------------------------------------------------------------------------------------

def parse_record(line):
    """(where, atoms or int pointer) of one `B` or `T` record line: where its offset, or its
    (table, key); WriteLogError on any other line."""
    fields = line.split(" ")
    if fields[0] == "B" and len(fields) >= 3:
        offset = _hex_int(fields[1], "offset")
        return offset, _parse_tokens(fields[2:], offset, 0)
    if fields[0] == "T" and len(fields) >= 3 and NAME.fullmatch(fields[1]):
        return (fields[1], _key(fields[2])), _parse_tokens(fields[3:], None, 0)
    raise WriteLogError("not a B or T record: %r" % line[:40])


def _reference_cuts(where, length, known):
    """[(start, stop, source start, source stop)]: a record's indices start..stop and the source
    bytes they replace, for a table entry its KS entry, for bank bytes the lowest-start KS span
    holding each offset (indices outside KS have none)."""
    entries, span_at = getattr(known, "entries", None), getattr(known, "span_at", None)
    if entries is None or span_at is None:
        return []
    if isinstance(where, tuple):
        span = entries.get(where)
        return [] if span is None else [(0, length) + tuple(span)]
    cuts = []
    for i in range(length):
        span = span_at(where + i)
        if span is None:
            continue
        if cuts and cuts[-1][1] == i and tuple(cuts[-1][2:]) == tuple(span):
            cuts[-1][1] = i + 1
        else:
            cuts.append([i, i + 1] + list(span))
    return [tuple(c) for c in cuts]


def record_fragments(where, atoms, rom, known):
    """[(start, n)] of one record's fragments: within each cut of its indices (_reference_cuts),
    each maximal literal run walked left to right, a piece the longest prefix of at least 1 byte
    occurring in the cut's source bytes, the walk moving on one byte where none does."""
    if isinstance(atoms, int):
        return []
    out = []
    for start, stop, low, high in _reference_cuts(where, len(atoms), known):
        hay = rom[low:high]
        i = start
        while i < stop:
            if atoms[i] >= 0:
                i += 1
                continue
            end = i
            while end < stop and atoms[end] < 0:
                end += 1
            values = bytes(-1 - a for a in atoms[i:end])
            j = 0
            while j < len(values):
                n = 0
                while j + n < len(values) and values[j:j + n + 1] in hay:
                    n += 1
                if n:
                    out.append((i + j, n))
                    j += n
                else:
                    j += 1
            i = end
    return out


def fragments(text, rom, known):
    """[(group, line number, start, n)] of every fragment (record_fragments) of a log's `B` and
    `T` records, the log checked as parse does; none when known has no `entries` and
    `span_at`."""
    parse(text)
    out, group = [], None
    for number, line in enumerate(text.split("\n"), 1):
        if line.startswith("G "):
            group = line[2:]
        elif line.startswith(("B ", "T ")):
            where, atoms = parse_record(line)
            out += [(group, number, start, n)
                    for start, n in record_fragments(where, atoms, rom, known)]
    return out


# Scrambled-ROM comparison ----------------------------------------------------------------------

def scramble(rom, known):
    """rom with every byte outside known changed (xor a position-dependent nonzero mask)."""
    return bytes(b if p in known else b ^ (1 + p * 7 % 255) for p, b in enumerate(rom))


def compare_scrambled(events, rom, sides, known, scramble_known=None):
    """One `SCRAMBLED <group>` line per C group whose projection differs when the stream is
    re-encoded on rom with every byte outside scramble_known (default known) scrambled. The
    table groups keep their encoding from rom: C copies it from step 6's tables."""
    return encode_scrambled(events, rom, sides, known, scramble_known)[1]


def encode_scrambled(events, rom, sides, known, scramble_known=None, invariant=None):
    """(encode's text, compare_scrambled's lines) from one encoding on the real rom."""
    real, fixed, _ = _encode(events, rom, sides, known, invariant=invariant)
    other = _encode(events, scramble(rom, known if scramble_known is None else scramble_known),
                    sides, known, fixed)[0]
    lines = []
    real_projections, other_projections = projections(real), projections(other)
    for group in groups(real):
        if sides.get(group) != "C":
            continue
        a = real_projections[group].splitlines()
        b = other_projections.get(group, "").splitlines()
        if a != b:
            first = next(n for n in range(max(len(a), len(b)))
                         if n >= len(a) or n >= len(b) or a[n] != b[n])
            lines.append("SCRAMBLED %s: projection differs from line %d" % (group, first + 1))
    return real, lines


# Command line ----------------------------------------------------------------------------------

def sides_of(inventory_text):
    """{group: side} of an inventory's Groups table."""
    sys.path.insert(0, HERE)
    import inventory_check
    named, bad = inventory_check.parse_named(inventory_text)
    if bad:
        raise WriteLogError("inventory: " + bad[0])
    return {row["Group"]: row["Side"] for row in named["Groups"]}


def _read(path, binary=False):
    with open(path, "rb") as f:
        data = f.read()
    return data if binary else data.decode("utf-8")


def _inputs(args):
    sys.path.insert(0, HERE)
    import known_source
    events = parse_events(_read(args.events))
    rom = _read(args.rom, binary=True)
    if events.source != hashlib.sha1(rom).hexdigest():
        raise WriteLogError("the event stream was recorded on another ROM")
    invariant = None if args.invariant is None else parse_invariant(_read(args.invariant))
    return events, rom, sides_of(_read(args.inventory)), known_source.build(rom), invariant


def self_test():
    """A synthetic stream through encode, decode, projection and the scramble; failure lines."""
    rom = random.Random(0x1ADE).randbytes(0x800)
    events = Events(["texts"], [("texts", 0, rom[0x100:0x104], tuple(range(0x100, 0x104)))], [
        ("B", "fix", "-", 0x10, rom[0x300:0x308], (KNOWN,) * 8),
        ("B", "cg", "-", 0x20, rom[0x40:0x42] + b"\x00", (0x40, 0x41, KNOWN)),
        ("T", "cg", "-", "texts", 0, rom[0x100:0x102] + b"\x09", (KNOWN,) * 3),
        ("B", "fix", "-", 0x30, rom[0x30:0x32], (KNOWN, KNOWN))])
    sides, known = {"fix": "table", "cg": "C"}, set(range(0x100, 0x104))
    failures = []
    log = encode(events, rom, sides, known)
    want = bytearray(rom)
    for e in events.events:
        if e[0] == "B":
            want[e[3]:e[3] + len(e[4])] = e[4]
    if decode(log, rom) != bytes(want):
        failures.append("decode does not give the written ROM")
    expected = ["G fix", "B 10 @300+8", "G cg", "B 20 @40+2 00", "T texts 0 @100+2 09",
                "G fix", "B 30 =2"]
    if log.splitlines()[2:] != expected:
        failures.append("encode gives %r" % log.splitlines()[2:])
    if project(log, "fix").splitlines()[2:] != ["G fix", "B 10 @300+8", "B 30 =2"]:
        failures.append("projection of fix")
    if compare_scrambled(events, rom, sides, known):
        failures.append("the scramble changes a C group")
    if not compare_scrambled(events, rom, sides, known, scramble_known=set()):
        failures.append("the scramble misses a C group reading outside its map")
    return failures


def main(argv):
    if argv == ["--self-test"]:
        failures = self_test()
        for failure in failures:
            print("write-log: self-test failed: " + failure)
        if not failures:
            print("write-log: self-test, encode, decode, projection and scramble")
        return 1 if failures else 0
    parser = argparse.ArgumentParser(prog="write_log.py")
    sub = parser.add_subparsers(dest="mode", required=True)
    for mode in ("encode", "scrambled"):
        p = sub.add_parser(mode)
        p.add_argument("--events", required=True)
        p.add_argument("--rom", required=True)
        p.add_argument("--inventory", default=DEFAULT_INVENTORY)
        p.add_argument("--invariant")
        if mode == "encode":
            p.add_argument("--out")
    p = sub.add_parser("decode")
    p.add_argument("--rom", required=True)
    p.add_argument("log")
    p = sub.add_parser("project")
    p.add_argument("log")
    p.add_argument("group")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    try:
        if args.mode == "decode":
            print(hashlib.sha1(decode(_read(args.log), _read(args.rom, True))).hexdigest())
        elif args.mode == "project":
            sys.stdout.write(project(_read(args.log), args.group))
        elif args.mode == "encode":
            events, rom, sides, known, invariant = _inputs(args)
            text = encode(events, rom, sides, known, invariant)
            if args.out:
                with open(args.out, "w", encoding="ascii", newline="\n") as f:
                    f.write(text)
            else:
                sys.stdout.write(text)
        else:
            events, rom, sides, known, _ = _inputs(args)
            lines = compare_scrambled(events, rom, sides, known)
            for line in lines:
                print(line)
            print("write-log: scrambled ROM, %d C groups changed" % len(lines))
            return 1 if lines else 0
    except (WriteLogError, OSError) as e:
        print("write-log: %s" % e)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
