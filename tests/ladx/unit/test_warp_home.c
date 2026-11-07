#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_warp_home"
#define OUT_MAX 262144
#define ENTITIES 1
#define INDOOR_B 5
#define BG_TILES 9
#define BANK(b) ((uint32_t)(b) * SYNTH_ROM_BANK)
#define OPERAND(k) (BANK(1) + 0x3E39u + 5u * (uint32_t)(k))
#define ROW_COUNT 4
#define ROW_LEN 20

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

static SynthRom synth;
static LadxRomTables rom_tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxRoom room;
static LadxBackground bg;
static LadxOutcome raised;
static char out[OUT_MAX];
static size_t out_len;
static int checks;

/* The rows warpHome reads, each ROW_LEN tiles from column 0. */
static const uint32_t rows[ROW_COUNT] = {0x98E0, 0x9940, 0x9960, 0x9980};

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

static uint8_t tile(int row, uint32_t n)
{
	return (uint8_t)(0x21 + 0x17 * row + 3 * n);
}

/* The planted background's runs: rows 0..rows_n-1, ROW_LEN tiles each, then the terminator. */
static size_t planted_background(uint8_t *raw, int rows_n)
{
	size_t len = 0;
	uint32_t n;
	int r;

	for (r = 0; r < rows_n; r++) {
		raw[len++] = (uint8_t)(rows[r] >> 8);
		raw[len++] = (uint8_t)(rows[r] & 0xFF);
		raw[len++] = ROW_LEN - 1;
		for (n = 0; n < ROW_LEN; n++)
			raw[len++] = tile(r, n);
	}
	raw[len++] = 0x00;
	return len;
}

static int room_index(const char *name)
{
	const LadxEntranceInfo *info;
	size_t count, i;

	info = ladx_entrance_info(&count);
	for (i = 0; i < count; i++)
		if (strcmp(info[i].name, name) == 0)
			return info[i].room;
	test_fail(TEST, "no entrance %s", name);
	return -1;
}

static void one_warp(int type, int map, int nr, int x, int y)
{
	memset(&room, 0, sizeof room);
	room.objects[0].kind = LADX_ROOM_SINGLE;
	room.objects[0].type_id = 0x11;
	ladx_room_warp(&room.objects[1], type, map, nr, x, y);
	ladx_room_warp(&room.objects[2], 0, 0, 0x33, 0x01, 0x02);
	room.object_count = 3;
}

static int target_is(int force, int t, int map, int nr, int x, int y)
{
	uint8_t got[LADX_WARP_HOME_OPERANDS];

	return ladx_warp_home_target(&room, force, got, &raised) == LADX_WARP_HOME_OK && got[0] == t &&
	       got[1] == map && got[2] == nr && got[3] == x && got[4] == y;
}

static int raises(const char *type, const char *message)
{
	uint8_t got[LADX_WARP_HOME_OPERANDS];

	memset(&raised, 0, sizeof raised);
	return ladx_warp_home_target(&room, 0, got, &raised) == LADX_WARP_HOME_RAISED &&
	       raised.kind == LADX_OUTCOME_CRASH && strcmp(raised.type, type) == 0 &&
	       strcmp(raised.message, message) == 0;
}

static void test_target(void)
{
	one_warp(0, 0x00, 0x31, 0x44, 0x66);
	check(target_is(0, 0x00, 0x00, 0x31, 0x44, 0x66), "an overworld warp: its room and target, the first warp");
	check(target_is(1, 0x01, 0x10, 0xA3, 0x50, 0x7F), "force_inside: the indoor fallback");
	one_warp(0, 0x00, room_index("d4"), 0x44, 0x66);
	check(target_is(0, 0x01, 0x10, 0xA3, 0x50, 0x7F), "a one-way entrance's room: the fallback");
	one_warp(0, 0x00, room_index("forest_madbatter"), 0x44, 0x66);
	check(target_is(0, 0x01, 0x10, 0xA3, 0x50, 0x7F), "the last one-way name: the fallback");
	one_warp(0, 0x00, room_index("d2"), 0x44, 0x66);
	check(target_is(0, 0x00, 0x00, (uint8_t)room_index("d2"), 0x44, 0x66), "D2 is not one-way");
	one_warp(1, 0x07, 0x10, 0x44, 0x66);
	check(target_is(0, 0x01, 0x10, 0xA3, 0x50, 0x7F), "a room over 0x100 (indoor B): the fallback");
	one_warp(1, 0x00, 0x00, 0x44, 0x66);
	check(raises("AssemblerException", "([NUMBER:256:21], '8 bit value out of range')"),
	      "room 0x100 overflows LADXR's 8-bit operand");
	one_warp(0, 0, 0x31, 0, 0);
	room.object_count = 1;
	check(raises("IndexError", "list index out of range"), "no warp: IndexError");
}

static void decode_planted(int rows_n)
{
	uint8_t raw[ROW_COUNT * (3 + ROW_LEN) + 1];
	size_t n = planted_background(raw, rows_n);

	if (!ladx_background_decode(&bg, raw, n))
		test_fail(TEST, "the planted background decodes");
}

