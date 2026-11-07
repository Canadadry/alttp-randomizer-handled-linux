"""Usage: python3 -I tests/ladx/oracle/known_source.py [--rom <file>] [--ladxr <dir>] [--spans <file>]

The C-knowable source map (KS) of write_log.py's encoding rule: the source ROM offsets whose
values C can parse from the user's ROM at runtime, built here from the ROM itself and only with
the ROM present (never committed in any form). `build(rom) -> KnownSource` parses the eleven
tables of ROMWithTables like LADXR at the pin (LAYOUT, from romTables.py) and keeps, each only
when its parsed fields rebuild its bytes exactly (`fields`, `rebuild`):
  every room's entity entry 0x000..0x315 and every room's object entry, alt rooms included;
  background entries 3, 4 and 0x0D of the tiles, 3 and 4 of the attributes;
  every room sprite data entry, and the pointer bytes (and bank bytes) of every table `save`
  re-serializes, recomputed from the entries' spans or integer pointers;
  the bytes of bank 0x14 0x04E0..0x051F that hold 0x00, and the item ids at bank 0x3E 0x3300
  plus each boss room.
A span whose fields do not rebuild it is left out and named in `failures`.
Its lookups: `entries` {(table, key): (start, stop)} of the entry spans kept, and `span_at(q)`,
the span holding source offset q with the lowest start, None outside the map (write_log.py's
fragment rule).

With --rom (default tmp/ladx/base.gbc) it prints `NOT-REBUILT <span>` per failure and the
summary `known-source: <n> spans, <b> bytes, <f> not rebuilt`. With --ladxr too (a checkout at
the pin) it also checks the parse against LADXR's own (every entry's source span, through
read_trace.py's origins) and that RoomEditor round-trips every unchanged source room, entity
and object entries included: one `MISMATCH <what>` line per difference, exit 1 on any. With
--spans it also writes `spans(ks)`, the span list C's KS writes (tests/ladx/README.md, "KS span
list"): offsets, lengths and labels, never a byte value.
"""

import argparse
import collections
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
DEFAULT_ROM = os.path.join(TOP, "tmp", "ladx", "base.gbc")
BANK = 0x4000

Table = collections.namedtuple("Table", "name info kind known")
Bosses = collections.namedtuple("Bosses",
                                "zero_bank zero_addr zero_count item_bank item_addr rooms")

ROOM_KEYS = "all"
LAYOUT = [
    Table("texts", {"count": 0x2B0, "pointers_addr": 1, "pointers_bank": 0x1C,
                    "banks_addr": 0x741, "banks_bank": 0x1C}, "text", None),
    Table("entities", {"count": 0x320, "pointers_addr": 0, "pointers_bank": 0x16,
                       "data_bank": 0x16}, "data", range(0x316)),
    Table("rooms_overworld_top", {"count": 0x080, "pointers_addr": 0x000, "pointers_bank": 0x09,
                                  "data_bank": 0x09, "alt_pointers": {
                                      "Alt06": (0x00, 0x31FD), "Alt0E": (0x00, 0x31CD),
                                      "Alt1B": (0x00, 0x320D), "Alt2B": (0x00, 0x321D),
                                      "Alt79": (0x00, 0x31ED)}}, "rooms", ROOM_KEYS),
    Table("rooms_overworld_bottom", {"count": 0x080, "pointers_addr": 0x100,
                                     "pointers_bank": 0x09, "data_bank": 0x1A,
                                     "alt_pointers": {"Alt8C": (0x00, 0x31DD)}},
          "rooms", ROOM_KEYS),
    Table("rooms_indoor_a", {"count": 0x100, "pointers_addr": 0x000, "pointers_bank": 0x0A,
                             "data_bank": 0x0A, "alt_pointers": {"Alt1F5": (0x00, 0x31A1)}},
          "rooms", ROOM_KEYS),
    Table("rooms_indoor_b", {"count": 0x0FF, "pointers_addr": 0x000, "pointers_bank": 0x0B,
                             "data_bank": 0x0B}, "rooms", ROOM_KEYS),
    Table("rooms_color_dungeon", {"count": 0x016, "pointers_addr": 0x3B77, "pointers_bank": 0x0A,
                                  "data_bank": 0x0A}, "rooms", ROOM_KEYS),
    Table("room_sprite_data_overworld", {"count": 0x100, "pointers_addr": 0x30D3,
                                         "pointers_bank": 0x20, "data_bank": 0x20,
                                         "data_addr": 0x33F3, "data_size": 4}, "sprite", "all"),
    Table("room_sprite_data_indoor", {"count": 0x220, "pointers_addr": 0x31D3,
                                      "pointers_bank": 0x20, "data_bank": 0x20,
                                      "data_addr": 0x363B, "data_size": 4}, "sprite", "all"),
    Table("background_tiles", {"count": 0x26, "pointers_addr": 0x052B, "pointers_bank": 0x20,
                               "data_bank": 0x08}, "background", [0x03, 0x04, 0x0D]),
    Table("background_attributes", {"count": 0x26, "pointers_addr": 0x1C4B,
                                    "pointers_bank": 0x24, "data_bank": 0x24}, "background",
          [0x03, 0x04]),
]
BOSS_ROOMS = [0x106, 0x12B, 0x15A, 0x166, 0x185, 0x1BC, 0x223, 0x234, 0x300]
BOSSES = Bosses(zero_bank=0x14, zero_addr=0x04E0, zero_count=64, item_bank=0x3E,
                item_addr=0x3300, rooms=BOSS_ROOMS)
