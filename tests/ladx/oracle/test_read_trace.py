"""Usage: python3 -I test_read_trace.py

Unit tests of read_trace.py on stand-in ROM, ROMWithTables and PointerTable classes written to a
temporary directory, with no LADXR and no ROM.
"""

import importlib.util
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("read_trace", os.path.join(HERE, "read_trace.py"))
read_trace = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(read_trace)

STAND_INS = textwrap.dedent('''\
    class ROM:
        def __init__(self, filestream):
            self.banks = []
            for n in range(4):
                self.banks.append(bytearray(filestream.read(0x100)))

        def patch(self, bank_nr, addr, old, new):
            bank = self.banks[bank_nr]
            assert bank[addr:addr + len(old)] == old or bank.find(old) >= 0, "mismatch"
            assert bank[addr] >= 0 and bank.find(old) == addr
            bank[addr:addr + len(new)] = new


    class PointerTable:
        def __init__(self, rom):
            self.data = [rom.banks[3][0x10], rom.banks[3][0x11]]

        def __getitem__(self, item):
            return self.data[item]

        def __contains__(self, item):
            return 0 <= item < len(self.data)

        def __len__(self):
            return len(self.data)

        def __setitem__(self, item, value):
            self.data[item] = value


    class ROMWithTables(ROM):
        def __init__(self, filestream):
            super().__init__(filestream)
            self.texts = PointerTable(self)
            self.name = "not a table"
    ''')

PLANTED = textwrap.dedent('''\
    import io

    def read_index(rom):
        return rom.banks[1][0x12]

    def read_slice(rom):
        return rom.banks[2][0x20:0x28]

    def read_iter(rom):
        return sum(rom.banks[1])

    def read_search(rom):
        return rom.banks[1].find(b"zz")

    def read_compare(rom):
        return rom.banks[2] == b"zz"

    def read_convert(rom):
        return bytes(rom.banks[0])

    def read_table(rom):
        return rom.texts[1]

    def write_byte(rom):
        rom.banks[1][0x40] = 7

    def write_slice(rom):
        rom.banks[1][0x41:0x43] = b"ab"

    def write_entry(rom):
        rom.texts[0] = 9

    def read_written(rom):
        return rom.banks[1][0x40:0x43]

    def read_mixed(rom):
        return rom.banks[1][0x3F:0x42]

    def read_entry(rom):
        return rom.texts[0]

    def generate(rom, which):
        if which == "index":
            read_index(rom)
        if which == "slice":
            read_slice(rom)
        if which == "iter":
            read_iter(rom)
        if which == "search":
            read_search(rom)
        if which == "compare":
            read_compare(rom)
        if which == "convert":
            read_convert(rom)
        if which == "table":
            read_table(rom)
        if which == "repeat":
            read_index(rom)
            read_slice(rom)
            read_index(rom)
        if which == "patch":
            rom.patch(1, 0x30, bytes(range(0x30, 0x34)), b"wxyz")
        if which == "write":
            rom.banks[1][0x40] = 7
            rom.banks[1][0x41:0x43] = b"ab"
        if which == "assign":
            rom.banks[2] = bytearray(0x100)
            rom.banks[2][0x05]
        if which == "nested":
            type(rom)(io.BytesIO(bytes(0x400)))
        if which == "loop":
            for n in (0x05, 0x50, 0x07):
                rom.banks[1][n]
        if which in ("writes", "provenance"):
            write_byte(rom)
            write_slice(rom)
            write_entry(rom)
        if which == "provenance":
            read_written(rom)
            read_mixed(rom)
            read_index(rom)
            read_entry(rom)
            read_table(rom)
        if which == "header":
            for c in rom.banks[1][0x05:0x07]:
                pass
        if which == "rewrite":
            write_byte(rom)
            rom.banks[1][0x40] = 8
    ''')

SECTIONS = [("planted.py", "generate"), ("standins.py", "ROMWithTables.__init__")]


