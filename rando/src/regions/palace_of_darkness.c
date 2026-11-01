#include "../world_rules.h"

#define FIRST RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM
#define NLOCATIONS (RANDO_LOC_PALACE_OF_DARKNESS_PRIZE - FIRST + 1)

static bool arrows(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_shoot_arrows(items, &world->config, 1);
}

static bool keys_or_more(const RandoWorld *world, const RandoWorld *locations,
			 const RandoItems *items, bool lamp, int keys)
{
	bool shortcut = has(items, RANDO_ITEM_Hammer) && arrows(world, locations, items) && (!lamp || has_lamp(world, items));

	return has_n(items, RANDO_ITEM_KeyD1, (shortcut || world->config.region_wild_keys) ? keys + 1 : keys);
}

static bool needs_arrows(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return arrows(world, locations, items);
}

static bool big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	if (rando_world_has_item(locations, RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST, RANDO_ITEM_KeyD1))
		return has(items, RANDO_ITEM_KeyD1);
	return keys_or_more(world, locations, items, true, 5);
}

static bool key_d1_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS && item == RANDO_ITEM_KeyD1
		&& has_n(items, RANDO_ITEM_KeyD1, 5);
}

static bool key_d1_fill_unless_locations(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_KeyD1;
}

static bool no_key_d1_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)world;
	(void)items;
	return item != RANDO_ITEM_KeyD1;
}

static bool key_or_shortcut(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_KeyD1) || (arrows(world, locations, items) && has(items, RANDO_ITEM_Hammer));
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has_lamp(world, items) && has(items, RANDO_ITEM_BigKeyD1) && keys_or_more(world, locations, items, false, 5);
}

static bool compass_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return keys_or_more(world, locations, items, true, 3);
}

static bool harmless_hellway(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	if (rando_world_has_item(locations, RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY, RANDO_ITEM_KeyD1))
		return keys_or_more(world, locations, items, true, 3);
	return keys_or_more(world, locations, items, true, 5);
}

static bool dark_basement(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (has_lamp(world, items)
		|| (world->config.item_placement == RANDO_PLACEMENT_ADVANCED && has(items, RANDO_ITEM_FireRod)))
		&& keys_or_more(world, locations, items, true, 3);
}

static bool dark_maze(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has_lamp(world, items) && keys_or_more(world, locations, items, false, 5);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
			&& rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& (((has(items, RANDO_ITEM_MoonPearl)
		      || ((c->can_owyba && rando_items_has_a_bottle(items))
			  || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))))
		     && can_enter(world, locations, RANDO_REGION_NORTH_EAST_DARK_WORLD, items))
		    || (c->can_one_frame_clip_uw && c->can_dungeon_revive
			&& can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)));
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_PALACE_OF_DARKNESS, items)
		&& boss_can_beat(world, RANDO_REGION_PALACE_OF_DARKNESS, items)
		&& has(items, RANDO_ITEM_Hammer) && has_lamp(world, items) && arrows(world, locations, items)
		&& has(items, RANDO_ITEM_BigKeyD1) && has_n(items, RANDO_ITEM_KeyD1, 6)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_PALACE_OF_DARKNESS_BOSS, RANDO_ITEM_CompassD1,
						 RANDO_ITEM_MapD1, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassD1, RANDO_ITEM_MapD1);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_PALACE_OF_DARKNESS_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST - FIRST] = {big_key_chest, key_d1_fill_unless_locations,
								key_d1_always_allow},
	[RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_LEDGE - FIRST] = {needs_arrows, NULL, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_BRIDGE - FIRST] = {key_or_shortcut, NULL, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_STALFOS_BASEMENT - FIRST] = {key_or_shortcut, NULL, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_MAP_CHEST - FIRST] = {needs_arrows, NULL, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_BIG_CHEST - FIRST] = {big_chest, no_key_d1_fill, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_COMPASS_CHEST - FIRST] = {compass_chest, NULL, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY - FIRST] = {harmless_hellway, key_d1_fill_unless_locations,
								   key_d1_always_allow},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_LEFT - FIRST] = {dark_basement, NULL, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_RIGHT - FIRST] = {dark_basement, NULL, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_TOP - FIRST] = {dark_maze, no_key_d1_fill, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_BOTTOM - FIRST] = {dark_maze, no_key_d1_fill, NULL},
	[RANDO_LOC_PALACE_OF_DARKNESS_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_PALACE_OF_DARKNESS_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_palace_of_darkness = {
	"Region\\Standard\\PalaceOfDarkness", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;
	/* VT tests getRegion('West Death Mountain') itself, an object: always true. */
	const bool west_death_mountain = true;

	return (c->item_placement != RANDO_PLACEMENT_BASIC
		|| ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		    && rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& (can_enter(world, locations, RANDO_REGION_NORTH_EAST_DARK_WORLD, items)
		    || (c->can_one_frame_clip_ow && west_death_mountain));
}

const RandoRegionRules rando_rules_inverted_palace_of_darkness = {
	"Region\\Inverted\\PalaceOfDarkness", inverted_enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
