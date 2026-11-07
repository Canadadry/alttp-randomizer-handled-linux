import hashlib
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gen_fill_outcomes as g  # noqa: E402
import ladx_cases  # noqa: E402
import ladx_oracle  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))

RUN = [
    "seed 00ff",
    "settings \"\"",
    "ws goal 8",
    "words_before 4",
    "pool SWORD=1",
    "words_after 9",
    "placer_pool SWORD=1",
    "place 0 Chest 0x001 SWORD",
    "trace 0 rnd randomizer:Randomizer.__init__ seed 00ff -> - [0]",
    "trace 1 rnd randomizer:RandomItemPlacer.run choice 3 -> 1 [1]",
    "trace 2 rnd generator:generateRom randint 0 9 -> 4 [1]",
    "outcome ok",
]


class FixtureLine(unittest.TestCase):
    def test_ok_run_is_its_fill_projection_without_the_outcome(self):
        text = "".join(line + "\n" for line in RUN[:10])
        line = g.fixture_line("baseline", 3, b"\x00\xff", ["race=1"], RUN)
        self.assertEqual(line, "baseline 3 00ff ok 10 %s race=1"
                         % hashlib.sha512(text.encode()).hexdigest()[:32])

    def test_error_run_keeps_its_outcome_line(self):
        run = RUN[:7] + RUN[8:10] + ["outcome error Failed to place an item for a bunch of retries"]
        text = "".join(line + "\n" for line in run)
        line = g.fixture_line("det-pain", None, b"\x01", [], run)
        self.assertEqual(line, "det-pain - 01 error 10 " + hashlib.sha512(text.encode()).hexdigest()[:32])

    def test_crash_kind(self):
        run = RUN[:3] + ["outcome crash IndexError Cannot choose from an empty sequence"]
        self.assertTrue(g.fixture_line("goal-maze", 0, b"\x02", ["goal=maze"], run).split(" ")[3] == "crash")

    def test_a_run_without_an_outcome_is_refused(self):
        with self.assertRaises(ladx_oracle.OracleError):
            g.fixture_line("baseline", 0, b"\x00", [], RUN[:-1])


class Compare(unittest.TestCase):
    def test_identical(self):
        self.assertEqual(g.compare("# h\na 0 x\n", "# h\na 0 x\n"), [])

    def test_a_differing_line_is_named_by_label_and_k(self):
        lines = g.compare("# h\na 0 x ok\nb 1 y ok\n", "# h\na 0 x ok\nb 1 y error\n")
        self.assertEqual(len(lines), 1)
        self.assertTrue(lines[0].startswith("DIFFERENT line 3 (b 1): "), lines[0])

    def test_a_missing_final_newline_differs(self):
        self.assertNotEqual(g.compare("a 0 x\n", "a 0 x"), [])


class CommittedFixture(unittest.TestCase):
    """The committed fixture runs exactly the plan the case tree and the inputs give."""

    def test_rows_follow_the_plan(self):
        with open(g.FIXTURE, encoding="utf-8") as f:
            rows = [line.split(" ") for line in f.read().splitlines() if not line.startswith("#")]
        plan = g.runs(os.path.dirname(HERE), HERE)
        self.assertEqual([(r[0], r[1], r[2], r[6:]) for r in rows],
                         [(label, "-" if k is None else str(k), seed.hex(), a) for label, k, seed, a in plan])
        for r in rows:
            self.assertIn(r[3], ("ok", "error", "crash"))
            self.assertRegex(r[5], "^[0-9a-f]{32}$")
            self.assertGreater(int(r[4]), 0)

    def test_plan_holds_every_refused_candidate_and_no_case_seed(self):
        plan = g.runs(os.path.dirname(HERE), HERE)
        labels = [label for label, _, _, _ in plan]
        self.assertEqual(labels.count("boomerang-default"), g.REFUSED_CANDIDATES)
        self.assertNotIn("det-bingo", labels)
        self.assertIn("det-pain", labels)
        for sub in ("cases", "combos"):
            for name in os.listdir(os.path.join(os.path.dirname(HERE), sub)):
                with open(os.path.join(os.path.dirname(HERE), sub, name, "seed"), encoding="utf-8") as f:
                    k = ladx_cases.seed_index(name, f.read())
                self.assertEqual(sorted(j for label, j, _, _ in plan if label == name), list(range(k)))


if __name__ == "__main__":
    unittest.main()
