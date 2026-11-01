#include "../world_rules.h"

#define FIRST RANDO_LOC_SKULL_WOODS_BIG_CHEST
#define NLOCATIONS (RANDO_LOC_SKULL_WOODS_PRIZE - FIRST + 1)

static bool pinball_room_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return !world->config.region_force_skull_woods_key || item == RANDO_ITEM_KeyD3;
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_BigKeyD3);
}

static bool big_chest_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS && item == RANDO_ITEM_BigKeyD3;
}

static bool big_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_BigKeyD3;
}

static bool bridge_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_FireRod)
		&& (has(items, RANDO_ITEM_MoonPearl)
		    || (world->config.can_dungeon_revive && has(items, RANDO_ITEM_PegasusBoots)
			&& (has(items, RANDO_ITEM_MagicMirror) || rando_items_has_a_bottle(items))));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
			&& rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& (c->can_dungeon_revive || has(items, RANDO_ITEM_MoonPearl)
		    || ((rando_items_has_a_bottle(items) && c->can_owyba)
			|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))))
		&& can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items);
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_SKULL_WOODS, items)
		&& bridge_room(world, locations, items)
		&& (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		&& has_n(items, RANDO_ITEM_KeyD3, 3)
		&& boss_can_beat(world, RANDO_REGION_SKULL_WOODS, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_SKULL_WOODS_BOSS, RANDO_ITEM_CompassD3,
						 RANDO_ITEM_MapD3, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassD3, RANDO_ITEM_MapD3);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_SKULL_WOODS_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SKULL_WOODS_BIG_CHEST - FIRST] = {big_chest, big_chest_fill, big_chest_always_allow},
	[RANDO_LOC_SKULL_WOODS_BRIDGE_ROOM - FIRST] = {bridge_room, NULL, NULL},
	[RANDO_LOC_SKULL_WOODS_PINBALL_ROOM - FIRST] = {NULL, pinball_room_fill, NULL},
	[RANDO_LOC_SKULL_WOODS_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_SKULL_WOODS_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_skull_woods = {
	"Region\\Standard\\SkullWoods", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->item_placement != RANDO_PLACEMENT_BASIC
		|| ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		    && rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items);
}

static bool inverted_bridge_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_FireRod);
}

static bool inverted_boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_SKULL_WOODS, items)
		&& has(items, RANDO_ITEM_FireRod)
		&& (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		&& has_n(items, RANDO_ITEM_KeyD3, 3)
		&& boss_can_beat(world, RANDO_REGION_SKULL_WOODS, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_SKULL_WOODS_BOSS, RANDO_ITEM_CompassD3,
						 RANDO_ITEM_MapD3, items);
}

static const RandoLocationRules inverted_locations[NLOCATIONS] = {
	[RANDO_LOC_SKULL_WOODS_BIG_CHEST - FIRST] = {big_chest, big_chest_fill, big_chest_always_allow},
	[RANDO_LOC_SKULL_WOODS_BRIDGE_ROOM - FIRST] = {inverted_bridge_room, NULL, NULL},
	[RANDO_LOC_SKULL_WOODS_PINBALL_ROOM - FIRST] = {NULL, pinball_room_fill, NULL},
	[RANDO_LOC_SKULL_WOODS_BOSS - FIRST] = {inverted_boss, boss_fill, boss_always_allow},
	[RANDO_LOC_SKULL_WOODS_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_skull_woods = {
	"Region\\Inverted\\SkullWoods", inverted_enter, complete, inverted_locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
