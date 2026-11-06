"""Usage: python3 -I test_invariant_run.py

Unit tests of invariant_run.py: planted C-group and table-group calls through read_trace.py's
instrumentation of stand-in ROM, ROMWithTables and PointerTable classes, with no LADXR and no
ROM.
"""

import importlib.util
import io
import os
import random
import shutil
import sys
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)


def _load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


read_trace = _load(os.path.join(HERE, "read_trace.py"), "read_trace")
invariant_run = _load(os.path.join(HERE, "invariant_run.py"), "invariant_run")

STAND_INS = textwrap.dedent('''\
    class ROM:
        def __init__(self, filestream):
            self.banks = []
            for n in range(4):
                self.banks.append(bytearray(filestream.read(0x100)))

        def patch(self, bank_nr, addr, old, new):
            bank = self.banks[bank_nr]
            assert bank[addr:addr + len(old)] == old, "mismatch"
            bank[addr:addr + len(new)] = new


    class PointerTable:
        def __init__(self, rom):
            self.data = [rom.banks[3][0x10:0x14], rom.banks[3][0x20:0x24]]

        def __getitem__(self, item):
            return self.data[item]

        def __len__(self):
            return len(self.data)

        def __setitem__(self, item, value):
            self.data[item] = value


    class ROMWithTables(ROM):
        def __init__(self, filestream):
            super().__init__(filestream)
            self.texts = PointerTable(self)
    ''')

# Bank 2 0x40..0x4F and texts entry 0 (bank 3 0x10..0x13) are the C-knowable map.
KNOWN = set(range(0x240, 0x250)) | set(range(0x310, 0x314))

PLANTED = textwrap.dedent('''\
    def constant(rom):
        rom.banks[1][0x10:0x14] = SOURCE[0x2A0:0x2A4]

    def arithmetic_unknowable(rom):
        rom.banks[1][0x20:0x24] = bytes(b ^ 0 for b in rom.banks[2][0x30:0x34])

    def arithmetic_knowable(rom):
        rom.banks[1][0x21] = (rom.banks[2][0x40] + 1) & 0xFF

    def table_copy(rom):
        rom.banks[1][0x50:0x54] = rom.banks[2][0x60:0x64]

    def table_copy_knowable(rom):
        rom.banks[1][0x58:0x5C] = rom.banks[2][0x44:0x48]

    def reads_table_copy(rom):
        rom.banks[1][0x60:0x64] = bytes(b ^ 0 for b in rom.banks[1][0x50:0x54])
        rom.banks[1][0x64] = rom.banks[1][0x58] ^ 0x55

    def computes(rom):
        rom.banks[1][0x70] = 0x42

    def reads_computed(rom):
        rom.banks[1][0x71] = rom.banks[1][0x70] + 1

    def draws(rom, rnd):
        rom.banks[1][0x80] = rnd.randrange(256)
        rom.banks[1][0x81] = random.randrange(256)

    def entries(rom):
        rom.texts[0] = bytes(b ^ 1 for b in rom.texts[0])
        rom.texts[1] = bytes(b ^ 1 for b in rom.texts[1])

    def patches(rom, old):
        rom.patch(1, 0x90, old, b"abcd")
        rom.banks[1][0x94] = 0x10

    def fails_on_scrambled(rom):
        assert rom.banks[2][0x30:0x32] == SOURCE[0x230:0x232]
        rom.banks[1][0xA0] = 0x99

    def nested(rom):
        computes(rom)

    def generate(rom, rnd, old):
        constant(rom)
        arithmetic_unknowable(rom)
        arithmetic_knowable(rom)
        table_copy(rom)
        table_copy_knowable(rom)
        reads_table_copy(rom)
        computes(rom)
        reads_computed(rom)
        draws(rom, rnd)
        entries(rom)
        patches(rom, old)
        fails_on_scrambled(rom)
        nested(rom)
        return rnd.random(), random.random()
    ''')

GROUPS = {"constant(rom)": "cg", "arithmetic_unknowable(rom)": "cg2",
          "arithmetic_knowable(rom)": "cg3", "table_copy(rom)": "tg",
          "table_copy_knowable(rom)": "tg", "reads_table_copy(rom)": "cg4",
          "computes(rom)": "cg5", "reads_computed(rom)": "cg6", "draws(rom, rnd)": "cg7",
          "entries(rom)": "cg8", "patches(rom, old)": "cg9", "fails_on_scrambled(rom)": "cg10",
          "nested(rom)": "tg2"}
SIDES = {g: "C" for g in GROUPS.values()}
SIDES.update(tg="table", tg2="table")


class Planted:
    """The planted run: real ROM bytes out, the event stream and the re-run's flags."""

    def __init__(self, rerun=True):
        self.dir = tempfile.mkdtemp(prefix="invariant-run-")
        try:
            self._run(rerun)
        finally:
            shutil.rmtree(self.dir, ignore_errors=True)

    def _run(self, rerun):
        source = random.Random(0x1ADE).randbytes(0x400)
        self.source = source
        path = os.path.join(self.dir, "planted.py")
        with open(path, "w") as f:
            f.write("import random\n" + STAND_INS + PLANTED)
        planted = _load(path, "planted_%d" % id(self))
        planted.SOURCE = source
        modules = {"ROM": planted.ROM, "ROMWithTables": planted.ROMWithTables,
                   "PointerTable": planted.PointerTable}
        inventory = {"planted.py generate\t" + line: group for line, group in GROUPS.items()}
        trace = read_trace.install(modules, [("planted.py", "generate")], inventory,
                                   roots=[self.dir])
        names = [line.partition("(")[0] for line in GROUPS]
        self.rerun = invariant_run.install(trace, [(planted, n) for n in names], SIDES,
                                           lambda _: KNOWN) if rerun else None
        state = random.getstate()
        random.seed(5)
        try:
            rom = planted.ROMWithTables(io.BytesIO(source))
            self.draws = planted.generate(rom, random.Random(9), source[0x190:0x194])
        finally:
            if self.rerun is not None:
                self.rerun.uninstall()
            trace.uninstall()
            random.setstate(state)
        self.out = b"".join(bytes(memoryview(b)) for b in rom.banks)
        self.texts = [bytes(memoryview(v)) for v in rom.texts.data]
        self.events = trace.events()
        self.tables, self.initial = trace.tables(), trace.initial()

    def flags(self, group):
        """[(event kind, offset or key, flags)] of group's events."""
        return [(e[0], e[3] if e[0] == "B" else e[4], self.rerun.flags.get(n))
                for n, e in enumerate(self.events) if e[1] == group]


