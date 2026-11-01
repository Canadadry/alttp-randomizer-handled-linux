#include "rando_options.h"

#include <assert.h>
#include <string.h>

void rando_options_init(RandoOptions *options)
{
	memset(options, 0, sizeof *options);
	options->crystals_tower = RANDO_CRYSTALS_7;
	options->crystals_ganon = RANDO_CRYSTALS_7;
}

static const RandoLogic logic_of[] = {
	[RANDO_GLITCHES_NONE] = RANDO_LOGIC_NO_GLITCHES,
	[RANDO_GLITCHES_OVERWORLD_GLITCHES] = RANDO_LOGIC_OVERWORLD_GLITCHES,
	[RANDO_GLITCHES_HYBRID_MAJOR_GLITCHES] = RANDO_LOGIC_HYBRID_MAJOR_GLITCHES,
	[RANDO_GLITCHES_MAJOR_GLITCHES] = RANDO_LOGIC_MAJOR_GLITCHES,
	[RANDO_GLITCHES_NO_LOGIC] = RANDO_LOGIC_NO_LOGIC,
};

static const RandoItemPlacement placement_of[] = {
	[RANDO_OPTION_PLACEMENT_ADVANCED] = RANDO_PLACEMENT_ADVANCED,
	[RANDO_OPTION_PLACEMENT_BASIC] = RANDO_PLACEMENT_BASIC,
};

static const RandoDungeonItems dungeon_items_of[] = {
	[RANDO_OPTION_DUNGEON_ITEMS_STANDARD] = RANDO_DUNGEON_ITEMS_STANDARD,
	[RANDO_OPTION_DUNGEON_ITEMS_MC] = RANDO_DUNGEON_ITEMS_MC,
	[RANDO_OPTION_DUNGEON_ITEMS_MCS] = RANDO_DUNGEON_ITEMS_MCS,
	[RANDO_OPTION_DUNGEON_ITEMS_FULL] = RANDO_DUNGEON_ITEMS_FULL,
};

static const RandoAccessibility accessibility_of[] = {
	[RANDO_OPTION_ACCESSIBILITY_ITEMS] = RANDO_ACCESSIBILITY_ITEMS,
	[RANDO_OPTION_ACCESSIBILITY_LOCATIONS] = RANDO_ACCESSIBILITY_LOCATIONS,
	[RANDO_OPTION_ACCESSIBILITY_NONE] = RANDO_ACCESSIBILITY_NONE,
};

static const RandoGoal goal_of[] = {
	[RANDO_OPTION_GOAL_GANON] = RANDO_GOAL_GANON,
	[RANDO_OPTION_GOAL_FAST_GANON] = RANDO_GOAL_FAST_GANON,
	[RANDO_OPTION_GOAL_DUNGEONS] = RANDO_GOAL_DUNGEONS,
	[RANDO_OPTION_GOAL_PEDESTAL] = RANDO_GOAL_PEDESTAL,
	[RANDO_OPTION_GOAL_TRIFORCE_HUNT] = RANDO_GOAL_TRIFORCE_HUNT,
	[RANDO_OPTION_GOAL_GANONHUNT] = RANDO_GOAL_GANONHUNT,
	[RANDO_OPTION_GOAL_COMPLETIONIST] = RANDO_GOAL_COMPLETIONIST,
};

static const RandoState state_of[] = {
	[RANDO_OPTION_STATE_OPEN] = RANDO_STATE_OPEN,
	[RANDO_OPTION_STATE_STANDARD] = RANDO_STATE_STANDARD,
	[RANDO_OPTION_STATE_INVERTED] = RANDO_STATE_INVERTED,
	[RANDO_OPTION_STATE_RETRO] = RANDO_STATE_RETRO,
};

static const RandoWeapons weapons_of[] = {
	[RANDO_OPTION_WEAPONS_RANDOMIZED] = RANDO_WEAPONS_RANDOMIZED,
	[RANDO_OPTION_WEAPONS_ASSURED] = RANDO_WEAPONS_ASSURED,
	[RANDO_OPTION_WEAPONS_VANILLA] = RANDO_WEAPONS_VANILLA,
	[RANDO_OPTION_WEAPONS_SWORDLESS] = RANDO_WEAPONS_SWORDLESS,
};

void rando_config_from_options(const RandoOptions *o, int crystals_ganon, int crystals_tower, RandoConfig *c)
{
	assert(crystals_ganon >= 0 && crystals_ganon <= 7 && crystals_tower >= 0 && crystals_tower <= 7);
	rando_config_init(c);
	c->logic = logic_of[o->glitches];
	c->item_placement = o->item_pool == RANDO_ITEM_POOL_CROWD_CONTROL ? RANDO_PLACEMENT_ADVANCED
									  : placement_of[o->item_placement];
	c->dungeon_items = dungeon_items_of[o->dungeon_items];
	c->accessibility = accessibility_of[o->accessibility];
	c->goal = goal_of[o->goal];
	c->mode_state = state_of[o->state];
	c->mode_weapons = weapons_of[o->weapons];
	c->crystals_ganon = crystals_ganon;
	c->crystals_tower = crystals_tower;
	c->item_pool = o->item_pool;
	c->item_functionality = o->item_functionality;
}

