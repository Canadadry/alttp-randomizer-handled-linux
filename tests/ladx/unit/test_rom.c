#include "ladx_rom.h"
#include "ladx_sha1.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_rom"

static uint8_t planted[LADX_ROM_SIZE];
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void test_base_check(void)
{
	uint8_t digest[LADX_SHA1_SIZE], want[LADX_SHA1_SIZE];
	size_t i;

	for (i = 0; i < sizeof planted; i++)
		planted[i] = (uint8_t)(i * 13 + (i >> 14));
	ladx_sha1(planted, sizeof planted, want);
	check(ladx_rom_check(planted, sizeof planted, digest) == LADX_ROM_WRONG_SHA1,
	      "a planted 1 MiB buffer is refused as wrong SHA-1");
	check(memcmp(digest, want, sizeof want) == 0, "the refused buffer's digest is its SHA-1");
	ladx_sha1(planted, sizeof planted - 1, want);
	check(ladx_rom_check(planted, sizeof planted - 1, digest) == LADX_ROM_WRONG_SIZE,
	      "1 MiB minus 1 byte is refused as wrong size");
	check(memcmp(digest, want, sizeof want) == 0, "the short buffer's digest is its SHA-1");
	check(strcmp(LADX_ROM_SHA1_HEX, "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a") == 0,
	      "the accepted SHA-1 is the LADX English 1.0 ROM's");
}

static SynthRom synth;
static LadxRomTables tables;
static uint8_t faulty[LADX_ROM_SIZE];

static const char *key_text(const SynthEntry *e)
{
	static char text[16];

	if (e->alt)
		return e->alt;
	sprintf(text, "%x", (unsigned)e->index);
	return text;
}

static void expect_entry(const SynthEntry *want, const LadxRomEntry *got)
{
	const char *table = synth_table_names[want->table];
	const char *key = key_text(want);

	checks++;
	if (!got)
		test_fail(TEST, "%s %s: no entry", table, key);
	if ((want->alt == NULL) != (got->alt == NULL) || (want->alt && strcmp(want->alt, got->alt) != 0) ||
	    (!want->alt && want->index != got->index))
		test_fail(TEST, "%s %s: parsed as another key", table, key);
	if (want->integer != got->integer || (want->integer && want->pointer != got->pointer))
		test_fail(TEST, "%s %s: integer %d pointer %x, built %d %x", table, key, got->integer,
			  (unsigned)got->pointer, want->integer, (unsigned)want->pointer);
	if (!want->integer && (want->start != got->start || want->stop != got->stop))
		test_fail(TEST, "%s %s: span %x..%x, built %x..%x", table, key, (unsigned)got->start,
			  (unsigned)got->stop, (unsigned)want->start, (unsigned)want->stop);
	if (want->bank != got->bank)
		test_fail(TEST, "%s %s: bank %x, built %x", table, key, got->bank, want->bank);
	if (want->pointer_at != got->pointer_at || want->pointer_len != got->pointer_len)
		test_fail(TEST, "%s %s: pointer at %x (%d bytes), built %x (%d)", table, key,
			  (unsigned)got->pointer_at, got->pointer_len, (unsigned)want->pointer_at, want->pointer_len);
	if (want->bank_at != got->bank_at)
		test_fail(TEST, "%s %s: bank byte at %ld, built %ld", table, key, (long)got->bank_at,
			  (long)want->bank_at);
}

static void test_parse(void)
{
	int t, i, integers = 0, alts = 0;

	synth_rom_build(&synth, 0x1A2B3C4Du);
	check(ladx_rom_tables_parse(synth.rom, LADX_ROM_SIZE, &tables) == LADX_ROM_PARSE_OK,
	      "the synthetic ROM parses");
	check(tables.failed_table == -1, "no failed table on success");
	check(tables.entry_count == (uint32_t)synth.entry_count, "as many entries as the builder placed");
	check(tables.table[0].data_bank == -1 && tables.table[0].pointers_bank == 0x1C && tables.table[0].pointers_addr == 1 &&
		      tables.table[1].data_bank == 0x16 && tables.table[3].data_bank == 0x1A &&
		      tables.table[3].pointers_addr == 0x100 && tables.table[8].pointers_addr == 0x31D3,
	      "each table's pointers and data bank, romTables.py's");
	for (t = 0; t < LADX_ROM_TABLES; t++)
		check(tables.table[t].claim_gaps == (t == 7 || t == 8) &&
			      tables.table[t].expand == (t == 6 || t == 9 || t == 10),
		      "claim_storage_gaps on the sprite data, expand_to_end_of_bank on colour dungeon rooms and backgrounds");
	for (t = 0; t < LADX_ROM_TABLES; t++) {
		const LadxRomTable *table = &tables.table[t];

		checks++;
		if (strcmp(table->name, synth_table_names[t]) != 0)
			test_fail(TEST, "table %d is %s, ROMWithTables' %s", t, table->name, synth_table_names[t]);
		if ((int)(table->count + table->alt_count) != synth.table_count[t] ||
		    (int)table->first != synth.table_first[t])
			test_fail(TEST, "%s: %u + %u entries from %u, built %d from %d", table->name,
				  (unsigned)table->count, (unsigned)table->alt_count, (unsigned)table->first,
				  synth.table_count[t], synth.table_first[t]);
		for (i = 0; i < synth.table_count[t]; i++) {
			const SynthEntry *want = &synth.entry[synth.table_first[t] + i];

			expect_entry(want, &tables.entry[table->first + i]);
			expect_entry(want, ladx_rom_entry(&tables, t, want->index, want->alt));
			integers += want->integer;
			alts += want->alt != NULL;
		}
	}
	check(integers > 0 && alts == 7, "integer pointers and the 7 alt keys were checked");
	check(ladx_rom_entry(&tables, 1, 0x320, NULL) == NULL, "no entry past a table's count");
	check(ladx_rom_entry(&tables, 1, 0, "Alt06") == NULL, "no alt key in a table without it");
	check(ladx_rom_entry(&tables, LADX_ROM_TABLES, 0, NULL) == NULL, "no table past the eleventh");
}

