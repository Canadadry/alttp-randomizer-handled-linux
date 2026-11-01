#include "../world_rules.h"

/* Slots in Region\Inverted\LightWorld\DeathMountain\West's location order: Ether Tablet and
 * Spectacle Rock moved to East Death Mountain. */
#define SLOTS(X) X(OLD_MAN) X(SPECTACLE_ROCK_CAVE)
enum { SLOTS(RANDO_SLOT_ENUM) NLOCATIONS };
#ifdef RANDO_FAULTS
static const uint8_t slot_ids[] = {SLOTS(RANDO_SLOT_ID)};
#endif

static bool old_man(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has_lamp(world, items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return rando_items_can_fly(items, c)
		|| (rando_items_can_lift_rocks(items) && has_lamp(world, items))
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow
		|| (c->can_owyba && rando_items_has_a_bottle(items));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[OLD_MAN] = {old_man, NULL, NULL},
};

const RandoRegionRules rando_rules_inverted_west_death_mountain = {
	"Region\\Inverted\\LightWorld\\DeathMountain\\West", enter, NULL, locations, NULL,
	RANDO_SLOT_LIST(slot_ids)
};
