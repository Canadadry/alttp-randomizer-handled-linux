#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_sprites"
#define OUT_MAX 262144
#define ENTITIES 1
#define OW_TOP 2
#define OW_BOTTOM 3
#define INDOOR_A 4
#define INDOOR_B 5
#define COLOR 6
#define SPRITE_OW 7
#define SPRITE_INDOOR 8
#define BANK(b) ((uint32_t)(b) * SYNTH_ROM_BANK)

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

static SynthRom synth;
static LadxRomTables rom_tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxRoom room;
static LadxOutcome raised;
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

/* Values CPython 3.13 printed for list(s) after s.add of each value in order. */
static int order_is(const int *values, size_t n, const int *want, size_t want_n)
{
	int got[LADX_SPRITES_SET_MAX];
	size_t k;

	if (ladx_sprites_set_order(values, n, got) != want_n)
		return 0;
	for (k = 0; k < want_n; k++)
		if (got[k] != want[k])
			return 0;
	return 1;
}

static void test_set_order(void)
{
	static const int a[] = {8, 0}, a_want[] = {8, 0};
	static const int b[] = {0, 8}, b_want[] = {0, 8};
	static const int c[] = {0x5F, 0x68}, c_want[] = {0x68, 0x5F};
	static const int d[] = {71, 79}, d_want[] = {79, 71};
	static const int e[] = {79, 71}, e_want[] = {71, 79};
	static const int f[] = {3, 11, 19}, f_want[] = {11, 19, 3};
	static const int g[] = {19, 11, 3}, g_want[] = {11, 3, 19};
	static const int h[] = {1, 9, 17, 25}, h_want[] = {1, 25, 9, 17};
	static const int i[] = {0x91, 0x65, 0x91}, i_want[] = {0x91, 0x65};
	static const int j[] = {1, 33}, j_want[] = {1, 33};
	static const int k[] = {33, 1}, k_want[] = {33, 1};

	check(order_is(a, 2, a_want, 2) && order_is(b, 2, b_want, 2), "a collision at slot 0 keeps insertion order");
	check(order_is(c, 2, c_want, 2), "slot order: 0x68 (slot 0) before 0x5F (slot 7)");
	check(order_is(d, 2, d_want, 2) && order_is(e, 2, e_want, 2),
	      "a collision at slot 7 perturbs the second to slot 6");
	check(order_is(f, 3, f_want, 3) && order_is(g, 3, g_want, 3), "three colliding values probe with perturb 0");
	check(order_is(h, 4, h_want, 4), "four values on one slot");
	check(order_is(i, 3, i_want, 2), "a repeated value is added once");
	check(order_is(j, 2, j_want, 2) && order_is(k, 2, k_want, 2),
	      "a collision at slot 1: 33's perturbation shifts by 5, not 4");
}

/* list(so.intersection(other)) as CPython 3.13 printed it, so and other built in the orders given. */
static int meet_is(const int *so, size_t so_n, const int *other, size_t other_n, const int *want, size_t want_n)
{
	int added[LADX_SPRITES_SET_MAX];
	size_t n = ladx_sprites_set_intersection(so, so_n, other, other_n, added);

	return n == want_n && order_is(added, n, want, want_n);
}

static void test_intersection(void)
{
	static const int x[] = {8, 0}, y[] = {0, 8}, z[] = {0, 8, 1}, w[] = {0x5F, 0x68}, v[] = {0x5F, 0x7B};
	static const int xy[] = {0, 8}, yx[] = {8, 0}, xz[] = {8, 0}, wv[] = {0x5F};
	static const int a[] = {71, 79}, b[] = {79, 71}, ab[] = {79, 71}, ba[] = {71, 79};

	check(meet_is(x, 2, y, 2, xy, 2) && meet_is(y, 2, x, 2, yx, 2),
	      "equal sizes: the argument is walked, its order re-added");
	check(meet_is(x, 2, z, 3, xz, 2) && meet_is(z, 3, x, 2, xz, 2), "the smaller set is walked, either side");
	check(meet_is(a, 2, b, 2, ab, 2) && meet_is(b, 2, a, 2, ba, 2), "colliding members re-added in walk order");
	check(meet_is(w, 2, v, 2, wv, 1), "one common member");
	check(ladx_sprites_set_intersection(x, 2, w, 2, (int[LADX_SPRITES_SET_MAX]){0}) == 0, "disjoint: empty");
}

