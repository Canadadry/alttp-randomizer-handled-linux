#include "../world_rules.h"

#define FIRST RANDO_LOC_SUPERBUNNY_CAVE_TOP
#define NLOCATIONS (RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_RIGHT - FIRST + 1)

static bool hookshot_cave_access(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_items_can_lift_rocks(items)
		|| (has(items, RANDO_ITEM_MagicMirror) && rando_items_can_bomb_things(items)
		    && can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items))
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow;
}

static bool hookshot_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_Hookshot) && hookshot_cave_access(world, locations, items);
}

static bool hookshot_cave_bottom_right(const RandoWorld *world, const RandoWorld *locations,
				       const RandoItems *items)
{
	return (has(items, RANDO_ITEM_Hookshot) || has(items, RANDO_ITEM_PegasusBoots))
		&& hookshot_cave_access(world, locations, items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (can_enter(world, locations, RANDO_REGION_WEST_DARK_WORLD_DEATH_MOUNTAIN, items)
		&& (!c->region_cant_take_damage
		    || has(items, RANDO_ITEM_CaneOfByrna)
		    || has(items, RANDO_ITEM_Cape)
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || c->can_one_frame_clip_ow))
		|| (has(items, RANDO_ITEM_MagicMirror) && has(items, RANDO_ITEM_MoonPearl)
		    && has(items, RANDO_ITEM_Hookshot)
		    && can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_RIGHT - FIRST] = {hookshot_cave, NULL, NULL},
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_LEFT - FIRST] = {hookshot_cave, NULL, NULL},
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_LEFT - FIRST] = {hookshot_cave, NULL, NULL},
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_RIGHT - FIRST] = {hookshot_cave_bottom_right, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_east_dark_world_death_mountain = {
	"Region\\Inverted\\DarkWorld\\DeathMountain\\East", enter, NULL, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
