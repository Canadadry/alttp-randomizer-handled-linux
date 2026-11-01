#ifndef RANDO_WORLD_RULES_H
#define RANDO_WORLD_RULES_H

#include "rando_world.h"

#include <stddef.h>

/* world is VT's $this->world/$this->locations, locations is the closure's $locations. VT's
 * $locations->itemInLocations, $locations["X"]->hasItem/getItem read locations;
 * $this->locations["X"]->hasItem, config and bosses read world. $this->locations["X"]->canAccess($items)
 * is rando_world_can_access(world, ...), $locations["X"]->canAccess($items) is
 * rando_world_can_access(locations, ...), and getRegion(X)->canEnter passes (world, locations). */
typedef bool (*RandoRule)(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items);
typedef bool (*RandoAlwaysAllowRule)(const RandoWorld *world, RandoItemId item, const RandoItems *items);
/* Non-const world: Link's Uncle's fill rule calls collect_items, which writes the memo. */
typedef bool (*RandoFillRule)(RandoWorld *world, RandoItemId item, const RandoItems *items);

typedef struct RandoLocationRules {
	RandoRule requirement;
	RandoFillRule fill;
	RandoAlwaysAllowRule always_allow;
} RandoLocationRules;

#ifdef RANDO_FAULTS
/* Host tests only: the location id slot i of a rule table means, ids[i] or first + i. */
typedef struct RandoRuleSlots {
	const uint8_t *ids;
	uint8_t first;
	uint8_t count;
} RandoRuleSlots;
#define RANDO_SLOT_RANGE(first, count) {NULL, (first), (count)},
#define RANDO_SLOT_LIST(ids) {(ids), 0, sizeof(ids)},
#else
#define RANDO_SLOT_RANGE(first, count)
#define RANDO_SLOT_LIST(ids)
#endif
/* For a slot list: SLOTS(RANDO_SLOT_ENUM) numbers the slots, SLOTS(RANDO_SLOT_ID) gives their location ids. */
#define RANDO_SLOT_ENUM(name) name,
#define RANDO_SLOT_ID(name) RANDO_LOC_##name,

/* Any rule may be NULL, as in VT. locations is indexed by the location's layout slot and shops from
 * the region's first_shop; either may be NULL when every entry would be. */
typedef struct RandoRegionRules {
	const char *vt_class;
	RandoRule can_enter;
	RandoRule can_complete;
	const RandoLocationRules *locations;
	const RandoRule *shops;
#ifdef RANDO_FAULTS
	RandoRuleSlots slots;
#endif
} RandoRegionRules;

const RandoRegionRules *rando_region_rules(RandoVariant variant, RandoRegionId region);
/* slot is the location's index in its region's list; region is RANDO_REGION_NONE, and slot 0, where
 * the variant's world lacks the location. */
typedef struct RandoLayoutLocation {
	uint8_t region;
	uint8_t slot;
} RandoLayoutLocation;

/* Indexed by location id. */
const RandoLayoutLocation *rando_layout_locations(RandoVariant variant);
/* rando_layout_location_count(variant) location ids, in VT's getLocations() order. */
const uint8_t *rando_layout_order(RandoVariant variant);

extern const RandoRegionRules rando_rules_north_east_light_world;
extern const RandoRegionRules rando_rules_north_west_light_world;
extern const RandoRegionRules rando_rules_south_light_world;
extern const RandoRegionRules rando_rules_standard_escape;
extern const RandoRegionRules rando_rules_open_escape;
extern const RandoRegionRules rando_rules_eastern_palace;
extern const RandoRegionRules rando_rules_desert_palace;
extern const RandoRegionRules rando_rules_west_death_mountain;
extern const RandoRegionRules rando_rules_east_death_mountain;
extern const RandoRegionRules rando_rules_tower_of_hera;
extern const RandoRegionRules rando_rules_hyrule_castle_tower;
extern const RandoRegionRules rando_rules_east_dark_world_death_mountain;
extern const RandoRegionRules rando_rules_west_dark_world_death_mountain;
extern const RandoRegionRules rando_rules_north_east_dark_world;
extern const RandoRegionRules rando_rules_north_west_dark_world;
extern const RandoRegionRules rando_rules_south_dark_world;
extern const RandoRegionRules rando_rules_mire;
extern const RandoRegionRules rando_rules_palace_of_darkness;
extern const RandoRegionRules rando_rules_swamp_palace;
extern const RandoRegionRules rando_rules_skull_woods;
extern const RandoRegionRules rando_rules_thieves_town;
extern const RandoRegionRules rando_rules_ice_palace;
extern const RandoRegionRules rando_rules_misery_mire;
extern const RandoRegionRules rando_rules_turtle_rock;
extern const RandoRegionRules rando_rules_ganons_tower;
/* An Inverted class calling VT's parent::initalize() is a second table in the classic region's file, reusing its
 * static rules; one rewriting every rule has its own inverted_*.c. */
