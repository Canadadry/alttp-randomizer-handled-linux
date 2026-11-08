#include "ladx_groups.h"
#include "ladx_items.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_bosses"
#define OUT_MAX 262144
#define ENTITIES 1
#define TOP 2
#define BOTTOM 3
#define INDOOR_A 4
#define INDOOR_B 5
#define COLOR 6
#define BANK(b) ((uint32_t)(b) * SYNTH_ROM_BANK)
#define COLOR_WARPS (BANK(0x14) + 0x04E0u)
#define DROP_TABLE (BANK(0x3E) + 0x3800u)
#define BOSS_ITEM (BANK(0x3E) + 0x3300u)

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

/* Invented instruction bytes: the opcodes C keeps, the operands it replaces. */
static const LadxBossesAsm code = {
	{0xA1, 0xA2}, {0xB1, 0xB2}, {0xC1, 0xC2, 0xC3}, {0xD1, 0xD2, 0xD3}, {{0xE1, 0xE2}, {0xE3, 0xE4}, {0xE5, 0xE6}},
};

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

/* No KS: planted entries are not matched against the synthetic ROM. */
static void begin(void)
{
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
}

static void end(void)
{
	if (ladx_enc_end(&enc) != LADX_ENC_OK)
		test_fail(TEST, "the encoder ends");
}

static void plant_key(int table, uint32_t index, const uint8_t *bytes, size_t n)
{
	int32_t known[128];
	LadxWlogKey key;
	size_t i;

	for (i = 0; i < n; i++)
		known[i] = LADX_ENC_KNOWN;
	memset(&key, 0, sizeof key);
	key.index = index;
	if (ladx_enc_table(&enc, span("plant"), table, key, bytes, known, n) != LADX_ENC_OK)
		test_fail(TEST, "planting an entry");
}

static void plant_byte(uint32_t offset, uint8_t value)
{
	int32_t known = LADX_ENC_KNOWN;

	if (ladx_enc_bank(&enc, span("plant"), offset, &value, &known, 1) != LADX_ENC_OK)
		test_fail(TEST, "planting a byte");
}

/* Room nr (0x100 and up): header 0x01 0x32, objects, then entities. */
static void plant_room(int nr, const uint8_t *objects, size_t n, const uint8_t *entities, size_t e)
{
	uint8_t raw[128];
	int table = nr < 0x200 ? INDOOR_A : nr < 0x300 ? INDOOR_B : COLOR;

	raw[0] = 0x01;
	raw[1] = 0x32;
	memcpy(raw + 2, objects, n);
	raw[n + 2] = 0xFE;
	plant_key(table, (uint32_t)(nr & 0xFF), raw, n + 3);
	memcpy(raw, entities, e);
	raw[e] = 0xFF;
	plant_key(ENTITIES, (uint32_t)nr, raw, e + 1);
}

#define WARP(t, map, lo, x, y) (0xE0 | (t)), (map), (lo), (x), (y)

/*
 * Every boss room: a warp, stairs 0xBE, lava 0x06, D1 pits 0x1C, 0x1E,
 * 0x1F, an open door 0xF5, three kept objects, a horizontal 0x22; one entity.
 */
static const uint8_t boss_objects[] = {WARP(1, 0x05, 0x77, 0x10, 0x20),
				       0x11, 0xBE, 0x12, 0x06, 0x13, 0x1C, 0x14, 0x1E, 0x15, 0x1F, 0x16, 0xF5,
				       0x33, 0x40, 0x63, 0x41, 0x19, 0x42, 0x82, 0x77, 0x22};
static const uint8_t boss_entity[] = {0x45, 0x99};
static const int fixed_rooms[4] = {0x1EF, 0x2F8, 0x22E, 0x22C};

/* Miniboss room k's objects: one at each of (3..6, 0..2)'s corner, a 0xDF tile, a kept tile. */
static const uint8_t mini_objects[] = {0x03, 0x70, 0x26, 0x71, 0x55, 0xDF, 0x57, 0x72};
static const uint8_t mini_entities[] = {0x11, 0x61, 0x22, 0x77, 0x33, 0x61};

