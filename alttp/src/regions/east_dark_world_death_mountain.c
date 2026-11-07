#include "../world_rules.h"

#define FIRST RANDO_LOC_SUPERBUNNY_CAVE_TOP
#define NLOCATIONS (RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_RIGHT - FIRST + 1)

static bool superbunny_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return c->can_super_bunny || has(items, RANDO_ITEM_MoonPearl)
		|| ((c->can_owyba && rando_items_has_a_bottle(items))
		    && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow));
}

static bool hookshot_cave_rest(const RandoConfig *c, const RandoItems *items)
{
	return (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& (rando_items_can_lift_rocks(items) || c->can_one_frame_clip_ow
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)));
}

static bool hookshot_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_Hookshot) && hookshot_cave_rest(&world->config, items);
}

static bool hookshot_cave_bottom_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (has(items, RANDO_ITEM_Hookshot)
		|| (c->item_placement != RANDO_PLACEMENT_BASIC && has(items, RANDO_ITEM_PegasusBoots)))
		&& hookshot_cave_rest(c, items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& ((rando_items_can_lift_dark_rocks(items) && can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items))
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)
			&& (has(items, RANDO_ITEM_MoonPearl) || has(items, RANDO_ITEM_Hammer)
			    || (c->can_owyba && rando_items_has_a_bottle(items))))
		    || c->can_one_frame_clip_ow
		    || (can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)
			&& (c->can_mirror_clip || c->can_mirror_wrap)
			&& has(items, RANDO_ITEM_MagicMirror)));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SUPERBUNNY_CAVE_TOP - FIRST] = {superbunny_cave, NULL, NULL},
	[RANDO_LOC_SUPERBUNNY_CAVE_BOTTOM - FIRST] = {superbunny_cave, NULL, NULL},
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_RIGHT - FIRST] = {hookshot_cave, NULL, NULL},
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_LEFT - FIRST] = {hookshot_cave, NULL, NULL},
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_LEFT - FIRST] = {hookshot_cave, NULL, NULL},
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_RIGHT - FIRST] = {hookshot_cave_bottom_right, NULL, NULL},
};

const RandoRegionRules rando_rules_east_dark_world_death_mountain = {
	"Region\\Standard\\DarkWorld\\DeathMountain\\East", enter, NULL, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