extern const RandoRegionRules rando_rules_inverted_north_east_light_world;
extern const RandoRegionRules rando_rules_inverted_north_west_light_world;
extern const RandoRegionRules rando_rules_inverted_south_light_world;
extern const RandoRegionRules rando_rules_inverted_escape;
extern const RandoRegionRules rando_rules_inverted_eastern_palace;
extern const RandoRegionRules rando_rules_inverted_desert_palace;
extern const RandoRegionRules rando_rules_inverted_west_death_mountain;
extern const RandoRegionRules rando_rules_inverted_east_death_mountain;
extern const RandoRegionRules rando_rules_inverted_tower_of_hera;
extern const RandoRegionRules rando_rules_inverted_hyrule_castle_tower;
extern const RandoRegionRules rando_rules_inverted_east_dark_world_death_mountain;
extern const RandoRegionRules rando_rules_inverted_west_dark_world_death_mountain;
extern const RandoRegionRules rando_rules_inverted_north_east_dark_world;
extern const RandoRegionRules rando_rules_inverted_north_west_dark_world;
extern const RandoRegionRules rando_rules_inverted_south_dark_world;
extern const RandoRegionRules rando_rules_inverted_mire;
extern const RandoRegionRules rando_rules_inverted_palace_of_darkness;
extern const RandoRegionRules rando_rules_inverted_swamp_palace;
extern const RandoRegionRules rando_rules_inverted_skull_woods;
extern const RandoRegionRules rando_rules_inverted_thieves_town;
extern const RandoRegionRules rando_rules_inverted_ice_palace;
extern const RandoRegionRules rando_rules_inverted_misery_mire;
extern const RandoRegionRules rando_rules_inverted_turtle_rock;
extern const RandoRegionRules rando_rules_inverted_ganons_tower;

static inline bool has(const RandoItems *items, RandoItemId id)
{
	return rando_items_has(items, id);
}

static inline bool has_n(const RandoItems *items, RandoItemId id, int at_least)
{
	return rando_items_has_n(items, id, at_least);
}

bool rando_world_region_can_enter_with(const RandoWorld *world, const RandoWorld *locations, RandoRegionId region,
				      const RandoItems *items);

/* A region's own rules call it for VT's $this->canEnter, never their file's enter: Inverted reuses
 * those rules under another entry rule. */
static inline bool can_enter(const RandoWorld *world, const RandoWorld *locations, RandoRegionId region,
			     const RandoItems *items)
{
	return rando_world_region_can_enter_with(world, locations, region, items);
}

static inline bool has_lamp(const RandoWorld *world, const RandoItems *items)
{
	return rando_items_has_n(items, RANDO_ITEM_Lamp, world->config.item_require_lamp);
}

bool rando_world_pearl_or_bunny_revive_or_owyba(const RandoConfig *c, const RandoItems *items);

static inline bool pearl_or_bunny_revive_or_owyba(const RandoConfig *c, const RandoItems *items)
{
	return rando_world_pearl_or_bunny_revive_or_owyba(c, items);
}

static inline bool wild_or_not_dungeon_item(const RandoConfig *c, RandoItemId item)
{
	return !rando_dungeon_item_confined(c, rando_item_info(item)->item_class);
}

static inline bool is_dungeon_item(RandoItemId item)
{
	RandoItemClass k = rando_item_info(item)->item_class;

	return k == RANDO_ITEM_CLASS_KEY || k == RANDO_ITEM_CLASS_BIG_KEY || k == RANDO_ITEM_CLASS_MAP
		|| k == RANDO_ITEM_CLASS_COMPASS;
}

static inline bool boss_can_beat(const RandoWorld *world, RandoRegionId region, const RandoItems *items)
{
	return rando_boss_can_beat(rando_world_boss(world, region, RANDO_BOSS_SLOT_BOSS), &world->config, items);
}

static inline bool boss_has_wild_compass_and_map(const RandoWorld *world, RandoLocationId boss,
						 RandoItemId compass, RandoItemId map, const RandoItems *items)
{
	const RandoConfig *c = &world->config;

	return (!c->region_wild_compasses || has(items, compass) || rando_world_has_item(world, boss, compass))
		&& (!c->region_wild_maps || has(items, map) || rando_world_has_item(world, boss, map));
}

static inline bool boss_fill_allows(const RandoWorld *world, RandoItemId item)
{
	return world->config.region_boss_normal_location || !is_dungeon_item(item);
}

static inline bool boss_always_allows(const RandoWorld *world, RandoItemId item, RandoItemId compass, RandoItemId map)
{
	return world->config.region_boss_normal_location && (item == compass || item == map);
}

#endif
