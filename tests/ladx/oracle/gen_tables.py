"""Usage: python3 -B tests/ladx/oracle/gen_tables.py build --out <dir> --runs <dir>
       [--ladxr <dir>] [--rom <file>] [--inventory <file>] [--inputs <dir>]
       python3 -B tests/ladx/oracle/gen_tables.py sample [--ladxr <dir>] [--inventory <file>]
       [--inputs <dir>]
       python3 -B tests/ladx/oracle/gen_tables.py const --out <dir> [--ladxr <dir>] [--rom <file>]
       [--inventory <file>] [--inputs <dir>]

The planning and building of LADX's patch tables (data/ladx/, tables.py): which `table` rows form
one unit, what each unit is keyed by, which key tuples LADXR can reach, the fewest coverage runs
that reach them all, and each unit's entries cut out of the runs' encodings. The planning is
ROM-free (the spot count and validate come from LADXR, through oracle_run.py's Driver).

`build` (gen_tables.sh's worker, with the ROM; run without -I or -E, which drop PYTHONHASHSEED)
plans on the keys the runs done so far give and runs what the plan adds, until it adds nothing:
one oracle_run.py --rom --reads --keys --no-invariant process per seed candidate under
<runs>/<run>/, `<run>: k=<k> <k=v>...` per run (`no ok seed below 64` when none is ok, and its
merged witnesses are then planned alone). Each run is encoded by write_log.py's own encoder and
each unit's entry for the run's tuple is its net records cut out of its segment. Then either one
problem line each, exit 1, nothing written (`CONFLICT <unit> <tuple>`: two runs give one tuple
different records; `UNREACHED`, `DEPENDS-C`, `FUSE-ACROSS`, below; `SPLIT <unit>`: a unit writes
in two segments of one run), or <out>'s files are replaced by the tables and it prints
`gen_tables: <n> coverage runs in <s> s, <f> files written to <out>`.

Units. A unit is one `table` row, fused with the rows it is coupled with, transitively: two rows
of one table segment of a coverage run's encoding (write_log.table_segments) are coupled when a
run the longest-match rule chose spans bytes both wrote last (`match`), or one rewrites a byte or
an entry the other wrote (`rewrite`). Only `match` couplings fuse; a `rewrite` makes the earlier
row's unit take the later row's unit's key (the earlier entry loses the rewritten bytes exactly
when the later row writes them). table_units.txt forces a fusion. A fused unit takes in the
rows of its group between its rows; a `C` row that writes in some run, or a row of another table
group, between them is `FUSE-ACROSS <unit> <row>`. Units are numbered <group>.<n> in their first
row's order.

Keys. A unit's components are the union, over its rows, of: the row's Settings column (`*` and
`-` dropped), else its group's Settings key; `count` in place of both for frame-counter; the
settings the row read and the assembler constants it looked up (key_trace.py), a constant only
when its `assembler.const` row names a setting (`asm:<NAME>`); the components of every unit of
an earlier row that wrote inside a range it read (read provenance: group, target and the
range's hull) or whose writes it carried (write identities); table_keys.txt's widenings.
Reading or carrying a `C` group's writes is `DEPENDS-C <unit> <group>`. Components are ordered: settings in LADXR's order, constants by
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

Constants (`const`, with the ROM; `build` writes them too): the C groups' constants files
(CONST_GROUPS, const_group), contract in tests/ladx/README.md, "Patch tables".

Sample (`sample`, ROM-free, LADXR's validate): the 12 `sample <nn> k=v ...` lines of combos.txt
(sample_lines), appended once; a combos.txt that has them is refused.
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
tables = _load("tables")


class TablesPlanError(Exception):
    pass


# Units ------------------------------------------------------------------------------------------

def _positions(event):
    if event[0] == "B":
        return [("B", event[3] + i) for i in range(len(event[4]))]
    return [("T", event[3], event[4])]


class RunTables:
    """One run's table segments as the goldens encode them (write_log's own encoder): tables, the
    stream's table order; segments [(group, {position: (atom or atoms, last writing site)})],
    positions ("B", offset) and ("T", table, key); couplings, as couplings() gives them."""

    def __init__(self, tables, segments, couplings):
        self.tables, self.segments, self.couplings = tables, segments, couplings

    def entries(self, unit_of):
        """({unit: its record lines}, SPLIT lines) of the units that write in the run, unit_of
        {site: unit id}: each unit's net records cut out of its segment."""
        out, seen, problems = {}, set(), []
        for group, writes in self.segments:
            cut = {}
            for pos, (atoms, s) in writes.items():
                cut.setdefault(unit_of[s], {})[pos] = atoms
            for unit in sorted(cut):
                if unit in seen and "SPLIT " + unit not in problems:
                    problems.append("SPLIT " + unit)
                seen.add(unit)
                banks = {p[1]: a for p, a in cut[unit].items() if p[0] == "B"}
                tables = {p[1:]: a for p, a in cut[unit].items() if p[0] == "T"}
                out.setdefault(unit, []).extend(
                    write_log._records(group, banks, tables, self.tables)[1:])
        return out, problems


def encode_run(events, rom, sides, known=()):
    """The RunTables of an event stream on the source rom."""
    first, found, segments = {}, set(), []
    for event in events.events:
        first.setdefault(event[2], len(first))
    for group, seg, banks, tables, matches in write_log.table_segments(events, rom, sides, known):
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
        writes = {("B", p): (a, last[("B", p)]) for p, a in banks.items()}
        writes.update({("T",) + k: (a, last[("T",) + k]) for k, a in tables.items()})
        segments.append((group, writes))
    return RunTables(list(events.tables), segments,
                     sorted(found, key=lambda f: (first[f[0]], first[f[1]], f[2])))


