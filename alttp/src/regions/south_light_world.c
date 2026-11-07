#include "../world_rules.h"

#define FIRST RANDO_LOC_FLOODGATE_CHEST
#define NLOCATIONS (RANDO_LOC_FLUTE_SPOT - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_LIGHT_WORLD_LAKE_HYLIA_SHOP
#define NSHOPS (RANDO_SHOP_KAKARIKO_GAMBLE_GAME - FIRST_SHOP + 1)

static bool rupee_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_lift_rocks(items);
}

static bool bonk_fairy_light(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PegasusBoots);
}

static bool light_hype_fairy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_bomb_things(items);
}

static bool capacity_upgrade(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_fake_flipper || has(items, RANDO_ITEM_Flippers);
}

static bool aginahs_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_bomb_things(items);
}

static bool mini_moldorm_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_bomb_things(items) && rando_items_can_kill_most_things(items, &world->config, 5);
}

static bool hobo(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_fake_flipper || has(items, RANDO_ITEM_Flippers);
}

static bool bombos_tablet(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_BookOfMudora)
		&& (rando_items_has_sword(items, 2)
		    || (c->mode_weapons == RANDO_WEAPONS_SWORDLESS && has(items, RANDO_ITEM_Hammer)))
		&& (c->can_one_frame_clip_ow
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_SOUTH_DARK_WORLD, items)));
}

static bool cave_45(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return c->can_one_frame_clip_ow
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_SOUTH_DARK_WORLD, items));
}

static bool checkerboard_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_items_can_lift_rocks(items)
		&& (c->can_one_frame_clip_ow
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_MIRE, items)));
}

static bool library(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PegasusBoots);
}

static bool desert_ledge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_DESERT_PALACE, items);
}

static bool lake_hylia_island(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return c->can_one_frame_clip_ow
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (has(items, RANDO_ITEM_MagicMirror) && c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots)
		    && (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		    && can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items))
		|| (has(items, RANDO_ITEM_Flippers) && has(items, RANDO_ITEM_MagicMirror)
		    && (c->can_bunny_surf
			|| has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		    && can_enter(world, locations, RANDO_REGION_NORTH_EAST_DARK_WORLD, items));
}

static bool flute_spot(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_Shovel);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_RescueZelda);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_AGINAHS_CAVE - FIRST] = {aginahs_cave, NULL, NULL},
	[RANDO_LOC_MINI_MOLDORM_CAVE_FAR_LEFT - FIRST] = {mini_moldorm_cave, NULL, NULL},
	[RANDO_LOC_MINI_MOLDORM_CAVE_LEFT - FIRST] = {mini_moldorm_cave, NULL, NULL},
	[RANDO_LOC_MINI_MOLDORM_CAVE_RIGHT - FIRST] = {mini_moldorm_cave, NULL, NULL},
	[RANDO_LOC_MINI_MOLDORM_CAVE_FAR_RIGHT - FIRST] = {mini_moldorm_cave, NULL, NULL},
	[RANDO_LOC_MINI_MOLDORM_CAVE_NPC - FIRST] = {mini_moldorm_cave, NULL, NULL},
	[RANDO_LOC_HOBO - FIRST] = {hobo, NULL, NULL},
	[RANDO_LOC_BOMBOS_TABLET - FIRST] = {bombos_tablet, NULL, NULL},
	[RANDO_LOC_CAVE_45 - FIRST] = {cave_45, NULL, NULL},
	[RANDO_LOC_CHECKERBOARD_CAVE - FIRST] = {checkerboard_cave, NULL, NULL},
	[RANDO_LOC_LIBRARY - FIRST] = {library, NULL, NULL},
	[RANDO_LOC_DESERT_LEDGE - FIRST] = {desert_ledge, NULL, NULL},
	[RANDO_LOC_LAKE_HYLIA_ISLAND - FIRST] = {lake_hylia_island, NULL, NULL},
	[RANDO_LOC_FLUTE_SPOT - FIRST] = {flute_spot, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_CAPACITY_UPGRADE - FIRST_SHOP] = capacity_upgrade,
	[RANDO_SHOP_20_RUPEE_CAVE - FIRST_SHOP] = rupee_cave,
	[RANDO_SHOP_50_RUPEE_CAVE - FIRST_SHOP] = rupee_cave,
	[RANDO_SHOP_BONK_FAIRY_LIGHT - FIRST_SHOP] = bonk_fairy_light,
	[RANDO_SHOP_LIGHT_HYPE_FAIRY - FIRST_SHOP] = light_hype_fairy,
};

const RandoRegionRules rando_rules_south_light_world = {
	"Region\\Standard\\LightWorld\\South", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
