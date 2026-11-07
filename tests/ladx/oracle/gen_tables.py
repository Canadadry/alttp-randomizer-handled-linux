"""The planning of LADX's patch tables (data/ladx/, tables.py), a module: which `table` rows form
one unit, what each unit is keyed by, which key tuples LADXR can reach, and the fewest coverage
runs that reach them all; ROM-free (the spot count and validate come from LADXR, through
oracle_run.py's Driver). gen_tables.sh (with the ROM) runs the coverage runs and builds the
tables.

Units. A unit is one `table` row, fused with the rows it is coupled with, transitively: two rows
of one table segment of a coverage run's encoding (write_log.table_segments) are coupled when a
run the longest-match rule chose spans bytes both wrote last (`match`), or one rewrites a byte or
an entry the other wrote (`rewrite`). table_units.txt forces a fusion. A fused unit takes in the
rows of its group between its rows; a `C` row that writes in some run, or a row of another table
group, between them is `FUSE-ACROSS <unit> <row>`. Units are numbered <group>.<n> in their first
row's order.

Keys. A unit's components are the union, over its rows, of: the row's Settings column (`*` and
`-` dropped), else its group's Settings key; `count` in place of both for frame-counter; the
settings the row read and the assembler constants it looked up (key_trace.py), a constant only
when its `assembler.const` row names a setting (`asm:<NAME>`); the components of every unit of
an earlier row whose writes it read (read provenance, per group and target) or carried (write
identities); table_keys.txt's widenings. Reading or carrying a `C` group's writes is
`DEPENDS-C <unit> <group>`. Components are ordered: settings in LADXR's order, constants by
name, then count.

Reachability. A unit's underlying settings are its setting components, the settings behind its
constants and, for count, the count's settings; its reachable tuples are the image of every
combination of their tier-1 values (the others at their defaults) through validate. A tuple's
witness is the defaults overridden by its setting values when that gives it, else the first
combination that does (in LADXR's order, values in tier-1 order); a refused value (combos.txt)
brings its covering combo's other values, and a witness that then misses its tuple falls to the
next one; a tuple left without one, and covered by no run, is `UNREACHED <unit> <tuple>`.
Coverage runs: the runs already done first, then the targets (unit, tuple), the largest witness
first, then by unit and tuple; each new run starts from the first target left and grows, in that
order, by every witness that agrees with it on every setting it sets and keeps every chosen
target's tuple; a run covers every target its validated settings give. The spot count is taken
from a pool-only run on COUNT_SEED (it is a function of the settings).
"""

