"""Usage: python3 -I tests/ladx/oracle/read_trace.py --self-test [--ladxr <dir>]
       python3 -I tests/ladx/oracle/read_trace.py --blind-spots [--ladxr <dir>]

The read-trace instrumentation of LADXR's ROM reads and writes, for Phase 2 step 3. As a module:
`install(modules, sections, inventory=None, roots=None) -> Trace` instruments `modules["ROM"]`,
`modules["ROMWithTables"]` and `modules["PointerTable"]` so that every read of a bank's bytes or
of a parsed table, outside `ROM.patch`, is recorded as
`(group, site, caller, channel, target, detail, provenance)`, and every write as
`(group, site, caller, target, detail)`; `Trace.records()`, `Trace.writes()`,
`Trace.write(path, raw=False)` (reads), `Trace.write_writes(path)`, `Trace.uninstall()`.
`Trace.events()` is the ordered write record with values, `Trace.initial()` the parsed entries,
`Trace.tables()` the table order, `Trace.write_events(path)` the event stream (`event_lines`).

Origins: every ROM position starts as its own source offset. A bank slice comes back as an
`OBytes` (a bytearray with per-byte `origins`), an index read as an `OInt` (`origin`); slicing,
bytes() (`OFrozen`), concatenation, in-place changes and assignment to a bank or a table keep
them, so a parsed entry and PointerTable.store's moves and de-duplication do too. Arithmetic and
re-encoding give KNOWN, and a KNOWN byte written gets a write identity (`virtual(n)`, below
KNOWN) that its unchanged copies keep: a computed entry moved by `store` names the write that made
it. A written origin whose recorded byte differs from the value is renewed, so a missed path only
ever turns a reference into a new write.

Channels: `index` (bank[i]), `slice` (bank[a:b]), `iter`, `search` (find, rfind, index, rindex,
count, startswith, endswith, in), `compare` (==, !=, ordering), `convert` (hex, decode, bytes())
and `table` (getitem, in, len on a table attribute of a ROMWithTables). Reads through the buffer
protocol are invisible: `--blind-spots` lists their candidate lines.
Writes: index and slice assignments to a bank (inside `ROM.patch` too), a bank replaced whole
(every byte written), and a table entry assigned through `__setitem__` after parsing.
Provenance of a read: `source` when none of its bytes or entries was written since parsing,
`written:<g1,g2>` when all were (by those groups), `mixed:<g1,g2>` otherwise; `iter`, `search`,
`compare`, `convert` and `__len__` cover the whole bank or table. An empty slice reads no byte
and is not recorded.

Site: `<file> <function>\\t<stripped line>` of the innermost frame in one of `sections` (pairs
(file, function), the file relative to the first root), with ` #<k>` on the k-th occurrence of
the same line text in that function (k >= 2), else `-`. A read on a loop or condition header
keeps that header line. Group: `inventory[site]`, else `-`. Caller: `<file>:<qualname>` of the
innermost frame under a root (default: the ROM class's folder).

Aggregated reads (`write`): group, site, caller, channel, target, provenance, `n=<reads>` and
the offset range. Writes (`write_writes`): one line per writing site, group, site,
`n=<positions>` and `sha1=<hex>` over the sorted (target, offset or index, last value the site
wrote there).

`--self-test` instruments LADXR's real `rom.ROM` and `pointerTable.PointerTable` (from `--ladxr`,
default tests/ladx/oracle/ladxr) on 64 synthetic banks with planted reads, one per channel and
per provenance, a header-line read, and planted writes (bank, `patch`, table), then planted
copies (slice, index, concatenation, table entry, `store`'s move and suffix de-duplication) and a
computed write, checked event by event with their origins.
"""

import argparse
import hashlib
import io
import os
import random
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_LADXR = os.path.join(HERE, "ladxr")

SEARCH_METHODS = ("find", "rfind", "index", "rindex", "count", "startswith", "endswith",
                  "__contains__")
COMPARE_METHODS = ("__eq__", "__ne__", "__lt__", "__le__", "__gt__", "__ge__")


def _qualname(code):
    return getattr(code, "co_qualname", code.co_name)


def _offset(n):
    return "0x%04X" % n


def _function_keys(path, first_line):
    """{line number: key text} for the function whose `def` is at or after first_line, the
    k-th occurrence of a stripped line text keyed `<text> #<k>` for k >= 2."""
    with open(path, "rb") as f:
        lines = f.read().decode("utf-8", "replace").replace("\r", "").split("\n")
    n = first_line - 1
    while n < len(lines) and not re.match(r"\s*(async\s+)?def\b", lines[n]):
        n += 1
    if n >= len(lines):
        return {}
    indent = len(lines[n]) - len(lines[n].lstrip())
    keys, seen = {}, {}
    for number in range(n + 2, len(lines) + 1):
        line = lines[number - 1]
        text = line.strip()
        if not text:
            continue
        if len(line) - len(line.lstrip()) <= indent:
            break
        seen[text] = seen.get(text, 0) + 1
        keys[number] = text if seen[text] == 1 else "%s #%d" % (text, seen[text])
    return keys


