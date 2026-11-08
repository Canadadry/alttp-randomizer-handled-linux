"""Usage: python3 tests/ladx/oracle/gen_store_vectors.py [--ladxr <dir>] [--out <file>] [--check]

Writes store_vectors.txt (default tests/ladx/oracle/store_vectors.txt): LADXR's own PointerTable
(pointerTable.py at the pin) parsing synthetic tables from synthetic banks and storing them, for
C's store (ladx_store) to replay. No ROM, no network; the LADXR checkout at the pin. --check
renders it again in memory and compares with the file. Contract in tests/ladx/README.md, "Store
fixture"; the format is in HEADER below.
"""

import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gen_fill_outcomes  # noqa: E402

TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
FIXTURE = os.path.join(HERE, "store_vectors.txt")
VERSION_LINE = "ladx-store-vectors 1"
BANK = 0x4000
BANKS = 0x40
RANDOM_SEED = 0x1ADE5A7E
RANDOM_VECTORS = 60

HEADER = """\
# LADXR's PointerTable (pointerTable.py at the pin) on synthetic tables; written by
# gen_store_vectors.py, never by hand. No byte of any real ROM: every bank is zeros with synthetic
# entries drawn from random.Random. Lines, hex lowercase, offsets in-bank:
#   ladx-store-vectors 1
#   V <name>                     a vector: hand-written ones by rule, then random-<nn>
#   info <count> <pointers bank> <pointers addr> <data bank|-1> <data addr|-> <data size|0>
#        <claim gaps 0|1> <expand 0|1>  (one line)
#   alt <bank> <addr>            one per alt pointer, in dict order
#   S <bank> <start> <end>       each _addStorage of the parse, in order
#   R <bank> <start> <end>       the parse's storage regions, in order
#   add <bank> <start> <end>     addStorage's regions, in order (sprite chains)
#   adjust <data addr>           adjustDataStart before the store
#   E <bank> <hex>|E int <value> every entry's value at store time: alt keys, then index keys
#   W <bank> <addr> <hex>        every assignment store makes to a bank, in order
#   L <bank> <start> <end>       the storage store returns (after an ok store)
#   raise <type> <message>       the exception store raised, after its writes
#   end                          the vector's last line
"""


class FixtureError(Exception):
    pass


class RecBank(bytearray):
    """A bank recording every assignment while its log is set."""
    log = None
    number = 0

    def __setitem__(self, key, value):
        super().__setitem__(key, value)
        if RecBank.log is not None:
            if isinstance(key, slice):
                RecBank.log.append((self.number, key.start, bytes(value)))
            else:
                RecBank.log.append((self.number, key, bytes([value])))


class FakeRom:
    def __init__(self):
        self.banks = []
        for n in range(BANKS):
            bank = RecBank(BANK)
            bank.number = n
            self.banks.append(bank)


class Spec:
    """A synthetic table: info (LADXR's dict), end (END_OF_DATA), writes [(bank, addr, bytes)]
    planted into zero banks, changes {key: bytes or int}, extra [(bank, start, end)] or None,
    adjust a data address or None, first: an optional Spec stored before, whose leftover is extra
    and whose leftover[0] start is adjust (sprite chains)."""

    def __init__(self, name, info, writes, changes=None, end=(0xFF,), extra=None, adjust=None,
                 first=None):
        self.name, self.info, self.writes, self.end = name, info, writes, end
        self.changes = changes or {}
        self.extra, self.adjust, self.first = extra, adjust, first


def _table_class(pointer_table, end):
    class Table(pointer_table):
        END_OF_DATA = end

        def __init__(self, rom, info):
            self.spans = []
            self._parsing = True
            super().__init__(rom, info)
            self._parsing = False

        def _addStorage(self, bank, start, stop):
            if self._parsing:
                self.spans.append((bank, start, stop))
            super()._addStorage(bank, start, stop)
    return Table


