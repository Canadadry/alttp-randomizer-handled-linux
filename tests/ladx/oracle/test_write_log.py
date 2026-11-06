"""Usage: python3 -I test_write_log.py

Unit tests of write_log.py on synthetic ROMs and event streams, with no LADXR and no ROM.
"""

import importlib.util
import os
import random
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("write_log", os.path.join(HERE, "write_log.py"))
write_log = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(write_log)

K = write_log.KNOWN


def V(n):
    return -2 - n

SIDES = {"fix": "table", "cg": "C", "other": "C"}


def synthetic_rom(size=0x1000, seed=0x5EED):
    return random.Random(seed).randbytes(size)


def bank(group, offset, values, origins=None):
    values = bytes(values)
    return ("B", group, "site", offset, values, tuple(origins or [K] * len(values)))


def entry(group, table, key, value, origins=None):
    if isinstance(value, int):
        return ("T", group, "site", table, key, value, None)
    value = bytes(value)
    return ("T", group, "site", table, key, value, tuple(origins or [K] * len(value)))


def stream(events, tables=("texts", "rooms"), initial=()):
    return write_log.Events(tables=list(tables), initial=list(initial), events=list(events))


def encode(events, rom, known=(), sides=SIDES, **kw):
    return write_log.encode(stream(events, **kw), rom, sides, set(known))


def body(text):
    """The log's lines after its header."""
    lines = text.splitlines()
    assert lines[0] == "ladx-rom-writes 1" and lines[1].startswith("tables"), lines[:2]
    return lines[2:]


class CGroups(unittest.TestCase):
    def setUp(self):
        self.rom = synthetic_rom()

    def test_a_copied_source_byte_is_a_copy_token(self):
        log = encode([bank("cg", 0x10, self.rom[0x200:0x203], [0x200, 0x201, 0x202])], self.rom)
        self.assertEqual(body(log), ["G cg", "B 10 @200+3"])

    def test_a_source_byte_copied_to_its_own_offset_is_a_keep_token(self):
        log = encode([bank("cg", 0x200, self.rom[0x200:0x203], [0x200, 0x201, 0x202])], self.rom)
        self.assertEqual(body(log), ["G cg", "B 200 =3"])

    def test_a_computed_byte_equal_to_a_knowable_byte_it_replaces_inherits_its_encoding(self):
        values = bytes([self.rom[0x40], self.rom[0x41] ^ 1, self.rom[0x42], self.rom[0x43]])
        log = encode([bank("cg", 0x40, values)], self.rom, known={0x40, 0x42})
        self.assertEqual(body(log), ["G cg", "B 40 =1 %02x =1 %02x" % (values[1], values[3])])

    def test_a_computed_byte_inherits_a_literal_it_replaces(self):
        log = encode([bank("other", 0x50, b"\x07\x08"), bank("cg", 0x50, b"\x07\x09")],
                     self.rom)
        self.assertEqual(body(log), ["G other", "B 50 0708", "G cg", "B 50 0709"])

    def test_no_arbitrary_source_match_is_applied_to_a_c_group(self):
        log = encode([bank("cg", 0x10, self.rom[0x300:0x308])], self.rom)
        self.assertEqual(body(log), ["G cg", "B 10 " + self.rom[0x300:0x308].hex()])

    def test_a_reference_outside_the_knowable_map_is_not_inherited(self):
        log = encode([bank("cg", 0x10, self.rom[0x300:0x302], [0x300, 0x301]),
                      bank("other", 0x10, self.rom[0x300:0x302])], self.rom, known={0x301})
        self.assertEqual(body(log), ["G cg", "B 10 @300+2", "G other",
                                     "B 10 %02x @301+1" % self.rom[0x300]])