static void plant_world(void)
{
	static const uint8_t r1ef[] = {0x11, 0x50, WARP(1, 0x03, 0x99, 0x01, 0x02)};
	static const uint8_t r2f8[] = {0x11, 0x51, WARP(1, 0x06, 0x98, 0x03, 0x04)};
	static const uint8_t r22e[] = {WARP(1, 0x06, 0x2F, 0x05, 0x06), 0x04, 0x52, WARP(1, 0x06, 0x30, 0x07, 0x08)};
	static const uint8_t r22c[] = {0x70, 0x53, 0x72, 0x54, 0x73, 0x55};
	static const uint8_t *fixed[4] = {r1ef, r2f8, r22e, r22c};
	static const size_t fixed_n[4] = {sizeof r1ef, sizeof r2f8, sizeof r22e, sizeof r22c};
	uint32_t a;
	int d, k;

	begin();
	for (d = 0; d < LADX_BOSS_ROOM_COUNT; d++) {
		plant_room(ladx_boss_room(d), boss_objects, sizeof boss_objects, boss_entity, sizeof boss_entity);
		plant_byte(DROP_TABLE + (uint32_t)ladx_boss_room(d), (uint8_t)(0x80 + d));
	}
	for (k = 0; k < 4; k++)
		plant_room(fixed_rooms[k], fixed[k], fixed_n[k], boss_entity, sizeof boss_entity);
	for (k = 0; k < LADX_MINIBOSS_KEY_COUNT; k++)
		plant_room(ladx_miniboss_room(k), mini_objects, sizeof mini_objects, mini_entities, sizeof mini_entities);
	for (a = 0; a < 64; a++)
		plant_byte(COLOR_WARPS + a, a % 3 == 0 ? 0x00 : (uint8_t)(0x40 + a));
	plant_byte(COLOR_WARPS + 1 + 3 * 8, 0x00);
	plant_byte(COLOR_WARPS + 2 + 6 * 8, 0x00);
}

static int bank(uint32_t offset)
{
	uint8_t v;
	int32_t origin;

	if (!ladx_enc_bank_value(&enc, offset, &v, &origin))
		test_fail(TEST, "byte %x readable", (unsigned)offset);
	return v;
}

static int32_t bank_origin(uint32_t offset)
{
	uint8_t v;
	int32_t origin;

	if (!ladx_enc_bank_value(&enc, offset, &v, &origin))
		test_fail(TEST, "byte %x readable", (unsigned)offset);
	return origin;
}

static const LadxRoom *reload(int nr)
{
	if (ladx_room_load(&room, &enc, nr, NULL) != LADX_ROOM_OK)
		test_fail(TEST, "room %x reloads", nr);
	return &room;
}

static int has_object(const LadxRoom *r, int x, int y, int type_id)
{
	size_t i;

	for (i = 0; i < r->object_count; i++)
		if (r->objects[i].kind != LADX_ROOM_WARP && r->objects[i].x == x && r->objects[i].y == y &&
		    r->objects[i].type_id == type_id)
			return 1;
	return 0;
}

static const LadxRoomObject *last(const LadxRoom *r)
{
	if (r->object_count == 0)
		test_fail(TEST, "room %x has no object", r->room);
	return &r->objects[r->object_count - 1];
}

static int warp_is(const LadxRoomObject *o, int type, int map, int lo, int x, int y)
{
	return o->kind == LADX_ROOM_WARP && o->warp_type == type && o->map_nr == map && (o->room & 0xFF) == lo &&
	       o->target_x == x && o->target_y == y;
}

static int entities_are(const LadxRoom *r, const int *e, size_t n)
{
	size_t i;

	if (r->entity_count != n)
		return 0;
	for (i = 0; i < n; i++)
		if (r->entities[i].x != e[3 * i] || r->entities[i].y != e[3 * i + 1] || r->entities[i].type_id != e[3 * i + 2])
			return 0;
	return 1;
}

