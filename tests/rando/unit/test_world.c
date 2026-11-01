#include "rando_world.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_world"

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

static int region_holds(RandoVariant variant, RandoRegionId region, RandoLocationId location)
{
	int i;

	for (i = 0; i < rando_layout_region_location_count(variant, region); i++)
		if (rando_layout_region_location(variant, region, i) == location)
			return 1;
	return 0;
}

/* Standard and Open share VT's classic world: 236 locations, none moved. */
static void check_classic_layout(void)
{
	RandoVariant v;

	for (v = RANDO_VARIANT_STANDARD; v <= RANDO_VARIANT_OPEN; v++) {
		CHECK(rando_layout_location_count(v) == 236);
		CHECK(rando_layout_location(v, 0) == RANDO_LOC_SAHASRAHLAS_HUT_LEFT);
		CHECK(rando_layout_location(v, 235) == RANDO_LOC_PYRAMID_BOTTLE);
		CHECK(rando_layout_location_region(v, RANDO_LOC_LINKS_HOUSE) == RANDO_REGION_SOUTH_LIGHT_WORLD);
		CHECK(region_holds(v, RANDO_REGION_SOUTH_LIGHT_WORLD, RANDO_LOC_LINKS_HOUSE));
		CHECK(rando_layout_location_region(v, RANDO_LOC_GANON) == RANDO_REGION_NORTH_EAST_DARK_WORLD);
		CHECK(rando_layout_region_prize(v, RANDO_REGION_NORTH_EAST_DARK_WORLD) == RANDO_LOC_GANON);
		CHECK(rando_layout_region_prize(v, RANDO_REGION_NORTH_EAST_LIGHT_WORLD) == RANDO_LOCATION_NONE);
		CHECK(rando_layout_location_region(v, RANDO_LOC_ETHER_TABLET) == RANDO_REGION_WEST_DEATH_MOUNTAIN);
		CHECK(region_holds(v, RANDO_REGION_WEST_DEATH_MOUNTAIN, RANDO_LOC_ETHER_TABLET));
		CHECK(rando_layout_region_location_count(v, RANDO_REGION_GANONS_TOWER) == 28);
		CHECK(rando_layout_location_region(v, RANDO_LOC_BOMB_MERCHANT) == RANDO_REGION_NONE);
	}
}

/* World::factory('inverted'): Ganon, Link's House, Ether Tablet and Spectacle Rock move, Bomb Merchant
 * is appended to South Light World. */
static void check_inverted_layout(void)
{
	RandoVariant v = RANDO_VARIANT_INVERTED;

	CHECK(rando_layout_location_count(v) == 237);
	CHECK(rando_layout_location_region(v, RANDO_LOC_LINKS_HOUSE) == RANDO_REGION_SOUTH_DARK_WORLD);
	CHECK(region_holds(v, RANDO_REGION_SOUTH_DARK_WORLD, RANDO_LOC_LINKS_HOUSE));
	CHECK(!region_holds(v, RANDO_REGION_SOUTH_LIGHT_WORLD, RANDO_LOC_LINKS_HOUSE));
	CHECK(rando_layout_location_region(v, RANDO_LOC_GANON) == RANDO_REGION_NORTH_EAST_LIGHT_WORLD);
	CHECK(rando_layout_region_prize(v, RANDO_REGION_NORTH_EAST_LIGHT_WORLD) == RANDO_LOC_GANON);
	CHECK(!region_holds(v, RANDO_REGION_NORTH_EAST_DARK_WORLD, RANDO_LOC_GANON));
	CHECK(rando_layout_location_region(v, RANDO_LOC_BOMB_MERCHANT) == RANDO_REGION_SOUTH_LIGHT_WORLD);
	CHECK(rando_layout_location_region(v, RANDO_LOC_ETHER_TABLET) == RANDO_REGION_EAST_DEATH_MOUNTAIN);
	CHECK(rando_layout_location_region(v, RANDO_LOC_SPECTACLE_ROCK) == RANDO_REGION_EAST_DEATH_MOUNTAIN);
	CHECK(rando_layout_region_location_count(v, RANDO_REGION_WEST_DEATH_MOUNTAIN) == 2);
}

/* A location's rule is its region table's entry at the layout slot: the layout's per-region order, each
 * location's {region, slot} and every region file's slot numbering must name the same location. */
static void check_layout_agreement(void)
{
	RandoVariant v;

	for (v = 0; v < RANDO_VARIANT_COUNT; v++) {
		int r, l, i, listed = 0, placed = 0;

		for (r = 0; r < RANDO_REGION_COUNT; r++)
			for (i = 0; i < rando_layout_region_location_count(v, (RandoRegionId)r); i++) {
				RandoLocationId id = rando_layout_region_location(v, (RandoRegionId)r, i);

				if (rando_layout_location_region(v, id) != (RandoRegionId)r
				    || rando_layout_location_slot(v, id) != i)
					test_fail(TEST, "variant %d region %d slot %d: location %d has region %d slot %d", v,
						  r, i, id, rando_layout_location_region(v, id),
						  rando_layout_location_slot(v, id));
				listed++;
			}
		for (l = 0; l < RANDO_LOCATION_COUNT; l++)
			placed += rando_layout_location_region(v, (RandoLocationId)l) != RANDO_REGION_NONE;
		CHECK(placed == rando_layout_location_count(v));
		CHECK(listed == placed);
		for (r = 0; r < RANDO_REGION_COUNT; r++) {
			int n = rando_region_rule_slot_count(v, (RandoRegionId)r);

			if (n < 0)
				continue;
			if (n != rando_layout_region_location_count(v, (RandoRegionId)r))
				test_fail(TEST, "variant %d region %d: rule table has %d slots, layout %d", v, r, n,
					  rando_layout_region_location_count(v, (RandoRegionId)r));
			for (i = 0; i < n; i++)
				if (rando_region_rule_slot_location(v, (RandoRegionId)r, i)
				    != rando_layout_region_location(v, (RandoRegionId)r, i))
					test_fail(TEST, "variant %d region %d slot %d: rules mean location %d, layout %d", v,
						  r, i, rando_region_rule_slot_location(v, (RandoRegionId)r, i),
						  rando_layout_region_location(v, (RandoRegionId)r, i));
		}
	}
}

static void check_inverted_init(void)
{
	RandoWorld world;
	RandoConfig config;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_INVERTED, &config);
	CHECK(world.config.mode_state == RANDO_STATE_INVERTED);
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	CHECK(rando_world_item(&world, RANDO_LOC_BOMB_MERCHANT) == RANDO_ITEM_BigRedBomb);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 0) == RANDO_ITEM_BluePotion);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 0) == 160);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 1) == RANDO_ITEM_BlueShield);

	standard_world(&world);
	CHECK(rando_world_item(&world, RANDO_LOC_BOMB_MERCHANT) == RANDO_ITEM_NONE);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 0) == RANDO_ITEM_RedPotion);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 0) == 150);
}

static RandoConfig retro_config(RandoItemPool pool)
{
	RandoConfig config;
	RandoWorld world;

	rando_config_init(&config);
	config.item_pool = pool;
	rando_world_init(&world, RANDO_VARIANT_RETRO, &config);
	return world.config;
}