class WriteIdentities(unittest.TestCase):
    def setUp(self):
        self.rom = synthetic_rom()

    def test_a_computed_byte_copied_unchanged_keeps_the_encoding_of_its_write(self):
        log = encode([bank("cg", 0x40, self.rom[0x40:0x42] + b"\x07", [V(0), V(1), V(2)]),
                      bank("other", 0x90, self.rom[0x41:0x42] + b"\x07", [V(1), V(2)])],
                     self.rom, known={0x41})
        self.assertEqual(body(log), ["G cg", "B 40 %02x =1 07" % self.rom[0x40], "G other",
                                     "B 90 @41+1 07"])

    def test_an_entry_moved_unchanged_keeps_its_entry_encoding(self):
        old = self.rom[0x80:0x84]
        new = old[:2] + b"\x55" + old[3:]
        origins = [V(0), V(1), V(2), V(3)]
        log = encode([entry("cg", "texts", 3, new, origins), bank("other", 0x200, new, origins)],
                     self.rom, known=range(0x80, 0x84),
                     initial=[("texts", 3, old, tuple(range(0x80, 0x84)))])
        self.assertEqual(body(log), ["G cg", "T texts 3 @80+2 55 @83+1", "G other",
                                     "B 200 @80+2 55 @83+1"])

    def test_a_table_group_s_encoding_follows_its_bytes_copies(self):
        value = self.rom[0x700:0x706]
        log = encode([bank("fix", 0x10, value, [V(n) for n in range(6)]),
                      bank("cg", 0x300, value[1:5], [V(n) for n in range(1, 5)])], self.rom)
        self.assertEqual(body(log), ["G fix", "B 10 @700+6", "G cg", "B 300 @701+4"])

    def test_the_event_stream_reads_write_identities(self):
        text = "ladx-events 1\nsource 00\ntables\nB\tg\t10\t0102\tv3+2\ts\n"
        self.assertEqual(write_log.parse_events(text).events[0][5], (V(3), V(4)))


class Segments(unittest.TestCase):
    def setUp(self):
        self.rom = synthetic_rom()

    def test_writes_within_a_segment_collapse_to_their_net_effect_in_merged_records(self):
        log = encode([bank("cg", 0x10, b"\x01\x02"), bank("cg", 0x12, b"\x03"),
                      bank("cg", 0x11, b"\x09"), bank("cg", 0x20, b"\x04"),
                      bank("cg", 0x13, self.rom[0x100:0x102], [0x100, 0x101]),
                      bank("cg", 0x15, self.rom[0x102:0x103], [0x102])], self.rom)
        self.assertEqual(body(log), ["G cg", "B 10 010903 @100+3", "B 20 04"])

    def test_each_maximal_run_of_one_group_s_writes_is_a_segment(self):
        log = encode([bank("cg", 0x10, b"\x01"), bank("other", 0x30, b"\x02"),
                      bank("cg", 0x10, b"\x03"), bank("cg", 0x11, b"\x04")], self.rom)
        self.assertEqual(body(log), ["G cg", "B 10 01", "G other", "B 30 02", "G cg",
                                     "B 10 0304"])

    def test_a_group_without_a_writing_side_is_refused(self):
        with self.assertRaisesRegex(write_log.WriteLogError, "side"):
            encode([bank("-", 0x10, b"\x01")], self.rom)


class Tables(unittest.TestCase):
    def setUp(self):
        self.rom = synthetic_rom()

    def test_table_records_follow_the_table_order_then_index_then_alt_key(self):
        log = encode([entry("cg", "rooms", "Alt06", b"\x01\xfe"), entry("cg", "rooms", 2, 0x4123),
                      entry("cg", "texts", 0x1f, b"\x02\xff"), entry("cg", "rooms", 1, b"\xfe")],
                     self.rom)
        self.assertEqual(body(log), ["G cg", "T texts 1f 02ff", "T rooms 1 fe", "T rooms 2 p4123",
                                     "T rooms Alt06 01fe"])

    def test_bank_records_come_before_table_records_and_an_entry_copy_is_a_reference(self):
        log = encode([entry("cg", "texts", 3, self.rom[0x80:0x83], [0x80, 0x81, 0x82]),
                      bank("cg", 0x10, b"\x01")], self.rom)
        self.assertEqual(body(log), ["G cg", "B 10 01", "T texts 3 @80+3"])

    def test_an_entry_inherits_the_knowable_prefix_and_suffix_of_its_previous_value(self):
        old = self.rom[0x80:0x86]
        new = old[:2] + b"\x11\x22" + old[3:]
        initial = [("texts", 3, old, tuple(range(0x80, 0x86)))]
        log = encode([entry("cg", "texts", 3, new)], self.rom, known=range(0x80, 0x86),
                     initial=initial)
        self.assertEqual(body(log), ["G cg", "T texts 3 @80+2 1122 @83+3"])
        partial = encode([entry("cg", "texts", 3, new)], self.rom, known={0x80, 0x84},
                         initial=initial)
        self.assertEqual(body(partial), ["G cg", "T texts 3 @80+1 %s1122%s"
                                         % (old[1:2].hex(), old[3:].hex())])

    def test_an_entry_never_written_nor_parsed_has_nothing_to_inherit(self):
        log = encode([entry("cg", "texts", 3, self.rom[0x80:0x82])], self.rom,
                     known=range(0x80, 0x82))
        self.assertEqual(body(log), ["G cg", "T texts 3 " + self.rom[0x80:0x82].hex()])


