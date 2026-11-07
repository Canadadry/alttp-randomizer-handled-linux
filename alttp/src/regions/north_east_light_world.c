#include "../world_rules.h"

#define FIRST RANDO_LOC_SAHASRAHLAS_HUT_LEFT
#define NLOCATIONS (RANDO_LOC_WATERFALL_FAIRY_RIGHT - FIRST + 1)

static bool sahasrahla(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PendantOfCourage);
}

static bool king_zora(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return c->can_fake_flipper
		|| ((c->can_water_walk || c->can_boots_clip) && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow
		|| rando_items_can_lift_rocks(items) || has(items, RANDO_ITEM_Flippers);
}

static bool potion_shop(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_Mushroom);
}

static bool zoras_ledge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_Flippers)
		|| (has(items, RANDO_ITEM_PegasusBoots)
		    && (c->can_water_walk
			&& (c->can_fake_flipper
			    || (c->can_super_speed && rando_items_can_spin_speed(items))
			    || c->can_boots_clip
			    || c->can_one_frame_clip_ow)));
}

static bool waterfall_fairy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_Flippers)
		|| (c->can_water_walk && (has(items, RANDO_ITEM_PegasusBoots)
					  || (has(items, RANDO_ITEM_MoonPearl) && c->can_fake_flipper)));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_RescueZelda);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SAHASRAHLA - FIRST] = {sahasrahla, NULL, NULL},
	[RANDO_LOC_KING_ZORA - FIRST] = {king_zora, NULL, NULL},
	[RANDO_LOC_POTION_SHOP - FIRST] = {potion_shop, NULL, NULL},
	[RANDO_LOC_ZORAS_LEDGE - FIRST] = {zoras_ledge, NULL, NULL},
	[RANDO_LOC_WATERFALL_FAIRY_LEFT - FIRST] = {waterfall_fairy, NULL, NULL},
	[RANDO_LOC_WATERFALL_FAIRY_RIGHT - FIRST] = {waterfall_fairy, NULL, NULL},
};

const RandoRegionRules rando_rules_north_east_light_world = {
	"Region\\Standard\\LightWorld\\NorthEast", enter, NULL, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
