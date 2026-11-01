#include "../world_rules.h"

/* Slots in Region\Inverted\DarkWorld\NorthEast's location order: Ganon left it. */
#define SLOTS(X) X(CATFISH) X(PYRAMID) X(PYRAMID_FAIRY_SWORD) X(PYRAMID_FAIRY_BOW) X(PYRAMID_FAIRY_LEFT) \
	X(PYRAMID_FAIRY_RIGHT)
enum { SLOTS(RANDO_SLOT_ENUM) NLOCATIONS };
#ifdef RANDO_FAULTS
static const uint8_t slot_ids[] = {SLOTS(RANDO_SLOT_ID)};
#endif

#define FIRST_SHOP RANDO_SHOP_DARK_WORLD_POTION_SHOP
#define NSHOPS (RANDO_SHOP_PALACE_OF_DARKNESS_HINT - FIRST_SHOP + 1)

static bool dark_world_potion_shop(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_items_can_fly(items, c)
		|| has(items, RANDO_ITEM_Hammer)
		|| has(items, RANDO_ITEM_Flippers)
		|| (has(items, RANDO_ITEM_MagicMirror)
		    && can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items)
		    && has(items, RANDO_ITEM_MoonPearl))
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (c->can_super_speed && rando_items_can_spin_speed(items))
		|| (c->can_one_frame_clip_ow
		    || (!c->region_cant_take_damage && c->can_fake_flipper)
		    || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c)))
		|| (c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots));
}

static bool catfish(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_items_can_lift_rocks(items)
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow
		|| (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_SOUTH_LIGHT_WORLD, items)
		    && (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		    && (has(items, RANDO_ITEM_Flippers) || c->can_fake_flipper
			|| (c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots))));
}

static bool big_bomb_and_mirror(const RandoItems *items)
{
	return has(items, RANDO_ITEM_BigRedBomb) && has(items, RANDO_ITEM_MagicMirror);
}

static bool pyramid_fairy_sword(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_has_sword(items, 1) && big_bomb_and_mirror(items);
}

static bool pyramid_fairy_bow(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_shoot_arrows(items, &world->config, 1) && big_bomb_and_mirror(items);
}

/* VT sets this requirement in initalize only when region.swordsInPool holds then. */
static bool pyramid_fairy_left_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return !world->config.region_swords_in_pool || big_bomb_and_mirror(items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_items_can_fly(items, c)
		|| has(items, RANDO_ITEM_Hammer)
		|| has(items, RANDO_ITEM_Flippers)
		|| (has(items, RANDO_ITEM_MagicMirror)
		    && can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items))
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots))
		|| (c->can_super_speed && rando_items_can_spin_speed(items))
		|| (!c->region_cant_take_damage && c->can_fake_flipper)
		|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
		|| c->can_one_frame_clip_ow;
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[CATFISH] = {catfish, NULL, NULL},
	[PYRAMID_FAIRY_SWORD] = {pyramid_fairy_sword, NULL, NULL},
	[PYRAMID_FAIRY_BOW] = {pyramid_fairy_bow, NULL, NULL},
	[PYRAMID_FAIRY_LEFT] = {pyramid_fairy_left_right, NULL, NULL},
	[PYRAMID_FAIRY_RIGHT] = {pyramid_fairy_left_right, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_DARK_WORLD_POTION_SHOP - FIRST_SHOP] = dark_world_potion_shop,
};

const RandoRegionRules rando_rules_inverted_north_east_dark_world = {
	"Region\\Inverted\\DarkWorld\\NorthEast", enter, NULL, locations, shops,
	RANDO_SLOT_LIST(slot_ids)
};
