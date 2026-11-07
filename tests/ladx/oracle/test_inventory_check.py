"""Usage: python3 -I test_inventory_check.py

Unit tests of inventory_check.py on planted LADXR-shaped source trees written to a temporary
directory, with no LADXR and no ROM.
"""

import contextlib
import importlib.util
import io
import os
import shutil
import sys
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("inventory_check",
                                               os.path.join(HERE, "inventory_check.py"))
inventory_check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(inventory_check)

GENERATOR = textwrap.dedent('''\
    import patches.planted
    import hints


    def generateRom(args, settings, seed, logic, *, rnd=None):
        rom = ROMWithTables(open(args.input_filename, 'rb'))
        # a comment
        patches.planted.fixed(rom)
        if settings.music == 'random':
            patches.planted.shuffle(rom)
        elif settings.music == 'off':
            patches.planted.fixed(rom)
        else:
            patches.planted.seeded(rom, seed, settings)
        hints.addHints(rom, rnd, logic.iteminfo_list)
        for spot in logic.iteminfo_list:
            spot.patch(rom, spot.item)
        if args.romdebugmode:
            rom.patch(0, 0x0003, "00", "01")
        print("done")
        return rom
    ''')

RANDOMIZER = textwrap.dedent('''\
    class Randomizer:
        def __init__(self, args, settings, *, seed=None):
            filename = args.output_filename
            item_placer.run(self.rnd)
            rom = generator.generateRom(args, settings, self.seed, self.__logic, rnd=self.rnd)
            filename = args.output_filename
            rom.save(filename, name="LADXR")

        def other(self):
            pass
    ''')

ROM_TABLES = textwrap.dedent('''\
    class ROMWithTables(ROM):
        def __init__(self, filestream):
            super().__init__(filestream)
            self.texts = Texts(self)

        def save(self, filename, *, name=None):
            self.texts.store(self)
            self.patch(0x00, 0x0DFA, ASM("ld hl, $763B"), ASM("ld hl, $%04x" % (start | 0x4000)))
            super().save(filename, name=name)
    ''')

ROM = textwrap.dedent('''\
    class ROM:
        def save(self, file, *, name=None):
            self.fixHeader(name=name)

        def fixHeader(self, *, name=None):
            self.banks[0][0x14D] = 0
    ''')

PATCHES = textwrap.dedent('''\
    import random


    def fixed(rom):
        rom.banks[0][0] = 1


    def shuffle(rom):
        rom.patch(0, 0x10, ASM("""
    ld a, 1
    """), ASM("""
    ld a, 2
    """))
        rom.banks[0][1] = random.randint(0, 3)


    def seeded(rom, seed, settings):
        rng = random.Random(seed)
    ''')

HINTS = textwrap.dedent('''\
    def addHints(rom, rnd, spots):
        rnd.shuffle(spots)
    ''')


def _row(call, settings="-", args="-", rng="-", reads="-", side="table", group="g", trace="-"):
    return "| %s | %s | %s | %s | %s | %s |  | %s | %s |" % (call, settings, args, rng, reads,
                                                            trace, side, group)


def _table(section, rows):
    return "\n".join(["## " + section, "",
                      "| " + " | ".join(inventory_check.COLUMNS) + " |",
                      "|" + "---|" * len(inventory_check.COLUMNS)] + rows + [""])


GENERATE_ROWS = [
    _row("`rom = ROMWithTables(open(args.input_filename, 'rb'))`", args="input_filename",
         group="parse"),
    _row("`patches.planted.fixed(rom)`"),
    _row("`patches.planted.shuffle(rom)`", settings="music", rng="global"),
    _row("`patches.planted.fixed(rom)` #2", settings="music"),
    _row("`patches.planted.seeded(rom, seed, settings)`", settings="music, *", rng="seed"),
    _row("`hints.addHints(rom, rnd, logic.iteminfo_list)`", rng="rnd", reads="placement"),
    _row("`spot.patch(rom, spot.item)`", reads="placement", side="C", group="items"),
    _row('`rom.patch(0, 0x0003, "00", "01")`', args="romdebugmode", side="unused",
         group="debug"),
]

