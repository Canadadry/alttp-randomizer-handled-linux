"""Unit tests for rank_e2e.py, check-e2e's failure ranker, through its command
line, on planted test directories and planted driver outputs.
Run: make test-template"""

import os
import re
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

SKILL = Path(__file__).resolve().parent.parent
RANK = SKILL / "scripts" / "rank_e2e.py"
REPO = SKILL.parent.parent.parent

ORDER = """\
ladx-tables-order 1
section generator.py generateRom
table fixed fixed.1 a(rom)
C bank3e - b(rom)
table options options.1 c(rom)
C marin - d(rom)
C hints - e(rom)
table fixed fixed.2 f(rom)
section randomizer.py Randomizer.__init__
C save - g(rom)
C spoiler - h(rom)
section rom.py ROM.fixHeader
C header - i(rom)
"""


def ladx_tree(root, cases=(), combos=()):
    game = Path(root) / "tests" / "ladx"
    for sub, names in (("cases", cases), ("combos", combos)):
        for name in names:
            (game / sub / name).mkdir(parents=True)
    tables = Path(root) / "data" / "ladx"
    tables.mkdir(parents=True)
    (tables / "order.txt").write_text(ORDER)
    return game


def divergent(case, *detail):
    return "DIVERGENT %s\n" % case + "".join("  %s\n" % line for line in detail)


def detail(section, n, golden="g", c="c"):
    return ["%s: line %d:" % (section, n), "  golden: %s" % golden, "  c: %s" % c]