static const SynthEntry *first_span(int table)
{
	int i;

	for (i = 0; i < synth.table_count[table]; i++) {
		const SynthEntry *e = &synth.entry[synth.table_first[table] + i];

		if (!e->integer && !e->alt)
			return e;
	}
	test_fail(TEST, "no span entry in table %d", table);
	return NULL;
}

static void plant_at_bank_end(int table, const uint8_t *tail, size_t n)
{
	const SynthEntry *e = first_span(table);
	uint32_t bank_end = (uint32_t)(e->bank + 1) * LADX_ROM_BANK;
	uint16_t pointer = (uint16_t)(0x4000 | ((bank_end - n) % LADX_ROM_BANK));

	memcpy(faulty, synth.rom, LADX_ROM_SIZE);
	memcpy(faulty + bank_end - n, tail, n);
	faulty[e->pointer_at] = (uint8_t)(pointer & 0xFF);
	faulty[e->pointer_at + 1] = (uint8_t)(pointer >> 8);
}

static void expect_fault(LadxRomParseStatus want, int table, const char *what)
{
	LadxRomParseStatus got = ladx_rom_tables_parse(faulty, LADX_ROM_SIZE, &tables);

	checks++;
	if (got != want || tables.failed_table != table)
		test_fail(TEST, "%s: status %d in table %d, expected %d in table %d", what, got,
			  tables.failed_table, want, table);
}

static void test_faults(void)
{
	static const uint8_t text_tail[3] = { 0x10, 0x20, 0x30 };
	static const uint8_t entity_tail[2] = { 0x12, 0x34 };
	static const uint8_t room_tail[4] = { 0x01, 0x02, 0x83, 0x00 };
	static const uint8_t background_tail[5] = { 0x98, 0x00, 0x03, 0x01, 0x02 };
	static const uint8_t room_header[1] = { 0x05 };

	check(ladx_rom_tables_parse(synth.rom, LADX_ROM_SIZE - 1, &tables) == LADX_ROM_PARSE_BAD_SIZE,
	      "a ROM of 1 MiB minus 1 byte is refused as bad size");
	plant_at_bank_end(0, text_tail, sizeof text_tail);
	expect_fault(LADX_ROM_PARSE_PAST_BANK, 0, "a text with no 0xFE or 0xFF before its bank's end");
	plant_at_bank_end(1, entity_tail, sizeof entity_tail);
	expect_fault(LADX_ROM_PARSE_PAST_BANK, 1, "an entity list with no 0xFF before its bank's end");
	plant_at_bank_end(5, room_tail, sizeof room_tail);
	expect_fault(LADX_ROM_PARSE_PAST_BANK, 5, "a room whose object runs past its bank's end");
	plant_at_bank_end(5, room_header, sizeof room_header);
	expect_fault(LADX_ROM_PARSE_PAST_BANK, 5, "a room whose header runs past its bank's end");
	plant_at_bank_end(9, background_tail, sizeof background_tail);
	expect_fault(LADX_ROM_PARSE_PAST_BANK, 9, "a background run whose values pass its bank's end");
	memcpy(faulty, synth.rom, LADX_ROM_SIZE);
	faulty[0x31DD] = 0x34;
	faulty[0x31DE] = 0x12;
	expect_fault(LADX_ROM_PARSE_ALT_POINTER, 3, "the Alt8C pointer 0x1234");
	faulty[0x31DE] = 0x80;
	expect_fault(LADX_ROM_PARSE_ALT_POINTER, 3, "the Alt8C pointer 0x8034");
}

int main(void)
{
	test_base_check();
	test_parse();
	test_faults();
	printf("test_rom: base check, the tables of a synthetic ROM parsed as built, faults refused (%d checks)\n",
	       checks);
	return 0;
}
