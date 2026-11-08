"""Usage: python3 -I tests/ladx/oracle/tables.py check [--root <dir>] [--inventory <file>]
       [--inputs <dir>]

LADX's patch tables, data/ladx/: LADXR's settings-only writes (the inventory's `table` groups) as
data the C side applies, keyed by what each write depends on. This module is their format:
`parse`, `render` (canonical: parse then render is the identity), `select` (the ordered steps
one key's walk applies) and `check`, the ROM-free structural check against the inventory.

Files, line-based ASCII, single spaces:
  order.txt
    ladx-tables-order 1
    section <file> <function>         an inventory section, in its order
    table <group> <unit> <call>       a `table` row, its unit's id <group>.<n>
    C <group> - <call>                a `C` row
  <group>.txt, one per `table` group, and at most one per `C` group (its constants file: the
  settings-only writes its call computes, as values; its units match no order.txt row)
    ladx-tables 1
    group <group>
    tables <table>...                 ROMWithTables' tables in their order, as in a write log
    unit <group>.<n> <component>...   a unit, numbered from 1 in its first row's order; a
                                      component is a setting key, asm:<CONSTANT> or count
    E <value>...                      an entry: one value per component, `""` the empty string,
                                      entries in increasing order of their values
    B ... / T ...                     its net writes, write-log records (write_log.py), or
    nothing                           the entry writes nothing
<call> is the inventory row's key (the `Call` text, ` #<k>` included), the rest of the line.

`check` prints one MISSING, EXTRA, BADFILE, BADROW, DUP or BADKEY line per mismatch (exit 1),
else `ladx-tables: <u> units in <g> groups, <e> entries, <s> bytes`; a malformed input exits 2.
It proves itself first on planted trees.
"""

import difflib
import importlib.util
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

GROUP_VERSION = "ladx-tables 1"
ORDER_VERSION = "ladx-tables-order 1"
NAME = re.compile(r"[A-Za-z0-9_.-]+")


class TablesError(Exception):
    pass


_SIBLINGS = {}


def _sibling(name):
    if name not in _SIBLINGS:
        spec = importlib.util.spec_from_file_location("tables_" + name,
                                                      os.path.join(HERE, name + ".py"))
        module = importlib.util.module_from_spec(spec)
        no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
        try:
            spec.loader.exec_module(module)
        finally:
            sys.dont_write_bytecode = no_bytecode
        _SIBLINGS[name] = module
    return _SIBLINGS[name]


class Entry:
    def __init__(self, values, lines, banks, entries):
        self.values, self.lines, self.banks, self.entries = values, lines, banks, entries


class Unit:
    def __init__(self, id, components, entries):
        self.id, self.components, self.entries = id, components, entries


class Group:
    def __init__(self, name, tables, units):
        self.name, self.tables, self.units = name, tables, units


class Row:
    def __init__(self, section, side, group, unit, key):
        self.section, self.side, self.group, self.unit, self.key = section, side, group, unit, key


class Order:
    def __init__(self, sections, rows):
        self.sections, self.rows = sections, rows


def _lines(text):
    lines = text.split("\n")
    if not lines or lines[-1] != "":
        raise TablesError("the file does not end with a newline")
    return lines[:-1]


def parse(text):
    lines = _lines(text)
    if lines and lines[0] == ORDER_VERSION:
        return _parse_order(lines)
    if lines and lines[0] == GROUP_VERSION:
        return _parse_group(lines)
    raise TablesError("line 1: not `%s` or `%s`" % (GROUP_VERSION, ORDER_VERSION))


COMPONENT = re.compile(r"[a-z][a-z0-9_]*|asm:[A-Z][A-Z0-9_]*")
UNIT_ID = re.compile(r"([A-Za-z0-9_-]+)\.([1-9][0-9]*)")


def _unit_number(unit, group):
    m = UNIT_ID.fullmatch(unit)
    return int(m.group(2)) if m and m.group(1) == group else None


def _parse_order(lines):
    sections, rows = [], []
    for number, line in enumerate(lines[1:], 2):
        fields = line.split(" ")
        if fields[0] == "section" and len(fields) == 3 and all(fields[1:]):
            name = line[len("section "):]
            if name in sections:
                raise TablesError("line %d: section %s repeated" % (number, name))
            sections.append(name)
            continue
        if not sections:
            raise TablesError("line %d: a row before any section line" % number)
        if len(fields) < 4 or fields[0] not in ("table", "C"):
            raise TablesError("line %d: unknown line %r" % (number, line[:40]))
        side, group, unit = fields[:3]
        key = line.split(" ", 3)[3]
        if not NAME.fullmatch(group):
            raise TablesError("line %d: bad group %r" % (number, group))
        if (unit == "-") != (side == "C") or side == "table" and _unit_number(unit, group) is None:
            raise TablesError("line %d: unit %r for a %s row of %s" % (number, unit, side, group))
        if not key or key != key.strip():
            raise TablesError("line %d: no call, or a call padded with spaces" % number)
        rows.append(Row(sections[-1], side, group, unit, key))
    return Order(sections, rows)


