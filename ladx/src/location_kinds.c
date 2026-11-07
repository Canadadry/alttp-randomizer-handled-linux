#include "ladx_logic.h"
#include "ladx_logic_builder.h"

#include "check_metadata.h"

#include <stdio.h>

/* locations/'s ItemInfo classes at 4325207e: __init__, nameId, OPTIONS and
 * configure; patch and read belong to the ROM steps. */

#define I(name) LADX_ITEM_##name

/* Chest */
static const int chest_options[] = {
	I(POWER_BRACELET), I(SHIELD), I(BOW), I(HOOKSHOT), I(MAGIC_ROD), I(PEGASUS_BOOTS), I(OCARINA),
	I(FEATHER), I(SHOVEL), I(MAGIC_POWDER), I(BOMB), I(SWORD), I(FLIPPERS), I(MAGNIFYING_LENS),
	I(MEDICINE), I(TAIL_KEY), I(ANGLER_KEY), I(FACE_KEY), I(BIRD_KEY), I(GOLD_LEAF), I(SLIME_KEY),
	I(ROOSTER), I(HAMMER), I(RUPEES_50), I(RUPEES_20), I(RUPEES_100), I(RUPEES_200), I(RUPEES_500),
	I(SEASHELL), I(MESSAGE), I(GEL), I(BOOMERANG), I(HEART_PIECE), I(BOWWOW), I(ARROWS_10),
	I(SINGLE_ARROW), I(MAX_POWDER_UPGRADE), I(MAX_BOMBS_UPGRADE), I(MAX_ARROWS_UPGRADE), I(RED_TUNIC),
	I(BLUE_TUNIC), I(HEART_CONTAINER), I(BAD_HEART_CONTAINER), I(TOADSTOOL), I(SONG1), I(SONG2),
	I(SONG3), I(INSTRUMENT1), I(INSTRUMENT2), I(INSTRUMENT3), I(INSTRUMENT4), I(INSTRUMENT5),
	I(INSTRUMENT6), I(INSTRUMENT7), I(INSTRUMENT8), I(TRADING_ITEM_YOSHI_DOLL),
	I(TRADING_ITEM_RIBBON), I(TRADING_ITEM_DOG_FOOD), I(TRADING_ITEM_BANANAS), I(TRADING_ITEM_STICK),
	I(TRADING_ITEM_HONEYCOMB), I(TRADING_ITEM_PINEAPPLE), I(TRADING_ITEM_HIBISCUS),
	I(TRADING_ITEM_LETTER), I(TRADING_ITEM_BROOM), I(TRADING_ITEM_FISHING_HOOK),
	I(TRADING_ITEM_NECKLACE), I(TRADING_ITEM_SCALE), I(TRADING_ITEM_MAGNIFYING_GLASS),
	I(TAIL_CAVE_OPENED), I(KEY_CAVERN_OPENED), I(ANGLER_TUNNEL_OPENED), I(FACE_SHRINE_OPENED),
	I(CASTLE_GATE_OPENED), I(EAGLE_TOWER_OPENED),
};

