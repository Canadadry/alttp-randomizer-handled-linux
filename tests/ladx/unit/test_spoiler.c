#include "ladx_items.h"
#include "ladx_sha1.h"
#include "ladx_spoiler.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_spoiler"
#define JSON_MAX 131072
#define ENTITIES 1
#define TOP 2
#define BOTTOM 3
#define INDOOR_A 4
#define INDOOR_B 5
#define COLOR 6
#define BANK(b) ((uint32_t)(b) * SYNTH_ROM_BANK)
#define CHEST(room) (BANK(0x14) + 0x560u + (uint32_t)(room))
#define DROP(room) (BANK(0x3E) + 0x3800u + (uint32_t)(room))
#define OWL(room) (BANK(0x3E) + 0x3B16u + (uint32_t)(room))

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
static LadxSpoiler sp;
static LadxRandomizer randomizer;
static LadxSettings settings;
static char json[JSON_MAX];
static size_t json_len;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void discard(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	(void)text;
	(void)len;
}

static void to_json(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	if (json_len + len >= JSON_MAX)
		test_fail(TEST, "spoiler over %d bytes", JSON_MAX);
	memcpy(json + json_len, text, len);
	json_len += len;
	json[json_len] = '\0';
}

static LadxSpan span(const char *s)
{
	LadxSpan sp_;

	sp_.at = s;
	sp_.len = strlen(s);
	return sp_;
}

static void use_settings(const char *const *assignments)
{
	int i;

	ladx_settings_defaults(&settings);
	for (i = 0; assignments && assignments[i]; i++)
		if (ladx_settings_set(&settings, assignments[i]) != LADX_SETTINGS_OK)
			test_fail(TEST, "cannot set %s", assignments[i]);
	ladx_settings_validate(&settings);
}

/* --- spheres and outputJson against LADXR's own --------------------------------------------- */

static int hex_digit(int c)
{
	return c <= '9' ? c - '0' : c - 'a' + 10;
}

static void upper(const char *hex, char *out)
{
	size_t i;

	for (i = 0; hex[i]; i++)
		out[i] = (char)(hex[i] >= 'a' ? hex[i] - 'a' + 'A' : hex[i]);
	out[i] = '\0';
}

/*
 * LADXR's own spoilerLog.SpoilerLog at the pin (PYTHONHASHSEED 0 and 1
 * alike): Randomizer in SKIP_ROM_GENERATION mode on the settings and the
 * seed (the first k < 64 of SHA-256("ladx-spoiler <k>")[:16] whose fill is
 * ok), a Logic on WorldSetup() with goal 8 and the fill's boss and miniboss
 * mappings, each spot's read returning its placed item (RUPEES_20 for all
 * when junk), _loadItems, then outputJson's bytes: their SHA-1 and length.
 */
static const struct {
	const char *name;
	const char *assignments[5];
	int junk;
	const char *seed;
	const char *sha1;
	size_t len;
	int accessible, inaccessible;
} vectors[] = {
	{"default", {NULL}, 0, "ecb664d5f3a60671a7b3b72409f1958f", "5baae5cb7b58888dd5e91bc38b4777bb55faa7ca", 43454,
	 267, 0},
	{"junk", {NULL}, 1, "ecb664d5f3a60671a7b3b72409f1958f", "ef59fee555e93ae2820ce3298fe83bb422a9129c", 44048, 1,
	 266},
	{"goal-open", {"accessibility=goal", "goal=open", NULL}, 0, "ecb664d5f3a60671a7b3b72409f1958f",
	 "6ee48ebdb919b8e152f7765322f5329aa8445538", 44022, 48, 219},
	{"shuffled",
	 {"boss=shuffle", "miniboss=shuffle", "logic=hard", "dungeon_keys=keysanity", NULL},
	 0,
	 "533cb512743b1d07e796867c789f369e",
	 "f5a0cbcd135238a9782f1c80f64b6c26778cb41d",
	 43517,
	 267,
	 0},
	{"removed", {"dungeon_keys=removed", "witch=0", "textmode=none", NULL}, 0, "ecb664d5f3a60671a7b3b72409f1958f",
	 "8845c0820a5034d09ae4f5f62d52a9da402934b4", 54620, 348, 0},
};