def _value_ok(value):
    return value == '""' or bool(NAME.fullmatch(value))


def _records(name, table_names, lines, first):
    write_log = _sibling("write_log")
    text = "".join(l + "\n" for l in [write_log.VERSION_LINE, " ".join(["tables"] + table_names),
                                     "G " + name] + lines)
    try:
        _, segments = write_log.parse(text)
    except write_log.WriteLogError as e:
        m = re.match(r"line (\d+): (.*)", str(e), re.S)
        if m:
            raise TablesError("line %d: %s" % (first + int(m.group(1)) - 4, m.group(2)))
        raise TablesError("lines %d-%d: %s" % (first, first + len(lines) - 1, e))
    return segments[0][1], segments[0][2]


def _parse_group(lines):
    if len(lines) < 3 or not lines[1].startswith("group ") or lines[2].split(" ")[0] != "tables":
        raise TablesError("lines 2-3: not `group <group>` and `tables <table>...`")
    name = lines[1][len("group "):]
    table_names = lines[2].split(" ")[1:]
    if not NAME.fullmatch(name) or not table_names or len(set(table_names)) != len(table_names) \
            or not all(NAME.fullmatch(t) for t in table_names):
        raise TablesError("lines 2-3: bad group or table names")
    units, bodies = [], []
    for number, line in enumerate(lines[3:], 4):
        fields = line.split(" ")
        if fields[0] == "unit" and len(fields) >= 2:
            if units and not units[-1].entries:
                raise TablesError("line %d: unit %s has no entry" % (number, units[-1].id))
            if _unit_number(fields[1], name) != len(units) + 1:
                raise TablesError("line %d: unit %r is not %s.%d" % (number, fields[1], name,
                                                                      len(units) + 1))
            components = fields[2:]
            if len(set(components)) != len(components) or not all(
                    COMPONENT.fullmatch(c) for c in components):
                raise TablesError("line %d: bad or repeated component" % number)
            units.append(Unit(fields[1], components, []))
        elif fields[0] == "E" and units:
            unit, values = units[-1], fields[1:]
            if len(values) != len(unit.components) or not all(_value_ok(v) for v in values):
                raise TablesError("line %d: values do not match %s's components" % (number,
                                                                                   unit.id))
            if unit.entries and values < unit.entries[-1].values:
                raise TablesError("line %d: entries out of order" % number)
            unit.entries.append(Entry(values, [], {}, {}))
            bodies.append((unit.entries[-1], number, []))
        elif bodies and bodies[-1][1] + len(bodies[-1][2]) == number - 1 and (
                fields[0] in ("B", "T") and "nothing" not in bodies[-1][2]
                or line == "nothing" and not bodies[-1][2]):
            bodies[-1][2].append(line)
        else:
            raise TablesError("line %d: unexpected line %r" % (number, line[:40]))
    if not units or not units[-1].entries:
        raise TablesError("a group without a unit, or a unit without an entry")
    for entry, number, body in bodies:
        if not body:
            raise TablesError("line %d: an entry without a record or `nothing`" % number)
        if body != ["nothing"]:
            entry.lines = body
            entry.banks, entry.entries = _records(name, table_names, body, number + 1)
    return Group(name, table_names, units)


def render(parsed):
    if isinstance(parsed, Order):
        lines = [ORDER_VERSION]
        for section in parsed.sections:
            lines.append("section " + section)
            lines += [" ".join([row.side, row.group, row.unit, row.key])
                      for row in parsed.rows if row.section == section]
    else:
        lines = [GROUP_VERSION, "group " + parsed.name, " ".join(["tables"] + parsed.tables)]
        for unit in parsed.units:
            lines.append(" ".join(["unit", unit.id] + unit.components))
            for entry in unit.entries:
                lines.append(" ".join(["E"] + entry.values))
                lines += entry.lines or ["nothing"]
    return "".join(line + "\n" for line in lines)


class Tables:
    def __init__(self, order, groups):
        self.order, self.groups = order, groups


