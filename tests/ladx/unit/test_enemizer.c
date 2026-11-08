#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_enemizer"
#define OUT_MAX 262144
#define ENTITIES 1
#define TOP 2
#define BOTTOM 3

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

static SynthRom synth;
static LadxRomTables rom_tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxRoom scratch;
static char out[OUT_MAX];
static size_t out_len;

static int checks;
static int draws;
static LadxRngRecord last;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void count_sink(void *ctx, const LadxRngRecord *record)
{
	(void)ctx;
	if (record->method == LADX_RNG_CHOICE)
		draws++;
	last = *record;
}

static const uint8_t test_seed[] = "enemizer-test";

static void seed_rng(LadxRng *rng)
{
	ladx_rng_init(rng, LADX_RNG_RND, count_sink, NULL);
	ladx_rng_seed(rng, LADX_STAGE_ENEMIES_RANDOMIZE, test_seed, sizeof test_seed - 1);
	draws = 0;
}

/* A planted room: its entities (x, y, type) and, when gravestone, a 0xC5 object then a warp. */
typedef struct Planted {
	int room;
	int gravestone;
	int entities[8][3];
	int entity_count;
} Planted;

static void build(LadxRoom *r, const Planted *pl)
{
	size_t i;

	memset(r, 0, sizeof *r);
	r->room = pl->room;
	if (pl->gravestone) {
		r->objects[0].kind = LADX_ROOM_SINGLE;
		r->objects[0].x = 1;
		r->objects[0].y = 1;
		r->objects[0].type_id = 0xC5;
		ladx_room_warp(&r->objects[1], 1, 0, 0x30, 8, 8);
		r->object_count = 2;
	}
	for (i = 0; i < (size_t)pl->entity_count; i++) {
		r->entities[i].x = pl->entities[i][0];
		r->entities[i].y = pl->entities[i][1];
		r->entities[i].type_id = pl->entities[i][2];
	}
	r->entity_count = (size_t)pl->entity_count;
}

/*
 * LADXR's own randomizeEnemies at the pin on these rooms (a stub
 * RoomEditor, Random(b"enemizer-test") counting its choices; with the evil
 * shop after SPRITE_DATA[0x77] = SPRITE_DATA[0x4D]), rooms in its order:
 * the moblin lambda, sprite slots taken by other entities, a gravestone
 * and a warp, every slot taken, the evil shop's 0x77, two-value sets, an
 * empty intersection in slot 0, a gravestone under six enemy types, an
 * empty intersection in slot 1 that a union would not empty.
 */
static const Planted planted[] = {
	{0x03, 0, {{1, 1, 0x14}, {4, 4, 0x45}, {5, 5, 0x6D}}, 3},
	{0x07, 0, {{1, 1, 0x9D}, {2, 2, 0x52}, {3, 3, 0xB1}, {4, 4, 0x09}}, 4},
	{0x10, 0, {{1, 1, 0x09}, {2, 2, 0x0B}, {3, 3, 0x09}}, 3},
	{0x20, 1, {{2, 3, 0xEC}, {3, 3, 0x12}}, 2},
	{0x30, 0, {{1, 1, 0x45}}, 1},
	{0x40, 0, {{1, 1, 0x13}, {2, 2, 0x9F}}, 2},
	{0x50, 0, {{1, 1, 0x77}, {2, 2, 0x09}}, 2},
	{0x60, 0, {{1, 1, 0xA0}, {2, 2, 0x16}, {3, 3, 0x1A}}, 3},
	{0x70, 1, {{1, 1, 0x09}, {2, 1, 0x0D}, {3, 1, 0x0E}, {4, 1, 0x0F}, {5, 1, 0x12}, {6, 1, 0x15}}, 6},
	{0x80, 0, {{1, 1, 0x9D}, {2, 2, 0x52}, {3, 3, 0xB1}, {4, 4, 0x09}, {5, 4, 0x0D}, {6, 4, 0x0E}, {7, 4, 0x0F},
		   {8, 4, 0x12}}, 8},
	{0x90, 0, {{1, 1, 0x27}, {2, 2, 0x10}, {3, 1, 0x09}, {4, 1, 0x0D}, {5, 1, 0x0E}, {6, 1, 0x0F}, {7, 1, 0x14},
		   {8, 1, 0x1A}}, 8},
};

#define PLANTED (sizeof planted / sizeof planted[0])

