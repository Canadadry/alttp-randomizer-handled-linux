#include "../world_rules.h"

#define FIRST RANDO_LOC_OLD_MAN
#define NLOCATIONS (RANDO_LOC_SPECTACLE_ROCK - FIRST + 1)

static bool old_man(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has_lamp(world, items);
}

static bool ether_tablet(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return has(items, RANDO_ITEM_BookOfMudora)
		&& (rando_items_has_sword(items, 2)
		    || (world->config.mode_weapons == RANDO_WEAPONS_SWORDLESS && has(items, RANDO_ITEM_Hammer)))
		&& can_enter(world, locations, RANDO_REGION_TOWER_OF_HERA, items);
}

static bool spectacle_rock(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_MagicMirror)
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow;
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return has(items, RANDO_ITEM_RescueZelda)
		&& (rando_items_can_fly(items, c)
		    || c->can_one_frame_clip_ow
		    || (c->can_owyba && rando_items_has_a_bottle(items))
		    || (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		    || (c->can_super_speed && rando_items_can_spin_speed(items))
		    || (rando_items_can_lift_rocks(items) && has_lamp(world, items)));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_OLD_MAN - FIRST] = {old_man, NULL, NULL},
	[RANDO_LOC_ETHER_TABLET - FIRST] = {ether_tablet, NULL, NULL},
	[RANDO_LOC_SPECTACLE_ROCK - FIRST] = {spectacle_rock, NULL, NULL},
};

const RandoRegionRules rando_rules_west_death_mountain = {
	"Region\\Standard\\LightWorld\\DeathMountain\\West", enter, NULL, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
