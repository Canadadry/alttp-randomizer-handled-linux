#include "../world_rules.h"

#define FIRST RANDO_LOC_CATFISH
#define NLOCATIONS (RANDO_LOC_PYRAMID_FAIRY_RIGHT - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_DARK_WORLD_POTION_SHOP
#define NSHOPS (RANDO_SHOP_PALACE_OF_DARKNESS_HINT - FIRST_SHOP + 1)

static bool dark_world_potion_shop(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
		|| ((has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		    && (c->can_one_frame_clip_ow
			|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			|| has(items, RANDO_ITEM_Hammer) || has(items, RANDO_ITEM_Flippers)
			|| rando_items_can_lift_rocks(items)));
}

static bool dark_world_hint(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool catfish(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return pearl_or_bunny_revive_or_owyba(c, items)
		&& (c->can_one_frame_clip_ow
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || rando_items_can_lift_rocks(items));
}

static bool pyramid_fairy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (has(items, RANDO_ITEM_Crystal5) && has(items, RANDO_ITEM_Crystal6)
		&& can_enter(world, locations, RANDO_REGION_SOUTH_DARK_WORLD, items)
		&& ((has(items, RANDO_ITEM_MoonPearl) && has(items, RANDO_ITEM_Hammer))
		    || (c->can_owyba && rando_items_has_a_bottle(items))
		    || (has(items, RANDO_ITEM_MagicMirror) && has(items, RANDO_ITEM_DefeatAgahnim))))
		|| (c->can_mirror_clip && has(items, RANDO_ITEM_MagicMirror)
		    && ((c->can_super_speed && rando_items_can_spin_speed(items))
			|| c->can_one_frame_clip_ow
			|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))));
}

static bool pyramid_fairy_sword(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_has_sword(items, 1) && pyramid_fairy(world, locations, items);
}

static bool pyramid_fairy_bow(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_can_shoot_arrows(items, &world->config, 1) && pyramid_fairy(world, locations, items);
}

/* VT sets this requirement in initalize only when region.swordsInPool holds then. */
static bool pyramid_fairy_left_right(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return !world->config.region_swords_in_pool || pyramid_fairy(world, locations, items);
}

static int crystals(const RandoItems *items)
{
	return has(items, RANDO_ITEM_Crystal1) + has(items, RANDO_ITEM_Crystal2) + has(items, RANDO_ITEM_Crystal3)
		+ has(items, RANDO_ITEM_Crystal4) + has(items, RANDO_ITEM_Crystal5) + has(items, RANDO_ITEM_Crystal6)
		+ has(items, RANDO_ITEM_Crystal7);
}