static const struct {
	int found, draws, types[8];
} expected_plain[PLANTED] = {
	{1, 2, {0xA0, 0x45, 0x6D}}, {1, 2, {0x9D, 0x52, 0xB1, 0x0F}}, {1, 2, {0xAE, 0xBB, 0xAE}},
	{1, 2, {0xA1, 0x21}},	    {0, 0, {0x45}},		    {1, 11, {0x13, 0x9B}},
	{1, 2, {0x77, 0xBD}},	    {1, 3, {0x0B, 0x23, 0xCC}},
	{1, 23, {0x21, 0x16, 0x1C, 0x1C, 0x17, 0x9B}},
	{1, 11, {0x9D, 0x52, 0xB1, 0x0B, 0x0D, 0x14, 0x9B, 0x23}},
	{1, 16, {0x27, 0x10, 0x09, 0x1A, 0x1E, 0x56, 0x9B, 0x09}},
}, expected_evilshop[PLANTED] = {
	{1, 2, {0xA0, 0x45, 0x6D}}, {1, 2, {0x9D, 0x52, 0xB1, 0x0F}}, {1, 2, {0xAE, 0xBB, 0xAE}},
	{1, 2, {0xA1, 0x21}},	    {0, 0, {0x45}},		    {1, 11, {0x13, 0x9B}},
	{1, 4, {0x77, 0xCC}},	    {1, 5, {0x16, 0x0B, 0x14}},
	{1, 16, {0x1C, 0xEE, 0x1C, 0x17, 0xEE, 0xED}},
	{1, 19, {0x9D, 0x52, 0xB1, 0x24, 0x9B, 0x0D, 0x9B, 0x24}},
	{1, 9, {0x27, 0x10, 0x9F, 0xED, 0x56, 0x9F, 0xA0, 0xED}},
};

static void test_picks_against_ladxr(int evilshop)
{
	static LadxRoom room;
	LadxRng rng;
	char what[128];
	size_t k, i;
	int found, before;
	const int(*types)[8];

	seed_rng(&rng);
	for (k = 0; k < PLANTED; k++) {
		int want_found = evilshop ? expected_evilshop[k].found : expected_plain[k].found;
		int want_draws = evilshop ? expected_evilshop[k].draws : expected_plain[k].draws;

		types = evilshop ? &expected_evilshop[k].types : &expected_plain[k].types;
		build(&room, &planted[k]);
		before = draws;
		found = -1;
		sprintf(what, "room 0x%02X%s: pick ok", planted[k].room, evilshop ? " (evil shop)" : "");
		check(ladx_enemizer_pick(&room, &rng, evilshop, &found) == LADX_RNG_OK, what);
		sprintf(what, "room 0x%02X%s: found %d, LADXR %d", planted[k].room, evilshop ? " (evil shop)" : "", found,
			want_found);
		check(found == want_found, what);
		sprintf(what, "room 0x%02X%s: %d draws, LADXR %d", planted[k].room, evilshop ? " (evil shop)" : "",
			draws - before, want_draws);
		check(draws - before == want_draws, what);
		check(room.entity_count == (size_t)planted[k].entity_count, "the entity count is kept");
		for (i = 0; i < room.entity_count; i++) {
			sprintf(what, "room 0x%02X%s: entity %lu is 0x%02X, LADXR 0x%02X", planted[k].room,
				evilshop ? " (evil shop)" : "", (unsigned long)i, room.entities[i].type_id, (*types)[i]);
			check(room.entities[i].type_id == (*types)[i] && room.entities[i].x == planted[k].entities[i][0] &&
				      room.entities[i].y == planted[k].entities[i][1],
			      what);
		}
	}
	check(last.stage == LADX_STAGE_ENEMIES_RANDOMIZE && last.n == LADX_ENEMY_TYPE_COUNT,
	      "every draw is a choice over the 43 enemy types in randomizeEnemies' frame");
}

static void log_sink(void *ctx, const char *text, size_t len)
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

static LadxWlogKey key_of(uint32_t index)
{
	LadxWlogKey key;

	memset(&key, 0, sizeof key);
	key.index = index;
	return key;
}

static void plant_key(int table, uint32_t index, const uint8_t *bytes, size_t n)
{
	int32_t known[64];
	size_t i;

	for (i = 0; i < n; i++)
		known[i] = LADX_ENC_KNOWN;
	if (ladx_enc_table(&enc, span("plant"), table, key_of(index), bytes, known, n) != LADX_ENC_OK)
		test_fail(TEST, "planting an entry");
}

