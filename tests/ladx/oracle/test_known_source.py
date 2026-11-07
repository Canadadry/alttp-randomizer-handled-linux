"""Usage: python3 -I test_known_source.py

Unit tests of known_source.py on a synthetic ROM with a planted table layout, no LADXR, no ROM.
"""

import importlib.util
import os
import random
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("known_source",
                                               os.path.join(HERE, "known_source.py"))
known_source = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(known_source)

BANK = 0x4000
Table = known_source.Table


def at(bank, addr):
    return bank * BANK + addr


class Planted:
    """A 6-bank ROM: rooms (bank 1, an alt room), entities (bank 2, an int pointer), texts
    (bank 3 pointers and banks, data in bank 4), sprite data (bank 5) and backgrounds (bank 5)."""

    def __init__(self):
        rom = bytearray(random.Random(0xB0A7).randbytes(6 * BANK))
        self.rom = rom
        rooms = [bytes([0x01, 0x02, 0x34, 0x05, 0x83, 0x12, 0x06, 0xE1, 0x10, 0x20, 0x30, 0x40,
                        0xFE]),
                 bytes([0x03, 0x04, 0xFE])]
        self.place(1, 0x000, 0x100, rooms)
        alt = bytes([0x09, 0x0A, 0xC2, 0x45, 0x07, 0xFE])
        rom[at(1, 0x300):at(1, 0x300) + len(alt)] = alt
        rom[at(0, 0x10):at(0, 0x12)] = struct.pack("<H", 0x4300)
        entities = [bytes([0x21, 0x05, 0xFF]), None, bytes([0x12, 0xFF])]
        self.place(2, 0x000, 0x100, entities)
        texts = [bytes([0x41, 0x42, 0xFF]), bytes([0x43, 0xFE])]
        self.place(3, 0x000, 0x100, texts, data_bank=4)
        rom[at(3, 0x200)] = 0x04
        rom[at(3, 0x201)] = 0x44
        for n in range(3):
            rom[at(5, 0x000) + n] = n
        rom[at(5, 0x100):at(5, 0x10C)] = bytes(range(0x30, 0x3C))
        backgrounds = [bytes([0x98, 0x00, 0x02, 0x01, 0x02, 0x03, 0x98, 0x20, 0xC1, 0x07, 0x00])]
        self.place(5, 0x200, 0x300, backgrounds)
        self.layout = [
            Table("rooms", {"count": 2, "pointers_bank": 1, "pointers_addr": 0, "data_bank": 1,
                            "alt_pointers": {"Alt06": (0, 0x10)}}, "rooms", "all"),
            Table("entities", {"count": 3, "pointers_bank": 2, "pointers_addr": 0,
                               "data_bank": 2}, "data", "all"),
            Table("texts", {"count": 2, "pointers_bank": 3, "pointers_addr": 0,
                            "banks_bank": 3, "banks_addr": 0x200}, "text", None),
            Table("sprites", {"count": 3, "pointers_bank": 5, "pointers_addr": 0, "data_bank": 5,
                              "data_addr": 0x100, "data_size": 4}, "sprite", "all"),
            Table("backgrounds", {"count": 1, "pointers_bank": 5, "pointers_addr": 0x200,
                                  "data_bank": 5}, "background", [0]),
        ]
        self.bosses = known_source.Bosses(zero_bank=5, zero_addr=0x800, zero_count=4,
                                          item_bank=5, item_addr=0x900, rooms=[1, 3])
        rom[at(5, 0x800):at(5, 0x804)] = bytes([0x00, 0x09, 0x00, 0x08])

    def place(self, bank, pointers_addr, data_addr, entries, data_bank=None):
        data_bank = bank if data_bank is None else data_bank
        for n, data in enumerate(entries):
            if data is None:
                pointer = 0x1234
            else:
                pointer = 0x4000 | data_addr
                self.rom[at(data_bank, data_addr):at(data_bank, data_addr) + len(data)] = data
                data_addr += len(data)
            self.rom[at(bank, pointers_addr) + 2 * n:at(bank, pointers_addr) + 2 * n + 2] = \
                struct.pack("<H", pointer)


def offsets(ranges):
    found = set()
    for start, stop in ranges:
        found.update(range(start, stop))
    return found


