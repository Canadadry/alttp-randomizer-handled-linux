"""Usage: python3 -I test_read_check.py

Unit tests of read_check.py on planted traces, inventories and LADXR-shaped sources written to a
temporary directory, with no LADXR and no ROM; and of inventory_trace.sh's setup lines.
"""

import contextlib
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("read_check", os.path.join(HERE, "read_check.py"))
read_check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(read_check)

GEN, HDR = read_check.GEN, read_check.HDR
line = read_check.read_line
wline = read_check.write_line


class Fixture(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="read-check-test-")
        self.addCleanup(shutil.rmtree, self.dir)
        self.ladxr = os.path.join(self.dir, "ladxr")
        for rel, text in read_check.PLANTED_SOURCES.items():
            path = os.path.join(self.ladxr, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as f:
                f.write(text)
        self.runs = read_check.planted_runs()
        self.rows, self.groups = list(read_check.PLANTED_ROWS), list(read_check.PLANTED_GROUPS)
        self.coverage = list(read_check.PLANTED_COVERAGE)
        self.cuts = read_check.PLANTED_CUTS

    def add_read(self, text, run=("a", "seed-0")):
        reads, writes, outcome = self.runs[run]
        self.runs[run] = (reads + [text], writes, outcome)

    def add_write(self, text, run=("a", "seed-0")):
        reads, writes, outcome = self.runs[run]
        self.runs[run] = (reads, writes + [text], outcome)

    def set_cell(self, index, cell):
        section, key, side, group, _ = self.rows[index]
        self.rows[index] = (section, key, side, group, cell)

    def inventory(self):
        return read_check.planted_inventory(self.rows, self.groups, self.coverage)

    def check(self):
        traces = os.path.join(self.dir, "traces")
        shutil.rmtree(traces, ignore_errors=True)
        read_check.write_traces(traces, self.runs)
        return read_check.check(self.ladxr, self.inventory(), traces, self.cuts)

    def kinds(self):
        return sorted({finding.split()[0] for finding in self.check().findings})

    def run_main(self, *extra, inventory=None, traces=None):
        if traces is None:
            traces = os.path.join(self.dir, "traces")
            shutil.rmtree(traces, ignore_errors=True)
            read_check.write_traces(traces, self.runs)
        inv = os.path.join(self.dir, "inventory.md")
        with open(inv, "w") as f:
            f.write(self.inventory() if inventory is None else inventory)
        cuts = os.path.join(self.dir, "cuts.md")
        with open(cuts, "w") as f:
            f.write(self.cuts)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = read_check.main(["--ladxr", self.ladxr, "--inventory", inv, "--traces",
                                    traces, "--scope-cuts", cuts] + list(extra))
        return code, out.getvalue()


class Clean(Fixture):
    def test_clean_set_passes_with_the_summary(self):
        code, out = self.run_main()
        self.assertEqual(code, 0, out)
        lines = out.splitlines()
        self.assertEqual(lines[0], "read-check: self-test, 17 planted trace sets as expected")
        self.assertEqual(lines[-1], "read-check: 2 cases, 84 reads, 3 groups covered")

    def test_information_lines_do_not_fail(self):
        code, out = self.run_main()
        self.assertEqual(code, 0)
        self.assertIn("STABLE %s: patches.p.summed(rom, c)" % GEN, out)
        self.assertIn("UNREACHED %s: patches.p.never(rom)" % GEN, out)
        self.assertIn("UNREACHED %s: checksum = 0" % HDR, out)
        self.assertNotIn("UNREACHED %s: checksum -= c + 1" % HDR, out)

    def test_finding_exits_1_with_the_count(self):
        self.set_cell(4, "-")
        code, out = self.run_main()
        self.assertEqual(code, 1)
        self.assertEqual(out.splitlines()[-1], "read-check: 1 findings")


class SiteResolution(Fixture):
    def test_a_for_header_read_goes_to_the_first_row_of_its_body(self):
        self.assertEqual(self.check().cells[(GEN, "patches.p.summed(rom, c)")].split("; ")[0],
                         "bank 0x00 source 0x0010..0x0011")

    def test_a_continuation_line_read_goes_to_its_statement(self):
        cell = self.check().cells[(GEN, "patches.p.items(rom,")]
        self.assertIn("bank 0x04 written:fixed 0x0000..0x0001", cell)

    def test_a_header_governing_no_row_is_untagged(self):
        self.add_read(line("-", GEN, "while rom.banks[0][0x20]:", "bank 0x00", "source",
                           "n=1 0x0020..0x0020"))
        findings = self.check().findings
        self.assertEqual(findings, ["UNTAGGED %s: while rom.banks[0][0x20]:: read bank 0x00 via "
                                    "p.py:f" % GEN])

    def test_an_unknown_site_and_no_site_are_untagged(self):
        self.add_read(line("-", None, None, "bank 0x09", "source", "n=1"))
        self.add_write(wline("-", GEN, "gone(rom)", 1, "1" * 40))
        self.assertEqual(sorted(self.check().findings),
                         ["UNTAGGED -: read bank 0x09 via p.py:f",
                          "UNTAGGED %s: gone(rom): write" % GEN])

    def test_traces_older_than_the_inventory_are_setup(self):
        self.groups = [g for g in self.groups]
        self.rows[4] = (GEN, "patches.p.later(rom)", "table", "fixed", self.rows[4][4])
        code, out = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("setup: read-check: %s: patches.p.later(rom) traced in group later" % GEN,
                      out)


class Findings(Fixture):
    def test_an_uncovered_c_read(self):
        self.coverage = self.coverage[:1] + self.coverage[2:]
        self.assertEqual(self.check().findings,
                         ["UNCOVERED items %s: patches.p.items(rom,: bank 0x03 "
                          "0x0000..0x0000 source" % GEN])

    def test_a_read_in_a_table_row_needs_no_coverage(self):
        self.assertNotIn("later", " ".join(self.coverage))
        self.assertEqual(self.check().findings, [])

    def test_a_none_row_access_outside_parse(self):
        self.add_read(line("consts", GEN, 'assembler.const("X", 1)', "bank 0x09", "source",
                           "n=2 0x0000..0x0001"))
        self.set_cell(5, "bank 0x09 source 0x0000..0x0001")
        self.assertEqual(self.check().findings,
                         ['NONE-ACCESS %s: assembler.const("X", 1): 2 reads, 0 writes' % GEN])

    def test_a_parse_row_may_read(self):
        self.assertEqual(self.rows[0][3], "parse")
        self.assertEqual(self.check().findings, [])

    def test_an_unused_row_reached(self):
        self.add_write(wline("debug", GEN, "patches.p.debug(rom)", 3, "1" * 40))
        self.assertEqual(self.check().findings,
                         ["UNUSED-REACHED %s: patches.p.debug(rom): 0 reads, 3 writes" % GEN])

    def test_a_table_row_varying_with_the_seed(self):
        reads, writes, outcome = self.runs[("a", "seed-2")]
        self.runs[("a", "seed-2")] = (reads, [w.replace("f" * 40, "d" * 40) for w in writes],
                                      outcome)
        self.assertEqual(self.check().findings,
                         ["SEED-VARIES %s: patches.p.fixed(rom): cases a" % GEN])

    def test_a_table_row_written_under_one_seed_only_varies(self):
        reads, writes, outcome = self.runs[("a", "seed-2")]
        self.runs[("a", "seed-2")] = (reads, [w for w in writes if "fixed" not in w], outcome)
        self.assertEqual(self.kinds(), ["SEED-VARIES"])

    def test_an_error_run_is_not_compared(self):
        reads, writes, _ = self.runs[("b", "seed-1")]
        self.assertTrue(any("e" * 40 in w for w in writes))
        self.assertEqual(self.check().findings, [])

    def test_a_c_row_varying_is_neither_a_finding_nor_stable(self):
        checker = self.check()
        self.assertEqual(checker.findings, [])
        self.assertNotIn("STABLE %s: patches.p.items(rom," % GEN, checker.infos)

    def test_a_table_row_reading_a_c_groups_bytes(self):
        self.add_read(line("later", GEN, "patches.p.later(rom)", "bank 0x02", "mixed:items",
                           "n=1 0x0000..0x0001"))
        self.set_cell(4, "bank 0x02 mixed:items 0x0000..0x0001; "
                         "bank 0x02 written:fixed 0x0000..0x0001")
        self.groups = [("later", "table", "fixed, items", "-") if g[0] == "later" else g
                       for g in self.groups]
        self.assertEqual(self.check().findings,
                         ["TABLE-READS-SEED %s: patches.p.later(rom): bank 0x02 mixed:items" % GEN])

    def test_depends_on_differs_from_the_trace(self):
        self.groups = [("sum", "C", "-", "sum") if g[0] == "sum" else g for g in self.groups]
        self.assertEqual(self.check().findings, ["DEPENDS sum: inventory -, trace items"])

    def test_reads_column_differs_from_the_trace(self):
        self.set_cell(1, "bank 0x01 source 0x0000..0x0001")
        self.assertEqual(self.check().findings,
                         ["READS-COLUMN %s: patches.p.fixed(rom): inventory bank 0x01 source "
                          "0x0000..0x0001, trace -" % GEN])


class Coverage(Fixture):
    def replace(self, old, new):
        self.assertTrue(any(old in c for c in self.coverage), old)
        self.coverage = [c.replace(old, new) for c in self.coverage]

    def test_copy_needs_a_reason(self):
        self.replace("| copy | moved to bank 0x01 |", "| copy |  |")
        self.assertEqual(self.check().findings,
                         ["BAD-COVER sum * bank 0x00 0x0010..0x0011: a copy needs a Reason "
                          "saying where the bytes go"])

    def test_written_refuses_a_source_read(self):
        self.replace("| 0x0010..0x0011 | copy |", "| 0x0010..0x0011 | written |")
        self.assertEqual(self.check().findings,
                         ["BAD-COVER sum * bank 0x00 0x0010..0x0011: written read of "
                          "provenance source"])

    def test_written_refuses_a_mixed_read(self):
        self.add_read(line("sum", GEN, "patches.p.summed(rom, c)", "bank 0x01", "mixed:items",
                           "n=1 0x0004..0x0004"))
        self.set_cell(3, "bank 0x00 source 0x0010..0x0011; bank 0x01 mixed:items 0x0004..0x0004; "
                         "bank 0x01 written:items 0x0000..0x0003; bank 0x05..0x06 source all")
        self.assertEqual(self.kinds(), ["BAD-COVER"])

    def test_table_refuses_bytes_a_c_group_wrote(self):
        self.replace("| bank 0x01 | * | written |", "| bank 0x01 | * | table |")
        self.assertEqual(self.check().findings,
                         ["BAD-COVER sum `patches.p.summed(rom, c)` bank 0x01 *: table read "
                          "written by a group that is not table: written:items"])

    def test_computed_only_on_the_header_checksum_rows(self):
        self.replace("| `checksum -= c + 1` | bank 0x00 | * | computed |",
                     "| * | bank 0x00 | * | computed |")
        self.assertEqual(self.kinds(), ["BAD-COVER"])

    def test_computed_outside_the_header_group(self):
        self.replace("| copy | moved to bank 0x01 |", "| computed | moved |")
        self.assertEqual(self.kinds(), ["BAD-COVER"])

    def test_rom_needs_a_reason(self):
        self.replace("| rom | decides the sprite sets |", "| rom |  |")
        self.assertEqual(self.check().findings, [
            "BAD-COVER items `patches.p.items(rom,` table entities 0x0000..0x00FF: a rom read "
            "needs a Reason saying what its value decides"])

    def test_an_extraction_kind_is_unknown(self):
        self.replace("| rom | decides the sprite sets |", "| extract:types | decides |")
        self.assertEqual(self.kinds(), ["BAD-COVER"])

    def test_cut_must_be_in_the_scope_cuts_note(self):
        self.cuts = "# Scope cuts\n\nNo cut so far.\n"
        self.assertEqual(self.check().findings,
                         ["BAD-COVER items * bank 0x03 *: cut no-x is not in the scope-cuts note"])

    def test_cut_as_a_list_item(self):
        self.cuts = "# Scope cuts\n\n- `no-x`: x=1 is cut.\n"
        self.assertEqual(self.check().findings, [])

    def test_a_missing_scope_cuts_note_fails_every_cut(self):
        self.cuts = None
        self.assertEqual(self.kinds(), ["BAD-COVER"])

    def test_an_entry_matching_no_read(self):
        self.coverage.append("| items | * | bank 0x30 | * | copy | nowhere |")
        self.assertEqual(self.check().findings,
                         ["BAD-COVER items * bank 0x30 *: matches no read"])

    def test_an_entry_of_a_table_group_or_a_row_of_another_group(self):
        self.coverage.append("| later | * | * | * | copy | the later patch |")
        self.coverage.append("| items | `patches.p.summed(rom, c)` | * | * | copy | x |")
        findings = self.check().findings
        self.assertIn("BAD-COVER later * * *: group later is not a C group", findings)
        self.assertIn("BAD-COVER items `patches.p.summed(rom, c)` * *: no row "
                      "`patches.p.summed(rom, c)` in group items", findings)

    def test_a_range_must_hold_the_read_and_a_whole_read_needs_a_star(self):
        self.replace("| sum | * | * | * | copy |", "| sum | * | * | 0x0000..0xFFFF | copy |")
        self.assertEqual(self.check().findings,
                         ["UNCOVERED sum %s: patches.p.summed(rom, c): bank 0x05 all source" % GEN,
                          "UNCOVERED sum %s: patches.p.summed(rom, c): bank 0x06 all source" % GEN])


class Cells(unittest.TestCase):
    def read(self, target, prov, span):
        return read_check.Read("g", None, "c", "slice", target, prov, 1, span)

    def test_no_read_is_a_dash(self):
        self.assertEqual(read_check.reads_cell([]), "-")

    def test_ranges_merge_per_target_and_provenance(self):
        cell = read_check.reads_cell([
            self.read("bank 0x02", "source", (5, 9)), self.read("bank 0x02", "source", (1, 3)),
            self.read("bank 0x02", "written:a", (4, 4)), self.read("table t", "source", (0, 0)),
            self.read("bank 0x02", "source", None)])
        self.assertEqual(cell, "bank 0x02 source all; bank 0x02 written:a 0x0004..0x0004; "
                               "table t source 0x0000..0x0000")

    def test_consecutive_banks_of_one_provenance_and_range_merge(self):
        cell = read_check.reads_cell([self.read("bank 0x%02X" % n, "source", None)
                                      for n in (0, 1, 2, 5)] +
                                     [self.read("bank 0x03", "written:a", None)])
        self.assertEqual(cell, "bank 0x00..0x02 source all; bank 0x03 written:a all; "
                               "bank 0x05 source all")


class Print(Fixture):
    def test_print_round_trips(self):
        self.set_cell(1, "stale")
        self.set_cell(3, "-")
        self.groups = [("sum", "C", "-", "sum") if g[0] == "sum" else g for g in self.groups]
        code, out = self.run_main("--print")
        self.assertEqual(code, 0)
        self.assertNotIn("self-test", out)
        code, again = self.run_main(inventory=out)
        self.assertEqual(code, 0, again)
        self.assertEqual(out, read_check.planted_inventory())

    def test_print_keeps_lines_it_does_not_change(self):
        text = self.inventory().replace("| `patches.p.never(rom)` | - |",
                                        "|  `patches.p.never(rom)`   | - |")
        code, out = self.run_main("--print", inventory=text)
        self.assertEqual(code, 0)
        self.assertIn("|  `patches.p.never(rom)`   | - |", out)


class Setup(Fixture):
    def test_no_checkout(self):
        shutil.rmtree(self.ladxr)
        os.makedirs(self.ladxr)
        code, out = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("setup: read-check: no LADXR checkout", out)

    def test_no_traces(self):
        code, out = self.run_main(traces=os.path.join(self.dir, "none"))
        self.assertEqual(code, 2)
        self.assertIn("setup: read-check: no traces in", out)

    def test_a_seed_folder_without_writes(self):
        traces = os.path.join(self.dir, "traces")
        read_check.write_traces(traces, self.runs)
        os.remove(os.path.join(traces, "a", "seed-1", "writes.txt"))
        code, out = self.run_main(traces=traces)
        self.assertEqual(code, 2)
        self.assertIn("setup: read-check: no writes.txt in", out)

    def test_a_malformed_read_line(self):
        self.add_read("only\ttwo")
        code, out = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("setup: read-check: malformed line in", out)

    def test_no_inventory(self):
        traces = os.path.join(self.dir, "traces")
        read_check.write_traces(traces, self.runs)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = read_check.main(["--ladxr", self.ladxr, "--traces", traces, "--inventory",
                                    os.path.join(self.dir, "missing.md")])
        self.assertEqual(code, 2)
        self.assertIn("setup: read-check: no inventory", out.getvalue())

    def test_self_test_passes(self):
        out = io.StringIO()
        self.assertEqual(read_check.self_test(out), 0, out.getvalue())


class Driver(unittest.TestCase):
    SCRIPT = os.path.join(HERE, "inventory_trace.sh")

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="inventory-trace-test-", dir=os.path.join(TOP, "tmp")
                                    if os.path.isdir(os.path.join(TOP, "tmp")) else None)
        self.addCleanup(shutil.rmtree, self.dir)
        self.out = os.path.join(TOP, "tmp", "inventory-trace-test-out")

    def run_driver(self, *args):
        done = subprocess.run(["sh", self.SCRIPT] + list(args), stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, universal_newlines=True)
        return done.returncode, done.stdout

    def test_no_rom_is_setup(self):
        code, out = self.run_driver("--out", self.out, "--rom", os.path.join(self.dir, "no.gbc"))
        self.assertEqual(code, 2, out)
        self.assertIn("setup: inventory_trace: no ROM at", out)
        self.assertFalse(os.path.exists(self.out))

    def test_no_checkout_is_setup(self):
        rom = os.path.join(self.dir, "base.gbc")
        with open(rom, "wb") as f:
            f.write(b"\x00" * 16)
        code, out = self.run_driver("--out", self.out, "--rom", rom, "--ladxr", self.dir)
        self.assertEqual(code, 2, out)
        self.assertIn("setup: inventory_trace: %s is not a LADXR checkout" % self.dir, out)
        self.assertFalse(os.path.exists(self.out))

    def test_out_outside_tmp_is_refused(self):
        code, out = self.run_driver("--out", os.path.join(HERE, "traces"))
        self.assertEqual(code, 2, out)
        self.assertIn("refused", out)

    def test_bad_side_seeds_are_refused(self):
        seeds = os.path.join(self.dir, "seeds.txt")
        with open(seeds, "w") as f:
            f.write("# header\nnot-hex\n")
        code, out = self.run_driver("--out", self.out, "--seeds", seeds)
        self.assertEqual(code, 2, out)
        self.assertIn("bad seed line", out)

    def test_the_committed_side_seeds_are_two_hex_seeds(self):
        with open(os.path.join(HERE, "side_seeds.txt")) as f:
            seeds = [l for l in f.read().splitlines() if l and not l.startswith("#")]
        self.assertEqual(len(seeds), 2)
        for seed in seeds:
            self.assertEqual(len(bytes.fromhex(seed)), 16)


if __name__ == "__main__":
    unittest.main()