class TableGroups(unittest.TestCase):
    def setUp(self):
        self.rom = synthetic_rom()

    def test_keep_then_origin_then_longest_source_match_then_literal(self):
        values = (self.rom[0x10:0x12] + self.rom[0x900:0x901] + self.rom[0x700:0x706]
                  + b"\xaa\xbb")
        origins = [K, K, 0x900] + [K] * 8
        log = encode([bank("fix", 0x10, values, origins)], self.rom)
        self.assertEqual(body(log), ["G fix", "B 10 =2 @900+1 @700+6 aabb"])

    def test_no_aligned_four_byte_literal_occurs_in_the_source(self):
        rng = random.Random(7)
        rom = bytes(rng.choice(b"\x00\x01\x02\x03") for _ in range(0x400))
        values = bytes(rng.choice(b"\x00\x01\x02\x03\x04") for _ in range(0x200))
        log = encode([bank("fix", 0x200, values)], rom)
        self.assertEqual(write_log.decode(log, rom)[0x200:0x400], values)
        for token in body(log)[1].split(" ")[2:]:
            if token[0] not in "=@":
                literal = bytes.fromhex(token)
                for i in range(len(literal) - 3):
                    self.assertEqual(rom.find(literal[i:i + 4]), -1, token)

    def test_the_longest_match_at_its_lowest_offset_is_taken(self):
        rom = bytearray(0x100)
        rom[0x10:0x15] = b"\x41\x42\x43\x44\x45"
        rom[0x30:0x36] = b"\x41\x42\x43\x44\x45\x46"
        rom[0x50:0x56] = b"\x41\x42\x43\x44\x45\x46"
        log = encode([bank("fix", 0x80, b"\x41\x42\x43\x44\x45\x46\x47")], bytes(rom))
        self.assertEqual(body(log), ["G fix", "B 80 @30+6 47"])

    def test_a_table_group_entry_uses_its_origins_then_source_matches(self):
        values = self.rom[0x400:0x405] + self.rom[0x80:0x81]
        log = encode([entry("fix", "texts", 1, values, [K] * 5 + [0x80])], self.rom)
        self.assertEqual(body(log), ["G fix", "T texts 1 @400+5 @80+1"])


class Decode(unittest.TestCase):
    def test_decoding_a_log_gives_the_rom_the_events_wrote(self):
        rom = synthetic_rom()
        rng = random.Random(3)
        events, want = [], bytearray(rom)
        for n in range(200):
            offset = rng.randrange(0, len(rom) - 16)
            length = rng.randrange(1, 16)
            source = rng.randrange(0, len(rom) - length)
            if rng.random() < 0.5:
                values, origins = rom[source:source + length], range(source, source + length)
            else:
                values, origins = rng.randbytes(length), None
            events.append(bank(rng.choice(["cg", "other", "fix"]), offset, values, origins))
            want[offset:offset + length] = values
            if rng.random() < 0.2:
                events.append(entry("cg", "texts", rng.randrange(4), values, origins))
        log = encode(events, rom, known=range(0, len(rom), 3))
        self.assertEqual(write_log.decode(log, rom), bytes(want))