OTHER_TABLES = [
    _table("randomizer.py Randomizer.__init__", [
        _row("`rom = generator.generateRom(args, settings, self.seed, self.__logic, "
             "rnd=self.rnd)`", settings="*", args="*", rng="rnd, seed", reads="logic"),
        _row("`filename = args.output_filename` #2", args="output_filename"),
        _row('`rom.save(filename, name="LADXR")`'),
    ]),
    _table("romTables.py ROMWithTables.__init__", [
        _row("`super().__init__(filestream)`"),
        _row("`self.texts = Texts(self)`"),
    ]),
    _table("romTables.py ROMWithTables.save", [
        _row("`self.texts.store(self)`"),
        _row('`self.patch(0x00, 0x0DFA, ASM("ld hl, $763B"), ASM("ld hl, $%04x" % '
             '(start \\| 0x4000)))`'),
        _row("`super().save(filename, name=name)`"),
    ]),
    _table("rom.py ROM.save", [_row("`self.fixHeader(name=name)`")]),
    _table("rom.py ROM.fixHeader", [_row("`self.banks[0][0x14D] = 0`")]),
]


GROUPS = [
    "| parse | table | - | - | - |",
    "| g | table | music | - | - |",
    "| items | C | - | g | items |",
    "| debug | unused | - | - | - |",
]
BLIND = ["| utils.py | `data = memoryview(b)` | not a ROM read: a planted line |"]


def _named(name, columns, rows):
    return "\n".join(["## " + name, "", "| " + " | ".join(columns) + " |",
                      "|" + "---|" * len(columns)] + rows + [""])


def _inventory(generate_rows=None, groups=None, coverage=(), blind=None):
    rows = GENERATE_ROWS if generate_rows is None else generate_rows
    return "# Inventory\n\nFree text.\n\n" + "\n".join(
        [_table("generator.py generateRom", rows)] + OTHER_TABLES + [
            _named("Groups", inventory_check.GROUP_COLUMNS, GROUPS if groups is None else groups),
            _named("Read coverage", inventory_check.COVERAGE_COLUMNS, list(coverage)),
            _named("Blind spots", inventory_check.BLIND_COLUMNS, BLIND if blind is None else blind),
            "## ROMWithTables.save\n\nProse.\n"])


class Fixture(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="inventory-check-test-")
        self.addCleanup(shutil.rmtree, self.dir)
        self.ladxr = os.path.join(self.dir, "ladxr")
        os.makedirs(os.path.join(self.ladxr, "patches"))
        for name, text in (("randomizer.py", RANDOMIZER), ("romTables.py", ROM_TABLES),
                           ("rom.py", ROM), ("hints.py", HINTS),
                           ("patches/planted.py", PATCHES),
                           ("utils.py", "data = memoryview(b)\n")):
            with open(os.path.join(self.ladxr, name), "w") as f:
                f.write(text)
        with open(os.path.join(self.ladxr, "generator.py"), "wb") as f:
            f.write(GENERATOR.replace("\n", "\r\n").encode())

    def check(self, text):
        return inventory_check.check(self.ladxr, text)

    def run_main(self, text, *extra):
        path = os.path.join(self.dir, "inventory.md")
        with open(path, "w") as f:
            f.write(text)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = inventory_check.main(["--ladxr", self.ladxr, "--inventory", path] +
                                        list(extra))
        return code, out.getvalue()


