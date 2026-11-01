#include "world_rules.h"

#include <assert.h>
#include <string.h>

typedef char item_id_fits_uint8[RANDO_ITEM_NONE <= UINT8_MAX ? 1 : -1];
typedef char boss_id_fits_uint8[RANDO_BOSS_RULELESS <= UINT8_MAX ? 1 : -1];

static void apply_logic_glitches(RandoConfig *c)
{
	switch (c->logic) {
	case RANDO_LOGIC_MAJOR_GLITCHES:
		c->can_mirror_wrap = true;
		c->can_owyba = true;
		c->can_one_frame_clip_ow = true;
		c->can_transition_wrapped = true;
		/* fall through */
	case RANDO_LOGIC_HYBRID_MAJOR_GLITCHES:
		c->can_one_frame_clip_uw = true;
		/* fall through */
	case RANDO_LOGIC_OVERWORLD_GLITCHES:
		c->can_super_bunny = true;
		c->can_fake_flipper = true;
		c->can_super_speed = true;
		c->can_boots_clip = true;
		c->can_mirror_clip = true;
		c->can_water_walk = true;
		c->can_dungeon_revive = true;
		break;
	case RANDO_LOGIC_NO_GLITCHES:
	case RANDO_LOGIC_NO_LOGIC:
		break;
	}
}

typedef struct ItemCount {
	uint8_t item;
	int16_t count;
} ItemCount;

static const ItemCount crowd_control_counts[] = {
	{RANDO_ITEM_BugCatchingNet, 0}, {RANDO_ITEM_HalfMagic, 0}, {RANDO_ITEM_CaneOfByrna, 0},
	{RANDO_ITEM_Cape, 0},           {RANDO_ITEM_TwentyRupees2, 4},
};

/* overflow_count_shield RANDO_CONFIG_NULL: the key keeps its value. */
typedef struct ItemPoolValues {
	int overflow_count_shield;
	bool shops_hard_mode;
	const ItemCount *counts;
	int ncounts;
} ItemPoolValues;

static const ItemPoolValues item_pool_values[] = {
	[RANDO_ITEM_POOL_NORMAL] = {RANDO_CONFIG_NULL, false, NULL, 0},
	[RANDO_ITEM_POOL_HARD] = {2, true, NULL, 0},
	[RANDO_ITEM_POOL_EXPERT] = {1, true, NULL, 0},
	[RANDO_ITEM_POOL_CROWD_CONTROL] = {0, false, crowd_control_counts,
					   (int)(sizeof crowd_control_counts / sizeof crowd_control_counts[0])},
};

/* RANDO_CONFIG_NULL: normal functionality, nothing changes. */
static const int functionality_bottle_fill_magic[] = {
	[RANDO_ITEM_FUNCTIONALITY_NORMAL] = RANDO_CONFIG_NULL,
	[RANDO_ITEM_FUNCTIONALITY_HARD] = 0x40,
	[RANDO_ITEM_FUNCTIONALITY_EXPERT] = 0x20,
};

/* RANDO_CONFIG_NULL for the goals that are not hunts. */
static const struct {
	int8_t goal_required;
	int8_t triforce_pieces;
} hunt_values[] = {
	[RANDO_GOAL_GANON] = {RANDO_CONFIG_NULL, RANDO_CONFIG_NULL},
	[RANDO_GOAL_FAST_GANON] = {RANDO_CONFIG_NULL, RANDO_CONFIG_NULL},
	[RANDO_GOAL_DUNGEONS] = {RANDO_CONFIG_NULL, RANDO_CONFIG_NULL},
	[RANDO_GOAL_PEDESTAL] = {RANDO_CONFIG_NULL, RANDO_CONFIG_NULL},
	[RANDO_GOAL_TRIFORCE_HUNT] = {20, 30},
	[RANDO_GOAL_GANONHUNT] = {40, 50},
	[RANDO_GOAL_COMPLETIONIST] = {RANDO_CONFIG_NULL, RANDO_CONFIG_NULL},
};

