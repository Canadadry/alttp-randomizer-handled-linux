#include "rando_prepare.h"
#include "rando_pool.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef char prize_location_ids_fit_uint8[RANDO_LOCATION_COUNT <= 256 ? 1 : -1];

static void place(RandoWorld *world, RandoLocationId location, RandoItemId item)
{
	bool placed = rando_world_set_item(world, location, item);

	assert(placed && "prepareWorld: a location refused its item");
	(void)placed;
}

void rando_prepare_start_equipment(RandoWorld *world)
{
	static const RandoItemId equipment[] = {
		RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer,
		RANDO_ITEM_BombUpgrade10, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade10,
	};
	size_t i;

	if (rando_items_count(&world->pre_collected) != 0)
		return;
	for (i = 0; i < sizeof equipment / sizeof equipment[0]; i++)
		rando_world_add_pre_collected(world, equipment[i]);
}

void rando_prepare_goal(RandoWorld *world)
{
	RandoConfig *c = &world->config;

	if (c->goal == RANDO_GOAL_COMPLETIONIST)
		c->accessibility = RANDO_ACCESSIBILITY_LOCATIONS;

	switch (c->goal) {
	case RANDO_GOAL_PEDESTAL:
		place(world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_Triforce);
		break;
	case RANDO_GOAL_GANON:
	case RANDO_GOAL_FAST_GANON:
	case RANDO_GOAL_DUNGEONS:
	case RANDO_GOAL_GANONHUNT:
	case RANDO_GOAL_COMPLETIONIST:
		place(world, RANDO_LOC_GANON, RANDO_ITEM_Triforce);
		break;
	default:
		break;
	}
}

static void drop_first_boots(RandoPools *pools)
{
	if (rando_pool_remove_first(pools, RANDO_POOL_ADVANCEMENT, RANDO_ITEM_PegasusBoots))
		rando_pool_append(pools, RANDO_POOL_ITEMS, RANDO_ITEM_TwentyRupees);
}

void rando_prepare_pre_collect(RandoWorld *world, RandoPools *pools)
{
	RandoLogic logic = world->config.logic;

	if (logic == RANDO_LOGIC_MAJOR_GLITCHES || logic == RANDO_LOGIC_HYBRID_MAJOR_GLITCHES
	    || logic == RANDO_LOGIC_OVERWORLD_GLITCHES || logic == RANDO_LOGIC_NO_LOGIC) {
		rando_world_add_pre_collected(world, RANDO_ITEM_PegasusBoots);
		drop_first_boots(pools);
	}

	if (world->config.mode_state != RANDO_STATE_STANDARD)
		rando_world_add_pre_collected(world, RANDO_ITEM_RescueZelda);
}

static const RandoShopId shield_shops[] = {
	RANDO_SHOP_DARK_WORLD_POTION_SHOP,
	RANDO_SHOP_DARK_WORLD_LUMBERJACK_HUT_SHOP,
	RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP,
	RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP,
};

static void add_at_default_price(RandoWorld *world, RandoShopId shop, int slot, RandoItemId item)
{
	rando_world_shop_add(world, shop, slot, item, rando_shop_info(shop)->default_prices[slot]);
}

static void set_take_anys(RandoWorld *world, RandoPrng *prng)
{
	uint8_t ids[RANDO_SHOP_COUNT], chosen[4];
	int i, n = 0, k;
	RandoShopId old_man;
	bool sword = world->config.mode_weapons != RANDO_WEAPONS_SWORDLESS
		&& world->config.mode_weapons != RANDO_WEAPONS_VANILLA;

	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind == RANDO_SHOP_KIND_TAKE_ANY)
			ids[n++] = (uint8_t)i;
	k = rando_random_collection(prng, ids, n, 4, chosen);
	for (i = 0; i < k; i++) {
		rando_world_shop_set_active(world, (RandoShopId)chosen[i], true);
		rando_world_shop_add(world, (RandoShopId)chosen[i], 0, RANDO_ITEM_BluePotion, 0);
		rando_world_shop_add(world, (RandoShopId)chosen[i], 1, RANDO_ITEM_BossHeartContainer, 0);
	}
	for (i = 0, n = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind == RANDO_SHOP_KIND_TAKE_ANY
		    && !rando_world_shop_active(world, (RandoShopId)i))
			ids[n++] = (uint8_t)i;
	assert(n > 0);
	old_man = (RandoShopId)ids[rando_random_int(prng, 0, n - 1)];
	rando_world_shop_set_active(world, old_man, true);
	rando_world_shop_add(world, old_man, 0, sword ? RANDO_ITEM_ProgressiveSword : RANDO_ITEM_ThreeHundredRupees, 0);
}

