#include "../world_rules.h"

#define FIRST RANDO_LOC_SPIRAL_CAVE
#define NLOCATIONS (RANDO_LOC_FLOATING_ISLAND - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_LIGHT_WORLD_DEATH_MOUNTAIN_SHOP
#define NSHOPS (RANDO_SHOP_HOOKSHOT_FAIRY - FIRST_SHOP + 1)

static bool light_world_death_mountain_shop(const RandoWorld *world, const RandoWorld *locations,
					    const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_bomb_things(items);
}

static bool mimic_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_Hammer) && has(items, RANDO_ITEM_MagicMirror)
		&& (c->can_mirror_clip
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)
			&& (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && has(items, RANDO_ITEM_Bottle))))
		    || (c->can_super_speed && rando_items_can_spin_speed(items) && has(items, RANDO_ITEM_MoonPearl)
			&& can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items))
		    || c->can_one_frame_clip_ow
		    || (has_n(items, RANDO_ITEM_KeyD7, 2) && can_enter(world, locations, RANDO_REGION_TURTLE_ROCK, items)));
}

static bool floating_island(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (c->can_owyba && has(items, RANDO_ITEM_Bottle))
		|| c->can_one_frame_clip_ow
		|| (has(items, RANDO_ITEM_MagicMirror)
		    && ((has(items, RANDO_ITEM_MoonPearl) && rando_items_can_bomb_things(items)
			 && rando_items_can_lift_rocks(items))
			|| c->can_mirror_wrap)
		    && can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->can_one_frame_clip_ow
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || (c->can_super_speed && rando_items_can_spin_speed(items))
		    || ((((c->can_mirror_clip || c->can_mirror_wrap) && has(items, RANDO_ITEM_MagicMirror))
			 || has(items, RANDO_ITEM_Hookshot))
			&& can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items))
		    || (has(items, RANDO_ITEM_Hammer) && can_enter(world, locations, RANDO_REGION_TOWER_OF_HERA, items)));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_MIMIC_CAVE - FIRST] = {mimic_cave, NULL, NULL},
	[RANDO_LOC_FLOATING_ISLAND - FIRST] = {floating_island, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_LIGHT_WORLD_DEATH_MOUNTAIN_SHOP - FIRST_SHOP] = light_world_death_mountain_shop,
};

const RandoRegionRules rando_rules_east_death_mountain = {
	"Region\\Standard\\LightWorld\\DeathMountain\\East", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