/* World\Retro's counts; VT skips them under difficulty custom, which the C port does not have. */
static void derive_retro_counts(RandoConfig *c)
{
	int16_t *twenty = &c->item_count[RANDO_ITEM_TwentyRupees2];
	int extra = 10;

	c->item_count[RANDO_ITEM_KeyD1] = 0;
	c->item_count[RANDO_ITEM_KeyA2] = 0;
	if (c->item_pool == RANDO_ITEM_POOL_HARD || c->item_pool == RANDO_ITEM_POOL_EXPERT) {
		c->item_count[RANDO_ITEM_KeyD7] = 0;
		c->item_count[RANDO_ITEM_KeyD2] = 0;
		extra = 15;
	}
	*twenty = (int16_t)((*twenty == RANDO_CONFIG_NULL ? 0 : *twenty) + extra);
}

static void derive_config(RandoConfig *c, RandoVariant variant)
{
	static const RandoState states[RANDO_VARIANT_COUNT] = {
		[RANDO_VARIANT_STANDARD] = RANDO_STATE_STANDARD,
		[RANDO_VARIANT_OPEN] = RANDO_STATE_OPEN,
		[RANDO_VARIANT_INVERTED] = RANDO_STATE_INVERTED,
		[RANDO_VARIANT_RETRO] = RANDO_STATE_RETRO,
	};
	const ItemPoolValues *pool;
	bool glitched;
	int i;

	c->mode_state = states[variant];
	if (variant == RANDO_VARIANT_RETRO) {
		c->rom_rupee_bow = true;
		c->rom_generic_keys = true;
		c->region_take_anys = true;
		c->region_wild_keys = true;
	}
	apply_logic_glitches(c);
	switch (c->dungeon_items) {
	case RANDO_DUNGEON_ITEMS_FULL:
		c->region_wild_big_keys = true;
		/* fall through */
	case RANDO_DUNGEON_ITEMS_MCS:
		c->region_wild_keys = true;
		/* fall through */
	case RANDO_DUNGEON_ITEMS_MC:
		c->region_wild_maps = true;
		c->rom_map_on_pickup = true;
		c->region_wild_compasses = true;
		break;
	case RANDO_DUNGEON_ITEMS_STANDARD:
		break;
	}
	glitched = c->logic == RANDO_LOGIC_HYBRID_MAJOR_GLITCHES || c->logic == RANDO_LOGIC_MAJOR_GLITCHES
		|| c->logic == RANDO_LOGIC_NO_LOGIC || c->can_one_frame_clip_uw;
	c->rom_vanilla_keys = c->rom_vanilla_keys || (!c->region_wild_keys && glitched);
	c->rom_vanilla_big_keys = c->rom_vanilla_big_keys || (!c->region_wild_big_keys && glitched);
	c->rom_vanilla_compasses = c->rom_vanilla_compasses || (!c->region_wild_compasses && glitched);
	c->rom_vanilla_maps = c->rom_vanilla_maps || (!c->region_wild_maps && glitched);
	/* TODO: item.pool's overflow counts other than Shield. */
	assert((size_t)c->item_pool < sizeof item_pool_values / sizeof item_pool_values[0]);
	assert((size_t)c->item_functionality
	       < sizeof functionality_bottle_fill_magic / sizeof functionality_bottle_fill_magic[0]);
	assert((size_t)c->goal < sizeof hunt_values / sizeof hunt_values[0]);
	pool = &item_pool_values[c->item_pool];
	if (pool->overflow_count_shield != RANDO_CONFIG_NULL)
		c->item_overflow_count_shield = pool->overflow_count_shield;
	if (pool->shops_hard_mode)
		c->shops_hard_mode = true;
	for (i = 0; i < pool->ncounts; i++)
		c->item_count[pool->counts[i].item] = pool->counts[i].count;
	/* TODO: item.functionality's rom.CapeMagicUsage.*, CaneOfByrnaInvulnerability,
	 * PowderedSpriteFairyPrize, BottleFill.Health, CatchableBees, StunItems, SilversOnlyAtGanon,
	 * NoFarieDrops. */
	if (functionality_bottle_fill_magic[c->item_functionality] != RANDO_CONFIG_NULL) {
		c->rom_bottle_fill_magic = functionality_bottle_fill_magic[c->item_functionality];
		c->rom_catchable_fairies = false;
	}
	c->region_require_better_bow = c->item_placement == RANDO_PLACEMENT_BASIC
		|| c->mode_weapons == RANDO_WEAPONS_SWORDLESS;
	c->region_require_better_sword = c->item_placement == RANDO_PLACEMENT_BASIC;
	if (c->item_placement == RANDO_PLACEMENT_BASIC)
		c->region_force_skull_woods_key = true;
	if (c->item_goal_required == RANDO_CONFIG_NULL)
		c->item_goal_required = hunt_values[c->goal].goal_required;
	if (c->item_count[RANDO_ITEM_TriforcePiece] == RANDO_CONFIG_NULL)
		c->item_count[RANDO_ITEM_TriforcePiece] = hunt_values[c->goal].triforce_pieces;
	if (variant == RANDO_VARIANT_RETRO)
		derive_retro_counts(c);
}

