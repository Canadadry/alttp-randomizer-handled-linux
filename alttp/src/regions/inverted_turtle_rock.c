#include "../world_rules.h"

#define FIRST RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS
#define NLOCATIONS (RANDO_LOC_TURTLE_ROCK_PRIZE - FIRST + 1)

/* VT's turtlerock.wastekey, read nowhere else and never set: always false. */
#define WASTE_KEY false

static int account_for_wasting_key_on_trinex_door(int without_waste, int with_waste)
{
	return WASTE_KEY ? without_waste : with_waste;
}

static bool enter_top(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Bombos)
		 && has(items, RANDO_ITEM_Bombos))
		|| (rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Ether)
		    && has(items, RANDO_ITEM_Ether))
		|| (rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake)
		    && has(items, RANDO_ITEM_Quake)))
		&& (c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		&& has(items, RANDO_ITEM_CaneOfSomaria)
		&& can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items);
}

static bool enter_middle(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items) && has(items, RANDO_ITEM_MagicMirror))
		|| (can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items)
		    && world->config.can_super_speed && rando_items_can_spin_speed(items));
}

static bool enter_bottom(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items);
}

static bool can_reach_top(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_CaneOfSomaria)
		&& (enter_top(world, locations, items)
		    || (enter_middle(world, locations, items) && has_n(items, RANDO_ITEM_KeyD7, 4))
		    || (enter_bottom(world, locations, items) && has_lamp(world, items)
			&& has_n(items, RANDO_ITEM_KeyD7, 4)));
}

static bool can_reach_middle(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return enter_middle(world, locations, items)
		|| (enter_top(world, locations, items)
		    && has_n(items, RANDO_ITEM_KeyD7, account_for_wasting_key_on_trinex_door(2, 3)))
		|| (enter_bottom(world, locations, items) && has_lamp(world, items)
		    && has(items, RANDO_ITEM_CaneOfSomaria));
}

static bool can_reach_bottom(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return enter_bottom(world, locations, items)
		|| ((enter_top(world, locations, items) || enter_middle(world, locations, items))
		    && has_lamp(world, items) && has(items, RANDO_ITEM_CaneOfSomaria)
		    && has(items, RANDO_ITEM_BigKeyD7) && has_n(items, RANDO_ITEM_KeyD7, 3));
}

static bool chain_chomps(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (enter_top(world, locations, items) && has(items, RANDO_ITEM_CaneOfSomaria)
		&& has_n(items, RANDO_ITEM_KeyD7, account_for_wasting_key_on_trinex_door(1, 2)))
		|| enter_middle(world, locations, items);
}

static bool roller_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_FireRod) && can_reach_top(world, locations, items);
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_BigKeyD7) && can_reach_middle(world, locations, items)
		&& (has(items, RANDO_ITEM_Hookshot) || has(items, RANDO_ITEM_CaneOfSomaria));
}

static bool big_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)world;
	(void)items;
	return item != RANDO_ITEM_BigKeyD7;
}

static bool big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_reach_middle(world, locations, items)
		&& (has_n(items, RANDO_ITEM_KeyD7, 4)
		    || rando_world_has_item(world, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_KeyD7));
}

static bool big_key_chest_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)world;
	(void)items;
	return item == RANDO_ITEM_KeyD7;
}

static bool big_key_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_KeyD7;
}

static bool crystaroller_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (has(items, RANDO_ITEM_BigKeyD7) && can_reach_middle(world, locations, items))
		|| (enter_bottom(world, locations, items) && has_lamp(world, items)
		    && has(items, RANDO_ITEM_CaneOfSomaria));
}

static bool eye_bridge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return can_reach_bottom(world, locations, items)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC || has(items, RANDO_ITEM_Cape)
		    || has(items, RANDO_ITEM_CaneOfByrna)
		    || (c->item_overflow_count_shield >= 3 && rando_items_can_block_lasers(items)));
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_reach_bottom(world, locations, items)
		&& has_n(items, RANDO_ITEM_KeyD7, 4)
		&& has(items, RANDO_ITEM_BigKeyD7) && has(items, RANDO_ITEM_CaneOfSomaria)
		&& boss_can_beat(world, RANDO_REGION_TURTLE_ROCK, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_TURTLE_ROCK_BOSS, RANDO_ITEM_CompassD7,
						 RANDO_ITEM_MapD7, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassD7, RANDO_ITEM_MapD7);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_TURTLE_ROCK_BOSS, items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->item_placement != RANDO_PLACEMENT_BASIC
		|| ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
		    && rando_items_has_health(items, 12)
		    && (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (enter_top(world, locations, items) || enter_middle(world, locations, items)
		    || enter_bottom(world, locations, items));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS - FIRST] = {chain_chomps, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_COMPASS_CHEST - FIRST] = {can_reach_top, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_LEFT - FIRST] = {roller_room, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_RIGHT - FIRST] = {roller_room, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_BIG_CHEST - FIRST] = {big_chest, big_chest_fill, NULL},
	[RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST - FIRST] = {big_key_chest, big_key_chest_fill,
							 big_key_chest_always_allow},
	[RANDO_LOC_TURTLE_ROCK_CRYSTAROLLER_ROOM - FIRST] = {crystaroller_room, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_LEFT - FIRST] = {eye_bridge, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_RIGHT - FIRST] = {eye_bridge, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT - FIRST] = {eye_bridge, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_RIGHT - FIRST] = {eye_bridge, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_TURTLE_ROCK_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_turtle_rock = {
	"Region\\Inverted\\TurtleRock", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