def select(tables, values):
    steps, seen = [], set()
    for row in tables.order.rows:
        if row.side == "C":
            steps.append((row, None))
            continue
        if row.unit in seen:
            continue
        seen.add(row.unit)
        group = tables.groups.get(row.group)
        unit = next((u for u in group.units if u.id == row.unit), None) if group else None
        if unit is None:
            raise TablesError("unit %s is not in %s.txt" % (row.unit, row.group))
        missing = [c for c in unit.components if c not in values]
        if missing:
            raise TablesError("unit %s: no value for %s" % (unit.id, " ".join(missing)))
        key = [values[c] for c in unit.components]
        entry = next((e for e in unit.entries if e.values == key), None)
        if entry is None:
            raise TablesError("unit %s: no entry for %s" % (unit.id, " ".join(key)))
        steps.append((row, entry))
    return steps


ORDER_FILE = "order.txt"
COUNT = re.compile(r"0|[1-9][0-9]*")


def _inventory(inventory_text):
    check = _sibling("inventory_check")
    sections, bad = check.parse_inventory(inventory_text)
    named, named_bad = check.parse_named(inventory_text)
    if bad or named_bad:
        raise TablesError("the inventory does not parse: %s" % (bad + named_bad)[0])
    names = [check.section_name(f, fn) for f, fn, _ in check.SECTIONS]
    names = [n for n in names if n in sections]
    rows = [(n, row["key"], row["Side"], row["Group"]) for n in names for row in sections[n]
            if row["Side"] in ("table", "C")]
    groups = sorted(g["Group"] for g in named["Groups"] if g["Side"] == "table")
    c_groups = sorted(g["Group"] for g in named["Groups"] if g["Side"] == "C")
    return names, rows, groups, c_groups


def _domains(tier1_text, settings_text):
    oracle = _sibling("ladx_oracle")
    try:
        settings = _sibling("ladx_cases").read_settings(settings_text)
        tier1 = dict(oracle.read_tier1(tier1_text, settings))
        return {s.key: (tier1[s.key], True) if s.key in tier1 else
                ([oracle.setting_value(s.default)], False) for s in settings}
    except Exception as e:
        raise TablesError("tier1.txt or settings.txt does not parse: %s" % e)


def _row_lines(order, names, rows):
    lines = []
    if order.sections != names:
        lines.append("BADROW %s: sections %s, expected %s" % (ORDER_FILE, ", ".join(order.sections),
                                                               ", ".join(names)))
    got = [(r.section, r.key) for r in order.rows]
    want = [(s, k) for s, k, _, _ in rows]
    matcher = difflib.SequenceMatcher(None, want, got, autojunk=False)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            for (section, key, side, group), row in zip(rows[i1:i2], order.rows[j1:j2]):
                if (row.side, row.group) != (side, group):
                    lines.append("BADROW %s: %s: %s: %s %s, inventory %s %s" % (
                        ORDER_FILE, section, key, row.side, row.group, side, group))
            continue
        lines += ["MISSING %s: %s: %s" % (ORDER_FILE, s, k) for s, k in want[i1:i2]]
        lines += ["EXTRA %s: %s: %s" % (ORDER_FILE, s, k) for s, k in got[j1:j2]]
    return lines


def _unit_lines(order, groups):
    lines = []
    used, split, last_unit, last_number = set(), set(), None, {}
    for row in order.rows:
        if row.side != "table":
            continue
        if row.group not in groups:
            last_unit = row.unit
            continue
        if not any(u.id == row.unit for u in groups[row.group].units):
            lines.append("BADROW %s: %s: unit %s is not in %s.txt" % (ORDER_FILE, row.key,
                                                                     row.unit, row.group))
            continue
        if row.unit in used and row.unit != last_unit and row.unit not in split:
            lines.append("BADROW %s: unit %s's rows are not consecutive among the table rows"
                         % (ORDER_FILE, row.unit))
            split.add(row.unit)
        if row.unit not in used:
            number, last = _unit_number(row.unit, row.group), last_number.get(row.group, 0)
            if number < last:
                lines.append("BADROW %s: %s: unit %s first used after %s.%d" % (
                    ORDER_FILE, row.key, row.unit, row.group, last))
            last_number[row.group] = max(number, last)
            used.add(row.unit)
        last_unit = row.unit
    for name in sorted(groups):
        lines += ["BADROW %s.txt: unit %s has no row" % (name, u.id)
                  for u in groups[name].units if u.id not in used]
    return lines


