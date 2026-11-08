#ifndef LADX_ROM_H
#define LADX_ROM_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_sha1.h"

#define LADX_ROM_SIZE 0x100000u
#define LADX_ROM_BANK 0x4000u
#define LADX_ROM_SHA1_HEX "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"

typedef enum LadxRomCheck {
	LADX_ROM_OK = 0,
	LADX_ROM_WRONG_SIZE,
	LADX_ROM_WRONG_SHA1
} LadxRomCheck;

LadxRomCheck ladx_rom_check(const uint8_t *bytes, size_t len, uint8_t digest[LADX_SHA1_SIZE]);

#define LADX_ROM_TABLES 11
#define LADX_ROM_ENTRY_MAX 3200

typedef enum LadxRomKind {
	LADX_ROM_KIND_TEXT = 0,
	LADX_ROM_KIND_DATA,
	LADX_ROM_KIND_ROOMS,
	LADX_ROM_KIND_SPRITE,
	LADX_ROM_KIND_BACKGROUND
} LadxRomKind;

typedef struct LadxRomEntry {
	uint32_t index;
	const char *alt;
	uint8_t integer;
	uint16_t pointer;
	uint32_t start, stop;
	uint8_t bank;
	uint32_t pointer_at;
	uint8_t pointer_len;
	int32_t bank_at;
} LadxRomEntry;

typedef struct LadxRomTable {
	const char *name;
	LadxRomKind kind;
	uint32_t data_addr, data_size;
	uint8_t pointers_bank;
	uint32_t pointers_addr;
	int data_bank;
	int claim_gaps;
	int expand;
	uint32_t count;
	uint32_t alt_count;
	uint32_t first;
} LadxRomTable;

typedef enum LadxRomParseStatus {
	LADX_ROM_PARSE_OK = 0,
	LADX_ROM_PARSE_BAD_SIZE,
	LADX_ROM_PARSE_PAST_BANK,
	LADX_ROM_PARSE_ALT_POINTER
} LadxRomParseStatus;

typedef struct LadxRomTables {
	LadxRomTable table[LADX_ROM_TABLES];
	LadxRomEntry entry[LADX_ROM_ENTRY_MAX];
	uint32_t entry_count;
	int failed_table;
	const LadxRomEntry *failed;
} LadxRomTables;

LadxRomParseStatus ladx_rom_tables_parse(const uint8_t *rom, size_t len, LadxRomTables *out);

const char *ladx_rom_table_name(int table);

const LadxRomEntry *ladx_rom_entry(const LadxRomTables *tables, int table, uint32_t index, const char *alt);

#endif
