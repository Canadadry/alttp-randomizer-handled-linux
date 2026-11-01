#ifndef RANDO_OPTIONS_H
#define RANDO_OPTIONS_H

#include "rando_config.h"

#include <stdbool.h>

/* The test matrix's option keys and values; each enum lists the baseline first. */
typedef enum RandoOptionKey {
	RANDO_OPTION_GLITCHES,
	RANDO_OPTION_ITEM_PLACEMENT,
	RANDO_OPTION_DUNGEON_ITEMS,
	RANDO_OPTION_ACCESSIBILITY,
	RANDO_OPTION_GOAL,
	RANDO_OPTION_CRYSTALS_TOWER,
	RANDO_OPTION_CRYSTALS_GANON,
	RANDO_OPTION_STATE,
	RANDO_OPTION_WEAPONS,
	RANDO_OPTION_ITEM_POOL,
	RANDO_OPTION_ITEM_FUNCTIONALITY,
	RANDO_OPTION_KEY_COUNT,
} RandoOptionKey;

typedef enum RandoGlitches {
	RANDO_GLITCHES_NONE,
	RANDO_GLITCHES_OVERWORLD_GLITCHES,
	RANDO_GLITCHES_HYBRID_MAJOR_GLITCHES,
	RANDO_GLITCHES_MAJOR_GLITCHES,
	RANDO_GLITCHES_NO_LOGIC,
} RandoGlitches;

typedef enum RandoOptionPlacement {
	RANDO_OPTION_PLACEMENT_ADVANCED,
	RANDO_OPTION_PLACEMENT_BASIC,
} RandoOptionPlacement;

typedef enum RandoOptionDungeonItems {
	RANDO_OPTION_DUNGEON_ITEMS_STANDARD,
	RANDO_OPTION_DUNGEON_ITEMS_MC,
	RANDO_OPTION_DUNGEON_ITEMS_MCS,
	RANDO_OPTION_DUNGEON_ITEMS_FULL,
} RandoOptionDungeonItems;

typedef enum RandoOptionAccessibility {
	RANDO_OPTION_ACCESSIBILITY_ITEMS,
	RANDO_OPTION_ACCESSIBILITY_LOCATIONS,
	RANDO_OPTION_ACCESSIBILITY_NONE,
} RandoOptionAccessibility;

typedef enum RandoOptionGoal {
	RANDO_OPTION_GOAL_GANON,
	RANDO_OPTION_GOAL_FAST_GANON,
	RANDO_OPTION_GOAL_DUNGEONS,
	RANDO_OPTION_GOAL_PEDESTAL,
	RANDO_OPTION_GOAL_TRIFORCE_HUNT,
	RANDO_OPTION_GOAL_GANONHUNT,
	RANDO_OPTION_GOAL_COMPLETIONIST,
} RandoOptionGoal;

/* A value below RANDO_CRYSTALS_RANDOM is the count itself. */
typedef enum RandoCrystals {
	RANDO_CRYSTALS_0,
	RANDO_CRYSTALS_1,
	RANDO_CRYSTALS_2,
	RANDO_CRYSTALS_3,
	RANDO_CRYSTALS_4,
	RANDO_CRYSTALS_5,
	RANDO_CRYSTALS_6,
	RANDO_CRYSTALS_7,
	RANDO_CRYSTALS_RANDOM,
} RandoCrystals;

typedef enum RandoOptionState {
	RANDO_OPTION_STATE_OPEN,
	RANDO_OPTION_STATE_STANDARD,
	RANDO_OPTION_STATE_INVERTED,
	RANDO_OPTION_STATE_RETRO,
} RandoOptionState;

typedef enum RandoOptionWeapons {
	RANDO_OPTION_WEAPONS_RANDOMIZED,
	RANDO_OPTION_WEAPONS_ASSURED,
	RANDO_OPTION_WEAPONS_VANILLA,
	RANDO_OPTION_WEAPONS_SWORDLESS,
} RandoOptionWeapons;

typedef struct RandoOptions {
	RandoGlitches glitches;
	RandoOptionPlacement item_placement;
	RandoOptionDungeonItems dungeon_items;
	RandoOptionAccessibility accessibility;
	RandoOptionGoal goal;
	RandoCrystals crystals_tower;
	RandoCrystals crystals_ganon;
	RandoOptionState state;
	RandoOptionWeapons weapons;
	RandoItemPool item_pool;
	RandoItemFunctionality item_functionality;
} RandoOptions;

/* The baseline: VT's default preset. */
void rando_options_init(RandoOptions *options);

/* VT's key and value strings, e.g. ("goal", "triforce-hunt"). Returns false and leaves options
 * unchanged on an unknown key or value. */
bool rando_options_set(RandoOptions *options, const char *key, const char *value);

/* RANDO_OPTION_KEY_COUNT when the key is unknown. */
RandoOptionKey rando_option_key_find(const char *key);
/* Asserts key < RANDO_OPTION_KEY_COUNT. */
const char *rando_option_key_name(RandoOptionKey key);
/* The VT string of the key's enum value, e.g. (RANDO_OPTION_CRYSTALS_GANON, RANDO_CRYSTALS_RANDOM)
 * gives "random". Asserts both are in range. */
const char *rando_option_value_name(RandoOptionKey key, int value);

/* The oracle wrapper's World::factory arguments on top of rando_config_init: no World constructor
 * derivation. crystals_ganon and crystals_tower are the resolved counts, asserted in 0..7. */
void rando_config_from_options(const RandoOptions *options, int crystals_ganon, int crystals_tower,
			       RandoConfig *config);

#endif
