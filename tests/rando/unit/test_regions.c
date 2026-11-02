#include "rando_world.h"
#include "support.h"

#include <stdio.h>

#define TEST "test_regions"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void standard_world(RandoWorld *world)
{
	RandoConfig config;

	rando_config_init(&config);
	rando_world_init(world, RANDO_VARIANT_STANDARD, &config);
}

/* TestCase::allItemsExcept: allItems, then offsetUnset of each name, which keeps the values view. */
static void all_items_except(RandoItems *items, const RandoItemId *except, int n)
{
	int i;

	rando_items_all(items);
	rando_items_unset(items, RANDO_ITEM_Key);
	rando_items_unset(items, RANDO_ITEM_BigKey);
	rando_items_unset(items, RANDO_ITEM_ShopKey);
	for (i = 0; i < n; i++)
		rando_items_unset(items, except[i]);
}

/* EasternPalaceTest::testNothingRequiredToEnter. */
static void check_nothing_required_to_enter_eastern_palace(void)
{
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	rando_items_clear(&items);
	rando_items_add(&items, RANDO_ITEM_RescueZelda);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_EASTERN_PALACE, &items));
}

/* TowerOfHeraTest::testCanEnterWithEverything. */
static void check_can_enter_tower_of_hera_with_everything(void)
{
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_except(&items, NULL, 0);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_TOWER_OF_HERA, &items));
}

/* Inverted TowerOfHeraTest::testCanEnterWithEverything. */
static void check_can_enter_inverted_tower_of_hera_with_everything(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_INVERTED, &config);
	all_items_except(&items, NULL, 0);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_TOWER_OF_HERA, &items));
}

/* Inverted SkullWoodsTest::testLocation's Bridge Room row: Fire Rod alone, no Moon Pearl. */
static void check_inverted_skull_woods_bridge_room_needs_only_fire_rod(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_INVERTED, &config);
	rando_items_clear(&items);
	rando_items_add(&items, RANDO_ITEM_FireRod);
	CHECK(rando_world_can_access(&world, RANDO_LOC_SKULL_WOODS_BRIDGE_ROOM, &items));
}

/* Inverted Ganon's Tower: a super bunny enters, but every chest but Bob's Torch wants the Moon Pearl;
 * Bob's Chest drops Standard's "basic" Fire Rod or Ether clause. */
static void check_inverted_ganons_tower_chests_need_moon_pearl(void)
{
	static const RandoItemId no_pearl[] = {RANDO_ITEM_MoonPearl, RANDO_ITEM_FireRod, RANDO_ITEM_Ether};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	config.item_placement = RANDO_PLACEMENT_BASIC;
	config.can_super_bunny = true;
	rando_world_init(&world, RANDO_VARIANT_INVERTED, &config);
	all_items_except(&items, no_pearl, 3);
	for (int i = 0; i < 12; i++)
		rando_items_add(&items, RANDO_ITEM_BossHeartContainer);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_GANONS_TOWER, &items));
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BOBS_TORCH, &items));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT, &items));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BOBS_CHEST, &items));
	rando_items_add(&items, RANDO_ITEM_MoonPearl);
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT, &items));
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BOBS_CHEST, &items));
}

/* Inverted TurtleRockTest::testLocation's Chain Chomps row entering from the top with two keys. */
static void check_inverted_turtle_rock_chain_chomps_from_the_top(void)
{
	static const RandoItemId collected[] = {
		RANDO_ITEM_Lamp, RANDO_ITEM_Hookshot, RANDO_ITEM_TitansMitt, RANDO_ITEM_Hammer, RANDO_ITEM_Quake,
		RANDO_ITEM_UncleSword, RANDO_ITEM_CaneOfSomaria, RANDO_ITEM_KeyD7, RANDO_ITEM_KeyD7,
	};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;
	size_t i;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_INVERTED, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake));
	rando_items_clear(&items);
	for (i = 0; i < sizeof collected / sizeof collected[0]; i++)
		rando_items_add(&items, collected[i]);
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS, &items));
}