static int edited_as_ladxr(const LadxBackground *b)
{
	static const uint8_t option[5] = {0xCE, 0xCF, 0xC4, 0x7F, 0x7F};
	uint8_t v;
	uint32_t n;

	for (n = 0; n < ROW_LEN; n++) {
		int moved = n >= 2 && n < 18;

		if (!ladx_background_get(b, 0x98E0 + n, &v) || v != tile(0, n) || !ladx_background_get(b, 0x9940 + n, &v) ||
		    v != tile(1, n))
			return 0;
		if (!ladx_background_get(b, 0x9960 + n, &v) ||
		    v != (n >= 10 && n < 15 ? option[n - 10] : moved ? tile(0, n) : tile(2, n)))
			return 0;
		if (!ladx_background_get(b, 0x9980 + n, &v) || v != (moved ? tile(1, n) : tile(3, n)))
			return 0;
		if (ladx_background_get(b, 0x99A0 + n, &v) != moved || (moved && v != tile(2, n)))
			return 0;
		if (ladx_background_get(b, 0x99C0 + n, &v) != moved || (moved && v != tile(3, n)))
			return 0;
	}
	return b->count == ROW_COUNT * ROW_LEN + 2 * 16;
}

static void test_edit(void)
{
	decode_planted(ROW_COUNT);
	check(ladx_warp_home_edit(&bg, &raised) == LADX_WARP_HOME_OK && edited_as_ladxr(&bg),
	      "rows moved down for columns 2..17, the third option at 0x996A..0x996E");
	decode_planted(ROW_COUNT - 1);
	memset(&raised, 0, sizeof raised);
	check(ladx_warp_home_edit(&bg, &raised) == LADX_WARP_HOME_RAISED && strcmp(raised.type, "KeyError") == 0 &&
		      strcmp(raised.message, "39298") == 0,
	      "a missing row 0x9980: KeyError on its first read, 0x9982");
}

static void plant_key(int table, uint32_t index, const uint8_t *bytes, size_t n)
{
	int32_t known[256];
	LadxWlogKey key;
	size_t i;

	for (i = 0; i < n; i++)
		known[i] = LADX_ENC_KNOWN;
	memset(&key, 0, sizeof key);
	key.index = index;
	if (ladx_enc_table(&enc, span("plant"), table, key, bytes, known, n) != LADX_ENC_OK)
		test_fail(TEST, "planting an entry");
}

#define WARP(t, map, lo, x, y) (0xE0 | (t)), (map), (lo), (x), (y)

/* The start house: a kept object then a warp, no entity; background 0x0D planted with rows_n rows. */
static void plant_world(int with_warp, int rows_n)
{
	static const uint8_t objects[] = {0x01, 0x32, 0x11, 0x50, WARP(0, 0x00, 0x31, 0x44, 0x66), 0xFE};
	static const uint8_t no_warp[] = {0x01, 0x32, 0x11, 0x50, 0xFE};
	static const uint8_t entities[] = {0xFF};
	uint8_t raw[ROW_COUNT * (3 + ROW_LEN) + 1];

	if (with_warp)
		plant_key(INDOOR_B, 0xA3, objects, sizeof objects);
	else
		plant_key(INDOOR_B, 0xA3, no_warp, sizeof no_warp);
	plant_key(ENTITIES, LADX_WARP_HOME_ROOM, entities, sizeof entities);
	plant_key(BG_TILES, 0x0D, raw, planted_background(raw, rows_n));
}

static LadxTables tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;

