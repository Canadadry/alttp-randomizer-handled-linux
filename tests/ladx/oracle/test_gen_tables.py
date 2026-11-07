"""Usage: python3 -I test_gen_tables.py

Unit tests of gen_tables.py's ROM-free planning (units, keys, reachability, coverage runs) on
planted event streams, inventories and models.
"""

import importlib.util
import os
import random
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("gen_tables", os.path.join(HERE, "gen_tables.py"))
gen_tables = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gen_tables)
write_log = gen_tables.write_log

SECTION = "generator.py generateRom"
KNOWN = write_log.KNOWN


def site(call):
    return "%s\t%s" % (SECTION, call)


def bank(call, offset, values, group="fixed", origins=None):
    return ("B", group, site(call), offset, bytes(values),
            tuple(origins) if origins else (KNOWN,) * len(values))


def entry(call, key, values, group="fixed", table="texts"):
    return ("T", group, site(call), table, key, bytes(values), (KNOWN,) * len(values))


ROM = bytearray(random.Random(6).randbytes(0x1000))
SIDES = {"fixed": "table", "opts": "table", "items": "C"}


def stream(*events):
    return write_log.Events(["texts"], [("texts", 0, b"\x01\x02", (KNOWN, KNOWN))], list(events))


class Coupling(unittest.TestCase):

    def couplings(self, *events):
        return gen_tables.couplings(stream(*events), bytes(ROM), SIDES)

    def test_a_source_match_spanning_two_rows_couples_them(self):
        got = self.couplings(bank("a(rom)", 0x100, ROM[0x800:0x803]),
                             bank("b(rom)", 0x103, ROM[0x803:0x806]))
        self.assertEqual(got, [(site("a(rom)"), site("b(rom)"), "match")])

    def test_rows_whose_matches_stay_apart_are_not_coupled(self):
        self.assertEqual(self.couplings(bank("a(rom)", 0x100, ROM[0x800:0x806]),
                                        bank("b(rom)", 0x106, ROM[0x900:0x906]),
                                        bank("c(rom)", 0x10c, b"\x00\x00")), [])

    def test_a_row_rewriting_another_rows_byte_in_the_segment_couples_them(self):
        got = self.couplings(bank("a(rom)", 0x100, b"\x01\x02"), bank("b(rom)", 0x101, b"\x03"),
                             entry("c(rom)", 0, b"\x05"), entry("d(rom)", 0, b"\x06"))
        self.assertEqual(got, [(site("a(rom)"), site("b(rom)"), "rewrite"),
                               (site("c(rom)"), site("d(rom)"), "rewrite")])

    def test_rows_in_two_segments_are_not_coupled(self):
        self.assertEqual(self.couplings(
            bank("a(rom)", 0x100, ROM[0x800:0x803]), bank("x(rom)", 0x400, b"\x00", "items"),
            bank("b(rom)", 0x103, ROM[0x803:0x806]), bank("b(rom)", 0x100, b"\x07")), [])

    def test_c_groups_are_never_coupled(self):
        self.assertEqual(self.couplings(bank("a(rom)", 0x100, b"\x01", "items"),
                                        bank("b(rom)", 0x100, b"\x02", "items")), [])


class RunEntries(unittest.TestCase):

    UNITS = {site("a(rom)"): "fixed.1", site("b(rom)"): "fixed.2", site("c(rom)"): "fixed.3"}

    def entries(self, *events, units=UNITS):
        return gen_tables.encode_run(stream(*events), bytes(ROM), SIDES).entries(units)

    def test_each_unit_gets_its_net_records_cut_from_its_segment(self):
        got = self.entries(bank("a(rom)", 0x100, ROM[0x800:0x806]),
                           bank("b(rom)", 0x106, b"\x00\x01"), entry("b(rom)", 1, ROM[0x900:0x905]),
                           bank("x(rom)", 0x400, b"\x00", "items"),
                           bank("c(rom)", 0x200, ROM[0x200:0x202] + b"\x07"),
                           bank("c(rom)", 0x202, b"\x08"))
        self.assertEqual(got, ({"fixed.1": ["B 100 @800+6"],
                                "fixed.2": ["B 106 0001", "T texts 1 @900+5"],
                                "fixed.3": ["B 200 =2 08"]},
                               []))

    def test_a_fused_units_rows_share_the_segments_encoding(self):
        run = gen_tables.encode_run(stream(bank("a(rom)", 0x100, ROM[0x800:0x803]),
                                           bank("b(rom)", 0x103, ROM[0x803:0x806])),
                                    bytes(ROM), SIDES)
        self.assertEqual(run.entries({site("a(rom)"): "fixed.1", site("b(rom)"): "fixed.1"}),
                         ({"fixed.1": ["B 100 @800+6"]}, []))
        self.assertEqual(run.couplings, [(site("a(rom)"), site("b(rom)"), "match")])

    def test_a_unit_writing_in_two_segments_of_a_run_is_split(self):
        _, problems = self.entries(bank("b(rom)", 0x100, b"\x01"),
                                   bank("x(rom)", 0x400, b"\x00", "items"),
                                   bank("c(rom)", 0x101, b"\x02"),
                                   units=dict(self.UNITS, **{site("c(rom)"): "fixed.2"}))
        self.assertEqual(problems, ["SPLIT fixed.2"])


