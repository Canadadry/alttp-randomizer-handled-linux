#include "../world_rules.h"

#define FIRST RANDO_LOC_GANONS_TOWER_BOBS_TORCH
#define NLOCATIONS (RANDO_LOC_AGAHNIM_2 - FIRST + 1)

static int crystals(const RandoItems *items)
{
	return has(items, RANDO_ITEM_Crystal1) + has(items, RANDO_ITEM_Crystal2) + has(items, RANDO_ITEM_Crystal3)
		+ has(items, RANDO_ITEM_Crystal4) + has(items, RANDO_ITEM_Crystal5) + has(items, RANDO_ITEM_Crystal6)
		+ has(items, RANDO_ITEM_Crystal7);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->item_placement != RANDO_PLACEMENT_BASIC
		    || ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
			&& rando_items_has_health(items, 12)
			&& (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (((has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		     && ((crystals(items) >= c->crystals_tower
			  && can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items))
			 || (((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			      || (c->can_super_speed && has(items, RANDO_ITEM_PegasusBoots)
				  && has(items, RANDO_ITEM_Hookshot)))
			     && can_enter(world, locations, RANDO_REGION_WEST_DARK_WORLD_DEATH_MOUNTAIN, items))))
		    || (c->can_one_frame_clip_ow
			&& (c->can_dungeon_revive || has(items, RANDO_ITEM_MoonPearl)
			    || (c->can_owyba && rando_items_has_a_bottle(items)))));
}

static bool beats(const RandoWorld *world, RandoBossSlot slot, const RandoItems *items)
{
	return rando_boss_can_beat(rando_world_boss(world, RANDO_REGION_GANONS_TOWER, slot), &world->config, items);
}

static bool big_key_in(const RandoWorld *locations, const RandoLocationId *chests, int n)
{
	int count = 1;

	for (int i = 0; i < n; i++)
		if (rando_world_has_item(locations, chests[i], RANDO_ITEM_BigKeyA2))
			count--;
	return count < 1;
}

static bool hammer_and_hookshot(const RandoItems *items)
{
	return has(items, RANDO_ITEM_Hammer) && has(items, RANDO_ITEM_Hookshot);
}

static bool fire_rod_and_somaria(const RandoItems *items)
{
	return has(items, RANDO_ITEM_FireRod) && has(items, RANDO_ITEM_CaneOfSomaria);
}

static bool bobs_torch(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PegasusBoots);
}

static bool dms_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return hammer_and_hookshot(items);
}

static bool randomizer_room(const RandoWorld *world, const RandoWorld *locations,
			    const RandoItems *items, RandoLocationId self)
{
	static const RandoLocationId room[] = {
		RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT,
		RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT,
	};
	RandoLocationId others[3];
	int n = 0;

	(void)world;
	for (int i = 0; i < 4; i++)
		if (room[i] != self)
			others[n++] = room[i];
	return hammer_and_hookshot(items)
		&& ((big_key_in(locations, others, n) && has_n(items, RANDO_ITEM_KeyA2, 3)) || has_n(items, RANDO_ITEM_KeyA2, 4));
}

static bool randomizer_room_top_left(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return randomizer_room(world, locations, items, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT);
}

static bool randomizer_room_top_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return randomizer_room(world, locations, items, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT);
}

static bool randomizer_room_bottom_left(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return randomizer_room(world, locations, items, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT);
}

static bool randomizer_room_bottom_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return randomizer_room(world, locations, items, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT);
}

static bool firesnake_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	static const RandoLocationId room[] = {
		RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT,
		RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT, RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT,
	};

	(void)world;
	return hammer_and_hookshot(items)
		&& (((big_key_in(locations, room, 4)
		      || rando_world_has_item(locations, RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM, RANDO_ITEM_KeyA2))
		     && has_n(items, RANDO_ITEM_KeyA2, 2))
		    || has_n(items, RANDO_ITEM_KeyA2, 3));
}

static bool map_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_Hammer)
		&& (has(items, RANDO_ITEM_Hookshot)
		    || (world->config.item_placement != RANDO_PLACEMENT_BASIC && has(items, RANDO_ITEM_PegasusBoots)))
		&& ((rando_world_has_item(locations, RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_ITEM_BigKeyA2)
		     || rando_world_has_item(locations, RANDO_LOC_GANONS_TOWER_MAP_CHEST, RANDO_ITEM_KeyA2))
			    ? has_n(items, RANDO_ITEM_KeyA2, 3)
			    : has_n(items, RANDO_ITEM_KeyA2, 4));
}

