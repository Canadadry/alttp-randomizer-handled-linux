#include "../world_rules.h"

#define FIRST RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS
#define NLOCATIONS (RANDO_LOC_TURTLE_ROCK_PRIZE - FIRST + 1)

static bool upper(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Bombos)
		 && has(items, RANDO_ITEM_Bombos))
		|| (rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Ether)
		    && has(items, RANDO_ITEM_Ether))
		|| (rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake)
		    && has(items, RANDO_ITEM_Quake)))
		&& (c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1))
		&& (has(items, RANDO_ITEM_MoonPearl)
		    || (c->can_owyba && rando_items_has_a_bottle(items)
			&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)))
		&& has(items, RANDO_ITEM_CaneOfSomaria)
		&& ((has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_dark_rocks(items)
		     && can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items))
		    || ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow));
}

static bool middle(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (((c->can_mirror_clip && has(items, RANDO_ITEM_MagicMirror))
		 && (has(items, RANDO_ITEM_MoonPearl) || c->can_dungeon_revive))
		|| ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    && ((c->can_owyba && rando_items_has_a_bottle(items)) || has(items, RANDO_ITEM_MoonPearl)))
		|| (c->can_super_speed && has(items, RANDO_ITEM_MoonPearl) && rando_items_can_spin_speed(items))
		|| (c->can_one_frame_clip_ow
		    && (c->can_dungeon_revive || has(items, RANDO_ITEM_MoonPearl)
			|| (c->can_owyba && rando_items_has_a_bottle(items)))))
		&& (has(items, RANDO_ITEM_PegasusBoots) || has(items, RANDO_ITEM_CaneOfSomaria)
		    || has(items, RANDO_ITEM_Hookshot) || !c->region_cant_take_damage
		    || has(items, RANDO_ITEM_Cape) || has(items, RANDO_ITEM_CaneOfByrna))
		&& can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items);
}

static bool lower(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)
		&& (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& ((((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)
		     && can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items))
		    || ((c->can_super_speed && rando_items_can_spin_speed(items))
			&& can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items)));
}

static bool chain_chomps(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (upper(world, locations, items) && has(items, RANDO_ITEM_KeyD7))
		|| middle(world, locations, items)
		|| (lower(world, locations, items) && has_lamp(world, items) && has(items, RANDO_ITEM_CaneOfSomaria));
}

static bool big_key_in(const RandoWorld *locations, RandoLocationId a, RandoLocationId b)
{
	return rando_world_has_item(locations, a, RANDO_ITEM_BigKeyD7)
		+ rando_world_has_item(locations, b, RANDO_ITEM_BigKeyD7) >= 1;
}

static bool roller_or_compass(const RandoWorld *world, const RandoWorld *locations,
			      const RandoItems *items, RandoLocationId other_a, RandoLocationId other_b)
{
	return upper(world, locations, items)
		|| (middle(world, locations, items)
		    && ((big_key_in(locations, other_a, other_b) && has_n(items, RANDO_ITEM_KeyD7, 2))
			|| has_n(items, RANDO_ITEM_KeyD7, 4)))
		|| (lower(world, locations, items) && has_lamp(world, items) && has_n(items, RANDO_ITEM_KeyD7, 4));
}

static bool roller_room_left(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_FireRod) && has(items, RANDO_ITEM_CaneOfSomaria)
		&& roller_or_compass(world, locations, items, RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_RIGHT,
				     RANDO_LOC_TURTLE_ROCK_COMPASS_CHEST);
}

static bool roller_room_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_FireRod) && has(items, RANDO_ITEM_CaneOfSomaria)
		&& roller_or_compass(world, locations, items, RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_LEFT,
				     RANDO_LOC_TURTLE_ROCK_COMPASS_CHEST);
}

static bool compass_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_CaneOfSomaria)
		&& roller_or_compass(world, locations, items, RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_LEFT,
				     RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_RIGHT);
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_BigKeyD7)
		&& ((upper(world, locations, items) && has_n(items, RANDO_ITEM_KeyD7, 2))
		    || (middle(world, locations, items) && (has(items, RANDO_ITEM_Hookshot) || has(items, RANDO_ITEM_CaneOfSomaria)))
		    || (lower(world, locations, items) && has_lamp(world, items) && has(items, RANDO_ITEM_CaneOfSomaria)));
}

static bool big_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)world;
	(void)items;
	return item != RANDO_ITEM_BigKeyD7;
}

static bool big_key_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	if (!rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_BigKeyD7)
	    && world->config.region_wild_keys)
		return rando_world_has_item(locations, RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST, RANDO_ITEM_KeyD7)
			? has_n(items, RANDO_ITEM_KeyD7, 3)
			: has_n(items, RANDO_ITEM_KeyD7, 4);
	return has_n(items, RANDO_ITEM_KeyD7, 2);
}

static bool big_key_chest_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS && item == RANDO_ITEM_KeyD7
		&& has_n(items, RANDO_ITEM_KeyD7, 3);
}

static bool big_key_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_KeyD7;
}

static bool crystaroller_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (has(items, RANDO_ITEM_BigKeyD7)
		&& ((upper(world, locations, items) && has_n(items, RANDO_ITEM_KeyD7, 2)) || middle(world, locations, items)))
		|| (lower(world, locations, items) && has_lamp(world, items) && has(items, RANDO_ITEM_CaneOfSomaria));
}

static bool eye_bridge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (lower(world, locations, items)
		|| ((upper(world, locations, items) || middle(world, locations, items))
		    && has_lamp(world, items) && has(items, RANDO_ITEM_CaneOfSomaria)
		    && has(items, RANDO_ITEM_BigKeyD7) && has_n(items, RANDO_ITEM_KeyD7, 3)))
		&& (c->item_placement != RANDO_PLACEMENT_BASIC || has(items, RANDO_ITEM_Cape)
		    || has(items, RANDO_ITEM_CaneOfByrna)
		    || (c->item_overflow_count_shield >= 3 && rando_items_can_block_lasers(items)));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (lower(world, locations, items) || middle(world, locations, items) || upper(world, locations, items));
}

static bool boss(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return can_enter(world, locations, RANDO_REGION_TURTLE_ROCK, items)
		&& has_n(items, RANDO_ITEM_KeyD7, 4)
		&& ((c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)
		     && (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		     && ((((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)
			  && can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items))
			 || ((c->can_super_speed && rando_items_can_spin_speed(items))
			     && can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items))))
		    || has_lamp(world, items))
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

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS - FIRST] = {chain_chomps, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_COMPASS_CHEST - FIRST] = {compass_chest, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_LEFT - FIRST] = {roller_room_left, NULL, NULL},
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_RIGHT - FIRST] = {roller_room_right, NULL, NULL},
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

const RandoRegionRules rando_rules_turtle_rock = {
	"Region\\Standard\\TurtleRock", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