/* World\Retro::__construct: four keys forced over the caller's config, then the item counts. */
static void check_retro_init(void)
{
	RandoConfig config, c;
	RandoWorld world;

	rando_config_init(&config);
	config.rom_rupee_bow = false;
	config.rom_generic_keys = false;
	config.region_take_anys = false;
	config.region_wild_keys = false;
	config.logic = RANDO_LOGIC_MAJOR_GLITCHES;
	rando_world_init(&world, RANDO_VARIANT_RETRO, &config);
	c = world.config;
	CHECK(c.mode_state == RANDO_STATE_RETRO);
	CHECK(c.rom_rupee_bow && c.rom_generic_keys && c.region_take_anys && c.region_wild_keys);
	CHECK(!c.rom_vanilla_keys);
	CHECK(c.rom_vanilla_big_keys);

	c = retro_config(RANDO_ITEM_POOL_NORMAL);
	CHECK(c.item_count[RANDO_ITEM_KeyD1] == 0 && c.item_count[RANDO_ITEM_KeyA2] == 0);
	CHECK(c.item_count[RANDO_ITEM_KeyD7] == RANDO_CONFIG_NULL && c.item_count[RANDO_ITEM_KeyD2] == RANDO_CONFIG_NULL);
	CHECK(c.item_count[RANDO_ITEM_TwentyRupees2] == 10);
	c = retro_config(RANDO_ITEM_POOL_CROWD_CONTROL);
	CHECK(c.item_count[RANDO_ITEM_KeyD1] == 0 && c.item_count[RANDO_ITEM_KeyD7] == RANDO_CONFIG_NULL);
	CHECK(c.item_count[RANDO_ITEM_TwentyRupees2] == 14);
	c = retro_config(RANDO_ITEM_POOL_HARD);
	CHECK(c.item_count[RANDO_ITEM_KeyD1] == 0 && c.item_count[RANDO_ITEM_KeyA2] == 0);
	CHECK(c.item_count[RANDO_ITEM_KeyD7] == 0 && c.item_count[RANDO_ITEM_KeyD2] == 0);
	CHECK(c.item_count[RANDO_ITEM_TwentyRupees2] == 15);
	c = retro_config(RANDO_ITEM_POOL_EXPERT);
	CHECK(c.item_count[RANDO_ITEM_KeyD7] == 0 && c.item_count[RANDO_ITEM_KeyD2] == 0);
	CHECK(c.item_count[RANDO_ITEM_TwentyRupees2] == 15);

	CHECK(rando_layout_location_count(RANDO_VARIANT_RETRO) == 236);
	CHECK(strcmp(rando_region_vt_class(RANDO_VARIANT_RETRO, RANDO_REGION_ESCAPE),
		     rando_region_vt_class(RANDO_VARIANT_OPEN, RANDO_REGION_ESCAPE)) == 0);

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	c = world.config;
	CHECK(c.mode_state == RANDO_STATE_OPEN);
	CHECK(!c.rom_rupee_bow && !c.rom_generic_keys && !c.region_take_anys && !c.region_wild_keys);
	CHECK(c.item_count[RANDO_ITEM_KeyD1] == RANDO_CONFIG_NULL);
	CHECK(c.item_count[RANDO_ITEM_TwentyRupees2] == RANDO_CONFIG_NULL);
}

/* World::__construct: the constructor items, VT's default bosses, the shops' default inventory. */
static void check_init(void)
{
	RandoWorld world;

	standard_world(&world);
	CHECK(rando_location_find("Ganon") == RANDO_LOC_GANON);
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_FAIRY_SWORD) == RANDO_ITEM_L1Sword);
	CHECK(rando_world_item(&world, RANDO_LOC_LINKS_UNCLE) == RANDO_ITEM_NONE);
	CHECK(rando_world_boss(&world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE) == RANDO_BOSS_LANMOLAS);
	CHECK(rando_world_boss(&world, RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS)
	      == RANDO_BOSS_ARMOS_KNIGHTS);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 1) == RANDO_ITEM_Bee);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, 2) == RANDO_ITEM_NONE);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_BOMB_HUT, 0) == RANDO_ITEM_NONE);
	CHECK(world.config.mode_state == RANDO_STATE_STANDARD);
	CHECK(rando_items_count(&world.pre_collected) == 0);
}

/* VT's 41 Standard shops: Shop, Shop\Upgrade, Shop\TakeAny. */
static void check_shop_kinds(void)
{
	int i, n[3] = {0, 0, 0};

	for (i = 0; i < RANDO_SHOP_COUNT; i++) {
		RandoShopKind kind = rando_shop_info((RandoShopId)i)->kind;

		CHECK(kind == RANDO_SHOP_KIND_SHOP || kind == RANDO_SHOP_KIND_UPGRADE
		      || kind == RANDO_SHOP_KIND_TAKE_ANY);
		n[kind == RANDO_SHOP_KIND_SHOP ? 0 : kind == RANDO_SHOP_KIND_UPGRADE ? 1 : 2]++;
	}
	CHECK(n[0] == 9 && n[1] == 1 && n[2] == 31);
	CHECK(rando_shop_info(RANDO_SHOP_CAPACITY_UPGRADE)->kind == RANDO_SHOP_KIND_UPGRADE);
	CHECK(rando_shop_info(RANDO_SHOP_DARK_WORLD_FOREST_SHOP)->kind == RANDO_SHOP_KIND_SHOP);
	CHECK(rando_shop_info(RANDO_SHOP_BOMB_HUT)->kind == RANDO_SHOP_KIND_TAKE_ANY);
}

/* Shop::__construct leaves $active false; setActive sets it either way. */
static void check_shop_active(void)
{
	RandoWorld world;
	int i;

	standard_world(&world);
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		CHECK(!rando_world_shop_active(&world, (RandoShopId)i));
	rando_world_shop_set_active(&world, RANDO_SHOP_CAPACITY_UPGRADE, true);
	CHECK(rando_world_shop_active(&world, RANDO_SHOP_CAPACITY_UPGRADE));
	CHECK(!rando_world_shop_active(&world, RANDO_SHOP_LIGHT_WORLD_LAKE_HYLIA_SHOP));
	rando_world_shop_set_active(&world, RANDO_SHOP_CAPACITY_UPGRADE, false);
	CHECK(!rando_world_shop_active(&world, RANDO_SHOP_CAPACITY_UPGRADE));
}

/* The regions' initalize(): addInventory(slot, item, price) for the ten shops with an inventory. */
static void check_shop_prices(void)
{
	RandoWorld world;

	standard_world(&world);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0) == RANDO_ITEM_RedShield);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0) == 500);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 2) == 30);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, 1) == RANDO_ITEM_ArrowUpgrade5);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_CAPACITY_UPGRADE, 1) == 100);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_LIGHT_WORLD_KAKARIKO_SHOP, 0) == 150);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_DARK_WORLD_POTION_SHOP, 1) == 50);
}

/* Shop::clearInventory empties every slot; addInventory's keyed write replaces the slot. */
static void check_shop_inventory(void)
{
	RandoWorld world;
	int i, s;

	standard_world(&world);
	rando_world_shop_clear(&world, RANDO_SHOP_CAPACITY_UPGRADE);
	for (s = 0; s < RANDO_SHOP_SLOTS; s++)
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, s) == RANDO_ITEM_NONE);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_LIGHT_WORLD_LAKE_HYLIA_SHOP, 0) == RANDO_ITEM_RedPotion);

	rando_world_shop_add(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, RANDO_ITEM_TwentyRupees2, 250);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0) == RANDO_ITEM_TwentyRupees2);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0) == 250);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 1) == RANDO_ITEM_Bee);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 1) == 10);

	rando_world_shop_add(&world, RANDO_SHOP_CAPACITY_UPGRADE, 2, RANDO_ITEM_Arrow, 1);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, 2) == RANDO_ITEM_Arrow);
	CHECK(rando_world_shop_price(&world, RANDO_SHOP_CAPACITY_UPGRADE, 2) == 1);
	CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, 0) == RANDO_ITEM_NONE);

	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		CHECK(!rando_world_shop_active(&world, (RandoShopId)i));
}