static void set_retro_sellers(RandoWorld *world, RandoPrng *prng)
{
	const RandoConfig *c = &world->config;
	uint8_t ids[RANDO_SHOP_COUNT], chosen[5];
	int i, n = 0, k;

	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind == RANDO_SHOP_KIND_SHOP
		    && (world->variant != RANDO_VARIANT_INVERTED || i != RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP))
			ids[n++] = (uint8_t)i;
	k = rando_random_collection(prng, ids, n, 5, chosen);
	for (i = 0; i < k; i++) {
		RandoShopId shop = (RandoShopId)chosen[i];

		rando_world_shop_set_active(world, shop, true);
		if (c->rom_rupee_bow)
			rando_world_shop_add(world, shop, 0, RANDO_ITEM_ShopArrow, 80);
		if (c->rom_generic_keys)
			rando_world_shop_add(world, shop, 1, RANDO_ITEM_ShopKey, 100);
		rando_world_shop_add(world, shop, 2, RANDO_ITEM_TenBombs, 50);
	}
}

static void set_rupee_bow_shops(RandoWorld *world)
{
	int s;

	rando_world_shop_set_active(world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, true);
	for (s = 0; s < RANDO_SHOP_SLOTS; s++) {
		RandoItemId item = rando_world_shop_item(world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, s);

		if (item != RANDO_ITEM_NONE && rando_item_info(item)->item_class == RANDO_ITEM_CLASS_ARROW)
			rando_world_shop_add(world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, s, RANDO_ITEM_ShopArrow, 80);
	}
	rando_world_shop_clear(world, RANDO_SHOP_CAPACITY_UPGRADE);
	if (!world->config.shops_hard_mode)
		rando_world_shop_add(world, RANDO_SHOP_CAPACITY_UPGRADE, 0, RANDO_ITEM_BombUpgrade5, 100);
}

void rando_set_shops(RandoWorld *world, RandoPrng *prng)
{
	const RandoConfig *c = &world->config;
	RandoItemId shield = c->item_overflow_replacement_shield;
	size_t j;
	int i;

	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind != RANDO_SHOP_KIND_TAKE_ANY)
			rando_world_shop_set_active(world, (RandoShopId)i, true);

	/* Capacity Upgrade stays active: VT lists it in Shops with no item. */
	if (c->shops_hard_mode)
		rando_world_shop_clear(world, RANDO_SHOP_CAPACITY_UPGRADE);
	if (c->item_overflow_count_shield < 2)
		add_at_default_price(world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, shield);
	if (c->item_overflow_count_shield < 1)
		for (j = 0; j < sizeof shield_shops / sizeof shield_shops[0]; j++)
			add_at_default_price(world, shield_shops[j], 1, shield);

	if (!c->rom_generic_keys && !c->rom_rupee_bow && !c->region_take_anys)
		return;
	assert(prng && "setShops' random path draws");
	if (c->region_take_anys)
		set_take_anys(world, prng);
	set_retro_sellers(world, prng);
	if (c->rom_rupee_bow)
		set_rupee_bow_shops(world);
}