/* HyruleCastleTowerTest::testCapeOrUpgradedSwordRequiredToEnter. */
static void check_cape_or_upgraded_sword_required_to_enter_castle_tower(void)
{
	static const RandoItemId except[] = {
		RANDO_ITEM_Cape, RANDO_ITEM_L2Sword, RANDO_ITEM_MasterSword, RANDO_ITEM_L3Sword, RANDO_ITEM_L4Sword,
		RANDO_ITEM_UncleSword,
	};
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_except(&items, except, (int)(sizeof except / sizeof except[0]));
	CHECK(!rando_world_region_can_enter(&world, RANDO_REGION_HYRULE_CASTLE_TOWER, &items));
}

/* HyruleCastleTowerTest::testSwordRequiredToComplete. */
static void check_sword_required_to_complete_castle_tower(void)
{
	static const RandoItemId except[] = {
		RANDO_ITEM_L1Sword, RANDO_ITEM_L1SwordAndShield, RANDO_ITEM_L2Sword, RANDO_ITEM_MasterSword,
		RANDO_ITEM_L3Sword, RANDO_ITEM_L4Sword, RANDO_ITEM_ProgressiveSword, RANDO_ITEM_UncleSword,
	};
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_except(&items, except, (int)(sizeof except / sizeof except[0]));
	CHECK(!rando_world_region_can_complete(&world, RANDO_REGION_HYRULE_CASTLE_TOWER, &items));
}

/* Region::initalize's bossNormalLocation fill rule, which no NoGlitches row varies. */
static void check_boss_normal_location_decides_dungeon_items_on_bosses(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_items_clear(&items);
	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_ITEM_MapP1, &items, true));
	config.region_boss_normal_location = false;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_ITEM_MapP1, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_TOWER_OF_HERA_BOSS, RANDO_ITEM_KeyP3, &items, false));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_DESERT_PALACE_BOSS, RANDO_ITEM_Hammer, &items, false));
}

/* The Tower of Hera Big Key Chest takes its own key unreachable unless accessibility is "locations". */
static void check_hera_big_key_chest_follows_accessibility(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_items_clear(&items);
	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST, RANDO_ITEM_KeyP3, &items, true));
	config.accessibility = RANDO_ACCESSIBILITY_LOCATIONS;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST, RANDO_ITEM_KeyP3, &items, false));
}

/* allItemsExcept(item) then n copies of item back. */
static void all_items_with_n(RandoItems *items, RandoItemId item, int n)
{
	all_items_except(items, &item, 1);
	while (n-- > 0)
		rando_items_add(items, item);
}

/* PoD Big Key Chest and Harmless Hellway, Thieves' Town Big Chest: hasItem branches no row presets. */
static void check_key_on_its_own_chest_lowers_the_key_count(void)
{
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_with_n(&items, RANDO_ITEM_KeyD1, 4);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, &items));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, &items));
	CHECK(rando_world_set_item(&world, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, RANDO_ITEM_KeyD1));
	CHECK(rando_world_set_item(&world, RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, RANDO_ITEM_KeyD1));
	CHECK(rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, &items));
	CHECK(rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, &items));

	all_items_with_n(&items, RANDO_ITEM_KeyD4, 0);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, &items));
	CHECK(rando_world_set_item(&world, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, RANDO_ITEM_KeyD4));
	CHECK(rando_world_can_access(&world, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, &items));
}

/* PalaceOfDarkness: region.wildKeys asks one more KeyD1, as the Hammer-and-Bow shortcut does. */
static void check_wild_keys_raise_palace_of_darkness_key_counts(void)
{
	static const RandoItemId except[] = {RANDO_ITEM_KeyD1, RANDO_ITEM_Hammer};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	all_items_except(&items, except, 2);
	rando_items_add(&items, RANDO_ITEM_KeyD1);
	rando_items_add(&items, RANDO_ITEM_KeyD1);
	rando_items_add(&items, RANDO_ITEM_KeyD1);
	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_COMPASS_CHEST, &items));
	config.region_wild_keys = true;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_COMPASS_CHEST, &items));
}

