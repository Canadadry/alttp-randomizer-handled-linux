#include "../world_rules.h"

#define FIRST RANDO_LOC_MASTER_SWORD_PEDESTAL
#define NLOCATIONS (RANDO_LOC_MUSHROOM - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_LIGHT_WORLD_KAKARIKO_SHOP
#define NSHOPS (RANDO_SHOP_BOMB_HUT - FIRST_SHOP + 1)

static bool link_state(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool bomb_hut(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_MoonPearl)
		|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
		|| (c->can_owyba && rando_items_has_a_bottle(items) && rando_items_can_bomb_things(items));
}

static bool master_sword_pedestal(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PendantOfPower) && has(items, RANDO_ITEM_PendantOfWisdom)
		&& has(items, RANDO_ITEM_PendantOfCourage);
}

static bool kings_tomb(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_PegasusBoots) && pearl_or_bunny_revive_or_owyba(c, items)
		&& (rando_items_can_lift_dark_rocks(items)
		    || (c->can_mirror_clip && has(items, RANDO_ITEM_MagicMirror) && has(items, RANDO_ITEM_MoonPearl)
			&& (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)))
		    || c->can_one_frame_clip_ow
		    || (can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items)
			&& (c->can_super_speed && rando_items_can_spin_speed(items)
			    && (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items))))));
}

static bool kakariko_tavern(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;
	/* VT reads the bare key cantTakeDamage, not region.cantTakeDamage; nothing sets it. */
	const bool cant_take_damage = false;

	(void)locations;
	return has(items, RANDO_ITEM_MoonPearl)
		|| (c->can_super_bunny && (has(items, RANDO_ITEM_MagicMirror) || !cant_take_damage))
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c));
}

static bool bombs_and_link_state(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_can_bomb_things(items) && link_state(world, locations, items);
}

static bool kakariko_well(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return world->config.can_super_bunny || link_state(world, locations, items);
}

static bool blinds_hideout(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (world->config.can_super_bunny && has(items, RANDO_ITEM_MagicMirror))
		|| link_state(world, locations, items);
}

static bool pegasus_rocks(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_PegasusBoots) && link_state(world, locations, items);
}

static bool magic_bat(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_Powder) && link_state(world, locations, items)
		&& (has(items, RANDO_ITEM_Hammer)
		    || (((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow)
			&& (c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))));
}

static bool sick_kid(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_has_a_bottle(items);
}

static bool lumberjack_tree(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_DefeatAgahnim)
		&& ((has(items, RANDO_ITEM_PegasusBoots) && link_state(world, locations, items))
		    || c->can_one_frame_clip_ow
		    || (c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror)));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_MASTER_SWORD_PEDESTAL - FIRST] = {master_sword_pedestal, NULL, NULL},
	[RANDO_LOC_KINGS_TOMB - FIRST] = {kings_tomb, NULL, NULL},
	[RANDO_LOC_KAKARIKO_TAVERN - FIRST] = {kakariko_tavern, NULL, NULL},
	[RANDO_LOC_CHICKEN_HOUSE - FIRST] = {link_state, NULL, NULL},
	[RANDO_LOC_KAKARIKO_WELL_TOP - FIRST] = {bombs_and_link_state, NULL, NULL},
	[RANDO_LOC_KAKARIKO_WELL_LEFT - FIRST] = {kakariko_well, NULL, NULL},
	[RANDO_LOC_KAKARIKO_WELL_MIDDLE - FIRST] = {kakariko_well, NULL, NULL},
	[RANDO_LOC_KAKARIKO_WELL_RIGHT - FIRST] = {kakariko_well, NULL, NULL},
	[RANDO_LOC_KAKARIKO_WELL_BOTTOM - FIRST] = {kakariko_well, NULL, NULL},
	[RANDO_LOC_BLINDS_HIDEOUT_TOP - FIRST] = {bombs_and_link_state, NULL, NULL},
	[RANDO_LOC_BLINDS_HIDEOUT_LEFT - FIRST] = {blinds_hideout, NULL, NULL},
	[RANDO_LOC_BLINDS_HIDEOUT_RIGHT - FIRST] = {blinds_hideout, NULL, NULL},
	[RANDO_LOC_BLINDS_HIDEOUT_FAR_LEFT - FIRST] = {blinds_hideout, NULL, NULL},
	[RANDO_LOC_BLINDS_HIDEOUT_FAR_RIGHT - FIRST] = {blinds_hideout, NULL, NULL},
	[RANDO_LOC_PEGASUS_ROCKS - FIRST] = {pegasus_rocks, NULL, NULL},
	[RANDO_LOC_MAGIC_BAT - FIRST] = {magic_bat, NULL, NULL},
	[RANDO_LOC_SICK_KID - FIRST] = {sick_kid, NULL, NULL},
	[RANDO_LOC_LOST_WOODS_HIDEOUT - FIRST] = {link_state, NULL, NULL},
	[RANDO_LOC_LUMBERJACK_TREE - FIRST] = {lumberjack_tree, NULL, NULL},
	[RANDO_LOC_GRAVEYARD_LEDGE - FIRST] = {bombs_and_link_state, NULL, NULL},
	[RANDO_LOC_MUSHROOM - FIRST] = {link_state, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_BUSH_COVERED_HOUSE - FIRST_SHOP] = link_state,
	[RANDO_SHOP_BOMB_HUT - FIRST_SHOP] = bomb_hut,
};

const RandoRegionRules rando_rules_inverted_north_west_light_world = {
	"Region\\Inverted\\LightWorld\\NorthWest", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
