"""Usage: python3 -I tests/ladx/oracle/gen_encoder_vectors.py [--out <file>] [--check]

Writes encoder_vectors.txt (default tests/ladx/oracle/encoder_vectors.txt): write_log.py's
encoder run on synthetic event streams over a synthetic ROM drawn from a fixed-seed
random.Random, each with a synthetic KS (known_source.KnownSource), for C's encoder to replay.
No ROM, no LADXR, no network. --check renders it again in memory and compares with the file.
Contract in tests/ladx/README.md, "Encoder fixture"; the format is in HEADER below.
"""

import argparse
import hashlib
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import known_source  # noqa: E402
import read_trace  # noqa: E402
import write_log  # noqa: E402

TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
FIXTURE = os.path.join(HERE, "encoder_vectors.txt")
VERSION_LINE = "ladx-encoder-vectors 1"
TABLES = [table.name for table in known_source.LAYOUT]
ROM_SIZE = 0x1000
ROM_SEED = 0x1ADE0E1C
RANDOM_SEED = 0x1ADE0E1D
RANDOM_VECTORS = 200
ROW = 64
KNOWN = write_log.KNOWN
ALT_KEYS = ("Alt06", "Alt0E", "Alt1B")

HEADER = """\
# write_log.py's encoder on synthetic event streams; written by gen_encoder_vectors.py, never by
# hand. No byte of any real ROM: the ROM below is random.Random(0x%X).randbytes(0x%X) with
# synthetic regions copied inside it (duplicates for KS-match ties). Lines, hex lowercase:
#   ladx-encoder-vectors 1
#   tables <table>...             ROMWithTables' tables, the order of every stream and log
#   rom <size>                    then one `R <hex>` line per %d bytes of the synthetic ROM
#   V <name>                      a vector: hand-written ones by rule, then random-<nnn>
#   sides <group>=<table|C>...    each group's side, in first-write order
#   ks none | ks <n>              no KS (the encoder runs on an empty one), or n `K` lines:
#   K <start> <length> <label>    a KS span in known_source's span-list label form; a
#                                 `<table> <key>` label is its key's entry (a later one wins)
#   events <n>                    n lines of the `ladx-events 1` stream after its header (I, B, T
#                                 lines, tab-separated, site `-`), source the ROM above
#   log <n>                       n lines: write_log.encode's rom_writes.txt for the stream
"""


class FixtureError(Exception):
    pass


class Vector:
    def __init__(self, name, sides, ks, initial, events):
        self.name, self.sides, self.ks, self.initial, self.events = name, sides, ks, initial, events


def synthetic_rom():
    rom = bytearray(random.Random(ROM_SEED).randbytes(ROM_SIZE))
    rom[0xC00:0xC10] = rom[0x200:0x210]
    rom[0x240:0x246] = rom[0xE00:0xE06]
    return bytes(rom)


def known(rom, spans):
    ks = known_source.KnownSource(len(rom))
    for start, stop, label, entry in spans or []:
        ks.add(start, stop, label, entry)
    return ks


def _key_text(key):
    return read_trace._key_text(key)


def entry_label(table, key):
    return "%s %s" % (table, _key_text(key))


def span(start, stop, table="entities", key=0, kind="pointer"):
    if kind is None:
        return (start, stop, entry_label(table, key), (table, key))
    return (start, stop, "%s %s %s" % (table, kind, _key_text(key)), None)


def src(q, n):
    return tuple(range(q, q + n))


def ids(i, n):
    return tuple(-2 - j for j in range(i, i + n))


def bank(group, offset, value, origins):
    value = bytes(value)
    if len(origins) != len(value):
        raise FixtureError("origins out of step with the value at %x" % offset)
    return ("B", group, "-", offset, value, tuple(origins))


def entry(group, table, key, value, origins=None):
    if isinstance(value, int):
        return ("T", group, "-", table, key, value, None)
    value = bytes(value)
    return ("T", group, "-", table, key, value, tuple(origins if origins is not None else
                                                      (KNOWN,) * len(value)))