void rando_set_medallions(RandoWorld *world, RandoPrng *prng)
{
	static const RandoItemId medallions[] = {RANDO_ITEM_Ether, RANDO_ITEM_Bombos, RANDO_ITEM_Quake};
	int i;

	/* Region order, Turtle Rock first: baseline's two equal draws would hide a swap. */
	for (i = 0; i < rando_layout_region_location_count(world->variant, RANDO_REGION_MEDALLIONS); i++) {
		RandoLocationId location = rando_layout_region_location(world->variant, RANDO_REGION_MEDALLIONS, i);
		bool placed;

		if (rando_world_has_item(world, location, RANDO_ITEM_NONE))
			continue;
		placed = rando_world_set_item(world, location, medallions[rando_random_int(prng, 0, 2)]);
		assert(placed && "setMedallions: a Medallion location refused a medallion");
		(void)placed;
	}
}

void rando_set_fountains(RandoWorld *world, RandoPrng *prng)
{
	int i;

	for (i = 0; i < rando_layout_region_location_count(world->variant, RANDO_REGION_FOUNTAINS); i++) {
		RandoLocationId location = rando_layout_region_location(world->variant, RANDO_REGION_FOUNTAINS, i);

		if (rando_world_has_item(world, location, RANDO_ITEM_NONE))
			continue;
		place(world, location, rando_get_bottle(world, prng, true));
	}
}

static const uint8_t vanilla_packs[7][RANDO_PRIZE_PACK_SLOTS] = {
	{RANDO_DROP_HEART, RANDO_DROP_HEART, RANDO_DROP_HEART, RANDO_DROP_HEART,
	 RANDO_DROP_RUPEE_GREEN, RANDO_DROP_HEART, RANDO_DROP_HEART, RANDO_DROP_RUPEE_GREEN},
	{RANDO_DROP_RUPEE_BLUE, RANDO_DROP_RUPEE_GREEN, RANDO_DROP_RUPEE_BLUE, RANDO_DROP_RUPEE_RED,
	 RANDO_DROP_RUPEE_BLUE, RANDO_DROP_RUPEE_GREEN, RANDO_DROP_RUPEE_BLUE, RANDO_DROP_RUPEE_BLUE},
	{RANDO_DROP_MAGIC_REFILL_FULL, RANDO_DROP_MAGIC_REFILL_SMALL, RANDO_DROP_MAGIC_REFILL_SMALL, RANDO_DROP_RUPEE_BLUE,
	 RANDO_DROP_MAGIC_REFILL_FULL, RANDO_DROP_MAGIC_REFILL_SMALL, RANDO_DROP_HEART, RANDO_DROP_MAGIC_REFILL_SMALL},
	{RANDO_DROP_BOMB_REFILL_1, RANDO_DROP_BOMB_REFILL_1, RANDO_DROP_BOMB_REFILL_1, RANDO_DROP_BOMB_REFILL_4,
	 RANDO_DROP_BOMB_REFILL_1, RANDO_DROP_BOMB_REFILL_1, RANDO_DROP_BOMB_REFILL_8, RANDO_DROP_BOMB_REFILL_1},
	{RANDO_DROP_ARROW_REFILL_5, RANDO_DROP_HEART, RANDO_DROP_ARROW_REFILL_5, RANDO_DROP_ARROW_REFILL_10,
	 RANDO_DROP_ARROW_REFILL_5, RANDO_DROP_HEART, RANDO_DROP_ARROW_REFILL_5, RANDO_DROP_ARROW_REFILL_10},
	{RANDO_DROP_MAGIC_REFILL_SMALL, RANDO_DROP_RUPEE_GREEN, RANDO_DROP_HEART, RANDO_DROP_ARROW_REFILL_5,
	 RANDO_DROP_MAGIC_REFILL_SMALL, RANDO_DROP_BOMB_REFILL_1, RANDO_DROP_RUPEE_GREEN, RANDO_DROP_HEART},
	{RANDO_DROP_HEART, RANDO_DROP_FAIRY, RANDO_DROP_MAGIC_REFILL_FULL, RANDO_DROP_RUPEE_RED,
	 RANDO_DROP_BOMB_REFILL_8, RANDO_DROP_HEART, RANDO_DROP_RUPEE_RED, RANDO_DROP_ARROW_REFILL_10},
};

