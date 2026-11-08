#include "ladx_rom.h"

LadxRomCheck ladx_rom_check(const uint8_t *bytes, size_t len, uint8_t digest[LADX_SHA1_SIZE])
{
	static const char hex[] = "0123456789abcdef";
	static const char *want = LADX_ROM_SHA1_HEX;
	int i;

	ladx_sha1(bytes, len, digest);
	if (len != LADX_ROM_SIZE)
		return LADX_ROM_WRONG_SIZE;
	for (i = 0; i < LADX_SHA1_SIZE; i++)
		if (want[2 * i] != hex[digest[i] >> 4] || want[2 * i + 1] != hex[digest[i] & 0x0F])
			return LADX_ROM_WRONG_SHA1;
	return LADX_ROM_OK;
}

#define ALT_MAX 5

typedef struct Layout {
	const char *name;
	LadxRomKind kind;
	uint32_t count, pointers_bank, pointers_addr;
	int data_bank; /* -1: the banks table gives each entry's bank */
	uint32_t banks_bank, banks_addr;
	uint32_t data_addr, data_size;
	int alt_count;
	const char *alt[ALT_MAX];
	uint32_t alt_bank[ALT_MAX], alt_addr[ALT_MAX];
	int claim_gaps, expand;
} Layout;

/* romTables.py at the pin; alt keys in their dict order. */
static const Layout layout[LADX_ROM_TABLES] = {
	{ "texts", LADX_ROM_KIND_TEXT, 0x2B0, 0x1C, 1, -1, 0x1C, 0x741, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0 },
	{ "entities", LADX_ROM_KIND_DATA, 0x320, 0x16, 0, 0x16, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0 },
	{ "rooms_overworld_top", LADX_ROM_KIND_ROOMS, 0x080, 0x09, 0x000, 0x09, 0, 0, 0, 0, 5,
	  { "Alt06", "Alt0E", "Alt1B", "Alt2B", "Alt79" }, { 0, 0, 0, 0, 0 }, { 0x31FD, 0x31CD, 0x320D, 0x321D, 0x31ED }, 0, 0 },
	{ "rooms_overworld_bottom", LADX_ROM_KIND_ROOMS, 0x080, 0x09, 0x100, 0x1A, 0, 0, 0, 0, 1, { "Alt8C" }, { 0 },
	  { 0x31DD }, 0, 0 },
	{ "rooms_indoor_a", LADX_ROM_KIND_ROOMS, 0x100, 0x0A, 0x000, 0x0A, 0, 0, 0, 0, 1, { "Alt1F5" }, { 0 },
	  { 0x31A1 }, 0, 0 },
	{ "rooms_indoor_b", LADX_ROM_KIND_ROOMS, 0x0FF, 0x0B, 0x000, 0x0B, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 0 },
	{ "rooms_color_dungeon", LADX_ROM_KIND_ROOMS, 0x016, 0x0A, 0x3B77, 0x0A, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, 0, 1 },
	{ "room_sprite_data_overworld", LADX_ROM_KIND_SPRITE, 0x100, 0x20, 0x30D3, 0x20, 0, 0, 0x33F3, 4, 0, { 0 },
	  { 0 }, { 0 }, 1, 0 },
	{ "room_sprite_data_indoor", LADX_ROM_KIND_SPRITE, 0x220, 0x20, 0x31D3, 0x20, 0, 0, 0x363B, 4, 0, { 0 },
	  { 0 }, { 0 }, 1, 0 },
	{ "background_tiles", LADX_ROM_KIND_BACKGROUND, 0x26, 0x20, 0x052B, 0x08, 0, 0, 0, 0, 0, { 0 }, { 0 },
	  { 0 }, 0, 1 },
	{ "background_attributes", LADX_ROM_KIND_BACKGROUND, 0x26, 0x24, 0x1C4B, 0x24, 0, 0, 0, 0, 0, { 0 }, { 0 },
	  { 0 }, 0, 1 },
};

/* A bank's byte at addr, -1 past its end (Python's IndexError). */
static int at(const uint8_t *bank, uint32_t addr)
{
	return addr < LADX_ROM_BANK ? bank[addr] : -1;
}

/* The end of the entry at addr in bank, as the table's _readData finds it; 0 past the bank's end. */
static uint32_t entry_end(const Layout *l, const uint8_t *bank, uint32_t addr)
{
	uint32_t p = addr;
	int v;

	switch (l->kind) {
	case LADX_ROM_KIND_SPRITE:
		p += l->data_size;
		return p <= LADX_ROM_BANK ? p : 0;
	case LADX_ROM_KIND_TEXT:
	case LADX_ROM_KIND_DATA:
		for (;;) {
			v = at(bank, p);
			if (v < 0)
				return 0;
			if (v == 0xFF || (v == 0xFE && l->kind == LADX_ROM_KIND_TEXT))
				return p + 1;
			p++;
		}
	case LADX_ROM_KIND_ROOMS:
		p += 2;
		while ((v = at(bank, p)) != 0xFE) {
			if (v < 0)
				return 0;
			if ((v & 0xF0) == 0xE0)
				p += 5;
			else if ((v & 0xF0) == 0xC0 || (v & 0xF0) == 0x80)
				p += 3;
			else
				p += 2;
		}
		return p + 1;
	default:
		while ((v = at(bank, p)) != 0x00) {
			int flags = at(bank, p + 2);

			if (v < 0 || at(bank, p + 1) < 0 || flags < 0)
				return 0;
			p += 3 + ((flags & 0x40) ? 1u : (uint32_t)(flags & 0x3F) + 1);
		}
		return p + 1;
	}
}

