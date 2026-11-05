"""Usage: python3 -I test_package.py

Self-test of app/package/: the zip builder on planted inputs, launch.sh on the host against a stub
alttpr, and config.json against the package. Paths are relative to this file's directory.
"""

import contextlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
PACKAGE = os.path.join(HERE, "..", "..", "app", "package")
DATA = os.path.join(HERE, "..", "..", "data")
BUILDER = os.path.join(PACKAGE, "build_zip.py")
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("build_zip", BUILDER)
build_zip = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(build_zip)

EXPECTED = [
    "App/",
    "App/ALttPR/",
    "App/ALttPR/DejaVuSansMono-LICENSE.txt",
    "App/ALttPR/DejaVuSansMono.ttf",
    "App/ALttPR/alttpr",
    "App/ALttPR/base.bps",
    "App/ALttPR/config.json",
    "App/ALttPR/icon.png",
    "App/ALttPR/launch.sh",
]
PACKAGE_FILES = ("launch.sh", "config.json", "icon.png")
DATA_FILES = ("base.bps", "DejaVuSansMono.ttf", "DejaVuSansMono-LICENSE.txt")


def write(path, data):
    with open(path, "wb") as f:
        f.write(data)


def read(path):
    with open(path, "rb") as f:
        return f.read()


def run_builder(*args):
    """build_zip's command line, run in-process: a spawned interpreter per call is too slow."""
    argv = [BUILDER] + list(args)
    err = io.StringIO()
    code = 0
    with contextlib.redirect_stderr(err):
        try:
            build_zip.main(argv)
        except SystemExit as e:
            code = e.code if isinstance(e.code, int) else 1
            if isinstance(e.code, str):
                print(e.code, file=sys.stderr)
    return subprocess.CompletedProcess(argv, code, "", err.getvalue())


class Builder(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp)
        self.binary = os.path.join(self.tmp, "alttpr")
        write(self.binary, b"\x7fELF fake binary")
        self.package = os.path.join(self.tmp, "package")
        self.data = os.path.join(self.tmp, "data")
        os.mkdir(self.package)
        os.mkdir(self.data)
        for d, names in ((self.package, PACKAGE_FILES), (self.data, DATA_FILES)):
            for name in names:
                write(os.path.join(d, name), ("fake %s\n" % name).encode())
        self.zip = os.path.join(self.tmp, "out", "ALttPR.zip")

    def run_builder(self, *extra, out=None):
        return run_builder(*extra, self.binary, self.package, self.data, out or self.zip)

    def build(self, out=None):
        r = self.run_builder(out=out)
        self.assertEqual(r.returncode, 0, r.stderr)
        return r

    def test_exact_entry_list_in_order(self):
        self.build()
        with zipfile.ZipFile(self.zip) as z:
            self.assertEqual(z.namelist(), EXPECTED)

    def test_files_are_byte_identical_copies(self):
        self.build()
        with zipfile.ZipFile(self.zip) as z:
            self.assertEqual(z.read("App/ALttPR/alttpr"), read(self.binary))
            for d, names in ((self.package, PACKAGE_FILES), (self.data, DATA_FILES)):
                for name in names:
                    self.assertEqual(z.read("App/ALttPR/" + name), read(os.path.join(d, name)), name)

    def test_modes_timestamps_and_compression(self):
        self.build()
        executable = ("App/ALttPR/alttpr", "App/ALttPR/launch.sh")
        with zipfile.ZipFile(self.zip) as z:
            for info in z.infolist():
                mode = info.external_attr >> 16
                if info.is_dir():
                    self.assertEqual(mode, 0o40755, info.filename)
                else:
                    want = 0o100755 if info.filename in executable else 0o100644
                    self.assertEqual(mode, want, info.filename)
                    self.assertEqual(info.compress_type, zipfile.ZIP_DEFLATED, info.filename)
                self.assertEqual(info.date_time, (1980, 1, 1, 0, 0, 0), info.filename)

    def test_two_runs_give_identical_bytes(self):
        other = os.path.join(self.tmp, "again.zip")
        self.build()
        os.utime(self.binary, (0, 0))
        self.build(out=other)
        self.assertEqual(read(self.zip), read(other))

    def test_missing_input_is_refused_naming_it(self):
        inputs = [self.binary] + [os.path.join(self.package, n) for n in PACKAGE_FILES]
        inputs += [os.path.join(self.data, n) for n in DATA_FILES]
        for path in inputs:
            with self.subTest(path=path):
                saved = read(path)
                os.remove(path)
                r = self.run_builder()
                write(path, saved)
                self.assertNotEqual(r.returncode, 0)
                self.assertIn(path, r.stderr)
                self.assertEqual(len(r.stderr.strip().splitlines()), 1, r.stderr)
                self.assertFalse(os.path.exists(self.zip))

    def test_check_accepts_the_built_zip(self):
        self.build()
        r = self.run_builder("--check")
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_check_refuses_an_extra_entry(self):
        self.build()
        with zipfile.ZipFile(self.zip, "a") as z:
            z.writestr("App/ALttPR/base.sfc", b"rom")
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("base.sfc", r.stderr)

    def test_check_refuses_a_missing_entry(self):
        self.build()
        names = [n for n in EXPECTED if n != "App/ALttPR/icon.png"]
        with zipfile.ZipFile(self.zip) as src:
            kept = [(src.getinfo(n), src.read(n)) for n in names]
        with zipfile.ZipFile(self.zip, "w") as z:
            for info, data in kept:
                z.writestr(info, data)
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)

    def test_check_refuses_a_changed_source(self):
        self.build()
        write(os.path.join(self.data, "base.bps"), b"another patch")
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("base.bps", r.stderr)

    def test_check_refuses_a_wrong_mode(self):
        self.build()
        with zipfile.ZipFile(self.zip) as src:
            kept = [(src.getinfo(n), src.read(n)) for n in EXPECTED]
        with zipfile.ZipFile(self.zip, "w") as z:
            for info, data in kept:
                if info.filename == "App/ALttPR/launch.sh":
                    info.external_attr = 0o100644 << 16
                z.writestr(info, data)
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("launch.sh", r.stderr)

    def test_wrong_arguments_print_the_usage(self):
        r = subprocess.run([sys.executable, "-I", BUILDER], capture_output=True, text=True)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("Usage:", r.stderr)


