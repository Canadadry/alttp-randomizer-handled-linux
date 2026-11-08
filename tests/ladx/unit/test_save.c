#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_save"
#define OUT_MAX (4 << 20)
#define TEXTS 0
#define ENTITIES 1
#define OW_TOP 2
#define SPRITE_OW 7
#define SPRITE_INDOOR 8
#define BG_TILES 9
#define BASE_LOAD 0x0DFAu
#define BANK(b) ((uint32_t)(b) * SYNTH_ROM_BANK)

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

static SynthRom synth;
static LadxRomTables rom_tables, out_tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static uint8_t image[SYNTH_ROM_SIZE];
static char out[OUT_MAX];
static size_t out_len;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void sink(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	if (out_len + len >= OUT_MAX)
		test_fail(TEST, "log text over %d bytes", OUT_MAX);
	memcpy(out + out_len, text, len);
	out_len += len;
	out[out_len] = '\0';
}

static LadxSpan span(const char *s)
{
	LadxSpan sp;

	sp.at = s;
	sp.len = strlen(s);
	return sp;
}

static LadxWlogKey key(uint32_t index)
{
	LadxWlogKey k;

	memset(&k, 0, sizeof k);
	k.index = index;
	return k;
}

static void begin(const uint8_t *base_load)
{
	synth_rom_build(&synth, 0x5A7E01u);
	memcpy(synth.rom + BASE_LOAD, base_load, 3);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM parses");
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = NULL;
	memset(&pipeline, 0, sizeof pipeline);
	pipeline.rom = &rom;
}

static void set_entry(int table, uint32_t index, const uint8_t *bytes, size_t n);

/* Sprite data as updateSpriteData leaves it, a few distinct values: 544 distinct indoor ones have no index byte. */
static void few_sprite_values(void)
{
	uint8_t v[4];
	uint32_t k;

	for (k = 0; k < 0x100 + 0x220; k++) {
		v[0] = (uint8_t)(k % 7);
		v[1] = (uint8_t)(k % 5);
		v[2] = 0xFF;
		v[3] = (uint8_t)(k % 3);
		set_entry(k < 0x100 ? SPRITE_OW : SPRITE_INDOOR, k < 0x100 ? k : k - 0x100, v, 4);
	}
}

static const uint8_t ld_hl_old[3] = {0x21, 0x3B, 0x76};

static void set_entry(int table, uint32_t index, const uint8_t *bytes, size_t n)
{
	int32_t origins[256];
	size_t i;

	if (n > 256)
		test_fail(TEST, "a planted entry over 256 bytes");
	for (i = 0; i < n; i++)
		origins[i] = LADX_ENC_KNOWN;
	if (ladx_enc_table(&enc, span("planted"), table, key(index), bytes, origins, n) != LADX_ENC_OK)
		test_fail(TEST, "a planted entry write");
}

static void build_image(void)
{
	uint32_t q;
	int32_t origin;

	for (q = 0; q < SYNTH_ROM_SIZE; q++)
		if (!ladx_enc_bank_value(&enc, q, &image[q], &origin))
			test_fail(TEST, "position %x has no value", (unsigned)q);
}

static uint32_t base_load(void)
{
	return (uint32_t)(image[BASE_LOAD + 1] | image[BASE_LOAD + 2] << 8);
}

static char mismatch[96];

static int differs(int t, uint32_t k, const char *why)
{
	snprintf(mismatch, sizeof mismatch, "table %d entry %lu: %s", t, (unsigned long)k, why);
	return 0;
}