/* The key-on-its-own-chest fill rules of PoD and Thieves' Town, which accessibility "locations" refuses. */
static void check_locations_accessibility_refuses_keys_on_their_own_chests(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_items_clear(&items);
	rando_items_add(&items, RANDO_ITEM_Hammer);
	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, RANDO_ITEM_KeyD1, &items, true));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, RANDO_ITEM_KeyD4, &items, true));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, RANDO_ITEM_KeyD1, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_TOP, RANDO_ITEM_KeyD1, &items, false));
	for (int i = 0; i < 5; i++)
		rando_items_add(&items, RANDO_ITEM_KeyD1);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, RANDO_ITEM_KeyD1, &items, true));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, RANDO_ITEM_KeyD1, &items, true));
	config.accessibility = RANDO_ACCESSIBILITY_LOCATIONS;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, RANDO_ITEM_KeyD1, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, RANDO_ITEM_KeyD1, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, RANDO_ITEM_KeyD4, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_SWAMP_PALACE_BIG_CHEST, RANDO_ITEM_BigKeyD2, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_SKULL_WOODS_BIG_CHEST, RANDO_ITEM_BigKeyD3, &items, false));
}

/* Swamp Palace Entrance with region.wildKeys, Skull Woods Pinball Room with region.forceSkullWoodsKey. */
static void check_key_config_decides_swamp_entrance_and_pinball_room(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_items_clear(&items);
	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_SKULL_WOODS_PINBALL_ROOM, RANDO_ITEM_BigKeyD3, &items, false));
	config.region_wild_keys = true;
	config.region_force_skull_woods_key = true;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_SWAMP_PALACE_ENTRANCE, RANDO_ITEM_BigKeyD2, &items, false));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_SKULL_WOODS_PINBALL_ROOM, RANDO_ITEM_BigKeyD3, &items, false));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_SKULL_WOODS_PINBALL_ROOM, RANDO_ITEM_KeyD3, &items, false));
}

/* VT's tests leave itemPlacement and mode.weapons unset; the baseline sets them. */
static void check_placement_and_weapons_in_dark_world_dungeons(void)
{
	static const RandoItemId swords[] = {
		RANDO_ITEM_L1Sword, RANDO_ITEM_L1SwordAndShield, RANDO_ITEM_L2Sword, RANDO_ITEM_MasterSword,
		RANDO_ITEM_L3Sword, RANDO_ITEM_L4Sword, RANDO_ITEM_ProgressiveSword, RANDO_ITEM_UncleSword,
	};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	all_items_with_n(&items, RANDO_ITEM_Lamp, 0);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_LEFT, &items));
	all_items_except(&items, swords, (int)(sizeof swords / sizeof swords[0]));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_SKULL_WOODS_BOSS, &items));
	config.item_placement = RANDO_PLACEMENT_ADVANCED;
	config.mode_weapons = RANDO_WEAPONS_SWORDLESS;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_can_access(&world, RANDO_LOC_SKULL_WOODS_BOSS, &items));
	all_items_with_n(&items, RANDO_ITEM_Lamp, 0);
	CHECK(rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_LEFT, &items));
	CHECK(rando_world_can_access(&world, RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_RIGHT, &items));
}

/* Ice Palace Spike Room and Misery Mire's torch rooms: itemInLocations and hasItem branches no row presets. */
static void check_big_key_placement_lowers_ice_and_mire_key_counts(void)
{
	static const RandoItemId no_hookshot[] = {RANDO_ITEM_Hookshot, RANDO_ITEM_KeyD5};
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	all_items_except(&items, no_hookshot, 2);
	rando_items_add(&items, RANDO_ITEM_KeyD5);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));
	CHECK(rando_world_set_item(&world, RANDO_LOC_ICE_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyD5));
	CHECK(rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));

	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether));
	all_items_with_n(&items, RANDO_ITEM_KeyD6, 2);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST, &items));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST, RANDO_ITEM_BigKeyD6, &items, true));
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_COMPASS_CHEST, RANDO_ITEM_BigKeyD6));
	CHECK(rando_world_can_access(&world, RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST, &items));
}

/* Ice Palace Spike Room's ShopKey shortcut, and region.cantTakeDamage there and at the Mire Spike Chest. */
static void check_shop_key_and_damage_decide_ice_and_mire_spikes(void)
{
	static const RandoItemId no_hookshot[] = {RANDO_ITEM_Hookshot, RANDO_ITEM_KeyD5};
	static const RandoItemId no_cape[] = {RANDO_ITEM_CaneOfByrna, RANDO_ITEM_Cape};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether));
	all_items_except(&items, no_hookshot, 2);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));
	rando_items_add(&items, RANDO_ITEM_ShopKey);
	CHECK(rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));
	rando_items_unset(&items, RANDO_ITEM_CaneOfByrna);
	rando_items_unset(&items, RANDO_ITEM_Cape);
	CHECK(rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));
	all_items_except(&items, no_cape, 2);
	CHECK(rando_world_can_access(&world, RANDO_LOC_MISERY_MIRE_SPIKE_CHEST, &items));

	config.region_cant_take_damage = true;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_MISERY_MIRE_SPIKE_CHEST, &items));
	all_items_except(&items, no_hookshot, 2);
	rando_items_add(&items, RANDO_ITEM_ShopKey);
	rando_items_unset(&items, RANDO_ITEM_CaneOfByrna);
	rando_items_unset(&items, RANDO_ITEM_Cape);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));
}

