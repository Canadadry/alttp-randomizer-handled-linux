"""Usage: python3 -I test_tables.py

Unit tests of tables.py, the patch tables' format (data/ladx/), on planted files and trees.
"""

import contextlib
import importlib.util
import io
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("tables", os.path.join(HERE, "tables.py"))
tables = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tables)

GROUP = """ladx-tables 1
group fixed
tables texts
unit fixed.1
E
B 10 =2 00
B 20 @100+4
unit fixed.2 textmode asm:ITEM_DIALOGS
E "" 0
B 30 0102
T texts 2 @200+3 ff
E fast 1
nothing
"""

ORDER = """ladx-tables-order 1
section generator.py generateRom
table fixed fixed.1 patches.core.fixed(rom)
C items - itempool.run(rom, 'a b')
table fixed fixed.2 patches.core.text(rom, settings)
section rom.py ROM.save
"""


class Format(unittest.TestCase):

    def test_parse_then_render_is_the_identity(self):
        for text in (GROUP, ORDER):
            self.assertEqual(tables.render(tables.parse(text)), text)

    def test_the_entry_records_are_parsed_as_a_write_log_segment(self):
        unit = tables.parse(GROUP).units[1]
        self.assertEqual(unit.entries[0].banks, {0x30: -2, 0x31: -3})
        self.assertEqual(unit.entries[0].entries, {("texts", 2): [0x200, 0x201, 0x202, -256]})
        self.assertEqual((unit.entries[1].banks, unit.entries[1].entries), ({}, {}))

    def assert_refused(self, text, base, old, new):
        self.assertIn(old, base)
        with self.assertRaises(tables.TablesError, msg=text):
            tables.parse(base.replace(old, new, 1))

    def test_a_group_file_not_in_canonical_form_is_refused(self):
        for text, old, new in [
            ("no final newline", "nothing\n", "nothing"),
            ("version", "ladx-tables 1", "ladx-tables 2"),
            ("no group line", "group fixed\n", ""),
            ("bad group name", "group fixed", "group fi/xed"),
            ("bad table name", "tables texts", "tables te/xts"),
            ("a repeated table", "tables texts", "tables texts texts"),
            ("an entry before a unit", "unit fixed.1\n", ""),
            ("a unit id of another group", "unit fixed.2", "unit options.2"),
            ("units not numbered from 1", "unit fixed.1", "unit fixed.0"),
            ("units out of number order", "unit fixed.2", "unit fixed.3"),
            ("a repeated component", "textmode asm:ITEM_DIALOGS", "textmode textmode"),
            ("a bad component", "asm:ITEM_DIALOGS", "asm:item_dialogs"),
            ("a bad component name", "textmode asm", "text/mode asm"),
            ("too few values", 'E "" 0', 'E ""'),
            ("too many values", "E\n", "E 1\n"),
            ("an empty value", 'E "" 0', 'E  0'),
            ("entries out of order", 'E "" 0', "E zzz 0"),
            ("a unit without an entry", "E\nB 10 =2 00\nB 20 @100+4\n", ""),
            ("an entry without a record", "nothing\n", ""),
            ("nothing and a record", "nothing\n", "nothing\nB 40 00\n"),
            ("a record after nothing", "E fast 1\nnothing", "E fast 1\nnothing\nB 50 00"),
            ("a bad record", "B 30 0102", "B 30 0x0102"),
            ("records out of order", "B 20 @100+4", "B 5 @100+4"),
            ("a record not maximal", "B 20 @100+4", "B 12 @100+4"),
            ("a G line", "B 30 0102\n", "G fixed\nB 30 0102\n"),
            ("an I line", "B 30 0102\n", "B 30 0102\nI 1 0+2\n"),
            ("a record before an entry", "unit fixed.1\nE\n", "unit fixed.1\nB 8 00\nE\n"),
            ("an unknown line", "nothing\n", "nothing\nX\n"),
        ]:
            self.assert_refused(text, GROUP, old, new)

    def test_an_order_file_not_in_canonical_form_is_refused(self):
        for text, old, new in [
            ("version", "ladx-tables-order 1", "ladx-tables-order 0"),
            ("a row before a section", "section generator.py generateRom\n", ""),
            ("a repeated section", "section rom.py ROM.save", "section generator.py generateRom"),
            ("a bad side", "C items", "none items"),
            ("a C row with a unit", "C items -", "C items items.1"),
            ("a table row without a unit", "table fixed fixed.1", "table fixed -"),
            ("a unit of another group", "table fixed fixed.1", "table fixed options.1"),
            ("a bad unit id", "table fixed fixed.1", "table fixed fixed.x"),
            ("no call", "table fixed fixed.1 patches.core.fixed(rom)", "table fixed fixed.1"),
            ("a padded call", "fixed.1 patches", "fixed.1  patches"),
            ("a trailing space", "(rom)\n", "(rom) \n"),
        ]:
            self.assert_refused(text, ORDER, old, new)


