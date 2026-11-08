"""Usage: python3 -I test_package.py

Self-test of app/package/: the zip builder on planted inputs, launch.sh on the host against a stub
zrando, and config.json against the package. Paths are relative to this file's directory.
"""

import contextlib
import importlib.util
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
PACKAGE = os.path.join(HERE, "..", "..", "app", "package")
DATA = os.path.join(HERE, "..", "..", "data")
ALTTP = os.path.join(HERE, "..", "..", "themes", "alttp")
MAKEFILE = os.path.join(HERE, "Makefile")
BUILDER = os.path.join(PACKAGE, "build_zip.py")
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("build_zip", BUILDER)
build_zip = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(build_zip)

THEME = "App/ZeldaRando/themes/default/"
TABLES = "App/ZeldaRando/ladx/tables/"
EXPECTED = [
    "App/",
    "App/ZeldaRando/",
    "App/ZeldaRando/alttp/",
    "App/ZeldaRando/alttp/base.bps",
    "App/ZeldaRando/config.json",
    "App/ZeldaRando/icon.png",
    "App/ZeldaRando/ladx/",
    TABLES,
    TABLES + "goal.txt",
    TABLES + "order.txt",
    "App/ZeldaRando/launch.sh",
    "App/ZeldaRando/themes/",
    THEME,
    THEME + "Mono-LICENSE.txt",
    THEME + "background.png",
    THEME + "mono.ttf",
    THEME + "theme.ini",
    "App/ZeldaRando/zrando",
]
PACKAGE_FILES = ("launch.sh", "config.json", "icon.png")
DATA_FILES = ("base.bps",)
THEME_FILES = ("theme.ini", "mono.ttf", "background.png", "Mono-LICENSE.txt")
TABLE_FILES = ("order.txt", "goal.txt")
SD_HEADER = os.path.join(HERE, "..", "..", "app", "sd", "rando_seed_save.h")


def write(path, data):
    with open(path, "wb") as f:
        f.write(data)


def read(path):
    with open(path, "rb") as f:
        return f.read()


def pkg_theme(makefile=MAKEFILE):
    """The theme folder make package ships: the Makefile's single `PKG_THEME := <path>`, resolved from HERE."""
    with open(makefile) as f:
        values = [line.split(":=", 1)[1].strip() for line in f
                  if line.split(":=", 1)[0].strip() == "PKG_THEME" and ":=" in line]
    if len(values) != 1:
        raise AssertionError("%s: %d PKG_THEME := lines, want 1" % (makefile, len(values)))
    return os.path.join(HERE, values[0])


