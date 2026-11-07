"""Usage: python3 -I tests/ladx/oracle/key_trace.py --self-test [--ladxr <dir>]

The key trace of LADX's patch tables: which settings and which assembler constants each
inventory row reads while it runs, the hidden dependencies (a callee handed `settings` whole, an
ASM patch naming `ITEM_DIALOGS`) that no grep of the row shows. As a module:
`install(settings_class, assembler, sections, roots) -> KeyTrace` wraps the class's
`__getattr__` (LADXR's Settings answers every setting read there) and replaces the module's
`CONST_MAP` by a dict that records each lookup by `[]` or `get` (the assembler's constant
substitution); `KeyTrace.lines()`, `KeyTrace.write(path)`, `KeyTrace.uninstall()` (the class
restored, the constants copied back into the original dict); `parse(text)`.

A read is recorded at the site read_trace.py gives (`<file> <function>\\t<stripped line>`, ` #<k>`
on a repeat) of the innermost frame in one of `sections`, files relative to the first root; a
read outside every section is not recorded.

Format, sorted unique lines after the version line:
  ladx-key-reads 1
  const\\t<site>\\t<NAME>        a constant the row looked up
  setting\\t<site>\\t<key>       a setting the row read

`--self-test` runs a planted section on LADXR's real Settings and assembler (from --ladxr,
default tests/ladx/oracle/ladxr): a setting read in the row, one in a callee, one outside any
section, and a constant looked up by an assembled line; `key-trace: self-test, 3 setting reads
and 1 constant lookup traced, the read outside the sections left out`, else one `self-test
failed: <case>` line each, exit 1.
"""

import argparse
import importlib.util
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_LADXR = os.path.join(HERE, "ladxr")
VERSION_LINE = "ladx-key-reads 1"
KINDS = ("const", "setting")


class KeyTraceError(Exception):
    pass