#define HEAD "{\n  \"accessibleItems\": [\n    {\n      \"id\": \""

static void test_vectors(void)
{
	static int items[LADX_LOGIC_SPOT_CAP];
	uint8_t seed[16], digest[LADX_SHA1_SIZE];
	char seed_hex[33], got[41], what[160];
	const LadxWorldSetup *fill_ws;
	const LadxLogic *placed;
	LadxWorldSetup ws;
	LadxSha1 sha;
	size_t v;
	int i;

	for (v = 0; v < sizeof vectors / sizeof vectors[0]; v++) {
		use_settings(vectors[v].assignments);
		for (i = 0; i < 16; i++)
			seed[i] = (uint8_t)(hex_digit(vectors[v].seed[2 * i]) * 16 + hex_digit(vectors[v].seed[2 * i + 1]));
		ladx_randomize(&randomizer, &settings, seed, sizeof seed, NULL, NULL);
		placed = ladx_randomizer_placement(&randomizer);
		fill_ws = ladx_randomizer_world_setup(&randomizer);
		if (!placed || !fill_ws)
			test_fail(TEST, "%s: the fill is not ok", vectors[v].name);
		ladx_world_setup_defaults(&ws);
		memcpy(ws.boss_mapping, fill_ws->boss_mapping, sizeof ws.boss_mapping);
		memcpy(ws.miniboss_mapping, fill_ws->miniboss_mapping, sizeof ws.miniboss_mapping);
		upper(vectors[v].seed, seed_hex);
		snprintf(what, sizeof what, "%s: the spoiler's logic builds", vectors[v].name);
		check(ladx_spoiler_begin(&sp, &settings, &ws, seed_hex) == LADX_SPOILER_OK, what);
		check(ladx_logic_spot_count(&sp.logic) == ladx_logic_spot_count(placed), "as many spots as the fill's");
		for (i = 0; i < ladx_logic_spot_count(placed); i++)
			items[i] = vectors[v].junk ? LADX_ITEM_RUPEES_20 : ladx_logic_spot(placed, i)->item;
		check(ladx_spoiler_spheres(&sp, items) == LADX_SPOILER_OK, "the spheres are counted");
		snprintf(what, sizeof what, "%s: %d accessible and %d inaccessible items as LADXR's", vectors[v].name,
			 vectors[v].accessible, vectors[v].inaccessible);
		check(sp.accessible_count == vectors[v].accessible && sp.inaccessible_count == vectors[v].inaccessible,
		      what);
		json_len = 0;
		check(ladx_spoiler_render(&sp, to_json, NULL), "every name renders");
		ladx_sha1_init(&sha);
		ladx_sha1_update(&sha, (const uint8_t *)json, json_len);
		ladx_sha1_final(&sha, digest);
		for (i = 0; i < LADX_SHA1_SIZE; i++)
			snprintf(got + 2 * i, 3, "%02x", digest[i]);
		snprintf(what, sizeof what, "%s: outputJson's %lu bytes, SHA-1 %s (c: %lu bytes, %s)", vectors[v].name,
			 (unsigned long)vectors[v].len, vectors[v].sha1, (unsigned long)json_len, got);
		check(json_len == vectors[v].len && strcmp(got, vectors[v].sha1) == 0, what);
	}
	check(strncmp(json, HEAD, strlen(HEAD)) == 0 &&
		      strstr(json, "\"forwardfactor\": 0.0,\n") && strstr(json, "\"entrances\": {},\n") &&
		      json[json_len - 1] == '}' && json[json_len - 2] == '\n',
	      "outputJson's indent=\"  \" layout, no trailing newline");
}

/* --- reads on the synthetic ROM -------------------------------------------------------------- */

