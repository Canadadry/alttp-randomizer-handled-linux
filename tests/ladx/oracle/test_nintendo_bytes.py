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
ZEROS, DOWN, THREE, TEXT, FOUR = 3000, 3100, 3200, 3300, 3400
TEXT_BYTES = b"Grandpa Ulrira's phone line is busy, so call again after the Wind Fish wakes up!"
ROM = bytearray(_RNG.getrandbits(8) for _ in range(4096))
ROM[ZEROS:ZEROS + 64] = bytes(64)
ROM[DOWN:DOWN + 64] = bytes(range(200, 136, -1))
ROM[THREE:THREE + 64] = bytes(_RNG.choice(b"\x11\x5a\xc3") for _ in range(64))
ROM[TEXT:TEXT + len(TEXT_BYTES)] = TEXT_BYTES
ROM[FOUR:FOUR + 64] = bytes(_RNG.choice(b"\x21\x6b\x90\xe7") for _ in range(64))
NAME, NAME_BYTES = 3600, b"Koholint_Island-Wind_Fish.Egg"
ROM[NAME:NAME + len(NAME_BYTES)] = NAME_BYTES
ROM = bytes(ROM)


INDEX = nintendo_bytes.RomIndex(ROM)
NO_CORPUS = nintendo_bytes.Corpus([])
STRICT = "tests/ladx/extract/x.bin"
GENERAL = "notes/x.md"


def check(path, data, corpus=NO_CORPUS, **kwargs):
    """(literal runs as (view, offset, length, scope), cleared runs as (view, length, category))."""
    literals, cleared = nintendo_bytes.check_blob(path, data, INDEX, corpus, **kwargs)
    return literals, [(view, length, category) for view, _, length, category in cleared]


class Thresholds(unittest.TestCase):
    def test_strict_raw_flags_4_bytes_not_3(self):
        self.assertEqual(check(STRICT, b"<" + ROM[100:103] + b">"), ([], []))
        self.assertEqual(check(STRICT, b"<" + ROM[100:104] + b">"), ([("raw", 1, 4, "strict")], []))

    def test_general_raw_flags_16_bytes_not_15(self):
        self.assertEqual(check(GENERAL, b"<" + ROM[100:115] + b">"), ([], []))
        self.assertEqual(check(GENERAL, b"<" + ROM[100:116] + b">"),
                         ([("raw", 1, 16, "general")], []))

    def test_strict_hex_flags_4_bytes_not_3(self):
        self.assertEqual(check(STRICT, b"x " + ROM[200:203].hex().encode() + b" y"), ([], []))
        self.assertEqual(check(STRICT, b"x " + ROM[200:204].hex().encode() + b" y")[0],
                         [("hex", 2, 4, "strict")])

    def test_general_hex_flags_8_bytes_not_7(self):
        self.assertEqual(check(GENERAL, b"x " + ROM[200:207].hex().encode() + b" y"), ([], []))
        self.assertEqual(check(GENERAL, b"x " + ROM[200:208].hex().upper().encode() + b" y")[0],
                         [("hex", 2, 8, "general")])

    def test_strict_bytelist_flags_4_bytes_not_3(self):
        three = ", ".join("$%02X" % b for b in ROM[300:303]).encode()
        four = ", ".join("$%02X" % b for b in ROM[300:304]).encode()
        self.assertEqual(check(STRICT, b"db " + three + b"\n"), ([], []))
        self.assertEqual(check(STRICT, b"db " + four + b"\n")[0], [("bytelist", 3, 4, "strict")])

    def test_general_bytelist_flags_8_bytes_not_7(self):
        seven = ", ".join("0x%02x" % b for b in ROM[300:307]).encode()
        eight = ",0x".join("%02X" % b for b in ROM[300:308]).encode()
        self.assertEqual(check(GENERAL, b"{" + seven + b"}"), ([], []))
        self.assertEqual(check(GENERAL, b"u8 a[] = {0x" + eight + b"};")[0],
                         [("bytelist", 10, 8, "general")])

    def test_every_strict_prefix_is_strict_and_other_paths_general(self):
        for path in ("tests/ladx/cases/c/g.txt", "tests/ladx/combos/k.txt",
                     "tests/ladx/extract/rooms.txt"):
            self.assertEqual(nintendo_bytes.scope_of(path), "strict", path)
        for path in ("data/base.bps", "tests/ladx/oracle/x.txt", "data/ladxr.txt", "README.md"):
            self.assertEqual(nintendo_bytes.scope_of(path), "general", path)

    def test_an_extraction_fixture_is_held_to_the_strict_4_bytes(self):
        path = "tests/ladx/extract/rooms.txt"
        self.assertEqual(check(path, b"<" + ROM[100:103] + b">"), ([], []))
        self.assertEqual(check(path, b"<" + ROM[100:104] + b">"), ([("raw", 1, 4, "strict")], []))

    def test_an_explicit_scope_overrides_the_path(self):
        self.assertEqual(check(GENERAL, ROM[100:104], scope="strict")[0],
                         [("raw", 0, 4, "strict")])
        self.assertEqual(check(STRICT, ROM[100:110], scope="general"), ([], []))

    def test_a_min_override_replaces_every_threshold(self):
        self.assertEqual(check(GENERAL, b"<" + ROM[100:106] + b">", min_len=6)[0],
                         [("raw", 1, 6, "general")])
        self.assertEqual(check(STRICT, b"<" + ROM[100:105] + b">", min_len=6), ([], []))