def couplings(events, rom, sides, known=()):
    """[(row a, row b, `match` or `rewrite`)] of an event stream's table segments, rows as event
    sites, a before b in the stream, sorted by their first write."""
    return encode_run(events, rom, sides, known).couplings


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
    setting keys in order; sections: the inventory's section names in order (default: its
    rows')."""

    def __init__(self, rows, group_keys, sides, constants, settings, const_exprs=None,
                 sections=None):
        self.rows, self.group_keys, self.sides = rows, group_keys, sides
        self.constants, self.settings = constants, settings
        self.const_exprs = const_exprs or {}
        self.sections = sections or list(dict.fromkeys(r.site.split("\t")[0] for r in rows))


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
    rows, constants, exprs, names = [], {}, {}, []
    for f, fn, _ in inventory_check.SECTIONS:
        name = inventory_check.section_name(f, fn)
        if name in sections:
            names.append(name)
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
                     {g["Group"]: g["Side"] for g in groups}, constants, settings, exprs, names)


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
RANGE = re.compile(r" 0x([0-9A-F]+)\.\.0x([0-9A-F]+)$")


class Facts:
    """What the coverage runs traced, per row site: settings_reads and constant_reads (names),
    provenance {(writer group, target, (first, last) or None)} of its reads of written bytes,
    the range read (bank offsets or table indices, None when the trace gives none), carried
    {writer site} of the write identities its writes carry, targets {target} it wrote
    (`bank 0x<NN>`, `table <name>`, as read_trace.py names them) and positions {target:
    {offset in the bank or table key}} of those writes. The add_ methods take one run's
    files."""

    def __init__(self):
        self.settings_reads, self.constant_reads = {}, {}
        self.provenance, self.carried, self.targets, self.positions = {}, {}, {}, {}

    def add_events(self, events):
        """A run's write_log.Events: targets, and carried from each write identity's first
        writer."""
        writer = {}
        for event in events.events:
            s = event[2]
            places = self.positions.setdefault(s, {})
            if event[0] == "B":
                for p in range(event[3], event[3] + len(event[4])):
                    places.setdefault("bank 0x%02X" % (p // BANK_SIZE), set()).add(p % BANK_SIZE)
                origins = event[5]
            else:
                places.setdefault("table " + event[3], set()).add(event[4])
                origins = event[6] or ()
            self.targets.setdefault(s, set()).update(places)
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
                m = RANGE.search(fields[7])
                span = (int(m.group(1), 16), int(m.group(2), 16)) if m else None
                self.provenance.setdefault("%s\t%s" % tuple(fields[1:3]), set()).update(
                    (g, fields[5], span) for g in groups.split(","))

    def add_keys(self, text):
        """A run's key trace (key_trace.py): settings_reads and constant_reads."""
        try:
            settings, constants = key_trace.parse(text)
        except key_trace.KeyTraceError as e:
            raise TablesPlanError(str(e))
        for mine, theirs in ((self.settings_reads, settings), (self.constant_reads, constants)):
            for site, names in theirs.items():
                mine.setdefault(site, set()).update(names)


def _inside(positions, span):
    """Whether a write at one of positions (None: unknown) lies in the read span (None: all);
    an alternate table key (a string) lies in every span."""
    return positions is None or span is None or any(
        isinstance(p, str) or span[0] <= p <= span[1] for p in positions)


def _ordered(components, settings):
    order = {s: n for n, s in enumerate(settings)}
    return sorted(components, key=lambda c: (
        (0, order[c], "") if c in order else (1, 0, c) if c.startswith("asm:") else (2, 0, c)))


def unit_keys(unit_list, inventory, facts, widenings, rewrites=()):
    """({unit id: [component]}, DEPENDS-C lines) of the units; widenings [(site, component,
    reason)] (table_keys.txt); rewrites [(row a, row b, _)]: b rewrote a byte a wrote in one
    segment, so a's unit takes b's unit's key."""
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
            for group, target, span in facts.provenance.get(s, ()):
                if inventory.sides.get(group) != "table":
                    problems.add("DEPENDS-C %s %s" % (unit.id, group))
                    continue
                needs |= {of_row[w].id for w in of_row
                          if inventory.rows[index[w]].group == group and index[w] < index[s]
                          and target in facts.targets.get(w, ())
                          and _inside(facts.positions.get(w, {}).get(target), span)}
            for w in facts.carried.get(s, ()):
                if w in of_row:
                    needs.add(of_row[w].id)
                else:
                    group = inventory.rows[index[w]].group if w in index else "-"
                    problems.add("DEPENDS-C %s %s" % (unit.id, group))
        needs |= {of_row[b].id for a, b, _ in rewrites if a in unit.rows}
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
        self._validated, self._counted, self._keys = {}, {}, {}

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
        return self.key_in(components, self.validated(assignment))

    def key_in(self, components, settings):
        """key's tuple for settings validated() gave (cached by their identity)."""
        cache = (id(settings), tuple(components))
        if cache in self._keys:
            return self._keys[cache]
        values = tuple(self.value(c, settings) for c in components)
        self._keys[cache] = None if None in values else values
        return self._keys[cache]

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


def plan_runs(keys, model, done=(), avoid=()):
    """([Run], UNREACHED lines) covering every reachable tuple of every unit, keys {unit id:
    [component]}: first the runs of done (assignments already run), then new ones; targets most
    constrained (largest witness) first, then by unit and tuple. A merged assignment in avoid
    (runs with no ok seed) is replaced by its first target's witness alone, and a witness in
    avoid is not planned."""
    targets, missing = [], []
    for unit in sorted(keys):
        found, unreached = reachable(keys[unit], model)
        targets += [(unit, t, w) for t, w in found]
        missing += [(unit, t) for t in unreached]
    targets.sort(key=lambda x: (-len(x[2]), x[0], x[1]))
    runs, covered = [], set()

    def add(assignment):
        settings = model.validated(assignment)
        covers = [(c, model.key_in(keys[c], settings)) for c in sorted(keys)]
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
            settings = model.validated(trial)
            if all(model.key_in(keys[c], settings) == v for c, v in chosen + [(other, u)]):
                assignment, chosen = trial, chosen + [(other, u)]
        if assignment in avoid:
            assignment = dict(witness)
            if assignment in avoid:
                continue
        add(assignment)
    missing += [(unit, t) for unit, t, _ in targets]
    return runs, ["UNREACHED %s %s" % (unit, " ".join(t)) for unit, t in sorted(set(missing))
                  if (unit, t) not in covered]


# The random sample -------------------------------------------------------------------------------

def _drawn(model, i, suffix):
    out = {}
    for key in model.settings:
        values = model.domains[key]
        digest = hashlib.sha256(("ladx-sample %d %s%s" % (i, key, suffix)).encode()).digest()
        value = values[int.from_bytes(digest, "big") % len(values)]
        if value != model.defaults[key]:
            out[key] = value
    return model.validated(out)


def _refused(model, settings):
    return any(settings[k] == v and any(settings[c] != w for c, w in model.refused[r].items())
               for r in model.refused for k, v in [r.split("=", 1)])


def sample_lines(model, n):
    """combos.txt's `sample <nn> k=v ...` lines 1..n: line i takes, for every setting in LADXR's
    order, its domain value at SHA-256(`ladx-sample <i> <key>`) mod the domain's size, big-endian;
    validated, its non-default values. A refused value without its companion is redrawn with
    ` #<j>` appended to every hashed text, the first j that is not."""
    lines = []
    for i in range(1, n + 1):
        j = 0
        settings = _drawn(model, i, "")
        while _refused(model, settings):
            j += 1
            settings = _drawn(model, i, " #%d" % j)
        lines.append(" ".join(["sample %02d" % i] + [
            "%s=%s" % (k, settings[k]) for k in model.settings
            if settings[k] != model.defaults[k]]))
    return lines


def with_sample(combos_text, lines):
    """combos.txt's text with the sample lines appended; refuses a text that has one already
    (the sample is drawn once)."""
    if any(line.startswith("sample ") for line in combos_text.splitlines()):
        raise TablesPlanError("combos.txt already holds the sample, never re-drawn")
    return combos_text + "".join(line + "\n" for line in lines)


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


# C-group constants -------------------------------------------------------------------------------

def _net(events):
    """{position: value} of writes ("B", offset, bytes) and ("T", table, key, bytes or int),
    positions ("B", offset) and ("T", table, key), the last write winning."""
    net = {}
    for e in events:
        if e[0] == "B":
            for i, v in enumerate(e[2]):
                net[("B", e[1] + i)] = v
        else:
            net[("T", e[1], e[2])] = e[3]
    return net


def _const_records(net, rom, known, order):
    """The record lines of net writes, values encoded by the table rule with no origin."""
    banks, entries, offsets = {}, {}, sorted(p[1] for p in net if p[0] == "B")
    for start, stop in write_log._spans(offsets):
        values = bytes(net[("B", p)] for p in range(start, stop))
        atoms = write_log._table_atoms(rom, values, [None] * len(values), start)
        atoms = write_log._fragment_atoms(rom, known, start, atoms)
        banks.update(zip(range(start, stop), atoms))
    for pos, value in net.items():
        if pos[0] == "T":
            if not isinstance(value, int):
                value = write_log._table_atoms(rom, value, [None] * len(value))
                value = write_log._fragment_atoms(rom, known, pos[1:], value)
            entries[pos[1:]] = value
    return write_log._records("-", banks, entries, order)[1:]


class _IndexedRom(bytes):
    """The ROM whose find of a MIN_MATCH-byte needle absent from it returns -1 at once."""

    def __new__(cls, data):
        self = super().__new__(cls, data)
        n = write_log.MIN_MATCH
        self._windows = {data[i:i + n] for i in range(len(data) - n + 1)}
        return self

    def find(self, sub, *args):
        if len(sub) == write_log.MIN_MATCH and not args and sub not in self._windows:
            return -1
        return super().find(sub, *args)


def const_group(group, order, components, runs, own, rom, known, variants=False):
    """The text of a C group's constants file (tables.py's group format): runs [(key tuple,
    [write])], one per reachable tuple of components, each the writes of the group's call in
    order; own [("T", table, key) or ("B", offset)], the writes C makes itself, left out and
    required in every run. The writes equal in every run form unit <group>.1 (no component), the
    others <group>.2 keyed by components; either is omitted when empty. With variants (no
    component), each run's other writes are instead their own unit <group>.<k+2>, in run order.
    TablesPlanError on an own write a run misses, or a position both units write."""
    kept = []
    for values, events in runs:
        for o in own:
            if not any(e[:len(o)] == tuple(o) for e in events):
                raise TablesPlanError("%s: %s never writes %s" % (group, " ".join(values) or "the run",
                                      " ".join(o[:1] + tuple("%x" % x if isinstance(x, int) else x
                                                             for x in o[1:]))))
        kept.append((values, [e for e in events
                              if not any(e[:len(o)] == tuple(o) for o in own)]))
    common = set(kept[0][1]) if kept else set()
    for _, events in kept[1:]:
        common &= set(events)
    fixed = [e for e in kept[0][1] if e in common] if kept else []
    keyed = [(values, [e for e in events if e not in common]) for values, events in kept]
    fixed_net = _net(fixed)
    rom = _IndexedRom(rom)
    for values, events in keyed:
        both = set(fixed_net) & set(_net(events))
        if both:
            raise TablesPlanError("%s: %s: %d positions written by both units"
                                  % (group, " ".join(values), len(both)))
    units = []
    if fixed:
        units.append(tables.Unit("", [], [tables.Entry([], _const_records(fixed_net, rom, known,
                                                                          order), {}, {})]))
    if variants:
        units += [tables.Unit("", [], [tables.Entry([], _const_records(_net(events), rom, known,
                                                                       order), {}, {})])
                  for _, events in keyed]
    elif any(events for _, events in keyed):
        units.append(tables.Unit("", list(components), [
            tables.Entry(list(values), _const_records(_net(events), rom, known, order), {}, {})
            for values, events in sorted(keyed)]))
    for n, unit in enumerate(units, 1):
        unit.id = "%s.%d" % (group, n)
    return tables.render(tables.Group(group, list(order), units))


def _kept(group, k, events, keep):
    """events' bank writes inside keep [(absolute offset, length)], each range required whole;
    TablesPlanError on a write straddling a range's edge or a range not written whole."""
    kept = []
    for start, n in keep:
        inside = [e for e in events if e[0] == "B" and start <= e[1] and e[1] + len(e[2]) <= start + n]
        if any(e[0] == "B" and e[1] < start + n and start < e[1] + len(e[2]) and e not in inside
               for e in events):
            raise TablesPlanError("%s: call %d straddles B %x+%x" % (group, k, start, n))
        if set(_net(inside)) != {("B", start + i) for i in range(n)}:
            raise TablesPlanError("%s: call %d never writes B %x+%x" % (group, k, start, n))
        kept += inside
    return [e for e in events if e in kept]


def calls_group(group, order, runs, owns, rom, known, keeps=None):
    """The text of a C group's constants file in calls mode: runs [[write]], one per call in
    order, owns [[("T", table, key) or ("B", offset)]] each call's writes C makes itself, left
    out and required; keeps, per call, None or [(absolute offset, length)], the only bank ranges
    its unit keeps (_kept). Each call's other writes form its own unit <group>.<k+1>, no
    component, one entry (`nothing` when empty); there is no fixed unit. TablesPlanError on an
    own write a call misses."""
    rom = _IndexedRom(rom)
    units = []
    keeps = keeps or [None] * len(runs)
    for k, (events, own, keep) in enumerate(zip(runs, owns, keeps), 1):
        for o in own:
            if not any(e[:len(o)] == tuple(o) for e in events):
                raise TablesPlanError("%s: call %d never writes %s" % (
                    group, k, " ".join(o[:1] + tuple("%x" % x if isinstance(x, int) else x
                                                     for x in o[1:]))))
        kept = [e for e in events if not any(e[:len(o)] == tuple(o) for o in own)]
        if keep is not None:
            kept = _kept(group, k, kept, keep)
        unit = tables.Unit("", [], [tables.Entry([], _const_records(_net(kept), rom, known, order),
                                                 {}, {})])
        unit.id = "%s.%d" % (group, k)
        units.append(unit)
    return tables.render(tables.Group(group, list(order), units))


class ConstSpec:
    """A C group whose call writes settings-only bytes C cannot compute (LADXR's assembler, or
    LADXR's files): call, its text evaluated in generator.py's namespace with the seed-dependent
    arguments None; components, ordered as a unit's; own, the writes C makes itself; variants,
    the dotted name of the module whose one choice the call draws (no component), or None;
    calls, a list of (call, own) or (call, own, keep) run once each with no component
    (calls_group), call and own then unused."""

    def __init__(self, call, components, own, variants=None, calls=None):
        self.call, self.components, self.own, self.variants = call, components, own, variants
        self.calls = calls


def _specific_lists():
    """setSpecificInstruments' sorted lists, one per non-empty subset of 1..8, by mask 1..255
    (bit i-1 for instrument i)."""
    return [[i for i in range(1, 9) if mask >> (i - 1) & 1] for mask in range(1, 256)]


_GOAL_TEXT = [("T", "texts", 0x1A3)]
_MAZE_ROOM6 = [("T", "entities", 6), ("T", "rooms_overworld_top", 6), ("B", 0x26 * 0x4000 + 6 * 80)]


def _items_calls():
    """items' calls (patches of locations/ on a fresh parse) and the positions C writes itself:
    the beach sword's, the start item's, chest 0x1B6's, the boomerang trade's and gift's, the
    keyhole hook, the castle keyhole's, then each other keyhole's handler at slot 0."""
    bank = BANK_SIZE

    def loc(module, expr):
        return "importlib.import_module(\"locations.%s\").%s" % (module, expr)
    item = lambda room: ("B", 0x3E * bank + 0x3800 + room)
    keyhole_item = lambda room: ("B", 0x3E * bank + 0x3B16 + room)
    hook = [("B", 2 * bank + 0x2FA9), ("B", 2 * bank + 0x3FFB), ("B", 2 * bank + 0x3D00)]
    castle = [("B", 2 * bank + 0x3831), ("B", 0x15 * bank + 0x00E9)]
    keyhole = lambda room, reward: loc("keyhole", "KeyHole(0x%03X, %r).patch(rom, \"SWORD\")"
                                       % (room, reward))
    calls = [
        (loc("beachSword", "BeachSword().patch(rom, \"SWORD\")"),
         [item(0xF2), ("T", "entities", 0xF2), ("T", "rooms_overworld_bottom", 0x72),
          ("B", 0x27 * bank + (0xF2 - 0xCC) * 80)]),
        (loc("startItem", "StartItem().patch(rom, \"SWORD\")"), [item(0x2A3)]),
        (loc("chest", "Chest(0x1B6).patch(rom, \"SWORD\")"), [("B", 0x14 * bank + 0x1B6 + 0x560)]),
        (loc("boomerangGuy", "BoomerangGuy().patch(rom, \"HOOKSHOT\")"),
         [("T", "texts", t) for t in (0x222, 0x224, 0x225, 0x226)]),
        ("(lambda g: (setattr(g, \"setting\", \"gift\"), g.patch(rom, \"HOOKSHOT\")))(%s)"
         % loc("boomerangGuy", "BoomerangGuy()"),
         [("T", "texts", 0x221), ("T", "texts", 0x225), ("B", 0x19 * bank + 0x072B)]),
        (keyhole(0x2C3, "CASTLE_GATE_OPENED"), [keyhole_item(0x2C3)] + castle),
        (keyhole(0x2C3, "CASTLE_GATE_OPENED"), [keyhole_item(0x2C3)] + hook),
    ]
    for room, reward in ((0x0D3, "TAIL_CAVE_OPENED"), (0x0B5, "KEY_CAVERN_OPENED"),
                         (0x02B, "ANGLER_TUNNEL_OPENED"), (0x08C, "FACE_SHRINE_OPENED"),
                         (0x00E, "EAGLE_TOWER_OPENED")):
        calls.append((keyhole(room, reward), [keyhole_item(room)] + hook + [("B", 2 * bank + 0x2FAA)]))
    return calls


def _bosses_calls():
    """bosses' calls: changeMiniBosses' fixed patches; changeBosses' genie patch and eagle check
    (identity); its fish and eagle flags and facade patches (a mapping placing all three)."""
    bank = BANK_SIZE
    return [
        ("patches.enemies.changeMiniBosses(rom, {})", []),
        ("patches.enemies.changeBosses(rom, list(range(9)))", [],
         [(4 * bank + 0x0521, 2), (2 * bank + 0x1FC8, 2)]),
        ("patches.enemies.changeBosses(rom, [3, 6, 5, 0, 4, 2, 1, 7, 8])", [],
         [(3 * bank + 0x1A0F, 3), (3 * bank + 0x1A04, 3), (4 * bank + 0x121D, 2),
          (4 * bank + 0x1226, 2), (4 * bank + 0x127F, 2)]),
    ]


def variant_runs(module, call):
    """[call()'s result], one per element of the sequence call draws from module.choice, the
    k-th with that choice returning the k-th element; module.choice restored after.
    TablesPlanError when a run does not draw exactly once."""
    def pinned(k):
        seen = []

        def choice(seq):
            seen.append(len(seq))
            return seq[min(k, len(seq) - 1)]
        missing = object()
        before = vars(module).get("choice", missing)
        module.choice = choice
        try:
            result = call()
        finally:
            if before is missing:
                del module.choice
            else:
                module.choice = before
        if len(seen) != 1:
            raise TablesPlanError("the call draws %d times, not once" % len(seen))
        return result, seen[0]
    first, n = pinned(0)
    return [first] + [pinned(k)[0] for k in range(1, n)]


CONST_GROUPS = {
    "bank3e": ConstSpec("patches.bank3e.addBank3E(rom, None, None)",
                        ["asm:HARD_MODE", "asm:KEYSANITY_BEAKS", "asm:KEYSANITY_KEYS",
                         "asm:KEYSANITY_MAPS", "asm:KEYSANITY_NIGHTMARE_KEYS"],
                        [("T", "texts", 0xBE)]),
    "endscreen": ConstSpec("patches.endscreen.updateEndScreen(rom)", [], [],
                           variants="random"),
    "goal": ConstSpec(None, [], [], calls=(
        [("patches.maze.patchMaze(rom, [], [])", _MAZE_ROOM6)]
        + [("patches.goal.setRequiredInstrumentCount(rom, %d)" % c, _GOAL_TEXT)
           for c in range(-1, 8)]
        + [("patches.goal.setSpecificInstruments(rom, %r)" % lst, _GOAL_TEXT)
           for lst in _specific_lists()])),
    "items": ConstSpec(None, [], [], calls=_items_calls()),
    "bosses": ConstSpec(None, [], [], calls=_bosses_calls()),
    "enemizer": ConstSpec(None, [], [], calls=[
        ("patches.enemies.randomizeEnemies(rom, bytes(16))", [],
         [(0x36 * 0x4000 + 0x23DD, 3), (0x36 * 0x4000 + 0x3F00, 16)])]),
}
ASM_CONSTS_GROUP = "asm-consts"
BANK_SIZE = 0x4000


def _const_statements(inventory_text):
    """generateRom's asm-consts rows (resetConsts and every assembler.const), in order."""
    sections, bad = inventory_check.parse_inventory(inventory_text)
    if bad:
        raise TablesPlanError("the inventory does not parse: %s" % bad[0])
    rows = sections.get(inventory_check.section_name("generator.py", "generateRom"), [])
    return [r["key"] for r in rows if r["Group"] == ASM_CONSTS_GROUP]


def _recorded(rom, table_names, events):
    """rom with every bank byte and table entry it is given recorded into events, as
    ("B", absolute offset, bytes) and ("T", table, key, bytes or int)."""
    class Bank(bytearray):
        def __setitem__(self, key, value):
            if isinstance(key, slice):
                start, stop, step = key.indices(len(self))
                data = bytes(value)
                if step != 1 or len(data) != stop - start:
                    raise TablesPlanError("a bank write that is not one contiguous run")
                events.append(("B", self.base + start, data))
            else:
                events.append(("B", self.base + key, bytes([value])))
            super().__setitem__(key, value)

    for n, data in enumerate(rom.banks):
        rom.banks[n] = Bank(data)
        rom.banks[n].base = n * BANK_SIZE
    for name in table_names:
        table = getattr(rom, name)
        base = type(table)

        def setitem(self, key, value, base=base, name=name):
            events.append(("T", name, key, value if isinstance(value, int) else bytes(value)))
            base.__setitem__(self, key, value)
        table.__class__ = type("Recorded" + base.__name__, (base,), {"__setitem__": setitem})
    return rom


def const_runs(spec, model, statements, rom_path, table_names):
    """[(tuple, writes)] of spec's call, one per reachable tuple of its components, each on a
    fresh parse of the ROM after generateRom's asm-consts statements on the tuple's witness
    (validated by LADXR); run inside the LADXR checkout. Every rom.patch's old bytes must
    match: a patch LADXR skips as already applied is refused. With spec.variants, one run per
    element its call draws, in order (variant_runs)."""
    import assembler
    import generator
    import romTables
    import settings as ladxr_settings

    found, unreached = reachable(spec.components, model)
    if unreached:
        raise TablesPlanError("unreached tuples %s" % unreached)

    def one(witness, call=spec.call):
        settings = ladxr_settings.Settings()
        for line in assignment_lines(witness):
            settings.set(line)
        with contextlib.redirect_stdout(io.StringIO()):
            settings.validate()
        namespace = dict(vars(generator), settings=settings, assembler=assembler)
        for statement in statements:
            exec(statement, namespace)
        events = []
        with open(rom_path, "rb") as f:
            rom = _recorded(romTables.ROMWithTables(f), table_names, events)
        namespace["rom"] = rom
        skipped = []
        patch = rom.patch

        def checked(bank, addr, old, new, **kw):
            before = len(events)
            patch(bank, addr, old, new, **kw)
            if len(events) == before:
                skipped.append((bank, addr))
        rom.patch = checked
        with contextlib.redirect_stdout(io.StringIO()):
            eval(call, namespace)
        if skipped:
            raise TablesPlanError("%s: patches skipped as already applied at %s"
                                  % (call, skipped))
        return events

    if spec.calls:
        (values, witness), = found
        runs = []
        for c in spec.calls:
            runs.append(one(witness, c[0]))
        return runs
    runs = []
    for values, witness in found:
        if spec.variants:
            module = importlib.import_module(spec.variants)
            runs += [(tuple(values), events)
                     for events in variant_runs(module, lambda: one(witness))]
        else:
            runs.append((tuple(values), one(witness)))
    return runs


def const_files(model, inventory_text, rom_path, rom, known, table_names):
    """{<group>.txt: text} of CONST_GROUPS, inside the LADXR checkout."""
    statements = _const_statements(inventory_text)
    files = {}
    for group, spec in sorted(CONST_GROUPS.items()):
        runs = const_runs(spec, model, statements, rom_path, table_names)
        if spec.calls:
            files["%s.txt" % group] = calls_group(group, table_names, runs,
                                                  [c[1] for c in spec.calls], rom, known,
                                                  [c[2] if len(c) > 2 else None for c in spec.calls])
        else:
            files["%s.txt" % group] = const_group(group, table_names, spec.components, runs,
                                                  spec.own, rom, known,
                                                  variants=spec.variants is not None)
    return files


# Building ----------------------------------------------------------------------------------------

def _plan_state(inventory, done, facts, widenings, fusions):
    """(units, {site: unit id}, keys, FUSE-ACROSS and DEPENDS-C lines) of the runs done."""
    coupled = sorted({c for _, run in done for c in run.couplings})
    c_rows = {r.site for r in inventory.rows if r.side == "C"}
    unit_list, problems = units(inventory.rows, [c for c in coupled if c[2] == "match"],
                                fusions, {s for s in facts.targets if s in c_rows})
    keys, depends = unit_keys(unit_list, inventory, facts, widenings,
                              [c for c in coupled if c[2] == "rewrite"])
    return unit_list, {s: u.id for u in unit_list for s in u.rows}, keys, problems + depends


def _key_line(kind, unit, values):
    return " ".join([kind, unit] + list(values))


def generate(inventory, model, widenings, fusions, runner, rom, known=()):
    """(files {name: text}, problem lines): data/ladx/'s files built from coverage runs, planned
    (plan_runs) on the keys the runs done so far give, until a plan asks for no new run.
    runner(name, assignment) runs one (names run-00, run-01, ... in order) and gives (Events,
    reads.txt's text, the key trace's text), or None when no seed is ok. Problems: FUSE-ACROSS,
    DEPENDS-C, SPLIT, CONFLICT and UNREACHED lines, sorted."""
    done, failed, facts = [], [], Facts()
    while True:
        unit_list, unit_of, keys, problems = _plan_state(inventory, done, facts, widenings,
                                                         fusions)
        if problems:
            break
        planned, _ = plan_runs(keys, model, [a for a, _ in done], failed)
        new = [r.assignment for r in planned[len(done):] if r.assignment not in failed]
        if not new:
            break
        for assignment in new:
            got = runner("run-%02d" % (len(done) + len(failed)), assignment)
            if got is None:
                failed.append(assignment)
                continue
            events, reads, keys_text = got
            facts.add_events(events)
            facts.add_reads(reads)
            facts.add_keys(keys_text)
            done.append((assignment, encode_run(events, rom, inventory.sides, known)))
    if not done:
        raise TablesPlanError("no coverage run is ok")
    order = {t for _, run in done for t in [tuple(run.tables)]}
    if len(order) != 1:
        raise TablesPlanError("the coverage runs disagree on the table order")
    entries = {u.id: {} for u in unit_list}
    for assignment, run in done:
        if any(s not in unit_of for _, writes in run.segments for _, s in writes.values()):
            raise TablesPlanError("a table group's write comes from no table row")
        got, split = run.entries(unit_of)
        problems += split
        for unit in unit_list:
            t = model.key(keys[unit.id], assignment)
            if t is None:
                continue
            lines = got.get(unit.id, [])
            if entries[unit.id].setdefault(t, lines) != lines:
                problems.append(_key_line("CONFLICT", unit.id, t))
    for unit in unit_list:
        found, unreached = reachable(keys[unit.id], model)
        problems += [_key_line("UNREACHED", unit.id, t)
                     for t in [t for t, _ in found] + unreached if t not in entries[unit.id]]
    files = {"order.txt": tables.render(tables.Order(inventory.sections, [
        tables.Row(r.site.split("\t")[0], r.side, r.group,
                   unit_of[r.site] if r.side == "table" else "-", _call(r.site))
        for r in inventory.rows]))}
    table_order = list(order.pop())
    for group in sorted({u.group for u in unit_list}):
        files[group + ".txt"] = tables.render(tables.Group(group, table_order, [
            tables.Unit(u.id, keys[u.id], [tables.Entry(list(t), lines, {}, {})
                                           for t, lines in sorted(entries[u.id].items())])
            for u in unit_list if u.group == group]))
    return files, sorted(set(problems))


# The command line --------------------------------------------------------------------------------

TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))