static void plain_room(int nr)
{
	memset(&room, 0, sizeof room);
	room.room = nr;
}

static void add_entity(int type)
{
	room.entities[room.entity_count].x = (int)room.entity_count;
	room.entities[room.entity_count].y = 1;
	room.entities[room.entity_count].type_id = type;
	room.entity_count++;
}

static void add_object(int type)
{
	room.objects[room.object_count].kind = LADX_ROOM_SINGLE;
	room.objects[room.object_count].type_id = type;
	room.object_count++;
}

static int values_are(int evilshop, int a, int b, int c, int d)
{
	uint8_t v[LADX_SPRITES_SLOTS];

	memset(&raised, 0, sizeof raised);
	return ladx_sprites_room(&room, evilshop, v, &raised) == LADX_SPRITES_OK && v[0] == a && v[1] == b && v[2] == c &&
	       v[3] == d;
}

static int raises(const char *type, const char *message)
{
	uint8_t v[LADX_SPRITES_SLOTS];

	memset(&raised, 0, sizeof raised);
	return ladx_sprites_room(&room, 0, v, &raised) == LADX_SPRITES_RAISED &&
	       raised.kind == LADX_OUTCOME_CRASH && strcmp(raised.type, type) == 0 &&
	       strcmp(raised.message, message) == 0;
}

/* Expected values printed by LADXR's updateSpriteData at the pin on planted rooms. */
static void test_room(void)
{
	plain_room(0x0A3);
	check(values_are(0, 0xFF, 0xFF, 0xFF, 0xFF), "an empty room: every slot 0xFF");
	plain_room(0x00E);
	check(values_are(0, 0xFF, 0xFF, 0xD6, 0xD7), "D7's entrance: slots 2 and 3 preset");
	plain_room(0x215);
	check(values_are(0, 0x66, 0xFF, 0xFF, 0xFF), "a D7 ball room: slot 0 preset");
	plain_room(0x21F);
	check(values_are(0, 0xFF, 0xFF, 0xFF, 0xFF), "0x21F is past the ball rooms");
	plain_room(0x21E);
	check(values_are(0, 0x66, 0xFF, 0xFF, 0xFF), "0x21E is the last ball room");
	plain_room(0x211);
	check(values_are(0, 0x66, 0xFF, 0xFF, 0xFF), "0x211 is the first ball room");
	plain_room(0x0A3);
	add_entity(0x0D);
	add_entity(0x18);
	check(values_are(0, 0xFF, 0x87, 0xFF, 0x93), "two entities' slots");
	plain_room(0x0A3);
	add_object(0xC5);
	check(values_are(0, 0xFF, 0xFF, 0xFF, 0x82), "a pushable gravestone: slot 3 0x82");
	plain_room(0x1A3);
	add_object(0xC5);
	check(values_are(0, 0xFF, 0xFF, 0xFF, 0xFF), "no gravestone rule indoors");
	plain_room(0x0A3);
	ladx_room_warp(&room.objects[0], 0, 0, 0xC5, 0, 0);
	room.objects[0].type_id = 0xC5;
	room.object_count = 1;
	check(values_are(0, 0xFF, 0xFF, 0xFF, 0xFF), "a warp is never a gravestone");
	plain_room(0x0A3);
	add_entity(0xA0);
	check(values_are(0, 0x68, 0xFF, 0xFF, 0xFF), "a set alone: its first in CPython's order");
	plain_room(0x0A3);
	add_entity(0x16);
	check(values_are(0, 0xFF, 0x91, 0xFF, 0xFF), "{0x91, 0x65}: 0x91 first");
	plain_room(0x0A3);
	add_entity(0xA0);
	add_entity(0xA1);
	check(values_are(0, 0x5F, 0xFF, 0xFF, 0xFF), "two sets meet in their intersection");
	plain_room(0x0A3);
	add_entity(0x16);
	add_entity(0x15);
	check(values_are(0, 0xFF, 0x91, 0xFF, 0xFF), "a set then a value in it: the value");
	plain_room(0x0A3);
	add_entity(0x15);
	add_entity(0x16);
	check(values_are(0, 0xFF, 0x91, 0xFF, 0xFF), "a value then a set holding it: kept");
	plain_room(0x230);
	add_entity(0x1A);
	add_entity(0x1F);
	check(values_are(0, 0x77, 0xFF, 0xFF, 0xFF), "a set then its second member: that member, not the set's first");
	plain_room(0x0A3);
	add_entity(0x1B);
	check(values_are(0, 0xFF, 0xFF, 0xFF, 0xFF), "an entity with None data is skipped");
	plain_room(0x0A3);
	add_entity(0x77);
	check(values_are(0, 0xFF, 0xFF, 0xFF, 0x46), "0x77 without the evil shop");
	check(values_are(1, 0xFF, 0xFF, 0x88, 0xC7), "0x77 with the evil shop answers as 0x4D");
	plain_room(0x2A3);
	add_entity(0x3E);
	check(values_are(0, 0x8D, 0xFF, 0x8F, 0xFF), "Marin's lambda in the start house");
	plain_room(0x0A3);
	add_entity(0x3E);
	check(values_are(0, 0xFF, 0xFF, 0xE6, 0xFF), "Marin's lambda elsewhere");
}