class Trace:
    """The reads and writes recorded since `install`; `uninstall` restores the classes."""

    def __init__(self, modules, sections, inventory, roots):
        self._sections = [(f, fn) for f, fn in sections]
        self._inventory = dict(inventory or {})
        self._roots = [os.path.realpath(r) for r in roots]
        self._records = []
        self._writes = []
        self._events = []
        self._initial = []
        self._tables = []
        self._source = None
        self._bases = []
        self._virtual = bytearray()
        self._net = {}
        self._group_ids = {}
        self._group_names = [None]
        self._masks = {}
        self._entries = {}
        self._code_info = {}
        self._function_keys = {}
        self._table_names = {}
        self._table_classes = {}
        self._restore = []
        self.active = True
        self._patch_code = modules["ROM"].patch.__code__

    def records(self):
        return list(self._records)

    def writes(self):
        return list(self._writes)

    def tables(self):
        """The parsed tables' attribute names, in ROMWithTables.__init__'s order."""
        return list(self._tables)

    def initial(self):
        """Every parsed entry right after the parse, tables in order then indices then alt keys:
        (table, key, value, origins) as in events()."""
        return list(self._initial)

    def write_events(self, path):
        _save(path, self.event_lines())

    def event_lines(self):
        """The event stream: `ladx-events 1`, `source <sha1>`, `tables <names>`, one `I` line
        per parsed entry, then one `B` or `T` line per event, tab-separated, site last."""
        source = hashlib.sha1(self._source or b"").hexdigest()
        out = ["ladx-events 1", "source " + source, " ".join(["tables"] + self._tables)]
        for table, key, value, origins in self._initial:
            out.append("\t".join(["I", table, _key_text(key), _value_text(value),
                                   origin_text(origins or ())]))
        for event in self._events:
            if event[0] == "B":
                kind, group, site, offset, values, origins = event
                fields = ["B", group, "%x" % offset, _value_text(values), origin_text(origins)]
            else:
                kind, group, site, table, key, value, origins = event
                fields = ["T", group, table, _key_text(key), _value_text(value),
                          origin_text(origins or ())]
            out.append("\t".join(fields + [site]))
        return out

    def events(self):
        """Every write in order: ("B", group, site, absolute offset, values, origins) for a run
        of bank bytes, ("T", group, site, table, key, value, origins) for a table entry, value
        bytes with a tuple of origins or an int pointer with None. An origin is the source offset
        of an unchanged copy of a source byte, else KNOWN."""
        return list(self._events)

    def write(self, path, raw=False):
        _save(path, self.lines(raw))

    def write_writes(self, path):
        _save(path, self.write_lines())

    def lines(self, raw=False):
        if raw:
            return ["\t".join(r) for r in self._records]
        groups = {}
        for record in self._records:
            entry = groups.setdefault(record[:5] + record[6:], [0, None, None])
            entry[0] += 1
            span = _span(record[5])
            if span is not None:
                entry[1] = span[0] if entry[1] is None else min(entry[1], span[0])
                entry[2] = span[1] if entry[2] is None else max(entry[2], span[1])
        out = []
        for key in sorted(groups):
            count, low, high = groups[key]
            detail = "n=%d" % count
            if low is not None:
                detail += " %s..%s" % (_offset(low), _offset(high))
            out.append("\t".join(key + (detail,)))
        return out

    def write_lines(self):
        """One line per writing site: group, site, `n=<positions>`, `sha1=<digest>` of the
        sorted (target, offset or index, last value the site wrote there)."""
        out = []
        for (group, site), net in sorted(self._net.items()):
            digest = hashlib.sha1()
            for (target, where), value in sorted(net.items()):
                digest.update(("%s\t%s\t%s\n" % (target, where, _value(value))).encode())
            out.append("%s\t%s\tn=%d\tsha1=%s" % (group, site, len(net), digest.hexdigest()))
        return out

    def uninstall(self):
        self.active = False
        for owner, name, value in reversed(self._restore):
            setattr(owner, name, value)
        self._restore = []

    def _info(self, code):
        info = self._code_info.get(code)
        if info is None:
            path = os.path.realpath(code.co_filename)
            rel = None
            for root in self._roots:
                if path.startswith(root + os.sep):
                    rel = os.path.relpath(path, root).replace(os.sep, "/")
                    break
            qual = _qualname(code)
            # Before Python 3.11 there is no co_qualname: a section matches on its last name.
            short = not hasattr(code, "co_qualname")
            section = next(("%s %s" % (f, fn) for f, fn in self._sections if f == rel and
                            (fn == qual or short and fn.rsplit(".", 1)[-1] == qual)), None)
            info = (rel, qual, section)
            self._code_info[code] = info
        return info

    def _site(self, frame, section):
        code = frame.f_code
        keys = self._function_keys.get(code)
        if keys is None:
            keys = _function_keys(code.co_filename, code.co_firstlineno)
            self._function_keys[code] = keys
        return "%s\t%s" % (section, keys.get(frame.f_lineno, "?"))

    def _locate(self, depth, in_patch):
        """(group, site, caller) of the access `depth` frames up; None for a read inside
        ROM.patch (in_patch False)."""
        site = caller = None
        frame = sys._getframe(depth + 1)
        while frame is not None:
            code = frame.f_code
            if code is self._patch_code and not in_patch:
                return None
            rel, qual, section = self._info(code)
            if rel is not None:
                if caller is None:
                    caller = "%s:%s" % (rel, qual)
                if site is None and section is not None:
                    site = self._site(frame, section)
            frame = frame.f_back
        site = site or "-"
        return self._inventory.get(site, "-"), site, caller or "-"

    def record(self, channel, target, detail, provenance):
        """provenance: a callable giving the read's provenance field."""
        if not self.active:
            return
        where = self._locate(2, False)
        if where is not None:
            self._records.append(where + (channel, target, detail, provenance()))

    def record_write(self, target, detail, positions, events=()):
        """positions: [(offset or index key, value now there)]; events: the write's event
        payloads, tagged here with group and site; returns the writer's group id for the
        caller's provenance mask, None when inactive."""
        if not self.active:
            return None
        group, site, caller = self._locate(2, True)
        self._writes.append((group, site, caller, target, detail))
        for event in events:
            self._events.append((event[0], group, site) + tuple(event[1:]))
        net = self._net.setdefault((group, site), {})
        for where, value in positions:
            net[(target, where)] = value
        gid = self._group_ids.get(group)
        if gid is None:
            assert len(self._group_names) < 256, "more than 255 writing groups"
            gid = self._group_ids[group] = len(self._group_names)
            self._group_names.append(group)
        return gid

    def checked(self, origins, values):
        """origins as written: one whose recorded byte differs from the value (a change no
        traced path saw, the buffer protocol) made KNOWN, then every KNOWN byte given a new
        write identity (`virtual`), so that its unchanged copies keep it."""
        source, written = self._source, self._virtual
        out = []
        for q, v in zip(origins, values):
            if q >= 0:
                q = q if source is None or q < len(source) and source[q] == v else KNOWN
            elif q != KNOWN:
                q = q if -2 - q < len(written) and written[-2 - q] == v else KNOWN
            if q == KNOWN:
                q = virtual(len(written))
                written.append(v)
            out.append(q)
        return out

    def provenance(self, ids):
        """`source`, `written:<groups>` or `mixed:<groups>` of a set of group ids, 0 the
        source."""
        names = sorted({self._group_names[i] for i in ids if i})
        if not names:
            return "source"
        return "%s:%s" % ("mixed" if 0 in ids else "written", ",".join(names))