static bool map_chest_always_allow(const RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS && item == RANDO_ITEM_KeyA2
		&& has_n(items, RANDO_ITEM_KeyA2, 3);
}

static bool map_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return world->config.accessibility != RANDO_ACCESSIBILITY_LOCATIONS || item != RANDO_ITEM_KeyA2;
}

static bool big_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_BigKeyA2) && has_n(items, RANDO_ITEM_KeyA2, 3)
		&& (hammer_and_hookshot(items) || fire_rod_and_somaria(items));
}

static bool not_big_key_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)world;
	(void)items;
	return item != RANDO_ITEM_BigKeyA2;
}

static bool bobs_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return (hammer_and_hookshot(items) || fire_rod_and_somaria(items))
		&& has_n(items, RANDO_ITEM_KeyA2, 3)
		&& (world->config.item_placement != RANDO_PLACEMENT_BASIC
		    || (has(items, RANDO_ITEM_FireRod)
			|| (has(items, RANDO_ITEM_Ether) && rando_items_has_sword(items, 1))));
}

static bool tile_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_CaneOfSomaria);
}

static bool compass_room(const RandoWorld *world, const RandoWorld *locations,
			 const RandoItems *items, RandoLocationId self)
{
	static const RandoLocationId room[] = {
		RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT, RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_RIGHT,
		RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_LEFT, RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT,
	};
	RandoLocationId others[3];
	int n = 0;

	(void)world;
	for (int i = 0; i < 4; i++)
		if (room[i] != self)
			others[n++] = room[i];
	return fire_rod_and_somaria(items)
		&& ((big_key_in(locations, others, n) && has_n(items, RANDO_ITEM_KeyA2, 3)) || has_n(items, RANDO_ITEM_KeyA2, 4));
}

static bool compass_room_top_left(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return compass_room(world, locations, items, RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT);
}

static bool compass_room_top_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return compass_room(world, locations, items, RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_RIGHT);
}

static bool compass_room_bottom_left(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return compass_room(world, locations, items, RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_LEFT);
}

static bool compass_room_bottom_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return compass_room(world, locations, items, RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT);
}

static bool big_key_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return (hammer_and_hookshot(items) || fire_rod_and_somaria(items))
		&& has_n(items, RANDO_ITEM_KeyA2, 3)
		&& beats(world, RANDO_BOSS_SLOT_BOTTOM, items);
}

static bool middle_floor(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_shoot_arrows(items, &world->config, 1) && rando_items_can_light_torches(items)
		&& has(items, RANDO_ITEM_BigKeyA2) && has_n(items, RANDO_ITEM_KeyA2, 3)
		&& beats(world, RANDO_BOSS_SLOT_MIDDLE, items);
}

static bool moldorm_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_Hookshot)
		&& rando_items_can_shoot_arrows(items, &world->config, 1) && rando_items_can_light_torches(items)
		&& has(items, RANDO_ITEM_BigKeyA2) && has_n(items, RANDO_ITEM_KeyA2, 4)
		&& beats(world, RANDO_BOSS_SLOT_MIDDLE, items)
		&& beats(world, RANDO_BOSS_SLOT_TOP, items);
}

static bool moldorm_chest_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)world;
	(void)items;
	return item != RANDO_ITEM_KeyA2 && item != RANDO_ITEM_BigKeyA2;
}

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_GANONS_TOWER, items)
		&& rando_world_can_access(world, RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST, items)
		&& beats(world, RANDO_BOSS_SLOT_BOSS, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_GANONS_TOWER_BOBS_TORCH - FIRST] = {bobs_torch, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT - FIRST] = {dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_RIGHT - FIRST] = {dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_LEFT - FIRST] = {dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_RIGHT - FIRST] = {dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT - FIRST] = {randomizer_room_top_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT - FIRST] = {randomizer_room_top_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT - FIRST] = {randomizer_room_bottom_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT - FIRST] = {randomizer_room_bottom_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM - FIRST] = {firesnake_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_MAP_CHEST - FIRST] = {map_chest, map_chest_fill, map_chest_always_allow},
	[RANDO_LOC_GANONS_TOWER_BIG_CHEST - FIRST] = {big_chest, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_BOBS_CHEST - FIRST] = {bobs_chest, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_TILE_ROOM - FIRST] = {tile_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT - FIRST] = {compass_room_top_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_RIGHT - FIRST] = {compass_room_top_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_LEFT - FIRST] = {compass_room_bottom_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT - FIRST] = {compass_room_bottom_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_CHEST - FIRST] = {big_key_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_LEFT - FIRST] = {big_key_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_RIGHT - FIRST] = {big_key_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_LEFT - FIRST] = {middle_floor, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_RIGHT - FIRST] = {middle_floor, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_PRE_MOLDORM_CHEST - FIRST] = {middle_floor, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST - FIRST] = {moldorm_chest, moldorm_chest_fill, NULL},
	[RANDO_LOC_AGAHNIM_2 - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_ganons_tower = {
	"Region\\Standard\\GanonsTower", enter, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->item_placement != RANDO_PLACEMENT_BASIC
		|| ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 2))
		    && rando_items_has_health(items, 12)
		    && (rando_items_has_bottle(items, 2) || rando_items_has_armor(items, 1))))
		&& (c->can_dungeon_revive
		    || (c->can_super_bunny && has(items, RANDO_ITEM_MagicMirror))
		    || has(items, RANDO_ITEM_MoonPearl)
		    || ((c->can_one_frame_clip_ow || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)))
			&& ((c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
			    || (c->can_owyba && rando_items_has_a_bottle(items)))))
		&& crystals(items) >= c->crystals_tower
		&& can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items);
}