static const struct {
	RandoVariant variant;
	RandoShopId shop;
	uint8_t slot;
	RandoItemId item;
	uint16_t price;
} shop_overrides[] = {
	{RANDO_VARIANT_INVERTED, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 0, RANDO_ITEM_BluePotion, 160},
};

void rando_world_init(RandoWorld *world, RandoVariant variant, const RandoConfig *config)
{
	int i, s;

	assert(variant < RANDO_VARIANT_COUNT);
	memset(world, 0, sizeof *world);
	world->config = *config;
	derive_config(&world->config, variant);
	world->variant = variant;
	memset(world->location_items, RANDO_ITEM_NONE, sizeof world->location_items);
	for (i = 0; i < rando_layout_location_count(variant); i++) {
		RandoLocationId location = rando_layout_location(variant, i);

		world->location_items[location] = (uint8_t)rando_location_info(location)->constructor_item;
	}
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (s = 0; s < RANDO_SHOP_SLOTS; s++) {
			world->shop_items[i][s] = (uint8_t)rando_shop_info((RandoShopId)i)->default_items[s];
			world->shop_prices[i][s] = rando_shop_info((RandoShopId)i)->default_prices[s];
		}
	for (i = 0; i < (int)(sizeof shop_overrides / sizeof shop_overrides[0]); i++)
		if (shop_overrides[i].variant == variant)
			rando_world_shop_add(world, shop_overrides[i].shop, shop_overrides[i].slot, shop_overrides[i].item,
					     shop_overrides[i].price);
	for (i = 0; i < RANDO_REGION_COUNT; i++)
		for (s = 0; s < RANDO_BOSS_SLOT_COUNT; s++)
			world->bosses[i][s] = (uint8_t)rando_region_info((RandoRegionId)i)->default_bosses[s];
	rando_items_clear(&world->pre_collected);
}

void rando_world_add_pre_collected(RandoWorld *world, RandoItemId item)
{
	assert(item < RANDO_ITEM_COUNT);
	assert(world->npre_collected < RANDO_PRE_COLLECTED_MAX && "raise RANDO_PRE_COLLECTED_MAX");
	world->pre_collected_order[world->npre_collected++] = (uint8_t)item;
	rando_items_add(&world->pre_collected, item);
}

int rando_world_pre_collected_count(const RandoWorld *world)
{
	return world->npre_collected;
}

RandoItemId rando_world_pre_collected_item(const RandoWorld *world, int i)
{
	assert(i >= 0 && i < world->npre_collected);
	return (RandoItemId)world->pre_collected_order[i];
}

RandoItemId rando_world_item(const RandoWorld *world, RandoLocationId location)
{
	assert(location < RANDO_LOCATION_COUNT);
	return (RandoItemId)world->location_items[location];
}