static void identity(int *mapping)
{
	int d;

	for (d = 0; d < LADX_BOSS_ROOM_COUNT; d++)
		mapping[d] = d;
}

static LadxBossesStatus change(const int *mapping)
{
	memset(&raised, 0, sizeof raised);
	return ladx_bosses_change(&enc, span("bosses"), &room, &code, mapping, &raised);
}

static int cleaned(const LadxRoom *r)
{
	return has_object(r, 3, 1, 0x01) && has_object(r, 4, 1, 0xaf) && has_object(r, 5, 1, 0xb0) &&
	       has_object(r, 6, 1, 0xF1) && has_object(r, 3, 3, 0x40) && !has_object(r, 1, 1, 0xBE) &&
	       !has_object(r, 2, 1, 0x06) && r->objects[0].kind != LADX_ROOM_WARP && r->objects[0].type_id == 0x01;
}

static void test_identity(void)
{
	int mapping[LADX_BOSS_ROOM_COUNT];
	uint32_t a;
	int ok = 1;

	plant_world();
	identity(mapping);
	check(change(mapping) == LADX_BOSSES_OK, "the identity runs");
	for (a = 0; a < 64; a++)
		if (a == 1 + 3 * 8 || a == 2 + 6 * 8)
			ok &= bank(COLOR_WARPS + a) == 0x00;
		else
			ok &= bank(COLOR_WARPS + a) == (a % 3 == 0 ? 0xFF : 0x40 + (int)a);
	check(ok, "the color dungeon's zero warps made 0xFF, the two exempt kept");
	check(bank(BANK(4) + 0x521) == 0xA1 && bank(BANK(4) + 0x522) == 0xA2, "the genie patch written");
	check(bank(BANK(2) + 0x1FC8) == 0xB1 && bank(BANK(2) + 0x1FC9) == 0x06, "the eagle check on dungeon 6");
	end();
	check(strstr(out, "G bosses\n") && !strstr(strstr(out, "G bosses\n"), "\nT "), "no room stored");
}

/* The fish into D1, D1's boss into D4: flags, the cleaned rooms, the stairs, 0x1EF's warp back, the items. */
static void test_fish(void)
{
	static const int d4_boss[3] = {3, 2, 0x59};
	int mapping[LADX_BOSS_ROOM_COUNT];
	const LadxRoom *r;
	uint8_t source_item = synth.rom[BOSS_ITEM + 0x106];

	plant_world();
	plant_byte(DROP_TABLE + 0x106, (uint8_t)ladx_items_chest_id(LADX_ITEM_MAP));
	identity(mapping);
	mapping[0] = 3;
	mapping[3] = 0;
	check(change(mapping) == LADX_BOSSES_OK, "the fish into D1 runs");
	check(bank(BANK(0x14) + 0x06) == 0x2A && bank(BANK(0x14) + 0x66) == 0x21, "the room event flags");
	r = reload(0x106);
	check(cleaned(r) && r->entity_count == 0, "D1's room cleaned, its entities removed");
	check(has_object(r, 4, 4, 0xBE) && warp_is(last(r), 2, 3, 0xEF, 24, 16), "the stairs down to the fish");
	check(warp_is(last(reload(0x1EF)), 1, 0, 0x06, 72, 80) && has_object(reload(0x1EF), 1, 1, 0x50),
	      "0x1EF's last object made the warp back to D1");
	check(bank(BANK(3) + 0x1A0F) == 0xC1 && bank(BANK(3) + 0x1A10) == 0x06 && bank(BANK(3) + 0x1A11) == 0xD9,
	      "the fish's heart container opens D1's room, ld hl kept");
	check(bank(DROP_TABLE + 0x1FF) == ladx_items_chest_id(LADX_ITEM_MAP1) &&
		      bank_origin(DROP_TABLE + 0x1FF) == LADX_ENC_KNOWN,
	      "the generic map made D1's map, with no origin");
	check(bank(BOSS_ITEM + 0x1FF) == source_item && bank_origin(BOSS_ITEM + 0x1FF) == (int32_t)(BOSS_ITEM + 0x106),
	      "the second item a source copy");
	r = reload(0x166);
	check(r->object_count == 6 && r->objects[0].type_id == 0x01 && r->objects[1].type_id == 0xaf &&
		      has_object(r, 1, 1, 0xAC) && has_object(r, 8, 1, 0xAC) && has_object(r, 1, 6, 0xAC) &&
		      has_object(r, 8, 6, 0xAC),
	      "D4's room keeps two objects and gets four blocks");
	check(entities_are(r, d4_boss, 1), "D1's boss in D4");
	check(bank(BANK(2) + 0x1FC9) == 0x06, "the eagle stays in dungeon 6");
	end();
}