def _load(name):
    spec = importlib.util.spec_from_file_location("key_trace_" + name,
                                                  os.path.join(HERE, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = no_bytecode
    return module


read_trace = _load("read_trace")


class _Constants(dict):
    def __init__(self, trace, items):
        super().__init__(items)
        self._trace = trace

    def __getitem__(self, name):
        value = super().__getitem__(name)
        self._trace.record("const", name)
        return value

    def get(self, name, default=None):
        if name in self:
            self._trace.record("const", name)
        return super().get(name, default)


class KeyTrace:
    def __init__(self, sections, roots):
        self._sections = list(sections)
        self._roots = [os.path.realpath(r) for r in roots]
        self._reads = set()
        self._codes = {}
        self._keys = {}
        self._restore = None
        self.active = True

    def _section(self, code):
        if code not in self._codes:
            path, rel = os.path.realpath(code.co_filename), None
            for root in self._roots:
                if path.startswith(root + os.sep):
                    rel = os.path.relpath(path, root).replace(os.sep, "/")
                    break
            qual = read_trace._qualname(code)
            self._codes[code] = next(("%s %s" % (f, fn) for f, fn in self._sections
                                      if f == rel and fn == qual), None)
        return self._codes[code]

    def _site(self):
        frame = sys._getframe(3)
        while frame is not None:
            section = self._section(frame.f_code)
            if section is not None:
                code = frame.f_code
                if code not in self._keys:
                    self._keys[code] = read_trace._function_keys(code.co_filename,
                                                                 code.co_firstlineno)
                return "%s\t%s" % (section, self._keys[code].get(frame.f_lineno, "?"))
            frame = frame.f_back
        return None

    def record(self, kind, name):
        if self.active:
            site = self._site()
            if site is not None:
                self._reads.add((kind, site, name))

    def lines(self):
        return [VERSION_LINE] + ["\t".join(r) for r in sorted(self._reads)]

    def write(self, path):
        with open(path, "w") as f:
            f.write("".join(line + "\n" for line in self.lines()))

    def uninstall(self):
        self.active = False
        if self._restore is not None:
            cls, getattr_, module, constants = self._restore
            cls.__getattr__ = getattr_
            original = dict(module.CONST_MAP)
            constants.clear()
            constants.update(original)
            module.CONST_MAP = constants
            self._restore = None


def install(settings_class, assembler, sections, roots):
    """A KeyTrace of settings_class's attribute reads and assembler.CONST_MAP's lookups;
    sections [(file, function)], files relative to the first of roots."""
    trace = KeyTrace(sections, roots)
    original = settings_class.__getattr__

    def traced(self, item):
        value = original(self, item)
        trace.record("setting", item)
        return value

    trace._restore = (settings_class, original, assembler, assembler.CONST_MAP)
    settings_class.__getattr__ = traced
    assembler.CONST_MAP = _Constants(trace, assembler.CONST_MAP)
    return trace


def parse(text):
    """({site: settings read}, {site: constants looked up}) of a key trace's text;
    KeyTraceError on a malformed one."""
    lines = text.split("\n")
    if not lines or lines[0] != VERSION_LINE or lines[-1] != "":
        raise KeyTraceError("not a key trace (no `%s` line, or no final newline)"
                            % VERSION_LINE)
    reads = {kind: {} for kind in KINDS}
    for number, line in enumerate(lines[1:-1], 2):
        fields = line.split("\t")
        if len(fields) != 4 or fields[0] not in KINDS or not all(fields):
            raise KeyTraceError("key trace line %d: bad line" % number)
        reads[fields[0]].setdefault("%s\t%s" % (fields[1], fields[2]), set()).add(fields[3])
    return reads["setting"], reads["const"]


PLANTED = '''\
import assembler


def helper(settings):
    return settings.race


def generate(settings):
    assembler.resetConsts()
    assembler.const("ITEM_DIALOGS", 1 if settings.textmode != "notextbox" else 0)
    assembler.ASM("ld a, ITEM_DIALOGS")
    helper(settings)
    return settings.witch
'''
SITE = "planted.py generate\t"
SELF_TEST_EXPECTED = [
    VERSION_LINE,
    "const\t%sassembler.ASM(\"ld a, ITEM_DIALOGS\")\tITEM_DIALOGS" % SITE,
    "setting\t%sassembler.const(\"ITEM_DIALOGS\", 1 if settings.textmode != \"notextbox\" "
    "else 0)\ttextmode" % SITE,
    "setting\t%shelper(settings)\trace" % SITE,
    "setting\t%sreturn settings.witch\twitch" % SITE,
]


def self_test(ladxr):
    """Failure lines of the planted run on LADXR's Settings and assembler."""
    folder = tempfile.mkdtemp()
    old_path, old_cwd = list(sys.path), os.getcwd()
    try:
        with open(os.path.join(folder, "planted.py"), "w") as f:
            f.write(PLANTED)
        sys.path[:0] = [folder, os.path.abspath(ladxr)]
        os.chdir(folder)
        import assembler
        import settings as settings_module
        import planted
        trace = install(settings_module.Settings, assembler, [("planted.py", "generate")],
                        [folder])
        try:
            s = settings_module.Settings()
            s.race
            planted.generate(s)
        finally:
            trace.uninstall()
        failures = []
        if trace.lines() != SELF_TEST_EXPECTED:
            failures.append("traced %s" % trace.lines()[1:])
        if type(assembler.CONST_MAP) is not dict or assembler.CONST_MAP.get("ITEM_DIALOGS") != 1:
            failures.append("uninstall did not restore the constants")
        return failures
    finally:
        sys.path[:] = old_path
        os.chdir(old_cwd)
        for name in ("planted", "assembler", "settings"):
            sys.modules.pop(name, None)
        shutil.rmtree(folder)


def main(argv):
    parser = argparse.ArgumentParser(prog="key_trace.py")
    parser.add_argument("--self-test", action="store_true", required=True)
    parser.add_argument("--ladxr", default=DEFAULT_LADXR)
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if not os.path.isfile(os.path.join(args.ladxr, "assembler.py")):
        print("setup: key-trace: no LADXR checkout in %s, run tests/ladx/oracle/setup.sh"
              % args.ladxr)
        return 2
    failures = self_test(args.ladxr)
    for failure in failures:
        print("key-trace: self-test failed: %s" % failure)
    if not failures:
        print("key-trace: self-test, 3 setting reads and 1 constant lookup traced, the read "
              "outside the sections left out")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
