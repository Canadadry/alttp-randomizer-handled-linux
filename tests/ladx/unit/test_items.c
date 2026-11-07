#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_items"
#define OUT_MAX (1 << 20)
#define TEXTS 0
#define BANK(b) ((uint32_t)(b) * SYNTH_ROM_BANK)
#define CHEST(room) (BANK(0x14) + 0x560u + (uint32_t)(room))
#define DROP(room) (BANK(0x3E) + 0x3800u + (uint32_t)(room))
#define BOSS_ITEM(room) (BANK(0x3E) + 0x3300u + (uint32_t)(room))
#define OWL(room) (BANK(0x3E) + 0x3B16u + (uint32_t)(room))
#define WITCH (BANK(0x05) + 0x08D5u)
#define GIFT (BANK(0x19) + 0x072Bu)
#define HOOK (BANK(0x02) + 0x2FA9u)
#define ENTITIES_TABLE 1
#define BOTTOM_TABLE 3

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static LadxRomTables rom_tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static LadxSettings settings;
static SynthRom synth;
static char out[OUT_MAX];
static size_t out_len;
static char order_text[1024], early_text[4096], items_text[4096];
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

/*
 * The planted items.txt: invented bytes at LADXR's offsets, one unit per call
 * (README "Patch tables"); unit k's keyhole handler is 12 bytes 0x10 * k + j.
 */
static void plant_files(int witch_byte)
{
	size_t len;
	int k, j;

	snprintf(order_text, sizeof order_text,
		 "ladx-tables-order 1\nsection generator.py generateRom\ntable early early.1 patches.early(rom)\n"
		 "C items - patches.chest.setMultiChest(rom, world_setup.multichest)\n"
		 "C items - spot.patch(rom, spot.item)\n");
	snprintf(early_text, sizeof early_text,
		 "ladx-tables 1\ngroup early\ntables " TABLES "\nunit early.1\nE\n"
		 "B %x f0\nB %x %02x\nB %x 00\nT entities f2 21314550ff\nT rooms_overworld_bottom 72 0102fe\n",
		 HOOK, WITCH, witch_byte, GIFT);
	len = (size_t)snprintf(items_text, sizeof items_text,
			       "ladx-tables 1\ngroup items\ntables " TABLES "\n"
			       "unit items.1\nE\nB %x a1a2a3\nB %x a4a5a6\n"
			       "unit items.2\nE\nB %x b1b2\n"
			       "unit items.3\nE\nB %x c1c2\n"
			       "unit items.4\nE\nB %x d1d2\nB %x d3d4\nB %x d5d6\nB %x d7d8\n"
			       "unit items.5\nE\nB %x e1e2e3\nB %x e4\n"
			       "unit items.6\nE\nB %x cdfb7f00\nB %x f1\nB %x f2f3\n"
			       "unit items.7\nE\nB %x f4f5\nB %x f6f7f8\n",
			       BANK(3) + 0x1B9Cu, BANK(3) + 0x244Du, BANK(5) + 0x0CDAu, BANK(3) + 0x145Du,
			       BANK(0x19) + 0x063Fu, BANK(0x19) + 0x06C1u, BANK(0x19) + 0x06FCu, BANK(0x19) + 0x0710u,
			       BANK(0x19) + 0x0668u, BANK(0x19) + 0x069Cu, HOOK, BANK(2) + 0x3D00u, BANK(2) + 0x3FFBu,
			       BANK(2) + 0x3831u, BANK(0x15) + 0x00E9u);
	for (k = 8; k <= 12; k++) {
		len += (size_t)snprintf(items_text + len, sizeof items_text - len, "unit items.%d\nE\nB %x ", k,
					BANK(2) + 0x3FEFu);
		for (j = 0; j < 12; j++)
			len += (size_t)snprintf(items_text + len, sizeof items_text - len, "%02x", 0x10 * k + j);
		len += (size_t)snprintf(items_text + len, sizeof items_text - len, "\n");
	}
}

