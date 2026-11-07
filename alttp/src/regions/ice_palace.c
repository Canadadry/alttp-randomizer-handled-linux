#include "../world_rules.h"

#define FIRST RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST
#define NLOCATIONS (RANDO_LOC_ICE_PALACE_PRIZE - FIRST + 1)

static bool can_take_damage(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return !world->config.region_cant_take_damage || has(items, RANDO_ITEM_CaneOfByrna)
		|| has(items, RANDO_ITEM_Cape) || has(items, RANDO_ITEM_Hookshot);
}

static bool spike_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	int big_keys = rando_world_has_item(locations, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, RANDO_ITEM_BigKeyD5)
		+ rando_world_has_item(locations, RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST, RANDO_ITEM_BigKeyD5)
		+ rando_world_has_item(locations, RANDO_LOC_ICE_PALACE_MAP_CHEST, RANDO_ITEM_BigKeyD5);

	return can_take_damage(world, locations, items)
		&& ((has(items, RANDO_ITEM_Hookshot) || has(items, RANDO_ITEM_ShopKey))
		    || (has(items, RANDO_ITEM_KeyD5) && big_keys >= 1));
}

static bool big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items)
		&& can_take_damage(world, locations, items)
		&& rando_world_can_access(world, RANDO_LOC_ICE_PALACE_SPIKE_ROOM, items);
}

static bool freezor_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_melt_things(items, &world->config);
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_BigKeyD5);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (rando_items_can_melt_things(items, c) || c->can_one_frame_clip_uw)
		&& (((has(items, RANDO_ITEM_MoonPearl) || c->can_dungeon_revive)
		     && (has(items, RANDO_ITEM_Flippers) || c->can_fake_flipper)
		     && rando_items_can_lift_dark_rocks(items))
		    || (can_enter(world, locations, RANDO_REGION_SOUTH_DARK_WORLD, items)
			&& ((pearl_or_bunny_revive_or_owyba(c, items)
			     && ((c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)
				  && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
				      || (c->can_super_speed && rando_items_can_spin_speed(items))))
				 || (has(items, RANDO_ITEM_Flippers) && c->can_transition_wrapped
				     && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
					 || c->can_one_frame_clip_ow))))
			    || (c->can_one_frame_clip_ow
				&& c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)))));
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return can_enter(world, locations, RANDO_REGION_ICE_PALACE, items)
		&& has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items)
		&& boss_can_beat(world, RANDO_REGION_ICE_PALACE, items)
		&& has(items, RANDO_ITEM_BigKeyD5)
		&& ((c->item_placement != RANDO_PLACEMENT_BASIC
		     && ((has(items, RANDO_ITEM_CaneOfSomaria) && has(items, RANDO_ITEM_KeyD5))
			 || has_n(items, RANDO_ITEM_KeyD5, 2)))
		    || (c->item_placement == RANDO_PLACEMENT_BASIC && has_n(items, RANDO_ITEM_KeyD5, 2)))
		&& boss_has_wild_compass_and_map(world, RANDO_LOC_ICE_PALACE_BOSS, RANDO_ITEM_CompassD5,
						 RANDO_ITEM_MapD5, items);
}

static bool boss_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_fill_allows(world, item);
}

static bool boss_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return boss_always_allows(world, item, RANDO_ITEM_CompassD5, RANDO_ITEM_MapD5);
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_ICE_PALACE_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST - FIRST] = {big_key_chest, NULL, NULL},
	[RANDO_LOC_ICE_PALACE_MAP_CHEST - FIRST] = {big_key_chest, NULL, NULL},
	[RANDO_LOC_ICE_PALACE_SPIKE_ROOM - FIRST] = {spike_room, NULL, NULL},
	[RANDO_LOC_ICE_PALACE_FREEZOR_CHEST - FIRST] = {freezor_chest, NULL, NULL},
	[RANDO_LOC_ICE_PALACE_BIG_CHEST - FIRST] = {big_chest, NULL, NULL},
	[RANDO_LOC_ICE_PALACE_BOSS - FIRST] = {boss, boss_fill, boss_always_allow},
	[RANDO_LOC_ICE_PALACE_PRIZE - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_ice_palace = {
	"Region\\Standard\\IcePalace", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->item_placement != RANDO_PLACEMENT_BASIC
		|| ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
		    && rando_items_has_health(items, 12)
		    && (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (rando_items_can_melt_things(items, c) || c->can_one_frame_clip_uw)
		&& ((has(items, RANDO_ITEM_Flippers)
		     || (c->can_fake_flipper
			 && (c->can_bunny_revive
			     || (!c->region_cant_take_damage
				 && can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items))
			     || rando_items_can_fly(items, c)
			     || (can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items)
				 && (has(items, RANDO_ITEM_Hammer) || rando_items_can_lift_rocks(items))))))
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || c->can_one_frame_clip_ow
		    || (c->can_super_speed && rando_items_can_spin_speed(items)));
}

const RandoRegionRules rando_rules_inverted_ice_palace = {
	"Region\\Inverted\\IcePalace", inverted_enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