/* DroppedKey, KeyHole */
static const int dropped_key_options[] = {
	I(POWER_BRACELET), I(SHIELD), I(BOW), I(HOOKSHOT), I(MAGIC_ROD), I(PEGASUS_BOOTS), I(OCARINA),
	I(FEATHER), I(SHOVEL), I(MAGIC_POWDER), I(BOMB), I(SWORD), I(FLIPPERS), I(MAGNIFYING_LENS),
	I(MEDICINE), I(TAIL_KEY), I(ANGLER_KEY), I(FACE_KEY), I(BIRD_KEY), I(GOLD_LEAF), I(SLIME_KEY),
	I(ROOSTER), I(HAMMER), I(RUPEES_50), I(RUPEES_20), I(RUPEES_100), I(RUPEES_200), I(RUPEES_500),
	I(SEASHELL), I(GEL), I(BOOMERANG), I(HEART_PIECE), I(ARROWS_10), I(SINGLE_ARROW),
	I(MAX_POWDER_UPGRADE), I(MAX_BOMBS_UPGRADE), I(MAX_ARROWS_UPGRADE), I(RED_TUNIC), I(BLUE_TUNIC),
	I(HEART_CONTAINER), I(BAD_HEART_CONTAINER), I(TOADSTOOL), I(SONG1), I(SONG2), I(SONG3),
	I(INSTRUMENT1), I(INSTRUMENT2), I(INSTRUMENT3), I(INSTRUMENT4), I(INSTRUMENT5), I(INSTRUMENT6),
	I(INSTRUMENT7), I(INSTRUMENT8), I(TRADING_ITEM_YOSHI_DOLL), I(TRADING_ITEM_RIBBON),
	I(TRADING_ITEM_DOG_FOOD), I(TRADING_ITEM_BANANAS), I(TRADING_ITEM_STICK),
	I(TRADING_ITEM_HONEYCOMB), I(TRADING_ITEM_PINEAPPLE), I(TRADING_ITEM_HIBISCUS),
	I(TRADING_ITEM_LETTER), I(TRADING_ITEM_BROOM), I(TRADING_ITEM_FISHING_HOOK),
	I(TRADING_ITEM_NECKLACE), I(TRADING_ITEM_SCALE), I(TRADING_ITEM_MAGNIFYING_GLASS),
	I(TAIL_CAVE_OPENED), I(KEY_CAVERN_OPENED), I(ANGLER_TUNNEL_OPENED), I(FACE_SHRINE_OPENED),
	I(CASTLE_GATE_OPENED), I(EAGLE_TOWER_OPENED),
};

/* MadBatter, TunicFairy, Witch, OwlStatue, SeashellMansionBonus */
static const int mad_batter_options[] = {
	I(POWER_BRACELET), I(SHIELD), I(BOW), I(HOOKSHOT), I(MAGIC_ROD), I(PEGASUS_BOOTS), I(OCARINA),
	I(FEATHER), I(SHOVEL), I(MAGIC_POWDER), I(BOMB), I(SWORD), I(FLIPPERS), I(MAGNIFYING_LENS),
	I(MEDICINE), I(TAIL_KEY), I(ANGLER_KEY), I(FACE_KEY), I(BIRD_KEY), I(GOLD_LEAF), I(SLIME_KEY),
	I(ROOSTER), I(HAMMER), I(RUPEES_50), I(RUPEES_20), I(RUPEES_100), I(RUPEES_200), I(RUPEES_500),
	I(SEASHELL), I(MESSAGE), I(BOOMERANG), I(HEART_PIECE), I(BOWWOW), I(ARROWS_10), I(SINGLE_ARROW),
	I(MAX_POWDER_UPGRADE), I(MAX_BOMBS_UPGRADE), I(MAX_ARROWS_UPGRADE), I(RED_TUNIC), I(BLUE_TUNIC),
	I(HEART_CONTAINER), I(BAD_HEART_CONTAINER), I(TOADSTOOL), I(SONG1), I(SONG2), I(SONG3),
	I(INSTRUMENT1), I(INSTRUMENT2), I(INSTRUMENT3), I(INSTRUMENT4), I(INSTRUMENT5), I(INSTRUMENT6),
	I(INSTRUMENT7), I(INSTRUMENT8), I(TRADING_ITEM_YOSHI_DOLL), I(TRADING_ITEM_RIBBON),
	I(TRADING_ITEM_DOG_FOOD), I(TRADING_ITEM_BANANAS), I(TRADING_ITEM_STICK),
	I(TRADING_ITEM_HONEYCOMB), I(TRADING_ITEM_PINEAPPLE), I(TRADING_ITEM_HIBISCUS),
	I(TRADING_ITEM_LETTER), I(TRADING_ITEM_BROOM), I(TRADING_ITEM_FISHING_HOOK),
	I(TRADING_ITEM_NECKLACE), I(TRADING_ITEM_SCALE), I(TRADING_ITEM_MAGNIFYING_GLASS),
	I(TAIL_CAVE_OPENED), I(KEY_CAVERN_OPENED), I(ANGLER_TUNNEL_OPENED), I(FACE_SHRINE_OPENED),
	I(CASTLE_GATE_OPENED), I(EAGLE_TOWER_OPENED),
};