def rows(*specs):
    """Rows of `<side> <group> <call>` specs, in order."""
    out = []
    for spec in specs:
        side, group, call = spec.split(" ", 2)
        out.append(gen_tables.Row(site(call), side, group, []))
    return out


ORDER = rows("table fixed a(rom)", "C items b(rom)", "table fixed c(rom)", "table fixed d(rom)",
             "table opts e(rom)", "table fixed f(rom)", "table fixed g(rom)", "C items h(rom)",
             "table fixed i(rom)")


class Units(unittest.TestCase):

    def units(self, pairs, forced=(), writing=()):
        got, problems = gen_tables.units(ORDER, [(site(a), site(b), "match") for a, b in pairs],
                                         [(site(a), site(b), "why") for a, b in forced],
                                         {site(c) for c in writing})
        return [(u.id, [r.split("\t")[1] for r in u.rows]) for u in got], problems

    def test_every_table_row_is_its_own_unit_numbered_per_group(self):
        self.assertEqual(self.units([]), ([
            ("fixed.1", ["a(rom)"]), ("fixed.2", ["c(rom)"]), ("fixed.3", ["d(rom)"]),
            ("opts.1", ["e(rom)"]), ("fixed.4", ["f(rom)"]), ("fixed.5", ["g(rom)"]),
            ("fixed.6", ["i(rom)"])], []))

    def test_coupled_rows_fuse_transitively_with_the_rows_between_them(self):
        got, problems = self.units([("f(rom)", "g(rom)"), ("g(rom)", "i(rom)")])
        self.assertEqual(got[-1], ("fixed.4", ["f(rom)", "g(rom)", "i(rom)"]))
        self.assertEqual(problems, [])
        got, _ = self.units([], forced=[("c(rom)", "d(rom)")])
        self.assertEqual(got[1], ("fixed.2", ["c(rom)", "d(rom)"]))
        got, _ = self.units([("a(rom)", "d(rom)")])
        self.assertEqual(got[0], ("fixed.1", ["a(rom)", "c(rom)", "d(rom)"]))

    def test_a_fusion_across_a_writing_c_row_or_another_group_is_refused(self):
        _, problems = self.units([("a(rom)", "c(rom)"), ("g(rom)", "i(rom)")],
                                 writing=["b(rom)"])
        self.assertEqual(problems, ["FUSE-ACROSS fixed.1 b(rom)"])
        _, problems = self.units([("d(rom)", "f(rom)")])
        self.assertEqual(problems, ["FUSE-ACROSS fixed.3 e(rom)"])


SETTINGS = ["race", "textmode", "hardmode", "lowhpbeep", "goal", "witch"]
KEY_ORDER = [
    gen_tables.Row(site("a(rom)"), "table", "fixed", []),
    gen_tables.Row(site("b(rom, settings)"), "table", "opts", ["witch"]),
    gen_tables.Row(site("c(rom)"), "table", "fixed", []),
    gen_tables.Row(site("d(rom)"), "C", "items", []),
    gen_tables.Row(site("e(rom)"), "table", "goal-fixed", []),
    gen_tables.Row(site("f(rom, n)"), "table", "frame-counter", ["race"]),
    gen_tables.Row(site("g(rom)"), "table", "opts", ["lowhpbeep", "hardmode"]),
]
GROUP_KEYS = {"fixed": [], "opts": ["hardmode", "lowhpbeep", "witch"], "goal-fixed": ["goal"],
              "frame-counter": ["goal", "witch"]}
CONSTANTS = {"ITEM_DIALOGS": ["textmode"]}
GROUP_SIDES = {"fixed": "table", "opts": "table", "goal-fixed": "table",
               "frame-counter": "table", "items": "C"}