FUSED_ORDER = """ladx-tables-order 1
section generator.py generateRom
table fixed fixed.1 patches.core.fixed(rom)
C items - itempool.run(rom, 'a b')
table fixed fixed.2 patches.core.text(rom, settings)
table fixed fixed.2 patches.core.more(rom)
C items - itempool.run(rom, 'a b') #2
section rom.py ROM.save
C header - rom.fixHeader()
"""


class Select(unittest.TestCase):

    def setUp(self):
        self.tables = tables.Tables(tables.parse(FUSED_ORDER), {"fixed": tables.parse(GROUP)})

    def steps(self, values):
        return [(row.side, row.key, entry and entry.lines)
                for row, entry in tables.select(self.tables, values)]

    def test_a_unit_is_applied_at_its_first_row_between_the_c_rows(self):
        self.assertEqual(self.steps({"textmode": '""', "asm:ITEM_DIALOGS": "0"}), [
            ("table", "patches.core.fixed(rom)", ["B 10 =2 00", "B 20 @100+4"]),
            ("C", "itempool.run(rom, 'a b')", None),
            ("table", "patches.core.text(rom, settings)", ["B 30 0102", "T texts 2 @200+3 ff"]),
            ("C", "itempool.run(rom, 'a b') #2", None),
            ("C", "rom.fixHeader()", None),
        ])

    def test_an_entry_that_writes_nothing_is_still_a_step(self):
        self.assertEqual(self.steps({"textmode": "fast", "asm:ITEM_DIALOGS": "1", "race": "1"})[2],
                         ("table", "patches.core.text(rom, settings)", []))

    def test_a_missing_component_or_entry_is_refused(self):
        for values in ({"textmode": "fast"}, {"textmode": "fast", "asm:ITEM_DIALOGS": "0"}):
            with self.assertRaises(tables.TablesError, msg=values):
                tables.select(self.tables, values)

    def test_a_unit_missing_from_its_group_file_is_refused(self):
        self.tables.groups["fixed"].units.pop()
        with self.assertRaises(tables.TablesError):
            tables.select(self.tables, {"textmode": "fast", "asm:ITEM_DIALOGS": "1"})



def _table(columns, rows):
    return ("| " + " | ".join(columns) + " |\n|" + "---|" * len(columns) + "\n"
            + "".join("| " + " | ".join(cells) + " |\n" for cells in rows))


def _section(name, rows):
    """An inventory section of rows [(call, settings, side, group)]."""
    return "## %s\n\n%s\n" % (name, _table(
        ["Call", "Settings", "Args", "RNG (grep)", "Reads (grep)", "Reads (trace)", "Notes",
         "Side", "Group"],
        [[call if call.startswith("`") else "`%s`" % call, settings, "-", "-", "-", "-", "",
          side, group] for call, settings, side, group in rows]))