import contextlib
import hashlib
import importlib.util
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def _load(name):
    spec = importlib.util.spec_from_file_location("gen_tables_" + name,
                                                  os.path.join(HERE, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = no_bytecode
    return module


write_log = _load("write_log")
inventory_check = _load("inventory_check")
ladx_oracle = _load("ladx_oracle")
ladx_cases = _load("ladx_cases")
key_trace = _load("key_trace")


class TablesPlanError(Exception):
    pass


# Units ------------------------------------------------------------------------------------------

def _positions(event):
    if event[0] == "B":
        return [("B", event[3] + i) for i in range(len(event[4]))]
    return [("T", event[3], event[4])]


def couplings(events, rom, sides, known=()):
    """[(row a, row b, `match` or `rewrite`)] of an event stream's table segments, rows as event
    sites, a before b in the stream, sorted by their first write."""
    first, found = {}, set()
    for event in events.events:
        first.setdefault(event[2], len(first))
    for _, seg, _, _, matches in write_log.table_segments(events, rom, sides, known):
        writers, last = {}, {}
        for event in seg:
            for pos in _positions(event):
                writers.setdefault(pos, set()).add(event[2])
                last[pos] = event[2]
        for rows in writers.values():
            found |= {(a, b, "rewrite") for a in rows for b in rows if first[a] < first[b]}
        for offset, n in matches:
            rows = {last[("B", p)] for p in range(offset, offset + n)}
            found |= {(a, b, "match") for a in rows for b in rows if first[a] < first[b]}
    return sorted(found, key=lambda f: (first[f[0]], first[f[1]], f[2]))


class Row:
    """An inventory row of side `table` or `C`: site `<section>\\t<call>`, its Settings column's
    names."""

    def __init__(self, site, side, group, settings):
        self.site, self.side, self.group, self.settings = site, side, group, settings


class Unit:
    def __init__(self, id, group, rows):
        self.id, self.group, self.rows = id, group, rows


def _call(site):
    return site.split("\t", 1)[1]


def units(rows, coupled, forced, writing_c):
    """([Unit] in first-row order, FUSE-ACROSS lines) of the rows (in order) fused by the coupled
    and forced pairs [(site, site, why)]; writing_c: the sites of the `C` rows that write in some
    run."""
    index = {r.site: n for n, r in enumerate(rows)}
    parent = {r.site: r.site for r in rows if r.side == "table"}

    def find(s):
        while parent[s] != s:
            s = parent[s]
        return s

    def union(a, b):
        a, b = find(a), find(b)
        if a != b:
            parent[max(a, b, key=index.get)] = min(a, b, key=index.get)

    for a, b, _ in list(coupled) + list(forced):
        if a not in parent or b not in parent or rows[index[a]].group != rows[index[b]].group:
            raise TablesPlanError("rows %s and %s are not table rows of one group"
                                  % (_call(a), _call(b)))
        union(a, b)
    changed = True
    while changed:
        changed = False
        members = {}
        for s in parent:
            members.setdefault(find(s), []).append(index[s])
        for root, found in members.items():
            group = rows[index[root]].group
            for n in range(min(found) + 1, max(found)):
                r = rows[n]
                if r.side == "table" and r.group == group and find(r.site) != root:
                    union(root, r.site)
                    changed = True
    members = {}
    for r in rows:
        if r.side == "table":
            members.setdefault(find(r.site), []).append(r.site)
    out, numbers, problems = [], {}, []
    for r in rows:
        if r.side != "table" or find(r.site) != r.site:
            continue
        numbers[r.group] = numbers.get(r.group, 0) + 1
        unit = Unit("%s.%d" % (r.group, numbers[r.group]), r.group, members[r.site])
        out.append(unit)
        first, last = index[unit.rows[0]], index[unit.rows[-1]]
        for between in rows[first + 1:last]:
            if between.side == "C" and between.site in writing_c or \
                    between.side == "table" and between.group != unit.group:
                problems.append("FUSE-ACROSS %s %s" % (unit.id, _call(between.site)))
    return out, problems


# Keys -------------------------------------------------------------------------------------------

COUNT = "count"
COUNT_GROUP = "frame-counter"


class Inventory:
    """What planning needs of the inventory: rows ([Row] of sides `table` and `C`, in order),
    group_keys {group: Settings key names}, sides {group: side}, constants {NAME: the settings
    its `assembler.const` row names}, only for a constant that names one; settings: LADXR's
    setting keys in order."""

    def __init__(self, rows, group_keys, sides, constants, settings, const_exprs=None):
        self.rows, self.group_keys, self.sides = rows, group_keys, sides
        self.constants, self.settings = constants, settings
        self.const_exprs = const_exprs or {}


CONST = re.compile(r'assembler\.const\("([A-Za-z_][A-Za-z0-9_]*)", (.*)\)')


def _names(cell):
    return [n for n in inventory_check._items(cell) if n != "*"]


def read_inventory(text, settings):
    """The Inventory of an inventory's text, settings LADXR's setting keys in order;
    TablesPlanError when it does not parse. const_exprs {NAME: the value's expression}."""
    sections, bad = inventory_check.parse_inventory(text)
    named, named_bad = inventory_check.parse_named(text)
    if bad or named_bad:
        raise TablesPlanError("the inventory does not parse: %s" % (bad + named_bad)[0])
    rows, constants, exprs = [], {}, {}
    for f, fn, _ in inventory_check.SECTIONS:
        name = inventory_check.section_name(f, fn)
        for row in sections.get(name, []):
            m = CONST.fullmatch(row["key"])
            if m and _names(row["Settings"]):
                constants[m.group(1).upper()] = _names(row["Settings"])
                exprs[m.group(1).upper()] = m.group(2)
            if row["Side"] in ("table", "C"):
                rows.append(Row("%s\t%s" % (name, row["key"]), row["Side"], row["Group"],
                                _names(row["Settings"])))
    groups = named["Groups"]
    return Inventory(rows, {g["Group"]: _names(g["Settings key"]) for g in groups},
                     {g["Group"]: g["Side"] for g in groups}, constants, settings, exprs)


def _hand_lines(text):
    """[(line number, [tab fields])] of a hand-written input, `#` lines and blank ones left out."""
    return [(n, line.split("\t")) for n, line in enumerate(text.splitlines(), 1)
            if line and not line.startswith("#")]


def _table_site(inventory, call, number):
    found = [r.site for r in inventory.rows if r.side == "table" and _call(r.site) == call]
    if len(found) != 1:
        raise TablesPlanError("line %d: %r is not one table row's call" % (number, call))
    return found[0]


def read_widenings(text, inventory):
    """table_keys.txt, `<call>\t<component>\t<reason>` lines, as [(site, component, reason)]."""
    out = []
    for number, fields in _hand_lines(text):
        if len(fields) != 3 or not fields[2].strip():
            raise TablesPlanError("line %d: not <call>, <component> and <reason>" % number)
        call, component, reason = fields
        site = _table_site(inventory, call, number)
        if not (component in inventory.settings or component == COUNT
                or component.startswith("asm:") and component[4:] in inventory.constants):
            raise TablesPlanError("line %d: %r is no setting, setting constant or count"
                                  % (number, component))
        out.append((site, component, reason))
    return out


def read_fusions(text, inventory):
    """table_units.txt, `<call>\t<call>\t<reason>` lines, as [(site, site, reason)]: two table
    rows of one group."""
    out = []
    for number, fields in _hand_lines(text):
        if len(fields) != 3 or not fields[2].strip():
            raise TablesPlanError("line %d: not <call>, <call> and <reason>" % number)
        a, b = (_table_site(inventory, c, number) for c in fields[:2])
        groups = {r.group for r in inventory.rows if r.site in (a, b)}
        if a == b or len(groups) != 1:
            raise TablesPlanError("line %d: not two table rows of one group" % number)
        out.append((a, b, fields[2]))
    return out


BANK_SIZE = 0x4000


class Facts:
    """What the coverage runs traced, per row site: settings_reads and constant_reads (names),
    provenance {(writer group, target)} of its reads of written bytes, carried {writer site} of
    the write identities its writes carry, targets {target} it wrote (`bank 0x<NN>`,
    `table <name>`, as read_trace.py names them). The add_ methods take one run's files."""

    def __init__(self):
        self.settings_reads, self.constant_reads = {}, {}
        self.provenance, self.carried, self.targets = {}, {}, {}

    def add_events(self, events):
        """A run's write_log.Events: targets, and carried from each write identity's first
        writer."""
        writer = {}
        for event in events.events:
            s = event[2]
            if event[0] == "B":
                targets = {"bank 0x%02X" % ((event[3] + i) // BANK_SIZE)
                           for i in range(len(event[4]))}
                origins = event[5]
            else:
                targets, origins = {"table " + event[3]}, event[6] or ()
            self.targets.setdefault(s, set()).update(targets)
            for q in origins:
                if q >= write_log.KNOWN:
                    continue
                if q not in writer:
                    writer[q] = s
                elif writer[q] != s:
                    self.carried.setdefault(s, set()).add(writer[q])

    def add_reads(self, text):
        """A run's aggregated reads.txt (read_trace.py Trace.write): provenance."""
        for line in text.splitlines():
            fields = line.split("\t")
            if len(fields) < 2 or fields[1] == "-":
                continue
            if len(fields) != 8:
                raise TablesPlanError("bad read line %r" % line[:60])
            kind, _, groups = fields[6].partition(":")
            if kind in ("written", "mixed"):
                self.provenance.setdefault("%s\t%s" % tuple(fields[1:3]), set()).update(
                    (g, fields[5]) for g in groups.split(","))

    def add_keys(self, text):
        """A run's key trace (key_trace.py): settings_reads and constant_reads."""
        try:
            settings, constants = key_trace.parse(text)
        except key_trace.KeyTraceError as e:
            raise TablesPlanError(str(e))
        for mine, theirs in ((self.settings_reads, settings), (self.constant_reads, constants)):
            for site, names in theirs.items():
                mine.setdefault(site, set()).update(names)


def _ordered(components, settings):
    order = {s: n for n, s in enumerate(settings)}
    return sorted(components, key=lambda c: (
        (0, order[c], "") if c in order else (1, 0, c) if c.startswith("asm:") else (2, 0, c)))


def unit_keys(unit_list, inventory, facts, widenings):
    """({unit id: [component]}, DEPENDS-C lines) of the units; widenings [(site, component,
    reason)] (table_keys.txt)."""
    index = {r.site: n for n, r in enumerate(inventory.rows)}
    of_row = {s: u for u in unit_list for s in u.rows}
    direct, deps, problems = {}, {}, set()
    for unit in unit_list:
        own, needs = set(), set()
        for s in unit.rows:
            row = inventory.rows[index[s]]
            if row.group == COUNT_GROUP:
                own.add(COUNT)
            else:
                own |= set(row.settings or inventory.group_keys.get(row.group, []))
            own |= facts.settings_reads.get(s, set())
            own |= {"asm:" + c for c in facts.constant_reads.get(s, ()) if c in inventory.constants}
            own |= {c for w, c, _ in widenings if w == s}
            for group, target in facts.provenance.get(s, ()):
                if inventory.sides.get(group) != "table":
                    problems.add("DEPENDS-C %s %s" % (unit.id, group))
                    continue
                needs |= {of_row[w].id for w in of_row
                          if inventory.rows[index[w]].group == group and index[w] < index[s]
                          and target in facts.targets.get(w, ())}
            for w in facts.carried.get(s, ()):
                if w in of_row:
                    needs.add(of_row[w].id)
                else:
                    group = inventory.rows[index[w]].group if w in index else "-"
                    problems.add("DEPENDS-C %s %s" % (unit.id, group))
        direct[unit.id], deps[unit.id] = own, needs - {unit.id}
    keys = {u: set(c) for u, c in direct.items()}
    changed = True
    while changed:
        changed = False
        for u in keys:
            more = set().union(*[keys[d] for d in deps[u]]) - keys[u]
            if more:
                keys[u] |= more
                changed = True
    return ({u.id: _ordered(keys[u.id], inventory.settings) for u in unit_list},
            sorted(problems))



# Reachability and coverage runs -----------------------------------------------------------------

class Model:
    """LADXR's settings as planning sees them, values rendered as in the tables: settings (keys
    in LADXR's order), defaults {key: value}, domains {key: values} (tier-1 values, else the
    default alone), validate({key: value} non-defaults) -> {key: value} of every setting,
    constants {NAME: (the settings it reads, fn(settings) -> value)}, count(settings) -> value
    or None (no count), count_settings, refused {`k=v`: {key: value} of its companion}."""

    def __init__(self, settings, defaults, domains, validate, constants, count, count_settings,
                 refused=None):
        self.settings, self.defaults, self.domains = settings, defaults, domains
        self.constants, self.count_settings = constants, count_settings
        self.refused = refused or {}
        self._validate, self._count = validate, count
        self._validated, self._counted = {}, {}

    def validated(self, assignment):
        key = tuple(sorted(assignment.items()))
        if key not in self._validated:
            self._validated[key] = self._validate(dict(assignment))
        return self._validated[key]

    def value(self, component, settings):
        if component == COUNT:
            key = tuple(settings[s] for s in self.count_settings)
            if key not in self._counted:
                self._counted[key] = self._count(settings)
            return self._counted[key]
        if component.startswith("asm:"):
            return self.constants[component[4:]][1](settings)
        return settings[component]

    def key(self, components, assignment):
        """The tuple the components take under the assignment, validated; None for a count
        LADXR gives none."""
        settings = self.validated(assignment)
        values = tuple(self.value(c, settings) for c in components)
        return None if None in values else values

    def underlying(self, components):
        names = set()
        for c in components:
            if c == COUNT:
                names |= set(self.count_settings)
            elif c.startswith("asm:"):
                names |= set(self.constants[c[4:]][0])
            else:
                names.add(c)
        return [s for s in self.settings if s in names]

    def with_companions(self, assignment):
        out = dict(assignment)
        for k, v in assignment.items():
            out.update(self.refused.get("%s=%s" % (k, v), {}))
        return out


def _combinations(names, model):
    """Every assignment of the names' domain values, others at their defaults, in LADXR's order
    with values in domain order, as {key: value} of the non-default ones."""
    out = [{}]
    for name in names:
        out = [dict(a, **({} if v == model.defaults[name] else {name: v}))
               for a in out for v in model.domains[name]]
    return out


def reachable(components, model):
    """([(tuple, witness)] sorted by tuple, [tuple] left without a witness) of the components."""
    image = {}
    for assignment in _combinations(model.underlying(components), model):
        t = model.key(components, assignment)
        if t is not None:
            image.setdefault(t, []).append(assignment)
    found, unreached = [], []
    for t in sorted(image):
        direct = {c: v for c, v in zip(components, t)
                  if c in model.defaults and v != model.defaults[c]}
        for candidate in [direct] + image[t]:
            witness = model.with_companions(candidate)
            if model.key(components, witness) == t:
                found.append((t, witness))
                break
        else:
            unreached.append(t)
    return found, unreached


class Run:
    """A coverage run: its assignment {key: value} and the targets (unit, tuple) it covers."""

    def __init__(self, assignment, covers):
        self.assignment, self.covers = assignment, covers


def _agree(a, b):
    return all(b[k] == v for k, v in a.items() if k in b)


def plan_runs(keys, model, done=()):
    """([Run], UNREACHED lines) covering every reachable tuple of every unit, keys {unit id:
    [component]}: first the runs of done (assignments already run), then new ones; targets most
    constrained (largest witness) first, then by unit and tuple."""
    targets, missing = [], []
    for unit in sorted(keys):
        found, unreached = reachable(keys[unit], model)
        targets += [(unit, t, w) for t, w in found]
        missing += [(unit, t) for t in unreached]
    targets.sort(key=lambda x: (-len(x[2]), x[0], x[1]))
    runs, covered = [], set()

    def add(assignment):
        covers = [(c, model.key(keys[c], assignment)) for c in sorted(keys)]
        covers = [(c, v) for c, v in covers if v is not None]
        covered.update(covers)
        runs.append(Run({k: assignment[k] for k in model.settings if k in assignment}, covers))

    for assignment in done:
        add(assignment)
    for unit, t, witness in targets:
        if (unit, t) in covered:
            continue
        assignment, chosen = dict(witness), [(unit, t)]
        for other, u, w in targets:
            if (other, u) in covered or (other, u) in chosen or not _agree(assignment, w):
                continue
            trial = dict(assignment, **w)
            if all(model.key(keys[c], trial) == v for c, v in chosen + [(other, u)]):
                assignment, chosen = trial, chosen + [(other, u)]
        add(assignment)
    return runs, ["UNREACHED %s %s" % (unit, " ".join(t)) for unit, t in missing
                  if (unit, t) not in covered]


# The model from the fixtures and LADXR -----------------------------------------------------------

COUNT_SEED = hashlib.sha256(b"ladx-tables count").digest()[:16]


def assignment_lines(assignment):
    """LADXR's `k=v` set strings of an assignment {key: rendered value}, `""` given as `k=`."""
    return ["%s=%s" % (k, "" if v == '""' else v) for k, v in assignment.items()]


def build_model(inventory, tier1_text, settings_text, combos_text, validate_text, validate,
                count, count_settings):
    """The Model of the fixtures (tier1.txt, settings.txt, combos.txt's refused values and
    their first covering combo, settings_validate.txt), the inventory's setting constants
    (their expressions evaluated on the settings), validate and count (see Model)."""
    try:
        meta = ladx_cases.read_settings(settings_text)
        tier1 = dict(ladx_oracle.read_tier1(tier1_text, meta))
        expected = ladx_cases.expected_list(tier1_text, settings_text, validate_text,
                                            combos_text)
    except (ladx_cases.CasesError, ladx_oracle.OracleError) as e:
        raise TablesPlanError(str(e))
    keys = [s.key for s in meta]
    defaults = {s.key: ladx_oracle.setting_value(s.default) for s in meta}
    domains = {k: tier1.get(k, [defaults[k]]) for k in keys}
    combos = dict(expected.combos)
    refused = {}
    for _, (single,), covering in expected.refused:
        refused[single] = dict(a.split("=", 1) for a in combos[covering[0]] if a != single)
    kinds = {s.key: s.kind for s in meta}

    def constant(expr):
        def value(settings):
            ns = type("S", (), {k: ladx_cases._python_value(kinds[k], v)
                                for k, v in settings.items()})
            return "%d" % eval(expr, {"__builtins__": {}}, {"settings": ns})
        return value

    constants = {name: (names, constant(inventory.const_exprs[name]))
                 for name, names in inventory.constants.items()}
    return Model(keys, defaults, domains, validate, constants, count, count_settings, refused)


def settings_validate(settings_class):
    """validate for Model: LADXR's Settings (settings_class) set from the assignment, validated
    (its prints dropped), every setting rendered."""
    def validate(assignment):
        settings = settings_class()
        for line in assignment_lines(assignment):
            settings.set(line)
        with contextlib.redirect_stdout(io.StringIO()):
            settings.validate()
        return {s.key: ladx_oracle.setting_value(s.value) for s in settings}
    return validate


def spot_count(driver, key_location, defaults):
    """count for Model: the spots of the logic that are not of class key_location, from an
    oracle_run.Driver's pool-only run on COUNT_SEED (the count is a function of the settings);
    None when the run does not reach the pool."""
    def count(settings):
        assignment = {k: v for k, v in settings.items() if k in defaults and v != defaults[k]}
        lines = driver.run(COUNT_SEED, assignment_lines(assignment), pool_only=True)
        if lines[-1] != "outcome ok":
            return None
        return "%d" % len([s for s in driver.logic.iteminfo_list if type(s) != key_location])
    return count