def sd_path(name, header=SD_HEADER):
    """The string a `#define NAME "a" OTHER "b"` macro of the app's SD header expands to."""
    defines = {}
    with open(header) as f:
        for line in f:
            m = re.match(r'#define (\w+) (.*)$', line.strip())
            if m:
                defines[m.group(1)] = m.group(2)
    out = ""
    for token in re.findall(r'"[^"]*"|\w+', defines[name]):
        out += token[1:-1] if token.startswith('"') else sd_path(token, header)
    return out


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
        self.binary = os.path.join(self.tmp, "zrando")
        write(self.binary, b"\x7fELF fake binary")
        self.package = os.path.join(self.tmp, "package")
        self.data = os.path.join(self.tmp, "data")
        self.theme = os.path.join(self.tmp, "theme")
        os.mkdir(self.package)
        os.mkdir(self.data)
        os.mkdir(self.theme)
        self.tables = os.path.join(self.data, "ladx")
        os.mkdir(self.tables)
        for d, names in ((self.package, PACKAGE_FILES), (self.data, DATA_FILES), (self.theme, THEME_FILES),
                         (self.tables, TABLE_FILES)):
            for name in names:
                write(os.path.join(d, name), ("fake %s\n" % name).encode())
        self.zip = os.path.join(self.tmp, "out", "ZeldaRando.zip")

    def run_builder(self, *extra, out=None):
        return run_builder(*extra, self.binary, self.package, self.data, self.theme, out or self.zip)

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
            self.assertEqual(z.read("App/ZeldaRando/zrando"), read(self.binary))
            for name in PACKAGE_FILES:
                self.assertEqual(z.read("App/ZeldaRando/" + name), read(os.path.join(self.package, name)), name)
            for name in DATA_FILES:
                self.assertEqual(z.read("App/ZeldaRando/alttp/" + name), read(os.path.join(self.data, name)), name)
            for name in THEME_FILES:
                self.assertEqual(z.read(THEME + name), read(os.path.join(self.theme, name)), name)
            for name in TABLE_FILES:
                self.assertEqual(z.read(TABLES + name), read(os.path.join(self.tables, name)), name)

    def test_modes_timestamps_and_compression(self):
        self.build()
        executable = ("App/ZeldaRando/zrando", "App/ZeldaRando/launch.sh")
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
        inputs.append(os.path.join(self.theme, "theme.ini"))
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

    def test_a_deleted_theme_file_is_refused_by_check_naming_it(self):
        self.build()
        for name in THEME_FILES[1:]:
            with self.subTest(name=name):
                path = os.path.join(self.theme, name)
                saved = read(path)
                os.remove(path)
                r = self.run_builder("--check")
                write(path, saved)
                self.assertNotEqual(r.returncode, 0)
                self.assertIn(THEME + name, r.stderr)
                self.assertEqual(len(r.stderr.strip().splitlines()), 1, r.stderr)

    def assert_refused(self, offender):
        r = self.run_builder()
        self.assertNotEqual(r.returncode, 0)
        self.assertTrue(r.stderr.startswith("build_zip: "), r.stderr)
        self.assertIn(offender, r.stderr)
        self.assertEqual(len(r.stderr.strip().splitlines()), 1, r.stderr)
        self.assertFalse(os.path.exists(self.zip))

    def test_a_missing_theme_dir_is_refused(self):
        shutil.rmtree(self.theme)
        self.assert_refused(self.theme)

    def test_a_theme_dir_that_is_a_file_is_refused(self):
        shutil.rmtree(self.theme)
        write(self.theme, b"not a folder")
        self.assert_refused(self.theme)

    def test_a_theme_subdirectory_is_refused(self):
        os.mkdir(os.path.join(self.theme, "sprites"))
        self.assert_refused(os.path.join(self.theme, "sprites"))

    def test_a_theme_symlink_is_refused(self):
        os.symlink(os.path.join(self.theme, "mono.ttf"), os.path.join(self.theme, "link.ttf"))
        self.assert_refused(os.path.join(self.theme, "link.ttf"))

    def test_a_hidden_theme_file_is_refused(self):
        write(os.path.join(self.theme, ".DS_Store"), b"junk")
        self.assert_refused(os.path.join(self.theme, ".DS_Store"))

    def test_a_missing_tables_dir_is_refused(self):
        shutil.rmtree(self.tables)
        self.assert_refused(self.tables)

    def test_tables_without_order_txt_are_refused(self):
        os.remove(os.path.join(self.tables, "order.txt"))
        self.assert_refused(os.path.join(self.tables, "order.txt"))

    def test_a_tables_subdirectory_is_refused(self):
        os.mkdir(os.path.join(self.tables, "old"))
        self.assert_refused(os.path.join(self.tables, "old"))

    def test_a_tables_symlink_is_refused(self):
        os.symlink(os.path.join(self.tables, "goal.txt"), os.path.join(self.tables, "link.txt"))
        self.assert_refused(os.path.join(self.tables, "link.txt"))

    def test_a_hidden_tables_file_is_refused(self):
        write(os.path.join(self.tables, ".goal.txt.swp"), b"junk")
        self.assert_refused(os.path.join(self.tables, ".goal.txt.swp"))

    def test_a_tables_file_that_is_not_txt_is_refused(self):
        write(os.path.join(self.tables, "goal.txt.orig"), b"old table")
        self.assert_refused(os.path.join(self.tables, "goal.txt.orig"))

    def test_check_refuses_a_changed_table(self):
        self.build()
        write(os.path.join(self.tables, "goal.txt"), b"another table")
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(TABLES + "goal.txt", r.stderr)

    def test_check_refuses_a_new_table(self):
        self.build()
        write(os.path.join(self.tables, "music.txt"), b"a new group")
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(TABLES + "music.txt", r.stderr)

    def test_check_refuses_a_changed_theme_file(self):
        self.build()
        write(os.path.join(self.theme, "background.png"), b"another picture")
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(THEME + "background.png", r.stderr)

    def test_check_accepts_the_built_zip(self):
        self.build()
        r = self.run_builder("--check")
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_check_refuses_an_extra_entry(self):
        self.build()
        with zipfile.ZipFile(self.zip, "a") as z:
            z.writestr("App/ZeldaRando/alttp/base.sfc", b"rom")
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("base.sfc", r.stderr)

    def test_check_refuses_a_missing_entry(self):
        self.build()
        names = [n for n in EXPECTED if n != "App/ZeldaRando/icon.png"]
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
                if info.filename == "App/ZeldaRando/launch.sh":
                    info.external_attr = 0o100644 << 16
                z.writestr(info, data)
        r = self.run_builder("--check")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("launch.sh", r.stderr)

    def test_wrong_arguments_print_the_usage(self):
        r = subprocess.run([sys.executable, "-I", BUILDER], capture_output=True, text=True)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("Usage:", r.stderr)


