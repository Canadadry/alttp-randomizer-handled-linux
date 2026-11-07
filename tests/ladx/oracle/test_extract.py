"""Usage: python3 -I test_extract.py

Unit tests of extract.py, the extraction fixtures' format (tests/ladx/extract/), on planted
files, trees and a synthetic ROM; no ROM, no LADXR.
"""

import contextlib
import importlib.util
import io
import os
import random
import shutil
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("extract", os.path.join(HERE, "extract.py"))
extract = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(extract)


BANK = 0x4000
Table = extract.known_source.Table
LAYOUT = [
    Table("rooms", {"count": 2, "pointers_bank": 1, "pointers_addr": 0, "data_bank": 1,
                    "alt_pointers": {"Alt06": (0, 0x10)}}, "rooms", "all"),
    Table("entities", {"count": 3, "pointers_bank": 2, "pointers_addr": 0, "data_bank": 2},
          "data", range(3)),
    Table("texts", {"count": 2, "pointers_bank": 3, "pointers_addr": 0, "banks_bank": 3,
                    "banks_addr": 0x200}, "text", None),
    Table("sprites", {"count": 3, "pointers_bank": 5, "pointers_addr": 0, "data_bank": 5,
                      "data_addr": 0x100, "data_size": 4}, "sprite", "all"),
    Table("backgrounds", {"count": 2, "pointers_bank": 5, "pointers_addr": 0x200,
                          "data_bank": 5}, "background", [1]),
]
BOSSES = extract.known_source.Bosses(zero_bank=5, zero_addr=0x800, zero_count=4, item_bank=5,
                                     item_addr=0x900, rooms=[1, 3])
FP = "=" + "r".join(str(n) for n in range(1, 9))
TREE = {
    "room-entities": "=1\n=1r0r0r0=1r2r1r0r0r3=1r2r5\n=1r0r0r1\n=1r0r0r2=0r2r1r3r0r2\n",
    "room-objects": "=1\n=0r1r0=1r1r3r0r0r6=9r10=2r2r5r4r7\n"
                    "=0r0r0r0=1r1r1r0r0r13=1r2=0r4r3r5=1r3r2r1r6=3r1r16r32r48r64\n"
                    "=0r0r0r1=1r1r1r13r0r3=3r4\n",
    "backgrounds": "=1\n=4r0r0r1=1r5r3r0r0r11=152r0r0r0r2r1r2r3=152r32r1r1r1r7\n",
    "boss-sources": "=1\n=5r8r0r4=8r0=8r2\n=5r9r0=0r1r17=0r3r34\n",
    "source-flags": "=1\n=1=0=1\n",
    "save-layout": "=1\n=0\n=1r1r0r1r16=1r3r0r3r6\n=1r0=1r1r1r3r0r0r6\n"
                   "=0r0r0=1r1r1r1r0r0r13=0\n=0r0r1=1r1r1r1r13r0r3=0\n"
                   "=1\n=2r1r0r1r5\n=0r0r0=1r1r2r1r0r0r3=0\n=0r0r1=0r1r2r18r52\n"
                   "=0r0r2=1r1r2r1r3r0r2=0\n"
                   "=2\n=4r1r0r1r5\n=0r0r0=1r3r4r1r0r0r3=0" + FP * 3 + "\n"
                   "=0r0r1=1r1r4r1r3r0r2=0" + FP * 2 + "\n"
                   "=3\n=5r1r0r1r12\n=0r0r0=1r1r5r1r0r0r4=0=48r49r50r51\n"
                   "=0r0r1=1r1r5r1r4r0r4=0=52r53r54r55\n=0r0r2=1r1r5r1r8r0r4=0=56r57r58r59\n"
                   "=4\n=5r3r0r64r0\n=0r0r0=0r1r5r18r52\n=0r0r1=1r1r5r3r0r0r11=0" + FP * 11
                   + "\n",
}


def read(name, text=None):
    return extract.read(name, TREE[name] if text is None else text, LAYOUT, BOSSES)


