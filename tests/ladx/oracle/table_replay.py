"""Usage: python3 -I tests/ladx/oracle/table_replay.py [--root <dir>] [--mirror <dir>] [--rom <file>]
       [--tables <dir>] [--ladxr <dir>] [--inventory <file>] [--inputs <dir>] [<case>...]

The host replay of LADX's patch tables (tables.py), with the ROM: for each case folder
<root>/cases|combos/<case>/ (the named ones, default every one; root default tests/ladx), the
tables (--tables, default data/ladx/) walked in order.txt's order for the case's key, a unit's
entry applied at its first row and, at a `C` row, the golden's `C` segments whose first write that
row made (rom_writes.txt's segments matched one to one with the event stream gen_golden.sh left in
<mirror>/<case>/events.txt, default tmp/ladx-goldens/). The key: config.txt validated by LADXR's
Settings, the assembler constants from the inventory's expressions, the spot count from a
pool-only run (gen_tables.py's model; LADXR checkout --ladxr, fixtures in --inputs).
  sha1         the replayed log decoded onto the ROM (--rom, default tmp/ladx/base.gbc) gives
               run.txt's rom_sha1; else the first differing offset and its last writer in the
               replay and in the golden (a unit id, a C group or ROM);
  projection   each table group's projection (write_log.py project) equals the golden's;
  key          every unit has an entry for the case's key;
  align        the golden's segments match the stream's, and each C segment's row is a C row
               of its group in order.txt.
It proves itself first on planted tables, goldens and streams (`table-replay: self-test, <n>
planted replays as expected`, else `self-test failed: <case>`, exit 1), then prints `ok <case>` or
one `BAD <case>: <check>: <detail>` line per problem, then `table-replay: <n> cases, <b> bad`;
exit 1 on any BAD. A missing ROM, table, stream or LADXR checkout is a `setup:` line, exit 2.
Never prints a ROM byte.
"""

import argparse
import hashlib
import importlib.util
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))


def _load(name):
    spec = importlib.util.spec_from_file_location("table_replay_" + name,
                                                  os.path.join(HERE, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = no_bytecode
    return module


tables = _load("tables")
write_log = tables._sibling("write_log")


class SetupError(Exception):
    pass


def _stream_segments(events):
    """[(group, [event])] of the stream's maximal runs of one group's events."""
    out = []
    for event in events.events:
        if out and out[-1][0] == event[1]:
            out[-1][1].append(event)
        else:
            out.append((event[1], [event]))
    return out


def _writer(row):
    return row.unit if row.side == "table" else row.group


def _first_difference(a, b):
    n = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), None)
    return min(len(a), len(b)) if n is None else n


def replay(tabs, values, golden, events, rom, sha1):
    """[(check, detail)] of one case: tabs a tables.Tables, values {component: value} of the
    case's key, golden its rom_writes.txt, events its write_log.Events, rom the source ROM, sha1
    run.txt's rom_sha1. Empty when the replay gives the golden's output and projections."""
    try:
        steps = tables.select(tabs, values)
    except tables.TablesError as e:
        m = re.match(r"unit (\S+):", str(e))
        return [("key " + (m.group(1) if m else "?"), str(e))]
    golden_tables, golden_segments = write_log.parse(golden)
    stream = _stream_segments(events)
    if [g for g, _ in stream] != [g for g, _, _ in golden_segments]:
        return [("align", "the golden's %d segments are not the stream's %d"
                 % (len(golden_segments), len(stream)))]
    bad_tables = sorted(g for g, group in tabs.groups.items() if group.tables != golden_tables)
    if bad_tables:
        return [("align", "%s's tables are not the golden's" % ", ".join(bad_tables))]
    rows = {"%s\t%s" % (r.section, r.key): r for r in tabs.order.rows}
    owned, problems = {}, []
    for n, (group, seg) in enumerate(stream):
        row = rows.get(seg[0][2])
        if row is None or row.side != "C" or row.group != group:
            if tabs.groups.get(group) is None:
                problems.append(("align", "segment %d of %s: its row is not a C row of it in "
                                 "order.txt" % (n + 1, group)))
            continue
        owned.setdefault(id(row), []).append(golden_segments[n])
    if problems:
        return problems
    applied, writers = [], {}
    for row, entry in steps:
        if entry is None:
            parts = [(g, b, e, row.group) for g, b, e in owned.get(id(row), [])]
        else:
            parts = [(row.group, entry.banks, entry.entries, row.unit)] if entry.lines else []
        for group, banks, entries, label in parts:
            writers.update((p, label) for p in banks)
            if applied and applied[-1][0] == group:
                applied[-1][1].update(banks)
                applied[-1][2].update(entries)
            else:
                applied.append((group, dict(banks), dict(entries)))
    text = write_log.render(golden_tables, applied)
    out = write_log.decode(text, rom)
    got = hashlib.sha1(out).hexdigest()
    if got != sha1:
        expected = write_log.decode(golden, rom)
        p = _first_difference(out, expected)
        golden_writer = "ROM"
        for event in events.events:
            if event[0] == "B" and event[3] <= p < event[3] + len(event[4]):
                row = rows.get(event[2])
                golden_writer = _writer(row) if row else event[1]
        problems.append(("sha1", "replay %s, rom_sha1 %s, first difference at 0x%x (replay %s, "
                         "golden %s)" % (got, sha1, p, writers.get(p, "ROM"), golden_writer)))
    mine, theirs = write_log.projections(text), write_log.projections(golden)
    empty = write_log.render(golden_tables, [])
    for group in sorted(tabs.groups):
        a, b = mine.get(group, empty), theirs.get(group, empty)
        if a != b:
            a, b = a.splitlines(), b.splitlines()
            problems.append(("projection " + group, "line %d differs from the golden's"
                             % (_first_difference(a, b) + 1)))
    return problems