void rando_shuffle_prize_packs(RandoWorld *world, RandoPrng *prng)
{
	uint8_t order[7] = {0, 1, 2, 3, 4, 5, 6};
	int k, i;

	if (world->config.custom_prize_packs)
		return;
	rando_fy_shuffle(prng, order, 7, 1);
	for (k = 0; k < 7; k++)
		for (i = 0; i < RANDO_PRIZE_PACK_SLOTS; i++)
			rando_world_set_drop(world, (RandoPrizePack)k, i, (RandoDrop)vanilla_packs[order[k]][i]);
}

typedef struct SideList {
	uint8_t items[RANDO_POOL_CAPACITY];
	uint16_t count;
} SideList;

static void push(uint8_t *items, uint16_t *count, RandoItemId item)
{
	assert(*count < RANDO_POOL_CAPACITY && "prepareWorld: a list outgrew RANDO_POOL_CAPACITY");
	items[(*count)++] = (uint8_t)item;
}

static RandoItemId pop(SideList *side)
{
	assert(side->count > 0);
	return (RandoItemId)side->items[--side->count];
}

static void merge_pool(RandoPools *pools, RandoPoolList list, const SideList *side)
{
	uint16_t i;

	for (i = 0; i < side->count; i++)
		rando_pool_append(pools, list, (RandoItemId)side->items[i]);
}

static RandoItemClass item_class(uint8_t item)
{
	return rando_item_info((RandoItemId)item)->item_class;
}

static const RandoLocationId boss_heart_locations[] = {
	RANDO_LOC_DESERT_PALACE_BOSS, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_LOC_ICE_PALACE_BOSS,
	RANDO_LOC_MISERY_MIRE_BOSS, RANDO_LOC_PALACE_OF_DARKNESS_BOSS, RANDO_LOC_SKULL_WOODS_BOSS,
	RANDO_LOC_SWAMP_PALACE_BOSS, RANDO_LOC_THIEVES_TOWN_BOSS, RANDO_LOC_TURTLE_ROCK_BOSS,
	RANDO_LOC_TOWER_OF_HERA_BOSS,
};

static void prefill_bosses(RandoWorld *world)
{
	const RandoConfig *c = &world->config;
	RandoItemId boss_item;
	size_t i;

	if (c->region_bosses_have_item == RANDO_ITEM_NONE && c->region_boss_hearts_in_pool)
		return;
	boss_item = !c->region_boss_hearts_in_pool ? RANDO_ITEM_BossHeartContainer : c->region_bosses_have_item;
	for (i = 0; i < sizeof boss_heart_locations / sizeof boss_heart_locations[0]; i++)
		place(world, boss_heart_locations[i], boss_item);
}

/* VT's setTarget(array_pop($swords)) on UncleSword, whose C alias target is fixed. */
static RandoItemId pop_uncle_sword(SideList *swords)
{
	RandoItemId target = pop(swords);

	assert(target == rando_item_info(RANDO_ITEM_UncleSword)->alias_target &&
	       "prepareWorld: the uncle's sword is not UncleSword's fixed alias target");
	(void)target;
	return RANDO_ITEM_UncleSword;
}

/* VT's only collectItems before the fill: it fixes the collectable shop cache, so it is neither hoisted
 * nor skipped. */
static void swordless_silvers(RandoWorld *world, RandoPools *pools)
{
	RandoItems world_items, merged;
	uint16_t i;

	rando_world_collect_items(world, NULL, &world_items);
	merged = world_items;
	for (i = 0; i < pools->count[RANDO_POOL_ADVANCEMENT]; i++)
		rando_items_add(&merged, (RandoItemId)pools->items[RANDO_POOL_ADVANCEMENT][i]);
	if (rando_items_has_n(&merged, RANDO_ITEM_ProgressiveBow, 2))
		return;
	if (world_items.values[RANDO_ITEM_SilverArrowUpgrade] || world_items.values[RANDO_ITEM_BowAndSilverArrows])
		return;
	if (rando_pool_find(pools, RANDO_POOL_NICE, RANDO_ITEM_SilverArrowUpgrade) < 0)
		rando_pool_append(pools, RANDO_POOL_ADVANCEMENT, RANDO_ITEM_SilverArrowUpgrade);
}