/* ShopItem, TradeSequenceItem */
static const int shop_item_options[] = {
	I(POWER_BRACELET), I(SHIELD), I(BOW), I(HOOKSHOT), I(MAGIC_ROD), I(PEGASUS_BOOTS), I(OCARINA),
	I(FEATHER), I(SHOVEL), I(MAGIC_POWDER), I(BOMB), I(SWORD), I(FLIPPERS), I(MAGNIFYING_LENS),
	I(MEDICINE), I(TAIL_KEY), I(ANGLER_KEY), I(FACE_KEY), I(BIRD_KEY), I(GOLD_LEAF), I(SLIME_KEY),
	I(ROOSTER), I(HAMMER), I(RUPEES_50), I(RUPEES_20), I(RUPEES_100), I(RUPEES_200), I(RUPEES_500),
	I(SEASHELL), I(BOOMERANG), I(HEART_PIECE), I(BOWWOW), I(ARROWS_10), I(SINGLE_ARROW),
	I(MAX_POWDER_UPGRADE), I(MAX_BOMBS_UPGRADE), I(MAX_ARROWS_UPGRADE), I(RED_TUNIC), I(BLUE_TUNIC),
	I(HEART_CONTAINER), I(BAD_HEART_CONTAINER), I(TOADSTOOL), I(SONG1), I(SONG2), I(SONG3),
	I(INSTRUMENT1), I(INSTRUMENT2), I(INSTRUMENT3), I(INSTRUMENT4), I(INSTRUMENT5), I(INSTRUMENT6),
	I(INSTRUMENT7), I(INSTRUMENT8), I(TRADING_ITEM_YOSHI_DOLL), I(TRADING_ITEM_RIBBON),
	I(TRADING_ITEM_DOG_FOOD), I(TRADING_ITEM_BANANAS), I(TRADING_ITEM_STICK),
	I(TRADING_ITEM_HONEYCOMB), I(TRADING_ITEM_PINEAPPLE), I(TRADING_ITEM_HIBISCUS),
	I(TRADING_ITEM_LETTER), I(TRADING_ITEM_BROOM), I(TRADING_ITEM_FISHING_HOOK),
	I(TRADING_ITEM_NECKLACE), I(TRADING_ITEM_SCALE), I(TRADING_ITEM_MAGNIFYING_GLASS),
	I(TAIL_CAVE_OPENED), I(KEY_CAVERN_OPENED), I(ANGLER_TUNNEL_OPENED), I(FACE_SHRINE_OPENED),
	I(CASTLE_GATE_OPENED), I(EAGLE_TOWER_OPENED),
};

/* BoomerangGuy */
static const int boomerang_guy_options[] = {
	I(BOOMERANG), I(HOOKSHOT), I(MAGIC_ROD), I(PEGASUS_BOOTS), I(FEATHER), I(SHOVEL),
};

/* BoomerangGuy.configure, boomerang gift */
static const int boomerang_guy_gift_options[] = {
	I(POWER_BRACELET), I(SHIELD), I(BOW), I(HOOKSHOT), I(MAGIC_ROD), I(PEGASUS_BOOTS), I(OCARINA),
	I(FEATHER), I(SHOVEL), I(MAGIC_POWDER), I(BOMB), I(SWORD), I(FLIPPERS), I(MAGNIFYING_LENS),
	I(MEDICINE), I(TAIL_KEY), I(ANGLER_KEY), I(FACE_KEY), I(BIRD_KEY), I(GOLD_LEAF), I(SLIME_KEY),
	I(ROOSTER), I(HAMMER), I(RUPEES_50), I(RUPEES_20), I(RUPEES_100), I(RUPEES_200), I(RUPEES_500),
	I(SEASHELL), I(BOOMERANG), I(HEART_PIECE), I(BOWWOW), I(ARROWS_10), I(SINGLE_ARROW),
	I(MAX_POWDER_UPGRADE), I(MAX_BOMBS_UPGRADE), I(MAX_ARROWS_UPGRADE), I(RED_TUNIC), I(BLUE_TUNIC),
	I(HEART_CONTAINER), I(BAD_HEART_CONTAINER), I(TOADSTOOL),
};