def absent(rom, low, high):
    return next(b for b in range(256) if b not in rom[low:high])


def hand_vectors(rom):
    k = KNOWN
    C2 = {"ca": "C", "cb": "C"}
    out = []

    def add(name, sides, ks, initial, events):
        out.append(Vector(name, sides, ks, initial, events))

    add("carried-source-copy", {"ca": "C"}, None, [],
        [bank("ca", 0x100, rom[0x100:0x101] + rom[0x300:0x306], (0x100,) + src(0x300, 6))])
    add("carried-identity", C2, [span(0x100, 0x104)], [],
        [bank("ca", 0x100, rom[0x100:0x101], ids(0, 1)),
         bank("cb", 0x500, rom[0x100:0x101], ids(0, 1)),
         entry("cb", "entities", 5, b"\x01" + rom[0x100:0x101], (k,) + ids(0, 1))])
    add("inherit-on-equal-bank", C2, [span(0x100, 0x110)], [],
        [bank("ca", 0x100, rom[0x100:0x104] + bytes([rom[0x104] ^ 0xFF]), (k,) * 5),
         bank("ca", 0x200, rom[0x200:0x202], (k, k)),
         bank("ca", 0x300, b"\x55", (k,)),
         bank("cb", 0x300, b"\x55", (k,)),
         bank("cb", 0x104, rom[0x104:0x105], (k,))])
    old = rom[0x400:0x410]
    add("table-prefix-suffix-middle", {"ca": "C"}, [span(0x400, 0x410, "entities", 3)],
        [("entities", 3, old, src(0x400, 16))],
        [entry("ca", "entities", 3, old[:3] + bytes([old[3] ^ 0xFF]) + old[6:11] + bytes([old[11] ^ 0xFF])
               + old[14:])])
    old = rom[0x500:0x508]
    add("table-unknowable-stop", {"ca": "C"}, [span(0x500, 0x502), span(0x506, 0x508, key=1)],
        [("entities", 4, old, src(0x500, 8))],
        [entry("ca", "entities", 4, old)])
    add("ks-match-run-boundary", {"ca": "C"}, [span(0x600, 0x608), span(0x609, 0x612, key=1)], [],
        [bank("ca", 0x700, rom[0x604:0x60C], (k,) * 8),
         bank("ca", 0x720, rom[0x60A:0x612], (k,) * 8)])
    add("ks-match-lowest-offset", {"ca": "C"}, [span(0xC00, 0xC10), span(0x200, 0x210, key=1)], [],
        [bank("ca", 0x800, rom[0x200:0x208], (k,) * 8)])
    add("ks-match-longest", {"ca": "C"}, [span(0x240, 0x248), span(0xE00, 0xE0C, key=1)], [],
        [bank("ca", 0x800, rom[0xE00:0xE0C], (k,) * 12)])
    x = absent(rom, 0x900, 0x910)
    add("fragment-table", {"ca": "C"}, [span(0x900, 0x910, "entities", 7, None)],
        [("entities", 7, rom[0x900:0x910], src(0x900, 16))],
        [entry("ca", "entities", 7, rom[0x905:0x907] + bytes([x]) + rom[0x90A:0x90B])])
    xa = absent(rom, 0xA00, 0xA20)
    value = bytes([rom[0xA02], rom[0xA03], xa, rom[0xA05], rom[0xA12], xa, rom[0xA18], rom[0xA19]])
    add("fragment-bank", {"ca": "C"},
        [span(0xA00, 0xA10, "rooms_indoor_a", 2, None), span(0xA08, 0xA20, "rooms_indoor_a", 2)], [],
        [bank("ca", 0xA0C, value, (k,) * 8)])
    add("rename-after-ks-match", C2, [span(0xB00, 0xB10)], [],
        [bank("ca", 0x780, rom[0xB00:0xB08], ids(10, 8)),
         bank("cb", 0x7A0, rom[0xB02:0xB06], ids(12, 4)),
         entry("cb", "entities", 9, rom[0xB00:0xB02] + b"\x00", ids(10, 2) + (k,))])
    y = absent(rom, 0x980, 0x990)
    add("rename-after-fragment", C2, [span(0x980, 0x990, "entities", 8, None)],
        [("entities", 8, rom[0x980:0x990], src(0x980, 16))],
        [entry("ca", "entities", 8, rom[0x986:0x988] + bytes([y]), ids(30, 3)),
         bank("cb", 0x7C0, rom[0x986:0x988], ids(30, 2))])
    add("keep-rewrite", C2, None, [],
        [bank("ca", 0x300, bytes([rom[0x300] ^ 0x5A]), (k,)),
         bank("ca", 0x310, bytes([rom[0x310] ^ 0x5A]), (k,)),
         bank("ca", 0x310, rom[0x310:0x311], src(0x310, 1)),
         bank("cb", 0x300, rom[0x300:0x301], src(0x300, 1))])
    add("integer-pointer", {"ca": "C"}, None, [("entities", 0xB, 0x1234, None)],
        [entry("ca", "entities", 0xA, 0x5678),
         entry("ca", "entities", 0xB, rom[0x40:0x44], (k,) * 4)])
    add("net-effect", {"ca": "C"}, None, [],
        [bank("ca", 0x100, b"\x01\x02\x03", (k,) * 3),
         bank("ca", 0x101, b"\x09", (k,)),
         entry("ca", "entities", 2, b"\x01", (k,)),
         entry("ca", "entities", 2, b"\x02\x03", (k, k))])
    add("table-passthrough", {"tg": "table", "ca": "C"}, [span(0x340, 0x348)], [],
        [bank("tg", 0x120, rom[0x340:0x348], ids(20, 8)),
         bank("tg", 0x130, b"\x00\x01\x02", ids(28, 3)),
         bank("ca", 0x140, rom[0x342:0x346] + b"\x02", ids(22, 4) + ids(30, 1)),
         entry("tg", "texts", 1, rom[0x344:0x348], ids(40, 4)),
         entry("ca", "texts", 2, rom[0x344:0x346], ids(40, 2))])
    return out