static int image_holds_entries(void)
{
	static uint8_t v[LADX_WLOG_ATOM_MAX];
	static int32_t o[LADX_WLOG_ATOM_MAX];
	int t;
	uint32_t k, value;

	if (image[BASE_LOAD] != 0x21 || ladx_rom_tables_parse(image, SYNTH_ROM_SIZE, &out_tables) != LADX_ROM_PARSE_OK)
		return differs(-1, 0, "the image does not parse");
	for (t = 0; t < LADX_ROM_TABLES; t++) {
		const LadxRomTable *table = &out_tables.table[t];

		for (k = 0; k < table->count + table->alt_count; k++) {
			const LadxRomEntry *e = &out_tables.entry[table->first + k];
			LadxWlogKey kk = key(e->alt ? 0 : e->index);
			size_t n;
			uint32_t start = e->start;
			int kind;

			kk.alt = e->alt ? span(e->alt) : kk.alt;
			kind = ladx_enc_entry_pointer(&enc, t, kk, &value);
			if (kind == 1) {
				/* store writes an integer's high byte | 0x40, as LADXR. */
				if (image[e->pointer_at] != (value & 0xFF) || image[e->pointer_at + 1] != (((value >> 8) & 0xFF) | 0x40))
					return differs(t, k, "integer pointer");
				continue;
			}
			if (kind != 0 || !ladx_enc_entry_value(&enc, t, kk, v, o, sizeof v, &n))
				return differs(t, k, "no value");
			if (t == SPRITE_INDOOR)
				start = BANK(0x20) + (base_load() & 0x3FFF) + 4u * image[e->pointer_at];
			if (e->integer)
				return differs(t, k, "an integer in the image");
			if (t != SPRITE_INDOOR && n != e->stop - e->start)
				return differs(t, k, "another length");
			if (memcmp(image + start, v, n) != 0)
				return differs(t, k, "other bytes");
		}
	}
	return 1;
}

static uint16_t pointer_of(int table, uint32_t index)
{
	const LadxRomEntry *e = ladx_rom_entry(&rom_tables, table, index, NULL);

	return (uint16_t)(image[e->pointer_at] | image[e->pointer_at + 1] << 8);
}

/* 544 distinct indoor sprite values: the index byte's ValueError. */
static void test_byte_range(void)
{
	begin(ld_hl_old);
	check(!ladx_group_save(&enc, &pipeline) && strcmp(pipeline.raised.type, "ValueError") == 0 &&
		      strcmp(pipeline.raised.message, "byte must be in range(0, 256)") == 0,
	      "distinct sprite data past index 255: ValueError");
}

static void test_unchanged(void)
{
	begin(ld_hl_old);
	few_sprite_values();
	if (!ladx_group_save(&enc, &pipeline))
		test_fail(TEST, "few values: %s %s", pipeline.raised.type ? pipeline.raised.type : "-", pipeline.raised.message ? pipeline.raised.message : "-");
	check(ladx_enc_end(&enc) == LADX_ENC_OK, "few values: the log ends");
	build_image();
	if (!image_holds_entries())
		test_fail(TEST, "few values: %s", mismatch);
	checks++;
	check((base_load() & 0x3FFF) >= 0x33F3 && (base_load() & 0xC000) == 0x4000,
	      "the indoor sprite data base is the overworld leftover's start | 0x4000");
	check(strstr(out, "G save\n") != NULL, "one save segment");
}