class Projection(unittest.TestCase):
    def setUp(self):
        self.rom = synthetic_rom()

    def test_a_projection_replays_one_group_s_segments_into_one_canonical_segment(self):
        log = encode([bank("cg", 0x10, b"\x01\x02"), bank("other", 0x30, b"\x09"),
                      bank("cg", 0x11, b"\x03"), bank("cg", 0x12, self.rom[0x12:0x13], [0x12]),
                      entry("cg", "texts", 2, b"\x05")], self.rom)
        self.assertEqual(body(write_log.project(log, "cg")),
                         ["G cg", "B 10 0103 =1", "T texts 2 05"])
        self.assertEqual(body(write_log.project(log, "nobody")), [])

    def test_a_projection_ignores_other_groups_records(self):
        events = [bank("cg", 0x10, b"\x01"), bank("other", 0x30, b"\x09"),
                  bank("cg", 0x11, b"\x03")]
        changed = [bank("cg", 0x10, b"\x01"), bank("other", 0x31, b"\x08\x07"),
                   entry("other", "rooms", 0, b"\xfe"), bank("cg", 0x11, b"\x03")]
        self.assertEqual(write_log.project(encode(events, self.rom), "cg"),
                         write_log.project(encode(changed, self.rom), "cg"))

    def test_a_projection_does_not_depend_on_how_other_groups_split_it(self):
        events = [bank("cg", 0x10, b"\x01"), bank("cg", 0x10, b"\x02"),
                  bank("cg", 0x11, self.rom[0x11:0x12])]
        split = [events[0], bank("other", 0x30, b"\x09"), events[1], events[2]]
        self.assertEqual(write_log.project(encode(events, self.rom, known={0x11}), "cg"),
                         write_log.project(encode(split, self.rom, known={0x11}), "cg"))

    def test_a_projection_of_a_projection_is_itself(self):
        log = encode([bank("cg", 0x10, b"\x01"), bank("other", 0x10, b"\x09"),
                      bank("cg", 0x11, b"\x03")], self.rom)
        once = write_log.project(log, "cg")
        self.assertEqual(write_log.project(once, "cg"), once)


class Scrambled(unittest.TestCase):
    def setUp(self):
        self.rom = synthetic_rom()
        self.known = set(range(0x100, 0x180))
        self.events = [
            bank("fix", 0x100, self.rom[0x300:0x308]),
            bank("cg", 0x100, self.rom[0x300:0x304]),
            bank("cg", 0x120, self.rom[0x120:0x124]),
            bank("cg", 0x200, self.rom[0x600:0x604], range(0x600, 0x604)),
            bank("cg", 0x204, self.rom[0x204:0x206]),
            entry("cg", "texts", 1, self.rom[0x140:0x142] + b"\x09"),
        ]
        self.initial = [("texts", 1, self.rom[0x140:0x143], (0x140, 0x141, 0x142))]

    def compare(self, **kw):
        return write_log.compare_scrambled(stream(self.events, initial=self.initial), self.rom,
                                           SIDES, self.known, **kw)

    def test_c_groups_encode_the_same_with_every_unknowable_byte_scrambled(self):
        self.assertEqual(self.compare(), [])

    def test_an_encoding_that_depends_on_a_byte_outside_the_map_is_flagged(self):
        self.assertEqual(self.compare(scramble_known=set()),
                         ["SCRAMBLED cg: projection differs from line 5"])

    def test_an_entry_s_alignment_stops_at_a_byte_c_cannot_know(self):
        old = self.rom[0x180:0x186]
        new = old[:3] + b"\x11" + old[4:]
        events = stream([entry("cg", "texts", 3, new)],
                        initial=[("texts", 3, old, tuple(range(0x180, 0x186)))])
        known = {0x180, 0x182, 0x183, 0x184, 0x185}
        log = write_log.encode(events, self.rom, SIDES, known)
        self.assertEqual(body(log), ["G cg", "T texts 3 @180+1 %s11 @184+2" % old[1:3].hex()])
        self.assertEqual(write_log.compare_scrambled(events, self.rom, SIDES, known), [])

    def test_table_groups_keep_the_encoding_step_6_fixed_on_the_real_rom(self):
        rom = bytearray(self.rom)
        value = b"\x5a\xa5\x3c\xc3"
        rom[0x050:0x054] = value
        rom[0x150:0x154] = value
        events = stream([bank("fix", 0x200, value), bank("cg", 0x200, value)])
        self.assertEqual(write_log.compare_scrambled(events, bytes(rom), SIDES, self.known), [])

    def test_the_scramble_changes_every_byte_outside_the_map_only(self):
        scrambled = write_log.scramble(self.rom, self.known)
        self.assertTrue(all((a == b) == (p in self.known)
                            for p, (a, b) in enumerate(zip(self.rom, scrambled))))


