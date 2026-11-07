import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gen_encoder_vectors as g  # noqa: E402
import write_log  # noqa: E402

KNOWN = write_log.KNOWN

# One record each hand-written vector must hold: the rule it is named after, at work.
HAND_RECORDS = {
    "carried-source-copy": "B 100 =1 @300+6",
    "carried-identity": "B 500 @100+1",
    "inherit-on-equal-bank": "B 100 =4 6a",
    "table-prefix-suffix-middle": "T entities 3 @400+3 e9 @406+5 b0 @40e+2",
    "table-unknowable-stop": "T entities 4 @500+2 f4972a2d @506+2",
    "ks-match-run-boundary": "B 700 @604+4 466982ee",
    "ks-match-lowest-offset": "B 800 @200+8",
    "ks-match-longest": "B 800 @e00+c",
    "fragment-table": "T entities 7 @905+2 00 @90a+1",
    "fragment-bank": "B a0c @a02+2 00 @a05+1 @a12+1 00 @a18+2",
    "rename-after-ks-match": "B 7a0 @b02+4",
    "rename-after-fragment": "B 7c0 @986+2",
    "keep-rewrite": "B 310 =1",
    "integer-pointer": "T entities a p5678",
    "net-effect": "B 100 010903",
    "table-passthrough": "B 140 @122+1 @343+3 02",
}


def _planted(rom):
    return [
        g.Vector("planted-a", {"ca": "C"}, None, [("entities", 1, rom[0x10:0x14], g.src(0x10, 4))],
                 [g.bank("ca", 0x20, b"\x01\x02", (KNOWN, KNOWN)),
                  g.entry("ca", "entities", 1, rom[0x10:0x12] + b"\x07")]),
        g.Vector("planted-b", {"tg": "table", "ca": "C"}, [g.span(0x30, 0x40, "texts", "Alt06", None),
                                                           g.span(0x50, 0x58)],
                 [("entities", 2, 0x4321, None)],
                 [g.bank("tg", 0x60, rom[0x30:0x38], g.ids(0, 8)),
                  g.entry("ca", "entities", 2, 0x99),
                  g.bank("ca", 0x70, rom[0x31:0x35], g.ids(1, 4))]),
    ]


class RenderParse(unittest.TestCase):
    def test_round_trip(self):
        rom = g.synthetic_rom()
        planted = _planted(rom)
        got_rom, parsed = g.parse(g.render(rom, planted))
        self.assertEqual(got_rom, rom)
        self.assertEqual([v.name for v, _ in parsed], ["planted-a", "planted-b"])
        for want, (got, log) in zip(planted, parsed):
            self.assertEqual(got.sides, want.sides)
            self.assertEqual(got.ks, want.ks)
            self.assertEqual(got.initial, want.initial)
            self.assertEqual(got.events, want.events)
            self.assertEqual(log, g.encode(rom, want))
        self.assertEqual(parsed[1][0].ks[0][3], ("texts", "Alt06"))
        self.assertIsNone(parsed[1][0].ks[1][3])

    def test_a_truncated_fixture_is_refused(self):
        rom = g.synthetic_rom()
        text = g.render(rom, _planted(rom))
        with self.assertRaises(g.FixtureError):
            g.parse(text[:text.index("\nlog ") + 1] + "log 9\n")


class Check(unittest.TestCase):
    def _check(self, path):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = g.main(["--check", "--out", path])
        return code, out.getvalue()

    def test_a_changed_line_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "encoder_vectors.txt")
            with open(g.FIXTURE, encoding="ascii") as f:
                lines = f.read().split("\n")
            n = next(i for i, line in enumerate(lines) if line.startswith("B 800 "))
            lines[n] = lines[n] + "00"
            with open(path, "w", encoding="ascii") as f:
                f.write("\n".join(lines))
            code, text = self._check(path)
            self.assertEqual(code, 1)
            self.assertIn("DIFFERENT line %d\n" % (n + 1), text)

    def test_a_missing_file_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, text = self._check(os.path.join(tmp, "none.txt"))
            self.assertEqual(code, 1)
            self.assertTrue(text.startswith("MISSING "))


class Committed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(g.FIXTURE, encoding="ascii") as f:
            cls.text = f.read()
        cls.rom, cls.parsed = g.parse(cls.text)

    def test_size_and_rom(self):
        self.assertLess(len(self.text), 300 * 1024)
        self.assertEqual(self.rom, g.synthetic_rom())

    def test_every_log_is_write_log_encode_of_its_stream(self):
        for v, log in self.parsed:
            stream = write_log.parse_events(g.stream_text(self.rom, g.TABLES, v.initial, v.events))
            self.assertEqual(log, write_log.encode(stream, self.rom, v.sides, g.known(self.rom, v.ks)), v.name)
            write_log.parse(log)

    def test_vectors(self):
        names = [v.name for v, _ in self.parsed]
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(names[:len(HAND_RECORDS)], list(HAND_RECORDS))
        self.assertEqual(sum(name.startswith("random-") for name in names), g.RANDOM_VECTORS)
        random_vectors = [v for v, _ in self.parsed if v.name.startswith("random-")]
        self.assertTrue(any(v.ks is None for v in random_vectors))
        self.assertTrue(any(v.ks and any(s[3] for s in v.ks) for v in random_vectors))

    def test_hand_vectors_hold_their_rule(self):
        logs = {v.name: log for v, log in self.parsed}
        for name, record in HAND_RECORDS.items():
            self.assertIn(record, logs[name].splitlines(), name)


if __name__ == "__main__":
    unittest.main()