/* The fish from D7 and the eagle into D4. */
static void test_fish_from_d7_and_eagle(void)
{
	int mapping[LADX_BOSS_ROOM_COUNT];
	const LadxRoom *r;
	size_t i;
	int ok = 1;

	plant_world();
	identity(mapping);
	mapping[6] = 3;
	mapping[3] = 6;
	check(change(mapping) == LADX_BOSSES_OK, "the fish from D7 runs");
	r = reload(0x22E);
	for (i = 0; i < r->object_count; i++)
		if (r->objects[i].kind == LADX_ROOM_WARP)
			ok &= warp_is(&r->objects[i], 2, 3, 0xEF, 24, 16);
	check(ok && has_object(r, 4, 0, 0x52), "0x22E's warps all lead to the fish");
	check(warp_is(last(reload(0x1EF)), 1, 6, 0x2E, 24, 16), "0x1EF leads back to 0x22E");
	check(bank(BANK(3) + 0x1A10) == 0x2E && bank(BANK(3) + 0x1A11) == 0xDA, "the fish opens room 0x22E");
	check(bank(DROP_TABLE + 0x1FF) == 0x86 && bank_origin(DROP_TABLE + 0x1FF) == bank_origin(DROP_TABLE + 0x223),
	      "D7's item copied with its origin");
	check(bank(BANK(0x14) + 0x66) == 0x2A, "D4's event flag");
	check(bank(BANK(3) + 0x1A04) == 0xD1 && bank(BANK(3) + 0x1A05) == 0x66 && bank(BANK(3) + 0x1A06) == 0xD9,
	      "the eagle opens D4's room");
	r = reload(0x166);
	check(has_object(r, 3, 3, 0x2C) && has_object(r, 6, 3, 0x2B) && has_object(r, 3, 4, 0x2A) &&
		      has_object(r, 4, 4, 0xA2) && has_object(r, 5, 4, 0x21) && has_object(r, 6, 4, 0x29) &&
		      warp_is(last(r), 2, 6, 0xF8, 72, 80) && r->entity_count == 0,
	      "the stairs up to the eagle");
	ok = 0;
	for (i = 0; i < r->object_count; i++)
		ok |= r->objects[i].kind == LADX_ROOM_HORIZONTAL && r->objects[i].x == 4 && r->objects[i].y == 3 &&
		      r->objects[i].type_id == 0x22 && r->objects[i].count == 2;
	check(ok, "the stairs' horizontal piece");
	check(bank(BANK(0x20) + 0x2EB3 + 0x66) == 0, "D4's tileset made the stairs'");
	check(warp_is(last(reload(0x2F8)), 1, 3, 0x66, 72, 80), "0x2F8 leads back to D4");
	check(bank(DROP_TABLE + 0x2E8) == 0x83 && bank(BOSS_ITEM + 0x2E8) == synth.rom[BOSS_ITEM + 0x166],
	      "D4's items at the eagle's");
	check(bank(BANK(2) + 0x1FC9) == 0x03, "the eagle check on dungeon 3");
	end();
}

