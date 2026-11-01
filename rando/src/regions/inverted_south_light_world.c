#include "../world_rules.h"

/* Slots in Region\Inverted\LightWorld\South's location order: Link's House left it, the Bomb Merchant came last. */
#define SLOTS(X) X(FLOODGATE_CHEST) X(AGINAHS_CAVE) X(MINI_MOLDORM_CAVE_FAR_LEFT) X(MINI_MOLDORM_CAVE_LEFT) \
	X(MINI_MOLDORM_CAVE_RIGHT) X(MINI_MOLDORM_CAVE_FAR_RIGHT) X(ICE_ROD_CAVE) X(HOBO) X(BOMBOS_TABLET) \
	X(CAVE_45) X(CHECKERBOARD_CAVE) X(MINI_MOLDORM_CAVE_NPC) X(LIBRARY) X(MAZE_RACE) X(DESERT_LEDGE) \
	X(LAKE_HYLIA_ISLAND) X(SUNKEN_TREASURE) X(FLUTE_SPOT) X(BOMB_MERCHANT)
enum { SLOTS(RANDO_SLOT_ENUM) NLOCATIONS };
#ifdef RANDO_FAULTS
static const uint8_t slot_ids[] = {SLOTS(RANDO_SLOT_ID)};
#endif

#define FIRST_SHOP RANDO_SHOP_LIGHT_WORLD_LAKE_HYLIA_SHOP
#define NSHOPS (RANDO_SHOP_KAKARIKO_GAMBLE_GAME - FIRST_SHOP + 1)

static bool link_state(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool link_state_or_super_bunny(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return link_state(world, locations, items)
		|| (world->config.can_super_bunny && has(items, RANDO_ITEM_MagicMirror));
}

static bool lift_rocks(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return link_state(world, locations, items) && rando_items_can_lift_rocks(items);
}

static bool bonk_fairy_light(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return link_state(world, locations, items) && has(items, RANDO_ITEM_PegasusBoots);
}

static bool bombs(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return link_state(world, locations, items) && rando_items_can_bomb_things(items);
}

static bool swim(const RandoConfig *c, const RandoItems *items)
{
	return (c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))
		|| (c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots));
}

static bool capacity_upgrade(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return ((has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& swim(c, items))
		|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c));
}

static bool bomb_merchant(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return (has(items, RANDO_ITEM_MoonPearl) || world->config.can_super_bunny)
		&& has(items, RANDO_ITEM_Crystal5) && has(items, RANDO_ITEM_Crystal6);
}

static bool mini_moldorm_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return bombs(world, locations, items) && rando_items_can_kill_most_things(items, &world->config, 5);
}

static bool ice_rod_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return bombs(world, locations, items)
		|| (has(items, RANDO_ITEM_BigRedBomb) && world->config.can_super_bunny
		    && has(items, RANDO_ITEM_MagicMirror));
}

static bool hobo(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return link_state(world, locations, items) && swim(&world->config, items);
}

static bool bombos_tablet(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_BookOfMudora)
		&& (rando_items_has_sword(items, 2)
		    || (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS && has(items, RANDO_ITEM_Hammer)));
}

static bool library(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return link_state_or_super_bunny(world, locations, items) && has(items, RANDO_ITEM_PegasusBoots);
}

static bool maze_race(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return (has(items, RANDO_ITEM_MoonPearl)
		&& (has(items, RANDO_ITEM_PegasusBoots) || rando_items_can_bomb_things(items)))
		|| world->config.can_one_frame_clip_ow;
}

static bool desert_ledge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((has(items, RANDO_ITEM_MoonPearl) || c->can_dungeon_revive
		 || (c->can_super_bunny && has(items, RANDO_ITEM_MagicMirror))
		 || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& can_enter(world, locations, RANDO_REGION_DESERT_PALACE, items))
		|| c->can_one_frame_clip_ow
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots));
}

static bool lake_hylia_island(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (link_state(world, locations, items)
		&& (has(items, RANDO_ITEM_Flippers)
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || (c->can_super_speed && rando_items_can_spin_speed(items))))
		|| c->can_one_frame_clip_ow;
}

static bool flute_spot(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return link_state(world, locations, items) && has(items, RANDO_ITEM_Shovel);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[FLOODGATE_CHEST] = {link_state_or_super_bunny, NULL, NULL},
	[AGINAHS_CAVE] = {bombs, NULL, NULL},
	[MINI_MOLDORM_CAVE_FAR_LEFT] = {mini_moldorm_cave, NULL, NULL},
	[MINI_MOLDORM_CAVE_LEFT] = {mini_moldorm_cave, NULL, NULL},
	[MINI_MOLDORM_CAVE_RIGHT] = {mini_moldorm_cave, NULL, NULL},
	[MINI_MOLDORM_CAVE_FAR_RIGHT] = {mini_moldorm_cave, NULL, NULL},
	[ICE_ROD_CAVE] = {ice_rod_cave, NULL, NULL},
	[HOBO] = {hobo, NULL, NULL},
	[BOMBOS_TABLET] = {bombos_tablet, NULL, NULL},
	[CAVE_45] = {link_state_or_super_bunny, NULL, NULL},
	[CHECKERBOARD_CAVE] = {lift_rocks, NULL, NULL},
	[MINI_MOLDORM_CAVE_NPC] = {mini_moldorm_cave, NULL, NULL},
	[LIBRARY] = {library, NULL, NULL},
	[MAZE_RACE] = {maze_race, NULL, NULL},
	[DESERT_LEDGE] = {desert_ledge, NULL, NULL},
	[LAKE_HYLIA_ISLAND] = {lake_hylia_island, NULL, NULL},
	[SUNKEN_TREASURE] = {link_state_or_super_bunny, NULL, NULL},
	[FLUTE_SPOT] = {flute_spot, NULL, NULL},
	[BOMB_MERCHANT] = {bomb_merchant, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_CAPACITY_UPGRADE - FIRST_SHOP] = capacity_upgrade,
	[RANDO_SHOP_20_RUPEE_CAVE - FIRST_SHOP] = lift_rocks,
	[RANDO_SHOP_50_RUPEE_CAVE - FIRST_SHOP] = lift_rocks,
	[RANDO_SHOP_BONK_FAIRY_LIGHT - FIRST_SHOP] = bonk_fairy_light,
	[RANDO_SHOP_LIGHT_HYPE_FAIRY - FIRST_SHOP] = bombs,
};

const RandoRegionRules rando_rules_inverted_south_light_world = {
	"Region\\Inverted\\LightWorld\\South", enter, NULL, locations, shops,
	RANDO_SLOT_LIST(slot_ids)
};