class Keys(unittest.TestCase):

    def keys(self, facts=None, widenings=(), rewrites=()):
        units, _ = gen_tables.units(KEY_ORDER, [], [], set())
        model = gen_tables.Inventory(KEY_ORDER, GROUP_KEYS, GROUP_SIDES, CONSTANTS, SETTINGS)
        facts = facts or gen_tables.Facts()
        got, problems = gen_tables.unit_keys(units, model, facts,
                                             [(site(c), k, "why") for c, k in widenings],
                                             [(site(a), site(b), "rewrite") for a, b in rewrites])
        return {u: " ".join(c) for u, c in got.items()}, problems

    def test_a_row_is_keyed_by_its_settings_column_else_its_groups_key(self):
        got, problems = self.keys()
        self.assertEqual(got, {"fixed.1": "", "opts.1": "witch", "fixed.2": "",
                               "goal-fixed.1": "goal", "frame-counter.1": "count",
                               "opts.2": "hardmode lowhpbeep"})
        self.assertEqual(problems, [])

    def test_traced_reads_widenings_and_constants_join_the_key_in_order(self):
        facts = gen_tables.Facts()
        facts.settings_reads[site("b(rom, settings)")] = {"race", "witch"}
        facts.constant_reads[site("a(rom)")] = {"ITEM_DIALOGS", "INV_SIZE"}
        facts.constant_reads[site("f(rom, n)")] = {"ITEM_DIALOGS"}
        got, _ = self.keys(facts, widenings=[("c(rom)", "goal")])
        self.assertEqual((got["fixed.1"], got["opts.1"], got["fixed.2"], got["frame-counter.1"]),
                         ("asm:ITEM_DIALOGS", "race witch", "goal", "asm:ITEM_DIALOGS count"))

    def test_a_unit_takes_the_key_of_the_earlier_units_whose_writes_it_reads(self):
        facts = gen_tables.Facts()
        facts.constant_reads[site("a(rom)")] = {"ITEM_DIALOGS"}
        facts.targets[site("a(rom)")] = {"bank 0x26"}
        facts.targets[site("c(rom)")] = {"table texts"}
        facts.targets[site("b(rom, settings)")] = {"table texts"}
        facts.provenance[site("c(rom)")] = {("opts", "table texts", None)}
        facts.provenance[site("e(rom)")] = {("fixed", "bank 0x26", (0x10, 0x20))}
        facts.carried[site("g(rom)")] = {site("c(rom)")}
        got, problems = self.keys(facts)
        self.assertEqual((got["fixed.2"], got["goal-fixed.1"], got["opts.2"]),
                         ("witch", "goal asm:ITEM_DIALOGS", "hardmode lowhpbeep witch"))
        self.assertEqual(problems, [])

    def test_only_the_units_that_wrote_inside_the_read_range_are_read(self):
        facts = gen_tables.Facts()
        facts.constant_reads[site("a(rom)")] = {"ITEM_DIALOGS"}
        facts.add_events(stream(bank("a(rom)", 0x26 * 0x4000 + 0x30, b"\x01"),
                                bank("c(rom)", 0x26 * 0x4000 + 0x18, b"\x01"),
                                entry("b(rom, settings)", 5, b"\x01", "opts")))
        facts.provenance[site("e(rom)")] = {("fixed", "bank 0x26", (0x10, 0x20))}
        facts.provenance[site("g(rom)")] = {("opts", "table texts", (6, 9))}
        got, _ = self.keys(facts)
        self.assertEqual((got["goal-fixed.1"], got["opts.2"]), ("goal", "hardmode lowhpbeep"))
        facts.provenance[site("e(rom)")] = {("fixed", "bank 0x26", (0x10, 0x30))}
        facts.provenance[site("g(rom)")] = {("opts", "table texts", (5, 9))}
        got, _ = self.keys(facts)
        self.assertEqual((got["goal-fixed.1"], got["opts.2"]),
                         ("goal asm:ITEM_DIALOGS", "hardmode lowhpbeep witch"))

    def test_a_row_rewritten_by_a_later_row_takes_its_key(self):
        got, _ = self.keys(rewrites=[("b(rom, settings)", "g(rom)")])
        self.assertEqual((got["opts.1"], got["opts.2"]),
                         ("hardmode lowhpbeep witch", "hardmode lowhpbeep"))

    def test_reading_or_carrying_a_c_groups_writes_is_depends_c(self):
        facts = gen_tables.Facts()
        facts.provenance[site("e(rom)")] = {("items", "bank 0x02", None)}
        facts.carried[site("g(rom)")] = {site("d(rom)")}
        _, problems = self.keys(facts)
        self.assertEqual(problems, ["DEPENDS-C goal-fixed.1 items", "DEPENDS-C opts.2 items"])


