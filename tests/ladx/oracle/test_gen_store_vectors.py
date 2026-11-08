import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gen_store_vectors as g  # noqa: E402

HAND = ["merge-one-byte-gap", "sort-stable-banks", "contained-span", "claim-gaps", "expand",
        "alt-first", "suffix-newest", "sprite-whole-only", "integer-pointer", "empty-entry",
        "no-room-index", "no-room-alt", "readback", "misaligned", "byte-range",
        "byte-range-negative", "add-adjust"]

PLANTED = g.VERSION_LINE + """
V a
info 1 1 100 2 - 0 0 0
S 2 100 102
R 2 100 102
E 2 01ff
W 2 100 01ff
W 1 100 00
W 1 101 41
L 2 102 102
end
V b
info 1 1 100 2 - 0 0 0
E int 5
W 1 100 05
raise AssertionError
end
"""


class StoreVectorsTest(unittest.TestCase):
    def test_parse_planted(self):
        vectors = g.parse("# header\n" + PLANTED)
        self.assertEqual([name for name, _ in vectors], ["a", "b"])
        self.assertEqual(vectors[0][1][-1], "L 2 102 102")
        self.assertEqual(vectors[1][1][-1], "raise AssertionError")

    def test_truncated_refused(self):
        with self.assertRaises(g.FixtureError):
            g.parse(PLANTED.rsplit("end", 1)[0])
        with self.assertRaises(g.FixtureError):
            g.parse("V a\nend\n")
        with self.assertRaises(g.FixtureError):
            g.parse(g.VERSION_LINE + "\nE int 5\n")

    def test_compare(self):
        self.assertEqual(g.compare(PLANTED, PLANTED), [])
        changed = PLANTED.replace("W 1 100 05", "W 1 100 06")
        self.assertEqual(g.compare(PLANTED, changed), ["DIFFERENT line 15"])
        self.assertEqual(len(g.compare(PLANTED, PLANTED + "x\n")), 2)

    def test_info_and_entry_lines(self):
        self.assertEqual(g.info_line({"count": 3, "pointers_bank": 0x20, "pointers_addr": 0x30D3,
                                      "data_bank": 0x20, "data_addr": 0x33F3, "data_size": 4,
                                      "claim_storage_gaps": True}),
                         "info 3 20 30d3 20 33f3 4 1 0")
        self.assertEqual(g.info_line({"count": 2, "pointers_bank": 1, "pointers_addr": 0x100,
                                      "banks_bank": 1, "banks_addr": 0x200}),
                         "info 2 1 100 -1 - 0 0 0")
        self.assertEqual(g.entry_line(2, b"\x01\xff"), "E 2 01ff")
        self.assertEqual(g.entry_line(2, 0x8000), "E int 8000")

    def test_committed_fixture(self):
        with open(g.FIXTURE, encoding="ascii") as f:
            text = f.read()
        self.assertLess(len(text), 300 * 1024)
        self.assertTrue(text.startswith(g.HEADER))
        vectors = g.parse(text)
        names = [name for name, _ in vectors]
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(names[:len(HAND)], HAND)
        self.assertEqual(names[len(HAND):], ["random-%02d" % n for n in range(g.RANDOM_VECTORS)])
        last = {name: lines[-1] for name, lines in vectors}
        self.assertTrue(last["no-room-index"].startswith(
            "raise AssertionError Not enough room in storage... 1/2 [{'bank': 2"))
        self.assertTrue(last["no-room-alt"].startswith(
            "raise AssertionError Not enough room in storage... [{'bank': 2"))
        self.assertEqual(last["readback"], "raise AssertionError")
        self.assertEqual(last["misaligned"], "raise AssertionError")
        self.assertEqual(last["byte-range"], "raise ValueError byte must be in range(0, 256)")
        self.assertEqual(last["byte-range-negative"], "raise ValueError byte must be in range(0, 256)")
        ok = [name for name, lines in vectors if lines[-1].startswith("L ")]
        self.assertGreater(len([n for n in ok if n.startswith("random-")]), 40)
        lines = dict(vectors)
        self.assertIn("R 2 100 108", lines["merge-one-byte-gap"])
        self.assertTrue(any(line.startswith("add ") for line in lines["add-adjust"]))
        self.assertTrue(any(line.startswith("adjust ") for line in lines["add-adjust"]))
        self.assertIn("E int 8000", lines["integer-pointer"])


if __name__ == "__main__":
    unittest.main()
