#include "../world_rules.h"

/* Slots in Region\Inverted\LightWorld\DeathMountain\East's location order: Ether Tablet and
 * Spectacle Rock came last, from West Death Mountain. */
#define SLOTS(X) X(SPIRAL_CAVE) X(MIMIC_CAVE) X(PARADOX_CAVE_LOWER_FAR_LEFT) X(PARADOX_CAVE_LOWER_LEFT) \
	X(PARADOX_CAVE_LOWER_RIGHT) X(PARADOX_CAVE_LOWER_FAR_RIGHT) X(PARADOX_CAVE_LOWER_MIDDLE) \
	X(PARADOX_CAVE_UPPER_LEFT) X(PARADOX_CAVE_UPPER_RIGHT) X(FLOATING_ISLAND) X(ETHER_TABLET) X(SPECTACLE_ROCK)
enum { SLOTS(RANDO_SLOT_ENUM) NLOCATIONS };
#ifdef RANDO_FAULTS
static const uint8_t slot_ids[] = {SLOTS(RANDO_SLOT_ID)};
#endif

#define FIRST_SHOP RANDO_SHOP_LIGHT_WORLD_DEATH_MOUNTAIN_SHOP
#define NSHOPS (RANDO_SHOP_HOOKSHOT_FAIRY - FIRST_SHOP + 1)

static bool owyba(const RandoConfig *c, const RandoItems *items)
{
	return c->can_owyba && rando_items_has_a_bottle(items);
}

static bool owyba_clip(const RandoConfig *c, const RandoItems *items)
{
	return owyba(c, items) && ((c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots)) || c->can_one_frame_clip_ow);
}

static bool hookshot_or_spin_speed(const RandoConfig *c, const RandoItems *items)
{
	return has(items, RANDO_ITEM_Hookshot) || (c->can_super_speed && rando_items_can_spin_speed(items));
}

/* VT's hasABottle(2) ignores its argument: one bottle is enough, here and below. */
static bool paradox_cave_lower(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_MoonPearl)
		|| (owyba(c, items) && hookshot_or_spin_speed(c, items))
		|| owyba_clip(c, items);
}

static bool paradox_cave_upper(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_can_bomb_things(items) && paradox_cave_lower(world, locations, items);
}

static bool light_world_death_mountain_shop(const RandoWorld *world, const RandoWorld *locations,
					    const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (((has(items, RANDO_ITEM_MoonPearl) || owyba(c, items)) && hookshot_or_spin_speed(c, items))
		|| owyba_clip(c, items))
		&& rando_items_can_bomb_things(items);
}

static bool spiral_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return paradox_cave_lower(world, locations, items)
		|| (c->can_super_bunny && has(items, RANDO_ITEM_MagicMirror) && rando_items_has_sword(items, 1));
}

static bool mimic_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_Hammer) && (has(items, RANDO_ITEM_MoonPearl) || owyba_clip(c, items));
}

static bool ether_tablet(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_BookOfMudora)
		&& ((c->mode_weapons == RANDO_WEAPONS_SWORDLESS && has(items, RANDO_ITEM_Hammer))
		    || rando_items_has_sword(items, 2))
		&& ((has(items, RANDO_ITEM_MoonPearl)
		     && (has(items, RANDO_ITEM_Hammer)
			 || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			 || (c->can_super_speed && rando_items_can_spin_speed(items))
			 || (owyba(c, items) && c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))))
		    || c->can_one_frame_clip_ow);
}

static bool spectacle_rock(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (has(items, RANDO_ITEM_MoonPearl)
		&& (has(items, RANDO_ITEM_Hammer)
		    || (c->can_super_speed && rando_items_can_spin_speed(items))
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))))
		|| (owyba(c, items) && c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow;
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (rando_items_can_lift_dark_rocks(items)
		&& can_enter(world, locations, RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN, items))
		|| (can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items)
		    && (((has(items, RANDO_ITEM_MoonPearl) || owyba(c, items))
			 && (has(items, RANDO_ITEM_Hookshot)
			     || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
			     || (c->can_super_speed && rando_items_can_spin_speed(items))))
			|| (c->can_mirror_wrap && has(items, RANDO_ITEM_MagicMirror))
			|| c->can_one_frame_clip_ow));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[SPIRAL_CAVE] = {spiral_cave, NULL, NULL},
	[MIMIC_CAVE] = {mimic_cave, NULL, NULL},
	[PARADOX_CAVE_LOWER_FAR_LEFT] = {paradox_cave_lower, NULL, NULL},
	[PARADOX_CAVE_LOWER_LEFT] = {paradox_cave_lower, NULL, NULL},
	[PARADOX_CAVE_LOWER_RIGHT] = {paradox_cave_lower, NULL, NULL},
	[PARADOX_CAVE_LOWER_FAR_RIGHT] = {paradox_cave_lower, NULL, NULL},
	[PARADOX_CAVE_LOWER_MIDDLE] = {paradox_cave_lower, NULL, NULL},
	[PARADOX_CAVE_UPPER_LEFT] = {paradox_cave_upper, NULL, NULL},
	[PARADOX_CAVE_UPPER_RIGHT] = {paradox_cave_upper, NULL, NULL},
	[ETHER_TABLET] = {ether_tablet, NULL, NULL},
	[SPECTACLE_ROCK] = {spectacle_rock, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_LIGHT_WORLD_DEATH_MOUNTAIN_SHOP - FIRST_SHOP] = light_world_death_mountain_shop,
};

const RandoRegionRules rando_rules_inverted_east_death_mountain = {
	"Region\\Inverted\\LightWorld\\DeathMountain\\East", enter, NULL, locations, shops,
	RANDO_SLOT_LIST(slot_ids)
};