class Format(unittest.TestCase):
    TEXT = "=1\n=0r1r2=255\n=12=3r40r100\n"

    def test_parse_then_render_is_the_identity(self):
        lines = extract.parse(self.TEXT)
        self.assertEqual(lines, [[[1]], [[0, 1, 2], [255]], [[12], [3, 40, 100]]])
        self.assertEqual(extract.render(lines), self.TEXT)

    def test_a_text_not_in_canonical_form_is_refused(self):
        for old, new in [("=255", "=256"), ("=12", "=012"), ("=12", "12"), ("=12", "= 12"),
                         ("=12", "=x12"), ("r40", "rr40"), ("=255\n", "=255=\n"),
                         ("=255\n", "=255\n\n"), ("r100\n", "r100"), ("=1\n", "=2\n"),
                         ("=1\n", "=1r0\n"), ("=12=", "=12r=")]:
            with self.subTest(old=old, new=new):
                with self.assertRaises(extract.ExtractError):
                    extract.parse(self.TEXT.replace(old, new, 1))

    def test_render_refuses_a_number_out_of_range_or_an_empty_token(self):
        for lines in ([[[1]], [[256]]], [[[1]], [[-1]]], [[[1]], [[]]], [[[1]], []]):
            with self.subTest(lines=lines):
                with self.assertRaises(extract.ExtractError):
                    extract.render(lines)


