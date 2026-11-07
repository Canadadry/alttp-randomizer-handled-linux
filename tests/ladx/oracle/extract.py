"""Usage: python3 -I tests/ladx/oracle/extract.py check [--root <dir>] [--inventory <file>]
       python3 tests/ladx/oracle/extract.py generate --out <dir> [--rom <file>] [--ladxr <dir>]
       python3 -I tests/ladx/oracle/extract.py verify [--root <dir>] [--inventory <file>]
              [--rom <file>] [--ladxr <dir>]

The extraction fixtures, tests/ladx/extract/: the source ROM's structure the inventory's
Extractions table names, so that C rebuilds the known-source map KS (known_source.py) and makes
the C groups' decisions without the ROM. This module is their format (`parse`, `render`, `read`,
`write`), the KS rebuild from the fixtures alone (`ks_spans`, `ks_rebuild`), the store model
(`store_table`, `store_model`: ROMWithTables.save's packing from the fixtures alone), `check`, the
ROM-free check against the inventory, `from_rom` and `generate`, the generator, and `verify`.
The layout of each file is in tests/ladx/README.md ("Extraction fixtures").

Format: only the characters `0-9`, `=`, `r` and the newline. Every line starts with `=`; tokens
are separated by `=`; the numbers inside a token by `r`; each number is 0..255 in decimal
without a leading zero; a larger quantity is split into bytes, high first. Line 1 is `=1`.

`check` prints one MISSING <name>, EXTRA <file> or BADFILE <name>: <reason> line per finding
(exit 1), else `extract: <n> fixtures, <b> bytes, every inventory extraction present`; a
malformed inventory exits 2. Every command proves `check` first on planted trees.

`generate` (driven by gen_extract.sh) needs the ROM (default tmp/ladx/base.gbc, SHA-1
d90ac17e...) and LADXR at the pin (default LADXR_DIR or tests/ladx/oracle/ladxr), else a
`setup:` line (exit 2). It parses the ROM with known_source.parse and with LADXR's
ROMWithTables (the storage regions from its PointerTables), cross-checks them
(known_source.check_ladxr, every entry's bytes, RoomEditor's entities and objects,
BackgroundEditor's tile maps), renders the six files in memory, runs the Nintendo-bytes check's
check_blob on each under the strict scope, and writes them only when nothing differs and no
literal run is found (else exit 1, nothing written, offsets and lengths only).

`verify` (ROM and LADXR, else `setup:`) prints `ok <check>` or `BAD <check>: <detail>` for parse
(the check above), ks (the map rebuilt from the fixtures equal to known_source.build(rom), span
by span and byte by byte), flags, bosses, backgrounds (tile maps against BackgroundEditor),
fingerprints (each the hash of its ROM suffix, no two suffixes sharing one) and store (the store
model's placements and written bytes equal to LADXR's own ROMWithTables.save stores), then
`extract-verify: 6 files, <k> KS spans, <b> KS bytes rebuilt, <n> bad`; exit 1 on any bad.
Never prints a byte value.
"""

import collections
import hashlib
import importlib.util
import os
import re
import struct
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
nintendo_bytes = _sibling("nintendo_bytes")
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


# source-flags: (bank, offset, value, flag when the byte equals value): the keyhole hook not
# installed, the witch off, the boomerang trade not patched.
FLAG_BYTES = [(0x02, 0x2FA9, 0xF0, True), (0x05, 0x08EF, 0x00, False), (0x19, 0x06C5, 0x00, True)]


def fingerprint(data):
    """The fingerprint of a suffix: the first 8 bytes of its SHA-512, as numbers."""
    return tuple(hashlib.sha512(data).digest()[:FINGERPRINT])