def _save(path, lines):
    with open(path, "w") as f:
        for line in lines:
            f.write(line + "\n")


def _value(value):
    if isinstance(value, int):
        return "%d" % value
    if isinstance(value, (bytes, bytearray)):
        return bytes(value).hex()
    return repr(value)


KNOWN = -1


def virtual(n):
    """The origin of the n-th computed byte written (its write identity), below KNOWN."""
    return -2 - n


def _origins_of(value):
    """The per-byte origins of a value assigned to a bank or a table: its own when it carries
    them, KNOWN for every byte otherwise. value: an int, a bytes-like or a list of ints."""
    origins = getattr(value, "origins", None)
    if origins is not None:
        return list(origins)
    if isinstance(value, int):
        return [getattr(value, "origin", KNOWN)]
    if isinstance(value, (bytes, bytearray, memoryview)):
        return [KNOWN] * len(value)
    return [getattr(v, "origin", KNOWN) for v in value]


def _assignable(value):
    """value as bank or entry assignment accepts it, a one-shot iterable made a list."""
    if isinstance(value, (int, bytes, bytearray, memoryview)):
        return value
    return list(value)


class OInt(int):
    """A byte read from a bank or an origin-carrying value: `origin` is its source offset."""


def _oint(value, origin):
    if origin == KNOWN:
        return int(value)
    result = OInt(value)
    result.origin = origin
    return result


class OBytes(bytearray):
    """A bytearray whose `origins` list holds, per byte, its source offset or KNOWN. Slicing,
    index reads, bytes(), concatenation and every in-place change keep it aligned; any other
    method returns a plain bytearray (all KNOWN)."""
    __slots__ = ("origins",)
    __hash__ = None

    def __getitem__(self, item):
        value = bytearray.__getitem__(self, item)
        if isinstance(item, slice):
            return _obytes(value, self.origins[item])
        return _oint(value, self.origins[item])

    def __setitem__(self, item, value):
        value = _assignable(value)
        origins = _origins_of(value)
        bytearray.__setitem__(self, item, value)
        if isinstance(item, slice):
            self.origins[item] = origins
        else:
            self.origins[item] = origins[0]

    def __delitem__(self, item):
        bytearray.__delitem__(self, item)
        del self.origins[item]

    def append(self, value):
        bytearray.append(self, value)
        self.origins.append(getattr(value, "origin", KNOWN))

    def extend(self, values):
        values = _assignable(values)
        bytearray.extend(self, values)
        self.origins.extend(_origins_of(values))

    def insert(self, index, value):
        bytearray.insert(self, index, value)
        self.origins.insert(index, getattr(value, "origin", KNOWN))

    def pop(self, index=-1):
        value = bytearray.pop(self, index)
        return _oint(value, self.origins.pop(index))

    def remove(self, value):
        del self[bytearray.index(self, value)]

    def reverse(self):
        bytearray.reverse(self)
        self.origins.reverse()

    def clear(self):
        bytearray.clear(self)
        self.origins = []

    def __iadd__(self, other):
        self.extend(other)
        return self

    def __imul__(self, n):
        bytearray.__imul__(self, n)
        self.origins = self.origins * max(n, 0)
        return self

    def __add__(self, other):
        value = bytearray.__add__(self, other)
        if value is NotImplemented:
            return value
        return _obytes(value, self.origins + _origins_of(other))

    def __mul__(self, n):
        return _obytes(bytearray.__mul__(self, n), self.origins * max(n, 0))

    __rmul__ = __mul__

    def __bytes__(self):
        return _ofrozen(bytes(memoryview(self)), self.origins)

    def copy(self):
        return _obytes(bytearray(memoryview(self)), self.origins)

    def __copy__(self):
        return self.copy()

    def __deepcopy__(self, memo):
        return self.copy()


def _obytes(value, origins):
    result = OBytes(value)
    result.origins = list(origins)
    assert len(result.origins) == len(result), "origins out of step with the bytes"
    return result