def random_vector(r, rom, n):
    groups = ["tg", "ca", "cb"]
    sides = {}
    spans = None
    if r.random() >= 0.3:
        spans, used = [], set()
        for i in range(r.randint(1, 6)):
            start = r.randrange(0x40, ROM_SIZE - 0x40)
            stop = start + r.randint(4, 48)
            table, key = TABLES[r.randrange(3)], r.choice([0, 1, 2, 3, 4, 5] + list(ALT_KEYS))
            if r.random() < 0.5 and (table, key) not in used:
                used.add((table, key))
                spans.append(span(start, stop, table, key, None))
            else:
                spans.append(span(start, stop, table, key))
    keys = [(TABLES[r.randrange(3)], r.choice([0, 1, 2, 3] + list(ALT_KEYS[:1]))) for _ in range(4)]
    keys = list(dict.fromkeys(keys))
    initial = []
    for table, key in keys:
        mode = r.random()
        if mode < 0.4:
            q, m = r.randrange(0, ROM_SIZE - 32), r.randint(1, 16)
            initial.append((table, key, rom[q:q + m], src(q, m)))
        elif mode < 0.6:
            value = r.randbytes(r.randint(0, 8))
            initial.append((table, key, value, (KNOWN,) * len(value)))
        elif mode < 0.7:
            initial.append((table, key, r.randrange(0x10000), None))
    if spans:
        for start, stop, _, entry_key in spans:
            if entry_key is not None and entry_key not in [(t, k) for t, k, _, _ in initial]:
                initial.append(entry_key + (rom[start:stop], src(start, stop - start)))
    written = []
    next_id = 0
    events = []
    for _ in range(r.randint(2, 10)):
        group = r.choice(groups)
        sides.setdefault(group, "table" if group == "tg" else "C")
        m = r.randint(1, 16)
        mode = r.random()
        if mode < 0.2:
            q = r.randrange(0, ROM_SIZE - m)
            value, origins = rom[q:q + m], src(q, m)
        elif mode < 0.4:
            q = r.choice([s[0] for s in spans]) if spans and r.random() < 0.7 else r.randrange(ROM_SIZE - m)
            q = min(q + r.randrange(4), ROM_SIZE - m)
            value, origins = rom[q:q + m], (KNOWN,) * m
        elif mode < 0.55 and written:
            value, origins = r.choice(written)
            cut = r.randrange(len(value))
            value, origins = value[cut:cut + m], origins[cut:cut + m]
        elif mode < 0.75:
            value, origins = r.randbytes(m), ids(next_id, m)
            next_id += m
        else:
            value, origins = r.randbytes(m), (KNOWN,) * m
        m = len(value)
        if r.random() < 0.6:
            if r.random() < 0.3 and spans:
                start = r.choice(spans)[0]
                offset = min(start + r.randrange(8), ROM_SIZE - m)
            else:
                offset = r.randrange(0, ROM_SIZE - m)
            if r.random() < 0.15:
                value, origins = rom[offset:offset + m], (KNOWN,) * m
            events.append(bank(group, offset, value, origins))
        else:
            table, key = r.choice(keys) if keys and r.random() < 0.8 else (TABLES[0], 7)
            if r.random() < 0.1:
                events.append(entry(group, table, key, r.randrange(0x10000)))
            else:
                if r.random() < 0.3:
                    previous = [v for t, k, v, _ in initial if (t, k) == (table, key) and not isinstance(v, int)]
                    if previous and len(previous[0]) >= 4:
                        old = bytearray(previous[0])
                        old[r.randrange(len(old))] ^= 0xFF
                        value, origins = bytes(old), (KNOWN,) * len(old)
                events.append(entry(group, table, key, value, origins))
        last = events[-1]
        if last[0] == "B":
            written.append((last[4], last[5]))
        elif last[6] is not None and last[5]:
            written.append((last[5], last[6]))
    return Vector("random-%03d" % n, sides, spans, initial, events)


