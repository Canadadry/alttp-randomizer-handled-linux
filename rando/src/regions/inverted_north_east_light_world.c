#include "../world_rules.h"

/* Slots in Region\Inverted\LightWorld\NorthEast's location order. */
#define SLOTS(X) X(SAHASRAHLAS_HUT_LEFT) X(SAHASRAHLAS_HUT_MIDDLE) X(SAHASRAHLAS_HUT_RIGHT) X(SAHASRAHLA) \
	X(KING_ZORA) X(POTION_SHOP) X(ZORAS_LEDGE) X(WATERFALL_FAIRY_LEFT) X(WATERFALL_FAIRY_RIGHT) X(GANON)
enum { SLOTS(RANDO_SLOT_ENUM) NLOCATIONS };
#ifdef RANDO_FAULTS
static const uint8_t slot_ids[] = {SLOTS(RANDO_SLOT_ID)};
#endif

static bool sahasrahlas_hut(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return ((has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& rando_items_can_bomb_things(items))
		|| (c->can_super_bunny && has(items, RANDO_ITEM_PegasusBoots)
		    && (has(items, RANDO_ITEM_MagicMirror) || !c->region_cant_take_damage));
}

static bool sahasrahla(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PendantOfCourage);
}

static bool king_zora(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((has(items, RANDO_ITEM_MoonPearl) || (c->can_owyba && rando_items_has_a_bottle(items)))
		&& (rando_items_can_lift_rocks(items)
		    || (c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))
		    || ((c->can_boots_clip || c->can_water_walk) && has(items, RANDO_ITEM_PegasusBoots))
		    || (c->can_super_speed && rando_items_can_spin_speed(items)
			&& can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items))))
		|| c->can_one_frame_clip_ow;
}

static bool potion_shop(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (has(items, RANDO_ITEM_MoonPearl)
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))
		|| c->can_one_frame_clip_ow)
		&& has(items, RANDO_ITEM_Mushroom);
}

static bool zoras_ledge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return pearl_or_bunny_revive_or_owyba(c, items)
		&& (has(items, RANDO_ITEM_Flippers)
		    || (c->can_water_walk
			&& ((c->can_fake_flipper && c->can_water_walk && has(items, RANDO_ITEM_MoonPearl)
			     && c->can_one_frame_clip_ow)
			    || ((c->can_boots_clip || c->can_one_frame_clip_ow
				 || (c->can_super_speed && rando_items_can_spin_speed(items)))
				&& has(items, RANDO_ITEM_PegasusBoots)))));
}

static bool waterfall_fairy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return pearl_or_bunny_revive_or_owyba(c, items)
		&& (c->can_fake_flipper
		    || (c->can_water_walk && (has(items, RANDO_ITEM_PegasusBoots) || has(items, RANDO_ITEM_MoonPearl)))
		    || has(items, RANDO_ITEM_Flippers)
		    || (can_enter(world, locations, RANDO_REGION_EAST_DEATH_MOUNTAIN, items)
			&& ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			    || (c->can_super_speed && rando_items_can_spin_speed(items))
			    || c->can_one_frame_clip_ow)));
}

static int crystals(const RandoItems *items)
{
	return has(items, RANDO_ITEM_Crystal1) + has(items, RANDO_ITEM_Crystal2) + has(items, RANDO_ITEM_Crystal3)
		+ has(items, RANDO_ITEM_Crystal4) + has(items, RANDO_ITEM_Crystal5) + has(items, RANDO_ITEM_Crystal6)
		+ has(items, RANDO_ITEM_Crystal7);
}

static bool fire_rod_magic(const RandoWorld *world, const RandoItems *items, int bars_with_pearl)
{
	const RandoConfig *c = &world->config;

	return has_lamp(world, items)
		|| (has(items, RANDO_ITEM_FireRod)
		    && ((rando_items_can_extend_magic(items, c, bars_with_pearl) && has(items, RANDO_ITEM_MoonPearl))
			|| rando_items_can_extend_magic(items, c, bars_with_pearl + 1)));
}

/* Unlike Standard's Ganon, no ganonhunt check: VT's Inverted rule has none. */
static bool ganon(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

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
	return (pearl_or_bunny_revive_or_owyba(c, items)
		|| (c->can_super_bunny && c->can_dungeon_revive
		    && can_enter(world, locations, RANDO_REGION_GANONS_TOWER, items)
		    && has(items, RANDO_ITEM_CaneOfSomaria) && has(items, RANDO_ITEM_MagicMirror)
		    && rando_items_has_a_bottle(items)))
		&& (has(items, RANDO_ITEM_DefeatAgahnim2) || c->goal == RANDO_GOAL_FAST_GANON)
		&& (!c->region_require_better_bow || rando_items_can_shoot_arrows(items, c, 2))
		&& ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS && has(items, RANDO_ITEM_Hammer)
		     && fire_rod_magic(world, items, 2))
		    || (!c->region_require_better_sword && rando_items_has_sword(items, 2)
			&& fire_rod_magic(world, items, 3))
		    || (rando_items_has_sword(items, 3) && fire_rod_magic(world, items, 2)));
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	/* VT's hasABottle(2) ignores its argument: one bottle is enough. */
	return has(items, RANDO_ITEM_DefeatAgahnim)
		|| (has(items, RANDO_ITEM_MoonPearl)
		    && ((has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items))
			|| rando_items_can_lift_dark_rocks(items)))
		|| (rando_items_can_fly(items, c) && rando_items_can_lift_dark_rocks(items))
		|| (c->can_owyba && (rando_items_has_a_bottle(items)
				     || (rando_items_has_a_bottle(items) && has_lamp(world, items))))
		|| (can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)
		    && (has(items, RANDO_ITEM_MoonPearl) || has(items, RANDO_ITEM_MagicMirror))
		    && ((c->can_super_speed && rando_items_can_spin_speed(items))
			|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))))
		|| c->can_one_frame_clip_ow;
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[SAHASRAHLAS_HUT_LEFT] = {sahasrahlas_hut, NULL, NULL},
	[SAHASRAHLAS_HUT_MIDDLE] = {sahasrahlas_hut, NULL, NULL},
	[SAHASRAHLAS_HUT_RIGHT] = {sahasrahlas_hut, NULL, NULL},
	[SAHASRAHLA] = {sahasrahla, NULL, NULL},
	[KING_ZORA] = {king_zora, NULL, NULL},
	[POTION_SHOP] = {potion_shop, NULL, NULL},
	[ZORAS_LEDGE] = {zoras_ledge, NULL, NULL},
	[WATERFALL_FAIRY_LEFT] = {waterfall_fairy, NULL, NULL},
	[WATERFALL_FAIRY_RIGHT] = {waterfall_fairy, NULL, NULL},
	[GANON] = {ganon, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_north_east_light_world = {
	"Region\\Inverted\\LightWorld\\NorthEast", enter, NULL, locations, NULL,
	RANDO_SLOT_LIST(slot_ids)
};