END_OF_DATA = {"text": (0xFE, 0xFF), "data": (0xFF,)}


class KnownSource:
    """The map: `q in ks` for a source offset; `spans` [(start, stop, label)], `failures`
    [label] of the spans left out, len() the number of offsets; `entries` {(table, key): (start,
    stop)} of the table entries among the spans (pointer, bank and boss bytes are not entries)."""

    def __init__(self, size):
        self.mask = bytearray(size)
        self.spans = []
        self.failures = []
        self.entries = {}
        self._span_of = None

    def add(self, start, stop, label, entry=None):
        """entry: the (table, key) whose source entry the span is, None for any other span."""
        self.spans.append((start, stop, label))
        self.mask[start:stop] = b"\x01" * (stop - start)
        if entry is not None:
            self.entries[entry] = (start, stop)
        self._span_of = None

    def span_at(self, q):
        """The (start, stop) of the span holding source offset q with the lowest start (the
        longest of those), None outside the map."""
        if self._span_of is None:
            self._span_of = {}
            for start, stop, _ in sorted(self.spans, key=lambda s: (s[0], -s[1]), reverse=True):
                for p in range(start, stop):
                    self._span_of[p] = (start, stop)
        return self._span_of.get(q)

    def __contains__(self, q):
        return 0 <= q < len(self.mask) and self.mask[q] == 1

    def __len__(self):
        return self.mask.count(1)


def _entry_end(rom, kind, start, size):
    """The end of the entry at start, as the table's _readData finds it."""
    p = start
    if kind == "sprite":
        return start + size
    if kind in END_OF_DATA:
        while rom[p] not in END_OF_DATA[kind]:
            p += 1
        return p + 1
    if kind == "rooms":
        p += 2
        while rom[p] != 0xFE:
            p += {0xE0: 5, 0xC0: 3, 0x80: 3}.get(rom[p] & 0xF0, 2)
        return p + 1
    while rom[p] != 0x00:
        flags = rom[p + 2]
        p += 3 + (1 if flags & 0x40 else (flags & 0x3F) + 1)
    return p + 1


def _pointers(rom, info):
    """[(key, pointer bytes' offset, length, entry bank, raw pointer)] of a table."""
    out = []
    pb, addr = info["pointers_bank"] * BANK, info["pointers_addr"]
    for key, (bank, at) in info.get("alt_pointers", {}).items():
        out.append((key, bank * BANK + at, 2, info["data_bank"],
                    struct.unpack_from("<H", rom, bank * BANK + at)[0]))
    for n in range(info["count"]):
        if "data_addr" in info:
            raw = info["data_addr"] + 0x4000 + rom[pb + addr + n] * info["data_size"]
            where, length = pb + addr + n, 1
        else:
            raw = struct.unpack_from("<H", rom, pb + addr + 2 * n)[0]
            where, length = pb + addr + 2 * n, 2
        if "data_bank" in info:
            bank = info["data_bank"]
        else:
            bank = rom[info["banks_bank"] * BANK + info["banks_addr"] + n] & 0x3F
        out.append((n, where, length, bank, raw))
    return out


def parse(rom, layout=LAYOUT):
    """{table: {key: (start, stop) absolute source span, or the integer pointer}}."""
    tables = {}
    for table in layout:
        entries = {}
        for key, _, _, bank, raw in _pointers(rom, table.info):
            if 0x4000 <= raw < 0x8000:
                start = bank * BANK + (raw & 0x3FFF)
                entries[key] = (start, _entry_end(rom, table.kind, start,
                                                  table.info.get("data_size")))
            else:
                entries[key] = raw
        tables[table.name] = dict(sorted(entries.items(), key=lambda kv: (isinstance(kv[0], str),
                                                                          kv[0])))
    return tables


