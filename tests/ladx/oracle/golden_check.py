"""Usage: python3 -I tests/ladx/oracle/golden_check.py [--root <dir>] [--mirror <dir>] [--rom <file>]
       [--ladxr <dir>] [--inventory <file>] [<case>...]

The goldens' verifier, with the ROM: checks each case folder <root>/cases|combos/<case>/ holding a
rom_writes.txt (the named ones, default every one; root default tests/ladx) against the event
stream gen_golden.sh left in <mirror>/<case>/events.txt (default tmp/ladx-goldens/):
  decode       the log replayed onto the ROM (--rom, default tmp/ladx/base.gbc) gives run.txt's
               rom_sha1;
  encode       rom_writes.txt is exactly the stream's encoding (write_log.py encode), its `I`
               lines from the oracle's re-run record <mirror>/<case>/invariant.txt;
  tags         every segment's group is a group of the inventory (--inventory, default
               notes/ladx-inventory.md) with side table or C;
  scrambled    re-encoded with every byte outside the C-knowable map scrambled, no C group's
               projection changes;
  projection   for each group h, dropping h's segments or changing one of h's records leaves
               every other group's projection unchanged, and the change shows in h's own;
  nintendo     the Nintendo-bytes check (nintendo_bytes.py's policy for a case folder:
               rom_writes.txt in scope writes, run.txt and spoiler.json general, its computed
               and invariant clearances included) finds 0 literal runs; never prints a byte;
  fragments    no literal piece of a B or T record, of any length, occurs in the source bytes it
               replaces (write_log.py fragments: a T record's KS entry, a B byte's KS span):
               `<group> <n> fragments, <b> bytes, first at line <l>` per group holding one.
It prints `ok <case>` or one `BAD <case>: <check>: <detail>` line per problem, then, when no case
is named, `BAD tags: <detail>` unless the union of the goldens' groups is the inventory's table
and C groups less spoiler, then `golden-check: <n> cases, <b> bad, <k> literal runs, <f>
fragments` (<f> over every case checked); exit 1 on any BAD. A missing ROM, inventory, stream or LADXR checkout is a `setup:` line, exit 2.
"""

import argparse
import hashlib
import importlib.util
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
SIDES = ("table", "C")
NO_ROM_WRITES = ("spoiler",)