def _rom_entries(rom, table, spans):
    """{key: (flags, bank, start, length, raw)} of a table's pointers in store order."""
    out = {}
    size = table.info.get("data_size")
    by_key = {key: (where, length, bank, raw)
              for key, where, length, bank, raw in known_source._pointers(rom, table.info)}
    for key in _keys(table):
        where, length, bank, raw = by_key[key]
        span = spans[key]
        if isinstance(span, int):
            rebuilt, start, n = struct.pack("<H", span), None, None
        else:
            start, n = span[0] % BANK, span[1] - span[0]
            rebuilt = (bytes([(start - table.info["data_addr"]) // size]) if size is not None
                       else struct.pack("<H", start | 0x4000))
        flags = int(rom[where:where + length] == rebuilt)
        if "banks_bank" in table.info and isinstance(key, int):
            at = table.info["banks_bank"] * BANK + table.info["banks_addr"] + key
            flags |= 2 * (rom[at] == bank)
        out[key] = (flags, bank, start, n, span if isinstance(span, int) else None)
    return out


def _room_entry(rom, kind, bank, start, length):
    data = rom[bank * BANK + start:bank * BANK + start + length]
    parts = known_source.fields(kind, data)
    if parts is not None and known_source.rebuild(kind, parts) == data:
        return Entry(1, bank, start, length, parts)
    return Entry(0, bank, start, length, None)


def from_rom(rom, regions, layout=LAYOUT, bosses=BOSSES, flag_bytes=FLAG_BYTES):
    """{name: structure} of the six fixtures of a source ROM, as `read` gives them; regions
    [[(bank, start, end)]] are each table's storage regions after LADXR's parse."""
    spans = known_source.parse(rom, layout)
    rooms = {name: {} for name in ROOM_FILES}
    kinds = {kind: name for name, kind in ROOM_FILES.items()}
    sections = []
    for t, table in enumerate(layout):
        size = table.info.get("data_size")
        variable = size is None
        done = collections.defaultdict(dict)
        entries = {}
        for key, (flags, bank, start, length, raw) in _rom_entries(
                rom, table, spans[table.name]).items():
            if raw is not None:
                entries[key] = SaveEntry(flags, bank, None, None, raw, None, None)
                if table.kind in kinds and _known(table, key):
                    rooms[kinds[table.kind]][(t, key)] = None
                continue
            data = rom[bank * BANK + start:bank * BANK + start + length]
            if table.kind in kinds and _known(table, key):
                rooms[kinds[table.kind]][(t, key)] = _room_entry(rom, table.kind, bank, start,
                                                                 length)
            if isinstance(key, str):
                entries[key] = SaveEntry(flags, bank, start, length, None, None, None)
                continue
            dedup = done[bank].get(data)
            if dedup is None:
                if variable:
                    for skip in range(len(data)):
                        done[bank][data[skip:]] = (key, skip)
                done[bank][data] = (key, 0)
            extra = None
            if table.kind == "sprite":
                extra = list(data)
            elif table.kind in ("text", "background"):
                extra = [fingerprint(data[i:]) for i in range(len(data))]
            entries[key] = SaveEntry(flags, bank, start, length, None, dedup, extra)
        sections.append(Section(t, list(regions[t]), entries))
    out = {name: rooms[name] for name in ROOM_FILES}
    out["save-layout"] = sections
    out["boss-sources"] = _boss_sources(rom, bosses)
    out["source-flags"] = _source_flags(rom, flag_bytes)
    return out


def _boss_sources(rom, bosses):
    zero = bosses.zero_bank * BANK + bosses.zero_addr
    item = bosses.item_bank * BANK + bosses.item_addr
    return BossSources(
        bosses.zero_bank, bosses.zero_addr, bosses.zero_count,
        [bosses.zero_addr + i for i in range(bosses.zero_count) if rom[zero + i] == 0x00],
        bosses.item_bank, bosses.item_addr,
        [(room, rom[item + room]) for room in bosses.rooms])


def _source_flags(rom, flag_bytes):
    return tuple(int((rom[bank * BANK + at] == value) == equal)
                 for bank, at, value, equal in flag_bytes)


def guard(texts, rom, corpus):
    """[(name, view, offset, length)] of the literal runs of the rendered fixtures {name: text}
    under the Nintendo-bytes check's strict scope against rom."""
    index = nintendo_bytes.RomIndex(rom)
    out = []
    for name, text in texts.items():
        literals, _ = nintendo_bytes.check_blob(name, text.encode("ascii"), index, corpus,
                                                scope="strict")
        out += [(name, view, offset, length) for view, offset, length, _ in literals]
    return out


def write_fixtures(out, structures, rom, corpus, layout=LAYOUT):
    """Renders the six fixtures, refuses (ExtractError, nothing written) when the strict guard
    finds a literal run, else writes them into out."""
    texts = {name: write(name, structures[name], layout) for name in NAMES}
    found = guard(texts, rom, corpus)
    if found:
        raise ExtractError("refused, literal runs: " + ", ".join(
            "%s %s @%d %d bytes" % run for run in found))
    os.makedirs(out, exist_ok=True)
    for name, text in texts.items():
        with open(os.path.join(out, name + ".txt"), "w", encoding="ascii", newline="\n") as f:
            f.write(text)


def tile_map(parts):
    """{VRAM address: value} of a background entry's fields, as BackgroundEditor reads it."""
    tiles = {}
    for address, vertical, repeat, amount, values in parts:
        for n in range(amount + 1):
            tiles[address + n * (0x20 if vertical else 1)] = values[0 if repeat else n]
    return tiles


class StoreError(Exception):
    pass


# writes {source offset: byte} of the pointers; placements [(key, bank, start, length)] of the
# entries stored, in store order; leftover [(bank, start, end)] the regions' room left.
Stored = collections.namedtuple("Stored", "writes placements leftover")


def store_table(info, regions, entries):
    """LADXR's PointerTable.store without the ROM. info: the table's info; regions [(bank,
    start, end)] its storage in LADXR's order; entries {key: integer pointer or (bank,
    suffixes)} in store order (alt keys first), suffixes[i] a hashable equal for equal suffixes
    from byte i. StoreError when an entry finds no room or no pointer."""
    storage = [list(region) for region in regions]
    writes, placements, done = {}, [], collections.defaultdict(dict)

    def place(key, bank, length):
        for st in storage:
            if st[0] == bank and st[2] - st[1] >= length:
                st[1] += length
                placements.append((key, bank, st[1] - length, length))
                return st[1] - length
        raise StoreError("no room for entry %s in bank %d" % (_key_text(key), bank))

    def pointer16(where, pointer):
        writes[where] = pointer & 0xFF
        writes[where + 1] = (pointer >> 8) & 0xFF | 0x40

    base, size = info["pointers_bank"] * BANK + info["pointers_addr"], info.get("data_size")
    for key, (bank, addr) in info.get("alt_pointers", {}).items():
        pointer16(bank * BANK + addr, place(key, info["data_bank"], len(entries[key][1])))
    for n, key in enumerate(k for k in entries if not isinstance(k, str)):
        if isinstance(entries[key], int):
            pointer16(base + 2 * n, entries[key])
            continue
        bank, suffixes = entries[key]
        pointer = done[bank].get(suffixes[0])
        if pointer is None:
            pointer = place(key, bank, len(suffixes))
            for skip, suffix in enumerate(suffixes if size is None else suffixes[:1]):
                done[bank][suffix] = pointer + skip
        if "data_addr" not in info:
            pointer16(base + 2 * n, pointer)
            continue
        index, misaligned = divmod(pointer - info["data_addr"], size or 1)
        if misaligned or not 0 <= index <= 255:
            raise StoreError("entry %s at %x has no index" % (_key_text(key), pointer))
        writes[base + n] = index
    return Stored(writes, placements, [tuple(st) for st in storage])


def _add_storage(regions, extra):
    """LADXR's PointerTable.addStorage: regions with extra added, merged and sorted by start."""
    storage = [list(region) for region in regions]
    for bank, start, end in extra:
        for st in storage:
            if st[0] == bank:
                if st[1] == end:
                    st[1] = start
                    break
                if st[2] == start:
                    st[2] = end
                    break
                if st[1] <= start and st[2] >= end:
                    break
        else:
            storage.append([bank, start, end])
    merged = True
    while merged:
        merged = False
        for n, m in ((n, m) for n in range(len(storage)) for m in range(len(storage))):
            if m != n and storage[n][0] == storage[m][0] and \
                    storage[m][1] - 1 <= storage[n][2] <= storage[m][2]:
                storage[n][1] = min(storage[n][1], storage[m][1])
                storage[n][2] = storage[m][2]
                storage.pop(m)
                merged = True
                break
    return [tuple(st) for st in sorted(storage, key=lambda st: st[1])]


def _prints(data):
    return [fingerprint(data[i:]) for i in range(len(data))]


def _source_suffixes(fixtures, layout):
    """{(t, key): [fingerprint of each suffix]} of every source entry with a span: from
    save-layout's fingerprints or sprite values, the room files' fields, else the entry it
    equals by save-layout's de-duplication."""
    content = {"rooms": fixtures["room-objects"], "data": fixtures["room-entities"],
               "background": fixtures["backgrounds"]}
    out = {}
    for t, table in enumerate(layout):
        for key, e in fixtures["save-layout"][t].entries.items():
            if e.raw is not None:
                continue
            room = content.get(table.kind, {}).get((t, key))
            if table.kind == "sprite":
                keys = _prints(bytes(e.extra))
            elif e.extra is not None:
                keys = list(e.extra)
            elif room is not None and room.known:
                keys = _prints(known_source.rebuild(table.kind, room.parts))
            elif e.dedup is not None:
                keys = out[(t, e.dedup[0])][e.dedup[1]:]
            else:
                raise StoreError("no content for %s" % _label(t, key))
            if len(keys) != e.length:
                raise StoreError("%s: content not its span's length" % _label(t, key))
            out[(t, key)] = keys
    return out


StoreModel = collections.namedtuple("StoreModel", "writes placements")
OVERWORLD_SPRITES, INDOOR_SPRITES = "room_sprite_data_overworld", "room_sprite_data_indoor"
SPRITE_BASE_LOAD = 0x0DFA


def store_model(fixtures, layout=LAYOUT, changed=None):
    """ROMWithTables.save's table stores from the fixtures alone ({name: structure}, as `load`
    gives them): writes {source offset: byte} of every pointer and of the `ld hl` at 0x0DFA,
    placements [(table, key, bank, start, length)] in store order. changed {(table, key): bytes
    or integer pointer} replaces those entries; every other entry is the source's."""
    changed = changed or {}
    source = _source_suffixes(fixtures, layout)
    writes, placements, leftover = {}, [], None
    for t, table in enumerate(layout):
        section = fixtures["save-layout"][t]
        entries = {}
        for key, e in section.entries.items():
            value = changed.get((t, key))
            if isinstance(value, (bytes, bytearray)):
                entries[key] = (e.bank, _prints(bytes(value)))
            elif value is not None:
                entries[key] = value
            else:
                entries[key] = e.raw if e.raw is not None else (e.bank, source[(t, key)])
        info, regions = table.info, section.regions
        if table.name == INDOOR_SPRITES and leftover is not None:
            regions = _add_storage(regions, leftover)
            info = dict(info, data_addr=leftover[0][1])
        stored = store_table(info, regions, entries)
        writes.update(stored.writes)
        placements += [(t,) + placement for placement in stored.placements]
        if table.name == OVERWORLD_SPRITES:
            leftover = stored.leftover
            base = leftover[0][1] | 0x4000
            writes.update({SPRITE_BASE_LOAD: 0x21, SPRITE_BASE_LOAD + 1: base & 0xFF,
                           SPRITE_BASE_LOAD + 2: base >> 8})
    return StoreModel(writes, placements)


def _verify_ks(fixtures, rom, ks, layout):
    want = {label: (start, rom[start:stop]) for start, stop, label in ks.spans}
    got = {label: (start, data) for start, data, label in ks_spans(fixtures, layout)}
    bad = []
    for label, (start, data) in want.items():
        if label not in got:
            bad.append("%s: missing" % label)
        elif got[label][0] != start or len(got[label][1]) != len(data):
            bad.append("%s: another span" % label)
        elif got[label][1] != data:
            bad.append("%s: bytes differ" % label)
    bad += ["%s: not in KS" % label for label in got if label not in want]
    if not bad:
        rebuilt = ks_rebuild(fixtures, layout)
        if len(rebuilt) != len(ks) or any(q not in ks or rom[q] != b for q, b in rebuilt.items()):
            bad.append("map: offsets or bytes differ")
    return bad


def _verify_bosses(sources, rom, bosses):
    want = _boss_sources(rom, bosses)
    bad = ["zero %x" % q for q in sorted(set(want.zeros) ^ set(sources.zeros))]
    got = dict(sources.items)
    bad += ["item of room %x" % room for room, item in want.items if got.get(room) != item]
    return bad


def _verify_fingerprints(sections, rom, layout):
    bad, by_print, by_suffix = [], {}, {}
    for section in sections:
        if layout[section.table].kind not in ("text", "background"):
            continue
        for key, e in section.entries.items():
            if e.extra is None:
                continue
            data = rom[e.bank * BANK + e.start:e.bank * BANK + e.start + e.length]
            for i, fp in enumerate(e.extra):
                where = "%s suffix %d" % (_label(section.table, key), i)
                suffix = data[i:]
                if fp != fingerprint(suffix):
                    bad.append("%s: not the hash of its suffix" % where)
                if by_print.setdefault(fp, suffix) != suffix:
                    bad.append("%s: shared with another suffix" % where)
                if by_suffix.setdefault(suffix, fp) != fp:
                    bad.append("%s: another fingerprint of the same suffix" % where)
    return bad


def _verify_store(fixtures, stored, layout):
    """The differences between the model's packing of the fixtures and stored, LADXR's
    (ladxr_store's shape): placements per table, then written offsets."""
    try:
        model = store_model(fixtures, layout)
    except StoreError as e:
        return [str(e)]
    bad = []
    for t in range(len(layout)):
        ours = [p for p in model.placements if p[0] == t]
        theirs = [p for p in stored.placements if p[0] == t]
        for (_, key, *span), other in zip(ours, theirs):
            if tuple(span) != other[1:]:
                bad.append("%s: placed at another span" % _label(t, key))
                break
        else:
            if len(ours) != len(theirs):
                bad.append("table %d: %d entries placed, LADXR %d" % (t, len(ours),
                                                                     len(theirs)))
    for q in sorted(set(model.writes) | set(stored.writes)):
        if q not in stored.writes:
            bad.append("offset %x: not written by LADXR" % q)
        elif q not in model.writes:
            bad.append("offset %x: not written by the model" % q)
        elif model.writes[q] != stored.writes[q]:
            bad.append("offset %x: another byte" % q)
    return bad


def verify(fixtures, rom, tiles, stored, layout=LAYOUT, bosses=BOSSES, flag_bytes=FLAG_BYTES):
    """(lines, KS spans, KS bytes) of the fixtures ({name: structure}) against the source ROM:
    `ok <check>` or `BAD <check>: <detail>` for ks, flags, bosses, backgrounds (tiles: {(table,
    key): {address: value}}, LADXR's BackgroundEditor maps), fingerprints and store (stored:
    LADXR's own store of the unchanged tables, as ladxr_store gives it); offsets and labels
    only, never a byte value."""
    ks = known_source.build(rom, layout, bosses)
    want_flags = _source_flags(rom, flag_bytes)
    backgrounds = fixtures["backgrounds"]
    checks = [
        ("ks", _verify_ks(fixtures, rom, ks, layout)),
        ("flags", ["flag %d" % i for i, (a, b) in enumerate(zip(want_flags,
                                                                 fixtures["source-flags"]), 1)
                   if a != b]),
        ("bosses", _verify_bosses(fixtures["boss-sources"], rom, bosses)),
        ("backgrounds", [_label(*k) for k in sorted(set(backgrounds) | set(tiles), key=str)
                         if backgrounds.get(k) is None or not backgrounds[k].known
                         or tile_map(backgrounds[k].parts) != tiles.get(k)]),
        ("fingerprints", _verify_fingerprints(fixtures["save-layout"], rom, layout)),
        ("store", _verify_store(fixtures, stored, layout)),
    ]
    lines = []
    for name, bad in checks:
        lines += ["BAD %s: %s" % (name, detail) for detail in bad] or ["ok " + name]
    return lines, len(ks.spans), len(ks)


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
LADX_SHA1 = "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"
ROOM_BASES = {"rooms_overworld_top": 0, "rooms_overworld_bottom": 0x80, "rooms_indoor_a": 0x100,
              "rooms_indoor_b": 0x200, "rooms_color_dungeon": 0x300}


class SetupError(Exception):
    pass


def read_rom(path):
    """The bytes of the LADX source ROM at path; SetupError when absent or another ROM."""
    if not os.path.isfile(path):
        raise SetupError("no ROM at %s (run fetch_rom.sh)" % path)
    with open(path, "rb") as f:
        rom = f.read()
    sha1 = hashlib.sha1(rom).hexdigest()
    if sha1 != LADX_SHA1:
        raise SetupError("ROM SHA-1 %s is not the LADX English 1.0 ROM (%s...)" % (sha1,
                                                                                LADX_SHA1[:8]))
    return rom


def ladxr_corpus(ladxr):
    """The LADXR checkout's Corpus, checked at the pin; SetupError otherwise."""
    try:
        return nintendo_bytes.load_ladxr(ladxr)
    except nintendo_bytes.SetupError as e:
        raise SetupError(str(e))


def _ladxr_modules(ladxr):
    sys.path.insert(0, os.path.abspath(ladxr))
    sys.dont_write_bytecode = True
    import romTables
    import roomEditor
    import backgroundEditor
    return romTables, roomEditor, backgroundEditor


def ladxr_source(rom_path, ladxr):
    """LADXR's ROMWithTables of the source ROM, parsed at the pin."""
    romTables, _, _ = _ladxr_modules(ladxr)
    with open(rom_path, "rb") as f:
        return romTables.ROMWithTables(f)


def ladxr_regions(source, layout=LAYOUT):
    """[[(bank, start, end)]] of each table's storage regions after LADXR's parse."""
    return [[(st["bank"], st["start"], st["end"])
             for st in getattr(source, table.name)._PointerTable__storage] for table in layout]


def ladxr_tiles(source, ladxr, layout=LAYOUT):
    """{(table, key): {address: value}} of BackgroundEditor for every background entry KS
    rebuilds."""
    _, _, backgroundEditor = _ladxr_modules(ladxr)
    out = {}
    for t, table in enumerate(layout):
        if table.kind == "background":
            for key in table.known:
                editor = backgroundEditor.BackgroundEditor(
                    source, key, attributes=table.name == "background_attributes")
                out[(t, key)] = dict(editor.tiles)
    return out


def ladxr_store(rom_path, ladxr, layout=LAYOUT):
    """LADXR's own ROMWithTables.save on a fresh parse of the source ROM, the file write
    skipped: StoreModel(writes {source offset: byte} of every byte written outside the tables'
    placements, placements [(table, bank, start, length)] of each table's slice writes)."""
    _ladxr_modules(ladxr)
    import rom as rom_module
    source = ladxr_source(rom_path, ladxr)
    current, placements, offsets = [None], [], set()

    class Bank(bytearray):
        def __setitem__(self, index, value):
            if isinstance(index, slice) and current[0] is not None:
                placements.append((current[0], self.number, index.start,
                                   index.stop - index.start))
            elif isinstance(index, slice):
                offsets.update(self.number * BANK + q for q in range(*index.indices(BANK)))
            else:
                offsets.add(self.number * BANK + index)
            super().__setitem__(index, value)

    banks = []
    for n, data in enumerate(source.banks):
        banks.append(Bank(data))
        banks[-1].number = n
    source.banks = banks

    def tagged(t, store):
        def run(rom):
            current[0] = t
            try:
                return store(rom)
            finally:
                current[0] = None
        return run

    for t, table in enumerate(layout):
        pointer_table = getattr(source, table.name)
        pointer_table.store = tagged(t, pointer_table.store)
    real_save, rom_module.ROM.save = rom_module.ROM.save, lambda self, file, name=None: None
    try:
        source.save(None, name=None)
    finally:
        rom_module.ROM.save = real_save
    return StoreModel({q: source.banks[q // BANK][q % BANK] for q in offsets}, placements)


def ladxr_crosscheck(rom_path, ladxr, rom, source, structures, layout=LAYOUT):
    """MISMATCH lines between the structures and LADXR's own parse: known_source.check_ladxr
    (spans and the RoomEditor round trip), every entry's bytes or integer pointer, RoomEditor's
    entities and objects, BackgroundEditor's tile maps."""
    _, roomEditor, _ = _ladxr_modules(ladxr)
    lines = known_source.check_ladxr(rom_path, ladxr, rom)
    for section in structures["save-layout"]:
        table = getattr(source, layout[section.table].name)
        for key, e in section.entries.items():
            ours = e.raw if e.raw is not None else \
                rom[e.bank * BANK + e.start:e.bank * BANK + e.start + e.length]
            theirs = table[key] if isinstance(table[key], int) else bytes(table[key])
            if ours != theirs:
                lines.append("MISMATCH entry %s" % _label(section.table, key))
    for (t, key), entry in structures["room-entities"].items():
        if entry is not None and entry.known and _has_objects(source, key) and \
                roomEditor.RoomEditor(source, key).entities != entry.parts:
            lines.append("MISMATCH entities %s" % _label(t, key))
    for (t, key), entry in structures["room-objects"].items():
        if entry is None or not entry.known:
            continue
        room = key if isinstance(key, str) else ROOM_BASES[layout[t].name] + key
        editor = roomEditor.RoomEditor(source, room)
        data = bytes([editor.animation_id, editor.floor_object]) + b"".join(
            bytes(obj.export()) for obj in editor.objects) + b"\xfe"
        if data != known_source.rebuild("rooms", entry.parts):
            lines.append("MISMATCH objects %s" % _label(t, key))
    tiles = ladxr_tiles(source, ladxr, layout)
    for k, entry in structures["backgrounds"].items():
        if entry is None or not entry.known or tile_map(entry.parts) != tiles.get(k):
            lines.append("MISMATCH background %s" % _label(*k))
    return lines


def _has_objects(source, room):
    """Whether RoomEditor can open room: its object table holds it (0x2FF has none)."""
    base, name = max((b, n) for n, b in ROOM_BASES.items() if b <= room)
    return room - base < len(getattr(source, name))


def generate(out, rom_path, ladxr):
    """Writes the six fixtures of the ROM into out through LADXR at the pin; SetupError on a
    wrong ROM or checkout, ExtractError on a cross-check mismatch or a literal run (nothing
    written). Returns the summary line."""
    rom = read_rom(rom_path)
    corpus = ladxr_corpus(ladxr)
    source = ladxr_source(rom_path, ladxr)
    structures = from_rom(rom, ladxr_regions(source))
    mismatches = ladxr_crosscheck(rom_path, ladxr, rom, source, structures)
    if mismatches:
        raise ExtractError("LADXR disagrees: " + ", ".join(mismatches[:10]) +
                           (" (%d in all)" % len(mismatches) if len(mismatches) > 10 else ""))
    write_fixtures(out, structures, rom, corpus)
    size = sum(os.path.getsize(os.path.join(out, name + ".txt")) for name in NAMES)
    return "extract: %d files written to %s, %d bytes, 0 literal runs (strict)" % (
        len(NAMES), out, size)


def verify_tree(root, inventory_text, rom_path, ladxr):
    """(lines, exit code) of `verify`: parse (the ROM-free check), then `verify`'s checks."""
    rom = read_rom(rom_path)
    ladxr_corpus(ladxr)
    lines, _ = check(root, inventory_text)
    if lines:
        lines = ["BAD parse: " + line for line in lines]
        return lines + ["extract-verify: %d files, 0 KS spans, 0 KS bytes rebuilt, %d bad" % (
            len(NAMES), len(lines))], 1
    fixtures = load(root)
    found, spans, size = verify(fixtures, rom, ladxr_tiles(ladxr_source(rom_path, ladxr), ladxr),
                                ladxr_store(rom_path, ladxr))
    lines = ["ok parse"] + found
    bad = sum(line.startswith("BAD ") for line in lines)
    return lines + ["extract-verify: %d files, %d KS spans, %d KS bytes rebuilt, %d bad" % (
        len(NAMES), spans, size, bad)], 1 if bad else 0


OPTIONS = {"check": ("--root", "--inventory"), "generate": ("--out", "--rom", "--ladxr"),
           "verify": ("--root", "--inventory", "--rom", "--ladxr")}


def main(argv):
    options = {"--root": os.path.join(TOP, "tests", "ladx", "extract"),
               "--inventory": os.path.join(TOP, "notes", "ladx-inventory.md"),
               "--rom": os.path.join(TOP, "tmp", "ladx", "base.gbc"),
               "--ladxr": os.environ.get("LADXR_DIR") or os.path.join(HERE, "ladxr"),
               "--out": None}
    if not argv or argv[0] not in OPTIONS or len(argv) % 2 == 0:
        print(USAGE, file=sys.stderr)
        return 2
    command = argv[0]
    for flag, value in zip(argv[1::2], argv[2::2]):
        if flag not in OPTIONS[command]:
            print(USAGE, file=sys.stderr)
            return 2
        options[flag] = value
    if command == "generate" and options["--out"] is None:
        print(USAGE, file=sys.stderr)
        return 2
    failed = self_test()
    if failed:
        print("extract: self-test failed: %s" % failed)
        return 1
    print("extract: self-test, %d planted trees as expected" % (len(_plant_variants()) + 1))
    try:
        if command == "generate":
            print(generate(options["--out"], options["--rom"], options["--ladxr"]))
            return 0
        with open(options["--inventory"], encoding="utf-8") as f:
            inventory = f.read()
        if command == "verify":
            lines, code = verify_tree(options["--root"], inventory, options["--rom"],
                                      options["--ladxr"])
            print("\n".join(lines))
            return code
        lines, summary = check(options["--root"], inventory)
    except SetupError as e:
        print("setup: extract: %s" % e)
        return 2
    except ExtractError as e:
        print("extract: %s" % e, file=sys.stderr)
        return 2 if command == "check" else 1
    except (OSError, UnicodeDecodeError) as e:
        print("extract: %s" % e, file=sys.stderr)
        return 2
    print("\n".join(lines or [summary]))
    return 1 if lines else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