def fields(kind, data):
    """The parsed fields of one entry (decimal-sized numbers), None when the entry is not
    made of whole fields ending at its terminator."""
    try:
        if kind == "sprite":
            return [list(data)]
        if kind == "data":
            out, i = [], 0
            while data[i] != 0xFF:
                out.append((data[i] & 0x0F, data[i] >> 4, data[i + 1]))
                i += 2
            return out if i == len(data) - 1 else None
        if kind == "rooms":
            out, i = [data[0], data[1]], 2
            while data[i] != 0xFE:
                kind_nibble = data[i] >> 4
                if kind_nibble in (0x8, 0xC):
                    out.append(("hv"[kind_nibble == 0xC], data[i] & 0x0F, data[i + 1] & 0x0F,
                                data[i + 1] >> 4, data[i + 2]))
                    i += 3
                elif kind_nibble == 0xE:
                    out.append(("w", data[i] & 0x0F) + tuple(data[i + 1:i + 5]))
                    if len(data[i + 1:i + 5]) != 4:
                        return None
                    i += 5
                else:
                    out.append(("s", data[i] & 0x0F, data[i] >> 4, data[i + 1]))
                    i += 2
            return out if i == len(data) - 1 else None
        if kind == "background":
            out, i = [], 0
            while data[i] != 0x00:
                flags = data[i + 2]
                count = 1 if flags & 0x40 else (flags & 0x3F) + 1
                values = list(data[i + 3:i + 3 + count])
                if len(values) != count:
                    return None
                out.append((data[i] << 8 | data[i + 1], flags >> 7, flags >> 6 & 1, flags & 0x3F,
                            values))
                i += 3 + count
            return out if i == len(data) - 1 else None
    except IndexError:
        return None
    raise ValueError("no fields for kind %r" % kind)


def rebuild(kind, parts):
    """The bytes an entry's fields give back."""
    if kind == "sprite":
        return bytes(parts[0])
    if kind == "data":
        return b"".join(bytes([x | y << 4, t]) for x, y, t in parts) + b"\xff"
    if kind == "rooms":
        out = bytearray(parts[:2])
        for obj in parts[2:]:
            if obj[0] in "hv":
                out += bytes([(0x80 if obj[0] == "h" else 0xC0) | obj[1], obj[2] | obj[3] << 4,
                              obj[4]])
            elif obj[0] == "w":
                out += bytes([0xE0 | obj[1]] + list(obj[2:]))
            else:
                out += bytes([obj[1] | obj[2] << 4, obj[3]])
        return bytes(out) + b"\xfe"
    out = bytearray()
    for address, vertical, repeat, amount, values in parts:
        out += bytes([address >> 8, address & 0xFF, vertical << 7 | repeat << 6 | amount] + values)
    return bytes(out) + b"\x00"


def _known_key(table, key):
    if table.known is None:
        return False
    if table.known == "all":
        return True
    return key in table.known


