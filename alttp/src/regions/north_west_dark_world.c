#include "../world_rules.h"

#define FIRST RANDO_LOC_BREWERY
#define NLOCATIONS (RANDO_LOC_PURPLE_CHEST - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_DARK_WORLD_FOREST_SHOP
#define NSHOPS (RANDO_SHOP_FORTUNE_TELLER_DARK - FIRST_SHOP + 1)

static bool dark_world_outcasts_shop(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_Hammer) && pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool brewery(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_bomb_things(items) && pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool c_shaped_house(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_MoonPearl) || c->can_super_bunny
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c));
}

static bool hammer_pegs(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_Hammer) && pearl_or_bunny_revive_or_owyba(c, items)
		&& (rando_items_can_lift_dark_rocks(items)
		    || (has(items, RANDO_ITEM_MagicMirror) && c->can_mirror_wrap)
		    || ((c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))
			&& (c->can_one_frame_clip_ow
			    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			    || (c->can_super_speed && rando_items_can_spin_speed(items)))));
}

static bool bumper_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return c->can_one_frame_clip_ow
		|| (pearl_or_bunny_revive_or_owyba(c, items)
		    && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			|| ((c->item_placement != RANDO_PLACEMENT_BASIC || has(items, RANDO_ITEM_Hookshot))
			    && rando_items_can_lift_rocks(items) && has(items, RANDO_ITEM_Cape))));
}

static bool blacksmith(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (c->item_placement != RANDO_PLACEMENT_BASIC || has(items, RANDO_ITEM_MagicMirror))
		&& ((pearl_or_bunny_revive_or_owyba(c, items) && rando_items_can_lift_dark_rocks(items))
		    || ((c->can_owyba && rando_items_has_a_bottle(items))
			&& (c->can_one_frame_clip_ow
			    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)
				&& (has(items, RANDO_ITEM_MoonPearl) || rando_items_has_bottle(items, 2))))));
}

static bool purple_chest(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_world_can_access(locations, RANDO_LOC_BLACKSMITH, items)
		&& ((has(items, RANDO_ITEM_MagicMirror) && c->can_mirror_wrap)
		    || (pearl_or_bunny_revive_or_owyba(c, items)
			&& (rando_items_can_lift_dark_rocks(items)
			    || ((c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))
				&& (c->can_one_frame_clip_ow
				    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
				    || (c->can_super_speed && rando_items_can_spin_speed(items)))))));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& (c->can_one_frame_clip_ow
		    || (c->can_owyba && rando_items_has_a_bottle(items))
		    || (can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items) && has(items, RANDO_ITEM_MagicMirror)
			&& (c->can_mirror_clip
			    || (c->can_mirror_wrap
				&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
				    || (c->can_super_speed && rando_items_can_spin_speed(items))))))
		    || (c->can_bunny_surf && c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)
			&& has(items, RANDO_ITEM_Flippers) && has(items, RANDO_ITEM_DefeatAgahnim))
		    || (has(items, RANDO_ITEM_MoonPearl)
			&& ((can_enter(world, locations, RANDO_REGION_NORTH_EAST_DARK_WORLD, items)
			     && ((has(items, RANDO_ITEM_Hookshot) || (c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)))
				 && (rando_items_can_lift_rocks(items) || has(items, RANDO_ITEM_Hammer)
				     || (has(items, RANDO_ITEM_Flippers)
					 || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))))))
			    || (has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items))
			    || rando_items_can_lift_dark_rocks(items)
			    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			    || (c->can_super_speed && rando_items_can_spin_speed(items)))));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_BREWERY - FIRST] = {brewery, NULL, NULL},
	[RANDO_LOC_C_SHAPED_HOUSE - FIRST] = {c_shaped_house, NULL, NULL},
	[RANDO_LOC_CHEST_GAME - FIRST] = {c_shaped_house, NULL, NULL},
	[RANDO_LOC_HAMMER_PEGS - FIRST] = {hammer_pegs, NULL, NULL},
	[RANDO_LOC_BUMPER_CAVE - FIRST] = {bumper_cave, NULL, NULL},
	[RANDO_LOC_BLACKSMITH - FIRST] = {blacksmith, NULL, NULL},
	[RANDO_LOC_PURPLE_CHEST - FIRST] = {purple_chest, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP - FIRST_SHOP] = dark_world_outcasts_shop,
};

const RandoRegionRules rando_rules_north_west_dark_world = {
	"Region\\Standard\\DarkWorld\\NorthWest", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