def run_spec(pointer_table, spec):
    """The vector's lines (without V), LADXR's own parse and store on spec's banks."""
    rom = FakeRom()
    for bank, addr, data in spec.writes:
        rom.banks[bank][addr:addr + len(data)] = data
    info = dict(spec.info)
    table = _table_class(pointer_table, spec.end)(rom, info)
    extra, adjust = spec.extra, spec.adjust
    if spec.first is not None:
        first = _table_class(pointer_table, spec.first.end)(rom, dict(spec.first.info))
        for key, value in spec.first.changes.items():
            first[key] = value
        left = first.store(rom)
        extra = [(st["bank"], st["start"], st["end"]) for st in left]
        adjust = left[0]["start"]
    lines = [info_line(spec.info)]
    for name, (bank, addr) in spec.info.get("alt_pointers", {}).items():
        lines.append("alt %x %x" % (bank, addr))
    lines += ["S %x %x %x" % span for span in table.spans]
    storage = table._PointerTable__storage
    lines += ["R %x %x %x" % (st["bank"], st["start"], st["end"]) for st in storage]
    for key, value in spec.changes.items():
        table[key] = value
    if extra is not None:
        table.addStorage([{"bank": b, "start": s, "end": e} for b, s, e in extra])
        lines += ["add %x %x %x" % region for region in extra]
    if adjust is not None:
        table.adjustDataStart(adjust)
        lines.append("adjust %x" % adjust)
    banks = table._PointerTable__banks
    for key in spec.info.get("alt_pointers", {}):
        lines.append(entry_line(spec.info["data_bank"], table[key]))
    for n in range(len(table)):
        lines.append(entry_line(banks[n], table[n]))
    log = []
    RecBank.log = log
    try:
        left = table.store(rom)
        raised = None
    except (AssertionError, ValueError) as e:
        raised = e
    finally:
        RecBank.log = None
    lines += ["W %x %x %s" % (bank, addr, data.hex()) for bank, addr, data in log]
    if raised is None:
        lines += ["L %x %x %x" % (st["bank"], st["start"], st["end"]) for st in left]
    else:
        message = str(raised)
        if "\n" in message:
            raise FixtureError("%s: a multi-line message" % spec.name)
        lines.append(("raise %s %s" % (type(raised).__name__, message)).rstrip(" "))
    return lines + ["end"]


def info_line(info):
    return "info %x %x %x %s %s %x %d %d" % (
        info["count"], info["pointers_bank"], info["pointers_addr"],
        "%x" % info["data_bank"] if "data_bank" in info else "-1",
        "%x" % info["data_addr"] if "data_addr" in info else "-",
        info.get("data_size", 0), int(bool(info.get("claim_storage_gaps"))),
        int(bool(info.get("expand_to_end_of_bank"))))


def entry_line(bank, value):
    if isinstance(value, int):
        return "E int %x" % value
    return "E %x %s" % (bank, bytes(value).hex())


def le16(v):
    return bytes([v & 0xFF, v >> 8])


def var_info(count, pointers_bank, pointers_addr, data_bank, **extra):
    info = {"count": count, "pointers_bank": pointers_bank, "pointers_addr": pointers_addr,
            "data_bank": data_bank}
    info.update(extra)
    return info


def var_table(name, data_bank, entries, *, pointers_bank=0x01, pointers_addr=0x100, at=None,
              changes=None, alts=(), **extra):
    """A variable-size table of data_bank: entries [(addr, bytes)] planted at addr (bytes None:
    an integer pointer addr); alts [(addr, bytes, (ptr bank, ptr addr))] parsed first."""
    writes, pointers = [], b""
    for addr, data in entries:
        if data is None:
            pointers += le16(addr)
            continue
        pointers += le16(addr | 0x4000)
        writes.append((data_bank, addr, data))
    writes.append((pointers_bank, pointers_addr, pointers))
    info = var_info(len(entries), pointers_bank, pointers_addr, data_bank, **extra)
    if alts:
        info["alt_pointers"] = {}
        for i, (addr, data, (pb, pa)) in enumerate(alts):
            writes.append((data_bank, addr, data))
            writes.append((pb, pa, le16(addr | 0x4000)))
            info["alt_pointers"]["Alt%d" % i] = (pb, pa)
    return Spec(name, info, writes, changes)