RUN = Planted()


class Rerun(unittest.TestCase):
    def test_a_constant_is_invariant(self):
        self.assertEqual(RUN.flags("cg"), [("B", 0x110, (True,) * 4)])

    def test_a_byte_computed_from_a_byte_c_cannot_know_is_not(self):
        self.assertEqual(RUN.flags("cg2"), [("B", 0x120, (False,) * 4)])

    def test_a_byte_computed_from_the_knowable_map_is_invariant(self):
        self.assertEqual(RUN.flags("cg3"), [("B", 0x121, (True,))])

    def test_bytes_computed_from_an_earlier_table_group_s_source_copies_are_not(self):
        self.assertEqual(RUN.flags("cg4"), [("B", 0x160, (False,) * 4), ("B", 0x164, (False,))])

    def test_a_byte_computed_from_an_earlier_computed_write_is_invariant(self):
        self.assertEqual(RUN.flags("cg6"), [("B", 0x171, (True,))])

    def test_the_re_run_draws_what_the_real_call_drew(self):
        self.assertEqual(RUN.flags("cg7"), [("B", 0x180, (True,)), ("B", 0x181, (True,))])

    def test_entries_are_judged_byte_by_byte_on_their_source(self):
        self.assertEqual(RUN.flags("cg8"), [("T", 0, (True,) * 4), ("T", 1, (False,) * 4)])

    def test_patch_s_old_bytes_are_not_checked_on_the_copy(self):
        self.assertEqual(RUN.flags("cg9"), [("B", 0x190, (True,) * 4), ("B", 0x194, (True,))])

    def test_a_re_run_that_raises_flags_nothing(self):
        self.assertEqual(RUN.flags("cg10"), [("B", 0x1A0, (False,))])
        self.assertEqual(RUN.rerun.failed, 1)

    def test_table_groups_and_nested_calls_are_not_re_run(self):
        self.assertEqual(RUN.flags("tg") + RUN.flags("tg2"),
                         [("B", 0x150, None), ("B", 0x158, None), ("B", 0x170, None)])

    def test_the_real_run_is_unchanged(self):
        plain = Planted(rerun=False)
        self.assertEqual((RUN.out, RUN.texts, RUN.draws), (plain.out, plain.texts, plain.draws))
        self.assertEqual(RUN.events, plain.events)

    def test_the_record_reads_back(self):
        sys.path.insert(0, HERE)
        write_log = _load(os.path.join(HERE, "write_log.py"), "write_log_for_rerun")
        text = "".join(line + "\n" for line in RUN.rerun.lines())
        self.assertEqual(write_log.parse_invariant(text), RUN.rerun.flags)


class EndToEnd(unittest.TestCase):
    def test_only_the_invariant_run_of_source_bytes_is_cleared(self):
        write_log = _load(os.path.join(HERE, "write_log.py"), "write_log_end_to_end")
        nb = _load(os.path.join(HERE, "nintendo_bytes.py"), "nintendo_bytes_end_to_end")
        events = write_log.Events(RUN.tables, RUN.initial, RUN.events)
        log = write_log.encode(events, RUN.source, SIDES, KNOWN, RUN.rerun.flags).encode()
        rows = [("%s(rom)" % g, "bank 0x00 source 0x0200..0x02FF", g) for g in SIDES]
        reads = nb.ReadSets(nb.planted_inventory(rows), RUN.source, lambda _: {},
                            lambda _: KNOWN)
        literals, cleared = nb.check_blob("tests/ladx/cases/c/rom_writes.txt", log,
                                          nb.RomIndex(RUN.source), nb.Corpus([]), reads=reads)
        at = lambda value: log.index(value.hex().encode())
        self.assertEqual([(off, n) for _, off, n, _ in literals],
                         [(at(RUN.source[0x230:0x234]), 4), (at(RUN.source[0x260:0x264]), 4)])
        self.assertEqual([(off, n, c) for _, off, n, c in cleared],
                         [(at(RUN.source[0x2A0:0x2A4]), 4, "invariant")])


class Targets(unittest.TestCase):
    def test_the_c_groups_calls_of_generate_rom_are_the_targets(self):
        nb = _load(os.path.join(HERE, "nintendo_bytes.py"), "nintendo_bytes_for_rerun")
        text = nb.planted_inventory([("patches.goal.setX(rom, 1)", "-", "g"),
                                     ("hints.addHints(rom, rnd)", "-", "g"),
                                     ("plain(rom)", "-", "g")])
        self.assertEqual(invariant_run.targets_of(text),
                         [("hints", "addHints"), ("patches.goal", "setX")])


if __name__ == "__main__":
    unittest.main()