static const char *const glitches_names[] = {"none", "overworld_glitches", "hybrid_major_glitches", "major_glitches",
					     "no_logic", NULL};
static const char *const placement_names[] = {"advanced", "basic", NULL};
static const char *const dungeon_items_names[] = {"standard", "mc", "mcs", "full", NULL};
static const char *const accessibility_names[] = {"items", "locations", "none", NULL};
static const char *const goal_names[] = {"ganon", "fast_ganon", "dungeons", "pedestal", "triforce-hunt", "ganonhunt",
					 "completionist", NULL};
static const char *const crystals_names[] = {"0", "1", "2", "3", "4", "5", "6", "7", "random", NULL};
static const char *const state_names[] = {"open", "standard", "inverted", "retro", NULL};
static const char *const weapons_names[] = {"randomized", "assured", "vanilla", "swordless", NULL};
static const char *const item_pool_names[] = {"normal", "hard", "expert", "crowd_control", NULL};
static const char *const item_functionality_names[] = {"normal", "hard", "expert", NULL};

typedef struct OptionKeyInfo {
	const char *name;
	const char *const *values;
} OptionKeyInfo;

static const OptionKeyInfo keys[RANDO_OPTION_KEY_COUNT] = {
	[RANDO_OPTION_GLITCHES] = {"glitches", glitches_names},
	[RANDO_OPTION_ITEM_PLACEMENT] = {"item_placement", placement_names},
	[RANDO_OPTION_DUNGEON_ITEMS] = {"dungeon_items", dungeon_items_names},
	[RANDO_OPTION_ACCESSIBILITY] = {"accessibility", accessibility_names},
	[RANDO_OPTION_GOAL] = {"goal", goal_names},
	[RANDO_OPTION_CRYSTALS_TOWER] = {"crystals_tower", crystals_names},
	[RANDO_OPTION_CRYSTALS_GANON] = {"crystals_ganon", crystals_names},
	[RANDO_OPTION_STATE] = {"state", state_names},
	[RANDO_OPTION_WEAPONS] = {"weapons", weapons_names},
	[RANDO_OPTION_ITEM_POOL] = {"item_pool", item_pool_names},
	[RANDO_OPTION_ITEM_FUNCTIONALITY] = {"item_functionality", item_functionality_names},
};

RandoOptionKey rando_option_key_find(const char *key)
{
	int k;

	for (k = 0; k < RANDO_OPTION_KEY_COUNT; k++)
		if (strcmp(keys[k].name, key) == 0)
			return (RandoOptionKey)k;
	return RANDO_OPTION_KEY_COUNT;
}

const char *rando_option_key_name(RandoOptionKey key)
{
	assert(key < RANDO_OPTION_KEY_COUNT);
	return keys[key].name;
}

const char *rando_option_value_name(RandoOptionKey key, int value)
{
	int v;

	assert(key < RANDO_OPTION_KEY_COUNT && value >= 0);
	for (v = 0; v < value; v++)
		assert(keys[key].values[v] != NULL && "option value out of range");
	assert(keys[key].values[value] != NULL && "option value out of range");
	return keys[key].values[value];
}

bool rando_options_set(RandoOptions *o, const char *key, const char *value)
{
	RandoOptionKey k = rando_option_key_find(key);
	int v;

	if (k == RANDO_OPTION_KEY_COUNT)
		return false;
	for (v = 0; keys[k].values[v] != NULL; v++)
		if (strcmp(keys[k].values[v], value) == 0)
			break;
	if (keys[k].values[v] == NULL)
		return false;
	switch (k) {
	case RANDO_OPTION_GLITCHES: o->glitches = (RandoGlitches)v; break;
	case RANDO_OPTION_ITEM_PLACEMENT: o->item_placement = (RandoOptionPlacement)v; break;
	case RANDO_OPTION_DUNGEON_ITEMS: o->dungeon_items = (RandoOptionDungeonItems)v; break;
	case RANDO_OPTION_ACCESSIBILITY: o->accessibility = (RandoOptionAccessibility)v; break;
	case RANDO_OPTION_GOAL: o->goal = (RandoOptionGoal)v; break;
	case RANDO_OPTION_CRYSTALS_TOWER: o->crystals_tower = (RandoCrystals)v; break;
	case RANDO_OPTION_CRYSTALS_GANON: o->crystals_ganon = (RandoCrystals)v; break;
	case RANDO_OPTION_STATE: o->state = (RandoOptionState)v; break;
	case RANDO_OPTION_WEAPONS: o->weapons = (RandoOptionWeapons)v; break;
	case RANDO_OPTION_ITEM_POOL: o->item_pool = (RandoItemPool)v; break;
	case RANDO_OPTION_ITEM_FUNCTIONALITY: o->item_functionality = (RandoItemFunctionality)v; break;
	case RANDO_OPTION_KEY_COUNT: break;
	}
	return true;
}