class Ranker(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def output(self, name, text):
        path = Path(self.root) / "out" / ("%s.txt" % name)
        path.parent.mkdir(exist_ok=True)
        path.write_text(text)
        return str(path)

    def rank(self, game_dir, *files):
        proc = subprocess.run([sys.executable, str(RANK), game_dir] + list(files), cwd=self.root,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        return proc.returncode, proc.stdout

    def ladx(self, cases=(), combos=()):
        ladx_tree(self.root, cases, combos)
        return "tests/ladx"

    def test_placement_ranks_before_an_earlier_write_log_failure(self):
        game = self.ladx(cases=["a-case", "z-case"])
        e2e = self.output("e2e", "e2e: claims stage fill, groups hints\n"
                          + divergent("a-case", *detail("hints", 3))
                          + divergent("z-case", *detail("place", 40))
                          + "e2e: 0 pass, 2 divergent\n")
        code, out = self.rank(game, e2e)
        self.assertEqual(code, 1, out)
        lines = out.splitlines()
        self.assertEqual(lines[0], "rank-e2e: 2 failures in tests/ladx (e2e)")
        self.assertEqual(lines[1], "1. e2e z-case: seed place line 40")
        self.assertEqual(lines[2], "2. e2e a-case: write log hints line 3")
        self.assertEqual(lines[3], "TOP e2e z-case")
        self.assertEqual(lines[4:8], ["DIVERGENT z-case", "  place: line 40:", "    golden: g", "    c: c"])
        self.assertEqual(lines[8], "REPRO make -C tests/ladx e2e CASES=z-case")

    def order_of(self, out):
        return [line.split(" ", 1)[1] for line in out.splitlines() if re.match(r"^\d+\. ", line)]

    def test_write_log_groups_rank_in_order_txt_order(self):
        game = self.ladx(cases=["a", "b", "c"])
        e2e = self.output("e2e", divergent("a", *detail("marin", 1)) + divergent("b", *detail("options", 9))
                          + divergent("c", *detail("fixed", 50)) + "e2e: 0 pass, 3 divergent\n")
        code, out = self.rank(game, e2e)
        self.assertEqual(self.order_of(out), ["e2e c: write log fixed line 50", "e2e b: write log options line 9",
                                              "e2e a: write log marin line 1"], out)

    def test_a_groups_trace_ranks_before_its_write_log_and_after_the_group_before(self):
        game = self.ladx(cases=["a", "b", "c"])
        e2e = self.output("e2e", divergent("a", *detail("hints", 1)) + divergent("b", *detail("trace hints", 4))
                          + divergent("c", *detail("trace marin", 7)) + "e2e: 0 pass, 3 divergent\n")
        code, out = self.rank(game, e2e)
        self.assertEqual(self.order_of(out), ["e2e c: trace marin line 7", "e2e b: trace hints line 4",
                                              "e2e a: write log hints line 1"], out)

    def test_a_case_ranks_at_its_cheapest_detail(self):
        game = self.ladx(cases=["a", "b"])
        e2e = self.output("e2e", divergent("a", *(detail("place", 30) + detail("hints", 2)))
                          + divergent("b", *detail("place", 8)) + "e2e: 0 pass, 2 divergent\n")
        code, out = self.rank(game, e2e)
        self.assertEqual(self.order_of(out), ["e2e b: seed place line 8", "e2e a: seed place line 30"], out)

    def test_generator_failure_then_seed_then_no_log_then_groups_then_spoiler_then_net(self):
        game = self.ladx(cases=["a", "b", "c", "d", "e", "f", "g"])
        e2e = self.output("e2e", divergent("a", "net: 1 offsets differ, first at 14d: golden literal, c literal")
                          + divergent("b", "spoiler: c wrote no spoiler.json")
                          + divergent("c", *detail("header", 2))
                          + divergent("d", *detail("save", 2))
                          + divergent("e", "c wrote no rom_writes.txt")
                          + divergent("f", *detail("ws", 3))
                          + divergent("g", "c generator exited 1", "  boom")
                          + "e2e: 0 pass, 7 divergent\n")
        code, out = self.rank(game, e2e)
        self.assertEqual(self.order_of(out), [
            "e2e g: seed generator: c generator exited 1", "e2e f: seed ws line 3",
            "e2e e: write log: c wrote no rom_writes.txt", "e2e d: write log save line 2", "e2e b: spoiler",
            "e2e c: write log header line 2", "e2e a: net"], out)

    def test_baseline_ranks_first_then_run_order_whatever_the_argument_order_then_case_name(self):
        game = self.ladx(cases=["baseline", "a", "b"], combos=["c"])
        e2e = self.output("e2e", divergent("b", *detail("place", 1)) + divergent("a", *detail("place", 1))
                          + divergent("baseline", *detail("hints", 9)) + "e2e: 0 pass, 3 divergent\n")
        combos = self.output("e2e-combos", divergent("c", *detail("place", 1)) + "e2e: 0 pass, 1 divergent\n")
        code, out = self.rank(game, combos, e2e)
        self.assertEqual(self.order_of(out), ["e2e baseline: write log hints line 9", "e2e a: seed place line 1",
                                              "e2e b: seed place line 1", "e2e-combos c: seed place line 1"], out)
        self.assertIn("rank-e2e: 4 failures in tests/ladx (e2e, e2e-combos)", out)

    def test_target_level_lines_rank_before_every_case(self):
        game = self.ladx(cases=["baseline"])
        e2e = self.output("e2e", divergent("baseline", *detail("place", 1)) + "e2e: 0 pass, 1 divergent\n")
        rom = self.output("e2e-rom", "KS DIFFERENT: line 3: boss item 3\n")
        code, out = self.rank(game, e2e, rom)
        self.assertEqual(code, 1)
        self.assertEqual(self.order_of(out), ["e2e-rom (target): target: KS DIFFERENT: line 3: boss item 3",
                                              "e2e baseline: seed place line 1"], out)
        self.assertIn("REPRO make -C tests/ladx e2e-rom\n", out)

    def test_projection_differences_repro_with_the_driver_on_that_case(self):
        game = self.ladx(combos=["sample-01"])
        out_file = self.output("e2e-combos", "e2e-combos: the combination cases\n"
                               "PROJECTION sample-01 hints: line 4\n"
                               "projections: 40 goldens, 1 differences from write_log.py's\n")
        code, out = self.rank(game, out_file)
        self.assertIn("TOP e2e-combos (target)\nPROJECTION sample-01 hints: line 4\n", out)
        self.assertIn("REPRO python3 -I -B tests/ladx/run_e2e.py --projections --cases tests/ladx/combos "
                      "--gen tests/ladx/build/ladx_gen sample-01\n", out)

    def test_an_output_without_a_verdict_is_a_target_failure(self):
        game = self.ladx(cases=["a"])
        e2e = self.output("e2e", "gcc -c x.c\nx.c:1: error: boom\nmake: *** [x.o] Error 1\n")
        code, out = self.rank(game, e2e)
        self.assertEqual(code, 1)
        self.assertIn("1. e2e (target): target: no verdict line (a build or driver failure): see e2e's output", out)
        self.assertIn("REPRO make -C tests/ladx e2e\n", out)

    def test_setup_lines_are_reported_apart_and_exit_3(self):
        game = self.ladx(cases=["a"])
        e2e = self.output("e2e", "e2e: claims stage fill\nPASS a (fill)\ne2e: 1 pass, 0 divergent\n")
        rom = self.output("e2e-rom", "setup: e2e-rom: no ROM at tmp/ladx/base.gbc (fetch it)\n")
        code, out = self.rank(game, e2e, rom)
        self.assertEqual((code, out.splitlines()), (3, [
            "SETUP e2e-rom: setup: e2e-rom: no ROM at tmp/ladx/base.gbc (fetch it)",
            "rank-e2e: no failure in tests/ladx (e2e, e2e-rom)"]))

    def test_green_outputs_exit_0(self):
        game = self.ladx(cases=["a"])
        e2e = self.output("e2e", "projections: 1 goldens, 3 group projections equal to write_log.py's\n"
                          "e2e: claims stage fill\nPASS a (fill)\ne2e: 1 pass, 0 divergent\n")
        code, out = self.rank(game, e2e)
        self.assertEqual((code, out), (0, "rank-e2e: no failure in tests/ladx (e2e)\n"))

    def test_e2e_rom_repro_skips_ks_on_the_cases_directory_holding_the_case(self):
        game = self.ladx(cases=["a"], combos=["sample-02"])
        rom = self.output("e2e-rom", "e2e-rom: KS equal, 2 spans, 4 bytes, 0 not rebuilt\n"
                          + divergent("sample-02", "items: line 5:", "  golden: B 4000 (3 tokens)",
                                      "  c: B 4000 (2 tokens)") + "e2e-rom: 0 pass, 1 divergent\n")
        code, out = self.rank(game, rom)
        self.assertIn("REPRO python3 -I -B tests/ladx/run_e2e.py --rom tmp/ladx/base.gbc --no-ks "
                      "--cases tests/ladx/combos --gen tests/ladx/build/ladx_gen --tables data/ladx sample-02\n", out)

    def test_usage_errors_exit_2(self):
        self.ladx()
        self.assertEqual(self.rank("tests/ladx")[0], 2)
        self.assertEqual(self.rank("tests/other", self.output("e2e", ""))[0], 2)
        self.assertEqual(self.rank("tests/ladx", "missing.txt")[0], 2)

    def test_the_repository_order_txt_gives_generate_roms_group_order(self):
        out = self.output("e2e", divergent("x", *detail("header", 1)) + divergent("y", *detail("fixed", 1))
                          + divergent("z", "spoiler: c wrote no spoiler.json")
                          + divergent("w", *detail("save", 1)) + "e2e: 0 pass, 4 divergent\n")
        code, text = self.rank(str(REPO / "tests" / "ladx"), out)
        self.assertEqual([line.split(" ")[1].rstrip(":") for line in self.order_of(text)], ["y", "w", "z", "x"], text)

    def alttp(self, cases=(), combos=(), logs=None):
        game = Path(self.root) / "tests" / "alttp"
        game.mkdir(parents=True)
        (game / "write_groups.txt").write_text("# groups\ngroup items writeItem\ngroup settings *\n"
                                               "group text writeText\n")
        for sub, names in (("cases", cases), ("combos", combos)):
            for name in names:
                (game / sub / name).mkdir(parents=True)
                log = (logs or {}).get(name)
                if log is not None:
                    (game / sub / name / "rom_writes.txt").write_text(log)
        return "tests/alttp"

    def test_alttp_seed_failures_rank_by_attempt_then_draw_before_write_logs(self):
        game = self.alttp(cases=["a", "b", "c", "d"])
        e2e = self.output("e2e", divergent("a", "rom_writes.txt: items: first differing write 2:",
                                           "  golden: 00EA82 3E writeItem", "  c: 00EA82 3F writeItem")
                          + divergent("b", "first differing draw:", "  golden: 1 3 x 0 6 -> 3", "  c: 1 3 x 0 6 -> 4")
                          + divergent("c", "first differing draw:", "  golden: 0 9 x 0 6 -> 3", "  c: 0 9 x 0 6 -> 4")
                          + divergent("d", "spoiler.json: first differing line 4:", "  golden: a", "  c: b")
                          + "e2e: 0 pass, 4 divergent, 0 pending\n")
        code, out = self.rank(game, e2e)
        self.assertEqual(self.order_of(out), ["e2e c: seed draw 0 9", "e2e b: seed draw 1 3",
                                              "e2e d: seed spoiler.json: first differing line 4",
                                              "e2e a: write log items write 2"], out)
        self.assertIn("REPRO tests/alttp/run_e2e.sh c\n", out)

    def test_alttp_write_logs_rank_by_group_order_and_short_logs_at_the_goldens_next_write(self):
        logs = {"a": "000001 01 writeItem\n000002 02 writeText\n", "b": "000001 01 setMisc\n"}
        game = self.alttp(combos=["a", "b", "c"], logs=logs)
        combos = self.output("e2e-combos", "e2e-combos: the combination cases\n"
                             + divergent("a", "rom_writes.txt: c ends after 1 writes, claim needs 2")
                             + divergent("b", "c wrote no rom_writes.txt")
                             + divergent("c", "rom_writes.txt: items: first differing write 7:",
                                         "  golden: 000009 01 writeItem", "  c: <end>")
                             + "e2e: 0 pass, 3 divergent, 0 pending\n")
        code, out = self.rank(game, combos)
        self.assertEqual(self.order_of(out), ["e2e-combos c: write log items write 7",
                                              "e2e-combos b: write log settings write 1",
                                              "e2e-combos a: write log text write 2"], out)
        self.assertIn("REPRO RANDO_CASES=tests/alttp/combos RANDO_CLAIMS=tests/alttp/combo_claims.txt "
                      "tests/alttp/run_e2e.sh c\n", out)


if __name__ == "__main__":
    unittest.main()