def _key_problem(component, value, domains):
    if component == "count":
        return None if COUNT.fullmatch(value) else "not a decimal count"
    if component.startswith("asm:"):
        return None if value in ("0", "1") else "not 0 or 1"
    values, tier1 = domains[component]
    if value in values:
        return None
    return "not a tier-1 value" if tier1 else "not the default of a setting outside tier 1"


def _entry_lines(name, group, domains):
    lines = []
    for unit in group.units:
        unknown = [c for c in unit.components
                   if c != "count" and not c.startswith("asm:") and c not in domains]
        if unknown:
            lines += ["BADKEY %s: unit %s: component %s is no setting" % (name, unit.id, c)
                      for c in unknown]
            continue
        seen = set()
        for entry in unit.entries:
            key = tuple(entry.values)
            if key in seen:
                lines.append("DUP %s: unit %s: %s" % (name, unit.id, " ".join(key)))
            seen.add(key)
            for component, value in zip(unit.components, entry.values):
                problem = _key_problem(component, value, domains)
                if problem:
                    lines.append("BADKEY %s: unit %s: %s: %s=%s %s" % (
                        name, unit.id, " ".join(key), component, value, problem))
    return lines


def check(root, inventory_text, tier1_text, settings_text):
    names, rows, table_groups, c_groups = _inventory(inventory_text)
    domains = _domains(tier1_text, settings_text)
    expected = [ORDER_FILE] + ["%s.txt" % g for g in table_groups]
    present = sorted(os.listdir(root)) if os.path.isdir(root) else []
    allowed = ["%s.txt" % g for g in c_groups if "%s.txt" % g in present]
    lines = ["MISSING %s" % f for f in expected if f not in present]
    lines += ["EXTRA %s" % f for f in present if f not in expected and f not in allowed]
    parsed, size = {}, 0
    for f in expected + allowed:
        path = os.path.join(root, f)
        if f not in present or not os.path.isfile(path):
            continue
        with open(path, "rb") as stream:
            data = stream.read()
        size += len(data)
        try:
            item = parse(data.decode("ascii"))
        except (UnicodeDecodeError, TablesError) as e:
            lines.append("BADFILE %s: %s" % (f, e))
            continue
        want = Order if f == ORDER_FILE else Group
        if not isinstance(item, want) or want is Group and item.name != f[:-len(".txt")]:
            lines.append("BADFILE %s: holds %s" % (
                f, "group " + item.name if isinstance(item, Group) else "the order"))
            continue
        parsed[f] = item
    groups = {f[:-len(".txt")]: g for f, g in parsed.items() if f != ORDER_FILE}
    order = parsed.get(ORDER_FILE)
    if order is not None:
        lines += _row_lines(order, names, rows)
        lines += _unit_lines(order, {g: groups[g] for g in groups if g in table_groups})
    for name in sorted(groups):
        lines += _entry_lines(name + ".txt", groups[name], domains)
    units = sum(len(g.units) for g in groups.values())
    entries = sum(len(u.entries) for g in groups.values() for u in g.units)
    return lines, "ladx-tables: %d units in %d groups, %d entries, %d bytes" % (
        units, len(groups), entries, size)


def _plant_table(columns, rows):
    return ("| " + " | ".join(columns) + " |\n|" + "---|" * len(columns) + "\n"
            + "".join("| " + " | ".join(cells) + " |\n" for cells in rows) + "\n")


def _plant_inventory():
    check = _sibling("inventory_check")
    rows = [("a(rom)", "table", "fixed"), ("b(rom)", "C", "items"), ("c(rom)", "table", "fixed"),
            ("d(rom)", "table", "fixed"), ("e(rom)", "table", "opts"), ("f(rom)", "C", "items")]
    return ("## generator.py generateRom\n\n"
            + _plant_table(check.COLUMNS, [["`%s`" % c, "-", "-", "-", "-", "-", "", side, g]
                                            for c, side, g in rows])
            + "## Groups\n\n" + _plant_table(check.GROUP_COLUMNS, [
                ["fixed", "table", "-", "-", "-"], ["items", "C", "-", "-", "items"],
                ["opts", "table", "race", "-", "-"]])
            + "".join("## %s\n\n%s" % (name, _plant_table(columns, []))
                      for name, columns in check.NAMED_TABLES.items() if name != "Groups"))


_PLANT_SETTINGS = ("setting race\tMain\tV\tbool\t0\tRace mode\t0\t-\n"
                   "setting beep\tUser\tp\tstr\tslow\tBeep\t1\t-\n"
                   "option beep\tnone\tD\tNone\t-\noption beep\tslow\tS\tSlow\t-\n")