static void turtle_rock_world(RandoWorld *world, const RandoConfig *config)
{
	rando_world_init(world, RANDO_VARIANT_STANDARD, config);
	CHECK(rando_world_set_item(world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake));
}

/* allItems holds too few hearts for the dungeon entries' hasHealth(12) under "basic". */
static void add_basic_hearts(RandoItems *items)
{
	for (int i = 0; i < 9; i++)
		rando_items_add(items, RANDO_ITEM_BossHeartContainer);
}

/* itemPlacement "basic": Ice Palace Boss wants two KeyD5, the Eye Bridge a laser guard (Cape, Byrna or shield). */
static void check_basic_placement_in_ice_palace_and_turtle_rock(void)
{
	static const RandoItemId no_cape[] = {RANDO_ITEM_Cape};
	static const RandoItemId no_guard[] = {RANDO_ITEM_Cape, RANDO_ITEM_CaneOfByrna};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	all_items_with_n(&items, RANDO_ITEM_KeyD5, 1);
	CHECK(rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_BOSS, &items));
	config.item_placement = RANDO_PLACEMENT_BASIC;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(!rando_world_region_can_enter(&world, RANDO_REGION_ICE_PALACE, &items));
	add_basic_hearts(&items);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_BOSS, &items));
	rando_items_add(&items, RANDO_ITEM_KeyD5);
	CHECK(rando_world_can_access(&world, RANDO_LOC_ICE_PALACE_BOSS, &items));

	turtle_rock_world(&world, &config);
	all_items_except(&items, no_guard, 2);
	add_basic_hearts(&items);
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT, &items));
	config.item_overflow_count_shield = 2;
	turtle_rock_world(&world, &config);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT, &items));
	all_items_except(&items, no_cape, 1);
	add_basic_hearts(&items);
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT, &items));
	all_items_except(&items, no_guard, 2);
	config.item_placement = RANDO_PLACEMENT_ADVANCED;
	turtle_rock_world(&world, &config);
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT, &items));
}

/* Turtle Rock Big Key Chest: region.wildKeys key counts, its KeyD7 always-allow and accessibility fill rule. */
static void check_turtle_rock_big_key_chest_keys(void)
{
	static const RandoItemId no_pearl[] = {RANDO_ITEM_MoonPearl, RANDO_ITEM_KeyD7};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	turtle_rock_world(&world, &config);
	all_items_with_n(&items, RANDO_ITEM_KeyD7, 2);
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, &items));
	config.region_wild_keys = true;
	turtle_rock_world(&world, &config);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, &items));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_BigKeyD7, &items, true));
	rando_items_add(&items, RANDO_ITEM_KeyD7);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, &items));
	rando_items_add(&items, RANDO_ITEM_KeyD7);
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, &items));
	rando_items_remove(&items, RANDO_ITEM_KeyD7);
	CHECK(rando_world_set_item(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_KeyD7));
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, &items));

	config.region_wild_keys = false;
	turtle_rock_world(&world, &config);
	all_items_except(&items, no_pearl, 2);
	rando_items_add(&items, RANDO_ITEM_KeyD7);
	rando_items_add(&items, RANDO_ITEM_KeyD7);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_KeyD7, &items, true));
	rando_items_add(&items, RANDO_ITEM_KeyD7);
	CHECK(rando_world_can_fill(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_KeyD7, &items, true));
	config.accessibility = RANDO_ACCESSIBILITY_LOCATIONS;
	turtle_rock_world(&world, &config);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_KeyD7, &items, false));
}