static void place_weapons(RandoWorld *world, RandoPools *pools, SideList *swords)
{
	const RandoConfig *c = &world->config;
	uint16_t i;

	switch (c->mode_weapons) {
	case RANDO_WEAPONS_SWORDLESS:
		for (i = 0; i < swords->count; i++)
			rando_pool_append(pools, RANDO_POOL_NICE, RANDO_ITEM_TwentyRupees2);
		swordless_silvers(world, pools);
		return;
	case RANDO_WEAPONS_VANILLA:
		assert(swords->count >= 4 && "prepareWorld: vanilla weapons need four swords");
		place(world, RANDO_LOC_LINKS_UNCLE, pop_uncle_sword(swords));
		place(world, RANDO_LOC_PYRAMID_FAIRY_LEFT, pop(swords));
		place(world, RANDO_LOC_BLACKSMITH, pop(swords));
		if (!rando_world_has_item(world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_Triforce)) {
			place(world, RANDO_LOC_MASTER_SWORD_PEDESTAL, pop(swords));
		} else {
			pop(swords);
			rando_pool_append(pools, RANDO_POOL_ITEMS, RANDO_ITEM_TwentyRupees);
		}
		return;
	default:
		break;
	}

	if (swords->count) {
		RandoItemId uncle = pop_uncle_sword(swords);

		if (c->mode_weapons == RANDO_WEAPONS_ASSURED) {
			rando_world_add_pre_collected(world, uncle);
			rando_pool_append(pools, RANDO_POOL_ITEMS, RANDO_ITEM_FiftyRupees);
		} else {
			rando_pool_append(pools, RANDO_POOL_ADVANCEMENT, uncle);
		}
	}
	if (swords->count)
		rando_pool_append(pools, RANDO_POOL_ADVANCEMENT, pop(swords));
	if (c->region_require_better_sword && swords->count)
		rando_pool_append(pools, RANDO_POOL_ADVANCEMENT, pop(swords));
	if (swords->count && c->region_take_anys) {
		pop(swords);
		rando_pool_append(pools, RANDO_POOL_ITEMS, RANDO_ITEM_TwentyRupees);
	}
	merge_pool(pools, RANDO_POOL_NICE, swords);
}

static bool wild_key(const RandoConfig *c, uint8_t item)
{
	return item_class(item) == RANDO_ITEM_CLASS_KEY
	       && (c->mode_state != RANDO_STATE_STANDARD || item != RANDO_ITEM_KeyH2 || c->logic == RANDO_LOGIC_NO_LOGIC);
}

/* Moves every dungeon item of the class (or, for KEY, every wild key) to the advancement list, in order. */
static void release_dungeon_items(const RandoConfig *c, RandoPools *pools, RandoItemClass wanted)
{
	uint8_t *dungeon = pools->items[RANDO_POOL_DUNGEON];
	uint16_t i, keep = 0;

	for (i = 0; i < pools->count[RANDO_POOL_DUNGEON]; i++) {
		bool wild = wanted == RANDO_ITEM_CLASS_KEY ? wild_key(c, dungeon[i]) : item_class(dungeon[i]) == wanted;

		if (wild)
			rando_pool_append(pools, RANDO_POOL_ADVANCEMENT, (RandoItemId)dungeon[i]);
		else
			dungeon[keep++] = dungeon[i];
	}
	pools->count[RANDO_POOL_DUNGEON] = keep;
}

static void force_skull_woods_key(RandoWorld *world, RandoPools *pools)
{
	if (rando_pool_remove_first(pools, RANDO_POOL_DUNGEON, RANDO_ITEM_KeyD3))
		place(world, RANDO_LOC_SKULL_WOODS_PINBALL_ROOM, RANDO_ITEM_KeyD3);
}

