#include "../world_rules.h"

#define FIRST RANDO_LOC_MASTER_SWORD_PEDESTAL
#define NLOCATIONS (RANDO_LOC_MUSHROOM - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_LIGHT_WORLD_KAKARIKO_SHOP
#define NSHOPS (RANDO_SHOP_BOMB_HUT - FIRST_SHOP + 1)

static bool bomb_hut(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_bomb_things(items);
}

static bool master_sword_pedestal(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return (world->config.item_placement != RANDO_PLACEMENT_BASIC || has(items, RANDO_ITEM_BookOfMudora))
		&& has(items, RANDO_ITEM_PendantOfPower)
		&& has(items, RANDO_ITEM_PendantOfWisdom)
		&& has(items, RANDO_ITEM_PendantOfCourage);
}

static bool kings_tomb(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_PegasusBoots)
		&& (c->can_boots_clip
		    || rando_items_can_lift_dark_rocks(items)
		    || c->can_one_frame_clip_ow
		    || (c->can_super_speed && rando_items_can_spin_speed(items))
		    || (has(items, RANDO_ITEM_MagicMirror)
			&& can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items)
			&& pearl_or_bunny_revive_or_owyba(c, items)));
}

static bool pegasus_rocks(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PegasusBoots);
}

static bool magic_bat(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_Powder)
		&& (has(items, RANDO_ITEM_Hammer)
		    || ((((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)
			 && c->can_transition_wrapped
			 && (has(items, RANDO_ITEM_Flippers) || c->can_fake_flipper)))
		    || (has(items, RANDO_ITEM_MagicMirror)
			&& ((c->can_mirror_wrap && can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items))
			    || (pearl_or_bunny_revive_or_owyba(c, items)
				&& ((rando_items_can_lift_dark_rocks(items)
				     && can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items))
				    || (c->can_super_speed && rando_items_can_spin_speed(items)
					&& (has(items, RANDO_ITEM_Flippers) || c->can_fake_flipper)
					&& can_enter(world, locations, RANDO_REGION_NORTH_EAST_DARK_WORLD, items)))))));
}

static bool sick_kid(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_has_a_bottle(items);
}

static bool lumberjack_tree(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_DefeatAgahnim) && has(items, RANDO_ITEM_PegasusBoots);
}

static bool graveyard_ledge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (c->can_super_speed && rando_items_can_spin_speed(items))
		|| c->can_one_frame_clip_ow
		|| (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items)
		    && (has(items, RANDO_ITEM_MoonPearl) || (rando_items_has_a_bottle(items) && c->can_owyba)
			|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_RescueZelda);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_MASTER_SWORD_PEDESTAL - FIRST] = {master_sword_pedestal, NULL, NULL},
	[RANDO_LOC_KINGS_TOMB - FIRST] = {kings_tomb, NULL, NULL},
	[RANDO_LOC_PEGASUS_ROCKS - FIRST] = {pegasus_rocks, NULL, NULL},
	[RANDO_LOC_MAGIC_BAT - FIRST] = {magic_bat, NULL, NULL},
	[RANDO_LOC_SICK_KID - FIRST] = {sick_kid, NULL, NULL},
	[RANDO_LOC_LUMBERJACK_TREE - FIRST] = {lumberjack_tree, NULL, NULL},
	[RANDO_LOC_GRAVEYARD_LEDGE - FIRST] = {graveyard_ledge, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_BOMB_HUT - FIRST_SHOP] = bomb_hut,
};

const RandoRegionRules rando_rules_north_west_light_world = {
	"Region\\Standard\\LightWorld\\NorthWest", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