/* Turtle Rock Big Chest refuses its own big key even unchecked; Chain Chomps' upper path wants a KeyD7. */
static void check_turtle_rock_big_chest_and_chain_chomps(void)
{
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	turtle_rock_world(&world, &config);
	rando_items_clear(&items);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_TURTLE_ROCK_BIG_CHEST, RANDO_ITEM_BigKeyD7, &items, false));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_TURTLE_ROCK_BIG_CHEST, RANDO_ITEM_KeyD7, &items, false));
	all_items_with_n(&items, RANDO_ITEM_KeyD7, 0);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS, &items));
	rando_items_add(&items, RANDO_ITEM_KeyD7);
	CHECK(rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS, &items));
}

/* The Ice Palace, Misery Mire and Turtle Rock bosses: compass and map, bossNormalLocation, wild guards, completion. */
static void check_ice_mire_and_turtle_rock_bosses(void)
{
	static const struct {
		RandoRegionId region;
		RandoLocationId boss, prize;
		RandoItemId compass, map, key, needed;
	} bosses[] = {
		{RANDO_REGION_ICE_PALACE, RANDO_LOC_ICE_PALACE_BOSS, RANDO_LOC_ICE_PALACE_PRIZE, RANDO_ITEM_CompassD5,
		 RANDO_ITEM_MapD5, RANDO_ITEM_KeyD5, RANDO_ITEM_Hammer},
		{RANDO_REGION_MISERY_MIRE, RANDO_LOC_MISERY_MIRE_BOSS, RANDO_LOC_MISERY_MIRE_PRIZE, RANDO_ITEM_CompassD6,
		 RANDO_ITEM_MapD6, RANDO_ITEM_KeyD6, RANDO_ITEM_CaneOfSomaria},
		{RANDO_REGION_TURTLE_ROCK, RANDO_LOC_TURTLE_ROCK_BOSS, RANDO_LOC_TURTLE_ROCK_PRIZE, RANDO_ITEM_CompassD7,
		 RANDO_ITEM_MapD7, RANDO_ITEM_KeyD7, RANDO_ITEM_Lamp},
	};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	for (size_t i = 0; i < sizeof bosses / sizeof bosses[0]; i++) {
		rando_config_init(&config);
		turtle_rock_world(&world, &config);
		CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether));
		rando_items_clear(&items);
		CHECK(rando_world_can_fill(&world, bosses[i].boss, bosses[i].compass, &items, true));
		CHECK(rando_world_can_fill(&world, bosses[i].boss, bosses[i].map, &items, true));
		CHECK(!rando_world_can_fill(&world, bosses[i].boss, bosses[i].key, &items, true));
		CHECK(rando_world_can_fill(&world, bosses[i].boss, bosses[i].key, &items, false));
		all_items_except(&items, NULL, 0);
		CHECK(rando_world_region_can_complete(&world, bosses[i].region, &items));
		CHECK(rando_world_can_access(&world, bosses[i].prize, &items));
		all_items_with_n(&items, bosses[i].needed, 0);
		CHECK(rando_world_region_can_enter(&world, bosses[i].region, &items));
		CHECK(!rando_world_region_can_complete(&world, bosses[i].region, &items));
		CHECK(!rando_world_can_access(&world, bosses[i].prize, &items));

		config.region_boss_normal_location = false;
		turtle_rock_world(&world, &config);
		rando_items_clear(&items);
		CHECK(!rando_world_can_fill(&world, bosses[i].boss, bosses[i].key, &items, false));
		CHECK(!rando_world_can_fill(&world, bosses[i].boss, bosses[i].compass, &items, false));

		config.region_wild_compasses = true;
		config.region_wild_maps = true;
		turtle_rock_world(&world, &config);
		CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether));
		all_items_with_n(&items, bosses[i].compass, 0);
		CHECK(!rando_world_can_access(&world, bosses[i].boss, &items));
		CHECK(rando_world_set_item(&world, bosses[i].boss, bosses[i].compass));
		CHECK(rando_world_can_access(&world, bosses[i].boss, &items));
		all_items_with_n(&items, bosses[i].map, 0);
		CHECK(!rando_world_can_access(&world, bosses[i].boss, &items));
	}
}

