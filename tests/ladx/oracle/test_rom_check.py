
import contextlib
import io
import os
import shutil
import sys
import tempfile
import time
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)

import rom_check  # noqa: E402


def run_main(argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        status = rom_check.main(argv)
    return status, out.getvalue(), err.getvalue()


def write_script(path, body):
    with open(path, "w", encoding="utf-8") as f:
        f.write("#!%s -IS\n%s" % (sys.executable, body))
    os.chmod(path, 0o755)


class SelfTest(unittest.TestCase):
    def test_every_planted_run_as_expected(self):
        started = time.monotonic()
        failures, runs = rom_check.self_test()
        self.assertEqual(failures, [])
        self.assertGreaterEqual(runs, 10)
        self.assertLess(time.monotonic() - started, 2.0)

    def test_self_test_flag_prints_one_line(self):
        status, out, err = run_main(["--self-test"])
        self.assertEqual(status, 0, err)
        self.assertRegex(out, r"^rom-check: self-test, \d+ planted runs as expected\n$")
        self.assertEqual(err, "")


class CommandLine(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.rom = os.path.join(self.dir, "base.gbc")
        with open(self.rom, "wb") as f:
            f.write(bytes(range(256)) * 4)
        self.work = os.path.join(TOP, "tmp", "test-rom-check-%d" % os.getpid())
        stub = mock.patch.object(rom_check, "self_test", return_value=([], 17))
        stub.start()
        self.addCleanup(stub.stop)

    def tearDown(self):
        shutil.rmtree(self.dir)
        shutil.rmtree(self.work, ignore_errors=True)

    def test_unknown_case_is_a_usage_error(self):
        gen = os.path.join(self.dir, "gen")
        write_script(gen, "import sys\nsys.exit(3)\n")
        status, out, err = run_main(["--rom", self.rom, "--gen", gen, "--work", self.work,
                                     "no-such-case"])
        self.assertEqual(status, 2)
        self.assertIn("no case no-such-case", err)
        self.assertFalse(os.path.exists(self.work))

    def test_work_outside_tmp_is_refused(self):
        gen = os.path.join(self.dir, "gen")
        write_script(gen, "import sys\nsys.exit(3)\n")
        outside = os.path.join(self.dir, "work")
        status, out, err = run_main(["--rom", self.rom, "--gen", gen, "--work", outside,
                                     "baseline"])
        self.assertEqual(status, 2)
        self.assertIn("outside", err)
        self.assertFalse(os.path.exists(outside))

    def test_missing_generator_is_a_setup_line(self):
        gen = os.path.join(self.dir, "ladx_patch")
        status, out, err = run_main(["--rom", self.rom, "--gen", gen, "--work", self.work])
        self.assertEqual(status, 2)
        self.assertIn("setup: rom-check: no generator at %s" % gen, err)

    def test_failed_self_test_stops_the_run(self):
        with mock.patch.object(rom_check, "self_test", return_value=(["summary"], 17)):
            status, out, err = run_main(["--rom", self.rom, "--gen", "/bin/true", "--work",
                                         self.work])
        self.assertEqual((status, out), (1, ""))
        self.assertEqual(err, "rom-check: self-test failed: summary\n")
        self.assertFalse(os.path.exists(self.work))

    def test_missing_rom_is_a_setup_line(self):
        missing = os.path.join(self.dir, "none.gbc")
        status, out, err = run_main(["--rom", missing, "--gen", "/bin/true", "--work",
                                     self.work])
        self.assertEqual(status, 2)
        self.assertIn("setup: rom-check: no ROM at %s" % missing, err)

    def test_real_case_with_a_refusing_generator(self):
        gen = os.path.join(self.dir, "gen")
        write_script(gen, "import sys\nprint(sys.argv[1:])\nsys.exit(3)\n")
        status, out, err = run_main(["--rom", self.rom, "--gen", gen, "--work", self.work,
                                     "baseline"])
        self.assertEqual(status, 1, err)
        lines = out.splitlines()
        self.assertRegex(lines[0], r"^rom-check: self-test, \d+ planted runs as expected$")
        self.assertRegex(lines[1], r"^rom-check: base [0-9a-f]{40} \(not the LADX ROM\), "
                         r"generator %s$" % gen)
        self.assertEqual(lines[2:], [
            "REFUSED baseline: base refused (exit 3)",
            "rom-check: 1 cases, 0 combos: 0 ok, 0 mismatched, 1 refused, 0 failed, "
            "0 no golden"])
        with open(os.path.join(self.work, "baseline", "gen.log"), encoding="utf-8") as f:
            logged = f.read()
        with open(os.path.join(TOP, "tests", "ladx", "cases", "baseline", "seed")) as f:
            seed = f.read().strip()
        self.assertIn(repr(["--base", os.path.abspath(self.rom), "--out",
                            os.path.join(os.path.abspath(self.work), "baseline", "out.gbc"),
                            "--seed", seed]), logged)


if __name__ == "__main__":
    unittest.main()