class Read(unittest.TestCase):
    def test_every_file_reads_then_writes_back_the_same_text(self):
        for name in extract.NAMES:
            with self.subTest(name=name):
                self.assertEqual(extract.write(name, read(name), LAYOUT), TREE[name])

    def test_room_entities_give_each_room_s_span_and_entities(self):
        Entry = extract.Entry
        self.assertEqual(read("room-entities"), {
            (1, 0): Entry(1, 2, 0x100, 3, [(1, 2, 5)]), (1, 1): None,
            (1, 2): Entry(0, 2, 0x103, 2, None)})

    def test_room_objects_give_alt_rooms_first_with_their_fields(self):
        Entry = extract.Entry
        objects = read("room-objects")
        self.assertEqual(list(objects), [(0, "Alt06"), (0, 0), (0, 1)])
        self.assertEqual(objects[(0, 0)], Entry(1, 1, 0x100, 13, [
            1, 2, ("s", 4, 3, 5), ("h", 3, 2, 1, 6), ("w", 1, 0x10, 0x20, 0x30, 0x40)]))
        self.assertEqual(objects[(0, "Alt06")].parts, [9, 10, ("v", 2, 5, 4, 7)])

    def test_backgrounds_give_each_run_of_the_tile_map(self):
        self.assertEqual(read("backgrounds")[(4, 1)].parts, [(0x9800, 0, 0, 2, [1, 2, 3]),
                                                             (0x9820, 1, 1, 1, [7])])

    def test_boss_sources_and_source_flags(self):
        self.assertEqual(read("boss-sources"), extract.BossSources(
            5, 0x800, 4, [0x800, 0x802], 5, 0x900, [(1, 17), (3, 34)]))
        self.assertEqual(read("source-flags"), (1, 0, 1))

    def test_save_layout_gives_each_table_s_regions_and_entries(self):
        sections = read("save-layout")
        self.assertEqual([s.table for s in sections], [0, 1, 2, 3, 4])
        self.assertEqual(sections[0].regions, [(1, 0x100, 0x110), (1, 0x300, 0x306)])
        self.assertEqual(list(sections[0].entries), ["Alt06", 0, 1])
        Save = extract.SaveEntry
        self.assertEqual(sections[1].entries[1], Save(1, 2, None, None, 0x1234, None, None))
        self.assertEqual(sections[2].entries[1], Save(1, 4, 0x103, 2, None, None,
                                                      [tuple(range(1, 9))] * 2))
        self.assertEqual(sections[3].entries[2], Save(1, 5, 0x108, 4, None, None,
                                                      [56, 57, 58, 59]))
        self.assertEqual(sections[4].regions, [(5, 0x300, 0x4000)])

    def assert_bad(self, name, old, new, reason=None):
        self.assertIn(old, TREE[name])
        with self.assertRaises(extract.ExtractError) as caught:
            read(name, TREE[name].replace(old, new, 1))
        if reason is not None:
            self.assertEqual(str(caught.exception), reason)

    def test_a_room_missing_extra_or_out_of_order_is_refused(self):
        self.assert_bad("room-entities", "=1r0r0r1\n", "",
                        "line 3: table 1 entry 2, expected table 1 entry 1")
        self.assert_bad("room-entities", "=1r0r0r2=0r2r1r3r0r2\n", "")
        self.assert_bad("room-objects", "=0r0r0r1=1r1r1r13r0r3=3r4\n",
                        "=0r0r0r1=1r1r1r13r0r3=3r4\n=0r0r0r2=1r1r1r16r0r3=3r4\n")
        self.assert_bad("backgrounds", "=4r0r0r1", "=4r0r0r0")

    def test_fields_that_do_not_give_the_span_s_bytes_are_refused(self):
        self.assert_bad("room-entities", "=1r2r5", "=1r16r5")
        self.assert_bad("room-entities", "r0r3=1r2r5", "r0r5=1r2r5")
        self.assert_bad("room-objects", "=0r4r3r5", "=0r4r8r5")
        self.assert_bad("backgrounds", "r1r1r1r7", "r1r1r1r7r8")
        self.assert_bad("room-entities", "=0r2r1r3r0r2", "=0r2r1r3r0r2=1r2r5")

    def test_bad_boss_sources_or_flags_are_refused(self):
        self.assert_bad("boss-sources", "=8r0=8r2", "=8r2=8r0")
        self.assert_bad("boss-sources", "=8r2\n", "=8r4\n")
        self.assert_bad("boss-sources", "=0r3r34", "")
        self.assert_bad("boss-sources", "=5r8r0r4", "=5r8r0r5")
        self.assert_bad("source-flags", "=1=0=1", "=1=0=2")
        self.assert_bad("source-flags", "=1=0=1", "=1=0")

    def test_bad_save_layout_sections_or_entries_are_refused(self):
        name = "save-layout"
        self.assert_bad(name, "=3\n=5r1r0r1r12\n", "=4\n=5r1r0r1r12\n")
        self.assert_bad(name, "=1r3r0r3r6", "=1r3r0r3r6r0")
        self.assert_bad(name, "=5r3r0r64r0", "=5r3r0r64r1")
        self.assert_bad(name, "=0r0r1=0r1r2r18r52", "=0r0r1=0r1r2r18r52=0")
        self.assert_bad(name, "r0r3=0" + FP * 3, "r0r3=0" + FP * 2)
        self.assert_bad(name, "=0r0r2=1r1r5r1r8r0r4=0=56r57r58r59",
                        "=0r0r2=1r1r5r1r9r0r4=0=56r57r58r59")
        self.assert_bad(name, "=0r0r2=1r1r5r1r8r0r4=0=56r57r58r59",
                        "=0r0r2=1r1r5r1r8r0r4=0=56r57r58")
        self.assert_bad(name, "=0r0r1=1r1r4r1r3r0r2=0", "=0r0r1=1r4r4r1r3r0r2=0")
        self.assert_bad(name, "=0r0r1=1r1r1r1r13r0r3=0", "=0r0r1=1r3r1r1r13r0r3=0")

    def test_a_de_duplication_names_an_earlier_span_of_the_same_bank(self):
        name = "save-layout"
        good = TREE[name].replace("=0r0r2=1r1r5r1r8r0r4=0", "=0r0r2=1r1r5r1r8r0r4=1r0r0r0r0")
        self.assertEqual(read(name, good)[3].entries[2].dedup, (0, 0))
        text = TREE[name].replace("=0r0r1=1r1r4r1r3r0r2=0", "=0r0r1=1r1r4r1r3r0r2=1r0r0r0r1")
        self.assertEqual(read(name, text)[2].entries[1].dedup, (0, 1))
        for dedup in ("=1r0r0r0r3", "=1r0r1r0r0", "=1r0r2r0r0", "=2"):
            with self.subTest(dedup=dedup):
                self.assert_bad(name, "=0r0r1=1r1r4r1r3r0r2=0", "=0r0r1=1r1r4r1r3r0r2" + dedup)
        self.assert_bad(name, "=0r0r2=1r1r5r1r8r0r4=0", "=0r0r2=1r1r5r1r8r0r4=1r0r0r0r1")
    def test_a_short_key_is_refused(self):
        self.assert_bad("room-entities", "=1r0r0r1\n", "=1\n", "line 3: no key")


