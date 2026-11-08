#include "synth_rom.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

enum { TEXT, DATA, ROOMS, SPRITE, BACKGROUND };

#define ALT_MAX 5

typedef struct Layout {
	const char *name;
	int kind;
	uint32_t count, pointers_bank, pointers_addr;
	int data_bank; /* -1: the banks table gives each entry's bank */
	uint32_t banks_bank, banks_addr;
	uint32_t data_addr, data_size;
	int alt_count;
	const char *alt[ALT_MAX];
	uint32_t alt_bank[ALT_MAX], alt_addr[ALT_MAX];
	uint32_t place_addr, place_end;
} Layout;

const char *const synth_table_names[SYNTH_TABLES] = {
	"texts", "entities", "rooms_overworld_top", "rooms_overworld_bottom", "rooms_indoor_a", "rooms_indoor_b",
	"rooms_color_dungeon", "room_sprite_data_overworld", "room_sprite_data_indoor", "background_tiles",
	"background_attributes",
};

const uint32_t synth_boss_rooms[SYNTH_BOSS_ROOMS] = { 0x106, 0x12B, 0x15A, 0x166, 0x185, 0x1BC, 0x223, 0x234, 0x300 };

static const Layout layout[SYNTH_TABLES] = {
	{ "texts", TEXT, 0x2B0, 0x1C, 1, -1, 0x1C, 0x741, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0x4000 },
	{ "entities", DATA, 0x320, 0x16, 0, 0x16, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0x640, 0x4000 },
	{ "rooms_overworld_top", ROOMS, 0x080, 0x09, 0x000, 0x09, 0, 0, 0, 0, 5,
	  { "Alt06", "Alt0E", "Alt1B", "Alt2B", "Alt79" }, { 0, 0, 0, 0, 0 },
	  { 0x31FD, 0x31CD, 0x320D, 0x321D, 0x31ED }, 0x200, 0x4000 },
	{ "rooms_overworld_bottom", ROOMS, 0x080, 0x09, 0x100, 0x1A, 0, 0, 0, 0, 1, { "Alt8C" }, { 0 }, { 0x31DD },
	  0x000, 0x4000 },
	{ "rooms_indoor_a", ROOMS, 0x100, 0x0A, 0x000, 0x0A, 0, 0, 0, 0, 1, { "Alt1F5" }, { 0 }, { 0x31A1 }, 0x200,
	  0x3B77 },
	{ "rooms_indoor_b", ROOMS, 0x0FF, 0x0B, 0x000, 0x0B, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0x200, 0x4000 },
	{ "rooms_color_dungeon", ROOMS, 0x016, 0x0A, 0x3B77, 0x0A, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0x3BA3,
	  0x4000 },
	{ "room_sprite_data_overworld", SPRITE, 0x100, 0x20, 0x30D3, 0x20, 0, 0, 0x33F3, 4, 0, { 0 }, { 0 }, { 0 }, 0,
	  0 },
	{ "room_sprite_data_indoor", SPRITE, 0x220, 0x20, 0x31D3, 0x20, 0, 0, 0x363B, 4, 0, { 0 }, { 0 }, { 0 }, 0,
	  0 },
	{ "background_tiles", BACKGROUND, 0x26, 0x20, 0x052B, 0x08, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0x4000 },
	{ "background_attributes", BACKGROUND, 0x26, 0x24, 0x1C4B, 0x24, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0,
	  0x1C4B },
};

static const uint32_t text_banks[3] = { 0x1D, 0x1E, 0x1F };

typedef struct Builder {
	SynthRom *s;
	uint32_t state;
	uint32_t cursor[0x40];
} Builder;

static uint32_t next(Builder *b)
{
	b->state ^= b->state << 13;
	b->state ^= b->state >> 17;
	b->state ^= b->state << 5;
	return b->state;
}

static uint8_t byte(Builder *b)
{
	return (uint8_t)(next(b) >> 24);
}

static uint8_t byte_not(Builder *b, uint8_t x, uint8_t y)
{
	uint8_t v;

	do
		v = byte(b);
	while (v == x || v == y);
	return v;
}