class Build(unittest.TestCase):
    def setUp(self):
        self.planted = Planted()
        self.known = known_source.build(bytes(self.planted.rom), self.planted.layout,
                                        self.planted.bosses)

    def test_the_parse_gives_every_entry_s_source_span_or_its_integer_pointer(self):
        entries = known_source.parse(bytes(self.planted.rom), self.planted.layout)
        self.assertEqual(entries["rooms"], {0: (at(1, 0x100), at(1, 0x10D)),
                                            1: (at(1, 0x10D), at(1, 0x110)),
                                            "Alt06": (at(1, 0x300), at(1, 0x306))})
        self.assertEqual(entries["entities"], {0: (at(2, 0x100), at(2, 0x103)), 1: 0x1234,
                                               2: (at(2, 0x103), at(2, 0x105))})
        self.assertEqual(entries["texts"], {0: (at(4, 0x100), at(4, 0x103)),
                                            1: (at(4, 0x103), at(4, 0x105))})
        self.assertEqual(entries["sprites"], {n: (at(5, 0x100 + 4 * n), at(5, 0x104 + 4 * n))
                                              for n in range(3)})
        self.assertEqual(entries["backgrounds"], {0: (at(5, 0x300), at(5, 0x30B))})

    def test_known_entries_pointers_and_boss_sources_are_in_the_map(self):
        want = offsets([
            (at(1, 0x100), at(1, 0x110)), (at(1, 0x300), at(1, 0x306)),
            (at(1, 0), at(1, 4)), (at(0, 0x10), at(0, 0x12)),
            (at(2, 0x100), at(2, 0x103)), (at(2, 0), at(2, 6)),
            (at(3, 0), at(3, 4)), (at(3, 0x200), at(3, 0x201)),
            (at(5, 0x100), at(5, 0x10C)), (at(5, 0), at(5, 3)),
            (at(5, 0x300), at(5, 0x30B)), (at(5, 0x200), at(5, 0x202)),
            (at(5, 0x800), at(5, 0x801)), (at(5, 0x802), at(5, 0x803)),
            (at(5, 0x901), at(5, 0x902)), (at(5, 0x903), at(5, 0x904))])
        self.assertEqual({q for q in range(len(self.planted.rom)) if q in self.known}, want)

    def test_a_span_its_fields_do_not_rebuild_is_left_out_and_named(self):
        self.assertNotIn(at(2, 0x103), self.known)
        self.assertNotIn(at(3, 0x201), self.known)
        self.assertEqual(self.known.failures, ["entities 2", "texts bank 1"])

    def test_text_data_is_not_in_the_map(self):
        self.assertFalse(any(q in self.known for q in range(at(4, 0x100), at(4, 0x105))))

    def test_entries_holds_every_rebuilt_entry_and_none_of_the_failed_ones(self):
        self.assertEqual(self.known.entries, {
            ("rooms", 0): (at(1, 0x100), at(1, 0x10D)), ("rooms", 1): (at(1, 0x10D), at(1, 0x110)),
            ("rooms", "Alt06"): (at(1, 0x300), at(1, 0x306)),
            ("entities", 0): (at(2, 0x100), at(2, 0x103)),
            ("sprites", 0): (at(5, 0x100), at(5, 0x104)),
            ("sprites", 1): (at(5, 0x104), at(5, 0x108)),
            ("sprites", 2): (at(5, 0x108), at(5, 0x10C)),
            ("backgrounds", 0): (at(5, 0x300), at(5, 0x30B))})

    def test_span_at_gives_the_lowest_start_span_holding_an_offset(self):
        self.assertEqual(self.known.span_at(at(1, 0x105)), (at(1, 0x100), at(1, 0x10D)))
        self.assertEqual(self.known.span_at(at(1, 0x10D)), (at(1, 0x10D), at(1, 0x110)))
        self.assertEqual(self.known.span_at(at(0, 0x11)), (at(0, 0x10), at(0, 0x12)))
        self.assertIsNone(self.known.span_at(at(2, 0x103)))
        self.assertIsNone(self.known.span_at(-1))
        self.assertIsNone(self.known.span_at(len(self.planted.rom)))

    def test_span_at_takes_the_lowest_start_of_nested_and_duplicate_spans(self):
        ks = known_source.KnownSource(0x40)
        ks.add(0x10, 0x18, "inner")
        ks.add(0x08, 0x20, "outer")
        ks.add(0x08, 0x20, "same")
        ks.add(0x1C, 0x28, "overlap")
        self.assertEqual(ks.span_at(0x12), (0x08, 0x20))
        self.assertEqual(ks.span_at(0x1F), (0x08, 0x20))
        self.assertEqual(ks.span_at(0x20), (0x1C, 0x28))
        self.assertIsNone(ks.span_at(0x28))

    def test_the_summary_counts_spans_bytes_and_failures(self):
        self.assertEqual(known_source.summary(self.known),
                         "known-source: %d spans, %d bytes, 2 not rebuilt"
                         % (len(self.known.spans), len(self.known)))


class Fields(unittest.TestCase):
    def test_each_kind_rebuilds_its_bytes_from_its_fields(self):
        for kind, data in [("rooms", bytes([1, 2, 0x34, 5, 0x83, 0x12, 6, 0xC2, 0x45, 7,
                                            0xE1, 0x10, 0x20, 0x30, 0x40, 0xFE])),
                           ("data", bytes([0x21, 0x05, 0x33, 0x06, 0xFF])),
                           ("background", bytes([0x98, 0, 2, 1, 2, 3, 0x98, 0x20, 0xC1, 7, 0])),
                           ("sprite", bytes([1, 2, 3, 4]))]:
            with self.subTest(kind=kind):
                fields = known_source.fields(kind, data)
                self.assertEqual(known_source.rebuild(kind, fields), data)

    def test_bytes_past_a_misplaced_terminator_are_no_fields(self):
        self.assertIsNone(known_source.fields("data", bytes([0x12, 0xFF])))
        self.assertIsNone(known_source.fields("rooms", bytes([1, 2, 0x34, 0xFE, 0x00])))


if __name__ == "__main__":
    unittest.main()
