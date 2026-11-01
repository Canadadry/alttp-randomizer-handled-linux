#include "../world_rules.h"

#define FIRST RANDO_LOC_BREWERY
#define NLOCATIONS (RANDO_LOC_PURPLE_CHEST - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_DARK_WORLD_FOREST_SHOP
#define NSHOPS (RANDO_SHOP_FORTUNE_TELLER_DARK - FIRST_SHOP + 1)

static bool hammer(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_Hammer);
}

static bool brewery(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_bomb_things(items);
}

static bool hammer_pegs(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_Hammer)
		&& (rando_items_can_lift_dark_rocks(items)
		    || (has(items, RANDO_ITEM_MagicMirror)
			&& can_enter(world, locations, RANDO_REGION_NORTH_WEST_LIGHT_WORLD, items))
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || ((c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))
			&& ((c->can_super_speed && rando_items_can_spin_speed(items)) || c->can_one_frame_clip_ow)));
}

static bool bumper_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (rando_items_can_lift_rocks(items) && has(items, RANDO_ITEM_Cape) && has(items, RANDO_ITEM_MoonPearl)
		&& has(items, RANDO_ITEM_MagicMirror)
		&& can_enter(world, locations, RANDO_REGION_NORTH_WEST_LIGHT_WORLD, items))
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow;
}

static bool owyba(const RandoConfig *c, const RandoItems *items)
{
	return c->can_owyba && rando_items_has_a_bottle(items);
}

static bool blacksmith(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (rando_items_can_lift_dark_rocks(items)
		|| has(items, RANDO_ITEM_MagicMirror)
		|| (owyba(c, items)
		    && (c->can_one_frame_clip_ow || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)))))
		&& can_enter(world, locations, RANDO_REGION_NORTH_WEST_LIGHT_WORLD, items);
}

/* As VT parses it: the North West Light World entry binds to the clip branch only. */
static bool purple_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (rando_items_can_lift_dark_rocks(items)
		|| has(items, RANDO_ITEM_MagicMirror)
		|| (owyba(c, items)
		    && ((c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))
			&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow))
		    && can_enter(world, locations, RANDO_REGION_NORTH_WEST_LIGHT_WORLD, items)))
		&& can_enter(world, locations, RANDO_REGION_SOUTH_LIGHT_WORLD, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_BREWERY - FIRST] = {brewery, NULL, NULL},
	[RANDO_LOC_HAMMER_PEGS - FIRST] = {hammer_pegs, NULL, NULL},
	[RANDO_LOC_BUMPER_CAVE - FIRST] = {bumper_cave, NULL, NULL},
	[RANDO_LOC_BLACKSMITH - FIRST] = {blacksmith, NULL, NULL},
	[RANDO_LOC_PURPLE_CHEST - FIRST] = {purple_chest, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP - FIRST_SHOP] = hammer,
};

/* No entry rule: Region\Inverted\DarkWorld\NorthWest sets none. */
const RandoRegionRules rando_rules_inverted_north_west_dark_world = {
	"Region\\Inverted\\DarkWorld\\NorthWest", NULL, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