_PLANT_TREE = {
    "order.txt": "ladx-tables-order 1\nsection generator.py generateRom\n"
                 "table fixed fixed.1 a(rom)\nC items - b(rom)\ntable fixed fixed.2 c(rom)\n"
                 "table fixed fixed.2 d(rom)\ntable opts opts.1 e(rom)\nC items - f(rom)\n",
    "fixed.txt": "ladx-tables 1\ngroup fixed\ntables texts\nunit fixed.1\nE\nB 10 =2 00\n"
                 "unit fixed.2 asm:ITEM_DIALOGS\nE 0\nT texts 2 @200+3\nE 1\nnothing\n",
    "opts.txt": "ladx-tables 1\ngroup opts\ntables texts\nunit opts.1 race beep count\n"
                "E 0 slow 3\nB 40 01\nE 1 slow 12\nnothing\n",
    "items.txt": "ladx-tables 1\ngroup items\ntables texts\nunit items.1 asm:HARD_MODE\n"
                 "E 0\nB 80 01\nE 1\nB 80 02\n",
}


def _plant_variants():
    order, fixed, opts, items = (_PLANT_TREE[f] for f in ("order.txt", "fixed.txt", "opts.txt",
                                                          "items.txt"))
    return [
        ("missing", {"opts.txt": None}, ["MISSING opts.txt"]),
        ("extra", {"notes.md": "# prose\n"}, ["EXTRA notes.md"]),
        ("unparsed", {"fixed.txt": fixed.replace("=2 00", "=2 0")},
         ["BADFILE fixed.txt: line 6: bad token '0'"]),
        ("misnamed", {"opts.txt": opts.replace("group opts", "group fixed").replace(
            "opts.1", "fixed.1")}, ["BADFILE opts.txt: holds group fixed"]),
        ("missing row", {"order.txt": order.replace("C items - b(rom)\n", "")},
         ["MISSING order.txt: generator.py generateRom: b(rom)"]),
        ("wrong side", {"order.txt": order.replace("C items - f(rom)", "C opts - f(rom)")},
         ["BADROW order.txt: generator.py generateRom: f(rom): C opts, inventory C items"]),
        ("unit without row", {"order.txt": order.replace("fixed fixed.1", "fixed fixed.2")},
         ["BADROW fixed.txt: unit fixed.1 has no row"]),
        ("split unit", {"order.txt": order.replace("fixed.2 d(rom)", "fixed.1 d(rom)")},
         ["BADROW order.txt: unit fixed.1's rows are not consecutive among the table rows"]),
        ("duplicate", {"fixed.txt": fixed.replace("E 1\n", "E 0\n")},
         ["DUP fixed.txt: unit fixed.2: 0"]),
        ("c-group duplicate", {"items.txt": items.replace("E 1\n", "E 0\n")},
         ["DUP items.txt: unit items.1: 0"]),
        ("bad key", {"opts.txt": opts.replace("E 1 slow 12", "E 1 none 012")},
         ["BADKEY opts.txt: unit opts.1: 1 none 012: beep=none not the default of a setting "
          "outside tier 1",
          "BADKEY opts.txt: unit opts.1: 1 none 012: count=012 not a decimal count"]),
    ]


def self_test():
    import shutil
    import tempfile
    inventory = _plant_inventory()
    for name, change, want in [("correct", {}, [])] + _plant_variants():
        root = tempfile.mkdtemp()
        try:
            for f, text in dict(_PLANT_TREE, **change).items():
                if text is not None:
                    with open(os.path.join(root, f), "w", newline="\n") as stream:
                        stream.write(text)
            if check(root, inventory, "race all\n", _PLANT_SETTINGS)[0] != want:
                return name
        except TablesError:
            return name
        finally:
            shutil.rmtree(root)
    return None


TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
USAGE = __doc__.split("\n\n")[0]


def main(argv):
    options = {"--root": os.path.join(TOP, "data", "ladx"),
               "--inventory": os.path.join(TOP, "notes", "ladx-inventory.md"), "--inputs": HERE}
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
        print("ladx-tables: self-test failed: %s" % failed)
        return 1
    try:
        texts = []
        for path in (options["--inventory"], os.path.join(options["--inputs"], "tier1.txt"),
                     os.path.join(options["--inputs"], "settings.txt")):
            with open(path, encoding="utf-8") as f:
                texts.append(f.read())
        lines, summary = check(options["--root"], *texts)
    except (OSError, UnicodeDecodeError, TablesError) as e:
        print("ladx-tables: %s" % e, file=sys.stderr)
        return 2
    print("\n".join(lines or [summary]))
    return 1 if lines else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
