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


def _row(call, settings="-", args="-", rng="-", reads="-", side="table", group="g"):
    return "| %s | %s | %s | %s | %s |  | %s | %s |" % (call, settings, args, rng, reads, side,
                                                       group)


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
    _row("`spot.patch(rom, spot.item)`", reads="placement"),
    _row('`rom.patch(0, 0x0003, "00", "01")`', args="romdebugmode", side="unused"),
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


def _inventory(generate_rows=None):
    rows = GENERATE_ROWS if generate_rows is None else generate_rows
    return "# Inventory\n\nFree text.\n\n" + "\n".join(
        [_table("generator.py generateRom", rows)] + OTHER_TABLES)


class Fixture(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="inventory-check-test-")
        self.addCleanup(shutil.rmtree, self.dir)
        self.ladxr = os.path.join(self.dir, "ladxr")
        os.makedirs(os.path.join(self.ladxr, "patches"))
        for name, text in (("randomizer.py", RANDOMIZER), ("romTables.py", ROM_TABLES),
                           ("rom.py", ROM), ("hints.py", HINTS),
                           ("patches/planted.py", PATCHES)):
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
        self.assertEqual(self.check(rendered), [])


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

    def test_bad_side_is_refused(self):
        rows = [r.replace("| table | g |", "| maybe | g |", 1) if "fixed(rom)` |" in r else r
                for r in GENERATE_ROWS]
        self.assertEqual(self.check(_inventory(rows)), [
            "BAD generator.py generateRom: patches.planted.fixed(rom): Side (draft) 'maybe' is "
            "not one of table, C, none, unused, ?"])

    def test_empty_group_is_refused(self):
        rows = [r.replace("| table | g |", "| table |  |") if "fixed(rom)` |" in r else r
                for r in GENERATE_ROWS]
        self.assertEqual(self.check(_inventory(rows)), [
            "BAD generator.py generateRom: patches.planted.fixed(rom): empty Group (draft)"])

    def test_missing_section_lists_every_row(self):
        text = _inventory().replace("## rom.py ROM.fixHeader", "## rom.py ROM.fixHeaders")
        self.assertEqual(self.check(text), [
            "MISSING rom.py ROM.fixHeader: self.banks[0][0x14D] = 0"])


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
        self.assertEqual(self.check(out), [])
        self.assertIn("| unused | g |", out)

    def test_self_test_passes(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = inventory_check.self_test()
        self.assertEqual(code, 0, out.getvalue())
        self.assertRegex(out.getvalue(), r"inventory: self-test, \d+ planted inventories as "
                                         r"expected")


if __name__ == "__main__":
    unittest.main()