static void rupee_bow(RandoPools *pools)
{
	uint8_t *items = pools->items[RANDO_POOL_ITEMS];
	uint16_t i, keep = 0, replaced = 0;

	for (i = 0; i < pools->count[RANDO_POOL_ITEMS]; i++) {
		RandoItemClass cls = item_class(items[i]);

		if (cls == RANDO_ITEM_CLASS_ARROW || cls == RANDO_ITEM_CLASS_UPGRADE_ARROW)
			replaced++;
		else
			items[keep++] = items[i];
	}
	pools->count[RANDO_POOL_ITEMS] = keep;
	while (replaced--)
		rando_pool_append(pools, RANDO_POOL_ITEMS, RANDO_ITEM_FiveRupees);
}

void rando_prepare_pools(RandoWorld *world, RandoPrng *prng, RandoPools *pools)
{
	const RandoConfig *c = &world->config;
	SideList swords = {{0}, 0}, bottles = {{0}, 0}, health = {{0}, 0}, armors = {{0}, 0};
	uint8_t *list;
	uint16_t i, keep;

	place(world, RANDO_LOC_PYRAMID_FAIRY_BOW,
	      c->region_pyramid_bow_upgrade ? RANDO_ITEM_BowAndSilverArrows : RANDO_ITEM_BowAndArrows);
	prefill_bosses(world);

	list = pools->items[RANDO_POOL_ADVANCEMENT];
	for (i = 0, keep = 0; i < pools->count[RANDO_POOL_ADVANCEMENT]; i++) {
		if (list[i] == RANDO_ITEM_SilverArrowUpgrade)
			rando_pool_append(pools, RANDO_POOL_NICE, RANDO_ITEM_SilverArrowUpgrade);
		else if (item_class(list[i]) == RANDO_ITEM_CLASS_SWORD)
			push(swords.items, &swords.count, (RandoItemId)list[i]);
		else if (item_class(list[i]) == RANDO_ITEM_CLASS_BOTTLE)
			push(bottles.items, &bottles.count, (RandoItemId)list[i]);
		else
			list[keep++] = list[i];
	}
	pools->count[RANDO_POOL_ADVANCEMENT] = keep;

	list = pools->items[RANDO_POOL_NICE];
	for (i = 0, keep = 0; i < pools->count[RANDO_POOL_NICE]; i++) {
		RandoItemClass cls = item_class(list[i]);

		if (cls == RANDO_ITEM_CLASS_SWORD)
			push(swords.items, &swords.count, (RandoItemId)list[i]);
		else if (cls == RANDO_ITEM_CLASS_HEALTH)
			push(health.items, &health.count, (RandoItemId)list[i]);
		else if (cls == RANDO_ITEM_CLASS_ARMOR)
			push(armors.items, &armors.count, (RandoItemId)list[i]);
		else
			list[keep++] = list[i];
	}
	pools->count[RANDO_POOL_NICE] = keep;

	list = pools->items[RANDO_POOL_ITEMS];
	for (i = 0, keep = 0; i < pools->count[RANDO_POOL_ITEMS]; i++) {
		if (item_class(list[i]) == RANDO_ITEM_CLASS_HEALTH)
			push(health.items, &health.count, (RandoItemId)list[i]);
		else
			list[keep++] = list[i];
	}
	pools->count[RANDO_POOL_ITEMS] = keep;

	if (c->item_placement == RANDO_PLACEMENT_BASIC) {
		merge_pool(pools, RANDO_POOL_ADVANCEMENT, &health);
		merge_pool(pools, RANDO_POOL_ADVANCEMENT, &armors);
	} else {
		merge_pool(pools, RANDO_POOL_NICE, &health);
		merge_pool(pools, RANDO_POOL_NICE, &armors);
	}

	place_weapons(world, pools, &swords);

	if (bottles.count)
		rando_pool_append(pools, RANDO_POOL_ADVANCEMENT, pop(&bottles));
	merge_pool(pools, RANDO_POOL_NICE, &bottles);

	if (c->rom_rupee_bow)
		rupee_bow(pools);
	if (c->region_force_skull_woods_key)
		force_skull_woods_key(world, pools);
	if (c->region_wild_big_keys)
		release_dungeon_items(c, pools, RANDO_ITEM_CLASS_BIG_KEY);
	if (c->region_wild_keys)
		release_dungeon_items(c, pools, RANDO_ITEM_CLASS_KEY);
	if (c->region_wild_maps)
		release_dungeon_items(c, pools, RANDO_ITEM_CLASS_MAP);
	if (c->region_wild_compasses)
		release_dungeon_items(c, pools, RANDO_ITEM_CLASS_COMPASS);

	rando_fy_shuffle(prng, pools->items[RANDO_POOL_ADVANCEMENT], pools->count[RANDO_POOL_ADVANCEMENT], 1);
	rando_fy_shuffle(prng, pools->items[RANDO_POOL_NICE], pools->count[RANDO_POOL_NICE], 1);
	rando_fy_shuffle(prng, pools->items[RANDO_POOL_ITEMS], pools->count[RANDO_POOL_ITEMS], 1);
}