/* Facade into D1, the slime eel into D2, Hot Head into D7's cleaned room. */
static void test_plain(void)
{
	static const int facade[3] = {4, 3, 0x5A}, eel[3] = {4, 3, 0x5D}, hothead[3] = {4, 3, 0x62};
	int mapping[LADX_BOSS_ROOM_COUNT];
	const LadxRoom *r;

	plant_world();
	identity(mapping);
	mapping[0] = 5;
	mapping[1] = 4;
	mapping[6] = 7;
	check(change(mapping) == LADX_BOSSES_OK, "plain bosses run");
	check(entities_are(reload(0x106), facade, 1) && cleaned(reload(0x106)), "the facade in D1's cleaned room");
	check(bank(BANK(4) + 0x121D) == 0xE1 && bank(BANK(4) + 0x1227) == 0xE4 && bank(BANK(4) + 0x1280) == 0xE6,
	      "the facade's three patches");
	check(bank(BANK(0x14) + 0x06) == 0x21 && bank(BANK(0x14) + 0x2B) == 0x21, "the event flags");
	r = reload(0x12B);
	check(entities_are(r, eel, 1) && r->object_count == 2 && !has_object(r, 9, 1, 0x42) && has_object(r, 3, 6, 0x41) &&
		      r->objects[1].kind == LADX_ROOM_HORIZONTAL,
	      "the eel in D2 with rows 0 and 1 and (3,3) cleared");
	check(bank(BANK(0x20) + 0x2EB3 + 0x2B) == 0x06, "the eel's wall tiles");
	check(bank(BANK(0x14) + 0x3a0 + 6 + 8) == 0x00 && bank(BANK(0x14) + 0x3a0 + 7 + 16) == 0x2C &&
		      bank(BANK(0x14) + 0x3a0 + 7 + 24) == 0x23 && bank(BANK(0x14) + 0x3a0 + 6 + 40) == 0x00 &&
		      bank(BANK(0x14) + 0x520 + 7 + 16) == 0x2C && bank(BANK(0x14) + 0x520 + 7 + 24) == 0x23 &&
		      bank(BANK(0x14) + 0x520 + 6 + 40) == 0x00,
	      "D7's map bytes moved");
	r = reload(0x22E);
	check(r->object_count == 2 && warp_is(&r->objects[1], 1, 6, 0x23, 24, 112) && !has_object(r, 4, 0, 0x52),
	      "0x22E's last warp leads to 0x223, its door removed");
	check(bank(BANK(0x14) + 0x12E) == 0x00, "0x22E's event flag cleared");
	r = reload(0x22C);
	check(has_object(r, 0, 7, 0x03) && has_object(r, 2, 7, 0x03) && has_object(r, 3, 7, 0x55), "0x22C's two walls");
	r = reload(0x223);
	check(cleaned(r) && r->floor_object == 0x02 && entities_are(r, hothead, 1), "D7's room cleaned, its floor masked");
	check(has_object(r, 4, 0, 0xF0) && has_object(r, 1, 6, 0xBE) && warp_is(last(r), 1, 6, 0x2E, 24, 16),
	      "D7's door, stairs and warp");
	check(bank(BANK(2) + 0x1FC9) == 0xFF, "no eagle: the check made 0xFF");
	end();
}

/* The eagle into the color dungeon: its flag in the color dungeon's range, its warp back on map 0xFF. */
static void test_eagle_color(void)
{
	int mapping[LADX_BOSS_ROOM_COUNT];

	plant_world();
	identity(mapping);
	mapping[8] = 6;
	mapping[6] = 8;
	check(change(mapping) == LADX_BOSSES_OK, "the eagle into the color dungeon runs");
	check(bank(BANK(3) + 0x1A05) == 0xE0 && bank(BANK(3) + 0x1A06) == 0xDD, "the color dungeon's status flag");
	check(warp_is(last(reload(0x2F8)), 1, 0xFF, 0x00, 72, 80), "0x2F8 leads back on map 0xFF");
	check(bank(BANK(2) + 0x1FC9) == 0xFF, "the eagle check on dungeon 8 made 0xFF");
	end();
}

