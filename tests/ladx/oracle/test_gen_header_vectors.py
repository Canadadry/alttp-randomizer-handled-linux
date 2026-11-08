import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gen_header_vectors as g  # noqa: E402

HAND = ["zero", "no-name", "empty-name", "long-name", "utf8-cut", "all-ff", "old-checksums",
        "header-wraps", "last-byte", "sum-wraps"]

PLANTED = g.VERSION_LINE + """
V a
P 134 01
name 4c41
W 134 4c41000000000000000000000000
W 14d 00
end
V b
name -
W 14d 00
end
"""


class HeaderVectorsTest(unittest.TestCase):
    def test_parse_planted(self):
        vectors = g.parse("# header\n" + PLANTED)
        self.assertEqual([name for name, _ in vectors], ["a", "b"])
        self.assertEqual(vectors[0][1][1], "name 4c41")
        self.assertEqual(vectors[1][1][-1], "W 14d 00")

    def test_malformed_refused(self):
        with self.assertRaises(g.FixtureError):
            g.parse(PLANTED.rsplit("end", 1)[0])
        with self.assertRaises(g.FixtureError):
            g.parse("V a\nname -\nend\n")
        with self.assertRaises(g.FixtureError):
            g.parse(g.VERSION_LINE + "\nW 14d 00\n")
        with self.assertRaises(g.FixtureError):
            g.parse(PLANTED.replace("name -\n", ""))
        with self.assertRaises(g.FixtureError):
            g.parse(PLANTED.replace("name -\n", "name -\nname -\n"))

    def test_image(self):
        data = g.image(g.Vector("x", fills=[(0, 4, 0x11)], plants=[(2, b"\x22")]))
        self.assertEqual(len(data), g.SIZE)
        self.assertEqual(data[:5], b"\x11\x11\x22\x11\x00")
        with self.assertRaises(g.FixtureError):
            g.image(g.Vector("y", plants=[(g.SIZE - 1, b"\x01\x02")]))

    def test_committed_fixture(self):
        with open(g.FIXTURE, encoding="ascii") as f:
            text = f.read()
        self.assertLess(len(text), 64 * 1024)
        self.assertTrue(text.startswith(g.HEADER))
        vectors = g.parse(text)
        names = [name for name, _ in vectors]
        self.assertEqual(names[:len(HAND)], HAND)
        self.assertEqual(names[len(HAND):], ["random-%02d" % n for n in range(g.RANDOM_VECTORS)])
        lines = dict(vectors)
        for name, rows in vectors:
            writes = [row.split()[1] for row in rows if row.startswith("W ")]
            named = not any(row == "name -" for row in rows)
            self.assertEqual(writes, (["134"] if named else []) + ["14d", "14e", "14f", "14e", "14f"],
                             name)
        self.assertIn("W 134 4c4144585200000000000000000000", lines["zero"])
        # every byte 0xFF but the 15 of the name and the 3 checksum bytes, counted after they change.
        name = b"LADXR" + bytes(10)
        header = (-sum(c + 1 for c in name + b"\xff" * 10)) & 0xFF
        total = (0xFF * (g.SIZE - 18) + sum(name) + header) & 0xFFFF
        self.assertEqual(lines["all-ff"][-5:], ["W 14d %02x" % header, "W 14e 00", "W 14f 00",
                                                "W 14e %02x" % (total >> 8), "W 14f %02x" % (total & 0xFF)])
        self.assertIn("W 134 4142434445464748494a4b4c4d4ec3", lines["utf8-cut"])
        self.assertIn("W 134 000000000000000000000000000000", lines["empty-name"])


if __name__ == "__main__":
    unittest.main()