def _load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Fixture(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="read-trace-test-")
        self.addCleanup(shutil.rmtree, self.dir)
        for name, text in (("standins.py", STAND_INS), ("planted.py", PLANTED)):
            with open(os.path.join(self.dir, name), "w") as f:
                f.write(text)
        self.standins = _load(os.path.join(self.dir, "standins.py"), "standins")
        self.planted = _load(os.path.join(self.dir, "planted.py"), "planted")

    def trace(self, which, inventory=None):
        modules = {"ROM": self.standins.ROM, "ROMWithTables": self.standins.ROMWithTables,
                   "PointerTable": self.standins.PointerTable}
        trace = read_trace.install(modules, SECTIONS, inventory)
        self.addCleanup(trace.uninstall)
        rom = self.standins.ROMWithTables(io.BytesIO(bytes(range(256)) * 4))
        self.planted.generate(rom, which)
        return trace

    def generated(self, trace):
        """Records made inside the planted `generate` section, the parse's left out."""
        return [r for r in trace.records() if r[1].startswith("planted.py generate\t")]


class Channels(Fixture):
    def test_an_index_read_is_recorded_with_its_site_group_and_caller(self):
        site = "planted.py generate\tread_index(rom)"
        trace = self.trace("index", {site: "g-index"})
        self.assertEqual(self.generated(trace),
                         [("g-index", site, "planted.py:read_index", "index", "bank 0x01",
                           "0x0012", "source")])

    def check_one(self, which, function, channel, target, detail):
        site = "planted.py generate\t%s(rom)" % function
        trace = self.trace(which, {site: "g-" + which})
        self.assertEqual(self.generated(trace),
                         [("g-" + which, site, "planted.py:" + function, channel, target, detail,
                           "source")])

    def test_a_slice_read(self):
        self.check_one("slice", "read_slice", "slice", "bank 0x02", "0x0020:0x0028")

    def test_an_iteration(self):
        self.check_one("iter", "read_iter", "iter", "bank 0x01", "__iter__")

    def test_a_search(self):
        self.check_one("search", "read_search", "search", "bank 0x01", "find")

    def test_a_comparison(self):
        self.check_one("compare", "read_compare", "compare", "bank 0x02", "__eq__")

    def test_a_conversion(self):
        self.check_one("convert", "read_convert", "convert", "bank 0x00", "__bytes__")

    def test_a_table_read(self):
        self.check_one("table", "read_table", "table", "table texts", "0x0001")

    def test_the_parse_reads_carry_the_parse_site(self):
        site = "standins.py ROMWithTables.__init__\tself.texts = PointerTable(self)"
        trace = self.trace("none", {site: "parse"})
        self.assertEqual(trace.records(),
                         [("parse", site, "standins.py:PointerTable.__init__", "index",
                           "bank 0x03", "0x0010", "source"),
                          ("parse", site, "standins.py:PointerTable.__init__", "index",
                           "bank 0x03", "0x0011", "source")])

    def test_the_innermost_section_is_the_site(self):
        records = self.trace("nested").records()[2:]
        site = "standins.py ROMWithTables.__init__\tself.texts = PointerTable(self)"
        self.assertEqual([r[1] for r in records], [site] * 2)


