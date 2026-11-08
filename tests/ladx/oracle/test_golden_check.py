
import contextlib
import hashlib
import importlib.util
import io
import os
import random
import shutil
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("golden_check", os.path.join(HERE,
                                                                           "golden_check.py"))
golden_check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(golden_check)
write_log = golden_check.write_log

K = write_log.KNOWN
SIDES = {"fix": "table", "cg": "C", "other": "C", "spoiler": "C", "plan": "-"}


def bank(group, offset, values, origins=None):
    values = bytes(values)
    return ("B", group, "site", offset, values, tuple(origins or [K] * len(values)))


def entry(group, table, key, value):
    value = bytes(value)
    return ("T", group, "site", table, key, value, (K,) * len(value))


def write(path, data):
    with open(path, "wb") as f:
        f.write(data if isinstance(data, bytes) else data.encode())


class Case(unittest.TestCase):
    def setUp(self):
        self.rom = random.Random(0x60D).randbytes(0x800)
        self.known = set(range(0x100, 0x180))
        old = self.rom[0x100:0x108]
        self.events = write_log.Events(["texts"], [("texts", 0, old, tuple(range(0x100, 0x108)))], [
            bank("fix", 0x10, self.rom[0x300:0x308]),
            bank("cg", 0x20, self.rom[0x40:0x42] + b"\x00", (0x40, 0x41, K)),
            entry("cg", "texts", 0, old[:3] + b"\x09" + old[4:]),
            bank("other", 0x30, b"\x01\x02"),
            bank("fix", 0x30, self.rom[0x30:0x32])])
        self.folder = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.folder)
        self.log = write_log.encode(self.events, self.rom, SIDES, self.known)
        out = write_log.decode(self.log, self.rom)
        write(os.path.join(self.folder, "rom_writes.txt"), self.log)
        write(os.path.join(self.folder, "run.txt"),
              "seed 00\nrom_sha1 %s\noutcome ok\n" % hashlib.sha1(out).hexdigest())
        write(os.path.join(self.folder, "spoiler.json"), "{}\n")
        self.seen = []

    def nintendo(self, name, data, scope):
        self.seen.append((name, scope))
        return []

    def check(self, **kw):
        return golden_check.check_case(self.folder, self.events, self.rom,
                                       kw.pop("sides", SIDES), self.known,
                                       kw.pop("nintendo", self.nintendo), **kw)

    def test_a_golden_of_its_stream_passes_every_check(self):
        self.assertEqual(self.check(), [])
        self.assertEqual(self.seen, [("rom_writes.txt", "writes"), ("run.txt", "general"),
                                     ("spoiler.json", "general")])

    def test_a_race_case_has_no_spoiler_to_check(self):
        os.remove(os.path.join(self.folder, "spoiler.json"))
        self.assertEqual(self.check(), [])
        self.assertEqual([name for name, _ in self.seen], ["rom_writes.txt", "run.txt"])

    def test_a_log_that_does_not_decode_to_rom_sha1(self):
        write(os.path.join(self.folder, "run.txt"), "rom_sha1 %s\noutcome ok\n" % ("0" * 40))
        problems = self.check()
        self.assertEqual([c for c, _ in problems], ["decode"])
        self.assertTrue(problems[0][1].endswith("run.txt rom_sha1 " + "0" * 40))

    def test_a_golden_that_is_not_the_stream_s_encoding(self):
        write(os.path.join(self.folder, "rom_writes.txt"), self.log.replace("B 30 0102",
                                                                          "B 30 0103"))
        self.assertEqual(self.check(), [
            ("encode", "rom_writes.txt is not the stream's encoding, from line 9")])

    def test_the_i_lines_must_be_the_re_run_record_s(self):
        self.events.events.append(bank("cg", 0x50, self.rom[0x500:0x504]))
        flags = {5: (True,) * 4}
        marked = write_log.encode(self.events, self.rom, SIDES, self.known, flags)
        self.assertIn("\nI ", marked)
        write(os.path.join(self.folder, "rom_writes.txt"), marked)
        write(os.path.join(self.folder, "run.txt"), "rom_sha1 %s\noutcome ok\n"
              % hashlib.sha1(write_log.decode(marked, self.rom)).hexdigest())
        self.assertEqual(self.check(invariant=flags), [])
        self.assertEqual([c for c, _ in self.check(invariant={})], ["encode"])

    def test_a_group_outside_the_table_and_c_groups(self):
        sides = dict(SIDES, other="-")
        problems = self.check(sides=sides)
        self.assertIn(("tags", "other is not a table or C group of the inventory"), problems)

    def test_a_c_group_whose_encoding_depends_on_an_unknowable_byte(self):
        problems = self.check(scramble_known=set())
        self.assertEqual([c for c, _ in problems], ["scrambled"])
        self.assertTrue(problems[0][1].startswith("SCRAMBLED cg:"))

    def test_a_projection_that_depends_on_another_group_is_flagged(self):
        real = write_log.projections

        def leaky(text):
            got = real(text)
            return {g: v + str(len(got)) for g, v in got.items()}
        with mock.patch.object(write_log, "projections", leaky):
            problems = self.check()
        self.assertIn(("projection", "dropping cg's records changes fix, other"), problems)

    def test_a_projection_blind_to_its_own_group_fails_the_check_s_self_proof(self):
        real = write_log.projections
        with mock.patch.object(write_log, "projections",
                               lambda text: {g: "" for g in real(text)}):
            problems = self.check()
        self.assertIn(("projection", "changing cg's records does not show in its projection"),
                      problems)

    def test_literal_runs_are_reported_per_file(self):
        problems = self.check(nintendo=lambda name, data, scope:
                              ["@3 4 bytes writes"] if name == "rom_writes.txt" else [])
        self.assertEqual(problems, [("nintendo", "rom_writes.txt: @3 4 bytes writes")])

    def known_source(self):
        ks = golden_check._sibling("known_source").KnownSource(len(self.rom))
        ks.add(0x100, 0x108, "texts 0", ("texts", 0))
        ks.add(0x108, 0x180, "other")
        return ks

    def test_a_golden_of_its_stream_has_no_fragment(self):
        ks = self.known_source()
        self.known = ks
        log = write_log.encode(self.events, self.rom, SIDES, ks)
        write(os.path.join(self.folder, "rom_writes.txt"), log)
        self.assertEqual(self.check(), [])

    def test_a_piece_of_the_replaced_entry_kept_as_a_literal_is_a_fragment(self):
        self.known = self.known_source()
        old = self.rom[0x100:0x108]
        other = bytes(v for v in range(256) if v not in old)[:1]
        lines = self.log.splitlines()
        n = next(i for i, line in enumerate(lines) if line.startswith("T texts 0 "))
        lines[n] = "T texts 0 " + (old[1:3] + other + old[5:6]).hex()
        write(os.path.join(self.folder, "rom_writes.txt"), "".join(l + "\n" for l in lines))
        problems = self.check()
        self.assertIn(("fragments", "cg 2 fragments, 3 bytes, first at line %d" % (n + 1)),
                      problems)
        self.assertEqual([c for c, _ in problems], ["encode", "fragments"])

    def test_an_unparsable_log_stops_at_its_format(self):
        write(os.path.join(self.folder, "rom_writes.txt"), "ladx-rom-writes 2\n")
        self.assertEqual([c for c, _ in self.check()], ["format"])


class Union(unittest.TestCase):
    def test_the_union_is_every_table_and_c_group_but_spoiler(self):
        self.assertEqual(golden_check.union_problems({"fix", "cg", "other"}, SIDES), [])
        self.assertEqual(golden_check.union_problems({"fix", "plan"}, SIDES),
                         ["no golden writes cg", "no golden writes other",
                          "plan writes but is not a writing group"])


class Command(unittest.TestCase):
    def test_a_missing_rom_is_a_setup_line(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = golden_check.main(["--rom", os.path.join(HERE, "no-such.gbc")])
        self.assertEqual(status, 2)
        self.assertTrue(out.getvalue().startswith("setup: golden-check: no ROM at"))


if __name__ == "__main__":
    unittest.main()
