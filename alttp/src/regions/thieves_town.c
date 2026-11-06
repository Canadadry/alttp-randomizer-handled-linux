#include "../world_rules.h"

#define FIRST RANDO_LOC_THIEVES_TOWN_ATTIC
#define NLOCATIONS (RANDO_LOC_THIEVES_TOWN_PRIZE - FIRST + 1)

static bool attic(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_KeyD4) && has(items, RANDO_ITEM_BigKeyD4);
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	if (rando_world_has_item(locations, RANDO_LOC_THIEVES_TOWN_BIG_CHEST, RANDO_ITEM_KeyD4))
		return has(items, RANDO_ITEM_Hammer) && has(items, RANDO_ITEM_BigKeyD4);
	return has(items, RANDO_ITEM_Hammer) && has(items, RANDO_ITEM_KeyD4) && has(items, RANDO_ITEM_BigKeyD4);
}

static bool big_chest_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS && item == RANDO_ITEM_KeyD4
		&& has(items, RANDO_ITEM_Hammer);
}

static bool big_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_KeyD4;
}

static bool blinds_cell(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_BigKeyD4);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
			&& rando_items_has_health(items, 7) && rando_items_has_a_bottle(items)))
		&& (has(items, RANDO_ITEM_MoonPearl)
		    || (rando_items_has_a_bottle(items) && c->can_owyba)
		    || (c->can_bunny_revive && rando_items_can_spin_speed(items)))
		&& can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items);
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_THIEVES_TOWN, items)
		&& attic(world, locations, items)
		&& boss_can_beat(world, RANDO_REGION_THIEVES_TOWN, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_THIEVES_TOWN_BOSS, RANDO_ITEM_CompassD4,
						 RANDO_ITEM_MapD4, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassD4, RANDO_ITEM_MapD4);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_THIEVES_TOWN_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_THIEVES_TOWN_ATTIC - FIRST] = {attic, NULL, NULL},
	[RANDO_LOC_THIEVES_TOWN_BIG_CHEST - FIRST] = {big_chest, big_chest_fill, big_chest_always_allow},
	[RANDO_LOC_THIEVES_TOWN_BLINDS_CELL - FIRST] = {blinds_cell, NULL, NULL},
	[RANDO_LOC_THIEVES_TOWN_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_THIEVES_TOWN_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_thieves_town = {
	"Region\\Standard\\ThievesTown", enter, complete, locations, NULL,
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

const RandoRegionRules rando_rules_inverted_thieves_town = {
	"Region\\Inverted\\ThievesTown", inverted_enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