static RandoConfig derived(RandoItemPool pool, RandoItemFunctionality functionality, RandoGoal goal)
{
	RandoConfig config;
	RandoWorld world;

	rando_config_init(&config);
	config.item_pool = pool;
	config.item_functionality = functionality;
	config.goal = goal;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	return world.config;
}

static int overrides(const RandoConfig *c)
{
	int i, n = 0;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		n += c->item_count[i] != RANDO_CONFIG_NULL;
	return n;
}

/* World::__construct's item.pool and item.functionality switches, and alttp.goals.<goal>.item.count. */
static void check_derivations(void)
{
	RandoConfig c, input;
	RandoWorld world;

	c = derived(RANDO_ITEM_POOL_NORMAL, RANDO_ITEM_FUNCTIONALITY_NORMAL, RANDO_GOAL_GANON);
	CHECK(c.rom_catchable_fairies && c.rom_bottle_fill_magic == 0x80);
	CHECK(c.item_overflow_count_shield == 3);
	CHECK(!c.shops_hard_mode);
	CHECK(c.item_overflow_replacement_shield == RANDO_ITEM_TwentyRupees2);
	CHECK(overrides(&c) == 0);

	c = derived(RANDO_ITEM_POOL_NORMAL, RANDO_ITEM_FUNCTIONALITY_HARD, RANDO_GOAL_GANON);
	CHECK(!c.rom_catchable_fairies && c.rom_bottle_fill_magic == 0x40);
	c = derived(RANDO_ITEM_POOL_NORMAL, RANDO_ITEM_FUNCTIONALITY_EXPERT, RANDO_GOAL_GANON);
	CHECK(!c.rom_catchable_fairies && c.rom_bottle_fill_magic == 0x20);
	CHECK(overrides(&c) == 0);

	c = derived(RANDO_ITEM_POOL_HARD, RANDO_ITEM_FUNCTIONALITY_NORMAL, RANDO_GOAL_GANON);
	CHECK(c.item_overflow_count_shield == 2 && overrides(&c) == 0);
	CHECK(c.shops_hard_mode);
	CHECK(c.rom_catchable_fairies);
	c = derived(RANDO_ITEM_POOL_EXPERT, RANDO_ITEM_FUNCTIONALITY_NORMAL, RANDO_GOAL_GANON);
	CHECK(c.item_overflow_count_shield == 1 && overrides(&c) == 0);
	CHECK(c.shops_hard_mode);
	c = derived(RANDO_ITEM_POOL_CROWD_CONTROL, RANDO_ITEM_FUNCTIONALITY_NORMAL, RANDO_GOAL_GANON);
	CHECK(c.item_overflow_count_shield == 0);
	CHECK(!c.shops_hard_mode);
	CHECK(c.item_overflow_replacement_shield == RANDO_ITEM_TwentyRupees2);
	CHECK(c.item_count[RANDO_ITEM_BugCatchingNet] == 0);
	CHECK(c.item_count[RANDO_ITEM_HalfMagic] == 0);
	CHECK(c.item_count[RANDO_ITEM_CaneOfByrna] == 0);
	CHECK(c.item_count[RANDO_ITEM_Cape] == 0);
	CHECK(c.item_count[RANDO_ITEM_TwentyRupees2] == 4);
	CHECK(overrides(&c) == 5);

	c = derived(RANDO_ITEM_POOL_NORMAL, RANDO_ITEM_FUNCTIONALITY_NORMAL, RANDO_GOAL_TRIFORCE_HUNT);
	CHECK(c.item_count[RANDO_ITEM_TriforcePiece] == 30 && overrides(&c) == 1);
	c = derived(RANDO_ITEM_POOL_NORMAL, RANDO_ITEM_FUNCTIONALITY_NORMAL, RANDO_GOAL_GANONHUNT);
	CHECK(c.item_count[RANDO_ITEM_TriforcePiece] == 50 && overrides(&c) == 1);
	c = derived(RANDO_ITEM_POOL_CROWD_CONTROL, RANDO_ITEM_FUNCTIONALITY_NORMAL, RANDO_GOAL_TRIFORCE_HUNT);
	CHECK(c.item_count[RANDO_ITEM_TriforcePiece] == 30 && overrides(&c) == 6);

	rando_config_init(&input);
	input.goal = RANDO_GOAL_TRIFORCE_HUNT;
	input.item_count[RANDO_ITEM_TriforcePiece] = 0;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &input);
	CHECK(world.config.item_count[RANDO_ITEM_TriforcePiece] == 0);

	rando_config_init(&input);
	input.item_overflow_replacement_shield = RANDO_ITEM_FiftyRupees;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &input);
	CHECK(world.config.item_overflow_replacement_shield == RANDO_ITEM_FiftyRupees);
}

static int glitch_flags_set(const RandoConfig *c, int overworld, int major)
{
	int overworld_set = c->can_super_bunny + c->can_fake_flipper + c->can_super_speed + c->can_boots_clip
			    + c->can_mirror_clip + c->can_water_walk + c->can_dungeon_revive;
	int major_set = c->can_mirror_wrap + c->can_owyba + c->can_one_frame_clip_ow + c->can_one_frame_clip_uw
			+ c->can_transition_wrapped;

	return !c->can_bunny_surf && !c->can_bunny_revive && overworld_set == (overworld ? 7 : 0)
	       && major_set == (major ? 5 : 0);
}

/* config/logic.php's OverworldGlitches block, outside VT's $glitched_logic for the vanilla dungeon items. */
static void check_overworld_glitches_derivation(void)
{
	RandoConfig input;
	RandoWorld world;

	rando_config_init(&input);
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &input);
	CHECK(glitch_flags_set(&world.config, 0, 0));

	input.logic = RANDO_LOGIC_OVERWORLD_GLITCHES;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &input);
	CHECK(glitch_flags_set(&world.config, 1, 0));
	CHECK(!world.config.rom_vanilla_keys && !world.config.rom_vanilla_big_keys && !world.config.rom_vanilla_maps
	      && !world.config.rom_vanilla_compasses);
}

/* config/logic.php's MajorGlitches block, inside VT's $glitched_logic for the vanilla dungeon items. */
static void check_major_glitches_derivation(void)
{
	RandoConfig input;
	RandoWorld world;

	rando_config_init(&input);
	input.logic = RANDO_LOGIC_MAJOR_GLITCHES;
	rando_world_init(&world, RANDO_VARIANT_STANDARD, &input);
	CHECK(glitch_flags_set(&world.config, 1, 1));
	CHECK(world.config.rom_vanilla_keys && world.config.rom_vanilla_big_keys && world.config.rom_vanilla_maps
	      && world.config.rom_vanilla_compasses);
}