RandoBossId rando_world_boss(const RandoWorld *world, RandoRegionId region, RandoBossSlot slot)
{
	assert(region < RANDO_REGION_COUNT && slot < RANDO_BOSS_SLOT_COUNT);
	return (RandoBossId)world->bosses[region][slot];
}

void rando_world_set_boss(RandoWorld *world, RandoRegionId region, RandoBossSlot slot, RandoBossId boss)
{
	assert(region < RANDO_REGION_COUNT && slot < RANDO_BOSS_SLOT_COUNT
	       && (boss < RANDO_BOSS_COUNT || boss == RANDO_BOSS_RULELESS));
	assert((slot == RANDO_BOSS_SLOT_BOSS || region == RANDO_REGION_GANONS_TOWER)
	       && "only Ganon's Tower has boss levels");
	world->bosses[region][slot] = (uint8_t)boss;
}

int rando_prize_pack_slots(RandoPrizePack pack)
{
	static const uint8_t slots[RANDO_PRIZE_PACK_COUNT] = {8, 8, 8, 8, 8, 8, 8, 3, 2, 1, 1};

	assert(pack < RANDO_PRIZE_PACK_COUNT);
	return slots[pack];
}

RandoDrop rando_world_drop(const RandoWorld *world, RandoPrizePack pack, int slot)
{
	assert(slot >= 0 && slot < rando_prize_pack_slots(pack));
	return (RandoDrop)world->prize_pack_drops[pack][slot];
}

void rando_world_set_drop(RandoWorld *world, RandoPrizePack pack, int slot, RandoDrop drop)
{
	assert(slot >= 0 && slot < rando_prize_pack_slots(pack));
	assert(drop > RANDO_DROP_NONE && drop < RANDO_DROP_COUNT && "VT's setDrop takes a Droppable, never null");
	world->prize_pack_drops[pack][slot] = (uint8_t)drop;
}

RandoItemId rando_world_shop_item(const RandoWorld *world, RandoShopId shop, int slot)
{
	assert(shop < RANDO_SHOP_COUNT && slot >= 0 && slot < RANDO_SHOP_SLOTS);
	return (RandoItemId)world->shop_items[shop][slot];
}

bool rando_world_shop_active(const RandoWorld *world, RandoShopId shop)
{
	assert(shop < RANDO_SHOP_COUNT);
	return world->shop_active[shop];
}

void rando_world_shop_set_active(RandoWorld *world, RandoShopId shop, bool active)
{
	assert(shop < RANDO_SHOP_COUNT);
	world->shop_active[shop] = active;
}

int rando_world_shop_price(const RandoWorld *world, RandoShopId shop, int slot)
{
	assert(shop < RANDO_SHOP_COUNT && slot >= 0 && slot < RANDO_SHOP_SLOTS);
	assert(world->shop_items[shop][slot] != RANDO_ITEM_NONE && "an empty shop slot has no price");
	return world->shop_prices[shop][slot];
}

void rando_world_shop_clear(RandoWorld *world, RandoShopId shop)
{
	int s;

	assert(shop < RANDO_SHOP_COUNT);
	for (s = 0; s < RANDO_SHOP_SLOTS; s++) {
		world->shop_items[shop][s] = RANDO_ITEM_NONE;
		world->shop_prices[shop][s] = 0;
	}
}

void rando_world_shop_add(RandoWorld *world, RandoShopId shop, int slot, RandoItemId item, int price)
{
	assert(shop < RANDO_SHOP_COUNT && slot >= 0 && slot < RANDO_SHOP_SLOTS);
	assert(item < RANDO_ITEM_COUNT && price >= 0 && price <= UINT16_MAX);
	world->shop_items[shop][slot] = (uint8_t)item;
	world->shop_prices[shop][slot] = (uint16_t)price;
}

/* Asserts the world has the location. */
static RandoLayoutLocation layout_location(const RandoWorld *world, RandoLocationId location)
{
	RandoLayoutLocation at;

	assert(location < RANDO_LOCATION_COUNT);
	at = rando_layout_locations(world->variant)[location];
	assert(at.region != RANDO_REGION_NONE && "the world does not have this location");
	return at;
}