static uint16_t le16(const uint8_t *rom, uint32_t at)
{
	return (uint16_t)(rom[at] | rom[at + 1] << 8);
}

/* Reads the entry at pointer (inside 0x4000..0x7FFF) of bank; 0 past the bank's end. */
static int read_entry(const uint8_t *rom, const Layout *l, LadxRomEntry *e, uint16_t pointer)
{
	uint32_t addr = pointer & 0x3FFFu, end = entry_end(l, rom + e->bank * LADX_ROM_BANK, addr);

	if (end == 0)
		return 0;
	e->start = e->bank * LADX_ROM_BANK + addr;
	e->stop = e->bank * LADX_ROM_BANK + end;
	return 1;
}

static void clear(LadxRomEntry *e)
{
	e->index = 0;
	e->alt = NULL;
	e->integer = 0;
	e->pointer = 0;
	e->start = e->stop = 0;
	e->bank = 0;
	e->pointer_at = 0;
	e->pointer_len = 2;
	e->bank_at = -1;
}

static LadxRomParseStatus parse_table(const uint8_t *rom, int t, LadxRomTables *out)
{
	const Layout *l = &layout[t];
	LadxRomTable *table = &out->table[t];
	uint32_t pointers = l->pointers_bank * LADX_ROM_BANK + l->pointers_addr, n;
	int a;

	table->name = l->name;
	table->kind = l->kind;
	table->data_addr = l->data_addr;
	table->data_size = l->data_size;
	table->pointers_bank = (uint8_t)l->pointers_bank;
	table->pointers_addr = l->pointers_addr;
	table->data_bank = l->data_bank;
	table->claim_gaps = l->claim_gaps;
	table->expand = l->expand;
	table->count = l->count;
	table->alt_count = (uint32_t)l->alt_count;
	table->first = out->entry_count;
	out->entry_count += l->count + (uint32_t)l->alt_count;
	for (a = 0; a < l->alt_count; a++) {
		LadxRomEntry *e = &out->entry[table->first + l->count + (uint32_t)a];
		uint16_t pointer;

		clear(e);
		e->alt = l->alt[a];
		e->bank = (uint8_t)l->data_bank;
		e->pointer_at = l->alt_bank[a] * LADX_ROM_BANK + l->alt_addr[a];
		pointer = le16(rom, e->pointer_at);
		out->failed = e;
		if (pointer < 0x4000 || pointer >= 0x8000)
			return LADX_ROM_PARSE_ALT_POINTER;
		if (!read_entry(rom, l, e, pointer))
			return LADX_ROM_PARSE_PAST_BANK;
	}
	for (n = 0; n < l->count; n++) {
		LadxRomEntry *e = &out->entry[table->first + n];
		uint32_t pointer;

		clear(e);
		e->index = n;
		if (l->data_size) {
			e->pointer_at = pointers + n;
			e->pointer_len = 1;
			pointer = l->data_addr + 0x4000u + rom[e->pointer_at] * l->data_size;
		} else {
			e->pointer_at = pointers + 2 * n;
			pointer = le16(rom, e->pointer_at);
		}
		if (l->data_bank >= 0) {
			e->bank = (uint8_t)l->data_bank;
		} else {
			e->bank_at = (int32_t)(l->banks_bank * LADX_ROM_BANK + l->banks_addr + n);
			e->bank = rom[e->bank_at] & 0x3F;
		}
		out->failed = e;
		if (pointer < 0x4000 || pointer >= 0x8000) {
			e->integer = 1;
			e->pointer = (uint16_t)pointer;
		} else if (!read_entry(rom, l, e, (uint16_t)pointer)) {
			return LADX_ROM_PARSE_PAST_BANK;
		}
	}
	out->failed = NULL;
	return LADX_ROM_PARSE_OK;
}

LadxRomParseStatus ladx_rom_tables_parse(const uint8_t *rom, size_t len, LadxRomTables *out)
{
	LadxRomParseStatus status;
	int t;

	out->entry_count = 0;
	out->failed = NULL;
	out->failed_table = -1;
	if (len != LADX_ROM_SIZE)
		return LADX_ROM_PARSE_BAD_SIZE;
	for (t = 0; t < LADX_ROM_TABLES; t++) {
		status = parse_table(rom, t, out);
		if (status != LADX_ROM_PARSE_OK) {
			out->failed_table = t;
			return status;
		}
	}
	return LADX_ROM_PARSE_OK;
}

static int same_text(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

const LadxRomEntry *ladx_rom_entry(const LadxRomTables *tables, int table, uint32_t index, const char *alt)
{
	const LadxRomTable *t;
	uint32_t a;

	if (table < 0 || table >= LADX_ROM_TABLES)
		return NULL;
	t = &tables->table[table];
	if (!alt)
		return index < t->count ? &tables->entry[t->first + index] : NULL;
	for (a = 0; a < t->alt_count; a++)
		if (same_text(tables->entry[t->first + t->count + a].alt, alt))
			return &tables->entry[t->first + t->count + a];
	return NULL;
}

const char *ladx_rom_table_name(int table)
{
	return table >= 0 && table < LADX_ROM_TABLES ? layout[table].name : NULL;
}