INVENTORY = ("# planted inventory\n\n" + _section("generator.py generateRom", [
    ("patches.core.fixed(rom)", "-", "table", "fixed"),
    ("itempool.run(rom, 'a b')", "-", "C", "items"),
    ("x = 1", "-", "none", "locals"),
    ("patches.core.text(rom, settings)", "textmode", "table", "fixed"),
    ("patches.core.more(rom)", "-", "table", "fixed"),
    ("`itempool.run(rom, 'a b')` #2", "-", "C", "items"),
    ("patches.options.beep(rom, settings.lowhpbeep)", "lowhpbeep", "table", "options"),
]) + _section("rom.py ROM.save", [("rom.fixHeader()", "-", "C", "header")])
    + "## Groups\n\n" + _table(["Group", "Side", "Settings key", "Depends on", "Evidence"], [
        ["fixed", "table", "-", "-", "-"], ["header", "C", "-", "-", "header"],
        ["items", "C", "-", "-", "items"], ["locals", "none", "-", "-", "-"],
        ["options", "table", "lowhpbeep", "-", "-"]])
    + "\n## Read coverage\n\n" + _table(["Group", "Row", "Target", "Range", "Kind", "Reason"], [])
    + "\n## Blind spots\n\n" + _table(["File", "Line", "Verdict"], []))

SETTINGS = """# planted settings.txt
setting race\tMain\tV\tbool\t0\tRace mode\t0\t-
setting textmode\tUser options\tf\tstr\tfast\tText mode\t1\t-
option textmode\tfast\t""\tFast\t-
option textmode\t""\tn\tNone\t-
setting lowhpbeep\tUser options\tp\tstr\tslow\tLow HP beeps\t1\t-
option lowhpbeep\tnone\tD\tDisabled\t-
option lowhpbeep\tslow\tS\tSlow\t-
"""

TIER1 = "race all\ntextmode all\n"

OPTIONS = """ladx-tables 1
group options
tables texts
unit options.1 lowhpbeep race count
E slow 0 12
B 40 00
E slow 1 7
nothing
"""

TREE = {"order.txt": FUSED_ORDER.replace(
    "C items - itempool.run(rom, 'a b') #2\n",
    "C items - itempool.run(rom, 'a b') #2\n"
    "table options options.1 patches.options.beep(rom, settings.lowhpbeep)\n"),
    "fixed.txt": GROUP, "options.txt": OPTIONS}