class OFrozen(bytes):
    """The bytes() of an origin-carrying value: `origins` as OBytes; a slice shares its parent's
    list until read, so the suffix keys of PointerTable.store stay cheap."""

    @property
    def origins(self):
        return self._parent[self._slice] if self._slice is not None else self._parent

    def __getitem__(self, item):
        value = bytes.__getitem__(self, item)
        if isinstance(item, slice):
            result = OFrozen(value)
            result._parent, result._slice = self.origins, item
            return result
        return _oint(value, self.origins[item])

    def __add__(self, other):
        value = bytes.__add__(self, other)
        if value is NotImplemented:
            return value
        return _ofrozen(value, self.origins + _origins_of(other))

    def __radd__(self, other):
        if not isinstance(other, (bytes, bytearray, memoryview)):
            return NotImplemented
        return _ofrozen(bytes(other) + bytes(memoryview(self)), _origins_of(other) + self.origins)

    def __mul__(self, n):
        return _ofrozen(bytes.__mul__(self, n), self.origins * max(n, 0))

    __rmul__ = __mul__

    def __bytes__(self):
        return self


def _ofrozen(value, origins):
    result = OFrozen(value)
    result._parent, result._slice = list(origins), None
    assert len(result._parent) == len(result), "origins out of step with the bytes"
    return result


def _span(detail):
    """(low, high) inclusive offsets of a record's detail, None when it names no offset."""
    m = re.fullmatch(r"0x([0-9A-F]+)(?::0x([0-9A-F]+))?", detail)
    if not m:
        return None
    low = int(m.group(1), 16)
    if m.group(2) is None:
        return low, low
    return low, max(low, int(m.group(2), 16) - 1)


def origin_text(origins):
    """Origins as comma-separated runs, hex: `<q>+<n>` for n source offsets from q on, `v<i>+<n>`
    for n write identities from i on, `k<n>` for n KNOWN bytes; `-` when empty."""
    out, n = [], 0
    while n < len(origins):
        q, m = origins[n], n + 1
        if q == KNOWN:
            while m < len(origins) and origins[m] == KNOWN:
                m += 1
            out.append("k%x" % (m - n))
        elif q < KNOWN:
            while m < len(origins) and origins[m] == q - (m - n):
                m += 1
            out.append("v%x+%x" % (-2 - q, m - n))
        else:
            while m < len(origins) and origins[m] == q + m - n:
                m += 1
            out.append("%x+%x" % (q, m - n))
        n = m
    return ",".join(out) or "-"


def _key_text(key):
    if isinstance(key, int):
        return "%x" % key
    assert not re.fullmatch(r"[0-9a-f]+", key), "an alt key that reads as an index: %r" % key
    return key


def _value_text(value):
    if isinstance(value, int):
        return "p%x" % value
    return bytes(memoryview(value)).hex() or "-"


def _runs(positions):
    """The maximal runs of consecutive positions, as ranges, in order."""
    runs, start, last = [], None, None
    for n in positions:
        if start is not None and n == last + 1:
            last = n
            continue
        if start is not None:
            runs.append(range(start, last + 1))
        start = last = n
    if start is not None:
        runs.append(range(start, last + 1))
    return runs


def _key_detail(item, length):
    """length: a callable, called only for a slice or a negative index."""
    if isinstance(item, slice):
        start, stop, _ = item.indices(length())
        return "%s:%s" % (_offset(start), _offset(stop))
    if isinstance(item, int):
        return _offset(item + length() if item < 0 else item)
    return str(item)


