#ifndef LADX_ROM_H
#define LADX_ROM_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_sha1.h"

/* The source ROM, the LADX English 1.0 ROM, read from caller-owned bytes. */
#define LADX_ROM_SIZE 0x100000u
#define LADX_ROM_BANK 0x4000u
#define LADX_ROM_SHA1_HEX "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"

typedef enum LadxRomCheck {
	LADX_ROM_OK = 0,
	LADX_ROM_WRONG_SIZE, /* not exactly LADX_ROM_SIZE bytes */
	LADX_ROM_WRONG_SHA1
} LadxRomCheck;

/* digest always receives the bytes' SHA-1, for the caller to print. */
LadxRomCheck ladx_rom_check(const uint8_t *bytes, size_t len, uint8_t digest[LADX_SHA1_SIZE]);

/*
 * The eleven tables of LADXR's ROMWithTables (romTables.py at the pin),
 * parsed from the source ROM as PointerTable parses them: each entry's
 * absolute source span or integer pointer, its bank and its pointer's
 * place. Storage regions are not computed.
 */
#define LADX_ROM_TABLES 11
#define LADX_ROM_ENTRY_MAX 3200 /* LADXR's layout has 3160 */

typedef enum LadxRomKind {
	LADX_ROM_KIND_TEXT = 0,	 /* until 0xFE or 0xFF */
	LADX_ROM_KIND_DATA,	 /* entities: until 0xFF */
	LADX_ROM_KIND_ROOMS,	 /* a 2-byte header, objects, 0xFE */
	LADX_ROM_KIND_SPRITE,	 /* data_size bytes */
	LADX_ROM_KIND_BACKGROUND /* runs until 0x00 */
} LadxRomKind;

typedef struct LadxRomEntry {
	uint32_t index;	 /* the index key, 0 for an alt key */
	const char *alt; /* NULL for an index key, else the alt key's name (static) */
	uint8_t integer; /* 1: an integer pointer (outside 0x4000..0x7FFF), no data read */
	uint16_t pointer; /* the integer pointer */
	uint32_t start, stop; /* the absolute source span, when not an integer */
	uint8_t bank;	      /* the data bank (a banks-table byte & 0x3F) */
	uint32_t pointer_at;  /* absolute offset of the pointer bytes */
	uint8_t pointer_len;  /* 2, or 1 for the sprite data tables */
	int32_t bank_at;      /* absolute offset of its banks-table byte, -1 for none */
} LadxRomEntry;

typedef struct LadxRomTable {
	const char *name; /* ROMWithTables' attribute, as the write log's tables line */
	LadxRomKind kind;
	uint32_t data_addr, data_size; /* the sprite tables' info, 0 elsewhere */
	uint32_t count;	    /* index keys */
	uint32_t alt_count; /* alt keys, after the index keys, in LADXR's dict order */
	uint32_t first;	    /* the table's first entry in LadxRomTables.entry */
} LadxRomTable;

typedef enum LadxRomParseStatus {
	LADX_ROM_PARSE_OK = 0,
	LADX_ROM_PARSE_BAD_SIZE,   /* not LADX_ROM_SIZE bytes */
	LADX_ROM_PARSE_PAST_BANK,  /* an entry reaches the end of its bank (LADXR's IndexError) */
	LADX_ROM_PARSE_ALT_POINTER /* an alt pointer outside 0x4000..0x7FFF (LADXR's assert) */
} LadxRomParseStatus;

typedef struct LadxRomTables {
	LadxRomTable table[LADX_ROM_TABLES]; /* in ROMWithTables' order */
	LadxRomEntry entry[LADX_ROM_ENTRY_MAX];
	uint32_t entry_count;
	int failed_table;	/* on an error: the table, -1 on success */
	const LadxRomEntry *failed; /* the entry being read */
} LadxRomTables;

/* rom outlives nothing: every span is an offset into it. */
LadxRomParseStatus ladx_rom_tables_parse(const uint8_t *rom, size_t len, LadxRomTables *out);

/* ROMWithTables' attribute name of table (0..LADX_ROM_TABLES-1), NULL out of range. */
const char *ladx_rom_table_name(int table);

/* The entry of table for an index key (alt NULL) or an alt key; NULL when none. */
const LadxRomEntry *ladx_rom_entry(const LadxRomTables *tables, int table, uint32_t index, const char *alt);

#endif