def table(columns, cells):
    return ("| " + " | ".join(columns) + " |\n|" + "---|" * len(columns) + "\n"
            + "".join("| " + " | ".join(c) + " |\n" for c in cells) + "\n")


def inventory_text():
    check = gen_tables.inventory_check
    calls = [("`assembler.const(\"INV_SIZE\", 16)`", "-", "none", "asm-consts"),
             ("`assembler.const(\"ITEM_DIALOGS\", 1 if settings.textmode != 'notextbox' else 0)`",
              "textmode", "none", "asm-consts"),
             ("`patches.core.a(rom)`", "-", "table", "fixed"),
             ("`patches.b(rom, settings)`", "witch, *", "table", "opts"),
             ("`hints.addHints(rom, rnd)`", "-", "C", "hints"),
             ("`patches.b(rom, settings)` #2", "-", "table", "opts")]
    text = "## generator.py generateRom\n\n" + table(check.COLUMNS, [
        [c, s, "-", "-", "-", "-", "", side, g] for c, s, side, g in calls])
    text += "## rom.py ROM.save\n\n" + table(check.COLUMNS, [
        ["`rom.fixHeader()`", "-", "-", "-", "-", "-", "", "C", "header"]])
    text += "## Groups\n\n" + table(check.GROUP_COLUMNS, [
        ["asm-consts", "none", "textmode", "-", "-"], ["fixed", "table", "-", "-", "-"],
        ["header", "C", "-", "-", "header"], ["hints", "C", "-", "-", "hints"],
        ["opts", "table", "witch", "-", "-"]])
    for name, columns in check.NAMED_TABLES.items():
        if name != "Groups":
            text += "## %s\n\n%s" % (name, table(columns, []))
    return text


class Inputs(unittest.TestCase):

    def test_the_inventory_gives_the_rows_group_keys_and_setting_constants(self):
        inv = gen_tables.read_inventory(inventory_text(), SETTINGS)
        self.assertEqual([(r.site, r.side, r.group, r.settings) for r in inv.rows], [
            (site("patches.core.a(rom)"), "table", "fixed", []),
            (site("patches.b(rom, settings)"), "table", "opts", ["witch"]),
            (site("hints.addHints(rom, rnd)"), "C", "hints", []),
            (site("patches.b(rom, settings) #2"), "table", "opts", []),
            ("rom.py ROM.save\trom.fixHeader()", "C", "header", [])])
        self.assertEqual((inv.group_keys["opts"], inv.sides["hints"]), (["witch"], "C"))
        self.assertEqual(inv.constants, {"ITEM_DIALOGS": ["textmode"]})
        self.assertEqual(inv.sections, [SECTION, "rom.py ROM.save"])
        self.assertEqual(inv.const_exprs["ITEM_DIALOGS"],
                         "1 if settings.textmode != 'notextbox' else 0")

    def test_widenings_and_fusions_name_table_rows_by_their_call(self):
        inv = gen_tables.read_inventory(inventory_text(), SETTINGS)
        text = "# header\npatches.b(rom, settings) #2\tasm:ITEM_DIALOGS\treads the dialog flag\n"
        self.assertEqual(gen_tables.read_widenings(text, inv), [
            (site("patches.b(rom, settings) #2"), "asm:ITEM_DIALOGS", "reads the dialog flag")])
        text = "patches.b(rom, settings)\tpatches.b(rom, settings) #2\tone copy\n"
        self.assertEqual(gen_tables.read_fusions(text, inv), [
            (site("patches.b(rom, settings)"), site("patches.b(rom, settings) #2"), "one copy")])
        for bad in ("patches.b(rom)\twitch\twhy\n", "patches.core.a(rom)\tnoset\twhy\n",
                    "patches.core.a(rom)\twitch\n", "patches.core.a(rom)\tasm:INV_SIZE\twhy\n",
                    "hints.addHints(rom, rnd)\twitch\twhy\n"):
            with self.assertRaises(gen_tables.TablesPlanError, msg=bad):
                gen_tables.read_widenings(bad, inv)
        for bad in ("patches.core.a(rom)\tpatches.b(rom, settings)\twhy\n",
                    "patches.core.a(rom)\tpatches.core.a(rom)\twhy\n"):
            with self.assertRaises(gen_tables.TablesPlanError, msg=bad):
                gen_tables.read_fusions(bad, inv)