def _make_bank_class(trace):
    class TracedBank(bytearray):
        __slots__ = ("_nr", "_base", "_orig")
        __hash__ = None

        def _from(self, start, stop):
            mask = trace._masks.get(self._nr)
            if mask is None or start >= stop:
                return lambda: "source"
            return lambda: trace.provenance(set(mask[start:stop]))

        def __getitem__(self, item):
            length = len(self)
            if isinstance(item, slice):
                start, stop, _ = item.indices(length)
                span = (start, max(start, stop))
                channel = "slice"
                if span[0] == span[1]:
                    return _obytes(bytearray.__getitem__(self, item), [])
            else:
                channel = "index"
                span = None
                if isinstance(item, int):
                    n = item + length if item < 0 else item
                    span = (n, n + 1)
            trace.record(channel, "bank 0x%02X" % self._nr, _key_detail(item, lambda: length),
                         self._from(*span) if span else self._from(0, length))
            value = bytearray.__getitem__(self, item)
            if isinstance(item, slice):
                return _obytes(value, self._orig[item])
            return _oint(value, self._orig[item])

        def __setitem__(self, item, value):
            old = len(self)
            value = _assignable(value)
            origins = _origins_of(value)
            if isinstance(item, slice):
                start, stop, step = item.indices(old)
                bytearray.__setitem__(self, item, value)
                self._orig[item] = origins
                if step == 1:
                    count = max(start, stop) - start + len(self) - old
                    positions = range(start, start + count)
                else:
                    positions = range(start, stop, step)
            else:
                start = item + old if item < 0 else item
                bytearray.__setitem__(self, item, value)
                self._orig[start] = origins[0]
                positions = range(start, start + 1)
            if isinstance(item, slice):
                detail = "%s:%s" % (_offset(start), _offset(start + len(positions)))
            else:
                detail = _offset(start)
            events = []
            for run in _runs(positions):
                values = bytes(bytearray.__getitem__(self, slice(run.start, run.stop)))
                checked = trace.checked([self._orig[n] for n in run], values)
                for n, q in zip(run, checked):
                    self._orig[n] = q
                events.append(("B", self._base + run.start, values, tuple(checked)))
            gid = trace.record_write("bank 0x%02X" % self._nr, detail,
                                     [(_offset(n), bytearray.__getitem__(self, n))
                                      for n in positions], events)
            if gid is None:
                return
            mask = trace._masks.get(self._nr)
            if mask is None:
                mask = trace._masks[self._nr] = bytearray(old)
            if isinstance(item, slice) and step == 1:
                mask[start:max(start, stop)] = bytes([gid]) * len(positions)
            else:
                for n in positions:
                    mask[n] = gid

        def __iter__(self):
            trace.record("iter", "bank 0x%02X" % self._nr, "__iter__", self._from(0, len(self)))
            return bytearray.__iter__(self)

        def __bytes__(self):
            trace.record("convert", "bank 0x%02X" % self._nr, "__bytes__",
                         self._from(0, len(self)))
            return _ofrozen(bytes(memoryview(self)), self._orig)

    def method(channel, name):
        base = getattr(bytearray, name)

        def traced(self, *args, **kwargs):
            trace.record(channel, "bank 0x%02X" % self._nr, name, self._from(0, len(self)))
            return base(self, *args, **kwargs)
        traced.__name__ = name
        return traced

    for channel, names in (("search", SEARCH_METHODS), ("compare", COMPARE_METHODS),
                           ("convert", ("hex", "decode"))):
        for name in names:
            setattr(TracedBank, name, method(channel, name))

    class TracedBanks(list):
        def __setitem__(self, item, value):
            if isinstance(item, slice):
                value = list(value)
                start = item.indices(len(self))[0]
                pairs = [(start + n, v) for n, v in enumerate(value)]
            else:
                pairs = [(item + len(self) if item < 0 else item, value)]
            wrapped = []
            for nr, bank in pairs:
                origins = (_origins_of(bank) if isinstance(bank, (bytes, bytearray))
                           and not isinstance(bank, TracedBank) else None)
                wrapped.append((_wrap(bank, nr, _base(nr)), origins))
            list.__setitem__(self, item, [b for b, _ in wrapped] if isinstance(item, slice)
                             else wrapped[0][0])
            for bank, origins in wrapped:
                if isinstance(bank, TracedBank) and origins is not None:
                    trace._masks.pop(bank._nr, None)
                    TracedBank.__setitem__(bank, slice(None),
                                           _ofrozen(bytes(memoryview(bank)), origins))

    def _base(nr):
        return trace._bases[nr] if nr < len(trace._bases) else nr * 0x4000

    def _wrap(bank, nr, base):
        if isinstance(bank, TracedBank) or not isinstance(bank, (bytes, bytearray)):
            return bank
        traced = TracedBank(bank)
        traced._nr = nr
        traced._base = base
        traced._orig = list(range(base, base + len(bank)))
        return traced

    return TracedBanks, _wrap


def _entry(trace, key, value):
    """(key, value, origins) of a table entry as events() gives it."""
    if isinstance(value, int):
        return key, int(value), None
    data = bytes(memoryview(value)) if isinstance(value, (bytes, bytearray)) else bytes(value)
    return key, data, tuple(trace.checked(_origins_of(value), data))


def _carrying(value, origins):
    """value carrying origins: an OBytes or OFrozen updated in place, a plain one copied."""
    if isinstance(value, OBytes):
        value.origins = list(origins)
    elif isinstance(value, OFrozen):
        value._parent, value._slice = list(origins), None
    elif isinstance(value, bytearray):
        value = _obytes(value, origins)
    else:
        value = _ofrozen(value, origins)
    return value


def _alt_keys(table):
    for klass in type(table).__mro__:
        alt = vars(table).get("_%s__alt_data" % klass.__name__.lstrip("_"))
        if isinstance(alt, dict):
            return list(alt)
    return []


def _table_class(trace, cls):
    traced = trace._table_classes.get(cls)
    if traced is not None:
        return traced

    def name(self):
        return trace._table_names[id(self)][0]

    def provenance(self, keys):
        def compute():
            written = trace._entries.get(name(self), {})
            return trace.provenance({written.get(k, 0) for k in keys()})
        return compute

    def __getitem__(self, item):
        detail = _key_detail(item, lambda: cls.__len__(self))
        trace.record("table", "table " + name(self), detail, provenance(self, lambda: [detail]))
        return cls.__getitem__(self, item)

    def __contains__(self, item):
        detail = _key_detail(item, lambda: cls.__len__(self))
        trace.record("table", "table " + name(self), "__contains__ " + detail,
                     provenance(self, lambda: [detail]))
        return cls.__contains__(self, item)

    def __len__(self):
        trace.record("table", "table " + name(self), "__len__",
                     provenance(self, lambda: [_offset(n) for n in range(cls.__len__(self))]))
        return cls.__len__(self)

    def __setitem__(self, item, value):
        value = value if isinstance(value, int) else _assignable(value)
        if trace.active and isinstance(value, (bytes, bytearray)):
            value = _carrying(value, trace.checked(_origins_of(value), bytes(memoryview(value))))
        cls.__setitem__(self, item, value)
        detail = _key_detail(item, lambda: cls.__len__(self))
        key = item + cls.__len__(self) if isinstance(item, int) and item < 0 else item
        gid = trace.record_write("table " + name(self), detail, [(detail, value)],
                                 [("T", name(self)) + _entry(trace, key, value)])
        if gid is not None:
            trace._entries.setdefault(name(self), {})[detail] = gid

    members = {"__getitem__": __getitem__, "__contains__": __contains__, "__len__": __len__,
               "__module__": cls.__module__, "__qualname__": cls.__qualname__}
    if hasattr(cls, "__setitem__"):
        members["__setitem__"] = __setitem__
    traced = type(cls.__name__, (cls,), members)
    trace._table_classes[cls] = traced
    return traced