class Extraction(Fixture):
    def test_matching_inventory_has_no_finding(self):
        self.assertEqual(self.check(_inventory()), [])

    def test_crlf_sources_give_the_same_rows(self):
        with open(os.path.join(self.ladxr, "generator.py"), "rb") as f:
            self.assertIn(b"\r\n", f.read())
        rows = dict(inventory_check.extract(self.ladxr))["generator.py generateRom"]
        self.assertTrue(all("\r" not in inventory_check.row_key(r) for r in rows))
        self.assertEqual(len(rows), len(GENERATE_ROWS))

    def test_else_row_names_every_condition_of_its_chain(self):
        rows = dict(inventory_check.extract(self.ladxr))["generator.py generateRom"]
        row = next(r for r in rows if r["text"].startswith("patches.planted.seeded"))
        self.assertEqual(row["Settings"], "music, *")

    def test_elif_row_names_the_if_above_it(self):
        rows = dict(inventory_check.extract(self.ladxr))["generator.py generateRom"]
        row = next(r for r in rows if inventory_check.row_key(r) == "patches.planted.fixed(rom) #2")
        self.assertEqual(row["Settings"], "music")

    def test_settings_passed_whole_is_a_star(self):
        found = self.check(_inventory([r.replace("music, *", "music") for r in GENERATE_ROWS]))
        self.assertEqual(found, ["MISMATCH generator.py generateRom: patches.planted.seeded(rom, "
                                 "seed, settings): Settings inventory music, source music, *"])

    def test_repeat_counts_lines_before_the_start_line(self):
        rows = dict(inventory_check.extract(self.ladxr))["randomizer.py Randomizer.__init__"]
        self.assertEqual([inventory_check.row_key(r) for r in rows][1],
                         "filename = args.output_filename #2")

    def test_multi_line_statement_is_one_row_greped_whole(self):
        with open(os.path.join(self.ladxr, "generator.py"), "w") as f:
            f.write(textwrap.dedent('''\
                def generateRom(args, settings, seed, logic, *, rnd=None):
                    patches.planted.fixed(rom,
                settings.witch, """
                if settings.hidden:
                """)
                    patches.planted.fixed(rom)
                '''))
        rows = dict(inventory_check.extract(self.ladxr))["generator.py generateRom"]
        self.assertEqual([(inventory_check.row_key(r), r["Settings"]) for r in rows],
                         [("patches.planted.fixed(rom,", "hidden, witch"),
                          ("patches.planted.fixed(rom)", "-")])

    def test_pipe_in_a_source_line_round_trips_escaped(self):
        rendered = inventory_check.render(inventory_check.extract(self.ladxr))
        self.assertIn("(start \\| 0x4000)", rendered)
        tables, bad = inventory_check.parse_inventory(rendered)
        self.assertEqual(bad, [])
        self.assertEqual(inventory_check.diff(inventory_check.extract(self.ladxr), tables),
                         ["BAD %s: %s: Side '?' is not one of table, C, none, unused" % (s, r["key"])
                          for s, rows in tables.items() for r in rows])


class Diff(Fixture):
    def test_dropped_row_is_missing(self):
        rows = [r for r in GENERATE_ROWS if "addHints" not in r]
        self.assertEqual(self.check(_inventory(rows)), [
            "MISSING generator.py generateRom: hints.addHints(rom, rnd, logic.iteminfo_list)"])

    def test_extra_row_is_stale(self):
        rows = GENERATE_ROWS[:2] + [_row("`patches.planted.gone(rom)`")] + GENERATE_ROWS[2:]
        self.assertEqual(self.check(_inventory(rows)), [
            "STALE generator.py generateRom: patches.planted.gone(rom)"])

    def test_wrong_settings_is_a_mismatch(self):
        rows = [r.replace("| music | - | global", "| witch | - | global") for r in GENERATE_ROWS]
        self.assertEqual(self.check(_inventory(rows)), [
            "MISMATCH generator.py generateRom: patches.planted.shuffle(rom): Settings inventory "
            "witch, source music"])

    def test_missed_global_random_callee_is_a_mismatch(self):
        rows = [r.replace("| global |", "| - |") for r in GENERATE_ROWS]
        self.assertEqual(self.check(_inventory(rows)), [
            "MISMATCH generator.py generateRom: patches.planted.shuffle(rom): RNG (grep) "
            "inventory -, source global"])

    def test_swapped_rows_are_missing_and_stale(self):
        rows = list(GENERATE_ROWS)
        rows[5], rows[6] = rows[6], rows[5]
        found = self.check(_inventory(rows))
        self.assertEqual(sorted(line.split(":")[0].split()[0] for line in found),
                         ["MISSING", "STALE"])
        self.assertEqual(len({line.split(": ", 1)[1] for line in found}), 1)

    def test_bad_side_is_refused_and_a_question_mark_is_no_longer_a_side(self):
        for side in ("maybe", "?"):
            rows = [r.replace("| table | g |", "| %s | g |" % side, 1)
                    if "fixed(rom)` |" in r else r for r in GENERATE_ROWS]
            self.assertEqual(self.check(_inventory(rows)), [
                "BAD generator.py generateRom: patches.planted.fixed(rom): Side %r is not one of "
                "table, C, none, unused" % side])

    def test_empty_group_is_refused(self):
        rows = [r.replace("| table | g |", "| table |  |") if "fixed(rom)` |" in r else r
                for r in GENERATE_ROWS]
        self.assertEqual(self.check(_inventory(rows)), [
            "BAD generator.py generateRom: patches.planted.fixed(rom): empty Group"])

    def test_the_old_draft_columns_are_refused(self):
        text = _inventory().replace("| Side | Group |", "| Side (draft) | Group (draft) |", 1)
        self.assertEqual([l.split(":")[0] for l in self.check(text)],
                         ["BAD generator.py generateRom"])

    def test_missing_section_lists_every_row(self):
        text = _inventory().replace("## rom.py ROM.fixHeader", "## rom.py ROM.fixHeaders")
        self.assertEqual(self.check(text), [
            "MISSING rom.py ROM.fixHeader: self.banks[0][0x14D] = 0"])