# The self-proof ----------------------------------------------------------------------------------

_SECTION = "generator.py generateRom"
_ORDER = ("ladx-tables-order 1\nsection %s\ntable fixed fixed.1 a(rom)\nC items - b(rom)\n"
          "table opts opts.1 c(rom)\nC items - d(rom)\n" % _SECTION)
_FIXED = "ladx-tables 1\ngroup fixed\ntables texts\nunit fixed.1\nE\nB 10 @20+4\n"
_OPTS = ("ladx-tables 1\ngroup opts\ntables texts\nunit opts.1 race\nE 0\nB 30 01\nT texts 2 05\n"
         "E 1\nnothing\n")
_GOLDEN = ("ladx-rom-writes 1\ntables texts\nG fixed\nB 10 @20+4\nG items\nB 11 aa\n"
           "G opts\nB 30 01\nT texts 2 05\nG items\nB 30 bb\n")


def _plant_stream(rom):
    def event(kind, group, call, where, value):
        site = "%s\t%s" % (_SECTION, call)
        fields = ([kind, group, "%x" % where] if kind == "B" else [kind, group, "texts",
                                                                   "%x" % where])
        return "\t".join(fields + [value.hex(), "k%x" % len(value), site])
    lines = ["ladx-events 1", "source " + hashlib.sha1(rom).hexdigest(), "tables texts",
             event("B", "fixed", "a(rom)", 0x10, rom[0x20:0x24]),
             event("B", "items", "b(rom)", 0x11, b"\xaa"),
             event("B", "opts", "c(rom)", 0x30, b"\x01"), event("T", "opts", "c(rom)", 2, b"\x05"),
             event("B", "items", "d(rom)", 0x30, b"\xbb")]
    return write_log.parse_events("".join(line + "\n" for line in lines))


def self_test():
    """Replays a planted case, correct then with a flipped table byte, a copy token swapped for
    the same bytes as a literal and an entry under another key; the first variant that does not
    give exactly its checks, or None."""
    rom = bytes((i * 7 + 3) % 256 for i in range(0x100))
    stream = _plant_stream(rom)
    sha1 = hashlib.sha1(write_log.decode(_GOLDEN, rom)).hexdigest()
    variants = [("correct", {}, []),
                ("flipped byte", {"opts": _OPTS.replace("B 30 01", "B 31 01")},
                 ["sha1", "projection opts"]),
                ("token swap", {"fixed": _FIXED.replace("@20+4", rom[0x20:0x24].hex())},
                 ["projection fixed"]),
                ("wrong key", {"opts": _OPTS.replace("E 0\n", "E 00\n")}, ["key opts.1"])]
    for name, change, want in variants:
        texts = dict({"fixed": _FIXED, "opts": _OPTS}, **change)
        tabs = tables.Tables(tables.parse(_ORDER), {g: tables.parse(t) for g, t in texts.items()})
        try:
            got = replay(tabs, {"race": "0"}, _GOLDEN, stream, rom, sha1)
        except (tables.TablesError, write_log.WriteLogError):
            return name
        if [check for check, _ in got] != want:
            return name
    return None


SELF_TEST_CASES = 4


# The command line --------------------------------------------------------------------------------

def _read(path, binary=False):
    with open(path, "rb") as f:
        data = f.read()
    return data if binary else data.decode("utf-8")


def load_tables(root):
    """The tables.Tables of the files under root; SetupError on a missing or broken file."""
    try:
        order = tables.parse(_read(os.path.join(root, tables.ORDER_FILE)))
        groups = {}
        for group in sorted({r.group for r in order.rows if r.side == "table"}):
            groups[group] = tables.parse(_read(os.path.join(root, group + ".txt")))
    except (OSError, UnicodeDecodeError, tables.TablesError) as e:
        raise SetupError("tables under %s: %s" % (root, e))
    return tables.Tables(order, groups)


def case_folders(root, names):
    folders = []
    for sub in ("cases", "combos"):
        try:
            entries = sorted(os.listdir(os.path.join(root, sub)))
        except FileNotFoundError:
            continue
        for name in entries:
            path = os.path.join(root, sub, name)
            if os.path.isfile(os.path.join(path, "rom_writes.txt")) and (not names
                                                                         or name in names):
                folders.append((name, path))
    missing = sorted(set(names) - {n for n, _ in folders})
    if missing:
        raise SetupError("no golden for %s under %s" % (" ".join(missing), root))
    return folders