static const struct {
	RandoRegionId region;
	RandoBossSlot slot;
	RandoBossId boss;
} unshuffled_bosses[] = {
	{RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_DESERT_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_TOWER_OF_HERA, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOLDORM},
	{RANDO_REGION_PALACE_OF_DARKNESS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_HELMASAUR_KING},
	{RANDO_REGION_SWAMP_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARRGHUS},
	{RANDO_REGION_SKULL_WOODS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOTHULA},
	{RANDO_REGION_THIEVES_TOWN, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_BLIND},
	{RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_KHOLDSTARE},
	{RANDO_REGION_MISERY_MIRE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_VITREOUS},
	{RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_TRINEXX},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOTTOM, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, RANDO_BOSS_MOLDORM},
};

void rando_place_bosses(RandoWorld *world)
{
	size_t i;

	if (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS)
		rando_world_set_boss(world, RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_KHOLDSTARE);

	for (i = 0; i < sizeof unshuffled_bosses / sizeof unshuffled_bosses[0]; i++)
		rando_world_set_boss(world, unshuffled_bosses[i].region, unshuffled_bosses[i].slot,
				     unshuffled_bosses[i].boss);

	rando_world_set_boss(world, RANDO_REGION_HYRULE_CASTLE_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM);
	rando_world_set_boss(world, RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM2);
}

#define PRIZE_LOCATIONS 15
#define PRIZES 10
#define PRIZE_PASSES 6

static const RandoItemId prize_items[PRIZES] = {
	RANDO_ITEM_Crystal1, RANDO_ITEM_Crystal2, RANDO_ITEM_Crystal3, RANDO_ITEM_Crystal4, RANDO_ITEM_Crystal5,
	RANDO_ITEM_Crystal6, RANDO_ITEM_Crystal7, RANDO_ITEM_PendantOfCourage, RANDO_ITEM_PendantOfPower,
	RANDO_ITEM_PendantOfWisdom,
};

static void assume_items(RandoWorld *world, RandoPrng *prng, const RandoItemId *prizes, int nprizes,
			 RandoItems *assumed)
{
	RandoItems start;
	int i;

	rando_world_reset_collected(world);
	rando_items_clear(&start);
	rando_pool_add_items(world, prng, RANDO_POOL_DUNGEON, &start);
	rando_pool_add_items(world, prng, RANDO_POOL_ADVANCEMENT, &start);
	for (i = 0; i < nprizes; i++)
		rando_items_add(&start, prizes[i]);
	rando_world_collect_items(world, &start, assumed);
}

static void clear_locations(RandoWorld *world, const RandoLocationId *locations, int n)
{
	int i;

	for (i = 0; i < n; i++)
		rando_world_set_item(world, locations[i], RANDO_ITEM_NONE);
}

/* One phase of fillPrizes over its locations. False when a location stays empty or a win check fails;
 * the phase's locations that were empty on entry are then empty again. */