def build(rom, layout=LAYOUT, bosses=BOSSES):
    """The KnownSource of a source ROM."""
    ks = KnownSource(len(rom))
    tables = parse(rom, layout)
    for table in layout:
        entries = tables[table.name]
        size = table.info.get("data_size")
        for key, where, length, bank, raw in _pointers(rom, table.info):
            entry = entries[key]
            if isinstance(entry, int):
                rebuilt = struct.pack("<H", entry)
            elif size is not None:
                rebuilt = bytes([(entry[0] % BANK - table.info["data_addr"]) // size])
            else:
                rebuilt = struct.pack("<H", entry[0] % BANK | 0x4000)
            label = "%s pointer %s" % (table.name, _key_text(key))
            if rom[where:where + length] == rebuilt:
                ks.add(where, where + length, label)
            else:
                ks.failures.append(label)
            if "banks_bank" in table.info and isinstance(key, int):
                at = table.info["banks_bank"] * BANK + table.info["banks_addr"] + key
                label = "%s bank %s" % (table.name, _key_text(key))
                if rom[at] == bank:
                    ks.add(at, at + 1, label)
                else:
                    ks.failures.append(label)
        for key, entry in entries.items():
            if isinstance(entry, int) or not _known_key(table, key):
                continue
            data = rom[entry[0]:entry[1]]
            parts = fields(table.kind, data)
            label = "%s %s" % (table.name, _key_text(key))
            if parts is not None and rebuild(table.kind, parts) == data:
                ks.add(entry[0], entry[1], label, (table.name, key))
            else:
                ks.failures.append(label)
    zero = bosses.zero_bank * BANK + bosses.zero_addr
    for q in range(zero, zero + bosses.zero_count):
        if rom[q] == 0x00:
            ks.add(q, q + 1, "boss zero %x" % q)
    for room in bosses.rooms:
        q = bosses.item_bank * BANK + bosses.item_addr + room
        ks.add(q, q + 1, "boss item %x" % room)
    return ks


def _key_text(key):
    return "%x" % key if isinstance(key, int) else key


def summary(ks):
    return "known-source: %d spans, %d bytes, %d not rebuilt" % (len(ks.spans), len(ks),
                                                               len(ks.failures))


def spans(ks):
    """The span list text: `ladx-ks-spans 1`, `<start> <length> <label>` per span in hex in the
    build's order, then `failure <label>` per span left out."""
    lines = ["ladx-ks-spans 1"]
    lines += ["%x %x %s" % (start, stop - start, label) for start, stop, label in ks.spans]
    lines += ["failure " + label for label in ks.failures]
    return "".join(line + "\n" for line in lines)


def check_ladxr(rom_path, ladxr, rom):
    """MISMATCH lines between this parse and LADXR's, and for every unchanged source room that
    RoomEditor does not store back byte for byte."""
    sys.path.insert(0, HERE)
    import read_trace
    sys.path.insert(0, os.path.abspath(ladxr))
    sys.dont_write_bytecode = True
    import rom as rom_module
    import romTables
    import pointerTable
    import roomEditor
    lines = []
    trace = read_trace.install({"ROM": rom_module.ROM, "ROMWithTables": romTables.ROMWithTables,
                                "PointerTable": pointerTable.PointerTable}, [])
    try:
        with open(rom_path, "rb") as f:
            source = romTables.ROMWithTables(f)
    finally:
        trace.uninstall()
    ours = parse(rom)
    theirs = collections.defaultdict(dict)
    for table, key, value, origins in trace.initial():
        if origins is None:
            theirs[table][key] = value
        else:
            theirs[table][key] = (origins[0], origins[-1] + 1) if origins and list(origins) == \
                list(range(origins[0], origins[0] + len(origins))) else ("not a span",)
    for table in [t.name for t in LAYOUT]:
        for key in sorted(set(ours[table]) | set(theirs[table]),
                          key=lambda k: (isinstance(k, str), k)):
            if ours[table].get(key) != theirs[table].get(key):
                lines.append("MISMATCH parse %s %s" % (table, _key_text(key)))
    if set(theirs) != {t.name for t in LAYOUT}:
        lines.append("MISMATCH tables %s" % " ".join(sorted(theirs)))
    rooms = list(range(0x316)) + [k for t in LAYOUT for k in t.info.get("alt_pointers", {})]
    for room in rooms:
        before = _room_bytes(source, room)
        if before is None:
            continue
        roomEditor.RoomEditor(source, room).store(source)
        if _room_bytes(source, room) != before:
            lines.append("MISMATCH room %s" % _key_text(room))
    return lines


def _room_bytes(rom, room):
    if isinstance(room, str):
        for name in ("rooms_overworld_top", "rooms_overworld_bottom", "rooms_indoor_a"):
            table = getattr(rom, name)
            if room in table:
                return bytes(table[room]), None
    tables = [(0x080, "rooms_overworld_top", 0), (0x100, "rooms_overworld_bottom", 0x80),
              (0x200, "rooms_indoor_a", 0x100), (0x300, "rooms_indoor_b", 0x200),
              (0x400, "rooms_color_dungeon", 0x300)]
    name, base = next((n, b) for limit, n, b in tables if room < limit)
    if room - base >= len(getattr(rom, name)):
        return None
    return bytes(getattr(rom, name)[room - base]), bytes(rom.entities[room])


def main(argv):
    parser = argparse.ArgumentParser(prog="known_source.py")
    parser.add_argument("--rom", default=DEFAULT_ROM)
    parser.add_argument("--ladxr")
    parser.add_argument("--spans")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if not os.path.isfile(args.rom):
        print("setup: known-source: no ROM at %s" % args.rom)
        return 2
    if args.ladxr and not os.path.isfile(os.path.join(args.ladxr, "romTables.py")):
        print("setup: known-source: no LADXR checkout in %s" % args.ladxr)
        return 2
    with open(args.rom, "rb") as f:
        rom = f.read()
    ks = build(rom)
    if args.spans:
        with open(args.spans, "w") as f:
            f.write(spans(ks))
    for label in ks.failures:
        print("NOT-REBUILT " + label)
    mismatches = check_ladxr(args.rom, args.ladxr, rom) if args.ladxr else []
    for line in mismatches:
        print(line)
    print(summary(ks) + (", LADXR parse and RoomEditor round trip %s" % (
        "checked" if not mismatches else "differ") if args.ladxr else ""))
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