def sprite_table(name, values, *, data_addr=0x2000, pointers_addr=0x1000, changes=None, extra=None,
                 adjust=None, first=None, gaps=True):
    """A sprite data table of bank 0x20: values [(index, bytes4)]: pointer n is index (data at
    data_addr + 4 * index)."""
    writes = [(0x20, pointers_addr, bytes(i for i, _ in values))]
    for i, data in values:
        writes.append((0x20, data_addr + 4 * i, data))
    info = {"count": len(values), "pointers_bank": 0x20, "pointers_addr": pointers_addr,
            "data_bank": 0x20, "data_addr": data_addr, "data_size": 4}
    if gaps:
        info["claim_storage_gaps"] = True
    return Spec(name, info, writes, changes, extra=extra, adjust=adjust, first=first)


def text_table(name, entries, *, changes=None):
    """A texts-style table (END_OF_DATA 0xFE, 0xFF; a banks table at bank 0x01 0x200): entries
    [(bank byte, addr, bytes)]."""
    writes, pointers, banks = [], b"", b""
    for bank, addr, data in entries:
        pointers += le16(addr | 0x4000)
        banks += bytes([bank])
        writes.append((bank & 0x3F, addr, data))
    writes += [(0x01, 0x100, pointers), (0x01, 0x200, banks)]
    info = {"count": len(entries), "pointers_bank": 0x01, "pointers_addr": 0x100,
            "banks_bank": 0x01, "banks_addr": 0x200}
    return Spec(name, info, writes, changes, end=(0xFE, 0xFF))


def hand_specs():
    a, b, c, d = b"\x11\x12\xff", b"\x21\x22\x23\xff", b"\x31\xff", b"\x41\x42\xff"
    chain_first = sprite_table("chain-first", [(0, b"\x01\x02\x03\x04"), (1, b"\x05\x06\x07\x08")],
                               data_addr=0x0800, pointers_addr=0x0700,
                               changes={0: b"\x01\x02\x03\x04", 1: b"\x01\x02\x03\x04"})
    return [
        # 0x103 to 0x104 is a one-byte gap: merged; 0x109 to 0x10B two bytes: not.
        var_table("merge-one-byte-gap", 0x02, [(0x100, a), (0x104, b), (0x10B, c)]),
        # equal starts in banks 0x02 and 0x03: the sort keeps parse order.
        text_table("sort-stable-banks", [(0x03, 0x300, b"\x01\xfe"), (0x02, 0x300, b"\x02\xff"),
                                         (0x02, 0x200, b"\x03\xfe"), (0x43, 0x100, b"\x04\xff")]),
        # two pointers to one entry, one into its middle: _addStorage's contained case.
        var_table("contained-span", 0x02, [(0x100, b"\x01\x02\x03\x04\xff"), (0x100, None),
                                           (0x102, None), (0x200, c)]),
        sprite_table("claim-gaps", [(0, b"\x01\x02\x03\x04"), (5, b"\x05\x06\x07\x08"),
                                    (2, b"\x09\x0a\x0b\x0c")],
                     changes={1: b"\x01\x02\x03\x04"}),
        var_table("expand", 0x02, [(0x100, a), (0x300, b), (0x200, c)], expand_to_end_of_bank=True,
                  changes={0: b"\x11" * 0x30 + b"\xff"}),
        var_table("alt-first", 0x02, [(0x100, a), (0x110, b)],
                  alts=[(0x120, d, (0x00, 0x3000)), (0x130, b, (0x00, 0x3002))],
                  changes={1: d}),
        # entry 3's bytes are a tail of entries 0 and 2: the newest registration wins.
        var_table("suffix-newest", 0x02, [(0x100, a), (0x110, b), (0x120, c), (0x130, d)],
                  changes={0: b"\x01\x31\xff", 2: b"\x02\x31\xff", 3: b"\x31\xff",
                           1: b"\x01\x31\xff"}),
        # a two-byte value equal to another entry's tail is stored anew in sprite data.
        sprite_table("sprite-whole-only", [(0, b"\x01\x02\x03\x04"), (1, b"\x05\x06\x07\x08"),
                                           (2, b"\x05\x06\x07\x08")],
                     changes={1: b"\x03\x04", 2: b"\x01\x02\x03\x04"}),
        var_table("integer-pointer", 0x02, [(0x100, a), (0x1234, None), (0x8000, None),
                                            (0x110, b)],
                  changes={0: 0x0042, 3: 0xC123}),
        var_table("empty-entry", 0x02, [(0x100, a), (0x110, b), (0x120, c)],
                  changes={0: b"", 1: b"\x99\xff", 2: b""}),
        var_table("no-room-index", 0x02, [(0x100, a), (0x110, b)],
                  changes={1: b"\x55" * 0x20 + b"\xff"}),
        var_table("no-room-alt", 0x02, [(0x100, a)], alts=[(0x110, b, (0x00, 0x3000))],
                  changes={"Alt0": b"\x66" * 0x40 + b"\xff"}),
        # the pointer table lies in its own storage: pointer 0 overwrites entry 0's head, so
        # entry 2's de-duplication reads back other bytes.
        Spec("readback", var_info(3, 0x02, 0x100, 0x02),
             [(0x02, 0x100, le16(0x4100) + le16(0x4110) + le16(0x4120) + b"\x01\x02\x03\x04\xff"),
              (0x02, 0x110, b"\xaa\xbb\xff"), (0x02, 0x120, b"\xcc\xff")],
             {0: b"\x11\x22\x33\xff", 1: b"\x33\xff", 2: b"\x22\x33\xff"}),
        sprite_table("misaligned", [(0, b"\x01\x02\x03\x04"), (1, b"\x05\x06\x07\x08")],
                     adjust=0x2001),
        sprite_table("byte-range", [(0, b"\x01\x02\x03\x04"), (1, b"\x05\x06\x07\x08")],
                     adjust=0x2000 - 4 * 256),
        sprite_table("byte-range-negative", [(0, b"\x01\x02\x03\x04")], adjust=0x2004),
        sprite_table("add-adjust", [(0, b"\x11\x12\x13\x14"), (1, b"\x15\x16\x17\x18"),
                                    (2, b"\x19\x1a\x1b\x1c")],
                     data_addr=0x0810, pointers_addr=0x0780, first=chain_first,
                     changes={2: b"\x01\x02\x03\x04"}),
    ]