/* User story 14's irregular cases of VT's location class tree. */
static void check_location_classes(void)
{
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_BIG_CHEST, RANDO_LOCATION_CLASS_CHEST));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_CHEST, RANDO_LOCATION_CLASS_BIG_CHEST));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_NPC_WITCH, RANDO_LOCATION_CLASS_NPC));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_NPC_ZORA, RANDO_LOCATION_CLASS_NPC));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_NPC_BUG_CATCHING_KID, RANDO_LOCATION_CLASS_NPC));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_NPC_UNCLE, RANDO_LOCATION_CLASS_NPC));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_DROP_BOMBOS, RANDO_LOCATION_CLASS_DROP));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_DROP_ETHER, RANDO_LOCATION_CLASS_DROP));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_STANDING_HERA_BASEMENT, RANDO_LOCATION_CLASS_STANDING));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_DIG_HAUNTED_GROVE, RANDO_LOCATION_CLASS_DIG));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_PRIZE_EVENT, RANDO_LOCATION_CLASS_PRIZE));
	CHECK(rando_location_is_a(RANDO_LOCATION_CLASS_TRADE, RANDO_LOCATION_CLASS_LOCATION));
	CHECK(!rando_location_is_a(RANDO_LOCATION_CLASS_PRIZE_CRYSTAL, RANDO_LOCATION_CLASS_PRIZE_PENDANT));
}

/* Location::setItem and its Pedestal, Prize, Prize\Event and Medallion overrides. */
static void check_set_item(void)
{
	RandoWorld world;

	standard_world(&world);
	CHECK(rando_world_set_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword));
	CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_L2Sword);
	CHECK(rando_world_has_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_L2Sword));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword));
	CHECK(rando_world_set_item(&world, RANDO_LOC_KINGS_TOMB, RANDO_ITEM_MasterSword));
	CHECK(rando_world_item(&world, RANDO_LOC_KINGS_TOMB) == RANDO_ITEM_MasterSword);

	CHECK(!rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_Hammer));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_Crystal3));
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_PendantOfPower));
	CHECK(!rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_DefeatGanon));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE) == RANDO_ITEM_PendantOfPower);
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));

	CHECK(!rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Crystal1));
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	CHECK(rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Triforce));

	CHECK(!rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Hammer));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_NONE));
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Quake));
	CHECK(rando_world_has_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Quake));
}

/* Region::canFill: dungeon items stay home unless wild; the sewers key stays home in Standard. */
static void check_region_can_fill(void)
{
	RandoWorld world;
	RandoConfig config;

	standard_world(&world);
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyP1));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_Key));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyP2));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, RANDO_ITEM_MapP1));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, RANDO_ITEM_Hammer));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, RANDO_ITEM_ShopKey));
	world.config.region_wild_keys = true;
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyP2));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyH2));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_ESCAPE, RANDO_ITEM_KeyH2));

	rando_config_init(&config);
	config.dungeon_items = RANDO_DUNGEON_ITEMS_MCS;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyH2));
	CHECK(rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_CompassP2));
	CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_BigKeyP2));
	CHECK(world.config.rom_map_on_pickup && !world.config.region_wild_big_keys);
}

/* Location::canFill sets the item while it evaluates, then restores the location; Location::fill
 * keeps the item only when canFill succeeds. */
static void check_can_fill_and_fill(void)
{
	RandoWorld world;
	RandoItems zelda;

	standard_world(&world);
	rando_items_clear(&zelda);
	rando_items_add(&zelda, RANDO_ITEM_RescueZelda);
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_Hammer));
	CHECK(rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_KeyP1, &zelda, true));
	CHECK(!rando_world_can_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_KeyP2, &zelda, false));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST) == RANDO_ITEM_Hammer);

	CHECK(!rando_world_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyP2, &zelda));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST) == RANDO_ITEM_Hammer);
	CHECK(rando_world_fill(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyP1, &zelda));
	CHECK(rando_world_item(&world, RANDO_LOC_EASTERN_PALACE_MAP_CHEST) == RANDO_ITEM_BigKeyP1);

	CHECK(rando_world_can_fill(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword, &zelda, false));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_NONE));
}

static int collected_count(const RandoWorld *world)
{
	int n = 0, i, slot;

	for (i = 0; i < rando_layout_location_count(world->variant); i++)
		n += rando_world_collected(world, rando_layout_location(world->variant, i));
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			n += rando_world_shop_collected(world, (RandoShopId)i, slot);
	return n;
}

static int same_items(const RandoItems *a, const RandoItems *b)
{
	return memcmp(a, b, sizeof *a) == 0;
}

/* World::collectItems: properties that hold whatever the region rules, since they are monotone. */
static void check_collect_items(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems first, again, start, out;
	int i, slot, collected;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	rando_world_add_pre_collected(&world, RANDO_ITEM_BugCatchingNet);
	CHECK(rando_world_set_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake));

	rando_world_collect_items(&world, NULL, &first);
	collected = collected_count(&world);
	CHECK(rando_items_count(&first) == 1 + collected);
	CHECK(first.counts[RANDO_ITEM_BugCatchingNet] == 1 && first.values[RANDO_ITEM_BugCatchingNet] == 1);
	CHECK(!rando_world_collected(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION));
	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		CHECK(!rando_world_collected(&world, (RandoLocationId)i)
		      || rando_world_has_item(&world, (RandoLocationId)i, RANDO_ITEM_NONE));
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			CHECK(!rando_world_shop_collected(&world, (RandoShopId)i, slot)
			      || rando_world_shop_item(&world, (RandoShopId)i, slot) != RANDO_ITEM_NONE);

	rando_world_collect_items(&world, NULL, &again);
	CHECK(same_items(&again, &world.pre_collected));
	CHECK(collected_count(&world) == collected);

	rando_world_reset_collected(&world);
	CHECK(collected_count(&world) == 0);
	rando_world_collect_items(&world, NULL, &again);
	CHECK(same_items(&again, &first));

	rando_world_reset_collected(&world);
	rando_items_clear(&start);
	start.counts[RANDO_ITEM_Hookshot] = 1;
	start.values[RANDO_ITEM_MoonPearl] = 1;
	rando_world_collect_items(&world, &start, &out);
	CHECK(out.values[RANDO_ITEM_MoonPearl] == out.counts[RANDO_ITEM_MoonPearl] + 1);
	CHECK(out.values[RANDO_ITEM_Hookshot] + 1 == out.counts[RANDO_ITEM_Hookshot]);
	CHECK(rando_items_count(&out) == 2 + collected_count(&world));
}

/* ItemCollection iterates cached_values in insertion order: Equipped numbers its slots that way. */
static void check_pre_collected_order(void)
{
	static const RandoItemId order[] = {
		RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer,
		RANDO_ITEM_BombUpgrade10, RANDO_ITEM_PegasusBoots, RANDO_ITEM_UncleSword,
	};
	RandoWorld world;
	RandoItems multiset;
	int i;

	standard_world(&world);
	CHECK(rando_world_pre_collected_count(&world) == 0);
	rando_items_clear(&multiset);
	for (i = 0; i < 6; i++) {
		rando_world_add_pre_collected(&world, order[i]);
		rando_items_add(&multiset, order[i]);
	}
	CHECK(rando_world_pre_collected_count(&world) == 6);
	for (i = 0; i < 6; i++)
		CHECK(rando_world_pre_collected_item(&world, i) == order[i]);
	CHECK(same_items(&world.pre_collected, &multiset));

	standard_world(&world);
	CHECK(rando_world_pre_collected_count(&world) == 0);
}

static void every_item_but_health(RandoItems *items, int hearts)
{
	int i;

	rando_items_clear(items);
	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (rando_item_info((RandoItemId)i)->item_class != RANDO_ITEM_CLASS_HEALTH && i != RANDO_ITEM_Triforce)
			rando_items_add(items, (RandoItemId)i);
	for (i = 0; i < hearts; i++)
		rando_items_add(items, RANDO_ITEM_BossHeartContainer);
}

/* World::collectOtherItems: unlike collectItems, no pre-collected merge, so the three pre-collected
 * hearts already in collected count once. Basic placement enters Palace of Darkness at 7 hearts. */