class Tables(Fixture):
    def test_a_row_group_missing_from_groups(self):
        groups = [g for g in GROUPS if not g.startswith("| debug ")]
        self.assertEqual(self.check(_inventory(groups=groups)), [
            'BAD generator.py generateRom: rom.patch(0, 0x0003, "00", "01"): group debug is not '
            'in Groups'])

    def test_a_row_side_unlike_its_group_side(self):
        rows = [r.replace("| table | g |", "| C | g |") if "fixed(rom)` |" in r else r
                for r in GENERATE_ROWS]
        self.assertEqual(self.check(_inventory(rows)), [
            "BAD generator.py generateRom: patches.planted.fixed(rom): Side C, its group g is "
            "table"])

    def test_a_group_no_row_uses_and_a_group_listed_twice(self):
        groups = GROUPS + ["| gone | table | - | - | - |", "| g | table | - | - | - |"]
        self.assertEqual(self.check(_inventory(groups=groups)), [
            "BAD Groups: g: listed twice", "BAD Groups: gone: no row uses it"])

    def test_a_bad_group_side(self):
        groups = GROUPS + ["| odd | ? | - | - | - |"]
        found = self.check(_inventory(groups=groups))
        self.assertIn("BAD Groups: odd: Side '?' is not one of table, C, none, unused", found)

    def test_a_c_group_needs_evidence_naming_its_write_log_group(self):
        for evidence in ("-", "", "fixture.txt"):
            groups = [g.replace("| g | items |", "| g | %s |" % evidence) for g in GROUPS]
            self.assertEqual(self.check(_inventory(groups=groups)), [
                "BAD Groups: items: a C group whose Evidence does not name its write-log group "
                "items"], evidence)

    def test_rom_content_is_never_an_extraction(self):
        groups = [g.replace("| g | items |", "| g | items, extract:rooms |") for g in GROUPS]
        coverage = ["| items | `spot.patch(rom, spot.item)` | bank 0x01 | * | extract:types | "
                    "decides the item |"]
        self.assertEqual(self.check(_inventory(groups=groups, coverage=coverage)), [
            "BAD Groups: items: Evidence extract:rooms: ROM content is never committed, use rom",
            "BAD Read coverage: items `spot.patch(rom, spot.item)`: kind 'extract:types' is not "
            "one of copy, written, computed, table, rom, cut:<id>"])
        groups = [g.replace("| g | items |", "| g | items, rom |") for g in GROUPS]
        coverage = ["| items | `spot.patch(rom, spot.item)` | bank 0x01 | * | rom | "
                    "decides the item |"]
        self.assertEqual(self.check(_inventory(groups=groups, coverage=coverage)), [])

    def test_coverage_kinds_and_groups(self):
        coverage = ["| items | * | bank 0x01 | 0x0010..0x0012 | %s | r |" % kind
                    for kind in ("copy", "written", "computed", "table", "rom", "cut:entrances")]
        self.assertEqual(self.check(_inventory(coverage=coverage)), [])
        bad = ["| items | * | bank 0x01 | * | guessed | r |",
               "| nowhere | * | bank 0x01 | * | copy | r |"]
        self.assertEqual(self.check(_inventory(coverage=bad)), [
            "BAD Read coverage: items *: kind 'guessed' is not one of copy, written, computed, "
            "table, rom, cut:<id>",
            "BAD Read coverage: nowhere *: group nowhere is not in Groups"])

    def test_blind_spots_match_read_trace_one_to_one(self):
        self.assertEqual(self.check(_inventory(blind=[])), [
            "MISSING Blind spots: utils.py: data = memoryview(b)"])
        extra = BLIND + ["| rom.py | `f.write(bank)` | output |"]
        self.assertEqual(self.check(_inventory(blind=extra)), [
            "STALE Blind spots: rom.py: f.write(bank)"])
        twice = BLIND + BLIND
        self.assertEqual(self.check(_inventory(blind=twice)), [
            "STALE Blind spots: utils.py: data = memoryview(b)"])

    def test_a_blind_spot_needs_a_verdict(self):
        blind = ["| utils.py | `data = memoryview(b)` |  |"]
        self.assertEqual(self.check(_inventory(blind=blind)), [
            "BAD Blind spots: utils.py: data = memoryview(b): empty Verdict"])

    def test_a_missing_table(self):
        text = _inventory().replace("## Read coverage", "## Read cover")
        self.assertEqual(self.check(text), ["BAD Read coverage: no table under ## Read coverage"])