class TracedFacts(unittest.TestCase):

    def test_events_give_each_rows_targets_and_the_writes_it_carries(self):
        made = write_log.KNOWN - 1
        facts = gen_tables.Facts()
        facts.add_events(stream(
            bank("a(rom)", 0x4ffe, b"\x01\x02\x03", origins=[made, made - 1, 5]),
            entry("b(rom)", 3, b"\x09"),
            ("B", "opts", site("c(rom)"), 0x10, b"\x02\x01", (made - 1, made)),
            ("T", "opts", site("c(rom)"), "texts", 4, 0x1234, None)))
        self.assertEqual(facts.targets, {site("a(rom)"): {"bank 0x01"},
                                         site("b(rom)"): {"table texts"},
                                         site("c(rom)"): {"bank 0x00", "table texts"}})
        self.assertEqual(facts.carried, {site("c(rom)"): {site("a(rom)")}})

    def test_reads_give_the_provenance_of_written_bytes(self):
        facts = gen_tables.Facts()
        facts.add_reads("\n".join([
            "\t".join(["opts", SECTION, "b(rom)", "patches/b.py:b", "slice", "bank 0x26",
                       "written:fixed,goal", "n=2 0x0010..0x0020"]),
            "\t".join(["opts", SECTION, "b(rom)", "patches/b.py:b", "table", "table texts",
                       "mixed:fixed", "n=1 0x0002..0x0002"]),
            "\t".join(["opts", SECTION, "c(rom)", "patches/b.py:b", "index", "bank 0x01",
                       "source", "n=1 0x4010..0x4010"]),
            "\t".join(["-", "-", "rom.py:ROM.save", "slice", "bank 0x00", "written:items",
                       "n=1 0x0134..0x014C"])]) + "\n")
        self.assertEqual(facts.provenance, {site("b(rom)"): {
            ("fixed", "bank 0x26", (0x10, 0x20)), ("goal", "bank 0x26", (0x10, 0x20)),
            ("fixed", "table texts", (2, 2))}})

    def test_key_traces_give_the_settings_and_constants_each_row_read(self):
        facts = gen_tables.Facts()
        trace = gen_tables.key_trace.VERSION_LINE + "\n%s\n%s\n" % (
            "\t".join(["const", SECTION, "b(rom)", "ITEM_DIALOGS"]),
            "\t".join(["setting", SECTION, "b(rom)", "witch"]))
        facts.add_keys(trace)
        facts.add_keys(trace.replace("witch", "race"))
        self.assertEqual((facts.settings_reads, facts.constant_reads), (
            {site("b(rom)"): {"witch", "race"}}, {site("b(rom)"): {"ITEM_DIALOGS"}}))
        with self.assertRaises(gen_tables.TablesPlanError):
            facts.add_keys("not a key trace\n")


def planted_model():
    """race 0/1, textmode fast/normal/notextbox, witch 0/1: race=1 forces witch=0; ITEM_DIALOGS
    is 0 under notextbox; the count is 10 + witch; textmode=notextbox is refused alone, its
    companion race=1."""
    defaults = {"race": "0", "textmode": "fast", "witch": "0"}

    def validate(assignment):
        values = dict(defaults, **assignment)
        if values["race"] == "1":
            values["witch"] = "0"
        return values

    return gen_tables.Model(
        ["race", "textmode", "witch"], defaults,
        {"race": ["0", "1"], "textmode": ["fast", "normal", "notextbox"], "witch": ["0", "1"]},
        validate, {"ITEM_DIALOGS": (["textmode"],
                                    lambda s: "0" if s["textmode"] == "notextbox" else "1")},
        lambda s: str(10 + int(s["witch"])), ["witch"],
        {"textmode=notextbox": {"race": "1"}})


class Reachability(unittest.TestCase):

    def test_tuples_are_the_image_of_the_tier1_product_through_validate(self):
        got, unreached = gen_tables.reachable(["race", "witch"], planted_model())
        self.assertEqual(got, [(("0", "0"), {}), (("0", "1"), {"witch": "1"}),
                               (("1", "0"), {"race": "1"})])
        self.assertEqual(unreached, [])

    def test_constants_and_counts_take_the_first_combination_that_gives_them(self):
        model = planted_model()
        self.assertEqual(gen_tables.reachable(["count"], model),
                         ([(("10",), {}), (("11",), {"witch": "1"})], []))
        self.assertEqual(gen_tables.reachable(["asm:ITEM_DIALOGS"], model), ([
            (("0",), {"race": "1", "textmode": "notextbox"}), (("1",), {})], []))

    def test_a_tuple_lost_to_a_refused_values_companion_is_unreached(self):
        got, unreached = gen_tables.reachable(["asm:ITEM_DIALOGS", "witch"], planted_model())
        self.assertEqual([t for t, _ in got], [("0", "0"), ("1", "0"), ("1", "1")])
        self.assertEqual(unreached, [("0", "1")])