def install(modules, sections, inventory=None, roots=None):
    """Instruments modules["ROM"], ["ROMWithTables"] and ["PointerTable"]; returns the Trace.
    The caller must call `uninstall` before installing another trace."""
    rom_cls, tables_cls, table_cls = (modules["ROM"], modules["ROMWithTables"],
                                      modules["PointerTable"])
    assert issubclass(tables_cls, rom_cls), "ROMWithTables must subclass ROM"
    if roots is None:
        roots = [os.path.dirname(os.path.abspath(rom_cls.patch.__code__.co_filename))]
    trace = Trace(modules, sections, inventory, roots)
    banks_cls, wrap = _make_bank_class(trace)

    rom_init = rom_cls.__dict__["__init__"]

    def traced_rom_init(self, *args, **kwargs):
        rom_init(self, *args, **kwargs)
        bases, base = [], 0
        for bank in self.banks:
            bases.append(base)
            base += len(bank)
        if trace._source is None:
            trace._source = b"".join(bytes(memoryview(b)) for b in self.banks)
            trace._bases = bases
        self.banks = banks_cls(wrap(b, n, bases[n]) for n, b in enumerate(self.banks))

    tables_init = tables_cls.__dict__["__init__"]

    def traced_tables_init(self, *args, **kwargs):
        tables_init(self, *args, **kwargs)
        for name, value in vars(self).items():
            if isinstance(value, table_cls) and id(value) not in trace._table_names:
                trace._table_names[id(value)] = (name, value)
                if name not in trace._tables:
                    trace._tables.append(name)
                    base = type(value)
                    keys = list(range(base.__len__(value))) + _alt_keys(value)
                    trace._initial += [(name,) + _entry(trace, k, base.__getitem__(value, k))
                                       for k in keys]
                value.__class__ = _table_class(trace, type(value))

    for owner, name, value in ((rom_cls, "__init__", traced_rom_init),
                               (tables_cls, "__init__", traced_tables_init)):
        trace._restore.append((owner, name, owner.__dict__[name]))
        setattr(owner, name, value)
    return trace


def blind_spots(root):
    """[(file, stripped line)] of every buffer-protocol candidate line under root, `www/` and
    hidden folders skipped, files sorted, lines in file order."""
    pattern = re.compile(r"memoryview|unpack_from|pack_into|hashlib|\.write\(|\.tobytes\(")
    found = []
    for folder, dirs, files in os.walk(root):
        rel_folder = os.path.relpath(folder, root)
        dirs[:] = sorted(d for d in dirs if not d.startswith(".")
                         and not (rel_folder == "." and d == "www"))
        for name in sorted(files):
            if not name.endswith(".py"):
                continue
            path = os.path.join(folder, name)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            with open(path, "rb") as f:
                text = f.read().decode("utf-8", "replace").replace("\r", "")
            for line in text.split("\n"):
                if pattern.search(line):
                    found.append((rel, line.strip()))
    found.sort(key=lambda hit: hit[0])
    return found


def main(argv):
    parser = argparse.ArgumentParser(prog="read_trace.py")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--self-test", action="store_true")
    mode.add_argument("--blind-spots", action="store_true")
    parser.add_argument("--ladxr", default=DEFAULT_LADXR)
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if not os.path.isfile(os.path.join(args.ladxr, "rom.py")):
        print("setup: read-trace: no LADXR checkout in %s, run tests/ladx/oracle/setup.sh"
              % args.ladxr)
        return 2
    if args.blind_spots:
        hits = blind_spots(args.ladxr)
        for rel, line in hits:
            print("BLIND %s\t%s" % (rel, line))
        print("read-trace: %d buffer-protocol sites to review" % len(hits))
        return 0
    return self_test(args.ladxr)


PLANTED_MODULE = """\
class PlantedTable(PointerTable):
    def __init__(self, rom):
        super().__init__(rom, {"count": 2, "pointers_addr": 0x100, "pointers_bank": 0x01,
                               "data_bank": 0x02})


class ROMWithTables(ROM):
    def __init__(self, filestream):
        super().__init__(filestream)
        self.parsed = self.banks[0x3F][0x0147]
        self.texts = PlantedTable(self)


def read_index(rom):
    return rom.banks[0x10][0x0123]

def read_slice(rom):
    return rom.banks[0x11][0x0020:0x0028]

def read_iter(rom):
    return sum(rom.banks[0x12])

def read_search(rom):
    return rom.banks[0x13].find(b"\\x00\\x01\\x02")

def read_compare(rom):
    return rom.banks[0x14] == b"planted"

def read_convert(rom):
    return rom.banks[0x15].hex()

def read_table(rom):
    return rom.texts[1]

def write_bank(rom):
    rom.banks[0x08][0x0040] = 0x12
    rom.banks[0x08][0x0041:0x0043] = b"ab"

def write_entry(rom):
    rom.texts[0] = bytearray(b"\\x09\\xFF")

def read_written(rom):
    return rom.banks[0x08][0x0040:0x0043]

def read_mixed(rom):
    return rom.banks[0x07][0x002F:0x0031]

def read_entry(rom):
    return rom.texts[0]

def run_copies(stream):
    rom = ROMWithTables(stream)
    rom.banks[0x08][0x0100:0x0104] = rom.banks[0x09][0x0200:0x0204]
    rom.banks[0x08][0x0104] = rom.banks[0x09][0x0300]
    rom.banks[0x08][0x0105:0x0108] = rom.banks[0x09][0x0010:0x0011] + bytes(rom.banks[0x09][0x0020:0x0022])
    rom.banks[0x08][0x0108] = rom.banks[0x09][0x0300] ^ 0xFF
    rom.texts[1] = rom.texts[0][1:]
    rom.texts.store(rom)
    return rom

def run_channels(stream):
    rom = ROMWithTables(stream)
    read_index(rom)
    read_slice(rom)
    read_iter(rom)
    read_search(rom)
    read_compare(rom)
    read_convert(rom)
    read_table(rom)
    rom.patch(0x07, 0x0030, "%(old)s", "A1B2")
    write_bank(rom)
    write_entry(rom)
    read_written(rom)
    read_mixed(rom)
    read_entry(rom)
    for c in rom.banks[0x16][0x0000:0x0002]:
        pass
    return rom
"""