static void test_changed(void)
{
	static const uint8_t grown[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF};
	static const uint8_t tail[] = {0x16, 0x17, 0x18, 0x19, 0xFF};
	static const uint8_t ow[4] = {1, 2, 3, 4}, indoor[4] = {5, 6, 7, 8};
	uint8_t v[256];
	int32_t o[256];
	size_t n;
	uint32_t twin, sibling;

	begin(ld_hl_old);
	few_sprite_values();
	set_entry(TEXTS, 5, grown, sizeof grown);
	for (sibling = 6; ladx_rom_entry(&rom_tables, TEXTS, sibling, NULL)->integer ||
			  ladx_rom_entry(&rom_tables, TEXTS, sibling, NULL)->bank != ladx_rom_entry(&rom_tables, TEXTS, 5, NULL)->bank;
	     sibling++)
		;
	set_entry(TEXTS, sibling, tail, sizeof tail);
	for (twin = 7; twin < 0x320 && !ladx_enc_entry_value(&enc, ENTITIES, key(twin), v, o, sizeof v, &n); twin++)
		;
	check(twin < 0x320, "an entity with bytes");
	set_entry(ENTITIES, 3, v, n);
	set_entry(SPRITE_OW, 0, ow, 4);
	set_entry(SPRITE_OW, 1, ow, 4);
	set_entry(SPRITE_INDOOR, 2, indoor, 4);
	check(ladx_enc_pointer(&enc, span("planted"), TEXTS, key(9), 0x8123) == LADX_ENC_OK, "an integer pointer");
	check(ladx_enc_entry_value(&enc, BG_TILES, key(3), v, o, sizeof v, &n) && n > 1, "background 3 read");
	v[0] = 0x00;
	set_entry(BG_TILES, 3, v, 1);
	check(ladx_group_save(&enc, &pipeline), "changed tables: saved");
	check(ladx_enc_end(&enc) == LADX_ENC_OK, "changed tables: the log ends");
	build_image();
	if (!image_holds_entries())
		test_fail(TEST, "changed tables: %s", mismatch);
	checks++;
	check(pointer_of(TEXTS, sibling) == pointer_of(TEXTS, 5) + sizeof grown - sizeof tail,
	      "a text equal to a stored text's tail points inside it");
	check(pointer_of(ENTITIES, 3) == pointer_of(ENTITIES, twin), "an equal entity is stored once");
	{
		const LadxRomEntry *e = ladx_rom_entry(&rom_tables, ENTITIES, 0x10, NULL);
		uint32_t moved = BANK(e->bank) + (pointer_of(ENTITIES, 0x10) & 0x3FFFu);

		check(!e->integer && moved != e->start && ladx_enc_bank_origin(&enc, moved) == (int32_t)e->start &&
			      ladx_enc_bank_origin(&enc, moved + 1) == (int32_t)e->start + 1,
		      "a moved source entry's bytes carry their source offsets");
	}
	check(image[ladx_rom_entry(&rom_tables, SPRITE_OW, 0, NULL)->pointer_at] ==
		      image[ladx_rom_entry(&rom_tables, SPRITE_OW, 1, NULL)->pointer_at],
	      "equal sprite data share an index");
}

static void test_patch_mismatch(void)
{
	static const uint8_t other[3] = {0x21, 0x00, 0x40};

	begin(other);
	few_sprite_values();
	check(!ladx_group_save(&enc, &pipeline) && pipeline.raised.kind == LADX_OUTCOME_CRASH &&
		      strcmp(pipeline.raised.type, "AssertionError") == 0 &&
		      strncmp(pipeline.raised.message, "Patch mismatch:\n", 16) == 0,
	      "other bytes at 0x0DFA: ROM.patch's AssertionError");
}

static void test_no_room(void)
{
	static uint8_t big[0x3000];

	begin(ld_hl_old);
	few_sprite_values();
	memset(big, 0x55, sizeof big);
	big[sizeof big - 1] = 0xFF;
	{
		static int32_t o[sizeof big];
		size_t i;

		for (i = 0; i < sizeof big; i++)
			o[i] = LADX_ENC_KNOWN;
		check(ladx_enc_table(&enc, span("planted"), ENTITIES, key(0), big, o, sizeof big) == LADX_ENC_OK,
		      "a huge entity");
	}
	check(!ladx_group_save(&enc, &pipeline) && pipeline.raised.kind == LADX_OUTCOME_CRASH &&
		      strcmp(pipeline.raised.type, "AssertionError") == 0 &&
		      strncmp(pipeline.raised.message, "Not enough room in storage... ", 30) == 0 &&
		      strstr(pipeline.raised.message, "] id:0(0) bank:22") != NULL,
	      "an entity over its bank: store's AssertionError with LADXR's message");
}

static void test_registry(void)
{
	const LadxPipelineGroup *groups;
	int count;

	groups = ladx_pipeline_groups(&count);
	check(count > 3 && strcmp(groups[count - 3].name, "save") == 0 && strcmp(groups[count - 4].name, "sprites") == 0 &&
		      strcmp(groups[count - 2].name, "spoiler") == 0 && strcmp(groups[count - 1].name, "header") == 0,
	      "save registered after sprites, before spoiler and header");
	check(groups[count - 2].needs_rom && groups[count - 2].draw == NULL, "a rom-group with no draw");
	memset(&pipeline, 0, sizeof pipeline);
	check(!ladx_group_save(&enc, &pipeline), "no ROM: the group fails");
}

int main(void)
{
	test_registry();
	test_byte_range();
	test_unchanged();
	test_changed();
	test_patch_mismatch();
	test_no_room();
	printf("%s: ROMWithTables.save's stores on a synthetic ROM, read back (%d checks)\n", TEST, checks);
	return 0;
}
