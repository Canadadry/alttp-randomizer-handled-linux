#include "../world_rules.h"

#define FIRST RANDO_LOC_MIRE_SHED_LEFT
#define NLOCATIONS (RANDO_LOC_MIRE_SHED_RIGHT - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_DARK_DESERT_FAIRY
#define NSHOPS (RANDO_SHOP_DARK_DESERT_HINT - FIRST_SHOP + 1)

static bool dark_desert(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_MoonPearl)
		|| ((c->can_owyba && rando_items_has_a_bottle(items))
		    && (c->can_one_frame_clip_ow || rando_items_has_bottle(items, 2)
			|| (has(items, RANDO_ITEM_MagicMirror) && has(items, RANDO_ITEM_BugCatchingNet) && c->can_bunny_revive)
			|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))));
}

static bool mire_shed(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return dark_desert(world, locations, items)
		|| (world->config.can_super_bunny && has(items, RANDO_ITEM_MagicMirror));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& ((rando_items_can_lift_dark_rocks(items)
		     && (rando_items_can_fly(items, c) || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))))
		    || c->can_one_frame_clip_ow
		    || ((((has(items, RANDO_ITEM_MoonPearl) || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c)))
			  && (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)))
			 || (c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)))
			&& can_enter(world, locations, RANDO_REGION_SOUTH_DARK_WORLD, items))
		    || (c->can_owyba && rando_items_has_a_bottle(items)));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_MIRE_SHED_LEFT - FIRST] = {mire_shed, NULL, NULL},
	[RANDO_LOC_MIRE_SHED_RIGHT - FIRST] = {mire_shed, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_DARK_DESERT_FAIRY - FIRST_SHOP] = dark_desert,
	[RANDO_SHOP_DARK_DESERT_HINT - FIRST_SHOP] = dark_desert,
};

const RandoRegionRules rando_rules_mire = {
	"Region\\Standard\\DarkWorld\\Mire", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