class Writes(Fixture):
    SITES = {"planted.py generate\twrite_byte(rom)": "g-a",
             "planted.py generate\twrite_slice(rom)": "g-b",
             "planted.py generate\twrite_entry(rom)": "g-t"}

    def test_bank_bytes_and_table_entries_written_are_recorded_with_their_site(self):
        trace = self.trace("writes", self.SITES)
        self.assertEqual(trace.writes(), [
            ("g-a", "planted.py generate\twrite_byte(rom)", "planted.py:write_byte",
             "bank 0x01", "0x0040"),
            ("g-b", "planted.py generate\twrite_slice(rom)", "planted.py:write_slice",
             "bank 0x01", "0x0041:0x0043"),
            ("g-t", "planted.py generate\twrite_entry(rom)", "planted.py:write_entry",
             "table texts", "0x0000")])

    def test_writes_inside_rom_patch_are_recorded(self):
        site = "planted.py generate\trom.patch(1, 0x30, bytes(range(0x30, 0x34)), b\"wxyz\")"
        trace = self.trace("patch", {site: "g-p"})
        self.assertEqual(trace.writes(), [("g-p", site, "standins.py:ROM.patch", "bank 0x01",
                                           "0x0030:0x0034")])

    def test_write_lines_give_each_site_its_positions_and_digest(self):
        lines = self.trace("writes", self.SITES).write_lines()
        self.assertEqual([l.split("\t")[:4] for l in lines], [
            ["g-a", "planted.py generate", "write_byte(rom)", "n=1"],
            ["g-b", "planted.py generate", "write_slice(rom)", "n=2"],
            ["g-t", "planted.py generate", "write_entry(rom)", "n=1"]])
        self.assertTrue(all(re.fullmatch(r"sha1=[0-9a-f]{40}", l.split("\t")[4])
                            for l in lines), lines)

    def test_the_digest_is_stable_across_runs_and_follows_the_net_value(self):
        once = self.trace("writes", self.SITES).write_lines()
        self.assertEqual(self.trace("writes", self.SITES).write_lines(), once)
        sites = dict(self.SITES)
        sites["planted.py generate\twrite_byte(rom) #2"] = "g-a"
        rewritten = self.trace("rewrite", sites)
        byte = [l for l in rewritten.write_lines() if l.startswith("g-a\t")]
        self.assertEqual([l.split("\t")[-1] for l in byte],
                         [l.split("\t")[-1] for l in once if l.startswith("g-a\t")])
        later = [l for l in rewritten.write_lines() if l.startswith("-\t")]
        self.assertEqual(len(later), 1)
        self.assertNotEqual(later[0].split("\t")[-1], byte[0].split("\t")[-1])

    def test_write_writes_saves_the_lines(self):
        trace = self.trace("writes", self.SITES)
        path = os.path.join(self.dir, "writes.txt")
        trace.write_writes(path)
        with open(path) as f:
            self.assertEqual(f.read().splitlines(), trace.write_lines())


class Provenance(Fixture):
    SITES = dict(Writes.SITES)

    def test_each_read_carries_where_its_bytes_come_from(self):
        trace = self.trace("provenance", self.SITES)
        found = [(r[2], r[6]) for r in self.generated(trace)]
        self.assertEqual(found, [("planted.py:read_written", "written:g-a,g-b"),
                                 ("planted.py:read_mixed", "mixed:g-a,g-b"),
                                 ("planted.py:read_index", "source"),
                                 ("planted.py:read_entry", "written:g-t"),
                                 ("planted.py:read_table", "source")])

    def test_a_read_on_a_loop_header_keeps_the_header_line_as_its_site(self):
        records = self.generated(self.trace("header"))
        self.assertEqual([r[1:] for r in records], [
            ("planted.py generate\tfor c in rom.banks[1][0x05:0x07]:", "planted.py:generate",
             "slice", "bank 0x01", "0x0005:0x0007", "source")])


class Exclusions(Fixture):
    def test_reads_inside_rom_patch_are_not_recorded(self):
        trace = self.trace("patch")
        self.assertEqual(self.generated(trace), [])

    def test_writes_are_not_recorded(self):
        self.assertEqual(self.generated(self.trace("write")), [])

    def test_a_bank_assigned_later_is_still_traced_and_written_whole(self):
        trace = self.trace("assign")
        records = self.generated(trace)
        self.assertEqual([r[3:] for r in records],
                         [("index", "bank 0x02", "0x0005", "written:-")])
        self.assertEqual([w[3:] for w in trace.writes()], [("bank 0x02", "0x0000:0x0100")])

    def test_uninstall_stops_recording_and_restores_the_classes(self):
        init = self.standins.ROM.__dict__["__init__"]
        trace = self.trace("none")
        trace.uninstall()
        self.assertIs(self.standins.ROM.__dict__["__init__"], init)
        rom = self.standins.ROMWithTables(io.BytesIO(bytes(256) * 4))
        self.planted.generate(rom, "index")
        self.assertEqual(self.generated(trace), [])