/* StartItem */
static const int start_item_options[] = {
	I(SWORD), I(SHIELD), I(POWER_BRACELET), I(OCARINA), I(FEATHER), I(BOOMERANG), I(MAGIC_ROD),
	I(TAIL_KEY), I(SHOVEL), I(HOOKSHOT), I(PEGASUS_BOOTS), I(MAGIC_POWDER), I(BOMB),
};

#define T10(n) (n), (n) + 1, (n) + 2, (n) + 3, (n) + 4, (n) + 5, (n) + 6, (n) + 7, (n) + 8, (n) + 9

/* item_ids[i] is i: one-item option lists ([HEART_PIECE], KeyLocation's [key]). */
static const int item_ids[] = {
	T10(0), T10(10), T10(20), T10(30), T10(40), T10(50), T10(60), T10(70), T10(80),
	T10(90), T10(100), T10(110), T10(120), T10(130), T10(140), T10(150), T10(160),
};

typedef char item_ids_cover_the_items[sizeof item_ids / sizeof item_ids[0] >= LADX_ITEM_COUNT ? 1 : -1];

/* f"KEY{n}" and its families, n from 0 to 9. */
static const int key_items[10] = {
	I(KEY0), I(KEY1), I(KEY2), I(KEY3), I(KEY4), I(KEY5), I(KEY6), I(KEY7), I(KEY8), I(KEY9),
};
static const int nightmare_key_items[10] = {
	I(NIGHTMARE_KEY0), I(NIGHTMARE_KEY1), I(NIGHTMARE_KEY2), I(NIGHTMARE_KEY3), I(NIGHTMARE_KEY4),
	I(NIGHTMARE_KEY5), I(NIGHTMARE_KEY6), I(NIGHTMARE_KEY7), I(NIGHTMARE_KEY8), I(NIGHTMARE_KEY9),
};
static const int stone_beak_items[10] = {
	I(STONE_BEAK0), I(STONE_BEAK1), I(STONE_BEAK2), I(STONE_BEAK3), I(STONE_BEAK4),
	I(STONE_BEAK5), I(STONE_BEAK6), I(STONE_BEAK7), I(STONE_BEAK8), I(STONE_BEAK9),
};
static const int map_items[10] = {
	I(MAP0), I(MAP1), I(MAP2), I(MAP3), I(MAP4), I(MAP5), I(MAP6), I(MAP7), I(MAP8), I(MAP9),
};
static const int compass_items[10] = {
	I(COMPASS0), I(COMPASS1), I(COMPASS2), I(COMPASS3), I(COMPASS4),
	I(COMPASS5), I(COMPASS6), I(COMPASS7), I(COMPASS8), I(COMPASS9),
};
/* "INSTRUMENT%d": no INSTRUMENT0. */
static const int instrument_items[9] = {
	LADX_ITEM_NONE, I(INSTRUMENT1), I(INSTRUMENT2), I(INSTRUMENT3), I(INSTRUMENT4),
	I(INSTRUMENT5), I(INSTRUMENT6), I(INSTRUMENT7), I(INSTRUMENT8),
};

enum { ARG_NONE, ARG_ITEM, ARG_INDEX };
enum { ROOM_REQUIRED, ROOM_NONE };

typedef struct KindInfo {
	const int *options;
	int option_count;
	int own_room; /* the room __init__ passes; -1 when the caller gives it */
	int room_rule;
	int arg;
	const char *name_suffix; /* nameId's "0x%03X<suffix>" */
} KindInfo;

#define OPTS(list) list, (int)(sizeof list / sizeof list[0])