class CoverageRuns(unittest.TestCase):

    KEYS = {"u.1": ["race"], "u.2": ["textmode"], "u.3": ["count"]}

    def plan(self, keys=None):
        return gen_tables.plan_runs(keys or self.KEYS, planted_model())

    def test_witnesses_merge_most_constrained_first_into_the_fewest_runs(self):
        runs, unreached = self.plan()
        self.assertEqual([r.assignment for r in runs], [
            {"race": "1", "textmode": "notextbox"}, {"textmode": "normal", "witch": "1"}, {}])
        self.assertEqual(runs[0].covers, [("u.1", ("1",)), ("u.2", ("notextbox",)),
                                          ("u.3", ("10",))])
        self.assertEqual(unreached, [])

    def test_every_reachable_tuple_is_covered_and_the_plan_is_deterministic(self):
        keys = dict(self.KEYS, **{"u.4": ["asm:ITEM_DIALOGS", "witch"]})
        runs, unreached = self.plan(keys)
        covered = {c for r in runs for c in r.covers}
        for unit, components in keys.items():
            tuples, _ = gen_tables.reachable(components, planted_model())
            self.assertTrue({(unit, t) for t, _ in tuples} <= covered, unit)
        self.assertEqual(unreached, ["UNREACHED u.4 0 1"])
        again, _ = self.plan(dict(reversed(list(keys.items()))))
        self.assertEqual([r.assignment for r in again], [r.assignment for r in runs])

    def test_a_merged_run_with_no_ok_seed_is_replanned_unmerged(self):
        runs, unreached = gen_tables.plan_runs(self.KEYS, planted_model(),
                                               avoid=[{"textmode": "normal", "witch": "1"}])
        self.assertEqual([r.assignment for r in runs], [
            {"race": "1", "textmode": "notextbox"}, {"textmode": "normal"}, {"witch": "1"}])
        self.assertEqual(unreached, [])
        runs, unreached = gen_tables.plan_runs(self.KEYS, planted_model(),
                                               avoid=[{"race": "1", "textmode": "notextbox"}])
        self.assertNotIn({"race": "1", "textmode": "notextbox"}, [r.assignment for r in runs])
        self.assertEqual(unreached, ["UNREACHED u.2 notextbox"])

    def test_runs_already_done_count_first_and_only_what_they_miss_is_planned(self):
        runs, _ = gen_tables.plan_runs(self.KEYS, planted_model(),
                                       done=[{"textmode": "normal", "witch": "1"}])
        self.assertEqual([r.assignment for r in runs], [
            {"textmode": "normal", "witch": "1"}, {"race": "1", "textmode": "notextbox"}, {}])
        self.assertEqual(runs[0].covers, [("u.1", ("0",)), ("u.2", ("normal",)),
                                          ("u.3", ("11",))])


BUILD_ROWS = [gen_tables.Row(site("a(rom)"), "table", "fixed", []),
              gen_tables.Row(site("b(rom)"), "C", "items", []),
              gen_tables.Row(site("c(rom)"), "table", "opts", ["race"])]
BUILD_SIDES = {"fixed": "table", "items": "C", "opts": "table"}
NOT_ROM_10 = bytes([ROM[0x10] ^ 0xff])