static size_t entry_bytes(Builder *b, int kind, uint8_t *out, int plant)
{
	size_t n = 0;
	int k, objects;

	switch (kind) {
	case TEXT:
		k = (int)(next(b) % 12);
		while (k-- > 0)
			out[n++] = byte_not(b, 0xFE, 0xFF);
		out[n++] = (next(b) & 1) ? 0xFE : 0xFF;
		return n;
	case DATA:
		if (plant) {
			/* the 0xFF falls on a pair's second byte */
			out[n++] = byte_not(b, 0xFF, 0xFF);
			out[n++] = byte_not(b, 0xFF, 0xFF);
			out[n++] = byte_not(b, 0xFF, 0xFF);
			out[n++] = 0xFF;
			return n;
		}
		k = (int)(next(b) % 5);
		while (k-- > 0) {
			out[n++] = byte_not(b, 0xFF, 0xFF);
			out[n++] = byte_not(b, 0xFF, 0xFF);
		}
		out[n++] = 0xFF;
		return n;
	case ROOMS:
		out[n++] = byte(b);
		out[n++] = byte(b);
		objects = (int)(next(b) % 6);
		while (objects-- > 0) {
			uint8_t first;

			switch (next(b) % 3) {
			case 0:
				do
					first = byte(b);
				while (first == 0xFE || (first & 0xF0) == 0x80 || (first & 0xF0) == 0xC0 ||
				       (first & 0xF0) == 0xE0);
				out[n++] = first;
				out[n++] = byte(b);
				break;
			case 1:
				out[n++] = (uint8_t)(((next(b) & 1) ? 0x80 : 0xC0) | (byte(b) & 0x0F));
				out[n++] = byte(b);
				out[n++] = byte(b);
				break;
			default:
				out[n++] = (uint8_t)(0xE0 | (byte(b) & 0x0F));
				for (k = 0; k < 4; k++)
					out[n++] = byte(b);
			}
		}
		out[n++] = 0xFE;
		return n;
	default:
		objects = 1 + (int)(next(b) % 4);
		while (objects-- > 0) {
			uint8_t flags = (uint8_t)((byte(b) & 0xC0) | (next(b) % 8));
			int count = (flags & 0x40) ? 1 : (flags & 0x3F) + 1;

			out[n++] = byte_not(b, 0x00, 0x00);
			out[n++] = byte(b);
			out[n++] = flags;
			while (count-- > 0)
				out[n++] = byte(b);
		}
		out[n++] = 0x00;
		return n;
	}
}

static uint16_t integer_pointer(Builder *b)
{
	uint16_t v = (uint16_t)(next(b) & 0x3FFF);

	return (next(b) & 1) ? (uint16_t)(v | 0x8000) : v;
}

static int is_integer(int table, uint32_t n)
{
	switch (table) {
	case 0:
		return n % 101 == 50;
	case 1:
		return n % 97 == 7;
	case 5:
		return n % 61 == 30;
	default:
		return 0;
	}
}

static SynthEntry *add(SynthRom *s, int table)
{
	SynthEntry *e;

	assert(s->entry_count < SYNTH_ENTRY_MAX);
	e = &s->entry[s->entry_count++];
	memset(e, 0, sizeof *e);
	e->table = table;
	e->bank_at = -1;
	e->entry_rebuilds = 1;
	e->bank_rebuilds = 1;
	s->table_count[table]++;
	return e;
}

static uint32_t place(Builder *b, const Layout *l, uint32_t bank, const uint8_t *bytes, size_t n)
{
	uint32_t addr = b->cursor[bank], at = bank * SYNTH_ROM_BANK + addr;

	assert(addr + n <= (l->kind == TEXT ? SYNTH_ROM_BANK : l->place_end));
	memcpy(b->s->rom + at, bytes, n);
	b->cursor[bank] = addr + (uint32_t)n;
	return at;
}