def synthetic_rom():
    """A ROM whose bytes TREE describes under LAYOUT and BOSSES, the rest random."""
    rom = bytearray(random.Random(0x5EED).randbytes(6 * BANK))

    def put(bank, addr, data):
        rom[bank * BANK + addr:bank * BANK + addr + len(data)] = bytes(data)

    def pointers(bank, addr, values):
        put(bank, addr, b"".join(struct.pack("<H", v) for v in values))

    pointers(1, 0, [0x4100, 0x410D])
    put(1, 0x100, [1, 2, 0x34, 5, 0x83, 0x12, 6, 0xE1, 0x10, 0x20, 0x30, 0x40, 0xFE, 3, 4, 0xFE])
    pointers(0, 0x10, [0x4300])
    put(1, 0x300, [9, 10, 0xC2, 0x45, 7, 0xFE])
    pointers(2, 0, [0x4100, 0x1234, 0x4103])
    put(2, 0x100, [0x21, 0x05, 0xFF, 0x12, 0xFF])
    pointers(3, 0, [0x4100, 0x4103])
    put(3, 0x200, [0x04, 0x44])
    put(4, 0x100, [0x41, 0x42, 0xFF, 0x43, 0xFE])
    put(5, 0, [0, 1, 2])
    put(5, 0x100, range(0x30, 0x3C))
    pointers(5, 0x200, [0x1234, 0x4300])
    put(5, 0x300, [0x98, 0x00, 0x02, 0x01, 0x02, 0x03, 0x98, 0x20, 0xC1, 0x07, 0x00])
    put(5, 0x800, [0x00, 0x09, 0x00, 0x08])
    put(5, 0x901, [17])
    put(5, 0x903, [34])
    return bytes(rom)


def write_tree(root, tree):
    os.makedirs(root, exist_ok=True)
    for name, text in tree.items():
        with open(os.path.join(root, name + ".txt"), "w", newline="\n") as f:
            f.write(text)