static bool accepts(RandoLocationClass location_class, RandoItemId item)
{
	RandoItemClass item_class;

	if (item == RANDO_ITEM_NONE)
		return true;
	item_class = rando_item_info(item)->item_class;
	if (location_class == RANDO_LOCATION_CLASS_PRIZE_EVENT)
		return item_class == RANDO_ITEM_CLASS_EVENT;
	if (rando_location_is_a(location_class, RANDO_LOCATION_CLASS_PRIZE))
		return item_class == RANDO_ITEM_CLASS_PENDANT || item_class == RANDO_ITEM_CLASS_CRYSTAL;
	if (location_class == RANDO_LOCATION_CLASS_MEDALLION)
		return item_class == RANDO_ITEM_CLASS_MEDALLION;
	return true;
}

bool rando_world_set_item(RandoWorld *world, RandoLocationId location, RandoItemId item)
{
	RandoLocationClass location_class;

	assert(item <= RANDO_ITEM_NONE);
	(void)layout_location(world, location);
	location_class = rando_location_info(location)->location_class;
	if (!accepts(location_class, item))
		return false;
	/* Pedestal::setItem: a MasterSword placed on the pedestal is stored as L2Sword. */
	if (location_class == RANDO_LOCATION_CLASS_PEDESTAL && item == RANDO_ITEM_MasterSword)
		item = RANDO_ITEM_L2Sword;
	world->location_items[location] = (uint8_t)item;
	return true;
}

bool rando_world_has_item(const RandoWorld *world, RandoLocationId location, RandoItemId item)
{
	RandoItemId held = rando_world_item(world, location);

	return item == RANDO_ITEM_NONE ? held != RANDO_ITEM_NONE : held == item;
}

static bool is_region_item(const RandoRegionInfo *region, RandoItemId item)
{
	int i;

	for (i = 0; i < region->nitems; i++)
		if (region->items[i] == item)
			return true;
	return false;
}

bool rando_world_region_can_fill(const RandoWorld *world, RandoRegionId region, RandoItemId item)
{
	const RandoConfig *c = &world->config;
	RandoItemClass item_class = rando_item_info(item)->item_class;

	if ((rando_dungeon_item_confined(c, item_class)
	     || (item == RANDO_ITEM_KeyH2 && c->mode_state == RANDO_STATE_STANDARD && c->logic != RANDO_LOGIC_NO_LOGIC))
	    && !is_region_item(rando_region_info(region), item))
		return false;
	return true;
}

bool rando_world_pearl_or_bunny_revive_or_owyba(const RandoConfig *c, const RandoItems *items)
{
	return has(items, RANDO_ITEM_MoonPearl) || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
		|| (c->can_owyba && rando_items_has_a_bottle(items));
}

static const RandoRegionRules *region_rules(const RandoWorld *world, RandoRegionId region)
{
	static const RandoRegionRules no_rules = {NULL, NULL, NULL, NULL, NULL, RANDO_SLOT_RANGE(0, 0)};

	return world->config.logic == RANDO_LOGIC_NO_LOGIC ? &no_rules : rando_region_rules(world->variant, region);
}

static const RandoLocationRules *location_rules(const RandoWorld *world, RandoLayoutLocation at)
{
	const RandoRegionRules *rules = region_rules(world, (RandoRegionId)at.region);

	return rules->locations ? &rules->locations[at.slot] : NULL;
}

bool rando_world_region_can_enter_with(const RandoWorld *world, const RandoWorld *locations, RandoRegionId region,
				      const RandoItems *items)
{
	RandoRule rule = region_rules(world, region)->can_enter;

	return !rule || rule(world, locations, items);
}

bool rando_world_region_can_enter(const RandoWorld *world, RandoRegionId region, const RandoItems *items)
{
	return rando_world_region_can_enter_with(world, world, region, items);
}