/* A planted warp-home.txt of units units: an `ld a` opcode before each operand (opcode at operand 2), a sprite byte. */
static void open_tables(int units, int opcode)
{
	static const char order_text[] =
		"ladx-tables-order 1\nsection generator.py generateRom\n"
		"C warp-home - patches.core.warpHome(rom, settings.overworld == \"dungeonchain\")\n";
	static char text[1024];
	LadxTablesFile order, group;
	LadxTablesError err;
	int n = 0, u, k;

	n += snprintf(text + n, sizeof text - (size_t)n, "ladx-tables 1\ngroup warp-home\ntables " TABLES "\n");
	for (u = 1; u <= units; u++) {
		n += snprintf(text + n, sizeof text - (size_t)n, "unit warp-home.%d\nE\n", u);
		for (k = 0; k < LADX_WARP_HOME_OPERANDS; k++)
			n += snprintf(text + n, sizeof text - (size_t)n, "B %x %02x\n", OPERAND(k) - 1u,
				      k == 2 ? opcode : 0x3E);
		n += snprintf(text + n, sizeof text - (size_t)n, "B %x a5\n", BANK(0x0F) + 0x08E0u);
	}
	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "warp-home.txt";
	group.text = span(text);
	if (ladx_tables_open(&tables, &order, &group, 1, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static LadxPipelineStatus run_pipeline(const char *assignment, int with_warp, int rows_n, LadxPipelineError *err)
{
	static LadxSettings settings;
	int count;

	ladx_settings_defaults(&settings);
	if (assignment && ladx_settings_set(&settings, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
	memset(&randomizer, 0, sizeof randomizer);
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	plant_world(with_warp, rows_n);
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = NULL;
	memset(&pipeline, 0, sizeof pipeline);
	pipeline.tables = &tables;
	pipeline.settings = &settings;
	pipeline.randomizer = &randomizer;
	pipeline.rom = &rom;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

static int bank(uint32_t offset)
{
	uint8_t v;
	int32_t origin;

	if (!ladx_enc_bank_value(&enc, offset, &v, &origin))
		test_fail(TEST, "byte %x readable", (unsigned)offset);
	return v;
}

static int operands_are(int t, int map, int nr, int x, int y)
{
	return bank(OPERAND(0)) == t && bank(OPERAND(1)) == map && bank(OPERAND(2)) == nr && bank(OPERAND(3)) == x &&
	       bank(OPERAND(4)) == y;
}

static int stored_background_edited(void)
{
	static uint8_t values[LADX_WLOG_ATOM_MAX];
	static int32_t origins[LADX_WLOG_ATOM_MAX];
	LadxWlogKey key;
	size_t len;

	memset(&key, 0, sizeof key);
	key.index = 0x0D;
	return ladx_enc_entry_value(&enc, BG_TILES, key, values, origins, LADX_WLOG_ATOM_MAX, &len) &&
	       ladx_background_decode(&bg, values, len) && edited_as_ladxr(&bg);
}

static void test_group(void)
{
	LadxPipelineError err;
	const char *seg;
	char record[32];

	open_tables(1, 0x3E);
	check(run_pipeline(NULL, 1, ROW_COUNT, &err) == LADX_PIPELINE_OK, "the group runs in the pipeline");
	check(operands_are(0x00, 0x00, 0x31, 0x44, 0x66), "the start house's warp as the operands");
	check(bank(OPERAND(0) - 1u) == 0x3E && bank(BANK(0x0F) + 0x08E0u) == 0xA5, "warp-home.txt's unit written");
	check(stored_background_edited(), "background 0x0D stored with the S&Q edits");
	snprintf(record, sizeof record, "B %x 3e31\n", OPERAND(2) - 1u);
	check((seg = strstr(out, "G warp-home\n")) != NULL && strstr(seg, record) && strstr(seg, "T background_tiles d "),
	      "one segment: the unit's opcode and C's operand in one record, then the background");
	check(run_pipeline("overworld=dungeonchain", 1, ROW_COUNT, &err) == LADX_PIPELINE_OK &&
		      operands_are(0x01, 0x10, 0xA3, 0x50, 0x7F),
	      "overworld=dungeonchain forces the indoor fallback");
	check(run_pipeline("entranceshuffle=madness", 1, ROW_COUNT, &err) == LADX_PIPELINE_OK &&
		      operands_are(0x01, 0x10, 0xA3, 0x50, 0x7F),
	      "entranceshuffle=madness forces the indoor fallback");
	check(run_pipeline("entranceshuffle=simple", 1, ROW_COUNT, &err) == LADX_PIPELINE_OK &&
		      operands_are(0x00, 0x00, 0x31, 0x44, 0x66),
	      "entranceshuffle=simple does not");
	check(run_pipeline(NULL, 0, ROW_COUNT, &err) == LADX_PIPELINE_RAISED && strcmp(err.group, "warp-home") == 0 &&
		      strcmp(err.outcome->type, "IndexError") == 0,
	      "no warp in the start house: IndexError");
	check(run_pipeline(NULL, 1, ROW_COUNT - 1, &err) == LADX_PIPELINE_RAISED &&
		      strcmp(err.outcome->type, "KeyError") == 0,
	      "a missing background row: KeyError");
	open_tables(1, 0x3D);
	check(run_pipeline(NULL, 1, ROW_COUNT, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "warp-home") == 0,
	      "an operand not after `ld a` fails the group");
	open_tables(2, 0x3E);
	check(run_pipeline(NULL, 1, ROW_COUNT, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "warp-home") == 0,
	      "a file without one unit fails the group");
}

static void test_registry(void)
{
	const LadxPipelineGroup *groups;
	int count, g, enemizer = -1, warp = -1, title = -1;

	groups = ladx_pipeline_groups(&count);
	for (g = 0; g < count; g++) {
		if (strcmp(groups[g].name, "enemizer") == 0)
			enemizer = g;
		if (strcmp(groups[g].name, "warp-home") == 0)
			warp = g;
		if (strcmp(groups[g].name, "title") == 0)
			title = g;
	}
	check(enemizer >= 0 && warp == enemizer + 1 && title == warp + 1,
	      "warp-home registered between enemizer and title, LADXR's order");
	check(groups[warp].needs_rom && groups[warp].draw == NULL, "a rom-group with no draw");
}

int main(void)
{
	synth_rom_build(&synth, 0x3A2F0D1u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM parses");
	test_target();
	test_edit();
	test_group();
	test_registry();
	printf("%s: warpHome's target, the S&Q background and the group on a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
