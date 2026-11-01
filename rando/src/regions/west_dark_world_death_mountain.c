#include "../world_rules.h"

#define FIRST RANDO_LOC_SPIKE_CAVE
#define NLOCATIONS 1
#define FIRST_SHOP RANDO_SHOP_DARK_DEATH_MOUNTAIN_FAIRY
#define NSHOPS 1

static bool dark_death_mountain_fairy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_MoonPearl)
		|| (c->can_owyba && rando_items_has_a_bottle(items)
		    && ((has(items, RANDO_ITEM_PegasusBoots) && c->can_boots_clip) || c->can_one_frame_clip_ow));
}

static bool spike_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (has(items, RANDO_ITEM_MoonPearl)
		|| (c->can_owyba && rando_items_has_a_bottle(items)
		    && ((has(items, RANDO_ITEM_PegasusBoots) && c->can_boots_clip) || c->can_one_frame_clip_ow)
		    && ((has(items, RANDO_ITEM_Cape) && rando_items_can_extend_magic(items, NULL, 3))
			|| ((!c->region_cant_take_damage || rando_items_can_extend_magic(items, NULL, 3))
			    && has(items, RANDO_ITEM_CaneOfByrna)))))
		&& has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items)
		&& ((rando_items_can_extend_magic(items, NULL, 2) && has(items, RANDO_ITEM_Cape))
		    || ((!c->region_cant_take_damage || rando_items_can_extend_magic(items, NULL, 2))
			&& has(items, RANDO_ITEM_CaneOfByrna)));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_RescueZelda) && can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SPIKE_CAVE - FIRST] = {spike_cave, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_DARK_DEATH_MOUNTAIN_FAIRY - FIRST_SHOP] = dark_death_mountain_fairy,
};

const RandoRegionRules rando_rules_west_dark_world_death_mountain = {
	"Region\\Standard\\DarkWorld\\DeathMountain\\West", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