static void test_raises(void)
{
	static const uint8_t empty[] = {0x00};
	static const uint8_t bad[] = {0x01, 0x32};
	int mapping[LADX_BOSS_ROOM_COUNT];

	plant_world();
	plant_byte(DROP_TABLE + 0x300, (uint8_t)ladx_items_chest_id(LADX_ITEM_MAP));
	identity(mapping);
	mapping[8] = 3;
	mapping[3] = 8;
	check(change(mapping) == LADX_BOSSES_RAISED && raised.kind == LADX_OUTCOME_CRASH &&
		      strcmp(raised.type, "KeyError") == 0 && strcmp(raised.message, "'MAP9'") == 0,
	      "the color dungeon's generic map raises KeyError 'MAP9'");
	plant_world();
	plant_byte(DROP_TABLE + 0x300, (uint8_t)ladx_items_chest_id(LADX_ITEM_STONE_BEAK));
	check(change(mapping) == LADX_BOSSES_RAISED && strcmp(raised.message, "'STONE_BEAK9'") == 0,
	      "and its generic beak 'STONE_BEAK9'");
	plant_world();
	plant_room(0x1EF, empty, 0, empty, 0);
	identity(mapping);
	mapping[0] = 3;
	check(change(mapping) == LADX_BOSSES_RAISED && strcmp(raised.type, "IndexError") == 0 &&
		      strcmp(raised.message, "list assignment index out of range") == 0,
	      "an empty 0x1EF raises IndexError");
	plant_world();
	plant_key(INDOOR_A, 0x06, bad, sizeof bad);
	check(change(mapping) == LADX_BOSSES_FAILED, "a malformed boss room fails");
	plant_world();
	mapping[0] = 9;
	check(change(mapping) == LADX_BOSSES_FAILED, "a target out of range fails");
}

static LadxBossesStatus mini(const LadxMiniboss *mapping)
{
	memset(&raised, 0, sizeof raised);
	return ladx_minibosses_change(&enc, span("bosses"), &room, mapping, &raised);
}