static void test_raises(void)
{
	plain_room(0x0A3);
	add_entity(0x1A);
	add_entity(0xA0);
	check(raises("AssertionError", ""), "two disjoint sets: the empty intersection's assert");
	plain_room(0x0A3);
	add_entity(0x0D);
	add_entity(0x10);
	check(raises("AssertionError",
		     "Room: 0a3 cannot load graphics for entity: 10 (Index: 1 Failed: 66, Active: 135)"),
	      "two values on one slot");
	plain_room(0x0A3);
	add_entity(0x0D);
	add_entity(0x16);
	check(raises("AssertionError",
		     "Room: 0a3 cannot load graphics for entity: 16 (Index: 1 Failed: {145, 101}, Active: 135)"),
	      "a value then a set without it: the set printed in CPython's order");
	plain_room(0x0A3);
	add_entity(0x16);
	add_entity(0x0D);
	check(raises("AssertionError",
		     "Room: 0a3 cannot load graphics for entity: 0d (Index: 1 Failed: 135, Active: {145, 101})"),
	      "a set then a value outside it");
	plain_room(0x00E);
	add_entity(0x18);
	check(raises("AssertionError",
		     "Room: 00e cannot load graphics for entity: 18 (Index: 3 Failed: 147, Active: 215)"),
	      "D7's preset against an entity");
	plain_room(0x0A3);
	add_object(0xC5);
	add_entity(0x18);
	check(raises("AssertionError",
		     "Room: 0a3 cannot load graphics for entity: 18 (Index: 3 Failed: 147, Active: 130)"),
	      "the gravestone's 0x82 against an entity");
	plain_room(0x0A3);
	add_entity(0x1B);
	add_entity(0x00);
	check(raises("KeyError", "0"), "an entity SPRITE_DATA lacks: KeyError, its decimal id");
	plain_room(0x0A3);
	add_entity(0xFD);
	check(raises("KeyError", "253"), "KeyError 253");
}

static void plant_key(int table, uint32_t index, const uint8_t *bytes, size_t n)
{
	int32_t known[64];
	LadxWlogKey key;
	size_t i;

	for (i = 0; i < n; i++)
		known[i] = LADX_ENC_KNOWN;
	memset(&key, 0, sizeof key);
	key.index = index;
	if (ladx_enc_table(&enc, span("plant"), table, key, bytes, known, n) != LADX_ENC_OK)
		test_fail(TEST, "planting an entry");
}

/* The objects table and key of an index room, as RoomEditor reads it. */
static void objects_key(int nr, int *table, uint32_t *index)
{
	if (nr < 0x80) {
		*table = OW_TOP;
		*index = (uint32_t)nr;
	} else if (nr < 0x100) {
		*table = OW_BOTTOM;
		*index = (uint32_t)nr - 0x80u;
	} else if (nr < 0x200) {
		*table = INDOOR_A;
		*index = (uint32_t)nr - 0x100u;
	} else if (nr < 0x300) {
		*table = INDOOR_B;
		*index = (uint32_t)nr - 0x200u;
	} else {
		*table = COLOR;
		*index = (uint32_t)nr - 0x300u;
	}
}

