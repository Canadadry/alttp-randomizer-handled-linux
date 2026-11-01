#include "../world_rules.h"

#define FIRST RANDO_LOC_HYPE_CAVE_TOP
#define NLOCATIONS (RANDO_LOC_DIGGING_GAME - FIRST + 1)
#define FIRST_SHOP RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP
#define NSHOPS (RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_SPIKE_CAVE - FIRST_SHOP + 1)

static bool ledge_water(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_Flippers)
		|| (can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items)
		    && c->can_fake_flipper && (!c->region_cant_take_damage))
		|| (c->can_bunny_revive && rando_items_can_bunny_revive(items, c));
}

static bool hype_cave(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool stumpy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return pearl_or_bunny_revive_or_owyba(c, items)
		|| (has(items, RANDO_ITEM_MagicMirror) && c->can_mirror_wrap);
}

static bool bonk_fairy_dark(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_PegasusBoots) && pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool dark_lake_hylia_ledge_fairy(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (rando_items_can_bomb_things(items) || (has(items, RANDO_ITEM_PegasusBoots) && c->can_boots_clip))
		&& ledge_water(world, locations, items)
		&& pearl_or_bunny_revive_or_owyba(c, items);
}

static bool dark_lake_hylia_ledge_hint(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return ledge_water(world, locations, items) && pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool dark_lake_hylia_ledge_spike_cave(const RandoWorld *world, const RandoWorld *locations,
					     const RandoItems *items)
{
	return rando_items_can_lift_rocks(items) && ledge_water(world, locations, items)
		&& pearl_or_bunny_revive_or_owyba(&world->config, items);
}

static bool enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return has(items, RANDO_ITEM_RescueZelda)
		&& ((c->can_owyba && rando_items_has_a_bottle(items))
		    || ((has(items, RANDO_ITEM_MoonPearl) || (c->can_bunny_revive && rando_items_can_bunny_revive(items, c)))
			&& (can_enter(world, locations, RANDO_REGION_NORTH_EAST_DARK_WORLD, items)
			    && (has(items, RANDO_ITEM_Hammer)
				|| (c->can_super_speed && rando_items_can_spin_speed(items)
				    && (c->can_fake_flipper || has(items, RANDO_ITEM_Flippers))))))
		    || can_enter(world, locations, RANDO_REGION_NORTH_WEST_DARK_WORLD, items));
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_HYPE_CAVE_TOP - FIRST] = {hype_cave, NULL, NULL},
	[RANDO_LOC_HYPE_CAVE_MIDDLE_RIGHT - FIRST] = {hype_cave, NULL, NULL},
	[RANDO_LOC_HYPE_CAVE_MIDDLE_LEFT - FIRST] = {hype_cave, NULL, NULL},
	[RANDO_LOC_HYPE_CAVE_BOTTOM - FIRST] = {hype_cave, NULL, NULL},
	[RANDO_LOC_STUMPY - FIRST] = {stumpy, NULL, NULL},
	[RANDO_LOC_HYPE_CAVE_NPC - FIRST] = {hype_cave, NULL, NULL},
	[RANDO_LOC_DIGGING_GAME - FIRST] = {hype_cave, NULL, NULL},
};

static const RandoRule shops[NSHOPS] = {
	[RANDO_SHOP_BONK_FAIRY_DARK - FIRST_SHOP] = bonk_fairy_dark,
	[RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_FAIRY - FIRST_SHOP] = dark_lake_hylia_ledge_fairy,
	[RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_HINT - FIRST_SHOP] = dark_lake_hylia_ledge_hint,
	[RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_SPIKE_CAVE - FIRST_SHOP] = dark_lake_hylia_ledge_spike_cave,
};

const RandoRegionRules rando_rules_south_dark_world = {
	"Region\\Standard\\DarkWorld\\South", enter, NULL, locations, shops,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
