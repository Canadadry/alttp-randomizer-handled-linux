#include "../world_rules.h"

#define FIRST RANDO_LOC_SPIKE_CAVE
#define NLOCATIONS 1

static bool spike_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items)
		&& ((rando_items_can_extend_magic(items, NULL, 2) && has(items, RANDO_ITEM_Cape))
		    || ((!world->config.region_cant_take_damage || rando_items_can_extend_magic(items, NULL, 2))
			&& has(items, RANDO_ITEM_CaneOfByrna)));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return rando_items_can_fly(items, c)
		|| (rando_items_can_lift_rocks(items) && has_lamp(world, items))
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| c->can_one_frame_clip_ow;
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SPIKE_CAVE - FIRST] = {spike_cave, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_west_dark_world_death_mountain = {
	"Region\\Inverted\\DarkWorld\\DeathMountain\\West", enter, NULL, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