static void put_pointer(SynthRom *s, uint32_t at, uint16_t v)
{
	s->rom[at] = (uint8_t)(v & 0xFF);
	s->rom[at + 1] = (uint8_t)(v >> 8);
}

static void build_table(Builder *b, int t)
{
	const Layout *l = &layout[t];
	SynthRom *s = b->s;
	uint8_t bytes[64];
	uint32_t n;
	int a;

	s->table_first[t] = s->entry_count;
	if (l->data_bank >= 0 && l->kind != SPRITE)
		b->cursor[l->data_bank] = l->place_addr;
	for (n = 0; n < l->count; n++) {
		SynthEntry *e = add(s, t);

		e->index = n;
		if (l->kind == SPRITE) {
			uint8_t slot = byte(b);

			e->pointer_at = l->pointers_bank * SYNTH_ROM_BANK + l->pointers_addr + n;
			e->pointer_len = 1;
			s->rom[e->pointer_at] = slot;
			e->bank = (uint8_t)l->data_bank;
			e->start = e->bank * SYNTH_ROM_BANK + l->data_addr + slot * l->data_size;
			e->stop = e->start + l->data_size;
			continue;
		}
		e->pointer_at = l->pointers_bank * SYNTH_ROM_BANK + l->pointers_addr + 2 * n;
		e->pointer_len = 2;
		if (l->data_bank >= 0) {
			e->bank = (uint8_t)l->data_bank;
		} else {
			uint8_t bank_byte;

			e->bank = (uint8_t)text_banks[n % 3];
			bank_byte = e->bank;
			if (n % 37 == 5) {
				bank_byte |= (n & 1) ? 0x40 : 0x80;
				e->bank_rebuilds = 0;
			}
			e->bank_at = (int32_t)(l->banks_bank * SYNTH_ROM_BANK + l->banks_addr + n);
			s->rom[e->bank_at] = bank_byte;
			if (b->cursor[e->bank] == 0)
				b->cursor[e->bank] = l->place_addr;
		}
		if (is_integer(t, n)) {
			e->integer = 1;
			e->pointer = integer_pointer(b);
			put_pointer(s, e->pointer_at, (uint16_t)e->pointer);
			continue;
		}
		{
			int plant = l->kind == DATA && n % 53 == 11;
			size_t len = entry_bytes(b, l->kind, bytes, plant);

			e->entry_rebuilds = !plant;
			e->start = place(b, l, e->bank, bytes, len);
			e->stop = e->start + (uint32_t)len;
			put_pointer(s, e->pointer_at, (uint16_t)(0x4000 | (e->start % SYNTH_ROM_BANK)));
		}
	}
	for (a = 0; a < l->alt_count; a++) {
		SynthEntry *e = add(s, t);
		size_t len = entry_bytes(b, l->kind, bytes, 0);

		e->alt = l->alt[a];
		e->bank = (uint8_t)l->data_bank;
		e->pointer_at = l->alt_bank[a] * SYNTH_ROM_BANK + l->alt_addr[a];
		e->pointer_len = 2;
		e->start = place(b, l, e->bank, bytes, len);
		e->stop = e->start + (uint32_t)len;
		put_pointer(s, e->pointer_at, (uint16_t)(0x4000 | (e->start % SYNTH_ROM_BANK)));
	}
}

void synth_rom_build(SynthRom *s, uint32_t seed)
{
	static Builder b;
	uint32_t i;
	int t;

	memset(&b, 0, sizeof b);
	b.s = s;
	b.state = seed ? seed : 0x9E3779B9u;
	s->entry_count = 0;
	memset(s->table_first, 0, sizeof s->table_first);
	memset(s->table_count, 0, sizeof s->table_count);
	for (i = 0; i < SYNTH_ROM_SIZE; i++)
		s->rom[i] = byte(&b);
	for (t = 0; t < SYNTH_TABLES; t++)
		build_table(&b, t);
	for (i = 0; i < SYNTH_BOSS_ZERO_COUNT; i++)
		s->rom[SYNTH_BOSS_ZERO + i] = (next(&b) & 1) ? 0x00 : byte_not(&b, 0x00, 0x00);
}