class Clearances(unittest.TestCase):
    def test_a_constant_run_is_trivial_in_both_scopes(self):
        for path in (STRICT, GENERAL):
            self.assertEqual(check(path, b"<" + ROM[ZEROS:ZEROS + 40] + b">"),
                             ([], [("raw", 40, "trivial")]), path)

    def test_a_counting_run_is_trivial_in_both_scopes_and_in_every_view(self):
        down = ROM[DOWN:DOWN + 20]
        self.assertEqual(check(STRICT, down), ([], [("raw", 20, "trivial")]))
        self.assertEqual(check(GENERAL, b"x=" + down.hex().encode()),
                         ([], [("hex", 20, "trivial")]))
        self.assertEqual(check(GENERAL, ", ".join("0x%02x" % b for b in down).encode()),
                         ([], [("bytelist", 20, "trivial")]))

    def test_a_three_value_run_is_low_info_in_general_only(self):
        three = ROM[THREE:THREE + 24]
        self.assertEqual(check(GENERAL, three), ([], [("raw", 24, "low-info")]))
        self.assertEqual(check(STRICT, three), ([("raw", 0, 24, "strict")], []))

    def test_a_four_value_run_is_flagged(self):
        four = ROM[FOUR:FOUR + 24]
        self.assertEqual(len(set(four)), 4)
        self.assertEqual(check(GENERAL, four), ([("raw", 0, 24, "general")], []))


class LadxrPublic(unittest.TestCase):
    def corpus(self, *files):
        return nintendo_bytes.Corpus([(f"f{i}", data) for i, data in enumerate(files)])

    def test_a_run_in_a_text_file_of_the_corpus_is_cleared_in_both_scopes(self):
        corpus = self.corpus(b"NAME = '" + TEXT_BYTES[10:60] + b"'\n")
        for path in (STRICT, GENERAL):
            self.assertEqual(check(path, b"<" + TEXT_BYTES[10:60] + b">", corpus),
                             ([], [("raw", 50, "ladxr-public")]), path)

    def test_a_run_only_partly_in_the_corpus_is_flagged(self):
        corpus = self.corpus(b"NAME = '" + TEXT_BYTES[10:60] + b"'\n")
        self.assertEqual(check(GENERAL, TEXT_BYTES[0:60], corpus),
                         ([("raw", 0, 60, "general")], []))

    def test_a_run_across_two_corpus_files_is_flagged(self):
        corpus = self.corpus(TEXT_BYTES[10:30], TEXT_BYTES[30:50])
        self.assertEqual(check(GENERAL, TEXT_BYTES[10:50], corpus)[0],
                         [("raw", 0, 40, "general")])

    def test_hex_digit_runs_of_the_corpus_are_decoded_at_both_alignments(self):
        even = self.corpus(b"rom.patch(0x01, 0x2345, '" + ROM[500:520].hex().encode() + b"')\n")
        odd = self.corpus(b"x = 7" + ROM[500:520].hex().upper().encode() + b"\n")
        for corpus in (even, odd):
            self.assertEqual(check(GENERAL, ROM[500:520], corpus),
                             ([], [("raw", 20, "ladxr-public")]))

    def test_binary_files_of_the_corpus_are_excluded(self):
        nul = b"\0" + TEXT_BYTES[10:60]
        start = next(i for i in range(600, 1000) if b"\0" not in ROM[i:i + 40])
        invalid = ROM[start:start + 40]
        self.assertRaises(UnicodeDecodeError, invalid.decode, "utf-8")
        corpus = self.corpus(nul, invalid)
        self.assertEqual(check(GENERAL, TEXT_BYTES[10:60], corpus)[0], [("raw", 0, 50, "general")])
        self.assertEqual(check(GENERAL, invalid, corpus)[0], [("raw", 0, 40, "general")])