static void test_minibosses(void)
{
	static const LadxMiniboss mapping[LADX_MINIBOSS_KEY_COUNT] = {
		LADX_MINIBOSS_GHOMA,	 LADX_MINIBOSS_BLAINO, LADX_MINIBOSS_ROLLING_BONES, LADX_MINIBOSS_DODONGO,
		LADX_MINIBOSS_GHOMA,	 LADX_MINIBOSS_CUE_BALL, LADX_MINIBOSS_HINOX,	     LADX_MINIBOSS_BLAINO,
		LADX_MINIBOSS_BLAINO,	 LADX_MINIBOSS_AVALAUNCH, LADX_MINIBOSS_BLAINO,	     LADX_MINIBOSS_GRIM_CREEPER};
	static const int ghoma[] = {1, 1, 0x61, 3, 3, 0x61, 2, 1, 0x5e, 2, 4, 0x5e};
	static const int hinox[] = {1, 1, 0x61, 3, 3, 0x61, 5, 2, 0x89};
	static const int bones[] = {1, 1, 0x61, 3, 3, 0x61, 8, 3, 0x81, 6, 3, 0x82};
	LadxMiniboss quiet[LADX_MINIBOSS_KEY_COUNT];
	static const uint8_t warped[] = {WARP(1, 0x02, 0x40, 0x01, 0x02)};
	const LadxRoom *r;
	int k;

	plant_world();
	plant_byte(BANK(3) + 0x372, 0x84);
	check(mini(mapping) == LADX_BOSSES_OK, "minibosses run");
	r = reload(0x111);
	check(entities_are(r, ghoma, 4), "warp entities kept, the miniboss's added");
	check(bank(BANK(3) + 0x34F) == 0xD5, "Ghoma away from D5 loses collision");
	check(warp_is(last(reload(0x128)), 1, 1, 0x36, 80, 80), "Blaino in D2 warps to D2's entrance");
	check(warp_is(last(reload(0x30C)), 1, 0xFF, 0x12, 80, 80), "Blaino in the color dungeon");
	check(warp_is(last(reload(0x23F)), 1, 7, 0x5D, 80, 80), "Blaino in D8");
	check(warp_is(last(reload(0x2E1)), 1, 0x15, 0xF0, 0x50, 0x7C), "Blaino in the moblin cave");
	check(entities_are(reload(0x145), bones, 4) && bank(BANK(3) + 0x372) == 0x95,
	      "Rolling Bones in D3 passes through walls");
	r = reload(0x164);
	check(!has_object(r, 5, 5, 0xDF) && has_object(r, 7, 5, 0x72) && r->object_count == 3, "Dodongo's floor kept whole");
	r = reload(0x1C5);
	check(r->object_count == 10 && has_object(r, 6, 4, 0x29) && has_object(r, 3, 3, 0x2c) &&
		      r->objects[8].kind == LADX_ROOM_HORIZONTAL && r->objects[8].type_id == 0x21 && r->objects[8].count == 2,
	      "Cue Ball's platform");
	r = reload(0x228);
	check(!has_object(r, 3, 0, 0x70) && !has_object(r, 6, 2, 0x71) && has_object(r, 5, 5, 0xDF),
	      "room 0x228 cleared for another than Grim Creeper");
	check(entities_are(r, hinox, 3), "Hinox's entity");
	end();

	for (k = 0; k < LADX_MINIBOSS_KEY_COUNT; k++)
		quiet[k] = (LadxMiniboss)k;
	quiet[1] = LADX_MINIBOSS_BLAINO;
	quiet[2] = LADX_MINIBOSS_ROLLING_BONES;
	quiet[6] = LADX_MINIBOSS_GRIM_CREEPER;
	quiet[0] = LADX_MINIBOSS_HINOX;
	plant_world();
	plant_room(0x128, warped, sizeof warped, mini_entities, sizeof mini_entities);
	plant_byte(BANK(3) + 0x372, 0x95);
	check(mini(quiet) == LADX_BOSSES_OK, "a quiet mapping runs");
	check(bank(BANK(3) + 0x34F) == synth.rom[BANK(3) + 0x34F], "Ghoma in D5 keeps collision");
	check(reload(0x128)->object_count == 1, "Blaino keeps a room's own warp");
	check(bank(BANK(3) + 0x372) == 0x95, "Rolling Bones' patch already applied");
	check(has_object(reload(0x228), 3, 0, 0x70), "Grim Creeper keeps room 0x228");
	plant_world();
	plant_byte(BANK(3) + 0x372, 0x12);
	check(mini(quiet) == LADX_BOSSES_FAILED, "Rolling Bones' patch over another byte fails");
	plant_world();
	quiet[3] = LADX_MINIBOSS_COUNT;
	check(mini(quiet) == LADX_BOSSES_FAILED, "a miniboss out of range fails");
}

static LadxTables tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;

