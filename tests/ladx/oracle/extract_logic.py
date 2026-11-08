"""Usage: python3 extract_logic.py <ladxr dir> <out dir>
       python3 extract_logic.py --graph <config> --out <dir> <ladxr dir>

Writes <out dir>/logic_graphs.txt and <out dir>/logic_vectors.txt from LADXR's Logic, built with
no ROM. A configuration is a logic value plus at most one other tier-1 value (tier1.txt); its name
is `logic=<v>/baseline` or `logic=<v>/<key>=<value>`. --graph writes the full canonical graph of
one configuration (any setting, tier-1 or not) to <dir>/graph.txt, for a line diff against a
digest of logic_vectors.txt.
"""

import contextlib
import hashlib
import io
import os
import random
import shutil
import sys

import ladx_oracle

HERE = os.path.dirname(os.path.abspath(__file__))
WORK_ROOT = os.path.join(HERE, "..", "..", "..", "tmp", "ladx-oracle")
SEED = bytes(range(16))

GRAPHS_HEADER = """\
# LADXR's logic graph of the baseline under each logic value; written by extract_logic.py
# (gen_fixtures.sh). Settings set in order then validated, world setup as in logic_vectors.txt.
# Each graph starts with
#   graph <config name>
# then its canonical text, whose SHA-512 (these lines after the graph line, each ending in LF) is
# the config's graph line in logic_vectors.txt:
#   loc <i> <dungeon or ->\\t<name or ->            Logic.location_list, in its (discovery) order
#   spot <loc> <class> <nameId> <options or ->\\t<metadata name>\\t<metadata area>
#                                                  Logic.iteminfo_list in order, options after configure
#   edge <from loc> <to loc> <requirement>         each location's connections, in order
# <requirement>: - (None), T, F, an item, and(...)/or(...) (LADXR's stored order: items, then
# children), count(ITEM,n), counts(n,ITEM,...), found(ITEM,n).
"""

VECTORS_HEADER = """\
# LADXR's logic per configuration; written by extract_logic.py (gen_fixtures.sh). Configurations:
# for each logic value, the baseline, then every non-default value of every other tier-1 setting
# alone (tier1.txt, LADXR's order). For each:
#   config <name> <k=v,...>          name logic=<v>/baseline or logic=<v>/<key>=<value>; the settings
#                                    set in that order on a new Settings, then validate
#   ws <field> <value>               every field of WorldSetup().randomize(settings, rnd), rnd =
#                                    random.Random(bytes.fromhex("%s"));
#                                    entrance_mapping is the identity on tier 1. Logic is built
#                                    from it after random.setstate(rnd.getstate()) and draws
#                                    nothing from either generator
#   graph <SHA-512 hex>              of the canonical graph text (logic_graphs.txt's format)
#   explore <inventory> -> <reached>\\t<required next>
#                                    a new Explorer, addItem(item, count) per inventory entry,
#                                    then visit(logic.start): <reached> the location_list indexes
#                                    of getAccessableLocations() as ranges (0-2,5), <required next>
#                                    getRequiredItemsForNextLocations() sorted, comma-separated
#                                    (- when empty). Inventories, item=count,... (- when empty), in
#                                    order: empty; the configuration's whole ItemPool (built with
#                                    rnd after randomize, zero counts left out); then %s
#                                    of its items (rounded down), drawn from that pool by four
#                                    successive sample calls on one generator
#                                    random.Random(bytes.fromhex("%s")),
#                                    written in pool order.
"""

PARTIAL = ((1, 8), (1, 4), (1, 2), (3, 4))


def configs(tier1, settings):
    defaults = {s.key: ladx_oracle.setting_value(s.default) for s in settings}
    logic_values = dict(tier1)["logic"]

    def assign(key, value):
        return "%s=%s" % (key, "" if value == '""' else value)
    for logic in logic_values:
        base = "logic=%s" % logic
        yield base + "/baseline", base, [assign("logic", logic)]
        for key, values in tier1:
            if key == "logic":
                continue
            for value in values:
                if value != defaults[key]:
                    yield ("%s/%s=%s" % (base, key, value), "%s,%s=%s" % (base, key, value),
                           [assign("logic", logic), assign(key, value)])


def inventory_text(inventory):
    return ",".join("%s=%d" % (ladx_oracle.item_name(k), v) for k, v in inventory.items()) or "-"


