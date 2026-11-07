#include "../world_rules.h"

#define FIRST RANDO_LOC_MISERY_MIRE_BIG_CHEST
#define NLOCATIONS (RANDO_LOC_MISERY_MIRE_PRIZE - FIRST + 1)

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_BigKeyD6);
}

static bool spike_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return !world->config.region_cant_take_damage || has(items, RANDO_ITEM_CaneOfByrna)
		|| has(items, RANDO_ITEM_Cape);
}

static bool main_lobby(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_KeyD6) || has(items, RANDO_ITEM_BigKeyD6);
}

static bool torch_rooms(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	return rando_items_can_light_torches(items)
		&& (((rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_COMPASS_CHEST, RANDO_ITEM_BigKeyD6)
		      || rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST, RANDO_ITEM_BigKeyD6))
		     && has_n(items, RANDO_ITEM_KeyD6, 2))
		    || has_n(items, RANDO_ITEM_KeyD6, 3));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (((rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Bombos)
		      && has(items, RANDO_ITEM_Bombos))
		     || (rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether)
			 && has(items, RANDO_ITEM_Ether))
		     || (rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Quake)
			 && has(items, RANDO_ITEM_Quake)))
		    && (c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1)))
		&& (has(items, RANDO_ITEM_MoonPearl)
		    || (rando_items_has_a_bottle(items)
			&& ((has(items, RANDO_ITEM_BugCatchingNet) && c->can_bunny_revive
			     && ((rando_items_can_lift_dark_rocks(items)
				  && (rando_items_can_fly(items, c)
				      || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))))
				 || (c->can_owyba && has(items, RANDO_ITEM_MagicMirror))
				 || c->can_one_frame_clip_ow))
			    || (c->can_owyba
				&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
				    || c->can_one_frame_clip_ow
				    || rando_items_has_bottle(items, 2))))))
		&& ((c->item_placement != RANDO_PLACEMENT_BASIC && has(items, RANDO_ITEM_PegasusBoots))
		    || has(items, RANDO_ITEM_Hookshot))
		&& rando_items_can_kill_most_things(items, c, 8)
		&& can_enter(world, locations, RANDO_REGION_MIRE, items);
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_MISERY_MIRE, items)
		&& has(items, RANDO_ITEM_CaneOfSomaria) && has_lamp(world, items)
		&& has(items, RANDO_ITEM_BigKeyD6)
		&& boss_can_beat(world, RANDO_REGION_MISERY_MIRE, items)
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_MISERY_MIRE_BOSS, RANDO_ITEM_CompassD6,
						 RANDO_ITEM_MapD6, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassD6, RANDO_ITEM_MapD6);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_MISERY_MIRE_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_MISERY_MIRE_BIG_CHEST - FIRST] = {big_chest, NULL, NULL},
	[RANDO_LOC_MISERY_MIRE_MAIN_LOBBY - FIRST] = {main_lobby, NULL, NULL},
	[RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST - FIRST] = {torch_rooms, NULL, NULL},
	[RANDO_LOC_MISERY_MIRE_COMPASS_CHEST - FIRST] = {torch_rooms, NULL, NULL},
	[RANDO_LOC_MISERY_MIRE_MAP_CHEST - FIRST] = {main_lobby, NULL, NULL},
	[RANDO_LOC_MISERY_MIRE_SPIKE_CHEST - FIRST] = {spike_chest, NULL, NULL},
	[RANDO_LOC_MISERY_MIRE_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_MISERY_MIRE_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_misery_mire = {
	"Region\\Standard\\MiseryMire", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->item_placement != RANDO_PLACEMENT_BASIC
		|| ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
		    && rando_items_has_health(items, 12)
		    && (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& ((rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Bombos)
		     && has(items, RANDO_ITEM_Bombos))
		    || (rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether)
			&& has(items, RANDO_ITEM_Ether))
		    || (rando_world_has_item(locations, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Quake)
			&& has(items, RANDO_ITEM_Quake)))
		&& (c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		&& (has(items, RANDO_ITEM_PegasusBoots) || has(items, RANDO_ITEM_Hookshot))
		&& rando_items_can_kill_most_things(items, c, 8)
		&& can_enter(world, locations, RANDO_REGION_MIRE, items);
}

const RandoRegionRules rando_rules_inverted_misery_mire = {
	"Region\\Inverted\\MiseryMire", inverted_enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