static int bad_room = -1, raising_room = -1;

/*
 * Every room: a header and no object (a gravestone in 0x020), no entity
 * but 0x0A3's 0x0D and 0x18, 0x305's 0x16 and 0x1A3's 0x77; bad_room's
 * entities without a terminator, raising_room's an entity SPRITE_DATA lacks.
 */
static void plant_world(void)
{
	static const uint8_t empty[] = {0x00, 0x00, 0xFE}, grave[] = {0x00, 0x00, 0x11, 0xC5, 0xFE};
	static const uint8_t none[] = {0xFF}, a3[] = {0x11, 0x0D, 0x22, 0x18, 0xFF}, c5[] = {0x11, 0x16, 0xFF};
	static const uint8_t shop[] = {0x11, 0x77, 0xFF}, open_ended[] = {0x11, 0x0D}, lacking[] = {0x11, 0x00, 0xFF};
	uint32_t index;
	int nr, table;

	for (nr = 0; nr < LADX_SPRITES_ROOMS; nr++) {
		objects_key(nr, &table, &index);
		if (nr == 0x020)
			plant_key(table, index, grave, sizeof grave);
		else
			plant_key(table, index, empty, sizeof empty);
		if (nr == bad_room)
			plant_key(ENTITIES, (uint32_t)nr, open_ended, sizeof open_ended);
		else if (nr == raising_room)
			plant_key(ENTITIES, (uint32_t)nr, lacking, sizeof lacking);
		else if (nr == 0x0A3)
			plant_key(ENTITIES, (uint32_t)nr, a3, sizeof a3);
		else if (nr == 0x305)
			plant_key(ENTITIES, (uint32_t)nr, c5, sizeof c5);
		else if (nr == 0x1A3)
			plant_key(ENTITIES, (uint32_t)nr, shop, sizeof shop);
		else
			plant_key(ENTITIES, (uint32_t)nr, none, sizeof none);
	}
}

static LadxTables tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;