class Main(Fixture):
    def test_pass_prints_the_summary(self):
        code, out = self.run_main(_inventory())
        self.assertEqual(code, 0, out)
        self.assertRegex(out.splitlines()[-1],
                         r"^inventory: 18 rows in 6 sections match \S")

    def test_finding_exits_1(self):
        code, out = self.run_main(_inventory(GENERATE_ROWS[1:]))
        self.assertEqual(code, 1)
        self.assertIn("MISSING generator.py generateRom: rom = ROMWithTables(", out)

    def test_the_cli_checks_the_named_tables_too(self):
        code, out = self.run_main(_inventory(blind=[]))
        self.assertEqual(code, 1)
        self.assertIn("MISSING Blind spots: utils.py: data = memoryview(b)", out)
        groups = [g for g in GROUPS if not g.startswith("| debug ")]
        code, out = self.run_main(_inventory(groups=groups))
        self.assertEqual(code, 1)
        self.assertIn("group debug is not in Groups", out)

    def test_default_inventory_is_the_committed_draft(self):
        self.assertEqual(os.path.relpath(inventory_check.DEFAULT_INVENTORY,
                                         os.path.join(HERE, "..", "..", "..")),
                         os.path.join("notes", "ladx-inventory.md"))

    def test_no_checkout_is_setup(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = inventory_check.main(["--ladxr", os.path.join(self.dir, "none")])
        self.assertEqual(code, 2)
        self.assertIn("\nsetup: inventory: no LADXR checkout", out.getvalue())

    def test_print_keeps_the_draft_columns(self):
        code, out = self.run_main(_inventory(), "--print")
        self.assertEqual(code, 0)
        self.assertEqual(inventory_check.parse_inventory(out),
                         inventory_check.parse_inventory(_inventory()))
        self.assertIn("| unused | debug |", out)

    def test_self_test_passes(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = inventory_check.self_test()
        self.assertEqual(code, 0, out.getvalue())
        self.assertRegex(out.getvalue(), r"inventory: self-test, \d+ planted inventories as "
                                         r"expected")


if __name__ == "__main__":
    unittest.main()
