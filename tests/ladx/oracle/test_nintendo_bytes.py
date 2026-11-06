"""Usage: python3 -I test_nintendo_bytes.py

Unit tests of nintendo_bytes.py on synthetic ROMs, with no real ROM.
"""

import contextlib
import hashlib
import importlib.util
import io
import os
import random
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("nintendo_bytes",
                                               os.path.join(HERE, "nintendo_bytes.py"))
nintendo_bytes = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(nintendo_bytes)

_RNG = random.Random(1234)
ROM = bytes(_RNG.getrandbits(8) for _ in range(4096))


def runs(data, min_len=4):
    return nintendo_bytes.literal_runs(data, nintendo_bytes.RomIndex(ROM, min_len))


class RawView(unittest.TestCase):
    def test_four_byte_slice_is_one_run_three_is_none(self):
        self.assertEqual(runs(b"hello " + ROM[100:104] + b" world"), [("raw", 6, 4)])
        self.assertEqual(runs(b"hello " + ROM[100:103] + b" world"), [])

    def test_a_longer_slice_is_one_merged_run(self):
        self.assertEqual(runs(b"text: " + ROM[200:209] + b"."), [("raw", 6, 9)])

    def test_a_longer_min_ignores_shorter_slices(self):
        self.assertEqual(runs(b"<" + ROM[300:307] + b">", 8), [])
        self.assertEqual(runs(b"<" + ROM[300:308] + b">", 8), [("raw", 1, 8)])


class HexView(unittest.TestCase):
    def test_upper_case_hex_at_an_odd_nibble_is_one_hex_run(self):
        text = b"zz7" + ROM[500:505].hex().upper().encode() + b" end"
        self.assertEqual(runs(text), [("hex", 3, 5)])

    def test_lower_case_hex_at_an_even_nibble_is_one_hex_run(self):
        text = b"db " + ROM[600:605].hex().encode() + b"\n"
        self.assertEqual(runs(text), [("hex", 3, 5)])

    def test_hex_shorter_than_twice_min_is_not_decoded(self):
        self.assertEqual(runs(b"x" + ROM[700:704].hex().encode()[:7] + b"y"), [])



class CommandLine(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = self.tmp.name
        self.rom = self.write("rom.bin", ROM)

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, rel, data):
        path = os.path.join(self.dir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
        return path

    def run_main(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = nintendo_bytes.main(list(args))
        return code, out.getvalue().splitlines()

    def test_a_planted_directory_reports_each_run_sorted_by_path_then_offset(self):
        tree = os.path.join(self.dir, "tree")
        self.write("tree/b.txt", b"ok " + ROM[10:15].hex().encode() + b" " + ROM[40:44])
        self.write("tree/a/x.bin", ROM[900:910])
        self.write("tree/clean.txt", b"nothing here\n")
        self.write("tree/.git/objects/leak", ROM[0:64])
        code, lines = self.run_main("--rom", self.rom, tree)
        a = os.path.join(tree, "a", "x.bin")
        b = os.path.join(tree, "b.txt")
        self.assertEqual(lines[-4:], [
            f"LITERAL {a} raw @0 10 bytes",
            f"LITERAL {b} hex @3 5 bytes",
            f"LITERAL {b} raw @14 4 bytes",
            f"nintendo-bytes: 3 files, rom {hashlib.sha1(ROM).hexdigest()}, min 4, "
            "3 literal runs",
        ])
        self.assertEqual(code, 1)

    def test_a_clean_file_passes_with_zero_literal_runs(self):
        clean = self.write("clean.txt", b"just text, 0000 and FF\n")
        code, lines = self.run_main("--rom", self.rom, "--min", "5", clean)
        self.assertEqual(lines[-1], f"nintendo-bytes: 1 files, rom {hashlib.sha1(ROM).hexdigest()}"
                                    ", min 5, 0 literal runs")
        self.assertEqual(code, 0)

    def test_no_line_holds_the_rom_bytes_or_their_hex(self):
        leak = self.write("leak.txt", ROM[1000:1032] + b" " + ROM[2000:2032].hex().encode())
        code, lines = self.run_main("--rom", self.rom, leak)
        self.assertEqual(code, 1)
        text = "\n".join(lines)
        for chunk in (ROM[1000:1032], ROM[2000:2032]):
            for i in range(0, 32, 4):
                self.assertNotIn(chunk[i:i + 4].hex(), text.lower())
                self.assertNotIn(chunk[i:i + 4].decode("latin-1"), text)

    def test_a_missing_rom_is_a_setup_line(self):
        code, lines = self.run_main("--rom", os.path.join(self.dir, "none.gbc"), self.rom)
        self.assertEqual(code, 2)
        self.assertTrue(lines[-1].startswith("setup: nintendo-bytes: no ROM at "), lines)

    def test_a_missing_input_is_a_setup_line(self):
        code, lines = self.run_main("--rom", self.rom, os.path.join(self.dir, "none"))
        self.assertEqual(code, 2)
        self.assertTrue(lines[-1].startswith("setup: nintendo-bytes: no such file "), lines)

    def test_the_self_proof_passes_before_the_scan(self):
        clean = self.write("clean.txt", b"text\n")
        code, lines = self.run_main("--rom", self.rom, clean)
        self.assertEqual(lines[0], "nintendo-bytes: self-test, 6 planted files as expected")
        self.assertEqual(code, 0)

    def test_a_check_that_finds_nothing_fails_its_self_proof(self):
        clean = self.write("clean.txt", b"text\n")
        real = nintendo_bytes.literal_runs
        nintendo_bytes.literal_runs = lambda data, index: []
        try:
            code, lines = self.run_main("--rom", self.rom, clean)
        finally:
            nintendo_bytes.literal_runs = real
        self.assertEqual(code, 1)
        self.assertEqual(lines, ["nintendo-bytes: self-test failed: four.bin"])

    def test_a_check_that_misses_hex_fails_its_self_proof(self):
        clean = self.write("clean.txt", b"text\n")
        real = nintendo_bytes.literal_runs
        nintendo_bytes.literal_runs = lambda data, index: [
            run for run in real(data, index) if run[0] == "raw"]
        try:
            code, lines = self.run_main("--rom", self.rom, clean)
        finally:
            nintendo_bytes.literal_runs = real
        self.assertEqual(code, 1)
        self.assertEqual(lines, ["nintendo-bytes: self-test failed: lower.txt"])

    def test_the_script_exits_1_on_a_literal_run(self):
        leak = self.write("leak.bin", ROM[50:60])
        result = subprocess.run([sys.executable, "-I", "-B", os.path.join(HERE, "nintendo_bytes.py"),
                                 "--rom", self.rom, leak], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertTrue(result.stdout.endswith(" 1 literal runs\n"), result.stdout)


if __name__ == "__main__":
    unittest.main()