bool rando_world_region_can_complete(const RandoWorld *world, RandoRegionId region, const RandoItems *items)
{
	RandoRule rule = region_rules(world, region)->can_complete;

	return !rule || rule(world, world, items);
}

bool rando_world_can_access_with(const RandoWorld *world, const RandoWorld *locations, RandoLocationId location,
				 const RandoItems *items)
{
	RandoLayoutLocation at = layout_location(world, location);
	const RandoLocationRules *rules = location_rules(world, at);

	if (!rando_world_region_can_enter_with(world, locations, (RandoRegionId)at.region, items))
		return false;
	return !rules || !rules->requirement || rules->requirement(world, locations, items);
}

bool rando_world_can_access(const RandoWorld *world, RandoLocationId location, const RandoItems *items)
{
	return rando_world_can_access_with(world, world, location, items);
}

bool rando_world_can_fill(RandoWorld *world, RandoLocationId location, RandoItemId item,
			  const RandoItems *items, bool check_access)
{
	RandoLayoutLocation at = layout_location(world, location);
	const RandoLocationRules *rules = location_rules(world, at);
	RandoItemId old_item = rando_world_item(world, location);
	bool fillable;

	assert(item < RANDO_ITEM_COUNT);
	if (!rando_world_set_item(world, location, item))
		assert(!"can_fill with an item VT's setItem would throw on");
	fillable = (rules && rules->always_allow && rules->always_allow(world, item, items))
		|| (rando_world_region_can_fill(world, (RandoRegionId)at.region, item)
		    && (!rules || !rules->fill || rules->fill(world, item, items))
		    && (!check_access || rando_world_can_access(world, location, items)));
	world->location_items[location] = (uint8_t)old_item;
	return fillable;
}

bool rando_world_fill(RandoWorld *world, RandoLocationId location, RandoItemId item, const RandoItems *items)
{
	RandoItemId old_item = rando_world_item(world, location);

	if (!rando_world_set_item(world, location, item))
		assert(!"fill with an item VT's setItem would throw on");
	if (rando_world_can_fill(world, location, item, items, true))
		return true;
	world->location_items[location] = (uint8_t)old_item;
	return false;
}

static void compute_collectable(RandoWorld *world)
{
	int i;

	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);
		RandoLocationClass c = rando_location_info(location)->location_class;

		world->collectable[location] = c != RANDO_LOCATION_CLASS_MEDALLION && c != RANDO_LOCATION_CLASS_FOUNTAIN
			&& !world->collected[location];
	}
	memcpy(world->collectable_shop_items, world->shop_items, sizeof world->shop_items);
	world->collectable_computed = true;
}

bool rando_world_can_access_shop(const RandoWorld *world, RandoShopId shop, const RandoItems *items)
{
	RandoRegionId region = rando_shop_info(shop)->region;
	const RandoRegionRules *rules = region_rules(world, region);
	RandoRule requirement = rules->shops ? rules->shops[shop - rando_region_info(region)->first_shop] : NULL;

	return rando_world_region_can_enter(world, region, items) && (!requirement || requirement(world, world, items));
}

/* found_total may be NULL. */
static void search(RandoWorld *world, RandoItems *my_items, RandoItems *found_total)
{
	bool available[RANDO_LOCATION_COUNT];
	const uint8_t *order = rando_layout_order(world->variant);
	RandoItems found;
	int n = rando_layout_location_count(world->variant), i, s, nfound;

	if (!world->collectable_computed)
		compute_collectable(world);
	for (i = 0; i < n; i++) {
		RandoLocationId l = (RandoLocationId)order[i];

		available[l] = world->collectable[l] && world->location_items[l] != RANDO_ITEM_NONE;
	}
	do {
		rando_items_clear(&found);
		nfound = 0;
		for (i = 0; i < n; i++) {
			RandoLocationId l = (RandoLocationId)order[i];

			if (!available[l] || world->collected[l] || !rando_world_can_access(world, l, my_items))
				continue;
			world->collected[l] = true;
			available[l] = false;
			rando_items_add(&found, (RandoItemId)world->location_items[l]);
			nfound++;
		}
		for (i = 0; i < RANDO_SHOP_COUNT; i++) {
			for (s = 0; s < RANDO_SHOP_SLOTS; s++) {
				if (world->collectable_shop_items[i][s] == RANDO_ITEM_NONE || world->shop_collected[i][s]
				    || !rando_world_can_access_shop(world, (RandoShopId)i, my_items))
					continue;
				world->shop_collected[i][s] = true;
				rando_items_add(&found, (RandoItemId)world->collectable_shop_items[i][s]);
				nfound++;
			}
		}
		rando_items_merge(my_items, my_items, &found);
		if (found_total)
			rando_items_merge(found_total, found_total, &found);
	} while (nfound > 0);
}