SELF_TEST_EXPECTED = [
    ("index", "read_index", "bank 0x10", "0x0123"),
    ("slice", "read_slice", "bank 0x11", "0x0020:0x0028"),
    ("iter", "read_iter", "bank 0x12", "__iter__"),
    ("search", "read_search", "bank 0x13", "find"),
    ("compare", "read_compare", "bank 0x14", "__eq__"),
    ("convert", "read_convert", "bank 0x15", "hex"),
    ("table", "read_table", "table texts", "0x0001"),
]
PATCH_LINE = 'rom.patch(0x07, 0x0030, "%(old)s", "A1B2")'
SELF_TEST_PROVENANCE = [
    ("g-written", "read_written(rom)", "planted.py:read_written", "slice", "bank 0x08",
     "0x0040:0x0043", "written:g-write"),
    ("g-mixed", "read_mixed(rom)", "planted.py:read_mixed", "slice", "bank 0x07",
     "0x002F:0x0031", "mixed:g-patch"),
    ("g-entry-read", "read_entry(rom)", "planted.py:read_entry", "table", "table texts",
     "0x0000", "written:g-entry"),
    ("-", "for c in rom.banks[0x16][0x0000:0x0002]:", "planted.py:run_channels", "slice",
     "bank 0x16", "0x0000:0x0002", "source"),
]
SELF_TEST_WRITES = [
    ("g-patch", PATCH_LINE, "rom.py:ROM.patch", "bank 0x07", "0x0030:0x0032"),
    ("g-write", "write_bank(rom)", "planted.py:write_bank", "bank 0x08", "0x0040"),
    ("g-write", "write_bank(rom)", "planted.py:write_bank", "bank 0x08", "0x0041:0x0043"),
    ("g-entry", "write_entry(rom)", "planted.py:write_entry", "table texts", "0x0000"),
]


def _load(path, name, extra=None):
    import importlib.util
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    module.__dict__.update(extra or {})
    spec.loader.exec_module(module)
    return module


def _synthetic_rom():
    rng = random.Random(0x1ADE)
    data = bytearray(rng.getrandbits(8 * 0x40 * 0x4000).to_bytes(0x40 * 0x4000, "little"))
    bank = lambda n, addr: n * 0x4000 + addr
    data[bank(1, 0x100):bank(1, 0x104)] = bytes([0x00, 0x42, 0x10, 0x42])
    data[bank(2, 0x200):bank(2, 0x204)] = bytes([0x01, 0x02, 0x03, 0xFF])
    data[bank(2, 0x210):bank(2, 0x213)] = bytes([0x04, 0x05, 0xFF])
    return bytes(data)


