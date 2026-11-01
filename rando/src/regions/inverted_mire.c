#include "../world_rules.h"

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return rando_items_can_fly(items, c)
		|| (has(items, RANDO_ITEM_MagicMirror) && can_enter(world, locations, RANDO_REGION_SOUTH_LIGHT_WORLD, items))
		|| (c->can_boots_clip && has(items, RANDO_ITEM_PegasusBoots))
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| c->can_one_frame_clip_ow;
}

const RandoRegionRules rando_rules_inverted_mire = {
	"Region\\Inverted\\DarkWorld\\Mire", enter, NULL, NULL, NULL,
	RANDO_SLOT_RANGE(0, 0)
};
