import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ladx_cases  # noqa: E402
import ladx_oracle  # noqa: E402

SETTINGS = """# stand-in settings.txt
setting seed\tMain\t<\tstr\t""\tSeed\t0\t-
setting logic\tMain\tL\tstr\t""\tLogic\t0\t-
option logic\tcasual\tc\tCasual\t-
option logic\t""\t""\tNormal\t-
option logic\thell\tH\tHell\t-
setting accessibility\tMain\tA\tstr\tall\tAccessibility\t0\t-
option accessibility\tall\ta\t100%\t-
option accessibility\tgoal\tg\tBeatable\t-
setting race\tMain\tV\tbool\t0\tRace mode\t0\t-
setting dungeon_keys\tItems\td\tstr\t""\tKeys\t0\t-
option dungeon_keys\t""\t""\tStandard\t-
option dungeon_keys\tkeysanity\tk\tKeysanity\t-
setting goal\tGameplay\tG\tstr\tvanilla\tGoal\t0\t-
option goal\tvanilla\tv\tVanilla\t-
option goal\topen-4\t<\tShort\t-
option goal\t5-8\t>\tLong\t-
option goal\tmaze\tm\tMaze\t-
setting music\tUser\tM\tstr\t""\tMusic\t1\t-
option music\t""\t""\tDefault\t-
option music\trandom\tr\tRandom\t-
"""

VALIDATE = """# stand-in settings_validate.txt
# keys accessibility goal
validate accessibility=all -> =\t
validate accessibility=goal -> =\tAg
validate goal=vanilla -> =\t
validate goal=open-4 -> =\tG<
validate goal=5-8 -> =\tG>
validate goal=maze -> =\tGm
validate accessibility=all,goal=vanilla -> =\t
validate accessibility=all,goal=open-4 -> =\tG<
validate accessibility=all,goal=5-8 -> =\tG>
validate accessibility=all,goal=maze -> =\tGm
validate accessibility=goal,goal=vanilla -> =\tAg
validate accessibility=goal,goal=open-4 -> =\tAgG<
validate accessibility=goal,goal=5-8 -> =\tAgG>
validate accessibility=goal,goal=maze -> accessibility=all\tGm
"""

TIER1 = """# stand-in tier1.txt
logic all
accessibility all
race all
dungeon_keys all
goal vanilla open-4 5-8 maze
music "" random
"""

SPEC = """# stand-in combos.txt
cross logic dungeon_keys
cross goal accessibility
case dungeon_keys=keysanity race=1 logic=hell
"""


def expected(tier1=TIER1, spec=SPEC, settings=SETTINGS, validate=VALIDATE):
    return ladx_cases.expected_list(tier1, settings, validate, spec)


class DeriveTest(unittest.TestCase):
    def test_fixed_vectors(self):
        self.assertEqual(ladx_cases.derive("baseline", 0).hex(), "3ad484d5f6d7795ad8893b6c536da4ad")
        self.assertEqual(ladx_cases.derive("baseline", 1).hex(), "e8515e7064fb5fd6458e72d56122673b")
        name = "logic-hell+dungeon_keys-keysanity"
        self.assertEqual(ladx_cases.derive(name, 0).hex(), "0dd7bf2fed653fb0eb50bc08881a0080")
        self.assertEqual(ladx_cases.derive(name, 1).hex(), "5edccd2fa62e08de4e75d77eae22b9ff")