static bool ganon(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	if (c->goal == RANDO_GOAL_GANONHUNT && !has_n(items, RANDO_ITEM_TriforcePiece, c->item_goal_required))
		return false;
	if (c->goal == RANDO_GOAL_DUNGEONS
	    && (!has(items, RANDO_ITEM_PendantOfCourage)
		|| !has(items, RANDO_ITEM_PendantOfWisdom)
		|| !has(items, RANDO_ITEM_PendantOfPower)
		|| !has(items, RANDO_ITEM_DefeatAgahnim)
		|| !has(items, RANDO_ITEM_Crystal1)
		|| !has(items, RANDO_ITEM_Crystal2)
		|| !has(items, RANDO_ITEM_Crystal3)
		|| !has(items, RANDO_ITEM_Crystal4)
		|| !has(items, RANDO_ITEM_Crystal5)
		|| !has(items, RANDO_ITEM_Crystal6)
		|| !has(items, RANDO_ITEM_Crystal7)
		|| !has(items, RANDO_ITEM_DefeatAgahnim2)))
		return false;
	if ((c->goal == RANDO_GOAL_GANON || c->goal == RANDO_GOAL_FAST_GANON) && crystals(items) < c->crystals_ganon)
		return false;
	return (has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& (has(items, RANDO_ITEM_DefeatAgahnim2) || c->goal == RANDO_GOAL_FAST_GANON
		    || c->goal == RANDO_GOAL_GANONHUNT)
		&& (!c->region_require_better_bow || rando_items_can_shoot_arrows(items, c, 2))
		&& ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS && has(items, RANDO_ITEM_Hammer)
		     && (has_lamp(world, items)
			 || (has(items, RANDO_ITEM_FireRod) && rando_items_can_extend_magic(items, c, 1))))
		    || (!c->region_require_better_sword
			&& (rando_items_has_sword(items, 2)
			    && (has_lamp(world, items)
				|| (has(items, RANDO_ITEM_FireRod) && rando_items_can_extend_magic(items, c, 3)))))
		    || (rando_items_has_sword(items, 3)
			&& (has_lamp(world, items)
			    || (has(items, RANDO_ITEM_FireRod) && rando_items_can_extend_magic(items, c, 2)))));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& ((c->can_owyba && rando_items_has_a_bottle(items))
		    || c->can_one_frame_clip_ow
		    || (has(items, RANDO_ITEM_MoonPearl) && (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)))
		    || (has(items, RANDO_ITEM_MagicMirror) && c->can_mirror_clip
			&& can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)
			&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			    || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
			    || (c->can_super_speed && rando_items_can_spin_speed(items))
			    || (has(items, RANDO_ITEM_MoonPearl) && c->can_fake_flipper)))
		    || has(items, RANDO_ITEM_DefeatAgahnim)
		    || (has(items, RANDO_ITEM_MagicMirror) && c->can_mirror_wrap
			&& c->can_bunny_revive && rando_items_can_bunny_revive(items, c)
			&& ((c->can_super_speed && rando_items_can_spin_speed(items))
			    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			    || c->can_mirror_clip)
			&& can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items))
		    || (has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items) && has(items, RANDO_ITEM_MoonPearl))
		    || ((rando_items_can_lift_dark_rocks(items)
			 || (can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)
			     && ((c->can_super_speed && rando_items_can_spin_speed(items))
				 || (has(items, RANDO_ITEM_MagicMirror) && c->can_mirror_clip))))
			&& has(items, RANDO_ITEM_MoonPearl)
			&& (has(items, RANDO_ITEM_Hammer) || has(items, RANDO_ITEM_Flippers)
			    || (has(items, RANDO_ITEM_MagicMirror) && c->can_mirror_wrap && rando_items_can_lift_rocks(items))
			    || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
			    || (c->can_water_walk && has(items, RANDO_ITEM_PegasusBoots))
			    || (c->can_fake_flipper && !c->region_cant_take_damage))));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_CATFISH - FIRST] = {catfish, NULL, NULL},
	[RANDO_LOC_PYRAMID_FAIRY_SWORD - FIRST] = {pyramid_fairy_sword, NULL, NULL},
	[RANDO_LOC_PYRAMID_FAIRY_BOW - FIRST] = {pyramid_fairy_bow, NULL, NULL},
	[RANDO_LOC_GANON - FIRST] = {ganon, NULL, NULL},
	[RANDO_LOC_PYRAMID_FAIRY_LEFT - FIRST] = {pyramid_fairy_left_right, NULL, NULL},
	[RANDO_LOC_PYRAMID_FAIRY_RIGHT - FIRST] = {pyramid_fairy_left_right, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_DARK_WORLD_POTION_SHOP - FIRST_SHOP] = dark_world_potion_shop,
	[RANDO_SHOP_EAST_DARK_WORLD_HINT - FIRST_SHOP] = dark_world_hint,
	[RANDO_SHOP_PALACE_OF_DARKNESS_HINT - FIRST_SHOP] = dark_world_hint,
};

const RandoRegionRules rando_rules_north_east_dark_world = {
	"Region\\Standard\\DarkWorld\\NorthEast", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