/* Ganon's Tower key rooms: BigKeyA2 in a sibling chest, or a KeyA2 or BigKeyA2 on the chest itself, saves a KeyA2. */
static void check_big_key_placement_lowers_ganons_tower_key_counts(void)
{
	static const struct {
		RandoLocationId location, holder, self;
		RandoItemId held;
		int keys;
	} rooms[] = {
		{RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT,
		 RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT, RANDO_ITEM_BigKeyA2, 3},
		{RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT, RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT,
		 RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT, RANDO_ITEM_BigKeyA2, 3},
		{RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT,
		 RANDO_LOCATION_NONE, RANDO_ITEM_BigKeyA2, 2},
		{RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM, RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM, RANDO_LOCATION_NONE,
		 RANDO_ITEM_KeyA2, 2},
		{RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_LOCATION_NONE, RANDO_ITEM_KeyA2, 3},
		{RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_LOCATION_NONE,
		 RANDO_ITEM_BigKeyA2, 3},
	};
	RandoWorld world;
	RandoItems items;

	for (size_t i = 0; i < sizeof rooms / sizeof rooms[0]; i++) {
		standard_world(&world);
		all_items_with_n(&items, RANDO_ITEM_KeyA2, rooms[i].keys);
		CHECK(!rando_world_can_access(&world, rooms[i].location, &items));
		if (rooms[i].self != RANDO_LOCATION_NONE) {
			CHECK(rando_world_set_item(&world, rooms[i].self, RANDO_ITEM_BigKeyA2));
			CHECK(!rando_world_can_access(&world, rooms[i].location, &items));
		}
		CHECK(rando_world_set_item(&world, rooms[i].holder, rooms[i].held));
		CHECK(rando_world_can_access(&world, rooms[i].location, &items));
	}
}

/* Ganon's Tower under "basic": the Map Chest loses its boots path, Bob's Chest wants the Fire Rod or Ether and a sword. */
static void check_basic_placement_in_ganons_tower(void)
{
	static const RandoItemId no_hookshot[] = {RANDO_ITEM_Hookshot};
	static const RandoItemId no_fire[] = {RANDO_ITEM_FireRod, RANDO_ITEM_Ether};
	RandoConfig config;
	RandoWorld world;
	RandoItems items;

	rando_config_init(&config);
	config.item_placement = RANDO_PLACEMENT_ADVANCED;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	all_items_except(&items, no_hookshot, 1);
	add_basic_hearts(&items);
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_MAP_CHEST, &items));
	all_items_except(&items, no_fire, 2);
	add_basic_hearts(&items);
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BOBS_CHEST, &items));

	config.item_placement = RANDO_PLACEMENT_BASIC;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_GANONS_TOWER, &items));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BOBS_CHEST, &items));
	rando_items_add(&items, RANDO_ITEM_Ether);
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BOBS_CHEST, &items));
	all_items_except(&items, no_hookshot, 1);
	add_basic_hearts(&items);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_MAP_CHEST, &items));
}

/* Ganon's Tower from an empty collection: Moldorm Chest refuses KeyA2, completion needs the Moldorm Chest. */
static void check_ganons_tower_moldorm_chest_and_completion(void)
{
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	rando_items_clear(&items);
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST, RANDO_ITEM_KeyA2, &items, false));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST, RANDO_ITEM_MapA2, &items, false));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_ITEM_KeyA2, &items, false));

	all_items_except(&items, NULL, 0);
	CHECK(rando_world_region_can_complete(&world, RANDO_REGION_GANONS_TOWER, &items));
	CHECK(rando_world_can_access(&world, RANDO_LOC_AGAHNIM_2, &items));
	all_items_with_n(&items, RANDO_ITEM_KeyA2, 3);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_GANONS_TOWER, &items));
	CHECK(!rando_world_region_can_complete(&world, RANDO_REGION_GANONS_TOWER, &items));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_AGAHNIM_2, &items));
}