class Output(Fixture):
    def test_a_repeated_site_line_gets_its_occurrence_index_in_the_whole_function(self):
        records = self.generated(self.trace("repeat"))
        self.assertEqual([r[1] for r in records],
                         ["planted.py generate\tread_index(rom) #2",
                          "planted.py generate\tread_slice(rom) #2",
                          "planted.py generate\tread_index(rom) #3"])

    def test_write_aggregates_with_a_count_and_the_offset_range(self):
        trace = self.trace("loop")
        path = os.path.join(self.dir, "trace.txt")
        trace.write(path)
        with open(path) as f:
            lines = [l for l in f.read().splitlines() if l.startswith("-\tplanted.py")]
        self.assertEqual(lines, ["-\tplanted.py generate\trom.banks[1][n]\tplanted.py:generate"
                                 "\tindex\tbank 0x01\tsource\tn=3 0x0005..0x0050"])

    def test_write_raw_lists_every_read_in_order(self):
        trace = self.trace("loop")
        path = os.path.join(self.dir, "trace.txt")
        trace.write(path, raw=True)
        with open(path) as f:
            details = [l.split("\t")[-2] for l in f.read().splitlines()
                       if l.startswith("-\tplanted.py")]
        self.assertEqual(details, ["0x0005", "0x0050", "0x0007"])

    def test_a_slice_range_ends_at_its_last_byte(self):
        trace = self.trace("slice")
        lines = [l for l in trace.lines() if "planted.py:read_slice" in l]
        self.assertEqual([l.split("\t")[-1] for l in lines], ["n=1 0x0020..0x0027"])


def _run(*args):
    proc = subprocess.run([sys.executable, "-I", "-B", os.path.join(HERE, "read_trace.py")]
                          + list(args), capture_output=True, text=True)
    return proc.returncode, proc.stdout


class Command(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="read-trace-cli-")
        self.addCleanup(shutil.rmtree, self.dir)

    def plant(self, rel, text):
        path = os.path.join(self.dir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(text.encode())

    def test_blind_spots_lists_buffer_protocol_lines_skipping_www(self):
        self.plant("rom.py", "class ROM:\r\n    def save(self, f):\r\n        f.write(self.b)\r\n")
        self.plant("utils.py", "import hashlib\nx = 1\ny = memoryview(b).tobytes()\n")
        self.plant("patches/a.py", "struct.unpack_from('<H', b)\nstruct.pack_into('<H', b, 0, 1)\n")
        self.plant("www/app.py", "import hashlib\n")
        self.plant("notes.txt", "memoryview\n")
        code, out = _run("--blind-spots", "--ladxr", self.dir)
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines(), [
            "BLIND patches/a.py\tstruct.unpack_from('<H', b)",
            "BLIND patches/a.py\tstruct.pack_into('<H', b, 0, 1)",
            "BLIND rom.py\tf.write(self.b)",
            "BLIND utils.py\timport hashlib",
            "BLIND utils.py\ty = memoryview(b).tobytes()",
            "read-trace: 5 buffer-protocol sites to review"])

    def test_a_missing_checkout_is_a_setup_line(self):
        for mode in ("--blind-spots", "--self-test"):
            code, out = _run(mode, "--ladxr", os.path.join(self.dir, "none"))
            self.assertEqual(code, 2)
            self.assertTrue(out.startswith("setup: read-trace: no LADXR checkout"), out)

    def test_no_mode_is_a_usage_error(self):
        self.assertEqual(_run()[0], 2)


if __name__ == "__main__":
    unittest.main()