def vectors(rom):
    r = random.Random(RANDOM_SEED)
    return hand_vectors(rom) + [random_vector(r, rom, n) for n in range(RANDOM_VECTORS)]


def stream_text(rom, tables, initial, events):
    lines = [write_log.EVENTS_LINE, "source " + hashlib.sha1(rom).hexdigest(), " ".join(["tables"] + tables)]
    return "".join(line + "\n" for line in lines + event_lines(initial, events))


def event_lines(initial, events):
    out = []
    for table, key, value, origins in initial:
        out.append("\t".join(["I", table, _key_text(key), read_trace._value_text(value),
                              read_trace.origin_text(origins or ())]))
    for event in events:
        if event[0] == "B":
            _, group, site, offset, value, origins = event
            fields = ["B", group, "%x" % offset, read_trace._value_text(value), read_trace.origin_text(origins)]
        else:
            _, group, site, table, key, value, origins = event
            fields = ["T", group, table, _key_text(key), read_trace._value_text(value),
                      read_trace.origin_text(origins or ())]
        out.append("\t".join(fields + [site]))
    return out


def encode(rom, vector):
    events = write_log.parse_events(stream_text(rom, TABLES, vector.initial, vector.events))
    return write_log.encode(events, rom, vector.sides, known(rom, vector.ks))