/* Ganon's Tower's default bosses: Armos Knights below (no rod without magic), Moldorm on top (sword or Hammer). */
static void check_ganons_tower_default_bosses(void)
{
	static const RandoItemId path[] = {
		RANDO_ITEM_RescueZelda, RANDO_ITEM_MoonPearl, RANDO_ITEM_Crystal1, RANDO_ITEM_Crystal2,
		RANDO_ITEM_Crystal3, RANDO_ITEM_Crystal4, RANDO_ITEM_Crystal5, RANDO_ITEM_Crystal6,
		RANDO_ITEM_Crystal7, RANDO_ITEM_TitansMitt, RANDO_ITEM_Lamp, RANDO_ITEM_Hookshot,
		RANDO_ITEM_MagicMirror, RANDO_ITEM_OcarinaActive, RANDO_ITEM_FireRod, RANDO_ITEM_CaneOfSomaria,
		RANDO_ITEM_BigKeyA2, RANDO_ITEM_KeyA2, RANDO_ITEM_KeyA2, RANDO_ITEM_KeyA2, RANDO_ITEM_KeyA2,
	};
	RandoWorld world;
	RandoItems items;

	standard_world(&world);
	rando_items_clear(&items);
	for (size_t i = 0; i < sizeof path / sizeof path[0]; i++)
		rando_items_add(&items, path[i]);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_GANONS_TOWER, &items));
	CHECK(!rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_LEFT, &items));
	rando_items_add(&items, RANDO_ITEM_Boomerang);
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_LEFT, &items));

	rando_items_add(&items, RANDO_ITEM_BowAndArrows);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST, &items));
	rando_items_add(&items, RANDO_ITEM_Hammer);
	CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST, &items));
}

/* Spike Room's $locations->itemInLocations reads the locations world, not the location's own. */
static void check_spike_room_reads_big_key_from_locations_world(void)
{
	static const RandoItemId no_hookshot[] = {RANDO_ITEM_Hookshot, RANDO_ITEM_KeyD5};
	RandoWorld with_key, without_key;
	RandoItems items;

	standard_world(&with_key);
	standard_world(&without_key);
	CHECK(rando_world_set_item(&with_key, RANDO_LOC_ICE_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyD5));
	all_items_except(&items, no_hookshot, 2);
	rando_items_add(&items, RANDO_ITEM_KeyD5);
	CHECK(rando_world_can_access_with(&without_key, &with_key, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));
	CHECK(!rando_world_can_access_with(&with_key, &without_key, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, &items));
}

/* $this->locations["Ice Palace - Spike Room"]->canAccess($items) resets $locations to the self world. */
static void check_nested_spike_room_reads_big_key_from_own_world(void)
{
	static const RandoItemId no_hookshot[] = {RANDO_ITEM_Hookshot, RANDO_ITEM_KeyD5};
	RandoWorld with_key, without_key;
	RandoItems items;

	standard_world(&with_key);
	standard_world(&without_key);
	CHECK(rando_world_set_item(&with_key, RANDO_LOC_ICE_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyD5));
	all_items_except(&items, no_hookshot, 2);
	rando_items_add(&items, RANDO_ITEM_KeyD5);
	CHECK(rando_world_can_access_with(&with_key, &without_key, RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST, &items));
	CHECK(!rando_world_can_access_with(&without_key, &with_key, RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST, &items));
}

static void check_thieves_town_big_chest_reads_key_from_locations_world(void)
{
	RandoWorld with_key, without_key;
	RandoItems items;

	standard_world(&with_key);
	standard_world(&without_key);
	CHECK(rando_world_set_item(&with_key, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, RANDO_ITEM_KeyD4));
	all_items_with_n(&items, RANDO_ITEM_KeyD4, 0);
	CHECK(rando_world_can_access_with(&without_key, &with_key, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, &items));
	CHECK(!rando_world_can_access_with(&with_key, &without_key, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, &items));
}

/* $this->locations["Eastern Palace - Boss"]->hasItem(CompassP1) reads the self world. */
static void check_wild_compass_on_boss_read_from_own_world(void)
{
	RandoConfig config;
	RandoWorld with_compass, without_compass;
	RandoItems items;

	rando_config_init(&config);
	config.region_wild_compasses = true;
	config.region_wild_maps = true;
	rando_world_init(&with_compass, RANDO_VARIANT_STANDARD, &config);
	rando_world_init(&without_compass, RANDO_VARIANT_STANDARD, &config);
	CHECK(rando_world_set_item(&with_compass, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_ITEM_CompassP1));
	all_items_with_n(&items, RANDO_ITEM_CompassP1, 0);
	CHECK(rando_world_can_access_with(&with_compass, &without_compass, RANDO_LOC_EASTERN_PALACE_BOSS, &items));
	CHECK(!rando_world_can_access_with(&without_compass, &with_compass, RANDO_LOC_EASTERN_PALACE_BOSS, &items));
}

