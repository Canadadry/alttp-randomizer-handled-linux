#include "../world_rules.h"

/* Slots in Region\Inverted\DarkWorld\South's location order: Link's House came last. */
#define SLOTS(X) X(HYPE_CAVE_TOP) X(HYPE_CAVE_MIDDLE_RIGHT) X(HYPE_CAVE_MIDDLE_LEFT) X(HYPE_CAVE_BOTTOM) \
	X(STUMPY) X(HYPE_CAVE_NPC) X(DIGGING_GAME) X(LINKS_HOUSE)
enum { SLOTS(RANDO_SLOT_ENUM) NLOCATIONS };
#ifdef RANDO_FAULTS
static const uint8_t slot_ids[] = {SLOTS(RANDO_SLOT_ID)};
#endif

#define FIRST_SHOP RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP
#define NSHOPS (RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_SPIKE_CAVE - FIRST_SHOP + 1)

static bool ledge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_Flippers)
		|| rando_items_can_fly(items, c)
		|| (c->can_owyba && rando_items_has_a_bottle(items))
		|| ((c->can_boots_clip || c->can_water_walk) && has(items, RANDO_ITEM_PegasusBoots))
		|| c->can_one_frame_clip_ow
		|| (c->can_fake_flipper
		    && ((can_enter(world, locations, RANDO_REGION_NORTH_EAST_DARK_WORLD, items)
			 && (has(items, RANDO_ITEM_Hammer) || rando_items_can_lift_rocks(items)))
			|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c))))
		|| (can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items) && c->region_cant_take_damage);
}

static bool bonk_fairy_dark(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return has(items, RANDO_ITEM_PegasusBoots);
}

static bool dark_lake_hylia_ledge_fairy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_can_bomb_things(items) && ledge(world, locations, items);
}

static bool lift_rocks_and_ledge(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_can_lift_rocks(items) && ledge(world, locations, items);
}

static bool hype_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)world;
	(void)locations;
	return rando_items_can_bomb_things(items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[HYPE_CAVE_TOP] = {hype_cave, NULL, NULL},
	[HYPE_CAVE_MIDDLE_RIGHT] = {hype_cave, NULL, NULL},
	[HYPE_CAVE_MIDDLE_LEFT] = {hype_cave, NULL, NULL},
	[HYPE_CAVE_BOTTOM] = {hype_cave, NULL, NULL},
	[HYPE_CAVE_NPC] = {hype_cave, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_BONK_FAIRY_DARK - FIRST_SHOP] = bonk_fairy_dark,
	[RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_FAIRY - FIRST_SHOP] = dark_lake_hylia_ledge_fairy,
	[RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_HINT - FIRST_SHOP] = lift_rocks_and_ledge,
	[RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_SPIKE_CAVE - FIRST_SHOP] = lift_rocks_and_ledge,
};

/* No entry rule: Region\Inverted\DarkWorld\South sets none. */
const RandoRegionRules rando_rules_inverted_south_dark_world = {
	"Region\\Inverted\\DarkWorld\\South", NULL, NULL, locations, shops,
	RANDO_SLOT_LIST(slot_ids)
};
