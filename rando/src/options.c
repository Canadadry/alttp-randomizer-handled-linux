#include "rando_options.h"
#include "rando_settings.h"

#include <assert.h>
#include <string.h>

void rando_options_init(RandoOptions *options)
{
	memset(options, 0, sizeof *options);
	options->crystals_tower = RANDO_CRYSTALS_7;
	options->crystals_ganon = RANDO_CRYSTALS_7;
	options->hints = true;
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
	c->spoil_hints = o->hints;
}

RandoOptionKey rando_option_key_find(const char *key)
{
	RandoSettingId id = rando_setting_find(key);

	return id < (RandoSettingId)RANDO_OPTION_KEY_COUNT ? (RandoOptionKey)id : RANDO_OPTION_KEY_COUNT;
}

const char *rando_option_key_name(RandoOptionKey key)
{
	assert(key < RANDO_OPTION_KEY_COUNT);
	return rando_setting_key((RandoSettingId)key);
}

const char *rando_option_value_name(RandoOptionKey key, int value)
{
	assert(key < RANDO_OPTION_KEY_COUNT);
	return rando_setting_value_name((RandoSettingId)key, value);
}

const char *rando_hints_name(bool hints)
{
	return rando_setting_value_name(RANDO_SETTING_HINTS, hints ? 0 : 1);
}

bool rando_hints_find(const char *name, bool *out)
{
	int v = rando_setting_value_find(RANDO_SETTING_HINTS, name);

	if (v < 0)
		return false;
	*out = v == 0;
	return true;
}

bool rando_options_set(RandoOptions *o, const char *key, const char *value)
{
	RandoOptionKey k = rando_option_key_find(key);
	RandoHeartBeep unused = RANDO_HEART_BEEP_HALF;
	int v;

	if (k == RANDO_OPTION_KEY_COUNT)
		return false;
	v = rando_setting_value_find((RandoSettingId)k, value);
	if (v < 0)
		return false;
	rando_setting_set(o, &unused, (RandoSettingId)k, v);
	return true;
}

int rando_options_get(const RandoOptions *o, RandoOptionKey key)
{
	assert(key < RANDO_OPTION_KEY_COUNT && "option key out of range");
	return rando_setting_get(o, RANDO_HEART_BEEP_HALF, (RandoSettingId)key);
}