static bool place_prize_phase(RandoWorld *world, RandoPrng *prng, const RandoLocationId *locations, int nlocations,
			      RandoItemId *prizes, int *nprizes)
{
	RandoLocationId empty[PRIZE_LOCATIONS];
	int nempty = 0, e, i;

	for (i = 0; i < nlocations; i++)
		if (!rando_world_has_item(world, locations[i], RANDO_ITEM_NONE))
			empty[nempty++] = locations[i];

	for (e = 0; e < nempty; e++) {
		RandoItemId prize = RANDO_ITEM_NONE;
		RandoItems assumed;
		int total = *nprizes;
		bool accessible = false;

		/* array_pop takes the candidate from the end; a refused one goes back at the front (array_unshift).
		 * Every candidate makes its four bottle draws before the access check. */
		for (i = 0; i < total; i++) {
			prize = prizes[--*nprizes];
			assume_items(world, prng, prizes, *nprizes, &assumed);
			if (rando_world_can_access(world, empty[e], &assumed)) {
				accessible = true;
				break;
			}
			memmove(prizes + 1, prizes, (size_t)*nprizes * sizeof prizes[0]);
			prizes[0] = prize;
			++*nprizes;
		}
		if (!accessible)
			continue;
		place(world, empty[e], prize);
		/* checkWinCondition collects on top of the candidate's collected locations: no reset here. */
		if (!rando_world_check_win_condition(world, &assumed)) {
			clear_locations(world, empty, nempty);
			return false;
		}
	}

	for (e = 0; e < nempty; e++)
		if (!rando_world_has_item(world, empty[e], RANDO_ITEM_NONE)) {
			clear_locations(world, empty, nempty);
			return false;
		}
	return true;
}

bool rando_fill_prizes(RandoWorld *world, RandoPrng *prng)
{
	int pass;

	for (pass = 0; pass < PRIZE_PASSES; pass++) {
		uint8_t pool[PRIZE_LOCATIONS], order[PRIZE_LOCATIONS];
		RandoLocationId crystals[PRIZE_LOCATIONS], pendants[PRIZE_LOCATIONS];
		RandoItemId prizes[PRIZES];
		int npool = 0, norder, ncrystals = 0, npendants = 0, nprizes = 0, i, j;

		for (i = 0; i < rando_layout_location_count(world->variant); i++) {
			RandoLocationId location = rando_layout_location(world->variant, i);

			if (rando_location_is_a(rando_location_info(location)->location_class, RANDO_LOCATION_CLASS_PRIZE)) {
				assert(npool < PRIZE_LOCATIONS
				       && "fillPrizes: more Prize locations than VT's 14 (Standard, Open) or 15 (Inverted)");
				pool[npool++] = (uint8_t)location;
			}
		}
		norder = rando_random_collection(prng, pool, npool, npool, order);
		for (j = 0; j < norder; j++) {
			RandoLocationId location = (RandoLocationId)order[j];
			RandoLocationClass location_class = rando_location_info(location)->location_class;

			if (location_class == RANDO_LOCATION_CLASS_PRIZE_CRYSTAL)
				crystals[ncrystals++] = location;
			else if (location_class == RANDO_LOCATION_CLASS_PRIZE_PENDANT)
				pendants[npendants++] = location;
		}

		for (j = 0; j < PRIZES; j++) {
			bool placed = false;
			int k;

			for (k = 0; k < norder && !placed; k++)
				placed = rando_world_has_item(world, (RandoLocationId)order[k], prize_items[j]);
			if (!placed)
				prizes[nprizes++] = prize_items[j];
		}
		rando_fy_shuffle(prng, prizes, (size_t)nprizes, sizeof prizes[0]);

		/* crossWorld: the pendant phase takes what the crystal phase left, so pendants land in crystal
		 * locations and crystals in pendant ones. */
		if (place_prize_phase(world, prng, crystals, ncrystals, prizes, &nprizes)
		    && place_prize_phase(world, prng, pendants, npendants, prizes, &nprizes))
			return true;
	}
	return false;
}