static void check_collect_other_items(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems collected, found, merged, out;
	int i;

	rando_config_init(&config);
	config.item_placement = RANDO_PLACEMENT_BASIC;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	for (i = 0; i < 3; i++)
		rando_world_add_pre_collected(&world, RANDO_ITEM_BossHeartContainer);
	CHECK(rando_world_set_item(&world, RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM, RANDO_ITEM_Hookshot));

	every_item_but_health(&collected, 4);
	rando_world_collect_other_items(&world, &collected, &found);
	CHECK(!rando_world_collected(&world, RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM));
	CHECK(found.counts[RANDO_ITEM_BossHeartContainer] == 0);
	CHECK(rando_items_count(&found) == collected_count(&world));
	CHECK(rando_items_count(&found) > 0);

	rando_world_reset_collected(&world);
	rando_world_collect_items(&world, &collected, &out);
	CHECK(rando_world_collected(&world, RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM));

	rando_world_reset_collected(&world);
	every_item_but_health(&collected, 7);
	rando_world_collect_other_items(&world, &collected, &found);
	CHECK(rando_world_collected(&world, RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM));
	CHECK(found.counts[RANDO_ITEM_Hookshot] == 1);
	CHECK(rando_items_count(&found) == collected_count(&world));
	rando_items_merge(&merged, &collected, &world.pre_collected);
	rando_items_merge(&merged, &merged, &found);
	rando_world_reset_collected(&world);
	rando_world_collect_items(&world, &collected, &out);
	CHECK(same_items(&out, &merged));

	rando_world_collect_other_items(&world, &collected, &found);
	CHECK(rando_items_count(&found) == 0);
}

/* getCollectedLocationsCount counts the collected_locations keys, shop slots included. */
static void check_collected_count(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems out;
	int i, slot, shops = 0;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_collected_count(&world) == 0);
	rando_world_collect_items(&world, NULL, &out);
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			shops += rando_world_shop_collected(&world, (RandoShopId)i, slot);
	CHECK(shops > 0);
	CHECK(rando_world_collected_count(&world) == collected_count(&world));
	rando_world_reset_collected(&world);
	CHECK(rando_world_collected_count(&world) == 0);
}

/* WorldCollection::isWinnable for one world: reset, collectOtherItems from the pre-collected items
 * until the collected count is stable, then the win condition. */
static void check_is_winnable(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems out;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Triforce));
	CHECK(!rando_world_is_winnable(&world));
	CHECK(rando_world_collected_count(&world) > 0);

	CHECK(rando_world_set_item(&world, RANDO_LOC_KAKARIKO_TAVERN, RANDO_ITEM_Triforce));
	CHECK(rando_world_is_winnable(&world));
	CHECK(rando_world_collected(&world, RANDO_LOC_KAKARIKO_TAVERN));

	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Triforce));
	rando_items_all(&world.pre_collected);
	rando_items_unset(&world.pre_collected, RANDO_ITEM_Triforce);
	rando_world_collect_items(&world, NULL, &out);
	CHECK(rando_world_collected(&world, RANDO_LOC_GANON));
	CHECK(rando_world_is_winnable(&world));
}

/* World::copy: a new world from the config, the same location items, shops, bosses and an ordered
 * copy of the pre-collected items, with no collected state and no collectable cache. */
static void check_copy(void)
{
	static RandoWorld world, before, copy;
	RandoConfig config;
	RandoItems out;
	int i, shop = -1, slot = 0, s2;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_KAKARIKO_TAVERN, RANDO_ITEM_Hookshot));
	rando_world_set_boss(&world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, RANDO_BOSS_MOLDORM);
	rando_world_add_pre_collected(&world, RANDO_ITEM_PegasusBoots);
	rando_world_add_pre_collected(&world, RANDO_ITEM_BossHeartContainer);
	rando_world_collect_items(&world, NULL, &out);
	for (i = 0; i < RANDO_SHOP_COUNT && shop < 0; i++)
		for (s2 = 0; s2 < RANDO_SHOP_SLOTS && shop < 0; s2++)
			if (rando_world_shop_collected(&world, (RandoShopId)i, s2)) {
				shop = i;
				slot = s2;
			}
	CHECK(shop >= 0);
	rando_world_shop_add(&world, (RandoShopId)shop, slot, RANDO_ITEM_Triforce, 10);
	before = world;

	rando_world_copy(&copy, &world);
	CHECK(memcmp(&world, &before, sizeof world) == 0);
	CHECK(rando_world_item(&copy, RANDO_LOC_KAKARIKO_TAVERN) == RANDO_ITEM_Hookshot);
	CHECK(memcmp(copy.location_items, world.location_items, sizeof world.location_items) == 0);
	CHECK(rando_world_boss(&copy, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP) == RANDO_BOSS_MOLDORM);
	CHECK(memcmp(copy.bosses, world.bosses, sizeof world.bosses) == 0);
	CHECK(rando_world_shop_item(&copy, (RandoShopId)shop, slot) == RANDO_ITEM_Triforce);
	CHECK(memcmp(copy.shop_items, world.shop_items, sizeof world.shop_items) == 0);
	CHECK(memcmp(copy.shop_prices, world.shop_prices, sizeof world.shop_prices) == 0);
	CHECK(memcmp(copy.shop_active, world.shop_active, sizeof world.shop_active) == 0);
	CHECK(rando_world_pre_collected_count(&copy) == 2);
	CHECK(rando_world_pre_collected_item(&copy, 0) == RANDO_ITEM_PegasusBoots);
	CHECK(rando_world_pre_collected_item(&copy, 1) == RANDO_ITEM_BossHeartContainer);
	CHECK(same_items(&copy.pre_collected, &world.pre_collected));
	CHECK(rando_world_collected_count(&copy) == 0);

	/* The source's cache still holds the shop slot's old item; the copy's sees the Triforce. */
	CHECK(!rando_world_is_winnable(&world));
	CHECK(rando_world_is_winnable(&copy));
}

static const char *const location_class_names[RANDO_LOCATION_CLASS_COUNT] = {
	[RANDO_LOCATION_CLASS_LOCATION] = "Location",
	[RANDO_LOCATION_CLASS_CHEST] = "Location\\Chest",
	[RANDO_LOCATION_CLASS_BIG_CHEST] = "Location\\BigChest",
	[RANDO_LOCATION_CLASS_DASH] = "Location\\Dash",
	[RANDO_LOCATION_CLASS_DIG] = "Location\\Dig",
	[RANDO_LOCATION_CLASS_DIG_HAUNTED_GROVE] = "Location\\Dig\\HauntedGrove",
	[RANDO_LOCATION_CLASS_DROP] = "Location\\Drop",
	[RANDO_LOCATION_CLASS_DROP_BOMBOS] = "Location\\Drop\\Bombos",
	[RANDO_LOCATION_CLASS_DROP_ETHER] = "Location\\Drop\\Ether",
	[RANDO_LOCATION_CLASS_FOUNTAIN] = "Location\\Fountain",
	[RANDO_LOCATION_CLASS_MEDALLION] = "Location\\Medallion",
	[RANDO_LOCATION_CLASS_NPC] = "Location\\Npc",
	[RANDO_LOCATION_CLASS_NPC_BUG_CATCHING_KID] = "Location\\Npc\\BugCatchingKid",
	[RANDO_LOCATION_CLASS_NPC_UNCLE] = "Location\\Npc\\Uncle",
	[RANDO_LOCATION_CLASS_NPC_WITCH] = "Location\\Npc\\Witch",
	[RANDO_LOCATION_CLASS_NPC_ZORA] = "Location\\Npc\\Zora",
	[RANDO_LOCATION_CLASS_PEDESTAL] = "Location\\Pedestal",
	[RANDO_LOCATION_CLASS_PRIZE] = "Location\\Prize",
	[RANDO_LOCATION_CLASS_PRIZE_CRYSTAL] = "Location\\Prize\\Crystal",
	[RANDO_LOCATION_CLASS_PRIZE_EVENT] = "Location\\Prize\\Event",
	[RANDO_LOCATION_CLASS_PRIZE_PENDANT] = "Location\\Prize\\Pendant",
	[RANDO_LOCATION_CLASS_STANDING] = "Location\\Standing",
	[RANDO_LOCATION_CLASS_STANDING_HERA_BASEMENT] = "Location\\Standing\\HeraBasement",
	[RANDO_LOCATION_CLASS_TRADE] = "Location\\Trade",
};