static bool not_bunny(const RandoWorld *world, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_MoonPearl) || c->can_dungeon_revive
		|| ((c->can_one_frame_clip_ow || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)))
		    && ((c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
			|| (c->can_owyba && rando_items_has_a_bottle(items))));
}

#define INVERTED(rule) \
	static bool inverted_##rule(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items) \
	{ \
		return rule(world, locations, items) && not_bunny(world, items); \
	}

INVERTED(dms_room)
INVERTED(randomizer_room_top_left)
INVERTED(randomizer_room_top_right)
INVERTED(randomizer_room_bottom_left)
INVERTED(randomizer_room_bottom_right)
INVERTED(firesnake_room)
INVERTED(map_chest)
INVERTED(big_chest)
INVERTED(tile_room)
INVERTED(compass_room_top_left)
INVERTED(compass_room_top_right)
INVERTED(compass_room_bottom_left)
INVERTED(compass_room_bottom_right)
INVERTED(big_key_room)
INVERTED(middle_floor)
INVERTED(moldorm_chest)

/* VT's Inverted Bob's Chest drops Standard's "basic" Fire Rod or Ether clause. */
static bool inverted_bobs_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return (hammer_and_hookshot(items) || fire_rod_and_somaria(items)) && has_n(items, RANDO_ITEM_KeyA2, 3)
		&& not_bunny(world, items);
}

static const RandoLocationRules inverted_locations[NLOCATIONS] = {
	[RANDO_LOC_GANONS_TOWER_BOBS_TORCH - FIRST] = {bobs_torch, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT - FIRST] = {inverted_dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_RIGHT - FIRST] = {inverted_dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_LEFT - FIRST] = {inverted_dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_RIGHT - FIRST] = {inverted_dms_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT - FIRST] = {inverted_randomizer_room_top_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT - FIRST] = {inverted_randomizer_room_top_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT - FIRST] = {inverted_randomizer_room_bottom_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT - FIRST] = {inverted_randomizer_room_bottom_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM - FIRST] = {inverted_firesnake_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_MAP_CHEST - FIRST] = {inverted_map_chest, map_chest_fill, map_chest_always_allow},
	[RANDO_LOC_GANONS_TOWER_BIG_CHEST - FIRST] = {inverted_big_chest, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_BOBS_CHEST - FIRST] = {inverted_bobs_chest, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_TILE_ROOM - FIRST] = {inverted_tile_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT - FIRST] = {inverted_compass_room_top_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_RIGHT - FIRST] = {inverted_compass_room_top_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_LEFT - FIRST] = {inverted_compass_room_bottom_left, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT - FIRST] = {inverted_compass_room_bottom_right, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_CHEST - FIRST] = {inverted_big_key_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_LEFT - FIRST] = {inverted_big_key_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_RIGHT - FIRST] = {inverted_big_key_room, NULL, NULL},
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_LEFT - FIRST] = {inverted_middle_floor, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_RIGHT - FIRST] = {inverted_middle_floor, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_PRE_MOLDORM_CHEST - FIRST] = {inverted_middle_floor, not_big_key_fill, NULL},
	[RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST - FIRST] = {inverted_moldorm_chest, moldorm_chest_fill, NULL},
	[RANDO_LOC_AGAHNIM_2 - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_ganons_tower = {
	"Region\\Inverted\\GanonsTower", inverted_enter, complete, inverted_locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