def assignment_of(config_text):
    """{key: value rendered as in the tables} of a case's config.txt."""
    out = {}
    for line in config_text.splitlines():
        key, _, value = line.partition("=")
        out[key] = value or '""'
    return out


class KeyModel:
    """The case keys, through gen_tables.py's model on LADXR (validate and the spot count)."""

    def __init__(self, ladxr, inventory_path, inputs, work):
        self.gen_tables = _load("gen_tables")
        sys.path.insert(0, HERE)
        import oracle_run
        text = lambda name: _read(os.path.join(inputs, name))
        settings_text = text("settings.txt")
        keys = [s.key for s in self.gen_tables.ladx_cases.read_settings(settings_text)]
        inventory = self.gen_tables.read_inventory(_read(inventory_path), keys)
        self.driver = oracle_run.Driver(ladxr, work)
        import settings as ladxr_settings
        import locations.keyLocation as key_location
        gt = self.gen_tables
        self.model = gt.build_model(inventory, text("tier1.txt"), settings_text,
                                    text("combos.txt"), text("settings_validate.txt"),
                                    gt.settings_validate(ladxr_settings.Settings), None,
                                    inventory.group_keys[gt.COUNT_GROUP])
        self.model._count = gt.spot_count(self.driver, key_location.KeyLocation,
                                          self.model.defaults)

    def values(self, config_text, components):
        settings = self.model.validated(assignment_of(config_text))
        out = {}
        for c in components:
            value = self.model.value(c, settings)
            if value is not None:
                out[c] = value
        return out

    def close(self):
        self.driver.close()


def main(argv):
    parser = argparse.ArgumentParser(prog="table_replay.py")
    parser.add_argument("--root", default=os.path.join(TOP, "tests", "ladx"))
    parser.add_argument("--mirror", default=os.path.join(TOP, "tmp", "ladx-goldens"))
    parser.add_argument("--rom", default=os.path.join(TOP, "tmp", "ladx", "base.gbc"))
    parser.add_argument("--tables", default=os.path.join(TOP, "data", "ladx"))
    parser.add_argument("--ladxr", default=os.environ.get("LADXR_DIR")
                        or os.path.join(HERE, "ladxr"))
    parser.add_argument("--inventory", default=os.path.join(TOP, "notes", "ladx-inventory.md"))
    parser.add_argument("--inputs", default=HERE)
    parser.add_argument("cases", nargs="*")
    args = parser.parse_args(argv)
    failed = self_test()
    if failed:
        print("table-replay: self-test failed: %s" % failed)
        return 1
    print("table-replay: self-test, %d planted replays as expected" % SELF_TEST_CASES, flush=True)
    for name in ("root", "mirror", "rom", "tables", "ladxr", "inventory", "inputs"):
        setattr(args, name, os.path.abspath(getattr(args, name)))
    keys = None
    try:
        if not os.path.isfile(args.rom):
            raise SetupError("no ROM at %s (run tests/ladx/oracle/fetch_rom.sh)" % args.rom)
        if not os.path.isfile(os.path.join(args.ladxr, "generator.py")):
            raise SetupError("no LADXR checkout in %s, run tests/ladx/oracle/setup.sh"
                             % args.ladxr)
        rom = _read(args.rom, True)
        tabs = load_tables(args.tables)
        components = sorted({c for g in tabs.groups.values() for u in g.units
                             for c in u.components})
        folders = case_folders(args.root, args.cases)
        keys = KeyModel(args.ladxr, args.inventory, args.inputs,
                        os.path.join(TOP, "tmp", "ladx-oracle"))
        bad = 0
        for name, folder in folders:
            stream = os.path.join(args.mirror, name, "events.txt")
            if not os.path.isfile(stream):
                raise SetupError("no event stream at %s (run gen_golden.sh)" % stream)
            events = write_log.parse_events(_read(stream))
            if events.source != hashlib.sha1(rom).hexdigest():
                raise SetupError("%s was recorded on another ROM" % stream)
            m = re.search(r"^rom_sha1 ([0-9a-f]{40})$", _read(os.path.join(folder, "run.txt")),
                          re.M)
            values = keys.values(_read(os.path.join(folder, "config.txt")), components)
            problems = replay(tabs, values, _read(os.path.join(folder, "rom_writes.txt")),
                              events, rom, m.group(1) if m else "missing")
            for check, detail in problems:
                print("BAD %s: %s: %s" % (name, check, detail), flush=True)
            if not problems:
                print("ok %s" % name, flush=True)
            bad += 1 if problems else 0
    except (SetupError, write_log.WriteLogError, OSError) as e:
        print("setup: table-replay: %s" % e)
        return 2
    finally:
        if keys is not None:
            keys.close()
    print("table-replay: %d cases, %d bad" % (len(folders), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