/* A planted bosses.txt cut to units units: unit 1 one byte, units 2 and 3 the invented instructions. */
static void open_tables(int units)
{
	static const char order_text[] =
		"ladx-tables-order 1\nsection generator.py generateRom\n"
		"C bosses - patches.enemies.changeBosses(rom, world_setup.boss_mapping)\n"
		"C bosses - patches.enemies.changeMiniBosses(rom, world_setup.miniboss_mapping)\n";
	static char text[2048];
	LadxTablesFile order, group;
	LadxTablesError err;
	char head[32], *cut;

	snprintf(text, sizeof text,
		 "ladx-tables 1\ngroup bosses\ntables " TABLES "\nunit bosses.1\nE\nB %x 77\n"
		 "unit bosses.2\nE\nB %x b1b2\nB %x a1a2\n"
		 "unit bosses.3\nE\nB %x d1d2d3\nB %x c1c2c3\nB %x e1e2\nB %x e3e4\nB %x e5e6\n",
		 BANK(0x36) + 0x1C14, BANK(2) + 0x1FC8, BANK(4) + 0x521, BANK(3) + 0x1A04, BANK(3) + 0x1A0F,
		 BANK(4) + 0x121D, BANK(4) + 0x1226, BANK(4) + 0x127F);
	snprintf(head, sizeof head, "unit bosses.%d\n", units + 1);
	if ((cut = strstr(text, head)) != NULL)
		*cut = '\0';
	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "bosses.txt";
	group.text = span(text);
	if (ladx_tables_open(&tables, &order, &group, 1, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static LadxPipelineStatus run_pipeline(LadxPipelineError *err)
{
	static LadxSettings settings;
	int count;

	ladx_settings_defaults(&settings);
	ladx_settings_validate(&settings);
	memset(&randomizer, 0, sizeof randomizer);
	ladx_world_setup_defaults(&randomizer.ws);
	randomizer.ws.boss_mapping[0] = 5;
	randomizer.ws.miniboss_mapping[1] = LADX_MINIBOSS_BLAINO;
	plant_world();
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = NULL;
	pipeline.tables = &tables;
	pipeline.settings = &settings;
	pipeline.spot_count = 0;
	pipeline.randomizer = &randomizer;
	pipeline.rom = &rom;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	pipeline.ctx = NULL;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

static void test_group(void)
{
	static const int facade[3] = {4, 3, 0x5A};
	LadxPipelineError err;
	const char *seg;

	open_tables(3);
	check(run_pipeline(&err) == LADX_PIPELINE_OK, "the group runs in the pipeline");
	check(entities_are(reload(0x106), facade, 1), "the world setup's boss mapping");
	check(warp_is(last(reload(0x128)), 1, 1, 0x36, 80, 80), "the world setup's miniboss mapping");
	check(bank(BANK(4) + 0x521) == 0xA1 && bank(BANK(2) + 0x1FC8) == 0xB1 && bank(BANK(2) + 0x1FC9) == 0x06 &&
		      bank(BANK(4) + 0x1226) == 0xE3 && bank(BANK(0x36) + 0x1C14) == 0x77,
	      "bosses.txt's instructions and changeMiniBosses' unit written");
	check((seg = strstr(out, "G bosses\n")) != NULL && strstr(seg, "B d9c14 77\n") && strstr(seg, "B 1121d e1e2\n") &&
		      strstr(seg, "T entities 30c "),
	      "one segment of the group, its unit and rooms");
	open_tables(2);
	check(run_pipeline(&err) == LADX_PIPELINE_GROUP && strcmp(err.group, "bosses") == 0,
	      "a file without 3 units fails the group");
}

static void test_registry(void)
{
	const LadxPipelineGroup *groups;
	int count, g, entrances = -1, bosses = -1;

	groups = ladx_pipeline_groups(&count);
	for (g = 0; g < count; g++) {
		if (strcmp(groups[g].name, "entrances") == 0)
			entrances = g;
		if (strcmp(groups[g].name, "bosses") == 0)
			bosses = g;
	}
	check(bosses == entrances + 1 && entrances >= 0, "bosses registered right after entrances, LADXR's order");
	check(groups[bosses].needs_rom && groups[bosses].draw == NULL, "a rom-group with no draw");
}

int main(void)
{
	synth_rom_build(&synth, 0xB0551301u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM parses");
	test_identity();
	test_fish();
	test_fish_from_d7_and_eagle();
	test_plain();
	test_eagle_color();
	test_raises();
	test_minibosses();
	test_group();
	test_registry();
	printf("%s: changeBosses and changeMiniBosses on planted rooms of a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