class DefaultTheme(unittest.TestCase):

    def makefile(self, text):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp)
        path = os.path.join(tmp, "Makefile")
        write(path, text.encode())
        return path

    def test_the_package_ships_alttp(self):
        self.assertEqual(os.path.realpath(pkg_theme()), os.path.realpath(ALTTP))

    def test_a_missing_pkg_theme_line_is_refused_naming_the_makefile(self):
        path = self.makefile("PKG_BIN := x\n")
        with self.assertRaisesRegex(AssertionError, "^" + re.escape(path)):
            pkg_theme(path)

    def test_a_duplicated_pkg_theme_line_is_refused_naming_the_makefile(self):
        path = self.makefile("PKG_THEME := ../a\nPKG_THEME := ../b\n")
        with self.assertRaisesRegex(AssertionError, "^" + re.escape(path)):
            pkg_theme(path)


class RealPackage(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.default = pkg_theme()
        cls.tmp = tempfile.mkdtemp()
        binary = os.path.join(cls.tmp, "zrando")
        write(binary, b"\x7fELF fake binary")
        out = os.path.join(cls.tmp, "ZeldaRando.zip")
        r = run_builder(binary, PACKAGE, DATA, cls.default, out)
        if r.returncode != 0:
            shutil.rmtree(cls.tmp)
            raise AssertionError(r.stderr)
        with zipfile.ZipFile(out) as z:
            cls.entries = z.namelist()
            cls.config = json.loads(z.read("App/ZeldaRando/config.json"))
            cls.icon = z.read("App/ZeldaRando/icon.png")
            cls.theme = {n[len(THEME):]: z.read(n) for n in cls.entries if n.startswith(THEME) and n != THEME}
            cls.tables = {n[len(TABLES):]: z.read(n) for n in cls.entries if n.startswith(TABLES) and n != TABLES}

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp)

    def test_config_names_the_app_and_its_files(self):
        self.assertEqual(self.config["label"], "Zelda Randomizer")
        self.assertTrue(self.config["description"])
        self.assertIn("App/ZeldaRando/" + self.config["launch"], self.entries)
        icon = self.config["icon"]
        self.assertTrue(icon.startswith("/mnt/SDCARD/App/ZeldaRando/"), icon)
        self.assertIn("App/ZeldaRando/" + os.path.basename(icon), self.entries)

    def test_theme_is_the_package_default_byte_for_byte(self):
        names = sorted(os.listdir(self.default))
        self.assertIn("theme.ini", names)
        self.assertEqual(sorted(self.theme), names)
        for name in names:
            self.assertEqual(self.theme[name], read(os.path.join(self.default, name)), name)

    def test_tables_ship_where_the_app_reads_them(self):
        self.assertEqual(TABLES, sd_path("RANDO_SD_LADX_TABLES_DIR") + "/")
        self.assertEqual(build_zip.TABLES, TABLES)

    def test_tables_are_data_ladx_byte_for_byte(self):
        source = os.path.join(DATA, "ladx")
        names = sorted(n for n in os.listdir(source) if n.endswith(".txt"))
        self.assertIn("order.txt", names)
        self.assertEqual(sorted(self.tables), names)
        for name in names:
            self.assertEqual(self.tables[name], read(os.path.join(source, name)), name)

    def test_no_font_sits_loose_in_the_app_folder(self):
        loose = [n for n in self.entries if n.startswith("App/ZeldaRando/") and "/" not in n[len("App/ZeldaRando/"):]]
        self.assertTrue(loose)
        self.assertEqual([n for n in loose if n.endswith(".ttf")], [])

    def test_icon_is_a_png(self):
        self.assertEqual(self.icon[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(self.icon[12:16], b"IHDR")


STUB = b"""#!/bin/sh
pwd >started_in
echo "stub stderr" >&2
echo "stub stdout"
"""


class Launch(unittest.TestCase):
    def test_runs_zrando_from_its_own_folder_with_stderr_in_a_log(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp)
        app = os.path.join(tmp, "sd card", "App", "ZeldaRando")
        elsewhere = os.path.join(tmp, "elsewhere")
        os.makedirs(app)
        os.mkdir(elsewhere)
        shutil.copy(os.path.join(PACKAGE, "launch.sh"), app)
        write(os.path.join(app, "zrando"), STUB)
        os.chmod(os.path.join(app, "zrando"), 0o755)
        r = subprocess.run(["sh", os.path.join(app, "launch.sh")], cwd=elsewhere,
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(read(os.path.join(app, "started_in")).decode().strip(), os.path.realpath(app))
        self.assertEqual(read(os.path.join(app, "zrando.log")), b"stub stderr\n")
        self.assertEqual(r.stderr, "")
        self.assertEqual(os.listdir(elsewhere), [])

    def test_is_posix_sh_with_lf_line_endings(self):
        text = read(os.path.join(PACKAGE, "launch.sh"))
        self.assertTrue(text.startswith(b"#!/bin/sh\n"))
        self.assertNotIn(b"\r", text)


if __name__ == "__main__":
    unittest.main()