def self_test(ladxr):
    """Planted reads and writes through LADXR's real ROM and PointerTable; prints the summary or
    one `self-test failed` line per mismatch; returns the exit code."""
    sys.dont_write_bytecode = True
    rom_module = _load(os.path.join(ladxr, "rom.py"), "rom")
    table_module = _load(os.path.join(ladxr, "pointerTable.py"), "pointerTable")
    data = _synthetic_rom()
    old = data[7 * 0x4000 + 0x30:7 * 0x4000 + 0x32].hex().upper()
    section = "planted.py run_channels"
    parse = "planted.py ROMWithTables.__init__"
    parse_byte = parse + "\tself.parsed = self.banks[0x3F][0x0147]"
    parse_table = parse + "\tself.texts = PlantedTable(self)"
    patch_site = section + "\t" + PATCH_LINE % {"old": old}
    inventory = {"%s\t%s(rom)" % (section, fn): "g-" + ch for ch, fn, _, _ in SELF_TEST_EXPECTED}
    inventory.update({parse_byte: "parse", parse_table: "parse"})
    for group, line, _, _, _, _, _ in SELF_TEST_PROVENANCE:
        if group != "-":
            inventory["%s\t%s" % (section, line)] = group
    for group, line, _, _, _ in SELF_TEST_WRITES:
        inventory[patch_site if line == PATCH_LINE else "%s\t%s" % (section, line)] = group
    failures = []
    with tempfile.TemporaryDirectory(prefix="read-trace-") as planted_dir:
        path = os.path.join(planted_dir, "planted.py")
        with open(path, "w") as f:
            f.write(PLANTED_MODULE % {"old": old})
        planted = _load(path, "planted", {"ROM": rom_module.ROM,
                                          "PointerTable": table_module.PointerTable})
        modules = {"ROM": rom_module.ROM, "ROMWithTables": planted.ROMWithTables,
                   "PointerTable": table_module.PointerTable}
        trace = install(modules, [("planted.py", "run_channels"),
                                  ("planted.py", "ROMWithTables.__init__")],
                        inventory, roots=[planted_dir, ladxr])
        try:
            rom = planted.run_channels(io.BytesIO(data))
        finally:
            trace.uninstall()
        records, writes = trace.records(), trace.writes()
        if rom.banks[0x07][0x30:0x32] != bytes.fromhex("A1B2"):
            failures.append("rom.patch did not apply")
    channel_records = [r for r in records if r[1].startswith(section + "\t")]
    for channel, function, target, detail in SELF_TEST_EXPECTED:
        want = ("g-" + channel, "%s\t%s(rom)" % (section, function), "planted.py:" + function,
                channel, target, detail, "source")
        if channel_records.count(want) != 1:
            failures.append("channel %s: expected one %r" % (channel, want))
    for group, line, caller, channel, target, detail, provenance in SELF_TEST_PROVENANCE:
        want = (group, "%s\t%s" % (section, line), caller, channel, target, detail, provenance)
        if channel_records.count(want) != 1:
            failures.append("provenance %s: expected one %r" % (provenance, want))
    expected = len(SELF_TEST_EXPECTED) + len(SELF_TEST_PROVENANCE)
    if len(channel_records) != expected:
        failures.append("planted section: %d records, expected %d (patch or write recorded?)"
                        % (len(channel_records), expected))
    want_writes = [(g, patch_site if line == PATCH_LINE else "%s\t%s" % (section, line), c, t, d)
                   for g, line, c, t, d in SELF_TEST_WRITES]
    if writes != want_writes:
        failures.append("writes %r, expected %r" % (writes, want_writes))
    byte_read = ("parse", parse_byte, "planted.py:ROMWithTables.__init__", "index", "bank 0x3F",
                 "0x0147", "source")
    if records.count(byte_read) != 1:
        failures.append("parse: the planted parse byte is not recorded once with its site")
    if not any(r[1] == parse_table and r[2].startswith("pointerTable.py:PointerTable.")
               for r in records):
        failures.append("parse: no PointerTable read tagged with the table's parse site")
    others = [r for r in records if r[1] not in (parse_byte, parse_table)
              and not r[1].startswith(section + "\t")]
    if others:
        failures.append("%d records outside the planted sites, first %r" % (len(others),
                                                                            others[0]))
    if any(r[2] == "rom.py:ROM.patch" for r in records):
        failures.append("a read inside rom.patch is recorded")
    failures += _self_test_origins(planted_dir_module(ladxr, rom_module, table_module, old),
                                   data)
    for failure in failures:
        print("read-trace: self-test failed: " + failure)
    if failures:
        return 1
    print("read-trace: self-test, %d channels, %d provenances and %d writes traced, patch reads "
          "excluded, %d origin events" % (len(SELF_TEST_EXPECTED), len(SELF_TEST_PROVENANCE),
                                          len(SELF_TEST_WRITES), len(_origin_expected(data))))
    return 0


def planted_dir_module(ladxr, rom_module, table_module, old):
    """A fresh load of the planted module over LADXR's real classes, in its own folder."""
    folder = tempfile.mkdtemp(prefix="read-trace-origins-")
    path = os.path.join(folder, "planted.py")
    with open(path, "w") as f:
        f.write(PLANTED_MODULE % {"old": old})
    return _load(path, "planted_origins", {"ROM": rom_module.ROM,
                                           "PointerTable": table_module.PointerTable}), folder


def _origin_expected(data):
    """The events run_copies must give, without group and site."""
    at = lambda bank, addr: bank * 0x4000 + addr
    src = lambda bank, addr, n: tuple(range(at(bank, addr), at(bank, addr) + n))
    piece = lambda bank, addr, n: data[at(bank, addr):at(bank, addr) + n]
    return [
        ("B", at(8, 0x100), piece(9, 0x200, 4), src(9, 0x200, 4)),
        ("B", at(8, 0x104), piece(9, 0x300, 1), src(9, 0x300, 1)),
        ("B", at(8, 0x105), piece(9, 0x10, 1) + piece(9, 0x20, 2),
         src(9, 0x10, 1) + src(9, 0x20, 2)),
        ("B", at(8, 0x108), bytes([data[at(9, 0x300)] ^ 0xFF]), (virtual(0),)),
        ("T", "texts", 1, bytes([2, 3, 0xFF]), src(2, 0x201, 3)),
        ("B", at(2, 0x200), bytes([1, 2, 3, 0xFF]), src(2, 0x200, 4)),
        ("B", at(1, 0x100), b"\x00", (virtual(1),)), ("B", at(1, 0x101), b"\x42", (virtual(2),)),
        ("B", at(1, 0x102), b"\x01", (virtual(3),)), ("B", at(1, 0x103), b"\x42", (virtual(4),)),
    ]


def _self_test_origins(planted_and_folder, data):
    """Planted copies (slice, index, concatenation, table entry, store's move and suffix
    de-duplication) and a computed write through LADXR's real classes: failure lines."""
    planted, folder = planted_and_folder
    try:
        modules = {"ROM": planted.ROM, "ROMWithTables": planted.ROMWithTables,
                   "PointerTable": planted.PointerTable}
        trace = install(modules, [], roots=[folder])
        try:
            planted.run_copies(io.BytesIO(data))
        finally:
            trace.uninstall()
    finally:
        import shutil
        shutil.rmtree(folder, ignore_errors=True)
    got = [e[:1] + e[3:] for e in trace.events()]
    want = _origin_expected(data)
    if got != want:
        return ["origins: events %r, expected %r" % (got, want)]
    if trace.initial() != [("texts", 0, bytes([1, 2, 3, 0xFF]), tuple(range(0x8200, 0x8204))),
                           ("texts", 1, bytes([4, 5, 0xFF]), tuple(range(0x8210, 0x8213)))]:
        return ["origins: parsed entries %r" % (trace.initial(),)]
    return []


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
