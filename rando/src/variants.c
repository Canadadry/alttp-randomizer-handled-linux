#include "world_rules.h"

#include <assert.h>
#include <stddef.h>

#define NO_RULES(id, cls) static const RandoRegionRules id = {cls, NULL, NULL, NULL, NULL, RANDO_SLOT_RANGE(0, 0)}

NO_RULES(medallions, "Region\\Standard\\Medallions");
NO_RULES(fountains, "Region\\Standard\\Fountains");

#define SHARED_REGIONS \
	[RANDO_REGION_NORTH_EAST_LIGHT_WORLD] = &rando_rules_north_east_light_world, \
	[RANDO_REGION_NORTH_WEST_LIGHT_WORLD] = &rando_rules_north_west_light_world, \
	[RANDO_REGION_SOUTH_LIGHT_WORLD] = &rando_rules_south_light_world, \
	[RANDO_REGION_EASTERN_PALACE] = &rando_rules_eastern_palace, \
	[RANDO_REGION_DESERT_PALACE] = &rando_rules_desert_palace, \
	[RANDO_REGION_WEST_DEATH_MOUNTAIN] = &rando_rules_west_death_mountain, \
	[RANDO_REGION_EAST_DEATH_MOUNTAIN] = &rando_rules_east_death_mountain, \
	[RANDO_REGION_TOWER_OF_HERA] = &rando_rules_tower_of_hera, \
	[RANDO_REGION_HYRULE_CASTLE_TOWER] = &rando_rules_hyrule_castle_tower, \
	[RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN] = &rando_rules_east_dark_world_death_mountain, \
	[RANDO_REGION_WEST_DARK_WORLD_DEATH_MOUNTAIN] = &rando_rules_west_dark_world_death_mountain, \
	[RANDO_REGION_NORTH_EAST_DARK_WORLD] = &rando_rules_north_east_dark_world, \
	[RANDO_REGION_NORTH_WEST_DARK_WORLD] = &rando_rules_north_west_dark_world, \
	[RANDO_REGION_SOUTH_DARK_WORLD] = &rando_rules_south_dark_world, \
	[RANDO_REGION_MIRE] = &rando_rules_mire, \
	[RANDO_REGION_PALACE_OF_DARKNESS] = &rando_rules_palace_of_darkness, \
	[RANDO_REGION_SWAMP_PALACE] = &rando_rules_swamp_palace, \
	[RANDO_REGION_SKULL_WOODS] = &rando_rules_skull_woods, \
	[RANDO_REGION_THIEVES_TOWN] = &rando_rules_thieves_town, \
	[RANDO_REGION_ICE_PALACE] = &rando_rules_ice_palace, \
	[RANDO_REGION_MISERY_MIRE] = &rando_rules_misery_mire, \
	[RANDO_REGION_TURTLE_ROCK] = &rando_rules_turtle_rock, \
	[RANDO_REGION_GANONS_TOWER] = &rando_rules_ganons_tower, \
	[RANDO_REGION_MEDALLIONS] = &medallions, \
	[RANDO_REGION_FOUNTAINS] = &fountains

static const RandoRegionRules *const variants[RANDO_VARIANT_COUNT][RANDO_REGION_COUNT] = {
	[RANDO_VARIANT_STANDARD] = {SHARED_REGIONS, [RANDO_REGION_ESCAPE] = &rando_rules_standard_escape},
	[RANDO_VARIANT_OPEN] = {SHARED_REGIONS, [RANDO_REGION_ESCAPE] = &rando_rules_open_escape},
	[RANDO_VARIANT_INVERTED] = {
		[RANDO_REGION_NORTH_EAST_LIGHT_WORLD] = &rando_rules_inverted_north_east_light_world,
		[RANDO_REGION_NORTH_WEST_LIGHT_WORLD] = &rando_rules_inverted_north_west_light_world,
		[RANDO_REGION_SOUTH_LIGHT_WORLD] = &rando_rules_inverted_south_light_world,
		[RANDO_REGION_ESCAPE] = &rando_rules_inverted_escape,
		[RANDO_REGION_EASTERN_PALACE] = &rando_rules_inverted_eastern_palace,
		[RANDO_REGION_DESERT_PALACE] = &rando_rules_inverted_desert_palace,
		[RANDO_REGION_WEST_DEATH_MOUNTAIN] = &rando_rules_inverted_west_death_mountain,
		[RANDO_REGION_EAST_DEATH_MOUNTAIN] = &rando_rules_inverted_east_death_mountain,
		[RANDO_REGION_TOWER_OF_HERA] = &rando_rules_inverted_tower_of_hera,
		[RANDO_REGION_HYRULE_CASTLE_TOWER] = &rando_rules_inverted_hyrule_castle_tower,
		[RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN] = &rando_rules_inverted_east_dark_world_death_mountain,
		[RANDO_REGION_WEST_DARK_WORLD_DEATH_MOUNTAIN] = &rando_rules_inverted_west_dark_world_death_mountain,
		[RANDO_REGION_NORTH_EAST_DARK_WORLD] = &rando_rules_inverted_north_east_dark_world,
		[RANDO_REGION_NORTH_WEST_DARK_WORLD] = &rando_rules_inverted_north_west_dark_world,
		[RANDO_REGION_SOUTH_DARK_WORLD] = &rando_rules_inverted_south_dark_world,
		[RANDO_REGION_MIRE] = &rando_rules_inverted_mire,
		[RANDO_REGION_PALACE_OF_DARKNESS] = &rando_rules_inverted_palace_of_darkness,
		[RANDO_REGION_SWAMP_PALACE] = &rando_rules_inverted_swamp_palace,
		[RANDO_REGION_SKULL_WOODS] = &rando_rules_inverted_skull_woods,
		[RANDO_REGION_THIEVES_TOWN] = &rando_rules_inverted_thieves_town,
		[RANDO_REGION_ICE_PALACE] = &rando_rules_inverted_ice_palace,
		[RANDO_REGION_MISERY_MIRE] = &rando_rules_inverted_misery_mire,
		[RANDO_REGION_TURTLE_ROCK] = &rando_rules_inverted_turtle_rock,
		[RANDO_REGION_GANONS_TOWER] = &rando_rules_inverted_ganons_tower,
		[RANDO_REGION_MEDALLIONS] = &medallions,
		[RANDO_REGION_FOUNTAINS] = &fountains,
	},
	[RANDO_VARIANT_RETRO] = {SHARED_REGIONS, [RANDO_REGION_ESCAPE] = &rando_rules_open_escape},
};

const RandoRegionRules *rando_region_rules(RandoVariant variant, RandoRegionId region)
{
	assert(variant < RANDO_VARIANT_COUNT && region < RANDO_REGION_COUNT);
	return variants[variant][region];
}

const char *rando_region_vt_class(RandoVariant variant, RandoRegionId region)
{
	assert(variant < RANDO_VARIANT_COUNT && region < RANDO_REGION_COUNT);
	return rando_region_rules(variant, region)->vt_class;
}

#ifdef RANDO_FAULTS
int rando_region_rule_slot_count(RandoVariant variant, RandoRegionId region)
{
	const RandoRegionRules *rules = rando_region_rules(variant, region);

	return rules->locations == NULL ? -1 : rules->slots.count;
}

RandoLocationId rando_region_rule_slot_location(RandoVariant variant, RandoRegionId region, int slot)
{
	const RandoRuleSlots *slots = &rando_region_rules(variant, region)->slots;

	assert(slot >= 0 && slot < slots->count);
	return (RandoLocationId)(slots->ids != NULL ? slots->ids[slot] : slots->first + slot);
}
#endif
