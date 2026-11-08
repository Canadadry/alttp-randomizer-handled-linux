
import contextlib
import hashlib
import importlib.util
import io
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("table_replay",
                                               os.path.join(HERE, "table_replay.py"))
table_replay = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(table_replay)
tables = table_replay.tables
write_log = table_replay.write_log

ROM = bytes((i * 7 + 3) % 256 for i in range(0x100))
SECTION = "generator.py generateRom"
ORDER = ("ladx-tables-order 1\nsection %s\n"
         "table fixed fixed.1 a(rom)\nC items - b(rom)\ntable opts opts.1 c(rom)\n"
         "C items - d(rom)\n" % SECTION)
FIXED = "ladx-tables 1\ngroup fixed\ntables texts\nunit fixed.1\nE\nB 10 @20+4\n"
OPTS = ("ladx-tables 1\ngroup opts\ntables texts\nunit opts.1 race\nE 0\nB 30 01\nT texts 2 05\n"
        "E 1\nnothing\n")
GOLDEN = ("ladx-rom-writes 1\ntables texts\nG fixed\nB 10 @20+4\nG items\nB 11 aa\n"
          "G opts\nB 30 01\nT texts 2 05\nG items\nB 30 bb\n")


def event(kind, group, call, where, value):
    origins = "k%x" % len(value)
    site = "%s\t%s" % (SECTION, call)
    if kind == "B":
        return "B\t%s\t%x\t%s\t%s\t%s" % (group, where, value.hex(), origins, site)
    return "T\t%s\ttexts\t%x\t%s\t%s\t%s" % (group, where, value.hex(), origins, site)


def stream(opts=True):
    return write_log.parse_events("".join(line + "\n" for line in [
        "ladx-events 1", "source " + hashlib.sha1(ROM).hexdigest(), "tables texts",
        event("B", "fixed", "a(rom)", 0x10, ROM[0x20:0x24]),
        event("B", "items", "b(rom)", 0x11, b"\xaa")] + ([
        event("B", "opts", "c(rom)", 0x30, b"\x01"),
        event("T", "opts", "c(rom)", 2, b"\x05")] if opts else []) + [
        event("B", "items", "d(rom)", 0x30, b"\xbb")]))


def sha1_of(golden):
    return hashlib.sha1(write_log.decode(golden, ROM)).hexdigest()


def load(order=ORDER, fixed=FIXED, opts=OPTS):
    return tables.Tables(tables.parse(order), {"fixed": tables.parse(fixed),
                                               "opts": tables.parse(opts)})


class Replay(unittest.TestCase):
    def replay(self, tabs=None, values=None, golden=GOLDEN, sha1=None, events=None):
        return table_replay.replay(tabs or load(), values or {"race": "0"}, golden,
                                   events or stream(), ROM, sha1 or sha1_of(golden))

    def test_tables_and_c_segments_compose_into_the_golden(self):
        self.assertEqual(self.replay(), [])

    def test_a_flipped_table_byte_is_bad_sha1_naming_the_writers(self):
        got = self.replay(load(opts=OPTS.replace("B 30 01", "B 31 01")))
        self.assertEqual(got[0][0], "sha1")
        self.assertIn("first difference at 0x31", got[0][1])
        self.assertIn("replay opts.1, golden ROM", got[0][1])

    def test_a_token_swap_with_the_same_bytes_is_bad_projection(self):
        literal = ROM[0x20:0x24].hex()
        got = self.replay(load(fixed=FIXED.replace("@20+4", literal)))
        self.assertEqual(got, [("projection fixed", "line 4 differs from the golden's")])

    def test_a_key_without_entry_is_bad_key(self):
        got = self.replay(load(opts=OPTS.replace("E 0\n", "E 00\n")))
        self.assertEqual(got, [("key opts.1", "unit opts.1: no entry for 0")])

    def test_a_writes_nothing_entry_lets_one_c_segment_span_two_rows(self):
        golden = "ladx-rom-writes 1\ntables texts\nG fixed\nB 10 @20+4\nG items\nB 11 aa\nB 30 bb\n"
        self.assertEqual(self.replay(values={"race": "1"}, golden=golden,
                                     events=stream(opts=False)), [])

    def test_the_wrong_entry_for_a_case_is_bad_sha1(self):
        golden = "ladx-rom-writes 1\ntables texts\nG fixed\nB 10 @20+4\nG items\nB 11 aa\nB 30 bb\n"
        got = self.replay(values={"race": "0"}, golden=golden, events=stream(opts=False))
        self.assertEqual([k for k, _ in got], ["sha1", "projection opts"])

    def test_c_segments_out_of_step_with_the_stream_are_bad_align(self):
        golden = GOLDEN.replace("G items\nB 30 bb\n", "")
        got = self.replay(golden=golden)
        self.assertEqual([k for k, _ in got], ["align"])


class Fragments(unittest.TestCase):
    def setUp(self):
        self.known = table_replay.known_source.KnownSource(len(ROM))
        self.known.add(0x40, 0x48, "texts 2", ("texts", 2))
        self.known.add(0x10, 0x18, "pointers")

    def scan(self, fixed=FIXED, opts=OPTS):
        return table_replay.table_fragments({"fixed.txt": fixed, "opts.txt": opts}, ROM,
                                            self.known)

    def test_clean_tables_have_no_fragment(self):
        self.assertEqual(self.scan(), [])

    def test_a_piece_of_the_replaced_entry_is_reported_by_file_and_line(self):
        piece = ROM[0x41:0x43] + b"\x05" + ROM[0x46:0x47]
        self.assertEqual(self.scan(opts=OPTS.replace("T texts 2 05", "T texts 2 " + piece.hex())),
                         [("opts.txt", 7, 2, 3)])

    def test_a_bank_literal_inside_a_ks_span_is_a_fragment(self):
        self.assertEqual(self.scan(fixed=FIXED.replace("B 10 @20+4",
                                                       "B 10 @20+4 " + ROM[0x11:0x13].hex())),
                         [("fixed.txt", 6, 1, 2)])


class SelfTest(unittest.TestCase):
    def test_self_test_passes(self):
        self.assertIsNone(table_replay.self_test())


class Main(unittest.TestCase):
    def test_a_missing_rom_is_a_setup_line(self):
        root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, root)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = table_replay.main(["--rom", os.path.join(root, "none.gbc")])
        self.assertEqual(code, 2)
        lines = out.getvalue().splitlines()
        self.assertEqual(lines[0], "table-replay: self-test, 5 planted replays as expected")
        self.assertTrue(lines[-1].startswith("setup: table-replay: no ROM at "))


if __name__ == "__main__":
    unittest.main()