static void check_single_world_access_is_access_with_itself(void)
{
	static const RandoLocationId sample[] = {
		RANDO_LOC_ICE_PALACE_SPIKE_ROOM, RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST, RANDO_LOC_THIEVES_TOWN_BIG_CHEST,
		RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, RANDO_LOC_GANONS_TOWER_MAP_CHEST,
	};
	static const RandoItemId no_hookshot[] = {RANDO_ITEM_Hookshot, RANDO_ITEM_KeyD5};
	RandoWorld worlds[2];
	RandoItems items;

	standard_world(&worlds[0]);
	standard_world(&worlds[1]);
	CHECK(rando_world_set_item(&worlds[1], RANDO_LOC_ICE_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyD5));
	CHECK(rando_world_set_item(&worlds[1], RANDO_LOC_THIEVES_TOWN_BIG_CHEST, RANDO_ITEM_KeyD4));
	CHECK(rando_world_set_item(&worlds[1], RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, RANDO_ITEM_KeyD1));
	CHECK(rando_world_set_item(&worlds[1], RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_ITEM_KeyA2));
	all_items_except(&items, no_hookshot, 2);
	rando_items_add(&items, RANDO_ITEM_KeyD5);
	rando_items_add(&items, RANDO_ITEM_KeyD1);
	rando_items_add(&items, RANDO_ITEM_KeyA2);
	for (int w = 0; w < 2; w++)
		for (size_t i = 0; i < sizeof sample / sizeof sample[0]; i++)
			CHECK(rando_world_can_access(&worlds[w], sample[i], &items)
			      == rando_world_can_access_with(&worlds[w], &worlds[w], sample[i], &items));
}

static int every_row(const char *file, const char *method)
{
	(void)file;
	(void)method;
	return 1;
}

static void replay_every_row(const char *path)
{
	int total, rows = fixture_replay_rows(TEST, path, every_row, &total);

	if (rows != total || rows == 0)
		test_fail(TEST, "%s: %d of %d rows replayed", path, rows, total);
	printf(TEST ": all %d rows of %s replayed\n", rows, path);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	check_nothing_required_to_enter_eastern_palace();
	check_can_enter_tower_of_hera_with_everything();
	check_can_enter_inverted_tower_of_hera_with_everything();
	check_inverted_skull_woods_bridge_room_needs_only_fire_rod();
	check_inverted_turtle_rock_chain_chomps_from_the_top();
	check_inverted_ganons_tower_chests_need_moon_pearl();
	check_cape_or_upgraded_sword_required_to_enter_castle_tower();
	check_sword_required_to_complete_castle_tower();
	check_boss_normal_location_decides_dungeon_items_on_bosses();
	check_hera_big_key_chest_follows_accessibility();
	check_key_on_its_own_chest_lowers_the_key_count();
	check_wild_keys_raise_palace_of_darkness_key_counts();
	check_locations_accessibility_refuses_keys_on_their_own_chests();
	check_key_config_decides_swamp_entrance_and_pinball_room();
	check_placement_and_weapons_in_dark_world_dungeons();
	check_big_key_placement_lowers_ice_and_mire_key_counts();
	check_shop_key_and_damage_decide_ice_and_mire_spikes();
	check_basic_placement_in_ice_palace_and_turtle_rock();
	check_turtle_rock_big_key_chest_keys();
	check_turtle_rock_big_chest_and_chain_chomps();
	check_ice_mire_and_turtle_rock_bosses();
	check_big_key_placement_lowers_ganons_tower_key_counts();
	check_basic_placement_in_ganons_tower();
	check_ganons_tower_moldorm_chest_and_completion();
	check_ganons_tower_default_bosses();
	check_spike_room_reads_big_key_from_locations_world();
	check_nested_spike_room_reads_big_key_from_own_world();
	check_thieves_town_big_chest_reads_key_from_locations_world();
	check_wild_compass_on_boss_read_from_own_world();
	check_single_world_access_is_access_with_itself();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	replay_every_row(FIXTURE_NOGLITCHES_ROWS);
	replay_every_row(FIXTURE_OVERWORLD_GLITCHES_ROWS);
	replay_every_row(FIXTURE_MAJOR_GLITCHES_ROWS);
	replay_every_row(FIXTURE_INVERTED_ROWS);
	replay_every_row(FIXTURE_INVERTED_OVERWORLD_GLITCHES_ROWS);
	replay_every_row(FIXTURE_INVERTED_MAJOR_GLITCHES_ROWS);
	return 0;
}