static void begin_encoder(void)
{
	if (ladx_wlog_begin(&wlog, span(TABLES), discard, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
}

static void plant_key(int table, LadxWlogKey key, const uint8_t *bytes, size_t n)
{
	int32_t known[128];
	size_t i;

	for (i = 0; i < n; i++)
		known[i] = LADX_ENC_KNOWN;
	if (ladx_enc_table(&enc, span("plant"), table, key, bytes, known, n) != LADX_ENC_OK)
		test_fail(TEST, "planting an entry");
}

static void plant_byte(uint32_t offset, uint8_t value)
{
	int32_t known = LADX_ENC_KNOWN;

	if (ladx_enc_bank(&enc, span("plant"), offset, &value, &known, 1) != LADX_ENC_OK)
		test_fail(TEST, "planting a byte");
}

/* Room nr (or alt key): header, objects, terminator; entities (index rooms) as (x, y, type) triples. */
static void plant_room(int nr, const char *alt, const uint8_t *objects, size_t n, const int *entities, size_t e)
{
	uint8_t raw[128];
	LadxWlogKey key;
	size_t i;
	int table;

	raw[0] = 0x01;
	raw[1] = 0x02;
	memcpy(raw + 2, objects, n);
	raw[n + 2] = 0xFE;
	key.index = 0;
	key.alt.at = alt;
	key.alt.len = alt ? strlen(alt) : 0;
	if (alt) {
		table = TOP;
	} else {
		key.index = (uint32_t)(nr & 0xFF) - (nr < 0x100 && nr >= 0x80 ? 0x80u : 0u);
		table = nr < 0x80 ? TOP : nr < 0x100 ? BOTTOM : nr < 0x200 ? INDOOR_A : nr < 0x300 ? INDOOR_B : COLOR;
	}
	plant_key(table, key, raw, n + 3);
	if (alt)
		return;
	for (i = 0; i < e; i++) {
		raw[2 * i] = (uint8_t)(entities[3 * i + 1] << 4 | entities[3 * i]);
		raw[2 * i + 1] = (uint8_t)entities[3 * i + 2];
	}
	raw[2 * e] = 0xFF;
	key.index = (uint32_t)nr;
	plant_key(ENTITIES, key, raw, 2 * e + 1);
}

/* The first spot of kind (in a dungeon when dungeon > 0, outside any when 0, either when -1, index when index >= 0). */
static int find(int kind, int dungeon, int index)
{
	int i;

	for (i = 0; i < ladx_logic_spot_count(&sp.logic); i++) {
		const LadxSpot *s = ladx_logic_spot(&sp.logic, i);
		int d = ladx_logic_location(&sp.logic, s->location)->dungeon;

		if ((int)s->kind == kind && (dungeon < 0 || (dungeon ? d > 0 : d < 0)) && (index < 0 || s->index == index))
			return i;
	}
	test_fail(TEST, "no %s spot", ladx_spot_kind_name(kind));
	return -1;
}

static int room_of(int spot)
{
	return ladx_logic_spot(&sp.logic, spot)->room;
}

static int dungeon_of(int spot)
{
	return ladx_logic_location(&sp.logic, ladx_logic_spot(&sp.logic, spot)->location)->dungeon;
}

static void expect_item(int spot, const char *name, const char *what)
{
	int item = LADX_ITEM_NONE;
	LadxSpoilerStatus st = ladx_spoiler_read_item(&sp, &enc, spot, &room, &item, &raised);

	check(st == LADX_SPOILER_OK && item != LADX_ITEM_NONE && strcmp(ladx_item_name(item), name) == 0, what);
}

static void expect_raise(int spot, const char *type, const char *message, const char *what)
{
	int item;

	memset(&raised, 0, sizeof raised);
	check(ladx_spoiler_read_item(&sp, &enc, spot, &room, &item, &raised) == LADX_SPOILER_RAISED &&
		      raised.kind == LADX_OUTCOME_CRASH && strcmp(raised.type, type) == 0 &&
		      strcmp(raised.message, message) == 0,
	      what);
}

static void begin_spoiler(const char *const *assignments)
{
	LadxWorldSetup ws;

	use_settings(assignments);
	ladx_world_setup_defaults(&ws);
	if (ladx_spoiler_begin(&sp, &settings, &ws, "00") != LADX_SPOILER_OK)
		test_fail(TEST, "the spoiler's logic does not build");
	begin_encoder();
}

static void test_reads(void)
{
	static const char *const no_witch[] = {"witch=0", NULL};
	static const char *const removed[] = {"dungeon_keys=removed", NULL};
	static const char *const owls[] = {"owlstatues=both", NULL};
	char what[128], message[96];
	int s, d;

	begin_spoiler(NULL);
	s = find(LADX_SPOT_CHEST, 0, -1);
	plant_byte(CHEST(room_of(s)), 0x0B);
	expect_item(s, "SWORD", "Chest: CHEST_ITEMS' key of bank 0x14 room + 0x560");
	plant_byte(CHEST(room_of(s)), 0x53);
	expect_raise(s, "ValueError", "Could not find chest contents in ROM (0x53)", "Chest: no gel rule, ValueError");
	plant_byte(CHEST(room_of(s)), 0x1A);
	expect_item(s, "KEY", "Chest: a generic key kept generic");

	s = find(LADX_SPOT_DUNGEON_CHEST, 1, -1);
	d = dungeon_of(s);
	plant_byte(CHEST(room_of(s)), 0x16);
	snprintf(what, sizeof what, "DungeonChest: MAP takes the dungeon's number (MAP%d)", d);
	snprintf(message, sizeof message, "MAP%d", d);
	expect_item(s, message, what);
	plant_byte(CHEST(room_of(s)), 0x2C);
	expect_item(s, "MAP1", "DungeonChest: a numbered map kept");

	s = find(LADX_SPOT_HEART_PIECE, 0, -1);
	plant_byte(DROP(room_of(s)), 0x54);
	expect_item(s, "GEL", "DroppedKey: a gel disguise reads GEL");
	plant_byte(DROP(room_of(s)), 0x56);
	expect_item(s, "GEL", "DroppedKey: 0x56, the last disguise, reads GEL");
	plant_byte(DROP(room_of(s)), 0x57);
	expect_item(s, "HAMMER", "DroppedKey: 0x57, past the disguises, is HAMMER");
	plant_byte(DROP(room_of(s)), 0xF7);
	expect_raise(s, "ValueError", "Could not find chest contents in ROM (0xf7)", "DroppedKey: an unknown byte");
	plant_byte(DROP(room_of(s)), 0x1A);
	snprintf(message, sizeof message, "Dungeon item outside of dungeon? HeartPiece:%03x", (unsigned)room_of(s));
	expect_raise(s, "AssertionError", message, "DroppedKey: a dungeon item outside a dungeon asserts");

	s = find(LADX_SPOT_DROPPED_KEY, 1, -1);
	plant_byte(DROP(room_of(s)), 0x19);
	snprintf(message, sizeof message, "NIGHTMARE_KEY%d", dungeon_of(s));
	expect_item(s, message, "DroppedKey: a nightmare key takes the dungeon's number");

	s = find(LADX_SPOT_KEY_HOLE, -1, -1);
	plant_byte(OWL(room_of(s)), 0x51);
	expect_item(s, "GEL", "KeyHole: bank 0x3E room + 0x3B16, gel rule");
	plant_byte(OWL(room_of(s)), 0x1A);
	expect_item(s, "KEY", "KeyHole: no dungeon number");

	s = find(LADX_SPOT_TRADE_SEQUENCE_ITEM, -1, -1);
	plant_byte(OWL(room_of(s)), 0x9C);
	expect_item(s, "TRADING_ITEM_HONEYCOMB", "TradeSequenceItem: bank 0x3E room + 0x3B16");
	plant_byte(OWL(room_of(s)), 0x52);
	expect_raise(s, "ValueError", "Could not find owl statue contents in ROM (0x52)",
		     "TradeSequenceItem: no gel rule, the owl statue's message");

	s = find(LADX_SPOT_MAD_BATTER, -1, -1);
	plant_byte(BANK(0x18) + 0x0F90u + ((unsigned)room_of(s) & 0x0Fu), 0x84);
	expect_item(s, "MAX_POWDER_UPGRADE", "MadBatter: bank 0x18 0x0F90 + room & 0x0F");
	plant_byte(BANK(0x18) + 0x0F90u + ((unsigned)room_of(s) & 0x0Fu), 0xFF);
	expect_raise(s, "ValueError", "Could not find mad batter contents in ROM (0xff)", "MadBatter: its message");

	s = find(LADX_SPOT_SEASHELL_MANSION_BONUS, -1, 1);
	plant_byte(BANK(0x3E) + 0x3E31u, 0x20);
	expect_item(s, "SEASHELL", "SeashellMansionBonus: bank 0x3E 0x3E30 + index");

	s = find(LADX_SPOT_SHOP_ITEM, -1, 0);
	plant_byte(DROP(room_of(s)), 0x02);
	expect_item(s, "BOW", "ShopItem 0: the drop table");
	s = find(LADX_SPOT_SHOP_ITEM, -1, 1);
	plant_byte(CHEST(room_of(s)), 0x05);
	expect_item(s, "PEGASUS_BOOTS", "ShopItem 1: the chest table");
	plant_byte(CHEST(room_of(s)), 0xFE);
	expect_raise(s, "ValueError", "Could not find shop item contents in ROM (0xfe)", "ShopItem: its message");

	s = find(LADX_SPOT_TUNIC_FAIRY, -1, 1);
	plant_byte(BANK(0x36) + 0x11C0u, 0x88);
	expect_item(s, "BLUE_TUNIC", "TunicFairy: bank 0x36 0x11BF + index");

	s = find(LADX_SPOT_WITCH, -1, -1);
	plant_byte(BANK(0x05) + 0x08EFu, 0x00);
	plant_byte(BANK(0x05) + 0x08D5u, 0x06);
	expect_item(s, "OCARINA", "Witch: 0x08EF zero, the item at 0x08D5");
	plant_byte(BANK(0x05) + 0x08EFu, 0x3C);
	expect_item(s, "MAGIC_POWDER", "Witch: 0x08EF non-zero reads MAGIC_POWDER");

	s = find(LADX_SPOT_BOOMERANG_GUY, -1, -1);
	plant_byte(BANK(0x19) + 0x06C5u, 0x00);
	plant_byte(BANK(0x19) + 0x072Bu, 0x0E);
	expect_item(s, "BOOMERANG", "BoomerangGuy: 0x06C5 zero, CHEST_ITEMS at 0x072B");
	plant_byte(BANK(0x19) + 0x06C5u, 0x01);
	plant_byte(BANK(0x19) + 0x0640u, 0x0B);
	expect_item(s, "SHOVEL", "BoomerangGuy: else INVENTORY_MAP at 0x0640");
	plant_byte(BANK(0x19) + 0x0640u, 0x40);
	expect_raise(s, "ValueError", "", "BoomerangGuy: an unknown inventory byte, ValueError()");

	s = find(LADX_SPOT_BEACH_SWORD, -1, -1);
	{
		static const int sword[] = {3, 4, 0x31};
		static const int crab[] = {3, 4, 0x30};

		plant_room(0x0F2, NULL, NULL, 0, sword, 1);
		plant_byte(DROP(0x0F2), 0x0A);
		expect_item(s, "SWORD", "BeachSword: entity 0x31 in room 0xF2 reads SWORD");
		plant_room(0x0F2, NULL, NULL, 0, crab, 1);
		expect_item(s, "BOMB", "BeachSword: else the drop table");
	}

	s = find(LADX_SPOT_TOADSTOOL, -1, -1);
	plant_byte(DROP(room_of(s)), 0x0D);
	expect_item(s, "MAGNIFYING_LENS", "Toadstool: several options, the drop table");
	begin_spoiler(no_witch);
	s = find(LADX_SPOT_TOADSTOOL, -1, -1);
	plant_byte(DROP(room_of(s)), 0x0D);
	expect_item(s, "TOADSTOOL", "Toadstool: one option (witch off) reads TOADSTOOL");

	begin_spoiler(owls);
	s = find(LADX_SPOT_OWL_STATUE, 1, -1);
	plant_byte(OWL(room_of(s)), 0x53);
	expect_raise(s, "ValueError", "Could not find owl statue contents in ROM (0x53)", "OwlStatue: no gel rule");
	plant_byte(OWL(room_of(s)), 0x18);
	snprintf(message, sizeof message, "STONE_BEAK%d", dungeon_of(s));
	expect_item(s, message, "OwlStatue: a beak takes the dungeon's number");
	s = find(LADX_SPOT_OWL_STATUE, 0, -1);
	plant_byte(OWL(room_of(s)), 0x18);
	snprintf(message, sizeof message, "Dungeon item outside of dungeon? OwlStatue:%03x", (unsigned)room_of(s));
	expect_raise(s, "AssertionError", message, "OwlStatue: a dungeon item outside a dungeon asserts");

	begin_spoiler(removed);
	s = find(LADX_SPOT_KEY_LOCATION, -1, -1);
	expect_item(s, ladx_item_name(ladx_logic_spot(&sp.logic, s)->default_item), "KeyLocation: its key");
	check(strncmp(ladx_item_name(ladx_logic_spot(&sp.logic, s)->default_item), "KEY", 3) == 0,
	      "KeyLocation: a key");
}

/* --- the world setup read back --------------------------------------------------------------- */

static const int boss_rooms[9] = {0x106, 0x12b, 0x15a, 0x166, 0x185, 0x1bc, 0x223, 0x234, 0x300};

static void test_bosses(void)
{
	static const uint8_t no_objects[1];
	static const uint8_t fish_warp[] = {0xE1, 0x00, 0xEF, 0x48, 0x50};   /* indoor A: 0x1EF */
	static const uint8_t eagle_warp[] = {0xE1, 0x06, 0xF8, 0x48, 0x50};  /* indoor B: 0x2F8 */
	static const uint8_t other_warp[] = {0xE1, 0x00, 0x10, 0x48, 0x50};
	static const int genie[] = {4, 3, 0x5A, 1, 1, 0x20};
	static const int bad[] = {4, 3, 0x5C};
	static const int nightmare[] = {4, 3, 0x13};
	static const int last_cue[] = {1, 1, 0x20, 1, 1, 0x8e};
	static const int dodongo[] = {3, 2, 0x60, 5, 2, 0x60};
	static const int other[] = {2, 2, 0x55};
	int mapping[9], d, k;
	LadxMiniboss mini[LADX_MINIBOSS_KEY_COUNT];

	begin_encoder();
	for (d = 0; d < 9; d++)
		plant_room(boss_rooms[d], NULL, other_warp, sizeof other_warp, NULL, 0);
	plant_room(boss_rooms[0], NULL, NULL, 0, genie, 2);
	plant_room(boss_rooms[1], NULL, fish_warp, sizeof fish_warp, NULL, 0);
	plant_room(boss_rooms[2], NULL, eagle_warp, sizeof eagle_warp, NULL, 0);
	check(ladx_spoiler_read_bosses(&enc, &room, mapping, &raised) == LADX_SPOILER_OK && mapping[0] == 5 &&
		      mapping[1] == 3 && mapping[2] == 6 && mapping[3] == 3 && mapping[8] == 8,
	      "readBossMapping: first entity's key, the fish's and the eagle's warps, else the dungeon itself");
	plant_room(boss_rooms[4], NULL, NULL, 0, bad, 1);
	memset(&raised, 0, sizeof raised);
	check(ladx_spoiler_read_bosses(&enc, &room, mapping, &raised) == LADX_SPOILER_RAISED &&
		      strcmp(raised.type, "ValueError") == 0 && strcmp(raised.message, "(4, 3, 92) is not in list") == 0,
	      "readBossMapping: an entity no boss has raises list.index's ValueError");
	plant_room(boss_rooms[4], NULL, no_objects, 0, NULL, 0);
	check(ladx_spoiler_read_bosses(&enc, &room, mapping, &raised) == LADX_SPOILER_RAISED &&
		      strcmp(raised.type, "IndexError") == 0,
	      "readBossMapping: no entity and no object raises IndexError");
	plant_room(boss_rooms[4], NULL, NULL, 0, nightmare, 1);
	check(ladx_spoiler_read_bosses(&enc, &room, mapping, &raised) == LADX_SPOILER_FAILED,
	      "readBossMapping: a nightmare boss is outside tier 1");

	for (k = 0; k < LADX_MINIBOSS_KEY_COUNT; k++)
		plant_room(ladx_miniboss_room(k), NULL, NULL, 0, dodongo, 2);
	plant_room(ladx_miniboss_room(3), NULL, NULL, 0, last_cue, 2);
	check(ladx_spoiler_read_minibosses(&enc, &room, mini, &raised) == LADX_SPOILER_OK &&
		      mini[0] == LADX_MINIBOSS_DODONGO && mini[3] == LADX_MINIBOSS_CUE_BALL &&
		      mini[11] == LADX_MINIBOSS_DODONGO,
	      "readMiniBossMapping: the last entity's type names the miniboss");
	plant_room(ladx_miniboss_room(5), NULL, NULL, 0, other, 1);
	check(ladx_spoiler_read_minibosses(&enc, &room, mini, &raised) == LADX_SPOILER_FAILED,
	      "readMiniBossMapping: a key with no match is outside tier 1");
	plant_room(ladx_miniboss_room(5), NULL, NULL, 0, NULL, 0);
	memset(&raised, 0, sizeof raised);
	check(ladx_spoiler_read_minibosses(&enc, &room, mini, &raised) == LADX_SPOILER_RAISED &&
		      strcmp(raised.type, "IndexError") == 0,
	      "readMiniBossMapping: a room with no entity raises IndexError");
}

#define WARP_IN(lo, x, y) 0xE1, 0x10, (lo), (x), (y) /* indoor B: room 0x200 + lo */
#define WARP_OUT(nr, x, y) 0xE0, 0x00, (nr), (x), (y)

static void test_entrances(void)
{
	/* Three planted entrances: a, b (same overworld room, told apart by x/y) and c, its warp in by index 1. */
	static const LadxEntranceInfo info[3] = {
		{"a", 0x20, NULL, 0, 0x2A0, -1, NULL},
		{"b", 0x20, NULL, 1, 0x2A1, -1, NULL},
		{"c", 0x30, NULL, 1, 0x2A2, -1, NULL},
	};
	static const LadxEntrancePos pos[3] = {{48, 40}, {48, 32}, {-1, -1}};
	static const uint8_t room20[] = {WARP_IN(0xA0, 1, 2), WARP_IN(0xA2, 1, 2)};
	static const uint8_t room30[] = {WARP_IN(0xA1, 1, 2), WARP_IN(0xA0, 1, 2)};
	static const uint8_t in_a[] = {WARP_OUT(0x20, 48, 32)};
	static const uint8_t in_a2[] = {WARP_OUT(0x20, 48, 40)};
	static const uint8_t in_b[] = {WARP_OUT(0x30, 7, 7)};
	static const uint8_t in_c[] = {0x11, 0x20};
	int mapping[6];

	begin_encoder();
	plant_room(0x20, NULL, room20, sizeof room20, NULL, 0);
	plant_room(0x30, NULL, room30, sizeof room30, NULL, 0);
	plant_room(0x2A0, NULL, in_a, sizeof in_a, NULL, 0);
	plant_room(0x2A1, NULL, in_b, sizeof in_b, NULL, 0);
	plant_room(0x2A2, NULL, in_c, sizeof in_c, NULL, 0);
	check(ladx_spoiler_read_entrances(&enc, &room, info, pos, 3, mapping) == LADX_SPOILER_OK, "readEntrances runs");
	check(mapping[0] == 3 && mapping[1] == 5 && mapping[2] == 3,
	      "readEntrances: the warp at the entrance's index names the entrance whose target it reaches");
	check(mapping[3] == 1, "readEntrances: a warp out matching an entrance's room, x and y names that entrance");
	plant_room(0x2A0, NULL, in_a2, sizeof in_a2, NULL, 0);
	check(ladx_spoiler_read_entrances(&enc, &room, info, pos, 3, mapping) == LADX_SPOILER_OK && mapping[3] == 0,
	      "readEntrances: the same x with another y is no match, the last match wins");
	check(mapping[4] == 2, "readEntrances: x/y None matches any warp into the room");
	check(mapping[5] == -1, "readEntrances: a target room with no warp leaves its :inside absent");
}

static void test_build(void)
{
	static const char *const race[] = {"race=1", NULL};
	static const uint8_t no_warp[] = {0x11, 0x20};
	int i;

	use_settings(race);
	begin_encoder();
	check(ladx_spoiler_build(&sp, &enc, &settings, &room, &raised) == LADX_SPOILER_NONE && !sp.built,
	      "race: no spoiler, as randomizer.py skips it");
	use_settings(NULL);
	plant_byte(BANK(0) + 7u, 0x01);
	memset(&raised, 0, sizeof raised);
	check(ladx_spoiler_build(&sp, &enc, &settings, &room, &raised) == LADX_SPOILER_RAISED &&
		      strcmp(raised.type, "RaceRomException") == 0,
	      "bank 0 offset 7 equal to 1 raises RaceRomException");
	plant_byte(BANK(0) + 7u, 0x00);
	for (i = 0; i < 16; i++)
		plant_byte(BANK(0x3E) + 0x2F00u + (uint32_t)i, (uint8_t)(0x0F * i + 0x0A));
	plant_room(0x010, NULL, no_warp, sizeof no_warp, NULL, 0);
	check(ladx_spoiler_build(&sp, &enc, &settings, &room, &raised) == LADX_SPOILER_FAILED && !sp.built,
	      "room 0x010 with no warp: the dungeon-only mapping, outside tier 1");
	check(strcmp(sp.seed, "0A192837465564738291A0AFBECDDCEB") == 0,
	      "readHexSeed: bank 0x3E 0x2F00..0x2F0F in upper-case hex");
}

/* SHA-1 of "<name> <x> <y>\n" per entry of LADXR's own ENTRANCE_INFO (None as -1), in its order. */
static void test_entrance_pos(void)
{
	const LadxEntranceInfo *info;
	const LadxEntrancePos *pos;
	uint8_t digest[LADX_SHA1_SIZE];
	char line[96], got[41];
	LadxSha1 sha;
	size_t n, k;
	int i;

	info = ladx_entrance_info(&n);
	pos = ladx_entrance_pos(&k);
	check(n == k && k == LADX_ENTRANCE_INFO_COUNT, "one x/y per ENTRANCE_INFO entry");
	ladx_sha1_init(&sha);
	for (k = 0; k < n; k++) {
		snprintf(line, sizeof line, "%s %d %d\n", info[k].name, pos[k].x, pos[k].y);
		ladx_sha1_update(&sha, (const uint8_t *)line, strlen(line));
	}
	ladx_sha1_final(&sha, digest);
	for (i = 0; i < LADX_SHA1_SIZE; i++)
		snprintf(got + 2 * i, 3, "%02x", digest[i]);
	check(strcmp(got, "01e47b61cfe65259ef4e2004319e95070634326c") == 0, "ENTRANCE_INFO's x and y equal LADXR's");
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	synth_rom_build(&synth, 0x5B011E12u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM does not parse");
	test_entrance_pos();
	test_vectors();
	test_reads();
	test_bosses();
	test_entrances();
	test_build();
	printf("%s: LADXR's outputJson on 5 fill placements, each kind's read, the world setup read back (%d checks)\n",
	       TEST, checks);
	return 0;
}