def _text(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def oracle_runner(ladxr, rom, inventory_path, runs_dir, work):
    """runner for generate: oracle_run.py on the run's first ok candidate seed
    derive("ladx-tables <run>", k), its outputs kept in <runs_dir>/<run>/."""
    import shutil
    import subprocess

    def run(name, assignment):
        folder = os.path.join(runs_dir, name)
        for k in range(ladx_cases.BOUND):
            shutil.rmtree(folder, ignore_errors=True)
            seed = ladx_cases.derive("ladx-tables " + name, k).hex()
            subprocess.run([sys.executable, "-B", os.path.join(HERE, "oracle_run.py"), "--rom", rom,
                            "--dir", folder, "--work", work, "--reads", "--keys", "--no-invariant",
                            "--inventory", inventory_path, ladxr, seed]
                           + assignment_lines(assignment), check=True, stdin=subprocess.DEVNULL)
            if _text(os.path.join(folder, "run.txt")).endswith("\noutcome ok\n"):
                print("%s: k=%d %s" % (name, k, " ".join(assignment_lines(assignment))),
                      flush=True)
                return (write_log.parse_events(_text(os.path.join(folder, "events.txt"))),
                        _text(os.path.join(folder, "reads.txt")),
                        _text(os.path.join(folder, "keys.txt")))
        shutil.rmtree(folder, ignore_errors=True)
        print("%s: no ok seed below %d %s" % (name, ladx_cases.BOUND,
                                              " ".join(assignment_lines(assignment))), flush=True)
        return None
    return run


def build(args):
    import time
    start = time.time()
    sys.path.insert(0, HERE)
    import known_source
    import oracle_run
    inputs = lambda name: _text(os.path.join(args.inputs, name))
    settings_text = inputs("settings.txt")
    keys = [s.key for s in ladx_cases.read_settings(settings_text)]
    inventory = read_inventory(_text(args.inventory), keys)
    widenings = read_widenings(inputs("table_keys.txt"), inventory)
    fusions = read_fusions(inputs("table_units.txt"), inventory)
    with open(args.rom, "rb") as f:
        rom = f.read()
    known = known_source.build(rom)
    runs_dir, out = os.path.abspath(args.runs), os.path.abspath(args.out)
    work = os.path.join(runs_dir, "work")
    os.makedirs(runs_dir, exist_ok=True)
    driver = oracle_run.Driver(args.ladxr, work)
    try:
        import settings as ladxr_settings
        import locations.keyLocation as key_location
        model = build_model(inventory, inputs("tier1.txt"), settings_text, inputs("combos.txt"),
                            inputs("settings_validate.txt"),
                            settings_validate(ladxr_settings.Settings), None,
                            inventory.group_keys[COUNT_GROUP])
        model._count = spot_count(driver, key_location.KeyLocation, model.defaults)
        runner = oracle_runner(os.path.abspath(args.ladxr), os.path.abspath(args.rom),
                               os.path.abspath(args.inventory), runs_dir, work)
        calls = []
        files, problems = generate(inventory, model, widenings, fusions,
                                   lambda n, a: calls.append(n) or runner(n, a), rom, known)
        if not problems:
            files.update(const_files(model, _text(args.inventory), os.path.abspath(args.rom), rom,
                                     known, [t.name for t in known_source.LAYOUT]))
    finally:
        driver.close()
    for line in problems:
        print(line)
    if problems:
        return 1
    os.makedirs(out, exist_ok=True)
    for name in sorted(os.listdir(out)):
        if name.endswith(".txt") and name not in files:
            os.remove(os.path.join(out, name))
    for name, text in sorted(files.items()):
        with open(os.path.join(out, name), "w", encoding="ascii", newline="\n") as f:
            f.write(text)
    print("gen_tables: %d coverage runs in %d s, %d files written to %s"
          % (len(calls), time.time() - start, len(files), args.out))
    return 0


def const(args):
    """Writes CONST_GROUPS' files into <out>, replacing only them."""
    import time
    start = time.time()
    sys.path.insert(0, HERE)
    import known_source
    inputs = lambda name: _text(os.path.join(args.inputs, name))
    settings_text = inputs("settings.txt")
    inventory_text = _text(args.inventory)
    inventory = read_inventory(inventory_text, [s.key for s in ladx_cases.read_settings(settings_text)])
    rom_path, out = os.path.abspath(args.rom), os.path.abspath(args.out)
    with open(rom_path, "rb") as f:
        rom = f.read()
    known = known_source.build(rom)
    cwd = os.getcwd()
    work = ladx_oracle.enter_checkout(args.ladxr, os.path.join(TOP, "tmp", "ladx-tables"))
    try:
        import settings as ladxr_settings
        model = build_model(inventory, inputs("tier1.txt"), settings_text, inputs("combos.txt"),
                            inputs("settings_validate.txt"),
                            settings_validate(ladxr_settings.Settings), None, [])
        files = const_files(model, inventory_text, rom_path, rom, known,
                            [t.name for t in known_source.LAYOUT])
    finally:
        os.chdir(cwd)
        import shutil
        shutil.rmtree(work, ignore_errors=True)
    os.makedirs(out, exist_ok=True)
    for name, text in sorted(files.items()):
        with open(os.path.join(out, name), "w", encoding="ascii", newline="\n") as f:
            f.write(text)
    print("gen_tables: %d C-group files in %d s written to %s" % (len(files), time.time() - start,
                                                                   args.out))
    return 0


SAMPLE_SIZE = 12


def sample(args):
    """Appends the SAMPLE_SIZE sample lines to <inputs>/combos.txt, validated by LADXR's live
    Settings.validate; prints them."""
    path = os.path.join(args.inputs, "combos.txt")
    combos_text = _text(path)
    inputs = lambda name: _text(os.path.join(args.inputs, name))
    cwd = os.getcwd()
    work = ladx_oracle.enter_checkout(args.ladxr, os.path.join(TOP, "tmp", "ladx-tables"))
    try:
        import settings as ladxr_settings
        model = build_model(read_inventory(_text(args.inventory), [
            s.key for s in ladx_cases.read_settings(inputs("settings.txt"))]),
            inputs("tier1.txt"), inputs("settings.txt"), combos_text,
            inputs("settings_validate.txt"), settings_validate(ladxr_settings.Settings),
            None, [])
        lines = sample_lines(model, SAMPLE_SIZE)
    finally:
        os.chdir(cwd)
        import shutil
        shutil.rmtree(work, ignore_errors=True)
    text = with_sample(combos_text, lines)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("\n".join(lines))
    print("gen_tables: %d sample lines written to %s" % (len(lines), path))
    return 0


def main(argv):
    import argparse
    parser = argparse.ArgumentParser(prog="gen_tables.py")
    sub = parser.add_subparsers(dest="mode", required=True)
    q = sub.add_parser("sample")
    q.add_argument("--ladxr", default=os.path.join(HERE, "ladxr"))
    q.add_argument("--inventory", default=os.path.join(TOP, "notes", "ladx-inventory.md"))
    q.add_argument("--inputs", default=HERE)
    p = sub.add_parser("build")
    p.add_argument("--out", required=True)
    p.add_argument("--runs", required=True)
    p.add_argument("--ladxr", default=os.path.join(HERE, "ladxr"))
    p.add_argument("--rom", default=os.path.join(TOP, "tmp", "ladx", "base.gbc"))
    p.add_argument("--inventory", default=os.path.join(TOP, "notes", "ladx-inventory.md"))
    p.add_argument("--inputs", default=HERE)
    c = sub.add_parser("const")
    c.add_argument("--out", required=True)
    c.add_argument("--ladxr", default=os.path.join(HERE, "ladxr"))
    c.add_argument("--rom", default=os.path.join(TOP, "tmp", "ladx", "base.gbc"))
    c.add_argument("--inventory", default=os.path.join(TOP, "notes", "ladx-inventory.md"))
    c.add_argument("--inputs", default=HERE)
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    try:
        return {"sample": sample, "build": build, "const": const}[args.mode](args)
    except (TablesPlanError, write_log.WriteLogError, ladx_oracle.OracleError, OSError) as e:
        print("gen_tables: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