static const KindInfo kinds[LADX_SPOT_KIND_COUNT] = {
	[LADX_SPOT_ANGLER_KEY] = {OPTS(dropped_key_options), 0x0CE, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_BEACH_SWORD] = {OPTS(dropped_key_options), 0x0F2, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_BIRD_KEY] = {OPTS(dropped_key_options), 0x27A, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_BOOMERANG_GUY] = {OPTS(boomerang_guy_options), 0x1F5, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_CHEST] = {OPTS(chest_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_DROPPED_KEY] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_DUNGEON_CHEST] = {OPTS(chest_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_FACE_KEY] = {OPTS(dropped_key_options), 0x27F, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_FISHING_MINIGAME] = {OPTS(dropped_key_options), 0x2B1, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_GOLD_LEAF] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_HEART_CONTAINER] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_HEART_PIECE] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_HOOKSHOT_DROP] = {OPTS(dropped_key_options), 0x180, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_INSTRUMENT] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_KEY_HOLE] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_ITEM, ""},
	[LADX_SPOT_KEY_LOCATION] = {NULL, 0, -1, ROOM_NONE, ARG_ITEM, NULL},
	[LADX_SPOT_MAD_BATTER] = {OPTS(mad_batter_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_OWL_STATUE] = {OPTS(mad_batter_options), -1, ROOM_REQUIRED, ARG_NONE, "-Owl"},
	[LADX_SPOT_SEASHELL] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_SEASHELL_MANSION] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_SEASHELL_MANSION_BONUS] = {OPTS(mad_batter_options), 0x2E9, ROOM_NONE, ARG_INDEX, ""},
	/* room=0x2A1 is a default: ShopItem(0, room=room) passes another. */
	[LADX_SPOT_SHOP_ITEM] = {OPTS(shop_item_options), 0x2A1, ROOM_REQUIRED, ARG_INDEX, ""},
	[LADX_SPOT_SLIME_KEY] = {OPTS(dropped_key_options), 0x0C6, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_SONG] = {OPTS(dropped_key_options), -1, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_START_ITEM] = {OPTS(start_item_options), 0x2A3, ROOM_NONE, ARG_NONE, ""},
	[LADX_SPOT_TOADSTOOL] = {OPTS(dropped_key_options), 0x050, ROOM_REQUIRED, ARG_NONE, ""},
	[LADX_SPOT_TRADE_SEQUENCE_ITEM] = {OPTS(shop_item_options), -1, ROOM_REQUIRED, ARG_ITEM, "-Trade"},
	[LADX_SPOT_TUNIC_FAIRY] = {OPTS(mad_batter_options), 0x301, ROOM_NONE, ARG_INDEX, ""},
	[LADX_SPOT_WITCH] = {OPTS(mad_batter_options), 0x2A2, ROOM_NONE, ARG_NONE, ""},
};

static bool is_item(int item)
{
	return item >= 0 && item < LADX_ITEM_COUNT;
}

static int broken(LadxLogic *logic)
{
	logic->broken = true;
	return -1;
}

int ladx_logic_add_item_info(LadxLogic *logic, int location, LadxSpotKind kind, int room, int arg)
{
	const KindInfo *k;
	LadxSpot *spot;
	char name_id[LADX_SPOT_NAME_ID_MAX + 1];
	int s;

	if ((int)kind < 0 || kind >= LADX_SPOT_KIND_COUNT)
		return broken(logic);
	k = &kinds[kind];
	if (room == -1)
		room = k->own_room;
	else if (k->room_rule == ROOM_NONE)
		return broken(logic);
	if (kind != LADX_SPOT_KEY_LOCATION && (room < 0 || room > 0xFFF))
		return broken(logic);
	if ((k->arg == ARG_NONE && arg != -1) || (k->arg == ARG_ITEM && !is_item(arg)) ||
	    (k->arg == ARG_INDEX && (arg < 0 || arg > 9)))
		return broken(logic);
	/* shop.py __init__: price [200, 980][index], room asserted in underworld 2. */
	if (kind == LADX_SPOT_SHOP_ITEM && (arg > 1 || room < 0x200 || room >= 0x2FF))
		return broken(logic);
	if (kind == LADX_SPOT_KEY_LOCATION)
		snprintf(name_id, sizeof name_id, "%s", ladx_item_name(arg));
	else if (k->arg == ARG_INDEX)
		snprintf(name_id, sizeof name_id, "0x%03X-%d", room, arg);
	else
		snprintf(name_id, sizeof name_id, "0x%03X%s", room, k->name_suffix);
	if (kind == LADX_SPOT_KEY_LOCATION)
		s = ladx_logic_add_spot(logic, location, kind, name_id, &item_ids[arg], 1);
	else
		s = ladx_logic_add_spot(logic, location, kind, name_id, k->options, k->option_count);
	if (s < 0)
		return s;
	spot = &logic->build_spot[s];
	/* KeyLocation's nameId reads OPTIONS before __init__ sets it: "None". */
	if (kind == LADX_SPOT_KEY_LOCATION)
		spot->metadata = ladx_check_metadata_find("None");
	spot->room = kind == LADX_SPOT_KEY_LOCATION ? -1 : room;
	spot->index = k->arg == ARG_INDEX ? arg : -1;
	spot->default_item = k->arg == ARG_ITEM ? arg : LADX_ITEM_NONE;
	return s;
}