def render(rom, vs):
    lines = [VERSION_LINE, " ".join(["tables"] + TABLES), "rom %x" % len(rom)]
    lines += ["R " + rom[i:i + ROW].hex() for i in range(0, len(rom), ROW)]
    for v in vs:
        lines.append("V " + v.name)
        lines.append(" ".join(["sides"] + ["%s=%s" % kv for kv in v.sides.items()]))
        if v.ks is None:
            lines.append("ks none")
        else:
            lines.append("ks %x" % len(v.ks))
            lines += ["K %x %x %s" % (start, stop - start, label) for start, stop, label, _ in v.ks]
        events = event_lines(v.initial, v.events)
        lines.append("events %x" % len(events))
        lines += events
        log = encode(rom, v).splitlines()
        lines.append("log %x" % len(log))
        lines += log
    return HEADER % (ROM_SEED, ROM_SIZE, ROW) + "".join(line + "\n" for line in lines)


def _entry_of(label):
    fields = label.split(" ")
    if len(fields) == 2 and fields[0] in TABLES:
        return (fields[0], write_log._key(fields[1]))
    return None


def parse(text):
    lines = [line for line in text.split("\n")[:-1] if not line.startswith("#")]
    pos = 0

    def take(prefix):
        nonlocal pos
        if pos >= len(lines) or not lines[pos].startswith(prefix):
            raise FixtureError("expected %r at %r" % (prefix, lines[pos] if pos < len(lines) else "<end>"))
        pos += 1
        return lines[pos - 1][len(prefix):]

    def count(prefix):
        n = int(take(prefix), 16)
        if pos + n > len(lines) and prefix in ("events ", "log "):
            raise FixtureError("%s%x runs past the end" % (prefix, n))
        return n

    take(VERSION_LINE)
    if take("tables ").split(" ") != TABLES:
        raise FixtureError("tables line differs from ROMWithTables' order")
    size = count("rom ")
    rom = b"".join(bytes.fromhex(take("R ")) for _ in range((size + ROW - 1) // ROW))
    out = []
    while pos < len(lines):
        name = take("V ")
        sides = dict(field.split("=") for field in take("sides").split(" ")[1:])
        ks_text = take("ks ")
        spans = None
        if ks_text != "none":
            spans = []
            for _ in range(int(ks_text, 16)):
                start, length, label = take("K ").split(" ", 2)
                start, length = int(start, 16), int(length, 16)
                spans.append((start, start + length, label, _entry_of(label)))
        n = count("events ")
        stream = write_log.parse_events(stream_text(rom, TABLES, [], []) + "".join(
            line + "\n" for line in lines[pos:pos + n]))
        pos += n
        n = count("log ")
        log = "".join(line + "\n" for line in lines[pos:pos + n])
        pos += n
        initial = [(t, k, v, o if not isinstance(v, int) else None) for t, k, v, o in stream.initial]
        out.append((Vector(name, sides, spans, initial, stream.events), log))
    return rom, out


def compare(committed, rendered):
    a, b = committed.split("\n"), rendered.split("\n")
    out = []
    for n in range(max(len(a), len(b))):
        x = a[n] if n < len(a) else "<end>"
        y = b[n] if n < len(b) else "<end>"
        if x != y:
            out.append("DIFFERENT line %d" % (n + 1))
    return out


def main(argv):
    parser = argparse.ArgumentParser(prog="gen_encoder_vectors.py")
    parser.add_argument("--out", default=FIXTURE)
    parser.add_argument("--check", action="store_true")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    rom = synthetic_rom()
    vs = vectors(rom)
    text = render(rom, vs)
    shown = os.path.relpath(args.out, TOP)
    if args.check:
        try:
            with open(args.out, encoding="ascii") as f:
                committed = f.read()
        except OSError:
            print("MISSING %s" % shown)
            return 1
        lines = compare(committed, text)
        for line in lines[:20]:
            print(line)
        if lines:
            print("encoder-vectors: %d lines differ from %s" % (len(lines), shown))
            return 1
        print("encoder-vectors: %d vectors byte-identical to %s" % (len(vs), shown))
        return 0
    with open(args.out, "w", encoding="ascii", newline="\n") as f:
        f.write(text)
    print("encoder-vectors: %d vectors written to %s" % (len(vs), shown))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