void rando_world_collect_items(RandoWorld *world, const RandoItems *start, RandoItems *out)
{
	assert(out != start);
	if (start)
		*out = *start;
	else
		rando_items_clear(out);
	rando_items_merge(out, out, &world->pre_collected);
	search(world, out, NULL);
}

void rando_world_collect_other_items(RandoWorld *world, const RandoItems *collected, RandoItems *found)
{
	RandoItems my_items = *collected;

	assert(found != collected);
	rando_items_clear(found);
	search(world, &my_items, found);
}

bool rando_world_win_condition(const RandoWorld *world, const RandoItems *items)
{
	return rando_items_has(items, RANDO_ITEM_Triforce)
		|| (rando_world_region_can_enter(world, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items)
		    && world->config.goal == RANDO_GOAL_TRIFORCE_HUNT
		    && rando_items_has_n(items, RANDO_ITEM_TriforcePiece, world->config.item_goal_required));
}

bool rando_world_check_win_condition(RandoWorld *world, const RandoItems *collected)
{
	RandoItems items;

	rando_world_collect_items(world, collected, &items);
	return rando_world_win_condition(world, &items);
}

void rando_world_reset_collected(RandoWorld *world)
{
	memset(world->collected, 0, sizeof world->collected);
	memset(world->shop_collected, 0, sizeof world->shop_collected);
}

bool rando_world_collected(const RandoWorld *world, RandoLocationId location)
{
	assert(location < RANDO_LOCATION_COUNT);
	return world->collected[location];
}

bool rando_world_shop_collected(const RandoWorld *world, RandoShopId shop, int slot)
{
	assert(shop < RANDO_SHOP_COUNT && slot >= 0 && slot < RANDO_SHOP_SLOTS);
	return world->shop_collected[shop][slot];
}

int rando_world_collected_count(const RandoWorld *world)
{
	int n = 0, i, s;

	for (i = 0; i < rando_layout_location_count(world->variant); i++)
		n += world->collected[rando_layout_location(world->variant, i)];
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		for (s = 0; s < RANDO_SHOP_SLOTS; s++)
			n += world->shop_collected[i][s];
	return n;
}

void rando_world_collect_until_stable(RandoWorld *world, RandoItems *items)
{
	RandoItems found;
	int found_locations = 0, current;

	rando_world_reset_collected(world);
	for (;;) {
		rando_world_collect_other_items(world, items, &found);
		rando_items_merge(items, items, &found);
		current = rando_world_collected_count(world);
		if (current == found_locations)
			break;
		found_locations = current;
	}
}

bool rando_world_is_winnable(RandoWorld *world)
{
	RandoItems assumed = world->pre_collected;

	rando_world_collect_until_stable(world, &assumed);
	return rando_world_win_condition(world, &assumed);
}

void rando_world_copy(RandoWorld *dst, const RandoWorld *src)
{
	assert(dst != src);
	*dst = *src;
	rando_world_reset_collected(dst);
	dst->collectable_computed = false;
	memset(dst->collectable, 0, sizeof dst->collectable);
	memset(dst->collectable_shop_items, 0, sizeof dst->collectable_shop_items);
}