class ExpansionTest(unittest.TestCase):
    def test_singles_are_the_baseline_then_every_non_default_value(self):
        self.assertEqual(expected().cases, [
            ("baseline", []),
            ("logic-casual", ["logic=casual"]),
            ("logic-hell", ["logic=hell"]),
            ("accessibility-goal", ["accessibility=goal"]),
            ("race-1", ["race=1"]),
            ("dungeon_keys-keysanity", ["dungeon_keys=keysanity"]),
            ("goal-open-4", ["goal=open-4"]),
            ("goal-5-8", ["goal=5-8"]),
            ("goal-maze", ["goal=maze"]),
            ("music-random", ["music=random"]),
        ])

    def test_cross_skips_default_cells_and_validate_rewrites_in_ladxr_order(self):
        e = expected()
        self.assertEqual(e.combos, [
            ("logic-casual+dungeon_keys-keysanity", ["logic=casual", "dungeon_keys=keysanity"]),
            ("logic-hell+dungeon_keys-keysanity", ["logic=hell", "dungeon_keys=keysanity"]),
            ("accessibility-goal+goal-open-4", ["accessibility=goal", "goal=open-4"]),
            ("accessibility-goal+goal-5-8", ["accessibility=goal", "goal=5-8"]),
            ("logic-hell+race-1+dungeon_keys-keysanity",
             ["logic=hell", "race=1", "dungeon_keys=keysanity"]),
        ])
        self.assertEqual(e.skipped, [("accessibility-goal+goal-maze", ["accessibility=all"])])

    def test_a_single_rewritten_by_validate_is_refused(self):
        validate = VALIDATE.replace("validate goal=maze -> =", "validate goal=maze -> goal=vanilla")
        with self.assertRaisesRegex(ladx_cases.CasesError, "goal-maze"):
            expected(validate=validate)

    def test_bad_spec_lines_are_refused(self):
        for line in ("case logic=hell logic=casual",            # a repeated key
                     "case dungeon_keys=keysanity logic=hell",  # a cross cell again
                     "case logic=hell",                         # one assignment
                     "case logic=hell race=0",                  # a default value
                     "case logic=hard race=1",                  # not a tier-1 value
                     "case logic=hell nope=1",                  # no such setting
                     "cross logic logic",
                     "cross logic nope",
                     "cross logic dungeon_keys",                # the same cross twice
                     "pair logic race"):
            with self.assertRaises(ladx_cases.CasesError, msg=line):
                expected(spec=SPEC + line + "\n")

    def test_a_case_line_rewritten_by_validate_is_refused(self):
        spec = "case accessibility=goal goal=maze race=1\n"
        with self.assertRaisesRegex(ladx_cases.CasesError, "validate"):
            expected(spec=spec)


class RefusedTest(unittest.TestCase):
    SPEC = SPEC + "refused music=random\ncase race=1 music=random\n"

    def test_a_refused_single_leaves_the_cases_and_is_covered_by_a_combo(self):
        e = expected(spec=self.SPEC)
        self.assertNotIn("music-random", [n for n, _ in e.cases])
        self.assertIn(("race-1+music-random", ["race=1", "music=random"]), e.combos)
        self.assertEqual(e.refused, [("music-random", ["music=random"], ["race-1+music-random"])])

    def test_bad_refused_lines(self):
        for spec in (SPEC + "refused music=random\n",                  # covered by no combo
                     self.SPEC + "refused music=random\n",             # twice
                     SPEC + "refused race=0\ncase race=0 logic=hell\n",  # a default value
                     SPEC + "refused logic=hell race=1\n",             # two assignments
                     SPEC + "refused dungeon_keys=keysanity\n"):       # a cross cell's single
            with self.assertRaises(ladx_cases.CasesError, msg=spec):
                expected(spec=spec)

    def test_the_check_reports_the_refused_single(self):
        root = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(root))
        e = expected(spec=self.SPEC)
        plant(root, e)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(ladx_cases.report(e, root), 0)
        self.assertEqual(out.getvalue().splitlines()[-2:], [
            "refused music-random: LADXR refuses every seed, covered by race-1+music-random",
            "ladx-cases: 9 cases, 6 combos, 1 cells skipped by validate, 1 singles refused by "
            "LADXR, every tier-1 value and cross cell covered"])
        ladx_cases.write_case(os.path.join(root, "cases", "music-random"), ["music=random"],
                              ladx_cases.derive("music-random", 0))
        self.assertEqual(ladx_cases.mismatches(e, root), ["EXTRA cases/music-random"])

def plant(root, e):
    for sub, entries in (("cases", e.cases), ("combos", e.combos)):
        for k, (name, assignments) in enumerate(entries):
            ladx_cases.write_case(os.path.join(root, sub, name), assignments,
                                  ladx_cases.derive(name, k % 3))