static bool is_value(const LadxSettings *settings, LadxSettingId id, const char *name)
{
	int v = ladx_setting_value_find(id, name);

	return v >= 0 && settings->value[id] == v;
}

/* A boolean setting's truth. */
static bool on(const LadxSettings *settings, LadxSettingId id)
{
	return is_value(settings, id, "1");
}

static void replace(LadxSpot *spot, int item)
{
	spot->base_options = &item_ids[item];
	spot->base_option_count = 1;
}

/* itemInfo.py ItemInfo.configure: the copy, then the appends. */
static void item_info_configure(LadxLogic *logic, int s, const LadxSettings *settings)
{
	bool keys = is_value(settings, LADX_SETTING_DUNGEON_KEYS, "keysanity");
	bool nightmare = is_value(settings, LADX_SETTING_NIGHTMARE_KEYS, "keysanity");
	bool beaks = is_value(settings, LADX_SETTING_DUNGEON_BEAKS, "keysanity");
	bool maps = is_value(settings, LADX_SETTING_DUNGEON_MAPS, "keysanity");
	int d = logic->location[logic->spot[s].location].dungeon, n;

	for (n = 0; n < 10; n++) {
		if (keys)
			ladx_logic_spot_append_option(logic, s, key_items[n]);
		if (nightmare)
			ladx_logic_spot_append_option(logic, s, nightmare_key_items[n]);
		if (beaks)
			ladx_logic_spot_append_option(logic, s, stone_beak_items[n]);
		if (maps) {
			ladx_logic_spot_append_option(logic, s, map_items[n]);
			ladx_logic_spot_append_option(logic, s, compass_items[n]);
		}
	}
	if (d < 0)
		return;
	if (d > 9) {
		logic->broken = true;
		return;
	}
	if (!keys)
		ladx_logic_spot_append_option(logic, s, key_items[d]);
	if (!nightmare)
		ladx_logic_spot_append_option(logic, s, nightmare_key_items[d]);
	if (!beaks)
		ladx_logic_spot_append_option(logic, s, stone_beak_items[d]);
	if (!maps) {
		ladx_logic_spot_append_option(logic, s, map_items[d]);
		ladx_logic_spot_append_option(logic, s, compass_items[d]);
	}
}

