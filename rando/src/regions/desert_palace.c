#include "../world_rules.h"

#define FIRST RANDO_LOC_DESERT_PALACE_BIG_CHEST
#define NLOCATIONS (RANDO_LOC_DESERT_PALACE_PRIZE - FIRST + 1)

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (has(items, RANDO_ITEM_BookOfMudora)
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || c->can_one_frame_clip_ow
		    || (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_MIRE, items)));
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_BigKeyP2);
}

static bool big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_KeyP2) && rando_items_can_kill_most_things(items, &world->config, 5);
}

static bool compass_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_KeyP2);
}

static bool torch(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PegasusBoots);
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return can_enter(world, locations, RANDO_REGION_DESERT_PALACE, items)
		&& (rando_items_can_lift_rocks(items)
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || (c->can_super_speed && rando_items_can_spin_speed(items))
		    || c->can_one_frame_clip_ow
		    || (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_MIRE, items)))
		&& rando_items_can_light_torches(items)
		&& has(items, RANDO_ITEM_BigKeyP2) && has(items, RANDO_ITEM_KeyP2)
		&& boss_can_beat(world, RANDO_REGION_DESERT_PALACE, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_DESERT_PALACE_BOSS, RANDO_ITEM_CompassP2,
						 RANDO_ITEM_MapP2, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item) && item != RANDO_ITEM_KeyP2 && item != RANDO_ITEM_BigKeyP2;
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassP2, RANDO_ITEM_MapP2);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_DESERT_PALACE_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_DESERT_PALACE_BIG_CHEST - FIRST] = {big_chest, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_TORCH - FIRST] = {torch, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_BIG_KEY_CHEST - FIRST] = {big_key_chest, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_COMPASS_CHEST - FIRST] = {compass_chest, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_DESERT_PALACE_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_desert_palace = {
	"Region\\Standard\\DesertPalace", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_main(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_BookOfMudora) && can_enter(world, locations, RANDO_REGION_SOUTH_LIGHT_WORLD, items);
}

static bool inverted_side(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return c->can_one_frame_clip_ow
		|| (pearl_or_bunny_revive_or_owyba(c, items)
		    && (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    && can_enter(world, locations, RANDO_REGION_SOUTH_LIGHT_WORLD, items));
}

static bool inverted_thieves(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_THIEVES_TOWN, items)
		&& has(items, RANDO_ITEM_KeyD4) && has(items, RANDO_ITEM_BigKeyD4)
		&& world->config.can_one_frame_clip_uw;
}

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->can_dungeon_revive
		|| (c->can_super_bunny && has(items, RANDO_ITEM_MagicMirror))
		|| pearl_or_bunny_revive_or_owyba(c, items))
		&& (inverted_main(world, locations, items) || inverted_side(world, locations, items)
		    || inverted_thieves(world, locations, items));
}

static bool inverted_big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_KeyP2) && rando_items_can_kill_most_things(items, c, 5)
		&& (pearl_or_bunny_revive_or_owyba(c, items) || c->can_dungeon_revive
		    || rando_items_has_sword(items, 1));
}

static bool inverted_boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return can_enter(world, locations, RANDO_REGION_DESERT_PALACE, items)
		&& (pearl_or_bunny_revive_or_owyba(c, items)
		    || (c->can_one_frame_clip_ow && c->can_dungeon_revive))
		&& (rando_items_can_lift_rocks(items)
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || c->can_one_frame_clip_ow)
		&& rando_items_can_light_torches(items)
		&& has(items, RANDO_ITEM_BigKeyP2) && has(items, RANDO_ITEM_KeyP2)
		&& boss_can_beat(world, RANDO_REGION_DESERT_PALACE, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_DESERT_PALACE_BOSS, RANDO_ITEM_CompassP2,
						 RANDO_ITEM_MapP2, items);
}

static const RandoLocationRules inverted_locations[NLOCATIONS] = {
	[RANDO_LOC_DESERT_PALACE_BIG_CHEST - FIRST] = {big_chest, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_TORCH - FIRST] = {torch, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_BIG_KEY_CHEST - FIRST] = {inverted_big_key_chest, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_COMPASS_CHEST - FIRST] = {compass_chest, NULL, NULL},
	[RANDO_LOC_DESERT_PALACE_BOSS - FIRST] = {inverted_boss, boss_fill, boss_always_allow},
	[RANDO_LOC_DESERT_PALACE_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_desert_palace = {
	"Region\\Inverted\\DesertPalace", inverted_enter, complete, inverted_locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
