#include "../world_rules.h"

#define FIRST RANDO_LOC_SANCTUARY
#define NLOCATIONS (RANDO_LOC_ZELDA - FIRST + 1)

static bool kill_escape_things_and_key(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_kill_escape_things(items, &world->config) && has(items, RANDO_ITEM_KeyH2);
}

static bool kill_escape_things(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_items_can_kill_escape_things(items, &world->config);
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

static bool complete(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)
{
	(void)locations;
	return rando_world_can_access(world, RANDO_LOC_SANCTUARY, items);
}

static const RandoLocationRules locations[NLOCATIONS] = {
	[RANDO_LOC_SANCTUARY - FIRST] = {kill_escape_things_and_key, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_LEFT - FIRST] = {kill_escape_things_and_key, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_MIDDLE - FIRST] = {kill_escape_things_and_key, NULL, NULL},
	[RANDO_LOC_SEWERS_SECRET_ROOM_RIGHT - FIRST] = {kill_escape_things_and_key, NULL, NULL},
	[RANDO_LOC_SEWERS_DARK_CROSS - FIRST] = {kill_escape_things, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_BOOMERANG_CHEST - FIRST] = {kill_escape_things, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_MAP_CHEST - FIRST] = {kill_escape_things, NULL, NULL},
	[RANDO_LOC_HYRULE_CASTLE_ZELDAS_CELL - FIRST] = {kill_escape_things, NULL, NULL},
	[RANDO_LOC_LINKS_UNCLE - FIRST] = {NULL, links_uncle_fill, NULL},
	[RANDO_LOC_SECRET_PASSAGE - FIRST] = {kill_escape_things, secret_passage_fill, NULL},
	[RANDO_LOC_ZELDA - FIRST] = {complete, NULL, NULL},
};

const RandoRegionRules rando_rules_standard_escape = {
	"Region\\Standard\\HyruleCastleEscape", NULL, complete, locations, NULL,
	RANDO_SLOT_RANGE(FIRST, NLOCATIONS)
};
