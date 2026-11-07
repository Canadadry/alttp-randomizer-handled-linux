"""Usage: python3 -I test_key_trace.py

Unit tests of key_trace.py on a stand-in Settings class and assembler module written to a
temporary directory, with no LADXR.
"""

import importlib.util
import os
import shutil
import sys
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("key_trace", os.path.join(HERE, "key_trace.py"))
key_trace = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(key_trace)

STAND_INS = textwrap.dedent('''\
    CONST_MAP = {}


    def const(name, value):
        CONST_MAP[name.upper()] = value


    def resetConsts():
        CONST_MAP.clear()
        CONST_MAP["WRAM"] = 0xC000


    def lookup(name):
        if name in CONST_MAP:
            return CONST_MAP[name]
        return None


    class Settings:
        def __init__(self):
            self.__dict__["_values"] = {"race": False, "textmode": "fast", "witch": True}

        def __getattr__(self, item):
            return self._values[item]
    ''')

PLANTED = textwrap.dedent('''\
    from standins import const, lookup, resetConsts


    def callee(settings):
        return settings.witch


    def generate(settings):
        resetConsts()
        const("item_dialogs", 1 if settings.textmode != "notextbox" else 0)
        lookup("ITEM_DIALOGS") + lookup("WRAM")
        callee(settings)
        callee(settings)
        lookup("NOPE")
        return settings.race
    ''')


class KeyTrace(unittest.TestCase):

    def setUp(self):
        self.root = tempfile.mkdtemp()
        for name, text in (("standins.py", STAND_INS), ("planted.py", PLANTED)):
            with open(os.path.join(self.root, name), "w") as f:
                f.write(text)
        sys.path.insert(0, self.root)
        import standins
        import planted
        self.standins, self.planted = standins, planted

    def tearDown(self):
        sys.path.remove(self.root)
        for name in ("standins", "planted"):
            sys.modules.pop(name, None)
        shutil.rmtree(self.root)

    def run_planted(self):
        trace = key_trace.install(self.standins.Settings, self.standins,
                                  [("planted.py", "generate")], [self.root])
        settings = self.standins.Settings()
        try:
            settings.race
            self.planted.generate(settings)
        finally:
            trace.uninstall()
        return trace, settings

    def test_reads_are_recorded_at_the_section_row_that_runs(self):
        trace, _ = self.run_planted()
        row = "planted.py generate\t%s"
        self.assertEqual(trace.lines(), [
            key_trace.VERSION_LINE,
            "const\t" + row % 'lookup("ITEM_DIALOGS") + lookup("WRAM")' + "\tITEM_DIALOGS",
            "const\t" + row % 'lookup("ITEM_DIALOGS") + lookup("WRAM")' + "\tWRAM",
            "setting\t" + row % "callee(settings)" + "\twitch",
            "setting\t" + row % "callee(settings) #2" + "\twitch",
            "setting\t" + row % 'const("item_dialogs", 1 if settings.textmode != "notextbox" '
            'else 0)' + "\ttextmode",
            "setting\t" + row % "return settings.race" + "\trace",
        ])

    def test_uninstall_restores_the_class_and_the_constants(self):
        _, settings = self.run_planted()
        self.assertIs(type(self.standins.CONST_MAP), dict)
        self.assertEqual(self.standins.CONST_MAP, {"WRAM": 0xC000, "ITEM_DIALOGS": 1})
        self.assertEqual(self.standins.Settings.__getattr__.__qualname__, "Settings.__getattr__")
        self.assertEqual(settings.witch, True)

    def test_the_lines_parse_back_into_reads_per_site(self):
        trace, _ = self.run_planted()
        settings_reads, constant_reads = key_trace.parse("\n".join(trace.lines()) + "\n")
        row = "planted.py generate\t%s"
        self.assertEqual(settings_reads[row % "callee(settings) #2"], {"witch"})
        self.assertEqual(constant_reads, {
            row % 'lookup("ITEM_DIALOGS") + lookup("WRAM")': {"ITEM_DIALOGS", "WRAM"}})
        for bad in ("", "ladx-key-reads 0\n", key_trace.VERSION_LINE + "\nother\ta\tb\tc\n"):
            with self.assertRaises(key_trace.KeyTraceError):
                key_trace.parse(bad)


class Main(unittest.TestCase):

    def test_no_checkout_is_a_setup_line(self):
        empty = tempfile.mkdtemp()
        try:
            import contextlib
            import io
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = key_trace.main(["--self-test", "--ladxr", empty])
        finally:
            shutil.rmtree(empty)
        self.assertEqual(code, 2)
        self.assertTrue(out.getvalue().startswith("setup: key-trace: no LADXR checkout in "))


if __name__ == "__main__":
    unittest.main()