static void configure_spot(LadxLogic *logic, int s, const LadxSettings *settings)
{
	LadxSpot *spot = &logic->spot[s];
	int d = logic->location[spot->location].dungeon;

	switch (spot->kind) {
	case LADX_SPOT_KEY_LOCATION:
		return;
	case LADX_SPOT_START_ITEM:
		if (!is_value(settings, LADX_SETTING_BOWWOW, "normal"))
			replace(spot, LADX_ITEM_SWORD);
		else if (on(settings, LADX_SETTING_RANDOMSTARTLOCATION) &&
			 !is_value(settings, LADX_SETTING_ENTRANCESHUFFLE, "none"))
			logic->broken = true; /* its append to the class list is not ported */
		return;
	case LADX_SPOT_BOOMERANG_GUY:
		if (is_value(settings, LADX_SETTING_BOOMERANG, "gift")) {
			spot->base_options = boomerang_guy_gift_options;
			spot->base_option_count = (int)(sizeof boomerang_guy_gift_options / sizeof(int));
			item_info_configure(logic, s, settings);
		}
		return;
	case LADX_SPOT_FISHING_MINIGAME:
	case LADX_SPOT_HEART_PIECE:
		if (on(settings, LADX_SETTING_HEARTPIECE))
			item_info_configure(logic, s, settings);
		else
			replace(spot, LADX_ITEM_HEART_PIECE);
		return;
	case LADX_SPOT_HEART_CONTAINER:
		if (on(settings, LADX_SETTING_HEARTCONTAINERS) || is_value(settings, LADX_SETTING_HPMODE, "extralow") ||
		    is_value(settings, LADX_SETTING_HPMODE, "5hit"))
			item_info_configure(logic, s, settings);
		else if (is_value(settings, LADX_SETTING_HPMODE, "inverted"))
			replace(spot, LADX_ITEM_BAD_HEART_CONTAINER);
		else if (is_value(settings, LADX_SETTING_HPMODE, "low"))
			replace(spot, LADX_ITEM_HEART_PIECE);
		else
			replace(spot, LADX_ITEM_HEART_CONTAINER);
		return;
	case LADX_SPOT_INSTRUMENT:
		if (on(settings, LADX_SETTING_INSTRUMENTS) || is_value(settings, LADX_SETTING_GOAL, "seashells"))
			item_info_configure(logic, s, settings);
		else if (d >= 1 && d <= 8)
			replace(spot, instrument_items[d]);
		else
			logic->broken = true;
		return;
	case LADX_SPOT_KEY_HOLE:
		if (on(settings, LADX_SETTING_KEYHOLESANITY))
			item_info_configure(logic, s, settings);
		else
			replace(spot, spot->default_item);
		return;
	case LADX_SPOT_SEASHELL:
	case LADX_SPOT_SEASHELL_MANSION_BONUS:
		if (on(settings, LADX_SETTING_SEASHELLS))
			item_info_configure(logic, s, settings);
		else
			replace(spot, LADX_ITEM_SEASHELL);
		return;
	case LADX_SPOT_SHOP_ITEM:
		item_info_configure(logic, s, settings);
		if (!is_value(settings, LADX_SETTING_SHOPSANITY, ""))
			logic->broken = true; /* shopsanity is not ported */
		return;
	case LADX_SPOT_OWL_STATUE:
		item_info_configure(logic, s, settings);
		if (is_value(settings, LADX_SETTING_OWLSTATUES, "both") ||
		    (is_value(settings, LADX_SETTING_OWLSTATUES, "dungeon") && spot->room >= 0x100) ||
		    (is_value(settings, LADX_SETTING_OWLSTATUES, "overworld") && spot->room < 0x100))
			return;
		logic->broken = true; /* "Tried to configure an owlstatue that was not enabled" */
		return;
	case LADX_SPOT_TOADSTOOL:
		if (on(settings, LADX_SETTING_WITCH))
			item_info_configure(logic, s, settings);
		else
			replace(spot, LADX_ITEM_TOADSTOOL);
		return;
	case LADX_SPOT_WITCH:
		if (on(settings, LADX_SETTING_WITCH))
			item_info_configure(logic, s, settings);
		else
			replace(spot, LADX_ITEM_MAGIC_POWDER);
		return;
	case LADX_SPOT_TRADE_SEQUENCE_ITEM:
		if (on(settings, LADX_SETTING_TRADEQUEST))
			item_info_configure(logic, s, settings);
		else
			replace(spot, spot->default_item);
		return;
	default:
		item_info_configure(logic, s, settings);
		return;
	}
}

void ladx_logic_configure(LadxLogic *logic, const LadxSettings *settings)
{
	int s;

	if (!logic->finished || logic->configured || ladx_logic_broken(logic)) {
		logic->broken = true;
		return;
	}
	logic->configured = true;
	for (s = 0; s < logic->spot_count && !ladx_logic_broken(logic); s++)
		configure_spot(logic, s, settings);
}