class Extractor:
    def __init__(self, ladxr_dir):
        self.cwd = os.getcwd()
        self.work = ladx_oracle.enter_checkout(ladxr_dir, os.path.abspath(WORK_ROOT))
        import settings
        import worldSetup
        import itempool
        import explorer
        import logic.main
        from patches import bingo
        self.settings, self.worldSetup, self.itempool, self.explorer = settings, worldSetup, itempool, explorer
        self.Logic = logic.main.Logic
        self.identity = worldSetup.WorldSetup().entrance_mapping
        self.bingo_goals = bingo.BINGO_GOALS

    def close(self):
        os.chdir(self.cwd)
        shutil.rmtree(self.work, ignore_errors=True)

    def build(self, assignments):
        settings = self.settings.Settings()
        for assignment in assignments:
            settings.set(assignment)
        with contextlib.redirect_stdout(io.StringIO()):
            settings.validate()
        rnd = random.Random(SEED)
        ws = self.worldSetup.WorldSetup()
        ws.randomize(settings, rnd)
        ws_lines = list(ladx_oracle.ws_lines(ws, self.identity, self.bingo_goals))
        random.setstate(rnd.getstate())
        before = (rnd.getstate(), random.getstate())
        logic = self.Logic(settings, world_setup=ws)
        if (rnd.getstate(), random.getstate()) != before:
            raise ladx_oracle.OracleError("oracle: Logic drew from a generator with %s" % assignments)
        return settings, ws_lines, logic, rnd

    def explore(self, logic, inventory):
        e = self.explorer.Explorer()
        for item, count in inventory.items():
            e.addItem(item, count)
        e.visit(logic.start)
        index = {id(loc): i for i, loc in enumerate(logic.location_list)}
        reached = ladx_oracle.index_ranges(index[id(loc)] for loc in e.getAccessableLocations())
        needed = ",".join(ladx_oracle.item_name(i) for i in sorted(e.getRequiredItemsForNextLocations()))
        return "explore %s -> %s\t%s" % (inventory_text(inventory), reached, needed or "-")

    def vectors(self, name, shown, assignments):
        settings, ws_lines, logic, rnd = self.build(assignments)
        if ws_lines[0] != "ws entrance_mapping identity":
            raise ladx_oracle.OracleError("oracle: %s: the entrance mapping is not the identity" % name)
        text = "".join(line + "\n" for line in ladx_oracle.graph_lines(logic))
        pool = self.itempool.ItemPool(logic, settings, rnd, False).toDict()
        pool = {item: count for item, count in pool.items() if count > 0}
        bag = [item for item, count in pool.items() for _ in range(count)]
        draw = random.Random(SEED)
        inventories = [{}, pool]
        for num, den in PARTIAL:
            picked = draw.sample(bag, len(bag) * num // den)
            inventories.append({item: picked.count(item) for item in pool if item in picked})
        lines = ["config %s %s" % (name, shown)] + ws_lines
        lines.append("graph " + hashlib.sha512(text.encode("utf-8")).hexdigest())
        lines += [self.explore(logic, inventory) for inventory in inventories]
        return lines

    def graph(self, assignments):
        return list(ladx_oracle.graph_lines(self.build(assignments)[2]))


def read_tier1(extractor):
    with open(os.path.join(HERE, "tier1.txt"), encoding="utf-8") as f:
        settings = list(extractor.settings.Settings())
        return ladx_oracle.read_tier1(f.read(), settings), settings


def parse_config(name):
    logic, _, rest = name.partition("/")
    if not logic.startswith("logic=") or not rest:
        raise ladx_oracle.OracleError("oracle: config %r is not logic=<v>/baseline or logic=<v>/<k>=<v>,..."
                                      % name)
    parts = [logic] + ([] if rest == "baseline" else rest.split(","))
    return [p.replace('""', "") for p in parts]


def write(path, header, lines):
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(header)
        out.writelines(line + "\n" for line in lines)


def main(argv):
    graph = out = None
    args = argv[1:]
    if args[:1] == ["--graph"] and len(args) == 5 and args[2] == "--out":
        graph, out, args = args[1], args[3], args[4:]
    if len(args) != (1 if graph else 2):
        print(__doc__.splitlines()[0], file=sys.stderr)
        return 2
    out = os.path.abspath(out if graph else args[1])
    extractor = Extractor(args[0])
    try:
        if graph:
            os.makedirs(out, exist_ok=True)
            write(os.path.join(out, "graph.txt"), "", extractor.graph(parse_config(graph)))
            print("extract_logic: %s written to %s" % (graph, os.path.join(out, "graph.txt")))
            return 0
        tier1, settings = read_tier1(extractor)
        graphs, vectors = [], []
        for name, shown, assignments in configs(tier1, settings):
            if name.endswith("/baseline"):
                graphs += ["graph " + name] + extractor.graph(assignments)
            vectors += extractor.vectors(name, shown, assignments)
    finally:
        extractor.close()
    write(os.path.join(out, "logic_graphs.txt"), GRAPHS_HEADER, graphs)
    write(os.path.join(out, "logic_vectors.txt"),
          VECTORS_HEADER % (SEED.hex(), ", ".join("%d/%d" % p for p in PARTIAL), SEED.hex()), vectors)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except ladx_oracle.OracleError as e:
        sys.exit(str(e))
