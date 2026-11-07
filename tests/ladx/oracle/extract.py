"""Usage: python3 -I tests/ladx/oracle/extract.py check [--root <dir>] [--inventory <file>]

The extraction fixtures, tests/ladx/extract/: the source ROM's structure the inventory's
Extractions table names, so that C rebuilds the known-source map KS (known_source.py) and makes
the C groups' decisions without the ROM. This module is their format (`parse`, `render`, `read`,
`write`), the KS rebuild from the fixtures alone (`ks_spans`, `ks_rebuild`) and `check`, the
ROM-free check against the inventory. The layout of each file is in tests/ladx/README.md
("Extraction fixtures").

Format: only the characters `0-9`, `=`, `r` and the newline. Every line starts with `=`; tokens
are separated by `=`; the numbers inside a token by `r`; each number is 0..255 in decimal
without a leading zero; a larger quantity is split into bytes, high first. Line 1 is `=1`.

`check` prints one MISSING <name>, EXTRA <file> or BADFILE <name>: <reason> line per finding
(exit 1), else `extract: <n> fixtures, <b> bytes, every inventory extraction present`; a
malformed inventory exits 2. It proves itself first on planted trees.
"""

import collections
import importlib.util
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def _sibling(name):
    """The module name.py next to this file."""
    spec = importlib.util.spec_from_file_location("extract_" + name,
                                                  os.path.join(HERE, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = no_bytecode
    return module


known_source = _sibling("known_source")
LAYOUT, BOSSES, BANK = known_source.LAYOUT, known_source.BOSSES, known_source.BANK

NAMES = ["room-entities", "room-objects", "boss-sources", "backgrounds", "save-layout",
         "source-flags"]
ROOM_FILES = {"room-entities": "data", "room-objects": "rooms", "backgrounds": "background"}
OBJECT_KINDS = "shvw"
FINGERPRINT = 8

# An entry of room-entities, room-objects or backgrounds; None for an integer pointer. parts
# are known_source.fields' of the entry, None when they do not rebuild it (known 0).
Entry = collections.namedtuple("Entry", "known bank start length parts")
# An entry of save-layout: a span (start, length) or an integer pointer (raw); dedup (index,
# offset) of the earlier entry it equals; extra the sprite data's values or the fingerprints.
SaveEntry = collections.namedtuple("SaveEntry", "flags bank start length raw dedup extra")
Section = collections.namedtuple("Section", "table regions entries")
BossSources = collections.namedtuple(
    "BossSources", "zero_bank zero_addr zero_count zeros item_bank item_addr items")

VERSION = [[1]]
_LINE = re.compile(r"=(?:0|[1-9][0-9]{0,2})(?:[=r](?:0|[1-9][0-9]{0,2}))*")


class ExtractError(Exception):
    pass


def parse(text):
    """[[[number]]] (lines of tokens of numbers) of a canonical fixture text; ExtractError
    otherwise."""
    if not text.endswith("\n"):
        raise ExtractError("no newline at the end")
    lines = []
    for n, line in enumerate(text[:-1].split("\n"), 1):
        if not _LINE.fullmatch(line):
            raise ExtractError("line %d: not canonical" % n)
        tokens = [[int(x) for x in token.split("r")] for token in line[1:].split("=")]
        if any(x > 255 for token in tokens for x in token):
            raise ExtractError("line %d: a number above 255" % n)
        lines.append(tokens)
    if lines[0] != VERSION:
        raise ExtractError("line 1: not the version line =1")
    return lines


def render(lines):
    """The text of [[[number]]]; ExtractError on an empty line or token or a number outside
    0..255."""
    out = []
    for line in lines:
        if not line or not all(line):
            raise ExtractError("an empty line or token")
        if not all(isinstance(x, int) and 0 <= x <= 255 for token in line for x in token):
            raise ExtractError("a number outside 0..255")
        out.append("=" + "=".join("r".join(str(x) for x in token) for token in line) + "\n")
    return "".join(out)


def _word(n):
    return [n >> 8, n & 0xFF]


def _unword(hi, lo):
    return hi << 8 | lo


def _keys(table):
    """A table's keys in store order: the alt keys, then the indices."""
    return list(table.info.get("alt_pointers", {})) + list(range(table.info["count"]))


def _known(table, key):
    return table.known is not None and (table.known == "all" or key in table.known)


def _key_token(table, key):
    if isinstance(key, str):
        return [1, list(table.info["alt_pointers"]).index(key)]
    return [0] + _word(key)


def _read_key(table, token):
    alts = list(table.info.get("alt_pointers", {}))
    if not token:
        raise ExtractError("no key")
    if token[0] == 1 and len(token) == 2 and token[1] < len(alts):
        return alts[token[1]]
    if token[0] == 0 and len(token) == 3 and _unword(*token[1:]) < table.info["count"]:
        return _unword(*token[1:])
    raise ExtractError("no key %s" % "r".join(map(str, token)))


def _label(t, key):
    return "table %d entry %s" % (t, key)


def _room_keys(name, layout):
    kind = ROOM_FILES[name]
    return [(t, key) for t, table in enumerate(layout) if table.kind == kind
            for key in _keys(table) if _known(table, key)]


def _parts_tokens(kind, parts):
    if kind == "data":
        return [list(entity) for entity in parts]
    if kind == "rooms":
        return [list(parts[:2])] + [[OBJECT_KINDS.index(obj[0])] + list(obj[1:])
                                    for obj in parts[2:]]
    return [_word(address) + [vertical, repeat, amount] + values
            for address, vertical, repeat, amount, values in parts]


def _tokens_parts(kind, tokens):
    if kind == "data":
        if any(len(token) != 3 for token in tokens):
            raise ExtractError("an entity is not x, y, type")
        return [tuple(token) for token in tokens]
    if kind == "rooms":
        if not tokens or len(tokens[0]) != 2:
            raise ExtractError("no animation and floor")
        parts = list(tokens[0])
        for token in tokens[1:]:
            if token[0] >= len(OBJECT_KINDS) or len(token) != (4, 5, 5, 6)[token[0]]:
                raise ExtractError("a malformed object")
            parts.append((OBJECT_KINDS[token[0]],) + tuple(token[1:]))
        return parts
    if any(len(token) < 6 for token in tokens):
        raise ExtractError("a malformed tile run")
    return [(_unword(*token[:2]), token[2], token[3], token[4], token[5:]) for token in tokens]


def _rebuilds(kind, parts, length):
    try:
        data = known_source.rebuild(kind, parts)
    except (ValueError, TypeError):
        return False
    return len(data) == length and known_source.fields(kind, data) == parts


def _span(token, table):
    """(bank, start, length) of a span's last five numbers, checked against the table."""
    bank, start, length = token[0], _unword(*token[1:3]), _unword(*token[3:5])
    if "data_bank" in table.info and bank != table.info["data_bank"]:
        raise ExtractError("bank %d, not the table's" % bank)
    if not length or start + length > BANK:
        raise ExtractError("a span outside its bank")
    return bank, start, length


def _read_rooms(name, lines, layout):
    kind = ROOM_FILES[name]
    expected = _room_keys(name, layout)
    out = {}
    for n, line in enumerate(lines[1:], 2):
        try:
            if len(out) == len(expected):
                raise ExtractError("an entry past the last")
            t = line[0][0]
            if t >= len(layout):
                raise ExtractError("no table %d" % t)
            key = _read_key(layout[t], line[0][1:])
            if (t, key) != expected[len(out)]:
                raise ExtractError("%s, expected %s" % (_label(t, key),
                                                        _label(*expected[len(out)])))
            if len(line) == 1:
                out[(t, key)] = None
                continue
            if len(line[1]) != 6 or line[1][0] > 1:
                raise ExtractError("a malformed span")
            known, (bank, start, length) = line[1][0], _span(line[1][1:], layout[t])
            parts = None
            if known:
                parts = _tokens_parts(kind, line[2:])
                if not _rebuilds(kind, parts, length):
                    raise ExtractError("fields that do not rebuild the span")
            elif len(line) > 2:
                raise ExtractError("fields on an entry not rebuilt")
            out[(t, key)] = Entry(known, bank, start, length, parts)
        except ExtractError as e:
            raise ExtractError("line %d: %s" % (n, e))
    if len(out) < len(expected):
        raise ExtractError("no line for %s" % _label(*expected[len(out)]))
    return out


def _write_rooms(name, entries, layout):
    kind = ROOM_FILES[name]
    lines = [VERSION]
    for (t, key), entry in entries.items():
        line = [[t] + _key_token(layout[t], key)]
        if entry is not None:
            line.append([entry.known, entry.bank] + _word(entry.start) + _word(entry.length))
            if entry.known:
                line += _parts_tokens(kind, entry.parts)
        lines.append(line)
    return lines


def _read_bosses(lines, bosses):
    if len(lines) != 3:
        raise ExtractError("%d lines, expected 3" % len(lines))
    zero, item = lines[1], lines[2]
    if zero[0] != [bosses.zero_bank] + _word(bosses.zero_addr) + [bosses.zero_count]:
        raise ExtractError("line 2: not the zero range")
    if item[0] != [bosses.item_bank] + _word(bosses.item_addr):
        raise ExtractError("line 3: not the item base")
    if any(len(token) != 2 for token in zero[1:]):
        raise ExtractError("line 2: an offset is not two numbers")
    zeros = [_unword(*token) for token in zero[1:]]
    if zeros != sorted(set(zeros)) or any(
            not bosses.zero_addr <= q < bosses.zero_addr + bosses.zero_count for q in zeros):
        raise ExtractError("line 2: offsets not increasing inside the range")
    if any(len(token) != 3 for token in item[1:]):
        raise ExtractError("line 3: a room is not room, item")
    items = [(_unword(*token[:2]), token[2]) for token in item[1:]]
    if [room for room, _ in items] != list(bosses.rooms):
        raise ExtractError("line 3: not the boss rooms")
    return BossSources(bosses.zero_bank, bosses.zero_addr, bosses.zero_count, zeros,
                       bosses.item_bank, bosses.item_addr, items)


def _write_bosses(sources):
    return [VERSION,
            [[sources.zero_bank] + _word(sources.zero_addr) + [sources.zero_count]]
            + [_word(q) for q in sources.zeros],
            [[sources.item_bank] + _word(sources.item_addr)]
            + [_word(room) + [item] for room, item in sources.items]]


def _read_flags(lines):
    if len(lines) != 2 or len(lines[1]) != 3 or any(t not in ([0], [1]) for t in lines[1]):
        raise ExtractError("not one line of three flags 0 or 1")
    return tuple(token[0] for token in lines[1])


def _read_section(t, table, lines, n):
    """(Section, the next line number) of table t's section from line n (1-based)."""
    if n >= len(lines) or lines[n - 1] != [[t]]:
        raise ExtractError("line %d: not the marker of table %d" % (n, t))
    regions = []
    for token in lines[n]:
        if len(token) != 5:
            raise ExtractError("line %d: a malformed region" % (n + 1))
        start, end = _unword(*token[1:3]), _unword(*token[3:5])
        if not start < end <= BANK:
            raise ExtractError("line %d: a region outside its bank" % (n + 1))
        regions.append((token[0], start, end))
    if not regions:
        raise ExtractError("line %d: no region" % (n + 1))
    entries = {}
    n += 2
    for key in _keys(table):
        try:
            if n > len(lines):
                raise ExtractError("missing")
            entries[key] = _read_save_entry(table, key, lines[n - 1], entries)
        except ExtractError as e:
            raise ExtractError("line %d: %s: %s" % (n, _label(t, key), e))
        n += 1
    return Section(t, regions, entries), n


def _read_save_entry(table, key, line, entries):
    if len(line) < 2 or _read_key(table, line[0]) != key:
        raise ExtractError("not this entry")
    pointer = line[1]
    banked = "banks_bank" in table.info and not isinstance(key, str)
    if pointer[0] > 1 or len(pointer) != (5, 7)[pointer[0]] or pointer[1] > (3 if banked else 1):
        raise ExtractError("a malformed pointer")
    flags = pointer[1]
    if pointer[0] == 0:
        raw = _unword(*pointer[3:])
        if 0x4000 <= raw < 0x8000 or isinstance(key, str) or len(line) > 2:
            raise ExtractError("a malformed integer pointer")
        if "data_bank" in table.info and pointer[2] != table.info["data_bank"]:
            raise ExtractError("bank %d, not the table's" % pointer[2])
        return SaveEntry(flags, pointer[2], None, None, raw, None, None)
    bank, start, length = _span(pointer[2:], table)
    size = table.info.get("data_size")
    if size is not None and (length != size or start < table.info["data_addr"] or
                             (start - table.info["data_addr"]) % size or
                             (start - table.info["data_addr"]) // size > 255):
        raise ExtractError("not a sprite data slot")
    if isinstance(key, str):
        if len(line) > 2:
            raise ExtractError("an alt entry with more than its span")
        return SaveEntry(flags, bank, start, length, None, None, None)
    if len(line) < 3:
        raise ExtractError("no de-duplication")
    dedup = None
    if line[2] != [0]:
        if line[2][0] != 1 or len(line[2]) != 5:
            raise ExtractError("a malformed de-duplication")
        dedup = (_unword(*line[2][1:3]), _unword(*line[2][3:5]))
        earlier = entries.get(dedup[0]) if dedup[0] < key else None
        if earlier is None or earlier.raw is not None or earlier.bank != bank or \
                dedup[1] >= earlier.length or (size is not None and dedup[1]):
            raise ExtractError("a de-duplication with no earlier span")
    rest = line[3:]
    extra = None
    if table.kind == "sprite":
        if len(rest) != 1 or len(rest[0]) != size:
            raise ExtractError("not the sprite data's values")
        extra = rest[0]
    elif table.kind in ("text", "background"):
        if len(rest) != length or any(len(token) != FINGERPRINT for token in rest):
            raise ExtractError("not one fingerprint per suffix")
        extra = [tuple(token) for token in rest]
    elif rest:
        raise ExtractError("more than its span")
    return SaveEntry(flags, bank, start, length, None, dedup, extra)


def _read_save(lines, layout):
    sections, n = [], 2
    for t, table in enumerate(layout):
        section, n = _read_section(t, table, lines, n)
        sections.append(section)
    if n <= len(lines):
        raise ExtractError("line %d: past the last table" % n)
    return sections


def _write_save(sections, layout):
    lines = [VERSION]
    for section in sections:
        table = layout[section.table]
        lines.append([[section.table]])
        lines.append([[bank] + _word(start) + _word(end) for bank, start, end in section.regions])
        for key, e in section.entries.items():
            line = [_key_token(table, key)]
            if e.raw is not None:
                line.append([0, e.flags, e.bank] + _word(e.raw))
            else:
                line.append([1, e.flags, e.bank] + _word(e.start) + _word(e.length))
                if not isinstance(key, str):
                    line.append([0] if e.dedup is None else [1] + _word(e.dedup[0])
                                + _word(e.dedup[1]))
                    if table.kind == "sprite":
                        line.append(list(e.extra))
                    elif table.kind in ("text", "background"):
                        line += [list(fp) for fp in e.extra]
            lines.append(line)
    return lines


def read(name, text, layout=LAYOUT, bosses=BOSSES):
    """The structure of fixture `name`'s text: {(table, key): Entry or None} for room-entities,
    room-objects and backgrounds, BossSources, a tuple of three flags, or [Section] for
    save-layout. ExtractError(reason) when it is not canonical or breaks a structural rule."""
    lines = parse(text)
    if name in ROOM_FILES:
        return _read_rooms(name, lines, layout)
    if name == "boss-sources":
        return _read_bosses(lines, bosses)
    if name == "source-flags":
        return _read_flags(lines)
    if name == "save-layout":
        return _read_save(lines, layout)
    raise ExtractError("no format for %s" % name)


def write(name, data, layout=LAYOUT):
    """The canonical text of a fixture's structure, as `read` gives it."""
    if name in ROOM_FILES:
        return render(_write_rooms(name, data, layout))
    if name == "boss-sources":
        return render(_write_bosses(data))
    if name == "source-flags":
        return render([VERSION, [[flag] for flag in data]])
    if name == "save-layout":
        return render(_write_save(data, layout))
    raise ExtractError("no format for %s" % name)


def _cross_problem(entries, sections):
    """The reason a room file's entries disagree with save-layout's spans, or None."""
    for (t, key), entry in entries.items():
        saved = sections[t].entries[key]
        span = None if entry is None else (entry.bank, entry.start, entry.length)
        if span != (None if saved.raw is not None else (saved.bank, saved.start,
                                                        saved.length)):
            return "%s: not save-layout's span" % _label(t, key)
    return None


def read_tree(root, names, layout=LAYOUT, bosses=BOSSES):
    """({name: structure}, {name: reason}) of the fixtures `names` under root; a file that does
    not read, or disagrees with save-layout, has a reason and no structure."""
    out, bad = {}, {}
    for name in names:
        try:
            with open(os.path.join(root, name + ".txt"), encoding="ascii", newline="") as f:
                text = f.read()
            out[name] = read(name, text, layout, bosses)
        except UnicodeDecodeError:
            bad[name] = "not ASCII"
        except (OSError, ExtractError) as e:
            bad[name] = str(e) if isinstance(e, ExtractError) else "unreadable"
    if "save-layout" in out:
        for name in ROOM_FILES:
            if name in out:
                problem = _cross_problem(out[name], out["save-layout"])
                if problem:
                    bad[name] = problem
                    del out[name]
    return out, bad


def load(root, layout=LAYOUT, bosses=BOSSES):
    """{name: structure} of the six fixtures under root; ExtractError(<name>: <reason>) on the
    first that does not read."""
    out, bad = read_tree(root, NAMES, layout, bosses)
    for name in NAMES:
        if name in bad:
            raise ExtractError("%s: %s" % (name, bad[name]))
    return out


def _pointer_at(table, key):
    """(source offset, length) of an entry's pointer bytes."""
    info = table.info
    if isinstance(key, str):
        bank, addr = info["alt_pointers"][key]
        return bank * BANK + addr, 2
    where = info["pointers_bank"] * BANK + info["pointers_addr"]
    return (where + key, 1) if "data_addr" in info else (where + 2 * key, 2)


def _key_text(key):
    return "%x" % key if isinstance(key, int) else key


def ks_spans(fixtures, layout=LAYOUT):
    """[(source offset, bytes, label)] of the known-source map rebuilt from the fixtures alone
    ({name: structure}, as `load` gives), in known_source.build's order and with its labels."""
    spans = []
    content = {"rooms": fixtures["room-objects"], "data": fixtures["room-entities"],
               "background": fixtures["backgrounds"]}
    for t, table in enumerate(layout):
        entries = fixtures["save-layout"][t].entries
        info, size = table.info, table.info.get("data_size")
        for key, entry in entries.items():
            where, length = _pointer_at(table, key)
            if entry.raw is not None:
                rebuilt = entry.raw.to_bytes(2, "little")
            elif size is not None:
                rebuilt = bytes([(entry.start - info["data_addr"]) // size])
            else:
                rebuilt = (entry.start | 0x4000).to_bytes(2, "little")
            if entry.flags & 1:
                spans.append((where, rebuilt, "%s pointer %s" % (table.name, _key_text(key))))
            if "banks_bank" in info and isinstance(key, int) and entry.flags & 2:
                spans.append((info["banks_bank"] * BANK + info["banks_addr"] + key,
                              bytes([entry.bank]), "%s bank %s" % (table.name, _key_text(key))))
        for key in sorted(entries, key=lambda k: (isinstance(k, str), k)):
            entry = entries[key]
            if entry.raw is not None or not _known(table, key):
                continue
            label = "%s %s" % (table.name, _key_text(key))
            if table.kind == "sprite":
                spans.append((entry.bank * BANK + entry.start, bytes(entry.extra), label))
                continue
            room = content[table.kind][(t, key)]
            if room.known:
                spans.append((room.bank * BANK + room.start,
                              known_source.rebuild(table.kind, room.parts), label))
    sources = fixtures["boss-sources"]
    for q in sources.zeros:
        at = sources.zero_bank * BANK + q
        spans.append((at, b"\x00", "boss zero %x" % at))
    for room, item in sources.items:
        spans.append((sources.item_bank * BANK + sources.item_addr + room, bytes([item]),
                      "boss item %x" % room))
    return spans


def ks_rebuild(fixtures, layout=LAYOUT):
    """{source offset: byte} of the known-source map rebuilt from the fixtures alone."""
    out = {}
    for start, data, _ in ks_spans(fixtures, layout):
        for i, byte in enumerate(data):
            out[start + i] = byte
    return out


def inventory_names(inventory_text):
    """The Extractions table's names of an inventory; ExtractError when it does not parse."""
    check_module = _sibling("inventory_check")
    named, bad = check_module.parse_named(inventory_text)
    bad = [line for line in bad if line.startswith("BAD Extractions")]
    names = [row["Name"] for row in named["Extractions"]]
    if bad or not names or len(set(names)) != len(names):
        raise ExtractError("the inventory's Extractions table does not parse: %s"
                           % (bad[0] if bad else "no names or a name twice"))
    return names


def check(root, inventory_text, layout=LAYOUT, bosses=BOSSES):
    """(finding lines, summary line) of the fixtures under root against the inventory's
    Extractions names."""
    names = inventory_names(inventory_text)
    files = sorted(os.listdir(root)) if os.path.isdir(root) else []
    present = [name for name in names if name + ".txt" in files]
    _, bad = read_tree(root, present, layout, bosses)
    lines = []
    for name in names:
        if name not in present:
            lines.append("MISSING " + name)
        elif name in bad:
            lines.append("BADFILE %s: %s" % (name, bad[name]))
    lines += ["EXTRA " + f for f in files if f not in [n + ".txt" for n in names]]
    size = sum(os.path.getsize(os.path.join(root, name + ".txt")) for name in present)
    return lines, "extract: %d fixtures, %d bytes, every inventory extraction present" % (
        len(names), size)


_PLANT_LAYOUT = [
    known_source.Table("rooms", {"count": 2, "pointers_bank": 1, "pointers_addr": 0,
                                 "data_bank": 1, "alt_pointers": {"Alt06": (0, 0x10)}},
                       "rooms", "all"),
    known_source.Table("entities", {"count": 2, "pointers_bank": 2, "pointers_addr": 0,
                                    "data_bank": 2}, "data", range(2)),
    known_source.Table("texts", {"count": 1, "pointers_bank": 3, "pointers_addr": 0,
                                 "banks_bank": 3, "banks_addr": 0x200}, "text", None),
    known_source.Table("sprites", {"count": 2, "pointers_bank": 5, "pointers_addr": 0,
                                   "data_bank": 5, "data_addr": 0x100, "data_size": 4},
                       "sprite", "all"),
    known_source.Table("backgrounds", {"count": 2, "pointers_bank": 5, "pointers_addr": 0x200,
                                       "data_bank": 5}, "background", [1]),
]
_PLANT_BOSSES = known_source.Bosses(zero_bank=5, zero_addr=0x800, zero_count=4, item_bank=5,
                                    item_addr=0x900, rooms=[1, 3])
_FP = "=" + "r".join(str(n) for n in range(10, 18))
_PLANT_TREE = {
    "room-entities": "=1\n=1r0r0r0=1r2r1r0r0r3=1r2r5\n=1r0r0r1\n",
    "room-objects": "=1\n=0r1r0=1r1r3r0r0r6=9r10=2r2r5r4r7\n=0r0r0r0=1r1r1r0r0r5=1r2=0r4r3r5\n"
                    "=0r0r0r1=0r1r1r5r0r3\n",
    "boss-sources": "=1\n=5r8r0r4=8r0=8r2\n=5r9r0=0r1r17=0r3r34\n",
    "backgrounds": "=1\n=4r0r0r1=1r5r3r0r0r7=152r0r0r0r2r1r2r3\n",
    "save-layout": "=1\n=0\n=1r1r0r1r8=1r3r0r3r6\n=1r0=1r1r1r3r0r0r6\n"
                   "=0r0r0=1r1r1r1r0r0r5=0\n=0r0r1=1r1r1r1r5r0r3=0\n"
                   "=1\n=2r1r0r1r3\n=0r0r0=1r1r2r1r0r0r3=0\n=0r0r1=0r1r2r18r52\n"
                   "=2\n=4r1r0r1r2\n=0r0r0=1r3r4r1r0r0r2=0" + _FP * 2 + "\n"
                   "=3\n=5r1r0r1r8\n=0r0r0=1r1r5r1r0r0r4=0=48r49r50r51\n"
                   "=0r0r1=1r1r5r1r4r0r4=1r0r0r0r0=48r49r50r51\n"
                   "=4\n=5r3r0r64r0\n=0r0r0=0r1r5r18r52\n=0r0r1=1r1r5r3r0r0r7=0" + _FP * 7
                   + "\n",
    "source-flags": "=1\n=1=0=0\n",
}
_PLANT_INVENTORY = ("## Extractions\n\n| Name | Group | Holds | Derived from | Format |\n"
                    "|---|---|---|---|---|\n"
                    + "".join("| %s | g | - | - | - |\n" % name for name in NAMES))


def _plant_variants():
    """(name, {file: text or None}, the lines the check must print)."""
    tree = _PLANT_TREE
    return [
        ("missing", {"source-flags.txt": None}, ["MISSING source-flags"]),
        ("extra", {"README.md": "# prose\n"}, ["EXTRA README.md"]),
        ("malformed", {"room-entities.txt": tree["room-entities"].replace("=1r2r5", "=1r2r05")},
         ["BADFILE room-entities: line 2: not canonical"]),
        ("non-canonical", {"boss-sources.txt": tree["boss-sources"].replace("=8r0=8r2",
                                                                          "=8r2=8r0")},
         ["BADFILE boss-sources: line 2: offsets not increasing inside the range"]),
        ("wrong count", {"room-objects.txt": tree["room-objects"].replace(
            "=0r0r0r1=0r1r1r5r0r3\n", "")},
         ["BADFILE room-objects: no line for table 0 entry 1"]),
        ("not rebuilt", {"backgrounds.txt": tree["backgrounds"].replace("r2r3\n", "r2\n")},
         ["BADFILE backgrounds: line 2: fields that do not rebuild the span"]),
        ("disagrees", {"save-layout.txt": tree["save-layout"].replace(
            "=0r0r1=1r1r1r1r5r0r3=0", "=0r0r1=1r1r1r1r5r0r2=0")},
         ["BADFILE room-objects: table 0 entry 1: not save-layout's span"]),
        ("save-layout", {"save-layout.txt": tree["save-layout"].replace(_FP + "\n=3",
                                                                        "\n=3")},
         ["BADFILE save-layout: line 13: table 2 entry 0: not one fingerprint per suffix"]),
    ]


def self_test():
    """Plants a correct tree, then each broken variant, and returns the first variant whose check
    output is not exactly its lines (the correct tree's name is `correct`), or None."""
    import shutil
    import tempfile
    for name, change, want in [("correct", {}, [])] + _plant_variants():
        root = tempfile.mkdtemp()
        try:
            files = {n + ".txt": text for n, text in _PLANT_TREE.items()}
            for f, text in dict(files, **change).items():
                if text is not None:
                    with open(os.path.join(root, f), "w", newline="\n") as stream:
                        stream.write(text)
            if check(root, _PLANT_INVENTORY, _PLANT_LAYOUT, _PLANT_BOSSES)[0] != want:
                return name
        except ExtractError:
            return name
        finally:
            shutil.rmtree(root)
    return None


TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
USAGE = __doc__.split("\n\n")[0]


def main(argv):
    options = {"--root": os.path.join(TOP, "tests", "ladx", "extract"),
               "--inventory": os.path.join(TOP, "notes", "ladx-inventory.md")}
    if not argv or argv[0] != "check" or len(argv) % 2 == 0:
        print(USAGE, file=sys.stderr)
        return 2
    for flag, value in zip(argv[1::2], argv[2::2]):
        if flag not in options:
            print(USAGE, file=sys.stderr)
            return 2
        options[flag] = value
    failed = self_test()
    if failed:
        print("extract: self-test failed: %s" % failed)
        return 1
    print("extract: self-test, %d planted trees as expected" % (len(_plant_variants()) + 1))
    try:
        with open(options["--inventory"], encoding="utf-8") as f:
            lines, summary = check(options["--root"], f.read())
    except (OSError, UnicodeDecodeError, ExtractError) as e:
        print("extract: %s" % e, file=sys.stderr)
        return 2
    print("\n".join(lines or [summary]))
    return 1 if lines else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