class RealPackage(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        binary = os.path.join(cls.tmp, "alttpr")
        write(binary, b"\x7fELF fake binary")
        out = os.path.join(cls.tmp, "ALttPR.zip")
        r = run_builder(binary, PACKAGE, DATA, out)
        if r.returncode != 0:
            shutil.rmtree(cls.tmp)
            raise AssertionError(r.stderr)
        with zipfile.ZipFile(out) as z:
            cls.entries = z.namelist()
            cls.config = json.loads(z.read("App/ALttPR/config.json"))
            cls.icon = z.read("App/ALttPR/icon.png")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp)

    def test_config_names_the_app_and_its_files(self):
        self.assertEqual(self.config["label"], "ALttPR")
        self.assertTrue(self.config["description"])
        self.assertIn("App/ALttPR/" + self.config["launch"], self.entries)
        icon = self.config["icon"]
        self.assertTrue(icon.startswith("/mnt/SDCARD/App/ALttPR/"), icon)
        self.assertIn("App/ALttPR/" + os.path.basename(icon), self.entries)

    def test_icon_is_a_png(self):
        self.assertEqual(self.icon[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(self.icon[12:16], b"IHDR")


STUB = b"""#!/bin/sh
pwd >started_in
echo "stub stderr" >&2
echo "stub stdout"
"""


class Launch(unittest.TestCase):
    def test_runs_alttpr_from_its_own_folder_with_stderr_in_a_log(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp)
        app = os.path.join(tmp, "sd card", "App", "ALttPR")
        elsewhere = os.path.join(tmp, "elsewhere")
        os.makedirs(app)
        os.mkdir(elsewhere)
        shutil.copy(os.path.join(PACKAGE, "launch.sh"), app)
        write(os.path.join(app, "alttpr"), STUB)
        os.chmod(os.path.join(app, "alttpr"), 0o755)
        r = subprocess.run(["sh", os.path.join(app, "launch.sh")], cwd=elsewhere,
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(read(os.path.join(app, "started_in")).decode().strip(), os.path.realpath(app))
        self.assertEqual(read(os.path.join(app, "alttpr.log")), b"stub stderr\n")
        self.assertEqual(r.stderr, "")
        self.assertEqual(os.listdir(elsewhere), [])

    def test_is_posix_sh_with_lf_line_endings(self):
        text = read(os.path.join(PACKAGE, "launch.sh"))
        self.assertTrue(text.startswith(b"#!/bin/sh\n"))
        self.assertNotIn(b"\r", text)


if __name__ == "__main__":
    unittest.main()