def planted_ladxr(directory, files):
    """A git checkout in directory holding files [(path, bytes)] in one commit; its commit."""
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
    git = lambda *args: subprocess.run(["git", *args], cwd=directory, env=env, check=True,
                                       capture_output=True).stdout.decode().strip()
    os.makedirs(directory)
    git("init", "-q")
    for rel, data in files:
        path = os.path.join(directory, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
    git("add", "-A")
    git("-c", "user.name=p", "-c", "user.email=p@example.invalid", "commit", "-q", "-m", "pin")
    return git("rev-parse", "HEAD")


@contextlib.contextmanager
def pinned(commit):
    real = nintendo_bytes.LADXR_PIN
    nintendo_bytes.LADXR_PIN = commit
    try:
        yield
    finally:
        nintendo_bytes.LADXR_PIN = real


class LadxrCheckout(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = os.path.join(self.tmp.name, "ladxr")
        self.head = planted_ladxr(self.dir, [
            ("names.py", b"NAME = '" + TEXT_BYTES[10:60] + b"'\n"),
            ("gfx/tiles.bin", b"\0" + ROM[600:640]),
        ])

    def tearDown(self):
        self.tmp.cleanup()

    def test_the_tracked_files_at_the_pin_are_the_corpus_working_tree_edits_ignored(self):
        with open(os.path.join(self.dir, "names.py"), "ab") as f:
            f.write(TEXT_BYTES[60:])
        with open(os.path.join(self.dir, "untracked.py"), "wb") as f:
            f.write(TEXT_BYTES[0:10])
        with pinned(self.head):
            corpus = nintendo_bytes.load_ladxr(self.dir)
        self.assertEqual(corpus.commit, self.head)
        self.assertIn(TEXT_BYTES[10:60], corpus)
        self.assertNotIn(TEXT_BYTES[10:61], corpus)
        self.assertNotIn(TEXT_BYTES[0:10], corpus)
        self.assertNotIn(ROM[600:640], corpus)

    def test_a_checkout_at_another_commit_is_a_setup_error(self):
        with pinned("0" * 40):
            with self.assertRaisesRegex(nintendo_bytes.SetupError,
                                        r"LADXR checkout in .* is at [0-9a-f]{12}, not at the "
                                        r"pin 00000000"):
                nintendo_bytes.load_ladxr(self.dir)

    def test_a_missing_checkout_is_a_setup_error(self):
        for missing in (os.path.join(self.tmp.name, "none"), self.tmp.name):
            with pinned(self.head):
                with self.assertRaisesRegex(nintendo_bytes.SetupError,
                                            r"no LADXR checkout in .*tests/ladx/oracle/setup.sh"):
                    nintendo_bytes.load_ladxr(missing)


class BytelistView(unittest.TestCase):
    def test_commas_whitespace_and_newlines_separate_tokens_of_both_forms(self):
        tokens = ["0x%02X" % b if i % 2 else "$%02x" % b for i, b in enumerate(ROM[400:410])]
        text = ("  " + ",\n    ".join(tokens[:5]) + " " + "\t".join(tokens[5:]) + "\n").encode()
        self.assertEqual(check(GENERAL, text)[0], [("bytelist", 2, 10, "general")])

    def test_a_token_of_more_than_two_digits_ends_the_list(self):
        words = ", ".join("0x%02X" % b for b in ROM[400:408]).encode()
        self.assertEqual(check(GENERAL, words[:-4] + b"0x123")[0], [])
        self.assertEqual(check(GENERAL, b"0x1234, " + words)[0], [("bytelist", 8, 8, "general")])

    def test_other_separators_end_the_list(self):
        half = lambda part: " ".join("$%02X" % b for b in part).encode()
        self.assertEqual(check(GENERAL, half(ROM[400:404]) + b"; " + half(ROM[404:408]))[0], [])


class MergedRuns(unittest.TestCase):
    def test_a_longer_slice_is_one_merged_run(self):
        self.assertEqual(check(STRICT, b"text: " + ROM[200:209] + b".")[0],
                         [("raw", 6, 9, "strict")])

    def test_upper_case_hex_at_an_odd_nibble_is_one_hex_run(self):
        text = b"zz7" + ROM[500:505].hex().upper().encode() + b" end"
        self.assertEqual(check(STRICT, text)[0], [("hex", 3, 5, "strict")])

    def test_lower_case_hex_at_an_even_nibble_is_one_hex_run(self):
        text = b"db " + ROM[600:605].hex().encode() + b"\n"
        self.assertEqual(check(STRICT, text)[0], [("hex", 3, 5, "strict")])

    def test_hex_shorter_than_twice_the_threshold_is_not_decoded(self):
        self.assertEqual(check(STRICT, b"x" + ROM[700:704].hex().encode()[:7] + b"y"), ([], []))


CASE = "tests/ladx/cases/c/"


def log(*lines):
    """A write log of lines after the version and tables lines."""
    return ("ladx-rom-writes 1\ntables texts\n" + "".join(l + "\n" for l in lines)).encode()


def reads(rows, coverage=(), tables=None, known=()):
    return nintendo_bytes.ReadSets(nintendo_bytes.planted_inventory(rows, coverage), ROM,
                                   tables or (lambda rom: {}), lambda rom: set(known))


class CaseFolders(unittest.TestCase):
    def test_a_case_folders_golden_files_have_their_own_scopes(self):
        for folder in ("tests/ladx/cases/c/", "tests/ladx/combos/a+b/"):
            self.assertEqual(nintendo_bytes.scope_of(folder + "run.txt"), "general")
            self.assertEqual(nintendo_bytes.scope_of(folder + "spoiler.json"), "general")
            self.assertEqual(nintendo_bytes.scope_of(folder + "rom_writes.txt"), "writes")
            for other in ("seed", "config.txt", "meta.txt", "out.gbc"):
                self.assertEqual(nintendo_bytes.scope_of(folder + other), "strict", other)
        for path in ("tests/ladx/cases/run.txt", "tests/ladx/cases/c/d/run.txt",
                     "tests/ladx/cases/c/d/rom_writes.txt", "tests/ladx/extract/run.txt"):
            self.assertEqual(nintendo_bytes.scope_of(path), "strict", path)

    def test_run_txt_and_spoiler_json_are_held_to_the_general_thresholds(self):
        for name in ("run.txt", "spoiler.json"):
            self.assertEqual(check(CASE + name, b"<" + ROM[100:115] + b">"), ([], []))
            self.assertEqual(check(CASE + name, b"<" + ROM[100:116] + b">")[0],
                             [("raw", 1, 16, "general")])
            self.assertEqual(check(CASE + name, b"x " + ROM[200:207].hex().encode()), ([], []))

    def test_the_seed_of_a_case_stays_strict(self):
        self.assertEqual(check(CASE + "seed", ROM[200:204].hex().encode() + b"\n")[0],
                         [("hex", 0, 4, "strict")])



def table_file(*records):
    """A patch-table group file of one unit with one entry holding records."""
    return ("ladx-tables 1\ngroup g\ntables texts\nunit g.1 race\nE 0\n"
            + "".join(r + "\n" for r in records)).encode()


class TablesView(unittest.TestCase):
    PATH = "data/ladx/g.txt"

    def test_every_file_under_data_ladx_has_the_tables_scope(self):
        for path in ("data/ladx/g.txt", "data/ladx/order.txt", "data/ladx/d/rom_writes.txt",
                     "data/ladx/x.bin"):
            self.assertEqual(nintendo_bytes.scope_of(path), "tables", path)
        for path in ("data/ladxr/g.txt", "data/base.bps"):
            self.assertEqual(nintendo_bytes.scope_of(path), "general", path)

    def test_a_literal_token_is_checked_at_4_bytes_at_its_own_offset(self):
        self.assertEqual(check(self.PATH, table_file("B 10 " + ROM[100:103].hex())), ([], []))
        for record in ("B 10 =2 %s", "T texts 2 @200+3 %s"):
            data = table_file(record % ROM[100:104].hex())
            at = data.index(ROM[100:104].hex().encode())
            self.assertEqual(check(self.PATH, data), ([("tables", at, 4, "strict")], []))

    def test_a_misaligned_nibble_is_not_flagged(self):
        token = "0" + ROM[400:420].hex() + "0"
        self.assertEqual(check(self.PATH, table_file("B 10 " + token)), ([], []))

    def test_the_rest_of_the_text_is_held_to_the_general_thresholds(self):
        name = NAME_BYTES[:15].decode()
        self.assertEqual(check(self.PATH, table_file("B 10 00").replace(b"race\nE 0",
                         b"race\nE " + name.encode())), ([], []))
        data = table_file("B 10 00").replace(b"race\nE 0", b"race\nE " + NAME_BYTES[:16])
        self.assertEqual(check(self.PATH, data)[0], [("raw", data.index(b"Koholint"), 16,
                                                       "general")])
        order = ("ladx-tables-order 1\nsection a.py f\nC g - say(%r)\n"
                 % TEXT_BYTES[:15].decode()).encode()
        self.assertEqual(check("data/ladx/order.txt", order), ([], []))
        copy = table_file("B 10 @%s+4" % ROM[200:208].hex())
        self.assertEqual(check(self.PATH, copy)[0], [("hex", copy.index(b"@") + 1, 8,
                                                       "general")])

    def test_no_computed_or_invariant_clearance(self):
        data = table_file("B 10 " + ROM[100:108].hex())
        sets = NeverRead(nintendo_bytes.planted_inventory([("g(rom)", "-", "g")]), ROM,
                         lambda rom: {}, lambda rom: set())
        self.assertEqual(check(self.PATH, data, reads=sets)[0],
                         [("tables", data.index(ROM[100:108].hex().encode()), 8, "strict")])

    def test_a_file_the_tables_parser_refuses_is_one_literal(self):
        for data in (b"", table_file("B 10 0"), table_file(), b"\xff\n",
                     log("G g", "B 10 00"), b"ladx-tables-order 1\nC g - f()\n"):
            self.assertEqual(check(self.PATH, data), ([("tables", 0, len(data), "unparsed")],
                                                      []), data)

    def test_trivial_and_public_runs_of_a_token_are_cleared(self):
        corpus = nintendo_bytes.Corpus([("names.py", b"'" + TEXT_BYTES[10:40] + b"'")])
        data = table_file("B 10 " + ROM[ZEROS:ZEROS + 8].hex(), "B 20 " + TEXT_BYTES[10:40].hex())
        self.assertEqual(check(self.PATH, data, corpus),
                         ([], [("tables", 8, "trivial"), ("tables", 30, "ladxr-public")]))

    def test_an_explicit_scope_selects_the_view(self):
        data = table_file("B 10 " + ROM[100:104].hex())
        self.assertEqual(check(GENERAL, data, scope="tables")[0][0][0], "tables")
        self.assertEqual(check(self.PATH, data, scope="strict")[0][0][0], "hex")

class WritesView(unittest.TestCase):
    PATH = CASE + "rom_writes.txt"

    def test_a_literal_token_is_checked_at_4_bytes_at_its_own_offset(self):
        self.assertEqual(check(self.PATH, log("G g", "B 10 " + ROM[100:103].hex())), ([], []))
        data = log("G g", "B 10 =2 " + ROM[100:104].hex())
        at = data.index(ROM[100:104].hex().encode())
        self.assertEqual(check(self.PATH, data), ([("writes", at, 4, "strict")], []))

    def test_a_run_inside_a_longer_token_is_reported_at_its_byte(self):
        data = log("G g", "T texts 2 " + "ab" * 3 + ROM[300:306].hex())
        at = data.index(ROM[300:306].hex().encode())
        self.assertEqual(check(self.PATH, data)[0], [("writes", at, 6, "strict")])

    def test_a_misaligned_nibble_is_not_flagged(self):
        token = "0" + ROM[400:420].hex() + "0"
        decoded = bytes.fromhex(token)
        windows = nintendo_bytes.RomIndex(ROM).windows(4)
        self.assertFalse(any(decoded[i:i + 4] in windows for i in range(len(decoded) - 3)))
        self.assertEqual(check(self.PATH, log("G g", "B 10 " + token)), ([], []))

    def test_the_rest_of_the_text_is_held_to_the_general_thresholds(self):
        self.assertEqual(check(self.PATH, log("G " + NAME_BYTES[:15].decode(), "B 10 00")),
                         ([], []))
        data = log("G " + NAME_BYTES[:16].decode(), "B 10 00")
        self.assertEqual(check(self.PATH, data)[0], [("raw", data.index(b"Koholint"), 16,
                                                       "general")])
        copy = log("G g", "B 10 @%s+4" % ROM[200:207].hex())
        self.assertEqual(check(self.PATH, copy), ([], []))
        copy = log("G g", "B 10 @%s+4" % ROM[200:208].hex())
        self.assertEqual(check(self.PATH, copy)[0], [("hex", copy.index(b"@") + 1, 8,
                                                       "general")])

    def test_literal_tokens_are_left_out_of_the_general_views(self):
        data = log("G g", "B 10 =1 " + ROM[NAME:NAME + 20].hex() + " =1")
        literals, _ = check(self.PATH, data)
        self.assertEqual([view for view, _, _, _ in literals], ["writes"])

    def test_an_unparseable_log_is_one_literal(self):
        for data in (b"", log("G g", "B 10 0"), log("G g", "B 10 0A"), b"\xff\n",
                     log("B 10 00")):
            self.assertEqual(check(self.PATH, data), ([("writes", 0, len(data), "unparsed")],
                                                      []), data)

    def test_trivial_and_public_runs_of_a_token_are_cleared(self):
        corpus = nintendo_bytes.Corpus([("names.py", b"'" + TEXT_BYTES[10:40] + b"'")])
        data = log("G g", "B 10 " + ROM[ZEROS:ZEROS + 8].hex(), "B 20 " + TEXT_BYTES[10:40].hex())
        self.assertEqual(check(self.PATH, data, corpus),
                         ([], [("writes", 8, "trivial"), ("writes", 30, "ladxr-public")]))

    def test_an_explicit_scope_selects_the_view(self):
        data = log("G g", "B 10 " + ROM[100:104].hex())
        self.assertEqual(check(GENERAL, data, scope="writes")[0][0][0], "writes")
        self.assertEqual(check(self.PATH, data, scope="strict")[0][0][0], "hex")


class Computed(unittest.TestCase):
    PATH = CASE + "rom_writes.txt"
    RUN = ROM[0x480:0x488]

    def verdict(self, group, read_sets, run=RUN):
        literals, cleared = check(self.PATH, log("G " + group, "B 10 " + run.hex()),
                                  reads=read_sets)
        return [view for view, _, _, _ in literals], cleared

    def test_without_read_sets_nothing_is_cleared_computed(self):
        self.assertEqual(self.verdict("g", None), (["writes"], []))

    def test_a_run_its_group_never_read_is_cleared(self):
        sets = reads([("g(rom)", "-", "g"), ("h(rom)", "bank 0x00 source 0x0400..0x04FF", "h")])
        self.assertEqual(self.verdict("g", sets), ([], [("writes", 8, "computed")]))

    def test_a_run_in_a_bank_range_its_group_read_is_not_cleared(self):
        sets = reads([("g(rom)", "bank 0x00 source 0x0480..0x0487", "g")])
        self.assertEqual(self.verdict("g", sets), (["writes"], []))
        sets = reads([("g(rom)", "bank 0x00 source 0x0480..0x0486", "g")])
        self.assertEqual(self.verdict("g", sets), ([], [("writes", 8, "computed")]))

    def test_whole_banks_and_bank_ranges_are_read(self):
        for cell in ("bank 0x00 source all", "bank 0x00..0x01 source 0x0400..0x0500"):
            self.assertEqual(self.verdict("g", reads([("g(rom)", cell, "g")])),
                             (["writes"], []), cell)

    def test_two_rows_of_one_group_are_one_read_set(self):
        sets = reads([("g(rom)", "bank 0x00 source 0x0400..0x0483", "g"),
                      ("g2(rom)", "bank 0x00 source 0x0484..0x0490", "g")])
        self.assertEqual(self.verdict("g", sets), (["writes"], []))

    def test_a_table_range_reads_its_entries_source_spans(self):
        tables = lambda rom: {"texts": {0: (0x100, 0x110), 1: (0x47C, 0x490), 2: 0x4123,
                                        "Alt1": (0x900, 0x910)}}
        for cell, cleared in (("table texts source 0x0001..0x0002", False),
                              ("table texts source 0x0000..0x0000", True),
                              ("table texts source all", False)):
            got = self.verdict("g", reads([("g(rom)", cell, "g")], tables=tables))
            self.assertEqual(got, ([], [("writes", 8, "computed")]) if cleared else
                             (["writes"], []), cell)
        alt = self.verdict("g", reads([("g(rom)", "table texts source all", "g")],
                                      tables=tables), ROM[0x900:0x908])
        self.assertEqual(alt, (["writes"], []))

    def test_written_reads_follow_the_writers_read_sets(self):
        sets = reads([("g(rom)", "bank 0x00 written:h 0x0010..0x0011", "g"),
                      ("h(rom)", "bank 0x00 written:k 0x0020..0x0021", "h"),
                      ("k(rom)", "bank 0x00 source 0x0480..0x0487; bank 0x00 written:g "
                       "0x0030..0x0031", "k")])
        self.assertEqual(self.verdict("g", sets), (["writes"], []))
        sets = reads([("g(rom)", "bank 0x00 written:h 0x0480..0x0487", "g"), ("h(rom)", "-", "h")])
        self.assertEqual(self.verdict("g", sets), ([], [("writes", 8, "computed")]))

    def test_mixed_reads_are_source_and_followed(self):
        sets = reads([("g(rom)", "bank 0x00 mixed:h 0x0480..0x0487", "g"), ("h(rom)", "-", "h")])
        self.assertEqual(self.verdict("g", sets), (["writes"], []))
        sets = reads([("g(rom)", "bank 0x00 mixed:h 0x0010..0x0011", "g"),
                      ("h(rom)", "bank 0x00 source 0x0480..0x0487", "h")])
        self.assertEqual(self.verdict("g", sets), (["writes"], []))

    def test_a_computed_coverage_rows_reads_are_left_out(self):
        rows = [("checksum(rom)", "bank 0x00 mixed:h all", "g"), ("h(rom)", "-", "h")]
        sets = reads(rows, [("g", "`checksum(rom)`", "*", "computed")])
        self.assertEqual(self.verdict("g", sets), ([], [("writes", 8, "computed")]))
        sets = reads(rows, [("g", "*", "bank 0x00", "computed")])
        self.assertEqual(self.verdict("g", sets), ([], [("writes", 8, "computed")]))
        sets = reads(rows, [("g", "`checksum(rom)`", "*", "copy")])
        self.assertEqual(self.verdict("g", sets), (["writes"], []))

    def test_a_run_only_partly_in_the_read_set_is_cleared(self):
        sets = reads([("g(rom)", "bank 0x00 source 0x0480..0x0485", "g")])
        run = self.RUN[:4] + ROM[0x700:0x704]
        self.assertEqual(self.verdict("g", sets, run), ([], [("writes", 8, "computed")]))
        self.assertEqual(self.verdict("g", sets, run[:4]), (["writes"], []))

    def test_each_token_is_judged_by_its_own_group(self):
        sets = reads([("g(rom)", "bank 0x00 source 0x0480..0x0487", "g"), ("h(rom)", "-", "h")])
        data = log("G h", "B 10 " + self.RUN.hex(), "G g", "B 20 " + self.RUN.hex())
        literals, cleared = check(self.PATH, data, reads=sets)
        self.assertEqual([(v, o) for v, o, _, _ in literals],
                         [("writes", data.rindex(self.RUN.hex().encode()))])
        self.assertEqual(cleared, [("writes", 8, "computed")])

    def test_a_malformed_inventory_is_a_setup_error(self):
        broken = nintendo_bytes.ReadSets("no tables here", ROM)
        with self.assertRaises(nintendo_bytes.SetupError):
            self.verdict("g", broken)
        bad_cell = reads([("g(rom)", "bank 0x00 sourced 0x0480..0x0487", "g")])
        with self.assertRaisesRegex(nintendo_bytes.SetupError, "Reads \\(trace\\)"):
            self.verdict("g", bad_cell)

    def test_the_inventory_is_read_only_when_a_run_needs_it(self):
        def missing():
            raise nintendo_bytes.SetupError("no inventory")
        sets = nintendo_bytes.ReadSets(missing, ROM)
        self.assertEqual(check(self.PATH, log("G g", "B 10 0001"), reads=sets), ([], []))
        with self.assertRaises(nintendo_bytes.SetupError):
            self.verdict("g", sets)


class Invariant(unittest.TestCase):
    """A literal run whose bytes the oracle's re-run on a scrambled copy gave unchanged (the
    log's `I` lines) is cleared `invariant`, unless it is source bytes C knows."""
    PATH = CASE + "rom_writes.txt"
    RUN = ROM[0x480:0x488]
    READER = [("g(rom)", "bank 0x00 source 0x0400..0x04FF", "g")]

    def verdict(self, marks, run=RUN, known=()):
        data = log("G g", "B 10 00" + run.hex(), *marks)
        literals, cleared = check(self.PATH, data, reads=reads(self.READER, known=known))
        return [view for view, _, _, _ in literals], cleared

    def test_a_run_inside_an_i_range_is_cleared(self):
        self.assertEqual(self.verdict(["I 4 1+8"]), ([], [("writes", 8, "invariant")]))
        self.assertEqual(self.verdict(["I 4 0+9"]), ([], [("writes", 8, "invariant")]))

    def test_a_run_with_a_byte_outside_the_ranges_stays_literal(self):
        self.assertEqual(self.verdict(["I 4 1+7"]), (["writes"], []))
        self.assertEqual(self.verdict(["I 4 0+4 5+4"]), (["writes"], []))
        self.assertEqual(self.verdict([]), (["writes"], []))

    def test_a_run_of_bytes_c_knows_from_the_source_stays_literal(self):
        self.assertEqual(self.verdict(["I 4 1+8"], known=range(0x478, 0x490)), (["writes"], []))
        self.assertEqual(self.verdict(["I 4 1+8"], known=range(0x481, 0x490)),
                         ([], [("writes", 8, "invariant")]))

    def test_a_range_names_its_own_record_only(self):
        data = log("G g", "B 10 " + self.RUN.hex(), "B 20 " + self.RUN.hex(), "I 4 0+8")
        literals, cleared = check(self.PATH, data, reads=reads(self.READER))
        self.assertEqual([o for _, o, _, _ in literals], [data.rindex(self.RUN.hex().encode())])
        self.assertEqual(cleared, [("writes", 8, "invariant")])

    def test_without_read_sets_nothing_is_cleared_invariant(self):
        literals, cleared = check(self.PATH, log("G g", "B 10 " + self.RUN.hex(), "I 4 0+8"))
        self.assertEqual((len(literals), cleared), (1, []))


@contextlib.contextmanager
def patched(name, value):
    real = getattr(nintendo_bytes, name)
    setattr(nintendo_bytes, name, value)
    try:
        yield
    finally:
        setattr(nintendo_bytes, name, real)


class NeverRead(nintendo_bytes.ReadSets):
    def __contains__(self, item):
        return False


class KnowsNothing(nintendo_bytes.ReadSets):
    def knows(self, run):
        return False


class SelfProof(unittest.TestCase):
    def test_the_self_proof_passes(self):
        self.assertIsNone(nintendo_bytes.self_test())

    def test_a_broken_policy_fails_the_self_proof_on_the_first_case_it_breaks(self):
        thresholds = dict(nintendo_bytes.THRESHOLDS)
        thresholds[("general", "raw")] = 15
        low_info_everywhere = lambda run, scope, corpus: real_clearance(run, "general", corpus)
        real_clearance = nintendo_bytes._clearance
        no_bytelist = tuple(v for v in nintendo_bytes.VIEWS if v[0] != "bytelist")
        broken = [
            ("check_blob", lambda path, data, index, corpus, *a: ([], []), "strict-raw"),
            ("VIEWS", no_bytelist, "strict-bytelist"),
            ("_clearance", lambda run, scope, corpus: None, "strict-raw-trivial"),
            ("_clearance", low_info_everywhere, "strict-raw-low-info"),
            ("THRESHOLDS", thresholds, "general-raw-below"),
            ("_is_text", lambda data: True, "general-binary-corpus"),
            ("scope_of", lambda path: "general", "strict-raw"),
            ("scope_of", lambda path: "strict" if path.startswith(nintendo_bytes.STRICT_PREFIXES)
             else "general", "case-run-general"),
            ("WRITES_MIN", 5, "writes-in-read-set"),
            ("ReadSets", NeverRead, "writes-literal"),
            ("ReadSets", KnowsNothing, "writes-invariant-known"),
            ("_token_length", lambda field: 0, "writes-invariant"),
            ("_check_tokens", lambda view, data, index, corpus, min_len, reads:
             real_tokens(view, data, index, corpus, min_len, None), "writes-computed"),
            ("TABLES_PREFIX", "nowhere/", "tables-literal"),
            ("_table_tokens", lambda data: None, "tables-below"),
            ("_check_tokens", lambda view, data, index, corpus, min_len, reads:
             real_tokens(view, data, index, corpus, min_len, reads or planted_reads()),
             "tables-literal"),
        ]
        real_tokens = nintendo_bytes._check_tokens
        planted_reads = lambda: nintendo_bytes.planted_reads(nintendo_bytes.planted_rom())
        for name, value, case in broken:
            with patched(name, value):
                self.assertEqual(nintendo_bytes.self_test(), case, name)


class CommandLine(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = self.tmp.name
        self.rom = self.write("rom.bin", ROM)
        self.ladxr = os.path.join(self.dir, "ladxr")
        head = planted_ladxr(self.ladxr, [("names.py", b"NAME = '" + TEXT_BYTES[10:60] + b"'\n")])
        self.short = head[:8]
        pin = pinned(head)
        pin.__enter__()
        self.addCleanup(pin.__exit__, None, None, None)

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
            code = nintendo_bytes.main(["--rom", self.rom, "--ladxr", self.ladxr, *args])
        return code, out.getvalue().splitlines()

    def summary(self, files, literals, cleared=(0, 0, 0), override="", computed=0,
                invariant=0):
        return (f"nintendo-bytes: {files} files, rom {hashlib.sha1(ROM).hexdigest()}, "
                f"ladxr {self.short}, {override}{literals} literal runs, cleared {cleared[0]} "
                f"trivial, {cleared[1]} low-info, {cleared[2]} ladxr-public, {computed} computed"
                f", {invariant} invariant")

    def test_a_planted_directory_reports_each_run_sorted_by_path_then_offset(self):
        tree = os.path.join(self.dir, "tree")
        self.write("tree/b.txt", b"ok " + ROM[10:18].hex().encode() + b" " + ROM[40:60])
        self.write("tree/a/x.bin", ROM[900:920])
        self.write("tree/clean.txt", b"nothing here " + ROM[100:110] + ROM[ZEROS:ZEROS + 20])
        self.write("tree/.git/objects/leak", ROM[0:64])
        code, lines = self.run_main(tree)
        a = os.path.join(tree, "a", "x.bin")
        b = os.path.join(tree, "b.txt")
        self.assertEqual(lines[1:], [
            f"LITERAL {a} raw @0 20 bytes general",
            f"LITERAL {b} hex @3 8 bytes general",
            f"LITERAL {b} raw @20 20 bytes general",
            self.summary(3, 3, (1, 0, 0)),
        ])
        self.assertEqual(code, 1)

    def test_the_scope_follows_the_repository_relative_path(self):
        real = nintendo_bytes.TOP
        nintendo_bytes.TOP = self.dir
        try:
            strict = self.write("tests/ladx/extract/t.bin", ROM[100:104])
            general = self.write("data/t.bin", ROM[100:104])
            code, lines = self.run_main(strict, general)
        finally:
            nintendo_bytes.TOP = real
        self.assertEqual(lines[1:], [f"LITERAL {strict} raw @0 4 bytes strict", self.summary(2, 1)])
        self.assertEqual(code, 1)

    def test_scope_overrides_the_path(self):
        leak = self.write("leak.bin", ROM[100:104])
        code, lines = self.run_main("--scope", "strict", leak)
        self.assertEqual(lines[1:], [f"LITERAL {leak} raw @0 4 bytes strict", self.summary(1, 1)])
        self.assertEqual(code, 1)

    def test_min_overrides_every_threshold_and_says_so(self):
        leak = self.write("leak.bin", b"<" + ROM[100:106] + b">")
        code, lines = self.run_main("--min", "6", leak)
        self.assertEqual(lines[1:], [f"LITERAL {leak} raw @1 6 bytes general",
                                     self.summary(1, 1, override="min 6 (override), ")])

    def test_verbose_lists_the_cleared_runs(self):
        f = self.write("f.txt", b"[" + TEXT_BYTES[10:60] + b"] " + ROM[THREE:THREE + 20] + b" "
                       + ROM[ZEROS:ZEROS + 16])
        code, lines = self.run_main("--verbose", f)
        self.assertEqual(lines[1:], [
            f"CLEARED {f} raw @1 50 bytes ladxr-public",
            f"CLEARED {f} raw @53 20 bytes low-info",
            f"CLEARED {f} raw @74 16 bytes trivial",
            self.summary(1, 0, (1, 1, 1)),
        ])
        self.assertEqual(code, 0)
        code, lines = self.run_main(f)
        self.assertEqual(lines[1:], [self.summary(1, 0, (1, 1, 1))])

    def in_top(self, *files):
        real = nintendo_bytes.TOP
        nintendo_bytes.TOP = self.dir
        self.addCleanup(setattr, nintendo_bytes, "TOP", real)
        return [self.write(rel, data) for rel, data in files]

    def test_a_case_folders_write_log_gets_the_writes_view_and_the_computed_clearance(self):
        inventory = self.write("inv.md", nintendo_bytes.planted_inventory([
            ("g(rom)", "bank 0x00 source 0x0480..0x0487", "g"), ("h(rom)", "-", "h")]).encode())
        run = ROM[0x480:0x488].hex()
        data = log("G h", "B 10 " + run, "G g", "B 20 " + run)
        writes, spoiler = self.in_top(("tests/ladx/cases/c/rom_writes.txt", data),
                                      ("tests/ladx/cases/c/spoiler.json", ROM[100:108]))
        code, lines = self.run_main("--verbose", "--inventory", inventory, writes, spoiler)
        first, second = data.index(run.encode()), data.rindex(run.encode())
        self.assertEqual(lines[1:], [
            f"CLEARED {writes} writes @{first} 8 bytes computed",
            f"LITERAL {writes} writes @{second} 8 bytes strict",
            self.summary(2, 1, computed=1),
        ])
        self.assertEqual(code, 1)

    def test_a_missing_inventory_is_a_setup_line_only_when_a_run_needs_it(self):
        clean, leak = self.in_top(("tests/ladx/cases/c/rom_writes.txt", log("G g", "B 10 00")),
                                  ("tests/ladx/combos/c/rom_writes.txt",
                                   log("G g", "B 10 " + ROM[0x480:0x488].hex())))
        none = os.path.join(self.dir, "none.md")
        code, lines = self.run_main("--inventory", none, clean)
        self.assertEqual((code, lines[1:]), (0, [self.summary(1, 0)]))
        code, lines = self.run_main("--inventory", none, leak)
        self.assertEqual(code, 2)
        self.assertEqual(lines[-1], f"setup: nintendo-bytes: no inventory at {none}")

    def test_no_line_holds_the_rom_bytes_or_their_hex(self):
        leak = self.write("leak.txt", ROM[1000:1032] + b" " + ROM[2000:2032].hex().encode()
                          + b" " + TEXT_BYTES)
        code, lines = self.run_main("--verbose", "--scope", "strict", leak)
        self.assertEqual(code, 1)
        text = "\n".join(lines)
        for chunk in (ROM[1000:1032], ROM[2000:2032], TEXT_BYTES):
            for i in range(0, 32, 4):
                self.assertNotIn(chunk[i:i + 4].hex(), text.lower())
                self.assertNotIn(chunk[i:i + 4].decode("latin-1"), text)

    def test_a_missing_rom_is_a_setup_line(self):
        code, lines = self.run_main("--rom", os.path.join(self.dir, "none.gbc"), self.rom)
        self.assertEqual(code, 2)
        self.assertTrue(lines[-1].startswith("setup: nintendo-bytes: no ROM at "), lines)

    def test_a_missing_input_is_a_setup_line(self):
        code, lines = self.run_main(os.path.join(self.dir, "none"))
        self.assertEqual(code, 2)
        self.assertTrue(lines[-1].startswith("setup: nintendo-bytes: no such file "), lines)

    def test_a_missing_or_unpinned_ladxr_checkout_is_a_setup_line(self):
        clean = self.write("clean.txt", b"text\n")
        code, lines = self.run_main("--ladxr", os.path.join(self.dir, "none"), clean)
        self.assertEqual(code, 2)
        self.assertTrue(lines[-1].startswith("setup: nintendo-bytes: no LADXR checkout in "), lines)
        with pinned("1" * 40):
            code, lines = self.run_main(clean)
        self.assertEqual(code, 2)
        self.assertRegex(lines[-1], r"^setup: nintendo-bytes: LADXR checkout in .* not at the pin")

    def test_ladxr_dir_in_the_environment_is_the_default_checkout(self):
        clean = self.write("clean.txt", b"text\n")
        out = io.StringIO()
        real = os.environ.get("LADXR_DIR")
        os.environ["LADXR_DIR"] = self.ladxr
        try:
            with contextlib.redirect_stdout(out):
                code = nintendo_bytes.main(["--rom", self.rom, clean])
        finally:
            if real is None:
                del os.environ["LADXR_DIR"]
            else:
                os.environ["LADXR_DIR"] = real
        self.assertEqual(out.getvalue().splitlines()[1:], [self.summary(1, 0)])
        self.assertEqual(code, 0)

    def test_the_self_proof_passes_before_the_scan(self):
        clean = self.write("clean.txt", b"text\n")
        code, lines = self.run_main(clean)
        self.assertRegex(lines[0], r"^nintendo-bytes: self-test, \d+ planted files as expected$")
        self.assertEqual(code, 0)

    def test_the_script_refuses_a_checkout_not_at_the_real_pin(self):
        leak = self.write("leak.bin", ROM[50:70])
        env = dict(os.environ, LADXR_DIR=self.ladxr)
        result = subprocess.run([sys.executable, "-I", "-B", os.path.join(HERE, "nintendo_bytes.py"),
                                 "--rom", self.rom, leak], capture_output=True, text=True, env=env)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("not at the pin", result.stdout)


if __name__ == "__main__":
    unittest.main()