def write(path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


class CheckTest(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(self.root))
        self.e = expected()
        plant(self.root, self.e)

    def run_check(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = ladx_cases.report(self.e, self.root)
        return status, out.getvalue().splitlines()

    def assert_lines(self, lines):
        status, out = self.run_check()
        self.assertEqual((status, out), (1, lines))

    def test_a_correct_tree_gives_the_skips_and_the_summary(self):
        self.assertEqual(self.run_check(), (0, [
            "skip accessibility-goal+goal-maze: validate sets accessibility=all",
            "ladx-cases: 10 cases, 5 combos, 1 cells skipped by validate, every tier-1 value and "
            "cross cell covered"]))

    def test_the_baseline_config_is_empty(self):
        with open(os.path.join(self.root, "cases", "baseline", "config.txt"), "rb") as f:
            self.assertEqual(f.read(), b"")
        with open(os.path.join(self.root, "combos", "logic-hell+dungeon_keys-keysanity",
                               "config.txt"), "rb") as f:
            self.assertEqual(f.read(), b"logic=hell\ndungeon_keys=keysanity\n")

    def test_missing_and_extra(self):
        __import__("shutil").rmtree(os.path.join(self.root, "cases", "goal-5-8"))
        ladx_cases.write_case(os.path.join(self.root, "combos", "race-1+goal-maze"),
                              ["race=1", "goal=maze"], ladx_cases.derive("race-1+goal-maze", 0))
        self.assert_lines(["MISSING cases/goal-5-8", "EXTRA combos/race-1+goal-maze"])

    def test_wrong_config(self):
        write(os.path.join(self.root, "combos", "logic-hell+dungeon_keys-keysanity", "config.txt"),
              "dungeon_keys=keysanity\nlogic=hell\n")
        write(os.path.join(self.root, "cases", "race-1", "config.txt"), "race=0\n")
        self.assert_lines([
            "WRONG cases/race-1: config.txt is 'race=0\\n', expected 'race=1\\n'",
            "WRONG combos/logic-hell+dungeon_keys-keysanity: config.txt is "
            "'dungeon_keys=keysanity\\nlogic=hell\\n', expected "
            "'logic=hell\\ndungeon_keys=keysanity\\n'"])

    def test_bad_seeds(self):
        write(os.path.join(self.root, "cases", "race-1", "seed"),
              ladx_cases.derive("race-1", 64 - 1).hex().upper() + "\n")
        write(os.path.join(self.root, "cases", "goal-maze", "seed"),
              ladx_cases.derive("goal-5-8", 0).hex() + "\n")
        os.remove(os.path.join(self.root, "cases", "baseline", "seed"))
        self.assert_lines([
            "BADSEED cases/baseline: no seed file",
            "BADSEED cases/race-1: not 32 lowercase hex digits and a newline",
            "BADSEED cases/goal-maze: not derive(goal-maze, k) for any k < 64"])

    def test_stray_files_and_a_name_in_both(self):
        write(os.path.join(self.root, "cases", "race-1", "notes.txt"), "x\n")
        write(os.path.join(self.root, "combos", "README"), "x\n")
        ladx_cases.write_case(os.path.join(self.root, "combos", "goal-maze"), ["goal=maze"],
                              ladx_cases.derive("goal-maze", 0))
        self.assert_lines(["STRAY cases/race-1/notes.txt", "STRAY combos/README",
                           "BOTH goal-maze"])

    def test_listing_is_what_ladx_oracle_cases_prints(self):
        lines = ladx_cases.listing(self.e, self.root)
        self.assertEqual(lines[0], "baseline " + ladx_cases.derive("baseline", 0).hex())
        self.assertEqual(lines[-1], "logic-hell+race-1+dungeon_keys-keysanity %s logic=hell "
                         "race=1 dungeon_keys=keysanity"
                         % ladx_cases.derive("logic-hell+race-1+dungeon_keys-keysanity", 1).hex())
        self.assertEqual(len(lines), 15)
        path = os.path.join(self.root, "list.txt")
        write(path, "\n".join(lines) + "\n")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(ladx_oracle.main(["cases", path]), 0)
        self.assertEqual(out.getvalue().splitlines(), lines)

    def test_listing_named_cases_in_list_order(self):
        self.assertEqual([line.split()[0] for line in ladx_cases.listing(
            self.e, self.root, ["logic-hell+dungeon_keys-keysanity", "race-1"])],
            ["race-1", "logic-hell+dungeon_keys-keysanity"])
        with self.assertRaises(ladx_cases.CasesError):
            ladx_cases.listing(self.e, self.root, ["race-0"])

    def test_no_directories_is_every_case_missing(self):
        __import__("shutil").rmtree(os.path.join(self.root, "cases"))
        __import__("shutil").rmtree(os.path.join(self.root, "combos"))
        status, out = self.run_check()
        self.assertEqual((status, len(out)), (1, 15))
        self.assertEqual(out[0], "MISSING cases/baseline")


LOG = "ladx-rom-writes 1\ntables texts\nG fixed\nB 10 =2 00\n"


def plant_goldens(root, e, skip=()):
    """Every case folder's golden files (a spoiler unless race=1), a small valid write log."""
    for sub, entries in (("cases", e.cases), ("combos", e.combos)):
        for name, assignments in entries:
            if name in skip:
                continue
            path = os.path.join(root, sub, name)
            write(os.path.join(path, "run.txt"), "seed 00\noutcome ok\n")
            write(os.path.join(path, "rom_writes.txt"), LOG)
            if "race=1" not in assignments:
                write(os.path.join(path, "spoiler.json"), "{}\n")


class GoldenTest(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(self.root))
        self.e = expected()
        plant(self.root, self.e)
        plant_goldens(self.root, self.e)

    def at(self, *parts):
        return os.path.join(self.root, *parts)

    def check(self, limits=None):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = ladx_cases.report(self.e, self.root, goldens=True,
                                       limits=limits or ladx_cases.LIMITS)
        return status, out.getvalue().splitlines()

    def test_a_tree_with_every_golden_prints_the_totals_against_the_budget(self):
        status, out = self.check()
        size = 15 * len(LOG) + 15 * len("seed 00\noutcome ok\n") + 13 * len("{}\n")
        self.assertEqual(status, 0)
        self.assertEqual(out[-2], "ladx-cases: goldens in 15 folders, rom_writes.txt %d bytes "
                         "(limits %d per case, %d in all), golden files %d bytes (limit %d)"
                         % (15 * len(LOG), 224 * 1024, 22 * 1024 * 1024, size,
                            34 * 1024 * 1024))

    def test_a_missing_golden_file_and_a_spoiler_on_a_race_case(self):
        os.remove(self.at("cases", "baseline", "rom_writes.txt"))
        os.remove(self.at("cases", "goal-maze", "spoiler.json"))
        write(self.at("cases", "race-1", "spoiler.json"), "{}\n")
        status, out = self.check()
        self.assertEqual((status, out), (1, [
            "NOGOLDEN cases/baseline: rom_writes.txt missing",
            "BADGOLDEN cases/race-1: spoiler.json on a race case",
            "NOGOLDEN cases/goal-maze: spoiler.json missing"]))

    def test_a_malformed_write_log_or_one_without_its_version_line(self):
        write(self.at("cases", "baseline", "rom_writes.txt"), LOG.replace("=2", "=0x2"))
        write(self.at("cases", "race-1", "rom_writes.txt"), LOG.split("\n", 1)[1])
        status, out = self.check()
        self.assertEqual(status, 1)
        self.assertEqual([line.split(":")[0] for line in out],
                         ["BADGOLDEN cases/baseline", "BADGOLDEN cases/race-1"])
        self.assertIn("rom_writes.txt: line 1: not `ladx-rom-writes 1`", out[1])

    def test_the_budget_per_case_and_in_all(self):
        limits = {"rom_writes_case": len(LOG) - 1, "rom_writes_all": 15 * len(LOG) - 1,
                  "all": 10}
        status, out = self.check(limits=limits)
        self.assertEqual(status, 1)
        self.assertEqual(out[0], "BUDGET cases/baseline: rom_writes.txt %d bytes, limit %d"
                         % (len(LOG), len(LOG) - 1))
        self.assertEqual(len([line for line in out if line.startswith(("BUDGET cases/", "BUDGET combos/"))]),
                         15)
        self.assertEqual(out[-2:], [
            "BUDGET rom_writes.txt: %d bytes in all, limit %d" % (15 * len(LOG),
                                                                 15 * len(LOG) - 1),
            "BUDGET golden files: %d bytes in all, limit 10"
            % (15 * len(LOG) + 15 * len("seed 00\noutcome ok\n") + 13 * len("{}\n"))])

    def test_the_golden_files_are_not_stray(self):
        self.assertEqual(ladx_cases.mismatches(self.e, self.root), [])


class CommandTest(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(self.root))
        self.inputs = os.path.join(self.root, "inputs")
        os.makedirs(self.inputs)
        for name, text in (("tier1.txt", TIER1), ("settings.txt", SETTINGS),
                           ("settings_validate.txt", VALIDATE), ("combos.txt", SPEC)):
            write(os.path.join(self.inputs, name), text)

    def main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = ladx_cases.main(list(args) + ["--root", self.root, "--inputs", self.inputs])
        return status, out.getvalue().splitlines(), err.getvalue()

    def test_check_proves_itself_then_checks_the_tree(self):
        plant(self.root, expected())
        plant_goldens(self.root, expected())
        status, out, _ = self.main("check")
        self.assertEqual(status, 0)
        self.assertTrue(out[-2].startswith("ladx-cases: goldens in 15 folders, rom_writes.txt"))
        self.assertTrue(out[-1].startswith("ladx-cases: 10 cases, 5 combos, 1 cells skipped"))
        __import__("shutil").rmtree(os.path.join(self.root, "combos"))
        status, out, _ = self.main("check")
        self.assertEqual((status, len(out)), (1, 5))

    def test_check_requires_the_goldens(self):
        plant(self.root, expected())
        plant_goldens(self.root, expected(), skip=("race-1",))
        status, out, _ = self.main("check")
        self.assertEqual((status, out), (1, ["NOGOLDEN cases/race-1: run.txt missing",
                                             "NOGOLDEN cases/race-1: rom_writes.txt missing"]))

    def test_a_blind_golden_checker_fails_its_self_proof(self):
        plant(self.root, expected())
        plant_goldens(self.root, expected())
        real = ladx_cases.golden_lines
        ladx_cases.golden_lines = lambda *a, **k: ([], "")
        try:
            status, out, err = self.main("check")
        finally:
            ladx_cases.golden_lines = real
        self.assertEqual(status, 1)
        self.assertIn("ladx-cases: self-test failed: missing golden", err)

    def test_a_blind_checker_fails_its_self_proof(self):
        plant(self.root, expected())
        real = ladx_cases.mismatches
        ladx_cases.mismatches = lambda e, root: []
        try:
            status, out, err = self.main("check")
        finally:
            ladx_cases.mismatches = real
        self.assertEqual(status, 1)
        self.assertIn("ladx-cases: self-test failed: missing", err)

    def test_malformed_input_exits_2(self):
        write(os.path.join(self.inputs, "combos.txt"), "cross logic logic\n")
        status, out, err = self.main("check")
        self.assertEqual((status, out), (2, []))
        self.assertIn("combos.txt", err)

    def test_list_refuses_a_tree_that_does_not_check(self):
        plant(self.root, expected())
        status, out, _ = self.main("list", "race-1")
        self.assertEqual((status, out), (0, ["race-1 %s race=1" % ladx_cases.derive("race-1", 1).hex()]))
        os.remove(os.path.join(self.root, "cases", "race-1", "seed"))
        status, out, err = self.main("list")
        self.assertEqual((status, out), (1, []))
        self.assertIn("BADSEED cases/race-1: no seed file", err)
        status, out, err = self.main("list", "nope")
        self.assertEqual(status, 2)


class Candidates(unittest.TestCase):
    """Stand-ins for the Nintendo-bytes test and the oracle run of the picker's candidate loop."""

    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(self.root))
        self.path = os.path.join(self.root, "combos", "goal-maze+race-1")
        self.name = "goal-maze+race-1"

    def stand_ins(self, bad_text, outcomes):
        """nintendo_ok refuses the seed texts of the candidates in bad_text; run gives outcomes
        by candidate (ok past the end) and records the candidates it ran."""
        ran = []
        bad = {ladx_cases.derive(self.name, k).hex() + "\n" for k in bad_text}

        def nintendo_ok(text):
            return text not in bad

        def run(seed):
            k = next(k for k in range(ladx_cases.BOUND) if ladx_cases.derive(self.name, k) == seed)
            ran.append(k)
            return outcomes[k] if k < len(outcomes) else "ok"
        return nintendo_ok, run, ran


class PickTest(Candidates):
    def test_picks_the_first_acceptable_candidate_and_records_refusals_in_order(self):
        nintendo_ok, run, ran = self.stand_ins([1], ["error Item placement failed", "ok",
                                                     "crash IndexError list index out of range"])
        k, refusals = ladx_cases.pick_case(self.path, self.name, ["goal=maze", "race=1"],
                                           nintendo_ok, run)
        self.assertEqual(k, 3)
        self.assertEqual(refusals, [(0, "error Item placement failed"), (1, "nintendo-bytes"),
                                    (2, "crash IndexError list index out of range")])
        self.assertEqual(ran, [0, 2, 3])
        with open(os.path.join(self.path, "config.txt")) as f:
            self.assertEqual(f.read(), "goal=maze\nrace=1\n")
        with open(os.path.join(self.path, "seed")) as f:
            self.assertEqual(f.read(), ladx_cases.derive(self.name, 3).hex() + "\n")

    def test_writes_nothing_when_every_candidate_is_refused(self):
        nintendo_ok, run, _ = self.stand_ins([], ["error x"] * ladx_cases.BOUND)
        k, refusals = ladx_cases.pick_case(self.path, self.name, ["race=1"], nintendo_ok, run)
        self.assertEqual((k, len(refusals)), (None, ladx_cases.BOUND))
        self.assertFalse(os.path.exists(self.path))

    def test_refuses_an_existing_directory(self):
        os.makedirs(self.path)
        nintendo_ok, run, ran = self.stand_ins([], [])
        with self.assertRaises(ladx_cases.CasesError):
            ladx_cases.pick_case(self.path, self.name, ["race=1"], nintendo_ok, run)
        self.assertEqual((ran, os.listdir(self.path)), ([], []))

    def test_refusal_kinds(self):
        self.assertEqual([ladx_cases.refusal_kind(r) for r in (
            "nintendo-bytes", "error Item placement failed", "crash IndexError list index",
            "crash UnicodeEncodeError 'x' codec")],
            ["nintendo-bytes", "error", "crash IndexError", "crash UnicodeEncodeError"])


class VerifyTest(Candidates):
    def plant(self, k):
        ladx_cases.write_case(self.path, ["race=1"], ladx_cases.derive(self.name, k))

    def test_a_first_ok_seed_is_verified(self):
        self.plant(2)
        nintendo_ok, run, ran = self.stand_ins([0], ["ok", "crash IndexError x"])
        self.assertEqual(ladx_cases.verify_case(self.path, self.name, nintendo_ok, run), (2, []))
        self.assertEqual(ran, [1, 2])

    def test_an_earlier_acceptable_candidate_or_a_refused_seed_is_a_problem(self):
        self.plant(1)
        nintendo_ok, run, _ = self.stand_ins([], ["ok", "error x"])
        self.assertEqual(ladx_cases.verify_case(self.path, self.name, nintendo_ok, run),
                         (1, ["candidate 0 is acceptable", "candidate 1 is refused: error x"]))


class VerifyRefusedTest(unittest.TestCase):
    def test_every_candidate_must_be_not_ok(self):
        name = "boomerang-default"
        outcomes = {ladx_cases.derive(name, k): "crash AssertionError x"
                    for k in range(ladx_cases.BOUND)}
        self.assertEqual(ladx_cases.verify_refused(name, outcomes.get),
                         ({"crash AssertionError": ladx_cases.BOUND}, []))
        outcomes[ladx_cases.derive(name, 5)] = "ok"
        counts, problems = ladx_cases.verify_refused(name, outcomes.get)
        self.assertEqual(problems, ["candidate 5 is ok"])

if __name__ == "__main__":
    unittest.main()