class Check(unittest.TestCase):

    def setUp(self):
        self.root = tempfile.mkdtemp()
        for name, text in TREE.items():
            self.write(name, text)

    def tearDown(self):
        shutil.rmtree(self.root)

    def write(self, name, text):
        with open(os.path.join(self.root, name), "w", newline="\n") as f:
            f.write(text)

    def edit(self, name, old, new):
        self.assertIn(old, TREE[name])
        self.write(name, TREE[name].replace(old, new, 1))

    def check(self):
        return tables.check(self.root, INVENTORY, TIER1, SETTINGS)

    def test_a_correct_tree_gives_its_summary(self):
        size = sum(len(t) for t in TREE.values())
        self.assertEqual(self.check(), ([], "ladx-tables: 3 units in 2 groups, 5 entries, "
                                            "%d bytes" % size))

    def test_a_file_that_does_not_parse_is_badfile(self):
        self.edit("fixed.txt", "B 30 0102", "B 30 0x0102")
        lines = self.check()[0]
        self.assertEqual(len(lines), 1)
        self.assertRegex(lines[0], r"^BADFILE fixed\.txt: line 10: bad token '0x0102'$")

    def test_a_group_file_under_another_name_is_badfile(self):
        self.write("options.txt", OPTIONS.replace("group options", "group fixed").replace(
            "options.1", "fixed.1"))
        self.assertEqual(self.check()[0], ["BADFILE options.txt: holds group fixed"])

    def test_a_missing_or_extra_file(self):
        os.remove(os.path.join(self.root, "options.txt"))
        self.write("notes.md", "# prose\n")
        os.mkdir(os.path.join(self.root, "sub"))
        self.assertEqual(self.check()[0], ["MISSING options.txt", "EXTRA notes.md", "EXTRA sub"])

    def test_order_rows_missing_extra_or_on_another_side(self):
        self.edit("order.txt", "C items - itempool.run(rom, 'a b')\n",
                  "C items - itempool.run(rom, 'a c')\n")
        self.assertEqual(self.check()[0], [
            "MISSING order.txt: generator.py generateRom: itempool.run(rom, 'a b')",
            "EXTRA order.txt: generator.py generateRom: itempool.run(rom, 'a c')"])
        self.edit("order.txt", "C header - rom.fixHeader()", "C items - rom.fixHeader()")
        self.assertEqual(self.check()[0], [
            "BADROW order.txt: rom.py ROM.save: rom.fixHeader(): C items, inventory C header"])

    def test_order_rows_out_of_order(self):
        self.edit("order.txt", "C items - itempool.run(rom, 'a b')\n"
                  "table fixed fixed.2 patches.core.text(rom, settings)\n",
                  "table fixed fixed.2 patches.core.text(rom, settings)\n"
                  "C items - itempool.run(rom, 'a b')\n")
        self.assertEqual(self.check()[0], [
            "EXTRA order.txt: generator.py generateRom: patches.core.text(rom, settings)",
            "MISSING order.txt: generator.py generateRom: patches.core.text(rom, settings)"])

    def test_sections_other_than_the_inventory_s(self):
        self.edit("order.txt", "section rom.py ROM.save\n", "section rom.py ROM.load\n")
        self.assertEqual(self.check()[0], [
            "BADROW order.txt: sections generator.py generateRom, rom.py ROM.load, expected "
            "generator.py generateRom, rom.py ROM.save",
            "MISSING order.txt: rom.py ROM.save: rom.fixHeader()",
            "EXTRA order.txt: rom.py ROM.load: rom.fixHeader()"])

    def test_a_row_naming_a_unit_its_group_file_lacks(self):
        self.edit("order.txt", "table fixed fixed.2 patches.core.more(rom)",
                  "table fixed fixed.3 patches.core.more(rom)")
        self.assertEqual(self.check()[0], [
            "BADROW order.txt: patches.core.more(rom): unit fixed.3 is not in fixed.txt"])

    def test_a_unit_without_a_row(self):
        self.edit("order.txt", "table fixed fixed.1 patches.core.fixed(rom)",
                  "table fixed fixed.2 patches.core.fixed(rom)")
        self.assertEqual(self.check()[0], ["BADROW fixed.txt: unit fixed.1 has no row"])

    def test_units_numbered_out_of_their_first_row_order(self):
        self.write("order.txt", TREE["order.txt"].replace("fixed.1", "fixed.X").replace(
            "fixed.2", "fixed.1").replace("fixed.X", "fixed.2"))
        self.assertEqual(self.check()[0], [
            "BADROW order.txt: patches.core.text(rom, settings): unit fixed.1 first used after "
            "fixed.2"])

    def test_fused_rows_not_consecutive_among_the_table_rows(self):
        self.edit("order.txt", "table fixed fixed.2 patches.core.text(rom, settings)\n"
                  "table fixed fixed.2 patches.core.more(rom)\n",
                  "table fixed fixed.2 patches.core.text(rom, settings)\n"
                  "table fixed fixed.1 patches.core.more(rom)\n")
        self.assertEqual(self.check()[0], [
            "BADROW order.txt: unit fixed.1's rows are not consecutive among the table rows"])

    def test_a_c_row_between_fused_rows_keeps_them_consecutive(self):
        self.edit("order.txt", "table fixed fixed.2 patches.core.more(rom)\n"
                  "C items - itempool.run(rom, 'a b') #2\n",
                  "C items - itempool.run(rom, 'a b') #2\n"
                  "table fixed fixed.2 patches.core.more(rom)\n")
        self.assertEqual(self.check()[0], [
            "EXTRA order.txt: generator.py generateRom: itempool.run(rom, 'a b') #2",
            "MISSING order.txt: generator.py generateRom: itempool.run(rom, 'a b') #2"])

    def test_a_duplicate_tuple(self):
        self.edit("options.txt", "E slow 1 7", "E slow 0 12")
        self.assertEqual(self.check()[0], ["DUP options.txt: unit options.1: slow 0 12"])

    def test_values_outside_their_component_s_domain(self):
        self.edit("fixed.txt", 'E "" 0', 'E default 0')
        self.write("options.txt", OPTIONS.replace("E slow 0 12", "E none 0 12").replace(
            "E slow 1 7", "E slow 2 07"))
        self.assertEqual(self.check()[0], [
            "BADKEY fixed.txt: unit fixed.2: default 0: textmode=default not a tier-1 value",
            "BADKEY options.txt: unit options.1: none 0 12: lowhpbeep=none not the default of "
            "a setting outside tier 1",
            "BADKEY options.txt: unit options.1: slow 2 07: race=2 not a tier-1 value",
            "BADKEY options.txt: unit options.1: slow 2 07: count=07 not a decimal count"])

    def test_a_constant_s_value_is_0_or_1(self):
        self.edit("fixed.txt", "E fast 1", "E fast 2")
        self.assertEqual(self.check()[0], [
            "BADKEY fixed.txt: unit fixed.2: fast 2: asm:ITEM_DIALOGS=2 not 0 or 1"])

    def test_a_c_groups_constants_file_is_accepted_and_checked_but_matched_to_no_row(self):
        self.write("items.txt", "ladx-tables 1\ngroup items\ntables texts\nunit items.1\nE\n"
                                "B 9 01\nunit items.2 asm:ITEM_DIALOGS\nE 0\nB a 02\nE 0\nB a 03\n")
        self.assertEqual(self.check()[0], ["DUP items.txt: unit items.2: 0"])

    def test_a_file_for_a_group_of_another_side_is_extra(self):
        self.write("locals.txt", "ladx-tables 1\ngroup locals\ntables texts\nunit locals.1\nE\n"
                                 "nothing\n")
        self.assertEqual(self.check()[0], ["EXTRA locals.txt"])

    def test_a_component_that_is_no_setting(self):
        self.write("options.txt", OPTIONS.replace("lowhpbeep race", "beep race").replace(
            "slow", "x"))
        self.assertEqual(self.check()[0], [
            "BADKEY options.txt: unit options.1: component beep is no setting"])