class Grammar(unittest.TestCase):
    HEADER = "ladx-rom-writes 1\ntables texts rooms\n"

    def refused(self, text, pattern):
        with self.assertRaisesRegex(write_log.WriteLogError, pattern):
            write_log.decode(text, bytes(0x1000))

    def test_a_valid_log_parses(self):
        tables, segments = write_log.parse(self.HEADER + "G a\nB 10 =2 @0+1 0a\nT texts 1 p4000\n"
                                           "T rooms Alt06 @3+2\nG b\nT texts 0\n")
        self.assertEqual(tables, ["texts", "rooms"])
        self.assertEqual([s[0] for s in segments], ["a", "b"])

    def test_every_broken_rule_is_refused(self):
        for text, pattern in [
                ("G a\nB 10 01\n", "line 1"),
                ("ladx-rom-writes 1\nG a\n", "line 2"),
                (self.HEADER + "B 10 01\n", "before any G"),
                (self.HEADER + "G a\n", "no record"),
                (self.HEADER + "G a\nG b\nB 1 01\n", "no record"),
                (self.HEADER + "G a\nB 1 01\nG a\nB 3 01\n", "continues"),
                (self.HEADER + "G a\nB 10\n", "without bytes"),
                (self.HEADER + "G a\nB 10 0\n", "bad token"),
                (self.HEADER + "G a\nB 10 0A\n", "bad token"),
                (self.HEADER + "G a\nB 10 01 02\n", "continues"),
                (self.HEADER + "G a\nB 10 @0+1 @1+1\n", "continues"),
                (self.HEADER + "G a\nB 10 @10+1\n", "not canonical"),
                (self.HEADER + "G a\nB 10 =0\n", "empty"),
                (self.HEADER + "G a\nB 10 01\nB 11 01\n", "not maximal"),
                (self.HEADER + "G a\nB 10 01\nB 5 01\n", "out of order"),
                (self.HEADER + "G a\nT texts 1 01\nB 5 01\n", "out of order"),
                (self.HEADER + "G a\nT rooms 1 01\nT texts 1 01\n", "out of order"),
                (self.HEADER + "G a\nT other 1 01\n", "unknown table"),
                (self.HEADER + "G a\nT texts 1 =1\n", "bad token"),
                (self.HEADER + "G a\nB 10 p40\n", "bad token"),
                (self.HEADER + "G a\nT texts 01 01\n", "bad key"),
                (self.HEADER + "G a\nX 10 01\n", "unknown line"),
                (self.HEADER + "G a\nB 10 01", "newline"),
                (self.HEADER + "G a\nB 1000 01\n", "beyond the ROM")]:
            with self.subTest(text=text):
                self.refused(text, pattern)


class EventStream(unittest.TestCase):
    def test_the_stream_read_trace_writes_is_read_back(self):
        text = ("ladx-events 1\nsource 00\ntables texts\nI\ttexts\t0\t0102\t30+2\n"
                "I\ttexts\tAlt06\tp4100\t-\nB\tg\t260\t606162\t160+2,k1\tf.py fn\tline\n"
                "T\tg\ttexts\t1f\t-\t-\tsite\n")
        events = write_log.parse_events(text)
        self.assertEqual(events.tables, ["texts"])
        self.assertEqual(events.initial, [("texts", 0, b"\x01\x02", (0x30, 0x31)),
                                          ("texts", "Alt06", 0x4100, None)])
        self.assertEqual(events.events, [
            ("B", "g", "f.py fn\tline", 0x260, b"\x60\x61\x62", (0x160, 0x161, K)),
            ("T", "g", "site", "texts", 0x1f, b"", ())])

    def test_a_malformed_stream_is_refused(self):
        for text in ("x\n", "ladx-events 1\nsource 00\ntables\nB\tg\t10\t01\t1+2\ts\n",
                     "ladx-events 1\nsource 00\ntables\nQ\n"):
            with self.subTest(text=text):
                with self.assertRaises(write_log.WriteLogError):
                    write_log.parse_events(text)


class Command(unittest.TestCase):
    def test_the_self_test_passes(self):
        self.assertEqual(write_log.self_test(), [])


if __name__ == "__main__":
    unittest.main()
