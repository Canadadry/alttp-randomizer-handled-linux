#include "../world_rules.h"

#define FIRST RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST
#define NLOCATIONS (RANDO_LOC_TOWER_OF_HERA_PRIZE - FIRST + 1)

static bool main_entrance(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((has(items, RANDO_ITEM_PegasusBoots) && c->can_boots_clip)
		|| (c->can_super_speed && rando_items_can_spin_speed(items)))
		|| c->can_one_frame_clip_ow
		|| ((has(items, RANDO_ITEM_MagicMirror) || (has(items, RANDO_ITEM_Hookshot) && has(items, RANDO_ITEM_Hammer)))
		    && can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items));
}

static bool mire(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return world->config.can_one_frame_clip_uw
		&& (((rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_COMPASS_CHEST, RANDO_ITEM_BigKeyD6)
		      || rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST, RANDO_ITEM_BigKeyD6))
		     && has_n(items, RANDO_ITEM_KeyD6, 2))
		    || has_n(items, RANDO_ITEM_KeyD6, 3))
		&& can_enter(world, locations, RANDO_REGION_MISERY_MIRE, items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_RescueZelda) && (main_entrance(world, locations, items) || mire(world, locations, items));
}

static bool big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_light_torches(items) && has(items, RANDO_ITEM_KeyP3);
}

static bool big_key_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_KeyP3;
}

static bool big_key_chest_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS && item == RANDO_ITEM_KeyP3;
}

static bool compass_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (main_entrance(world, locations, items) && has(items, RANDO_ITEM_BigKeyP3)) || mire(world, locations, items);
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (main_entrance(world, locations, items) && has(items, RANDO_ITEM_BigKeyP3))
		|| (mire(world, locations, items) && (has(items, RANDO_ITEM_BigKeyP3) || has(items, RANDO_ITEM_BigKeyD6)));
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return main_entrance(world, locations, items)
		&& boss_can_beat(world, RANDO_REGION_TOWER_OF_HERA, items)
		&& (has(items, RANDO_ITEM_BigKeyP3) || (mire(world, locations, items) && has(items, RANDO_ITEM_BigKeyD6)))
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_TOWER_OF_HERA_BOSS, RANDO_ITEM_CompassP3,
						 RANDO_ITEM_MapP3, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassP3, RANDO_ITEM_MapP3);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_TOWER_OF_HERA_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST - FIRST] = {big_key_chest, big_key_chest_fill,
							   big_key_chest_always_allow},
	[RANDO_LOC_TOWER_OF_HERA_COMPASS_CHEST - FIRST] = {compass_chest, NULL, NULL},
	[RANDO_LOC_TOWER_OF_HERA_BIG_CHEST - FIRST] = {big_chest, NULL, NULL},
	[RANDO_LOC_TOWER_OF_HERA_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_TOWER_OF_HERA_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_tower_of_hera = {
	"Region\\Standard\\TowerOfHera", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_main(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items)
		&& has(items, RANDO_ITEM_MoonPearl) && has(items, RANDO_ITEM_Hammer))
		|| (can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)
		    && (((has(items, RANDO_ITEM_MoonPearl)
			  || (c->can_owyba && rando_items_has_bottle(items, 2))
			  || (((c->can_owyba && rando_items_has_a_bottle(items))
			       || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c)))
			      && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)))
			 && (c->can_one_frame_clip_ow
			     || (c->can_super_speed && rando_items_can_spin_speed(items))
			     || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))))
			|| (c->can_one_frame_clip_ow && rando_items_has_sword(items, 1) && c->can_super_bunny
			    && has(items, RANDO_ITEM_MagicMirror))));
}

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return inverted_main(world, locations, items) || mire(world, locations, items);
}

static bool inverted_big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_items_can_light_torches(items)
		&& ((has(items, RANDO_ITEM_KeyP3)
		     && (has(items, RANDO_ITEM_MoonPearl)
			 || (c->can_owyba && rando_items_has_a_bottle(items))
			 || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))))
		    || mire(world, locations, items));
}

static bool inverted_compass_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (inverted_main(world, locations, items) && has(items, RANDO_ITEM_BigKeyP3)) || mire(world, locations, items);
}

static bool inverted_big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (inverted_main(world, locations, items) && has(items, RANDO_ITEM_BigKeyP3))
		|| (mire(world, locations, items) && (has(items, RANDO_ITEM_BigKeyP3) || has(items, RANDO_ITEM_BigKeyD6)));
}

static bool inverted_boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return inverted_main(world, locations, items)
		&& boss_can_beat(world, RANDO_REGION_TOWER_OF_HERA, items)
		&& (has(items, RANDO_ITEM_BigKeyP3) || (mire(world, locations, items) && has(items, RANDO_ITEM_BigKeyD6)))
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_TOWER_OF_HERA_BOSS, RANDO_ITEM_CompassP3,
						 RANDO_ITEM_MapP3, items);
}

static const RandoLocationRules inverted_locations[NLOCATIONS] = {
	[RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST - FIRST] = {inverted_big_key_chest, big_key_chest_fill,
							   big_key_chest_always_allow},
	[RANDO_LOC_TOWER_OF_HERA_COMPASS_CHEST - FIRST] = {inverted_compass_chest, NULL, NULL},
	[RANDO_LOC_TOWER_OF_HERA_BIG_CHEST - FIRST] = {inverted_big_chest, NULL, NULL},
	[RANDO_LOC_TOWER_OF_HERA_BOSS - FIRST] = {inverted_boss, boss_fill, boss_always_allow},
	[RANDO_LOC_TOWER_OF_HERA_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_tower_of_hera = {
	"Region\\Inverted\\TowerOfHera", inverted_enter, complete, inverted_locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