/* Every overworld room: header, no object (a gravestone 0xC5 in room 0x20); no entity but the planted ones. */
static void plant_overworld(void)
{
	static const uint8_t empty_objects[] = {0x00, 0x00, 0xFE}, grave_objects[] = {0x00, 0x00, 0x11, 0xC5, 0xFE};
	static const uint8_t none[] = {0xFF};
	static const uint8_t r10[] = {0x11, 0x09, 0x22, 0x0B, 0x33, 0x09, 0xFF};
	static const uint8_t r20[] = {0x32, 0xEC, 0x33, 0x12, 0xFF};
	static const uint8_t r24[] = {0x11, 0x09, 0xFF};
	static const uint8_t r30[] = {0x11, 0x45, 0xFF};
	uint32_t n;

	for (n = 0; n < 0x100; n++) {
		if (n == 0x20)
			plant_key(n < 0x80 ? TOP : BOTTOM, n & 0x7F, grave_objects, sizeof grave_objects);
		else
			plant_key(n < 0x80 ? TOP : BOTTOM, n & 0x7F, empty_objects, sizeof empty_objects);
		if (n == 0x10)
			plant_key(ENTITIES, n, r10, sizeof r10);
		else if (n == 0x20)
			plant_key(ENTITIES, n, r20, sizeof r20);
		else if (n == 0x24)
			plant_key(ENTITIES, n, r24, sizeof r24);
		else if (n == 0x30)
			plant_key(ENTITIES, n, r30, sizeof r30);
		else
			plant_key(ENTITIES, n, none, sizeof none);
	}
}

static void check_entities(uint32_t room, const uint8_t *want, size_t len, const char *what)
{
	uint8_t values[64];
	int32_t origins[64];
	size_t n = 0;

	check(ladx_enc_entry_value(&enc, ENTITIES, key_of(room), values, origins, sizeof values, &n) && n == len &&
		      memcmp(values, want, len) == 0,
	      what);
}

/*
 * LADXR's own randomizeEnemies at the pin with a stub RoomEditor holding
 * these rooms (every other room empty), Random(b"enemizer-test"): rooms
 * 0x10 and 0x20 stored, two draws each.
 */
static void test_room_loop(void)
{
	static const uint8_t want10[] = {0x11, 0xF2, 0x22, 0xA0, 0x33, 0xF2, 0xFF};
	static const uint8_t want20[] = {0x32, 0x0F, 0x33, 0x9F, 0xFF};
	static const uint8_t keep24[] = {0x11, 0x09, 0xFF};
	static const uint8_t keep30[] = {0x11, 0x45, 0xFF};
	LadxRng rng;
	const char *seg;

	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), log_sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	plant_overworld();
	seed_rng(&rng);
	check(ladx_enemizer_rooms(&enc, span("enemizer"), &scratch, &rng, 0), "the room loop runs");
	check(draws == 4, "four draws, as LADXR's");
	check_entities(0x10, want10, sizeof want10, "room 0x10's enemies replaced as LADXR's");
	check_entities(0x20, want20, sizeof want20, "room 0x20's enemies replaced under the gravestone's slot 3");
	check_entities(0x24, keep24, sizeof keep24, "logic-important room 0x24 untouched");
	check_entities(0x30, keep30, sizeof keep30, "room 0x30 holds no enemy");
	if (ladx_enc_end(&enc) != LADX_ENC_OK)
		test_fail(TEST, "the encoder ends");
	seg = strstr(out, "G enemizer\n");
	check(seg && strstr(seg, "T entities 10 ") && strstr(seg, "T entities 20 ") && strstr(seg, "T rooms_overworld_top 20 ") &&
		      !strstr(seg, "T entities 24 ") && !strstr(seg, "T entities 30 ") && !strstr(seg, "T rooms_overworld_top 30 "),
	      "rooms 0x10 and 0x20 stored, 0x24 and 0x30 not");
}