def _sibling(name):
    spec = importlib.util.spec_from_file_location("golden_check_" + name,
                                                  os.path.join(HERE, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = no_bytecode
    return module


write_log = _sibling("write_log")


class SetupError(Exception):
    pass


def _read(path, binary=False):
    with open(path, "rb") as f:
        data = f.read()
    return data if binary else data.decode("utf-8")


def _independence(text):
    """Problems of the projections' independence from other groups' records."""
    tables, segments = write_log.parse(text)
    base = write_log.projections(text)
    problems = []
    for h in base:
        dropped, changed = [], []
        last = max(n for n, segment in enumerate(segments) if segment[0] == h)
        for n, (name, banks, entries) in enumerate(segments):
            if name != h:
                if dropped and dropped[-1][0] == name:
                    dropped[-1][1].update(banks)
                    dropped[-1][2].update(entries)
                else:
                    dropped.append((name, dict(banks), dict(entries)))
            if n == last:
                banks, entries = dict(banks), dict(entries)
                if banks:
                    p = min(banks)
                    banks[p] = write_log._lit(1 if banks[p] == write_log._lit(0) else 0)
                else:
                    key = min(entries, key=write_log._key_order(tables))
                    entries[key] = [write_log._lit(1 if entries[key] == [write_log._lit(0)]
                                                   else 0)]
            changed.append((name, banks, entries))
        for what, variant in (("dropping", dropped), ("changing", changed)):
            got = write_log.projections(write_log.render(tables, variant))
            moved = sorted(g for g in base if g != h and got.get(g) != base[g])
            if moved:
                problems.append("%s %s's records changes %s" % (what, h, ", ".join(moved)))
            if what == "changing" and got.get(h) == base[h]:
                problems.append("changing %s's records does not show in its projection" % h)
    return problems


def check_case(folder, events, rom, sides, known, nintendo, scramble_known=None,
               invariant=None):
    """[(check, detail)] of one case folder against its event stream (write_log.Events) on the
    source rom; sides {group: side}; known the C-knowable map; nintendo(name, data, scope) ->
    [literal run description]. scramble_known (default known): the map the scramble keeps.
    invariant: the oracle's re-run flags (write_log.parse_invariant), which the `I` lines must
    match."""
    problems = []
    text = _read(os.path.join(folder, "rom_writes.txt"))
    run = _read(os.path.join(folder, "run.txt"))
    try:
        groups = write_log.groups(text)
    except write_log.WriteLogError as e:
        return [("format", str(e))]
    m = re.search(r"^rom_sha1 ([0-9a-f]{40})$", run, re.M)
    got = hashlib.sha1(write_log.decode(text, rom)).hexdigest()
    if m is None or got != m.group(1):
        problems.append(("decode", "SHA-1 %s, run.txt rom_sha1 %s"
                         % (got, m.group(1) if m else "missing")))
    for group in groups:
        if sides.get(group) not in SIDES:
            problems.append(("tags", "%s is not a table or C group of the inventory" % group))
    try:
        encoded, scrambled = write_log.encode_scrambled(events, rom, sides, known,
                                                        scramble_known, invariant)
        if encoded != text:
            a, b = encoded.splitlines(), text.splitlines()
            line = next(n for n in range(max(len(a), len(b)))
                        if n >= len(a) or n >= len(b) or a[n] != b[n])
            problems.append(("encode", "rom_writes.txt is not the stream's encoding, from line "
                             "%d" % (line + 1)))
        problems += [("scrambled", line) for line in scrambled]
    except write_log.WriteLogError as e:
        problems.append(("encode", str(e)))
    problems += [("projection", p) for p in _independence(text)]
    for name, scope in (("rom_writes.txt", "writes"), ("run.txt", "general"),
                        ("spoiler.json", "general")):
        path = os.path.join(folder, name)
        if os.path.exists(path):
            problems += [("nintendo", "%s: %s" % (name, run))
                         for run in nintendo(name, _read(path, True), scope)]
    problems += [("fragments", detail) for detail in fragment_details(text, rom, known)]
    return problems


def fragment_details(text, rom, known):
    """`<group> <n> fragments, <b> bytes, first at line <l>` per group of the log holding a
    fragment (write_log.fragments), in first-fragment order."""
    found = {}
    for group, line, _, n in write_log.fragments(text, rom, known):
        count = found.setdefault(group, [0, 0, line])
        count[0] += 1
        count[1] += n
    return ["%s %d fragments, %d bytes, first at line %d" % (group, n, b, line)
            for group, (n, b, line) in found.items()]


def union_problems(found, sides):
    """[detail] of the union of the goldens' groups against the inventory's writing groups."""
    want = {g for g, side in sides.items() if side in SIDES} - set(NO_ROM_WRITES)
    return (["no golden writes %s" % g for g in sorted(want - found)]
            + ["%s writes but is not a writing group" % g for g in sorted(found - want)])


def _case_folders(root, names):
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


def _nintendo(args, rom):
    nb = _sibling("nintendo_bytes")
    try:
        corpus = nb.load_ladxr(args.ladxr)
    except nb.SetupError as e:
        raise SetupError(str(e))
    index = nb.RomIndex(rom)
    reads = nb.ReadSets(lambda: nb.read_inventory(args.inventory), rom)

    def check(name, data, scope):
        try:
            literals, _ = nb.check_blob(name, data, index, corpus, scope, None, reads)
        except nb.SetupError as e:
            raise SetupError(str(e))
        return ["@%d %d bytes %s" % (off, length, view) for view, off, length, _ in literals]
    return check


def main(argv):
    parser = argparse.ArgumentParser(prog="golden_check.py")
    parser.add_argument("--root", default=os.path.join(TOP, "tests", "ladx"))
    parser.add_argument("--mirror", default=os.path.join(TOP, "tmp", "ladx-goldens"))
    parser.add_argument("--rom", default=os.path.join(TOP, "tmp", "ladx", "base.gbc"))
    parser.add_argument("--ladxr", default=os.environ.get("LADXR_DIR")
                        or os.path.join(HERE, "ladxr"))
    parser.add_argument("--inventory", default=write_log.DEFAULT_INVENTORY)
    parser.add_argument("cases", nargs="*")
    args = parser.parse_args(argv)
    try:
        if not os.path.isfile(args.rom):
            raise SetupError("no ROM at %s (run tests/ladx/oracle/fetch_rom.sh)" % args.rom)
        if not os.path.isfile(args.inventory):
            raise SetupError("no inventory at %s" % args.inventory)
        rom = _read(args.rom, True)
        sides = write_log.sides_of(_read(args.inventory))
        known = _sibling("known_source").build(rom)
        nintendo = _nintendo(args, rom)
        folders = _case_folders(args.root, args.cases)
        bad, literals, fragments, found = 0, 0, 0, set()
        for name, folder in folders:
            stream = os.path.join(args.mirror, name, "events.txt")
            if not os.path.isfile(stream):
                raise SetupError("no event stream at %s (run gen_golden.sh)" % stream)
            events = write_log.parse_events(_read(stream))
            if events.source != hashlib.sha1(rom).hexdigest():
                raise SetupError("%s was recorded on another ROM" % stream)
            record = os.path.join(args.mirror, name, "invariant.txt")
            if not os.path.isfile(record):
                raise SetupError("no re-run record at %s (run gen_golden.sh)" % record)
            invariant = write_log.parse_invariant(_read(record))
            problems = check_case(folder, events, rom, sides, known, nintendo,
                                  invariant=invariant)
            literals += sum(1 for check, _ in problems if check == "nintendo")
            fragments += sum(int(detail.split(" ")[1]) for check, detail in problems
                             if check == "fragments")
            try:
                found.update(write_log.groups(_read(os.path.join(folder, "rom_writes.txt"))))
            except write_log.WriteLogError:
                pass
            for check, detail in problems:
                print("BAD %s: %s: %s" % (name, check, detail), flush=True)
            if not problems:
                print("ok %s" % name, flush=True)
            bad += 1 if problems else 0
    except (SetupError, write_log.WriteLogError, OSError) as e:
        print("setup: golden-check: %s" % e)
        return 2
    union = [] if args.cases else union_problems(found, sides)
    for detail in union:
        print("BAD tags: %s" % detail)
    print("golden-check: %d cases, %d bad, %d literal runs, %d fragments"
          % (len(folders), bad, literals, fragments))
    return 1 if bad or union else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