static void open_tables(void)
{
	LadxTablesFile order, groups[2];
	LadxTablesError err;

	order.name = "order.txt";
	order.text = span(order_text);
	groups[0].name = "early.txt";
	groups[0].text = span(early_text);
	groups[1].name = "items.txt";
	groups[1].text = span(items_text);
	if (ladx_tables_open(&tables, &order, groups, 2, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

/* An ok fill of the settings: the first of the seeds 0, 1, ... whose fill is ok. */
static void randomize(const char *const *assignments)
{
	uint8_t seed[16];
	int i, n;

	ladx_settings_defaults(&settings);
	for (i = 0; assignments[i]; i++)
		if (ladx_settings_set(&settings, assignments[i]) != LADX_SETTINGS_OK)
			test_fail(TEST, "cannot set %s", assignments[i]);
	ladx_settings_validate(&settings);
	for (n = 0; n < 64; n++) {
		memset(seed, 0x5A, sizeof seed);
		seed[0] = (uint8_t)n;
		ladx_randomize(&randomizer, &settings, seed, sizeof seed, NULL, NULL);
		if (ladx_randomizer_placement(&randomizer))
			return;
	}
	test_fail(TEST, "no ok fill below 64 seeds");
}

/* The spot of kind in room (index -1: any), placed item; its index. */
static int place(int kind, int room, int index, int item)
{
	int i;

	for (i = 0; i < ladx_logic_spot_count(&randomizer.logic); i++) {
		const LadxSpot *s = ladx_logic_spot(&randomizer.logic, i);

		if ((int)s->kind == kind && s->room == room && (index < 0 || s->index == index)) {
			if (!ladx_logic_place(&randomizer.logic, i, item))
				test_fail(TEST, "cannot place");
			return i;
		}
	}
	test_fail(TEST, "no %s spot in room %x", ladx_spot_kind_name(kind), (unsigned)room);
	return -1;
}

static LadxPipelineStatus run(LadxPipelineError *err)
{
	int count;

	out_len = 0;
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	pipeline.tables = &tables;
	pipeline.settings = &settings;
	pipeline.spot_count = 0;
	pipeline.randomizer = &randomizer;
	pipeline.rnd = &randomizer.rnd;
	pipeline.global = &randomizer.global;
	pipeline.rom = &rom;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	pipeline.ctx = NULL;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

static void run_ok(const char *what)
{
	LadxPipelineError err;

	if (run(&err) != LADX_PIPELINE_OK)
		test_fail(TEST, "%s: the pipeline fails (status %d, group %s)", what, (int)err.status,
			  err.group ? err.group : "-");
}

static int byte_at(uint32_t offset)
{
	uint8_t v;
	int32_t origin;

	return ladx_enc_bank_value(&enc, offset, &v, &origin) ? v : -1;
}

static int32_t origin_at(uint32_t offset)
{
	uint8_t v;
	int32_t origin = 0;

	ladx_enc_bank_value(&enc, offset, &v, &origin);
	return origin;
}

static int bytes_are(uint32_t offset, const char *hex)
{
	size_t i, n = strlen(hex) / 2;
	unsigned v;

	for (i = 0; i < n; i++) {
		if (sscanf(hex + 2 * i, "%2x", &v) != 1 || byte_at(offset + (uint32_t)i) != (int)v)
			return 0;
	}
	return 1;
}

static int unwritten(uint32_t offset)
{
	return origin_at(offset) == (int32_t)offset;
}

/* The text entry's current bytes equal formatText(text, ask=ask). */
static int text_is(unsigned id, const char *text, const char *ask)
{
	uint8_t want[1024], got[1024];
	int32_t origins[1024];
	size_t wn, gn;
	LadxOutcome raised;
	LadxWlogKey key;

	if (ladx_format_text(text, strlen(text), 0, ask, want, sizeof want, &wn, &raised) != LADX_TEXT_OK)
		test_fail(TEST, "formatText of %s", text);
	key.index = id;
	key.alt.at = NULL;
	key.alt.len = 0;
	return ladx_enc_entry_value(&enc, TEXTS, key, got, origins, sizeof got, &gn) && gn == wn &&
	       memcmp(got, want, wn) == 0;
}

static int entry_is(int table, unsigned id, const char *hex)
{
	uint8_t got[256];
	int32_t origins[256];
	size_t n, i;
	unsigned v;
	LadxWlogKey key;

	key.index = id;
	key.alt.at = NULL;
	key.alt.len = 0;
	if (!ladx_enc_entry_value(&enc, table, key, got, origins, sizeof got, &n) || n != strlen(hex) / 2)
		return 0;
	for (i = 0; i < n; i++)
		if (sscanf(hex + 2 * i, "%2x", &v) != 1 || got[i] != v)
			return 0;
	return 1;
}

static const char *const defaults[] = {NULL};

static void test_lookups(void)
{
	check(ladx_items_chest_id(LADX_ITEM_POWER_BRACELET) == 0x00 && ladx_items_chest_id(LADX_ITEM_SWORD) == 0x0B &&
		      ladx_items_chest_id(LADX_ITEM_ROOSTER) == 0x96 && ladx_items_chest_id(LADX_ITEM_KEY0) == 0x2B &&
		      ladx_items_chest_id(LADX_ITEM_NIGHTMARE_KEY0) == 0x4F &&
		      ladx_items_chest_id(LADX_ITEM_INSTRUMENT8) == 0x95 &&
		      ladx_items_chest_id(LADX_ITEM_TRADING_ITEM_MAGNIFYING_GLASS) == 0xA4,
	      "CHEST_ITEMS' ids");
	check(ladx_items_chest_id(LADX_ITEM_KEY9) == -1 && ladx_items_chest_id(LADX_ITEM_RUPEES) == -1 &&
		      ladx_items_chest_id(LADX_ITEM_NONE) == -1 && ladx_items_chest_id(LADX_ITEM_COUNT) == -1,
	      "an item CHEST_ITEMS lacks has no id");
	check(ladx_items_inventory_id(LADX_ITEM_BOOMERANG) == 0x0D && ladx_items_inventory_id(LADX_ITEM_TOADSTOOL) == 0x0E &&
		      ladx_items_inventory_id(LADX_ITEM_SWORD) == 0x01 && ladx_items_inventory_id(LADX_ITEM_FLIPPERS) == -1,
	      "INVENTORY_MAP's ids");
	check(ladx_items_dungeon_item(LADX_ITEM_KEY3, 3) == LADX_ITEM_KEY &&
		      ladx_items_dungeon_item(LADX_ITEM_MAP0, 0) == LADX_ITEM_MAP &&
		      ladx_items_dungeon_item(LADX_ITEM_NIGHTMARE_KEY8, 8) == LADX_ITEM_NIGHTMARE_KEY &&
		      ladx_items_dungeon_item(LADX_ITEM_STONE_BEAK1, 1) == LADX_ITEM_STONE_BEAK &&
		      ladx_items_dungeon_item(LADX_ITEM_COMPASS5, 5) == LADX_ITEM_COMPASS,
	      "a dungeon item in its own dungeon is reduced");
	check(ladx_items_dungeon_item(LADX_ITEM_KEY3, 4) == LADX_ITEM_KEY3 &&
		      ladx_items_dungeon_item(LADX_ITEM_KEY3, -1) == LADX_ITEM_KEY3 &&
		      ladx_items_dungeon_item(LADX_ITEM_KEY_CAVERN_OPENED, 3) == LADX_ITEM_KEY_CAVERN_OPENED &&
		      ladx_items_dungeon_item(LADX_ITEM_SWORD, 1) == LADX_ITEM_SWORD,
	      "another dungeon's item, outside a dungeon, an OPENED item or another item is kept");
}

static void test_tables(void)
{
	int spot;

	randomize(defaults);
	randomizer.ws.multichest = LADX_ITEM_RUPEES_50;
	place(LADX_SPOT_DUNGEON_CHEST, 0x1B6, -1, LADX_ITEM_NIGHTMARE_KEY6);
	place(LADX_SPOT_CHEST, 0x2BF, -1, LADX_ITEM_KEY6);
	place(LADX_SPOT_DROPPED_KEY, 0x169, -1, LADX_ITEM_GEL);
	place(LADX_SPOT_HEART_CONTAINER, 0x166, -1, LADX_ITEM_KEY4);
	place(LADX_SPOT_HEART_CONTAINER, 0x223, -1, LADX_ITEM_SWORD);
	place(LADX_SPOT_SONG, 0x92, -1, LADX_ITEM_BOW);
	place(LADX_SPOT_DROPPED_KEY, 0x154, -1, LADX_ITEM_NIGHTMARE_KEY3);
	place(LADX_SPOT_ANGLER_KEY, 0xCE, -1, LADX_ITEM_BOMB);
	place(LADX_SPOT_MAD_BATTER, 0x1E2, -1, LADX_ITEM_FEATHER);
	place(LADX_SPOT_TUNIC_FAIRY, 0x301, 1, LADX_ITEM_OCARINA);
	place(LADX_SPOT_SEASHELL_MANSION_BONUS, 0x2E9, 1, LADX_ITEM_SHOVEL);
	place(LADX_SPOT_START_ITEM, 0x2A3, -1, LADX_ITEM_SWORD);
	place(LADX_SPOT_SHOP_ITEM, 0x2A1, 0, LADX_ITEM_BOMB);
	place(LADX_SPOT_SHOP_ITEM, 0x2A1, 1, LADX_ITEM_HOOKSHOT);
	place(LADX_SPOT_BOOMERANG_GUY, 0x1F5, -1, LADX_ITEM_FLIPPERS);
	place(LADX_SPOT_TRADE_SEQUENCE_ITEM, 0x2FE, -1, LADX_ITEM_BOOMERANG);
	place(LADX_SPOT_TRADE_SEQUENCE_ITEM, 0x2A8, -1, LADX_ITEM_TRADING_ITEM_BROOM);
	place(LADX_SPOT_KEY_HOLE, 0xD3, -1, LADX_ITEM_TAIL_CAVE_OPENED);
	place(LADX_SPOT_KEY_HOLE, 0x2B, -1, LADX_ITEM_ANGLER_TUNNEL_OPENED);
	place(LADX_SPOT_KEY_HOLE, 0x2C3, -1, LADX_ITEM_CASTLE_GATE_OPENED);
	place(LADX_SPOT_KEY_HOLE, 0xE, -1, LADX_ITEM_EAGLE_TOWER_OPENED);
	place(LADX_SPOT_KEY_HOLE, 0xB5, -1, LADX_ITEM_KEY_CAVERN_OPENED);
	place(LADX_SPOT_KEY_HOLE, 0x8C, -1, LADX_ITEM_FACE_SHRINE_OPENED);
	spot = place(LADX_SPOT_WITCH, 0x2A2, -1, LADX_ITEM_HAMMER);
	run_ok("default settings");
	check(byte_at(CHEST(0x2F2)) == 0x1B, "setMultiChest writes the world setup's item");
	check(byte_at(CHEST(0x1B6)) == 0x19 && bytes_are(BANK(3) + 0x145Du, "c119"),
	      "a dungeon chest reduces its own dungeon's item and patches the pot chest's operand");
	check(byte_at(CHEST(0x2BF)) == 0x28, "a chest outside a dungeon keeps the dungeon item");
	check(byte_at(DROP(0x169)) == 0x51 + 0x169 % 6 && byte_at(DROP(0x17C)) == 0x51 + 0x169 % 6,
	      "a dropped gel is disguised by its room, room 0x169 copied to 0x17C");
	check(byte_at(DROP(0x166)) == 0x26 && byte_at(DROP(0x1FF)) == 0x26,
	      "room 0x166 is not reduced and is copied to 0x1FF");
	check(byte_at(DROP(0x223)) == 0x0B && byte_at(DROP(0x2E8)) == 0x0B && byte_at(DROP(0x92)) == 0x02 &&
		      byte_at(DROP(0xDC)) == 0x02,
	      "rooms 0x223 and 0x092 are copied to 0x2E8 and 0x0DC");
	check(byte_at(DROP(0x154)) == 0x19, "a dropped key reduces its own dungeon's item");
	check(byte_at(DROP(0xCE)) == 0x0A && byte_at(DROP(0x1F8)) == 0x0A &&
		      byte_at(BOSS_ITEM(0x1F8)) == synth.rom[BOSS_ITEM(0xCE)] &&
		      origin_at(BOSS_ITEM(0x1F8)) == (int32_t)BOSS_ITEM(0xCE),
	      "the angler key copies both item bytes, the boss item as a source copy");
	check(byte_at(BANK(0x18) + 0x0F92u) == 0x07 && byte_at(BANK(0x36) + 0x11C0u) == 0x06 &&
		      byte_at(BANK(0x3E) + 0x3E31u) == 0x08,
	      "the mad batter, tunic fairy and mansion bonus tables");
	check(entry_is(ENTITIES_TABLE, 0xF2, "45505535ff") && entry_is(BOTTOM_TABLE, 0x72, "0102fe") &&
		      bytes_are(BANK(3) + 0x1B9Cu, "a1a2a3") && bytes_are(BANK(3) + 0x244Du, "a4a5a6"),
	      "the beach room's sword becomes a heart piece, with the like-like patches");
	check(byte_at(DROP(0x2A3)) == 0x0B && bytes_are(BANK(5) + 0x0CDAu, "b1b2"),
	      "a start item other than the shield patches Link's sprite");
	check(text_is(0x030, "Deluxe {BOMB} 200 {RUPEES}!", "Buy  No Way") && byte_at(DROP(0x2A1)) == 0x0A &&
		      text_is(0x02C, "{HOOKSHOT} Only 980 {RUPEES}!", "Buy  No Way") && byte_at(CHEST(0x2A1)) == 0x03,
	      "the shop's two items and texts");
	check(byte_at(WITCH) == 0x57 && ladx_logic_spot_options(&randomizer.logic, spot, NULL, 0) > 1,
	      "the shuffled witch's item over 0x09");
	check(text_is(0x221, "I found a good item washed up on the beach... Want to have it?", "Okay No") &&
		      text_is(0x225, "It's a secret to everybody.", NULL) && bytes_are(BANK(0x19) + 0x0668u, "e1e2e3") &&
		      bytes_are(BANK(0x19) + 0x069Cu, "e4") && byte_at(GIFT) == 0x0C && unwritten(BANK(0x19) + 0x063Fu),
	      "the boomerang gift: its texts, code and item");
	check(byte_at(OWL(0x2FE)) == 0x0E && text_is(0x1CA,
		      "MUNCH MUNCH!! ... ... ... ... That was great! I know it's not a fair trade, but here's some "
		      "{BOOMERANG}! YUM...", NULL),
	      "a changed trade item writes its text");
	check(byte_at(OWL(0x2A8)) == 0xA0, "an unchanged trade item writes its id alone");
	check(byte_at(OWL(0xD3)) == 0x58 && byte_at(HOOK) == 0xF0, "an unchanged keyhole installs no hook");
}

static void test_keyholes(void)
{
	static const char *const settings_keyholes[] = {"keyholesanity=1", NULL};
	int first, second;

	randomize(settings_keyholes);
	place(LADX_SPOT_KEY_HOLE, 0x2C3, -1, LADX_ITEM_SWORD);
	place(LADX_SPOT_KEY_HOLE, 0xD3, -1, LADX_ITEM_TAIL_CAVE_OPENED);
	place(LADX_SPOT_KEY_HOLE, 0x2B, -1, LADX_ITEM_ANGLER_TUNNEL_OPENED);
	place(LADX_SPOT_KEY_HOLE, 0xE, -1, LADX_ITEM_EAGLE_TOWER_OPENED);
	first = place(LADX_SPOT_KEY_HOLE, 0x8C, -1, LADX_ITEM_GEL);
	second = place(LADX_SPOT_KEY_HOLE, 0xB5, -1, LADX_ITEM_BOW);
	run_ok("keyholesanity");
	check(byte_at(OWL(0x8C)) == 0x51 + 0x8C % 6 && byte_at(OWL(0xB5)) == 0x02 && byte_at(OWL(0x2C3)) == 0x0B,
	      "keyholes write their item, a gel disguised");
	check(bytes_are(HOOK, "cde37f00") && bytes_are(BANK(2) + 0x3D00u, "f1") && bytes_are(BANK(2) + 0x3FFBu, "f2f3"),
	      "the hook is installed by the first changed keyhole");
	check(bytes_are(BANK(2) + 0x3831u, "f4f5") && bytes_are(BANK(0x15) + 0x00E9u, "f6f7f8"),
	      "the castle keyhole's patches");
	if (first > second) {
		int t = first;

		first = second;
		second = t;
	}
	check(bytes_are(BANK(2) + 0x3FEFu,
			ladx_logic_spot(&randomizer.logic, first)->room == 0x8C ? "b0b1b2b3b4b5b6b7b8b9babb"
										 : "909192939495969798999a9b") &&
		      bytes_are(BANK(2) + 0x3FE3u,
				ladx_logic_spot(&randomizer.logic, second)->room == 0x8C ? "b0b1b2b3b4b5b6b7b8b9babb"
											  : "909192939495969798999a9b") &&
		      bytes_are(BANK(2) + 0x2FAAu, "e37f") && unwritten(BANK(2) + 0x3FD7u),
	      "each changed keyhole's handler goes below the last, the target moved down");
}

static void test_trade_bowwow_owls(void)
{
	static const char *const settings_trade[] = {"boomerang=trade", "bowwow=always", "owlstatues=both", NULL};

	randomize(settings_trade);
	place(LADX_SPOT_BOOMERANG_GUY, 0x1F5, -1, LADX_ITEM_SHOVEL);
	place(LADX_SPOT_START_ITEM, 0x2A3, -1, LADX_ITEM_SWORD);
	place(LADX_SPOT_OWL_STATUE, 0x129, -1, LADX_ITEM_KEY2);
	place(LADX_SPOT_OWL_STATUE, 0x35, -1, LADX_ITEM_KEY2);
	place(LADX_SPOT_TRADE_SEQUENCE_ITEM, 0x2B2, -1, LADX_ITEM_SWORD);
	run_ok("boomerang trade");
	check(bytes_are(BANK(0x19) + 0x063Fu, "d10b") && bytes_are(BANK(0x19) + 0x06C1u, "d30b") &&
		      bytes_are(BANK(0x19) + 0x06FCu, "d50b") && bytes_are(BANK(0x19) + 0x0710u, "d70b"),
	      "the trade's four instructions take the item's inventory id");
	check(text_is(0x222, "Okay, let's do it!", NULL) &&
		      text_is(0x224, "You got the {SHOVEL} in exchange for the item you had.", NULL) &&
		      text_is(0x225, "Give me back my {SHOVEL}, I beg you! I'll return the item you gave me",
			      "Okay Not Now") &&
		      text_is(0x226, "The item came back to you. You returned the other item.", NULL) &&
		      byte_at(GIFT) == 0x00 && unwritten(BANK(0x19) + 0x0668u),
	      "the trade's texts, no gift code");
	check(byte_at(DROP(0x2A3)) == 0x81 && text_is(0xC8, "Got BowWow!", NULL) && bytes_are(BANK(5) + 0x0CDAu, "b1b2"),
	      "bowwow makes the start item BowWow");
	check(byte_at(OWL(0x129)) == 0x1A && byte_at(OWL(0x35)) == 0x24, "an owl statue reduces its own dungeon's item");
	check(text_is(0x181,
		      "Make-up! Jewels! Dresses! I want it all! Sigh... And some new accessories would be nice... Oh! "
		      "That Ribbon! I need it! Will you trade for my {SWORD}?",
		      "Yes  No!") &&
		      entry_is(TEXTS, 0x182, "ff") && text_is(0x183, "Lucky! Thanks! Well, here's your {SWORD}!", NULL),
	      "the dog food trade's three texts");
}

static void test_shield_and_witch(void)
{
	static const char *const settings_no_witch[] = {"witch=0", NULL};
	LadxPipelineError err;

	randomize(defaults);
	place(LADX_SPOT_START_ITEM, 0x2A3, -1, LADX_ITEM_SHIELD);
	place(LADX_SPOT_WITCH, 0x2A2, -1, LADX_ITEM_SWORD);
	plant_files(0x0B);
	open_tables();
	run_ok("a witch byte already patched");
	check(unwritten(BANK(5) + 0x0CDAu) && byte_at(DROP(0x2A3)) == 0x01, "the shield start item keeps Link's sprite");
	check(origin_at(WITCH) != LADX_ENC_KNOWN && byte_at(WITCH) == 0x0B, "a witch patch already applied writes nothing");
	plant_files(0x07);
	open_tables();
	check(run(&err) == LADX_PIPELINE_RAISED && strcmp(err.group, "items") == 0 &&
		      strcmp(err.outcome->type, "AssertionError") == 0 &&
		      strcmp(err.outcome->message, "Patch mismatch:\nb'07' !=\nb'09' at 0x08d5") == 0,
	      "a witch byte neither old nor new raises LADXR's AssertionError");
	plant_files(0x09);
	open_tables();
	randomize(settings_no_witch);
	run_ok("no witch");
	check(origin_at(WITCH) != LADX_ENC_KNOWN && byte_at(WITCH) == 0x09, "an unshuffled witch writes nothing");
}

static void test_raises(void)
{
	LadxPipelineError err;

	randomize(defaults);
	place(LADX_SPOT_CHEST, 0x2BF, -1, LADX_ITEM_KEY9);
	check(run(&err) == LADX_PIPELINE_RAISED && strcmp(err.group, "items") == 0 &&
		      err.outcome->kind == LADX_OUTCOME_CRASH && strcmp(err.outcome->type, "KeyError") == 0 &&
		      strcmp(err.outcome->message, "'KEY9'") == 0,
	      "an item CHEST_ITEMS lacks raises KeyError");
	randomize(defaults);
	randomizer.ws.multichest = LADX_ITEM_RUPEES;
	check(run(&err) == LADX_PIPELINE_RAISED && strcmp(err.outcome->message, "'RUPEES'") == 0,
	      "a multichest item CHEST_ITEMS lacks raises KeyError");
	randomize(defaults);
	plant_files(0x09);
	items_text[strstr(items_text, "unit items.12") - items_text] = '\0';
	open_tables();
	check(run(&err) == LADX_PIPELINE_GROUP && strcmp(err.group, "items") == 0,
	      "a constants file without 12 units fails the group");
	plant_files(0x09);
	open_tables();
}

int main(void)
{
	synth_rom_build(&synth, 0x17E450B1u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	plant_files(0x09);
	open_tables();
	test_lookups();
	test_tables();
	test_keyholes();
	test_trade_bowwow_owls();
	test_shield_and_witch();
	test_raises();
	printf("%s: setMultiChest and every location kind's patch on a synthetic ROM and planted constants (%d checks)\n",
	       TEST, checks);
	return 0;
}