static void test_malformed_room(void)
{
	static const uint8_t bad[] = {0x11, 0x09};
	LadxRng rng;

	out_len = 0;
	if (ladx_wlog_begin(&wlog, span(TABLES), log_sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	plant_overworld();
	plant_key(ENTITIES, 0x40, bad, sizeof bad);
	seed_rng(&rng);
	check(!ladx_enemizer_rooms(&enc, span("enemizer"), &scratch, &rng, 0), "a room with no terminator fails the loop");
}

static LadxTables tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static LadxRng rnd;
static int seeded;

static void group_sink(void *ctx, const LadxRngRecord *record)
{
	if (record->method == LADX_RNG_SEED && record->stage == LADX_STAGE_ENEMIES_RANDOMIZE &&
	    record->seed_len == sizeof test_seed - 1 && memcmp(record->seed, test_seed, record->seed_len) == 0)
		seeded++;
	count_sink(ctx, record);
}

/* A planted enemizer.txt of units units, each one invented byte in bank 0x36. */
static void open_tables(int units)
{
	static const char order_text[] = "ladx-tables-order 1\nsection generator.py generateRom\n"
					 "C enemizer - patches.enemies.randomizeEnemies(rom, seed)\n";
	static char text[1024];
	LadxTablesFile order, group;
	LadxTablesError err;
	int n, u;

	n = snprintf(text, sizeof text, "ladx-tables 1\ngroup enemizer\ntables " TABLES "\n");
	for (u = 1; u <= units; u++)
		n += snprintf(text + n, sizeof text - (size_t)n, "unit enemizer.%d\nE\nB %x %02x\n", u,
			      0x36u * SYNTH_ROM_BANK + 0x23DDu + (unsigned)u, 0x70 + u);
	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "enemizer.txt";
	group.text = span(text);
	if (ladx_tables_open(&tables, &order, &group, 1, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static LadxPipelineStatus run_group(const char *assignment, int units, int with_rnd, LadxPipelineError *err)
{
	static LadxSettings settings;
	int count;

	open_tables(units);
	ladx_settings_defaults(&settings);
	if (assignment && ladx_settings_set(&settings, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
	ladx_settings_validate(&settings);
	memset(&randomizer, 0, sizeof randomizer);
	memcpy(randomizer.seed, test_seed, sizeof test_seed - 1);
	randomizer.seed_len = sizeof test_seed - 1;
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), log_sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	plant_overworld();
	ladx_rng_init(&rnd, LADX_RNG_RND, group_sink, NULL);
	draws = seeded = 0;
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = NULL;
	memset(&pipeline, 0, sizeof pipeline);
	pipeline.tables = &tables;
	pipeline.settings = &settings;
	pipeline.randomizer = &randomizer;
	pipeline.rnd = with_rnd ? &rnd : NULL;
	pipeline.global = with_rnd ? &rnd : NULL;
	pipeline.rom = &rom;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

static void test_group(void)
{
	static const uint8_t want10[] = {0x11, 0xF2, 0x22, 0xA0, 0x33, 0xF2, 0xFF};
	LadxPipelineError err;
	const char *seg;
	char unit_record[32];

	check(run_group("enemies=overworld", 1, 1, &err) == LADX_PIPELINE_OK, "the group runs in the pipeline");
	check(seeded == 1 && draws == 4, "its own generator seeded from the run's seed, traced through rnd's sink");
	check_entities(0x10, want10, sizeof want10, "the rooms picked as LADXR's");
	snprintf(unit_record, sizeof unit_record, "B %x 71\n", 0x36u * SYNTH_ROM_BANK + 0x23DEu);
	check((seg = strstr(out, "G enemizer\n")) != NULL && strstr(seg, "T entities 10 ") && strstr(seg, unit_record),
	      "one segment: the rooms, then enemizer.txt's unit");
	check(run_group(NULL, 1, 1, &err) == LADX_PIPELINE_OK && draws == 0 && seeded == 0 &&
		      !strstr(out, "G enemizer\n"),
	      "enemies=default: nothing drawn or written");
	check(run_group("enemies=overworld", 2, 1, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "enemizer") == 0,
	      "a file without one unit fails the group");
	check(run_group("enemies=overworld", 1, 0, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "enemizer") == 0,
	      "no rnd fails the group");
}

static void test_registry(void)
{
	const LadxPipelineGroup *groups;
	int count, g, bosses = -1, enemizer = -1;

	groups = ladx_pipeline_groups(&count);
	for (g = 0; g < count; g++) {
		if (strcmp(groups[g].name, "bosses") == 0)
			bosses = g;
		if (strcmp(groups[g].name, "enemizer") == 0)
			enemizer = g;
	}
	check(bosses >= 0 && enemizer == bosses + 1, "enemizer registered right after bosses, LADXR's order");
	check(groups[enemizer].needs_rom && groups[enemizer].draw == NULL, "a rom-group with no draw");
}

int main(void)
{
	synth_rom_build(&synth, 0xE7E31u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM parses");
	test_room_loop();
	test_malformed_room();
	test_group();
	test_registry();
	test_picks_against_ladxr(0);
	test_picks_against_ladxr(1);
	printf("%s: randomizeEnemies' picks against LADXR on planted rooms, the room loop and the group on a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