class KnownSource(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        write_tree(self.dir, TREE)
        self.rom = synthetic_rom()
        self.fixtures = extract.load(self.dir, LAYOUT, BOSSES)

    def tearDown(self):
        shutil.rmtree(self.dir)

    def test_the_rebuilt_spans_are_known_source_s_with_the_rom_s_bytes(self):
        ks = extract.known_source.build(self.rom, LAYOUT, BOSSES)
        self.assertEqual(ks.failures, ["entities 2", "texts bank 1"])
        spans = extract.ks_spans(self.fixtures, LAYOUT)
        self.assertEqual([(start, start + len(data), label) for start, data, label in spans],
                         ks.spans)
        for start, data, label in spans:
            with self.subTest(label=label):
                self.assertEqual(data, self.rom[start:start + len(data)])

    def test_the_rebuilt_map_holds_every_ks_offset_with_its_byte(self):
        ks = extract.known_source.build(self.rom, LAYOUT, BOSSES)
        rebuilt = extract.ks_rebuild(self.fixtures, LAYOUT)
        self.assertEqual(rebuilt, {q: self.rom[q] for q in range(len(self.rom)) if q in ks})

    def test_a_tree_missing_a_file_does_not_load(self):
        os.remove(os.path.join(self.dir, "boss-sources.txt"))
        with self.assertRaises(extract.ExtractError):
            extract.load(self.dir, LAYOUT, BOSSES)


INVENTORY = """# Inventory

## Extractions

| Name | Group | Holds | Derived from | Format |
|---|---|---|---|---|
""" + "".join("| %s | g | h | d | f |\n" % name for name in extract.NAMES)


class Check(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.root = os.path.join(self.dir, "extract")
        write_tree(self.root, TREE)
        with open(os.path.join(self.dir, "inventory.md"), "w") as f:
            f.write(INVENTORY)

    def tearDown(self):
        shutil.rmtree(self.dir)

    def check(self, inventory=INVENTORY):
        return extract.check(self.root, inventory, LAYOUT, BOSSES)

    def change(self, name, old, new):
        path = os.path.join(self.root, name + ".txt")
        with open(path) as f:
            text = f.read()
        self.assertIn(old, text)
        with open(path, "w", newline="\n") as f:
            f.write(text.replace(old, new, 1))

    def test_a_correct_tree_gives_its_summary(self):
        self.assertEqual(self.check(), ([], "extract: 6 fixtures, %d bytes, every inventory "
                                            "extraction present" % sum(map(len, TREE.values()))))

    def test_a_missing_and_an_extra_file(self):
        os.remove(os.path.join(self.root, "source-flags.txt"))
        write_tree(self.root, {"notes": "=1\n"})
        os.mkdir(os.path.join(self.root, "sub"))
        self.assertEqual(self.check()[0], ["MISSING source-flags", "EXTRA notes.txt", "EXTRA sub"])

    def test_a_missing_root_is_every_name_missing(self):
        shutil.rmtree(self.root)
        self.assertEqual(self.check()[0], ["MISSING " + name for name in extract.NAMES])

    def test_a_file_that_does_not_read_is_badfile(self):
        self.change("room-objects", "=9r10", "=09r10")
        self.change("boss-sources", "=0r3r34", "")
        self.assertEqual(self.check()[0], [
            "BADFILE room-objects: line 2: not canonical",
            "BADFILE boss-sources: line 3: not the boss rooms"])

    def test_a_room_file_disagreeing_with_save_layout_is_badfile(self):
        self.change("save-layout", "=0r0r0=1r1r2r1r0r0r3=0", "=0r0r0=1r1r2r1r1r0r2=0")
        self.assertEqual(self.check()[0], [
            "BADFILE room-entities: table 1 entry 0: not save-layout's span"])

    def test_a_name_with_no_format_is_badfile_when_present(self):
        inventory = INVENTORY + "| room-palettes | g | h | d | f |\n"
        self.assertEqual(self.check(inventory)[0], ["MISSING room-palettes"])
        write_tree(self.root, {"room-palettes": "=1\n"})
        self.assertEqual(self.check(inventory)[0],
                         ["BADFILE room-palettes: no format for room-palettes"])

    def test_a_malformed_inventory_is_refused(self):
        for inventory in ("# nothing\n", INVENTORY.replace("| Format |", "| Form |"),
                          INVENTORY + "| room-objects | g | h | d | f |\n"):
            with self.subTest(inventory=inventory):
                with self.assertRaises(extract.ExtractError):
                    self.check(inventory)

    SELF_TEST = "extract: self-test, 9 planted trees as expected\n"

    def main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = extract.main(list(args) or ["check", "--root", self.root, "--inventory",
                                               os.path.join(self.dir, "inventory.md")])
        return code, out.getvalue(), err.getvalue()

    def test_main_on_the_real_layout_reports_each_planted_file_bad(self):
        code, out, _ = self.main()
        self.assertEqual(code, 1)
        self.assertTrue(out.startswith(self.SELF_TEST), out)
        self.assertEqual([line.split(":")[0] for line in out.splitlines()[1:]],
                         ["BADFILE " + name for name in extract.NAMES if name != "source-flags"])

    def test_main_on_a_missing_root_prints_missing_lines(self):
        shutil.rmtree(self.root)
        code, out, _ = self.main()
        self.assertEqual((code, out), (1, self.SELF_TEST + "".join("MISSING %s\n" % n
                                                                    for n in extract.NAMES)))

    def test_main_exits_2_on_a_malformed_inventory_or_usage(self):
        os.remove(os.path.join(self.dir, "inventory.md"))
        code, out, err = self.main()
        self.assertEqual((code, out), (2, self.SELF_TEST))
        self.assertIn("inventory.md", err)
        self.assertEqual(self.main("list")[0], 2)
        self.assertEqual(self.main("check", "--rom", "x")[0], 2)

    def test_the_self_test_passes(self):
        self.assertIsNone(extract.self_test())

    def test_the_self_test_fails_when_the_check_misses_a_kind(self):
        real = extract.check
        try:
            for kind in ("MISSING", "EXTRA", "BADFILE"):
                extract.check = lambda *a, kind=kind: (
                    [l for l in real(*a)[0] if not l.startswith(kind + " ")], real(*a)[1])
                self.assertIsNotNone(extract.self_test(), kind)
                code, out, _ = self.main()
                self.assertEqual(code, 1)
                self.assertTrue(out.startswith("extract: self-test failed: "), out)
        finally:
            extract.check = real


if __name__ == "__main__":
    unittest.main()