class Main(unittest.TestCase):

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.root = os.path.join(self.dir, "ladx")
        os.mkdir(self.root)
        for name, text in TREE.items():
            self.write(os.path.join("ladx", name), text)
        self.inputs = os.path.join(self.dir, "inputs")
        os.mkdir(self.inputs)
        self.write(os.path.join("inputs", "tier1.txt"), TIER1)
        self.write(os.path.join("inputs", "settings.txt"), SETTINGS)
        self.write("inventory.md", INVENTORY)

    def tearDown(self):
        shutil.rmtree(self.dir)

    def write(self, rel, text):
        with open(os.path.join(self.dir, rel), "w", newline="\n") as f:
            f.write(text)

    def main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = tables.main(list(args) or ["check", "--root", self.root, "--inventory",
                                              os.path.join(self.dir, "inventory.md"),
                                              "--inputs", self.inputs])
        return code, out.getvalue(), err.getvalue()

    def test_a_correct_tree_prints_its_summary(self):
        code, out, _ = self.main()
        self.assertEqual((code, out), (0, "ladx-tables: 3 units in 2 groups, 5 entries, %d bytes\n"
                                       % sum(len(t) for t in TREE.values())))

    def test_a_mismatch_prints_its_lines_and_exits_1(self):
        os.remove(os.path.join(self.root, "options.txt"))
        code, out, _ = self.main()
        self.assertEqual((code, out), (1, "MISSING options.txt\n"))

    def test_a_malformed_input_exits_2(self):
        os.remove(os.path.join(self.dir, "inventory.md"))
        code, out, err = self.main()
        self.assertEqual((code, out), (2, ""))
        self.assertIn("inventory.md", err)
        self.write("inventory.md", "## Groups\n\n| Group |\n|---|\n")
        self.assertEqual(self.main()[0], 2)

    def test_a_usage_error_exits_2(self):
        self.assertEqual(self.main("list")[0], 2)

    def test_the_self_test_passes(self):
        self.assertIsNone(tables.self_test())

    def test_the_self_test_fails_when_the_check_misses_a_kind(self):
        real = tables.check
        try:
            for kind in ("MISSING", "EXTRA", "BADFILE", "BADROW", "DUP", "BADKEY"):
                tables.check = lambda *a, kind=kind: (
                    [l for l in real(*a)[0] if not l.startswith(kind + " ")], real(*a)[1])
                self.assertIsNotNone(tables.self_test(), kind)
                code, out, _ = self.main()
                self.assertEqual(code, 1)
                self.assertTrue(out.startswith("ladx-tables: self-test failed: "), out)
        finally:
            tables.check = real


if __name__ == "__main__":
    unittest.main()