static const char *const slot_names[RANDO_BOSS_SLOT_COUNT] = {"boss", "top", "middle", "bottom"};

static const char *item_name(RandoItemId id)
{
	return id == RANDO_ITEM_NONE ? "-" : rando_item_info(id)->name;
}

static RandoVariant variant_of(const Fixture *fx, const char *name)
{
	if (strcmp(name, "standard") == 0)
		return RANDO_VARIANT_STANDARD;
	if (strcmp(name, "open") == 0)
		return RANDO_VARIANT_OPEN;
	if (strcmp(name, "inverted") == 0)
		return RANDO_VARIANT_INVERTED;
	if (strcmp(name, "retro") == 0)
		return RANDO_VARIANT_RETRO;
	fixture_fail(fx, "unknown variant");
	return RANDO_VARIANT_COUNT;
}

static void expect_field(const Fixture *fx, const char *want, const char *got, const char *what)
{
	if (strcmp(want, got) != 0) {
		fprintf(stderr, "%s: VT \"%s\", C \"%s\"\n", what, want, got);
		fixture_fail(fx, "differs from VT");
	}
}

static void check_region_record(const Fixture *fx, RandoVariant variant, int id)
{
	char buf[512];
	const RandoRegionInfo *info;
	RandoLocationId prize;
	RandoWorld world;
	RandoConfig config;
	size_t n = 0;
	int k;

	if (id >= RANDO_REGION_COUNT)
		fixture_fail(fx, "more regions in the dump than in the C table");
	info = rando_region_info((RandoRegionId)id);
	expect_field(fx, fx->field[2], info->key, "world key");
	if (rando_region_find(fx->field[2]) != (RandoRegionId)id)
		fixture_fail(fx, "rando_region_find does not return the region's id");
	expect_field(fx, fx->field[3], info->spoiler_name, "spoiler name");
	expect_field(fx, fx->field[4], rando_region_vt_class(variant, (RandoRegionId)id), "VT class");
	prize = rando_layout_region_prize(variant, (RandoRegionId)id);
	expect_field(fx, fx->field[5], prize == RANDO_LOCATION_NONE ? "-" : rando_location_info(prize)->name,
		     "prize location");
	rando_config_init(&config);
	rando_world_init(&world, variant, &config);
	buf[0] = '\0';
	for (k = 0; k < info->nboss_slots; k++)
		n += (size_t)snprintf(buf + n, sizeof buf - n, "%s%s=%s", k ? "|" : "", slot_names[k],
				      rando_boss_name(rando_world_boss(&world, (RandoRegionId)id, (RandoBossSlot)k)));
	for (; k < RANDO_BOSS_SLOT_COUNT; k++)
		if (rando_world_boss(&world, (RandoRegionId)id, (RandoBossSlot)k) != RANDO_BOSS_NONE)
			fixture_fail(fx, "a boss in a slot past the region's boss slots");
	expect_field(fx, fx->field[6], n ? buf : "-", "boss slots");
	n = 0;
	buf[0] = '\0';
	for (k = 0; k < info->nitems; k++)
		n += (size_t)snprintf(buf + n, sizeof buf - n, "%s%s", k ? "|" : "",
				      item_name((RandoItemId)info->items[k]));
	expect_field(fx, fx->field[7], n ? buf : "-", "region items");
}

/* i is the record's index in VT's getLocations() order. */
static void check_location_record(const Fixture *fx, RandoVariant variant, int i)
{
	const RandoLocationInfo *info;
	RandoLocationId id;
	RandoWorld world;
	RandoConfig config;

	if (i >= rando_layout_location_count(variant))
		fixture_fail(fx, "more locations in the dump than in the C layout");
	id = rando_layout_location(variant, i);
	info = rando_location_info(id);
	expect_field(fx, fx->field[2], info->name, "location name");
	if (rando_location_find(fx->field[2]) != id)
		fixture_fail(fx, "rando_location_find does not return the location's id");
	expect_field(fx, fx->field[3], rando_region_info(rando_layout_location_region(variant, id))->key,
		     "location region");
	expect_field(fx, fx->field[4], location_class_names[info->location_class], "location class");
	rando_config_init(&config);
	rando_world_init(&world, variant, &config);
	expect_field(fx, fx->field[5], item_name(rando_world_item(&world, id)), "constructor item");
}

/* The regions' lists, in region order, are the world's getLocations() order, and a location the
 * world lacks has no region. */
static void check_region_spans(RandoVariant variant)
{
	int r, l, next = 0, present = 0;

	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		const char *key = rando_region_info((RandoRegionId)r)->key;
		int n = rando_layout_region_location_count(variant, (RandoRegionId)r);

		if (next + n > rando_layout_location_count(variant))
			test_fail(TEST, "variant %d: region %s spans past the world's locations", variant, key);
		for (l = 0; l < n; l++) {
			RandoLocationId id = rando_layout_region_location(variant, (RandoRegionId)r, l);

			if (id != rando_layout_location(variant, next + l))
				test_fail(TEST, "variant %d: region %s is not a span of the world's order", variant, key);
			if (rando_layout_location_region(variant, id) != (RandoRegionId)r)
				test_fail(TEST, "variant %d: region %s lists a location of another region", variant, key);
		}
		next += n;
	}
	if (next != rando_layout_location_count(variant))
		test_fail(TEST, "variant %d: the regions span %d locations, the world has %d", variant, next,
			  rando_layout_location_count(variant));
	for (l = 0; l < RANDO_LOCATION_COUNT; l++)
		present += rando_layout_location_region(variant, (RandoLocationId)l) != RANDO_REGION_NONE;
	if (present != next)
		test_fail(TEST, "variant %d: %d locations have a region, the world has %d", variant, present, next);
}

/* Returns the number of region and location records checked. */
static int check_world_dump(void)
{
	int regions[RANDO_VARIANT_COUNT] = {0}, locations[RANDO_VARIANT_COUNT] = {0}, v, records = 0;
	Fixture fx;

	fixture_open(&fx, FIXTURE_WORLD_DUMP);
	while (fixture_next(&fx)) {
		if (strcmp(fx.field[0], "region") == 0) {
			v = variant_of(&fx, fx.field[1]);
			check_region_record(&fx, (RandoVariant)v, regions[v]++);
		} else if (strcmp(fx.field[0], "location") == 0) {
			v = variant_of(&fx, fx.field[1]);
			check_location_record(&fx, (RandoVariant)v, locations[v]++);
		} else {
			continue;
		}
		records++;
	}
	fixture_close(&fx);
	for (v = 0; v < RANDO_VARIANT_COUNT; v++)
		if (regions[v] != RANDO_REGION_COUNT || locations[v] != rando_layout_location_count((RandoVariant)v))
			test_fail(TEST, "variant %d: the dump has %d regions and %d locations, the C layout %d and %d",
				  v, regions[v], locations[v], RANDO_REGION_COUNT,
				  rando_layout_location_count((RandoVariant)v));
	for (v = 0; v < RANDO_VARIANT_COUNT; v++)
		check_region_spans((RandoVariant)v);
	if (rando_location_find("No Such Location") != RANDO_LOCATION_NONE
	    || rando_region_find("Light World") != RANDO_REGION_NONE)
		test_fail(TEST, "a lookup of an unknown name does not return its NONE sentinel");
	return records;
}