class PlantedRunner:
    """Coverage runs of a planted LADXR: a(rom) copies ROM 0x800.. to 0x100 (plus a byte when
    a_reads_race and race=1), b(rom) writes 0x400, c(rom) writes 0x10 when race=1 (and 0x11 when
    witch=1, a read the key trace shows when c_reads_witch); runs in `fail` have no ok seed."""

    def __init__(self, a_reads_race=False, c_reads_witch=False, fail=(), c_reads_items=False,
                 d_rewrites=False):
        self.a_reads_race, self.c_reads_witch = a_reads_race, c_reads_witch
        self.d_rewrites = d_rewrites
        self.fail, self.c_reads_items, self.calls = fail, c_reads_items, []

    def __call__(self, name, assignment):
        self.calls.append((name, assignment))
        if assignment in self.fail:
            return None
        values = dict(planted_model().validated(assignment))
        events = [bank("a(rom)", 0x100, ROM[0x800:0x806])]
        if self.a_reads_race and values["race"] == "1":
            events.append(bank("a(rom)", 0x106, b"\x00"))
        if self.d_rewrites and values["race"] == "1":
            events.append(bank("d(rom)", 0x105, bytes([ROM[0x805] ^ 0xff])))
        events.append(bank("b(rom)", 0x400, b"\x00", "items"))
        if values["race"] == "1":
            events.append(bank("c(rom)", 0x10, NOT_ROM_10, "opts"))
        if self.c_reads_witch and values["witch"] == "1":
            events.append(bank("c(rom)", 0x11, b"\x00", "opts"))
        keys = gen_tables.key_trace.VERSION_LINE + "\n"
        if self.c_reads_witch:
            keys += "\t".join(["setting", SECTION, "c(rom)", "witch"]) + "\n"
        reads = ""
        if self.c_reads_items:
            reads = "\t".join(["opts", SECTION, "c(rom)", "patches/c.py:c", "index", "bank 0x00",
                               "written:items", "n=1 0x0400..0x0400"]) + "\n"
        return stream(*events), reads, keys


class Generate(unittest.TestCase):

    def generate(self, runner, widenings=(), fusions=(), rows=BUILD_ROWS):
        inv = gen_tables.Inventory(rows, {"fixed": [], "opts": ["race"]}, BUILD_SIDES, {},
                                   ["race", "textmode", "witch"])
        return gen_tables.generate(inv, planted_model(), list(widenings), list(fusions), runner,
                                   bytes(ROM))

    def test_the_coverage_runs_give_the_order_and_one_file_per_table_group(self):
        runner = PlantedRunner()
        files, problems = self.generate(runner)
        self.assertEqual(problems, [])
        self.assertEqual(runner.calls, [("run-00", {"race": "1"}), ("run-01", {})])
        self.assertEqual(files, {
            "order.txt": "ladx-tables-order 1\nsection generator.py generateRom\n"
                         "table fixed fixed.1 a(rom)\nC items - b(rom)\n"
                         "table opts opts.1 c(rom)\n",
            "fixed.txt": "ladx-tables 1\ngroup fixed\ntables texts\nunit fixed.1\nE\n"
                         "B 100 @800+6\n",
            "opts.txt": "ladx-tables 1\ngroup opts\ntables texts\nunit opts.1 race\nE 0\n"
                        "nothing\nE 1\nB 10 %s\n" % NOT_ROM_10.hex()})
        for text in files.values():
            self.assertEqual(gen_tables.tables.render(gen_tables.tables.parse(text)), text)

    def test_a_traced_read_widens_the_key_and_plans_the_runs_it_needs(self):
        runner = PlantedRunner(c_reads_witch=True)
        files, problems = self.generate(runner)
        self.assertEqual(problems, [])
        self.assertEqual(runner.calls[2:], [("run-02", {"witch": "1"})])
        self.assertIn("unit opts.1 race witch\nE 0 0\nnothing\nE 0 1\nB 11 00\nE 1 0\n",
                      files["opts.txt"])

    def test_a_row_rewritten_by_a_later_one_is_keyed_by_it_not_fused(self):
        rows = BUILD_ROWS[:1] + [gen_tables.Row(site("d(rom)"), "table", "fixed", ["race"])] \
            + BUILD_ROWS[1:]
        files, problems = self.generate(PlantedRunner(d_rewrites=True), rows=rows)
        self.assertEqual(problems, [])
        self.assertIn("unit fixed.1 race\nE 0\nB 100 @800+6\nE 1\nB 100 @800+5\n"
                      "unit fixed.2 race\nE 0\nnothing\nE 1\nB 105 %02x\n" % (ROM[0x805] ^ 0xff),
                      files["fixed.txt"])

    def test_two_runs_giving_one_tuple_different_writes_conflict(self):
        _, problems = self.generate(PlantedRunner(a_reads_race=True))
        self.assertEqual(problems, ["CONFLICT fixed.1"])
        files, problems = self.generate(PlantedRunner(a_reads_race=True),
                                        widenings=[(site("a(rom)"), "race", "why")])
        self.assertEqual(problems, [])
        self.assertIn("unit fixed.1 race\nE 0\nB 100 @800+6\nE 1\n", files["fixed.txt"])

    def test_a_tuple_whose_run_has_no_ok_seed_is_unreached(self):
        runner = PlantedRunner(fail=[{"race": "1"}])
        _, problems = self.generate(runner)
        self.assertEqual(problems, ["UNREACHED opts.1 1"])
        self.assertEqual(runner.calls, [("run-00", {"race": "1"}), ("run-01", {})])

    def test_reading_a_c_groups_writes_is_reported(self):
        _, problems = self.generate(PlantedRunner(c_reads_items=True))
        self.assertEqual(problems, ["DEPENDS-C opts.1 items"])