def random_spec(r, n):
    kind = r.choice(["var", "var", "text", "sprite"])
    name = "random-%02d" % n
    if kind == "sprite":
        count = r.randint(1, 24)
        indexes = [r.randrange(32) for _ in range(count)]
        values = {}
        for i in indexes:
            values.setdefault(i, bytes(r.randrange(256) for _ in range(4)))
        table = [(i, values[i]) for i in indexes]
        changes = {}
        for k in range(count):
            if r.random() < 0.4:
                changes[k] = r.choice(list(values.values())) if r.random() < 0.5 else bytes(
                    r.randrange(4) for _ in range(4))
        return sprite_table(name, table, changes=changes, gaps=r.random() < 0.8)
    terminators = (0xFE, 0xFF) if kind == "text" else (0xFF,)
    body_values = [v for v in range(256) if v not in terminators]
    count = r.randint(1, 30)
    banks = [0x02, 0x03, 0x04] if kind == "text" else [0x02]
    cursor = {b: 0x100 + r.randrange(0x40) for b in banks}
    pool, entries = [], []

    def body(length):
        return bytes(r.choice(body_values[:6] if r.random() < 0.5 else body_values)
                     for _ in range(length))

    for k in range(count):
        bank = r.choice(banks)
        choice = r.random()
        if pool and choice < 0.15:
            entries.append(r.choice(pool))
            continue
        if kind != "text" and choice < 0.22:
            entries.append((bank, r.choice([0x0000, 0x0012, 0x8000, 0xC0DE]), None))
            continue
        data = body(r.randint(0, 6)) + bytes([r.choice(terminators)])
        addr = cursor[bank]
        cursor[bank] += len(data) + r.choice([0, 0, 0, 1, 2, 5])
        entries.append((bank, addr, data))
        pool.append((bank, addr, data))
    values = [data for _, _, data in pool]
    changes = {}
    for k in range(count):
        roll = r.random()
        if roll < 0.15 and values:
            v = r.choice(values)
            changes[k] = v[r.randrange(len(v)):]
        elif roll < 0.3:
            changes[k] = body(r.randint(0, 2)) + bytes([r.choice(terminators)])
        elif roll < 0.33 and kind != "text":
            changes[k] = r.choice([0x0001, 0x9ABC])
    if kind == "text":
        rows = []
        for bank, addr, data in entries:
            rows.append((bank | (0x40 if r.random() < 0.2 else 0), addr, data))
        return text_table(name, rows, changes=changes)
    spec_entries = [(addr, data) for _, addr, data in entries]
    alts = []
    if r.random() < 0.4:
        for i in range(r.randint(1, 3)):
            data = body(r.randint(0, 4)) + b"\xff"
            addr = cursor[0x02]
            cursor[0x02] += len(data) + r.choice([0, 1, 3])
            alts.append((addr, data, (0x00, 0x3000 + 2 * i)))
            if r.random() < 0.5:
                changes["Alt%d" % i] = body(r.randint(0, 2)) + b"\xff"
    extra = {}
    if r.random() < 0.3:
        extra["expand_to_end_of_bank"] = True
    return var_table(name, 0x02, spec_entries, alts=alts, changes=changes, **extra)