static void check_collected_set(const Fixture *fx, const RandoWorld *world, const char *want)
{
	char got[FIXTURE_LINE_MAX];
	size_t n = 0;
	int i, slot;

	got[0] = '\0';
	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		RandoLocationId l = rando_layout_location(world->variant, i);

		if (rando_world_collected(world, l))
			n += (size_t)snprintf(got + n, sizeof got - n, "%s%s", n ? "|" : "", rando_location_info(l)->name);
	}
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			if (rando_world_shop_collected(world, (RandoShopId)i, slot))
				n += (size_t)snprintf(got + n, sizeof got - n, "%s%s - %d", n ? "|" : "",
						      rando_shop_info((RandoShopId)i)->name, slot);
	expect_field(fx, want, n ? got : "-", "collected locations");
}

static void check_items_equal(const Fixture *fx, const RandoItems *want, const RandoItems *got)
{
	int i;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (want->counts[i] != got->counts[i] || want->values[i] != got->values[i]) {
			fprintf(stderr, "item %s: VT %u/%u, C %u/%u (counts/values)\n",
				rando_item_info((RandoItemId)i)->name, want->counts[i], want->values[i],
				got->counts[i], got->values[i]);
			fixture_fail(fx, "collected items differ from VT");
		}
}

static RandoLocationId location_of(const Fixture *fx, const char *name)
{
	RandoLocationId id = rando_location_find(name);

	if (id == RANDO_LOCATION_NONE)
		fixture_fail(fx, "unknown location");
	return id;
}

static RandoItemId item_of(const Fixture *fx, const char *name)
{
	RandoItemId id = rando_item_find(name);

	if (id == RANDO_ITEM_NONE)
		fixture_fail(fx, "unknown item");
	return id;
}

static void collect_scenario(const Fixture *fx, RandoWorld *world)
{
	char placements[FIXTURE_LINE_MAX];
	char *p = placements;
	RandoConfig config;

	rando_config_init(&config);
	rando_world_init(world, variant_of(fx, fx->field[2]), &config);
	strcpy(placements, fx->field[3]);
	while (strcmp(placements, "-") != 0 && p) {
		char *bar = strchr(p, '|'), *eq = strchr(p, '=');

		if (bar)
			*bar = '\0';
		if (!eq)
			fixture_fail(fx, "placement without '='");
		*eq = '\0';
		if (!rando_world_set_item(world, location_of(fx, p), item_of(fx, eq + 1)))
			fixture_fail(fx, "placement refused by its location");
		p = bar ? bar + 1 : NULL;
	}
	fixture_parse_items(fx, fx->field[4], fx->field[5], &world->pre_collected);
}

static void collect_step(const Fixture *fx, RandoWorld *world)
{
	const char *op = fx->field[3];

	if (strcmp(op, "collect") == 0) {
		RandoItems start, want, got;
		int has_start = strcmp(fx->field[4], "-") != 0;

		if (has_start)
			fixture_parse_items(fx, fx->field[4], fx->field[5], &start);
		rando_world_collect_items(world, has_start ? &start : NULL, &got);
		fixture_parse_items(fx, fx->field[7], fx->field[8], &want);
		check_items_equal(fx, &want, &got);
	} else if (strcmp(op, "reset") == 0) {
		rando_world_reset_collected(world);
	} else if (strcmp(op, "place") == 0) {
		if (!rando_world_set_item(world, location_of(fx, fx->field[4]), item_of(fx, fx->field[5])))
			fixture_fail(fx, "placement refused by its location");
	} else if (strcmp(op, "fill") == 0) {
		RandoItems none;

		rando_items_clear(&none);
		if (rando_world_fill(world, location_of(fx, fx->field[4]), item_of(fx, fx->field[5]), &none)
		    != atoi(fx->field[6]))
			fixture_fail(fx, "fill answer differs from VT");
	} else {
		fixture_fail(fx, "unknown collect step");
	}
	check_collected_set(fx, world, fx->field[9]);
}

/* Returns the number of collect steps replayed. */
static int check_collect_vectors(void)
{
	RandoWorld world;
	Fixture fx;
	int steps = 0, scenario = 0;

	fixture_open(&fx, FIXTURE_NOGLITCHES_ROWS);
	while (fixture_next(&fx)) {
		if (strcmp(fx.field[0], "collect_scenario") == 0) {
			collect_scenario(&fx, &world);
			scenario = 1;
		} else if (strcmp(fx.field[0], "collect_step") == 0) {
			if (!scenario)
				fixture_fail(&fx, "collect step before any scenario");
			collect_step(&fx, &world);
			steps++;
		}
	}
	fixture_close(&fx);
	return steps;
}

static int region_locked_items(const char *file, const char *method)
{
	(void)file;
	return strcmp(method, "testRegionLockedItems") == 0;
}

static int vanilla_dungeon_items(const char *file, const char *method)
{
	(void)file;
	return strcmp(method, "testSetVanillaDungeonItems") == 0;
}

/* placeBosses: Region::setBoss, and GanonsTower's level argument. */
static void check_set_boss(void)
{
	RandoWorld world;

	standard_world(&world);
	rando_world_set_boss(&world, RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_KHOLDSTARE);
	rando_world_set_boss(&world, RANDO_REGION_HYRULE_CASTLE_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM);
	rando_world_set_boss(&world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, RANDO_BOSS_ARMOS_KNIGHTS);
	CHECK(rando_world_boss(&world, RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS) == RANDO_BOSS_KHOLDSTARE);
	CHECK(rando_world_boss(&world, RANDO_REGION_HYRULE_CASTLE_TOWER, RANDO_BOSS_SLOT_BOSS) == RANDO_BOSS_AGAHNIM);
	CHECK(rando_world_boss(&world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP) == RANDO_BOSS_ARMOS_KNIGHTS);
	CHECK(rando_world_boss(&world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE) == RANDO_BOSS_LANMOLAS);
}

/* World::__construct: eleven empty prize packs, 0 to 6 with 8 slots, then pull 3, crab 2, stun 1, fish 1. */
static void check_prize_packs_empty(void)
{
	static const int slots[RANDO_PRIZE_PACK_COUNT] = {8, 8, 8, 8, 8, 8, 8, 3, 2, 1, 1};
	RandoWorld world;
	int p, s;

	standard_world(&world);
	for (p = 0; p < RANDO_PRIZE_PACK_COUNT; p++) {
		CHECK(rando_prize_pack_slots((RandoPrizePack)p) == slots[p]);
		for (s = 0; s < slots[p]; s++)
			CHECK(rando_world_drop(&world, (RandoPrizePack)p, s) == RANDO_DROP_NONE);
	}
}