PLANT_SETTINGS = ("setting race\tMain\tV\tbool\t0\tRace\t0\t-\n"
                  "setting textmode\tUser\tf\tstr\tfast\tText\t0\t-\n"
                  "option textmode\tfast\tF\tFast\t-\noption textmode\tnormal\tN\tNormal\t-\n"
                  "option textmode\tnotextbox\tX\tNone\t-\n"
                  "setting witch\tMain\tw\tbool\t1\tWitch\t0\t-\n"
                  "setting music\tUser\tm\tstr\t\"\"\tMusic\t1\t-\n"
                  "option music\t\"\"\tD\tDefault\t-\noption music\toff\tO\tOff\t-\n")
PLANT_TIER1 = "race all\ntextmode all\n"
PLANT_COMBOS = "refused textmode=notextbox\ncase race=1 textmode=notextbox\n"
PLANT_VALIDATE = "# keys \n"


class ModelInputs(unittest.TestCase):

    def model(self, validate=None, count=None):
        inv = gen_tables.Inventory([], {}, {}, {"ITEM_DIALOGS": ["textmode"]}, [],
                                   {"ITEM_DIALOGS": "1 if settings.textmode != 'notextbox' else 0"})
        return gen_tables.build_model(inv, PLANT_TIER1, PLANT_SETTINGS, PLANT_COMBOS,
                                      PLANT_VALIDATE, validate or (lambda a: a), count,
                                      ["witch"])

    def test_fixtures_give_defaults_domains_constants_and_refused_companions(self):
        model = self.model()
        self.assertEqual(model.settings, ["race", "textmode", "witch", "music"])
        self.assertEqual(model.defaults, {"race": "0", "textmode": "fast", "witch": "1",
                                          "music": '""'})
        self.assertEqual(model.domains, {"race": ["0", "1"], "textmode": [
            "fast", "normal", "notextbox"], "witch": ["1"], "music": ['""']})
        self.assertEqual(model.refused, {"textmode=notextbox": {"race": "1"}})
        fn = model.constants["ITEM_DIALOGS"][1]
        self.assertEqual((fn({"textmode": "notextbox"}), fn({"textmode": "normal"})), ("0", "1"))

    def test_assignments_are_rendered_as_ladxr_sets_them(self):
        self.assertEqual(gen_tables.assignment_lines({"race": "1", "music": '""'}),
                         ["race=1", "music="])

    def test_validate_sets_ladxrs_settings_and_renders_every_one(self):
        class Setting:
            def __init__(self, key, value):
                self.key, self.value = key, value

        class Settings:
            def __init__(self):
                self.all = [Setting("race", False), Setting("music", "x")]

            def set(self, line):
                key, value = line.split("=")
                setting = next(s for s in self.all if s.key == key)
                setting.value = bool(int(value)) if key == "race" else value

            def validate(self):
                print("Warning: dropped")
                if self.all[0].value:
                    self.all[1].value = ""

            def __iter__(self):
                return iter(self.all)

        validate = gen_tables.settings_validate(Settings)
        self.assertEqual(validate({}), {"race": "0", "music": "x"})
        self.assertEqual(validate({"race": "1"}), {"race": "1", "music": '""'})

    def test_the_spot_count_is_the_non_key_spots_of_a_pool_only_run(self):
        class Key:
            pass

        class Spot:
            pass

        class Driver:
            def run(self, seed, assignments, pool_only=False):
                self.seen = (seed, assignments, pool_only)
                self.logic = type("L", (), {"iteminfo_list": [Spot(), Key(), Spot()]})()
                return ["outcome ok"] if "race=1" not in assignments else ["outcome error x"]

        driver = Driver()
        count = gen_tables.spot_count(driver, Key, {"race": "0", "witch": "1"})
        self.assertEqual(count({"race": "0", "witch": "0", "textmode": "fast"}), "2")
        self.assertEqual(driver.seen, (gen_tables.COUNT_SEED, ["witch=0"], True))
        self.assertIsNone(count({"race": "1", "witch": "1"}))


if __name__ == "__main__":
    unittest.main()