def specs():
    r = random.Random(RANDOM_SEED)
    return hand_specs() + [random_spec(r, n) for n in range(RANDOM_VECTORS)]


def render(pointer_table):
    out = [HEADER, VERSION_LINE + "\n"]
    for spec in specs():
        out.append("V %s\n" % spec.name)
        out += [line + "\n" for line in run_spec(pointer_table, spec)]
    return "".join(out)


def parse(text):
    """[(name, [line])] of a fixture's vectors; FixtureError on a malformed text."""
    lines = [line for line in text.split("\n") if line and not line.startswith("#")]
    if not lines or lines[0] != VERSION_LINE:
        raise FixtureError("no version line")
    out, current = [], None
    for line in lines[1:]:
        if line.startswith("V "):
            if current is not None:
                raise FixtureError("vector %s has no end" % current[0])
            current = (line[2:], [])
        elif current is None:
            raise FixtureError("a line outside a vector: %s" % line)
        elif line == "end":
            out.append(current)
            current = None
        else:
            current[1].append(line)
    if current is not None:
        raise FixtureError("vector %s has no end" % current[0])
    return out


def compare(committed, rendered):
    a, b = committed.split("\n"), rendered.split("\n")
    out = []
    for i in range(max(len(a), len(b))):
        if (a[i] if i < len(a) else None) != (b[i] if i < len(b) else None):
            out.append("DIFFERENT line %d" % (i + 1))
    return out


def load_pointer_table(ladxr):
    sys.path.insert(0, ladxr)
    import pointerTable
    return pointerTable.PointerTable


def main(argv):
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
        gen_fill_outcomes.check_setup(ladxr)
    except gen_fill_outcomes.SetupError as e:
        print("setup: store-vectors: %s" % e, file=sys.stderr)
        return 2
    text = render(load_pointer_table(ladxr))
    out = os.path.abspath(options["--out"])
    shown = os.path.relpath(out, TOP)
    count = len(parse(text))
    if not check:
        with open(out, "w", encoding="ascii", newline="\n") as f:
            f.write(text)
        print("store-vectors: %d vectors written to %s" % (count, shown))
        return 0
    if not os.path.isfile(out):
        print("MISSING %s" % shown)
        return 1
    with open(out, encoding="ascii") as f:
        committed = f.read()
    lines = compare(committed, text)
    for line in lines[:20]:
        print(line)
    if lines:
        print("store-vectors: %d lines differ from %s" % (len(lines), shown))
        return 1
    print("store-vectors: %d vectors byte-identical to %s" % (count, shown))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