/* A planted sprites.txt of units units, each writing one byte at bank 0 0x0DAD. */
static void open_tables(int units)
{
	static const char order_text[] = "ladx-tables-order 1\nsection generator.py generateRom\n"
					 "C sprites - patches.aesthetics.updateSpriteData(rom)\n";
	static char text[1024];
	LadxTablesFile order, group;
	LadxTablesError err;
	int n = 0, u;

	n += snprintf(text + n, sizeof text - (size_t)n, "ladx-tables 1\ngroup sprites\ntables " TABLES "\n");
	for (u = 1; u <= units; u++)
		n += snprintf(text + n, sizeof text - (size_t)n, "unit sprites.%d\nE\nB dad c3\n", u);
	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "sprites.txt";
	group.text = span(text);
	if (ladx_tables_open(&tables, &order, &group, 1, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static LadxPipelineStatus run_pipeline(const char *assignment, LadxPipelineError *err)
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
	plant_world();
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

/* The current entry of room nr's sprite data equals a, b, c, d. */
static int sprite_entry_is(int nr, int a, int b, int c, int d)
{
	uint8_t v[16];
	int32_t origins[16];
	LadxWlogKey key;
	size_t len;

	memset(&key, 0, sizeof key);
	key.index = (uint32_t)(nr < 0x100 ? nr : nr - 0x100);
	return ladx_enc_entry_value(&enc, nr < 0x100 ? SPRITE_OW : SPRITE_INDOOR, key, v, origins, 16, &len) &&
	       len == 4 && v[0] == a && v[1] == b && v[2] == c && v[3] == d;
}

static int count_lines(const char *from, const char *prefix)
{
	const char *at = from;
	size_t n = strlen(prefix);
	int k = 0;

	while ((at = strstr(at, prefix)) != NULL) {
		if (at == out || at[-1] == '\n')
			k++;
		at += n;
	}
	return k;
}

static void test_group(void)
{
	LadxPipelineError err;
	const char *seg;
	uint8_t v;
	int32_t origin;

	open_tables(1);
	check(run_pipeline(NULL, &err) == LADX_PIPELINE_OK, "the group runs in the pipeline");
	check(ladx_enc_bank_value(&enc, 0x0DAD, &v, &origin) && v == 0xC3, "sprites.txt's unit written");
	check(sprite_entry_is(0x000, 0xFF, 0xFF, 0xFF, 0xFF), "an empty overworld room: 0xFF");
	check(sprite_entry_is(0x00E, 0xFF, 0xFF, 0xD6, 0xD7), "D7's entrance preset");
	check(sprite_entry_is(0x020, 0xFF, 0xFF, 0xFF, 0x82), "the gravestone room");
	check(sprite_entry_is(0x0A3, 0xFF, 0x87, 0xFF, 0x93), "room 0x0A3's entities");
	check(sprite_entry_is(0x1A3, 0xFF, 0xFF, 0xFF, 0x46), "0x77 without the evil shop");
	check(sprite_entry_is(0x215, 0x66, 0xFF, 0xFF, 0xFF), "a D7 ball room, indoor key 0x115");
	check(sprite_entry_is(0x305, 0xFF, 0x91, 0xFF, 0xFF), "a colour dungeon room, indoor key 0x205");
	check(sprite_entry_is(0x315, 0xFF, 0xFF, 0xFF, 0xFF), "the last room, 0x315");
	seg = strstr(out, "G sprites\n");
	check(seg != NULL && count_lines(seg, "T room_sprite_data_overworld ") == 0x100 &&
		      count_lines(seg, "T room_sprite_data_indoor ") == 0x216 - 1,
	      "one segment: 256 overworld and 533 indoor entries");
	check(seg != NULL && !strstr(seg, "T room_sprite_data_indoor 1ff "), "room 0x2FF skipped");
	check(seg != NULL && strstr(seg, "B dad c3\n") && strstr(seg, "B dad c3\n") < strstr(seg, "T "),
	      "the unit's bytes first, as LADXR patches before the rooms");
	check(run_pipeline("evilshop=enabled", &err) == LADX_PIPELINE_OK &&
		      sprite_entry_is(0x1A3, 0xFF, 0xFF, 0x88, 0xC7),
	      "evilshop: 0x77 answers as 0x4D");
	raising_room = 0x1B0;
	check(run_pipeline(NULL, &err) == LADX_PIPELINE_RAISED && strcmp(err.group, "sprites") == 0 &&
		      strcmp(err.outcome->type, "KeyError") == 0 && strcmp(err.outcome->message, "0") == 0,
	      "an entity SPRITE_DATA lacks: KeyError ends the walk");
	raising_room = -1;
	bad_room = 0x2FE;
	check(run_pipeline(NULL, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "sprites") == 0,
	      "a room that does not load fails the group");
	bad_room = 0x2FF;
	check(run_pipeline(NULL, &err) == LADX_PIPELINE_OK, "room 0x2FF is never loaded");
	bad_room = -1;
	open_tables(2);
	check(run_pipeline(NULL, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "sprites") == 0,
	      "a file without one unit fails the group");
}

static void test_registry(void)
{
	const LadxPipelineGroup *groups;
	int count, g, endscreen = -1, sprites = -1;

	groups = ladx_pipeline_groups(&count);
	for (g = 0; g < count; g++) {
		if (strcmp(groups[g].name, "endscreen") == 0)
			endscreen = g;
		if (strcmp(groups[g].name, "sprites") == 0)
			sprites = g;
	}
	check(endscreen >= 0 && sprites == endscreen + 1, "sprites registered after endscreen, LADXR's order");
	check(groups[sprites].needs_rom && groups[sprites].draw == NULL, "a rom-group with no draw");
}

int main(void)
{
	synth_rom_build(&synth, 0x5B17E5u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM parses");
	test_set_order();
	test_intersection();
	test_room();
	test_raises();
	test_registry();
	test_group();
	printf("%s: CPython's set order, every room's sprite values and the group on a synthetic ROM (%d checks)\n",
	       TEST, checks);
	return 0;
}