static void check_set_drop(void)
{
	RandoWorld world;

	standard_world(&world);
	rando_world_set_drop(&world, RANDO_PRIZE_PACK_6, 7, RANDO_DROP_FAIRY);
	rando_world_set_drop(&world, RANDO_PRIZE_PACK_FISH, 0, RANDO_DROP_HEART);
	CHECK(rando_world_drop(&world, RANDO_PRIZE_PACK_6, 7) == RANDO_DROP_FAIRY);
	CHECK(rando_world_drop(&world, RANDO_PRIZE_PACK_6, 6) == RANDO_DROP_NONE);
	CHECK(rando_world_drop(&world, RANDO_PRIZE_PACK_FISH, 0) == RANDO_DROP_HEART);
	CHECK(rando_world_drop(&world, RANDO_PRIZE_PACK_STUN, 0) == RANDO_DROP_NONE);
}

static void hunt_pieces(RandoItems *items, int n)
{
	int i;

	rando_items_clear(items);
	for (i = 0; i < n; i++)
		rando_items_add(items, RANDO_ITEM_TriforcePiece);
}

/* World::__construct's win_condition: has('Triforce'), or North East Light World canEnter and goal
 * triforce-hunt and has('TriforcePiece', item.Goal.Required). */
static void check_win_condition(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems items;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	rando_items_clear(&items);
	rando_items_add(&items, RANDO_ITEM_Triforce);
	CHECK(rando_world_check_win_condition(&world, &items));
	rando_world_reset_collected(&world);
	hunt_pieces(&items, 20);
	rando_items_add(&items, RANDO_ITEM_RescueZelda);
	CHECK(!rando_world_check_win_condition(&world, &items));

	config.goal = RANDO_GOAL_TRIFORCE_HUNT;
	config.item_goal_required = 20;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	hunt_pieces(&items, 20);
	rando_items_add(&items, RANDO_ITEM_RescueZelda);
	CHECK(rando_world_region_can_enter(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, &items));
	CHECK(rando_world_check_win_condition(&world, &items));
	rando_world_reset_collected(&world);
	hunt_pieces(&items, 19);
	rando_items_add(&items, RANDO_ITEM_RescueZelda);
	CHECK(!rando_world_check_win_condition(&world, &items));
}

/* The win_condition closure alone, on the given items: nothing is collected. */
static void check_bare_win_condition(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems items;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	rando_items_clear(&items);
	CHECK(!rando_world_win_condition(&world, &items));
	rando_items_add(&items, RANDO_ITEM_Triforce);
	CHECK(rando_world_win_condition(&world, &items));
	CHECK(collected_count(&world) == 0);
	hunt_pieces(&items, 20);
	rando_items_add(&items, RANDO_ITEM_RescueZelda);
	CHECK(!rando_world_win_condition(&world, &items));

	config.goal = RANDO_GOAL_TRIFORCE_HUNT;
	config.item_goal_required = 20;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_win_condition(&world, &items));
	hunt_pieces(&items, 19);
	rando_items_add(&items, RANDO_ITEM_RescueZelda);
	CHECK(!rando_world_win_condition(&world, &items));
	hunt_pieces(&items, 20);
	CHECK(!rando_world_region_can_enter(&world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, &items));
	CHECK(!rando_world_win_condition(&world, &items));
	CHECK(collected_count(&world) == 0);
}

/* checkWinCondition calls collectItems, which skips locations already collected: fillPrizes
 * depends on the check not resetting them. */
static void check_win_keeps_collected(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoItems start, out;

	rando_config_init(&config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Triforce));
	rando_items_all(&start);
	rando_items_unset(&start, RANDO_ITEM_Triforce);
	CHECK(!rando_items_has(&start, RANDO_ITEM_Triforce));
	CHECK(rando_world_check_win_condition(&world, &start));
	CHECK(rando_world_collected(&world, RANDO_LOC_GANON));
	CHECK(!rando_world_check_win_condition(&world, &start));

	rando_world_reset_collected(&world);
	rando_world_collect_items(&world, &start, &out);
	CHECK(rando_world_collected(&world, RANDO_LOC_GANON));
	CHECK(!rando_world_check_win_condition(&world, &start));
}

/* VT's World::__construct skips every region's initalize() under NoLogic; values from a VT probe. */
static void check_no_logic_world(void)
{
	static const RandoVariant variants[] = {RANDO_VARIANT_OPEN, RANDO_VARIANT_STANDARD};
	RandoWorld world;
	RandoConfig config;
	RandoItems none;
	int i;

	rando_items_clear(&none);
	for (i = 0; i < 2; i++) {
		rando_config_init(&config);
		config.logic = RANDO_LOGIC_NO_LOGIC;
		rando_world_init(&world, variants[i], &config);
		CHECK(rando_world_can_fill(&world, RANDO_LOC_LINKS_UNCLE, RANDO_ITEM_MapH2, &none, false));
		CHECK(rando_world_can_fill(&world, RANDO_LOC_SECRET_PASSAGE, RANDO_ITEM_MapH2, &none, false));
		CHECK(rando_world_can_access(&world, RANDO_LOC_GANON, &none));
		CHECK(rando_world_can_access(&world, RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST, &none));
		CHECK(rando_world_can_access(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, &none));
		CHECK(rando_world_can_access_shop(&world, RANDO_SHOP_CAPACITY_UPGRADE, &none));
		CHECK(!rando_world_can_fill(&world, RANDO_LOC_PALACE_OF_DARKNESS_MAP_CHEST, RANDO_ITEM_MapP1, &none,
					    false));
		CHECK(!rando_world_region_can_fill(&world, RANDO_REGION_EASTERN_PALACE, RANDO_ITEM_KeyH2));
		CHECK(!rando_world_win_condition(&world, &none));

		rando_config_init(&config);
		rando_world_init(&world, variants[i], &config);
		CHECK(!rando_world_can_fill(&world, RANDO_LOC_LINKS_UNCLE, RANDO_ITEM_MapH2, &none, false));
		CHECK(!rando_world_can_access(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL, &none));
	}
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	int rows, total;

	check_classic_layout();
	check_inverted_layout();
	check_layout_agreement();
	check_inverted_init();
	check_retro_init();
	check_init();
	check_shop_kinds();
	check_shop_active();
	check_shop_prices();
	check_shop_inventory();
	check_derivations();
	check_overworld_glitches_derivation();
	check_major_glitches_derivation();
	check_location_classes();
	check_set_item();
	check_region_can_fill();
	check_can_fill_and_fill();
	check_no_logic_world();
	check_collect_items();
	check_pre_collected_order();
	check_collect_other_items();
	check_collected_count();
	check_is_winnable();
	check_copy();
	check_set_boss();
	check_prize_packs_empty();
	check_set_drop();
	check_win_condition();
	check_win_keeps_collected();
	check_bare_win_condition();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	printf(TEST ": %d world dump records checked\n", check_world_dump());
	rows = fixture_replay_rows(TEST, FIXTURE_NOGLITCHES_ROWS, region_locked_items, &total);
	if (rows != 450)
		test_fail(TEST, "%d testRegionLockedItems rows replayed, the four classes and RegionTest have 450", rows);
	printf(TEST ": %d of %d rows replayed (testRegionLockedItems)\n", rows, total);
	rows = fixture_replay_rows(TEST, FIXTURE_NOGLITCHES_ROWS, vanilla_dungeon_items, &total);
	if (rows != 8)
		test_fail(TEST, "%d testSetVanillaDungeonItems rows replayed, WorldTest has 8", rows);
	printf(TEST ": %d of %d rows replayed (testSetVanillaDungeonItems)\n", rows, total);
	rows = check_collect_vectors();
	if (rows == 0)
		test_fail(TEST, "no collectItems step replayed");
	printf(TEST ": %d collectItems steps replayed\n", rows);
	return 0;
}
