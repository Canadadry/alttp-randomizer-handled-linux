#include "../world_rules.h"

/* Region\Open\HyruleCastleEscape::initalize does not call its parent's: Sanctuary, the Map Chest
 * and Zelda have no requirement, and the region no completion rule. */

#define FIRST RANDO_LOC_SANCTUARY
#define NLOCATIONS (RANDO_LOC_ZELDA - FIRST + 1)

static bool lamp_or_advanced_fire_rod(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has_lamp(world, items)
		|| (world->config.item_placement == RANDO_PLACEMENT_ADVANCED && has(items, RANDO_ITEM_FireRod));
}

static bool sewers_secret_room(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return rando_items_can_lift_rocks(items)
		|| (lamp_or_advanced_fire_rod(world, locations, items)
		    && has(items, RANDO_ITEM_KeyH2) && rando_items_can_kill_most_things(items, &world->config, 5));
}

static bool key_and_kill_most_things(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_KeyH2) && rando_items_can_kill_most_things(items, &world->config, 5);
}

static bool secret_passage_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	(void)items;
	return wild_or_not_dungeon_item(&world->config, item);
}

static bool links_uncle_fill(RandoWorld *world, RandoItemId item, const RandoItems *items)
{
	RandoItems collected;

	(void)items;
	rando_world_collect_items(world, NULL, &collected);
	return rando_world_can_access(world, RANDO_LOC_SANCTUARY, &collected)
		&& wild_or_not_dungeon_item(&world->config, item);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SEWERS_SECRET_ROOM_LEFT - FIRST] = {sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_MIDDLE - FIRST] = {sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_RIGHT - FIRST] = {sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_DARK_CROSS - FIRST] = {lamp_or_advanced_fire_rod, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_BOOMERANG_CHEST - FIRST] = {key_and_kill_most_things, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_ZELDAS_CELL - FIRST] = {key_and_kill_most_things, NULL, NULL},
	[RANDO_LOC_LINKS_UNCLE - FIRST] = {NULL, links_uncle_fill, NULL},
	[RANDO_LOC_SECRET_PASSAGE - FIRST] = {NULL, secret_passage_fill, NULL},
};

const RandoRegionRules rando_rules_open_escape = {
	"Region\\Open\\HyruleCastleEscape", NULL, NULL, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};

static bool inverted_fights(const RandoConfig *c, const RandoItems *items)
{
	return c->can_dungeon_revive || rando_items_has_sword(items, 1) || pearl_or_bunny_revive_or_owyba(c, items);
}

static bool inverted_sewers_secret_room(const RandoWorld *world, const RandoWorld *locations,
					const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	(void)locations;
	return (rando_items_can_lift_rocks(items) && pearl_or_bunny_revive_or_owyba(c, items))
		|| (has_lamp(world, items) && has(items, RANDO_ITEM_KeyH2) && inverted_fights(c, items));
}

static bool inverted_key_and_fights(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return has(items, RANDO_ITEM_KeyH2) && inverted_fights(&world->config, items);
}

static bool inverted_sanctuary(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (has(items, RANDO_ITEM_KeyH2) && has_lamp(world, items))
		|| ((pearl_or_bunny_revive_or_owyba(c, items)
		     || (c->can_super_bunny && has(items, RANDO_ITEM_MagicMirror)))
		    && can_enter(world, locations, RANDO_REGION_NORTH_WEST_LIGHT_WORLD, items));
}

/* VT's has('PegasusBoot') names no item: always false. */
static bool inverted_mirror_clip(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;
	const bool pegasus_boot = false;

	return c->can_mirror_clip && has(items, RANDO_ITEM_MagicMirror)
		&& ((c->can_boots_clip && pegasus_boot)
		    || (c->can_super_speed && rando_items_can_spin_speed(items))
		    || c->can_one_frame_clip_ow)
		&& can_enter(world, locations, RANDO_REGION_WEST_DEATH_MOUNTAIN, items);
}

static bool inverted_secret_passage(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return ((c->can_super_bunny && inverted_mirror_clip(world, locations, items))
		|| pearl_or_bunny_revive_or_owyba(c, items))
		&& can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items);
}

static bool inverted_links_uncle(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	return (inverted_mirror_clip(world, locations, items) || pearl_or_bunny_revive_or_owyba(&world->config, items))
		&& can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items);
}

static bool inverted_enter(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (c->can_dungeon_revive || (c->can_super_bunny && has(items, RANDO_ITEM_MagicMirror))
		|| pearl_or_bunny_revive_or_owyba(c, items))
		&& can_enter(world, locations, RANDO_REGION_NORTH_EAST_LIGHT_WORLD, items);
}

static const RandoLocationRules inverted_locations[NLOCATIONS] = {
	[RANDO_LOC_SANCTUARY - FIRST] = {inverted_sanctuary, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_LEFT - FIRST] = {inverted_sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_MIDDLE - FIRST] = {inverted_sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_RIGHT - FIRST] = {inverted_sewers_secret_room, NULL, NULL},
	[RANDO_LOC_SEWERS_DARK_CROSS - FIRST] = {lamp_or_advanced_fire_rod, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_BOOMERANG_CHEST - FIRST] = {inverted_key_and_fights, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_ZELDAS_CELL - FIRST] = {inverted_key_and_fights, NULL, NULL},
	[RANDO_LOC_LINKS_UNCLE - FIRST] = {inverted_links_uncle, links_uncle_fill, NULL},
	[RANDO_LOC_SECRET_PASSAGE - FIRST] = {inverted_secret_passage, secret_passage_fill, NULL},
};

const RandoRegionRules rando_rules_inverted_escape = {
	"Region\\Inverted\\HyruleCastleEscape", inverted_enter, NULL, inverted_locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
