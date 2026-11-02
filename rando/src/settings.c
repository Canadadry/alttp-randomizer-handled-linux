#include "rando_settings.h"

#include <assert.h>
#include <string.h>

typedef char rando_setting_ids_follow_option_keys[(RANDO_OPTION_KEY_COUNT == 11 &&
							(int)RANDO_SETTING_ITEM_FUNCTIONALITY ==
								(int)RANDO_OPTION_ITEM_FUNCTIONALITY &&
							(int)RANDO_SETTING_HINTS == (int)RANDO_OPTION_KEY_COUNT)
							   ? 1
							   : -1];

static const char *const glitches_names[] = {"none", "overworld_glitches", "hybrid_major_glitches", "major_glitches",
					     "no_logic"};
static const char *const placement_names[] = {"advanced", "basic"};
static const char *const dungeon_items_names[] = {"standard", "mc", "mcs", "full"};
static const char *const accessibility_names[] = {"items", "locations", "none"};
static const char *const goal_names[] = {"ganon",	"fast_ganon", "dungeons", "pedestal",
					 "triforce-hunt", "ganonhunt",	"completionist"};
static const char *const crystals_names[] = {"0", "1", "2", "3", "4", "5", "6", "7", "random"};
static const char *const state_names[] = {"open", "standard", "inverted", "retro"};
static const char *const weapons_names[] = {"randomized", "assured", "vanilla", "swordless"};
static const char *const item_pool_names[] = {"normal", "hard", "expert", "crowd_control"};
static const char *const item_functionality_names[] = {"normal", "hard", "expert"};
static const char *const hints_names[] = {"on", "off"};
static const char *const heart_beep_names[RANDO_HEART_BEEP_COUNT] = {
	[RANDO_HEART_BEEP_OFF] = "off",		[RANDO_HEART_BEEP_HALF] = "half",
	[RANDO_HEART_BEEP_QUARTER] = "quarter", [RANDO_HEART_BEEP_DOUBLE] = "double",
	[RANDO_HEART_BEEP_NORMAL] = "normal",
};

typedef struct SettingInfo {
	const char *key;
	const char *const *values;
	int count;
	int default_value;
} SettingInfo;

#define VALUES(names) names, (int)(sizeof names / sizeof names[0])

static const SettingInfo settings[RANDO_SETTING_COUNT] = {
	[RANDO_SETTING_GLITCHES] = {"glitches", VALUES(glitches_names), RANDO_GLITCHES_NONE},
	[RANDO_SETTING_ITEM_PLACEMENT] = {"item_placement", VALUES(placement_names), RANDO_OPTION_PLACEMENT_ADVANCED},
	[RANDO_SETTING_DUNGEON_ITEMS] = {"dungeon_items", VALUES(dungeon_items_names),
					 RANDO_OPTION_DUNGEON_ITEMS_STANDARD},
	[RANDO_SETTING_ACCESSIBILITY] = {"accessibility", VALUES(accessibility_names),
					 RANDO_OPTION_ACCESSIBILITY_ITEMS},
	[RANDO_SETTING_GOAL] = {"goal", VALUES(goal_names), RANDO_OPTION_GOAL_GANON},
	[RANDO_SETTING_CRYSTALS_TOWER] = {"crystals_tower", VALUES(crystals_names), RANDO_CRYSTALS_7},
	[RANDO_SETTING_CRYSTALS_GANON] = {"crystals_ganon", VALUES(crystals_names), RANDO_CRYSTALS_7},
	[RANDO_SETTING_STATE] = {"state", VALUES(state_names), RANDO_OPTION_STATE_OPEN},
	[RANDO_SETTING_WEAPONS] = {"weapons", VALUES(weapons_names), RANDO_OPTION_WEAPONS_RANDOMIZED},
	[RANDO_SETTING_ITEM_POOL] = {"item_pool", VALUES(item_pool_names), RANDO_ITEM_POOL_NORMAL},
	[RANDO_SETTING_ITEM_FUNCTIONALITY] = {"item_functionality", VALUES(item_functionality_names),
					      RANDO_ITEM_FUNCTIONALITY_NORMAL},
	[RANDO_SETTING_HINTS] = {"hints", VALUES(hints_names), 0},
	[RANDO_SETTING_HEART_BEEP] = {"heart_beep", VALUES(heart_beep_names), RANDO_HEART_BEEP_HALF},
};

static const SettingInfo *info(RandoSettingId id)
{
	assert((unsigned)id < RANDO_SETTING_COUNT);
	return &settings[id];
}

const char *rando_setting_key(RandoSettingId id)
{
	return info(id)->key;
}

int rando_setting_value_count(RandoSettingId id)
{
	return info(id)->count;
}

const char *rando_setting_value_name(RandoSettingId id, int value)
{
	assert(value >= 0 && value < info(id)->count && "setting value out of range");
	return info(id)->values[value];
}

RandoSettingId rando_setting_find(const char *key)
{
	int id;

	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		if (strcmp(settings[id].key, key) == 0)
			return (RandoSettingId)id;
	return RANDO_SETTING_COUNT;
}

int rando_setting_value_find(RandoSettingId id, const char *name)
{
	int v;

	for (v = 0; v < info(id)->count; v++)
		if (strcmp(settings[id].values[v], name) == 0)
			return v;
	return -1;
}

int rando_setting_default(RandoSettingId id)
{
	return info(id)->default_value;
}

int rando_setting_get(const RandoOptions *o, RandoHeartBeep heart_beep, RandoSettingId id)
{
	switch (id) {
	case RANDO_SETTING_GLITCHES: return (int)o->glitches;
	case RANDO_SETTING_ITEM_PLACEMENT: return (int)o->item_placement;
	case RANDO_SETTING_DUNGEON_ITEMS: return (int)o->dungeon_items;
	case RANDO_SETTING_ACCESSIBILITY: return (int)o->accessibility;
	case RANDO_SETTING_GOAL: return (int)o->goal;
	case RANDO_SETTING_CRYSTALS_TOWER: return (int)o->crystals_tower;
	case RANDO_SETTING_CRYSTALS_GANON: return (int)o->crystals_ganon;
	case RANDO_SETTING_STATE: return (int)o->state;
	case RANDO_SETTING_WEAPONS: return (int)o->weapons;
	case RANDO_SETTING_ITEM_POOL: return (int)o->item_pool;
	case RANDO_SETTING_ITEM_FUNCTIONALITY: return (int)o->item_functionality;
	case RANDO_SETTING_HINTS: return o->hints ? 0 : 1;
	case RANDO_SETTING_HEART_BEEP:
		assert((unsigned)heart_beep < RANDO_HEART_BEEP_COUNT);
		return (int)heart_beep;
	case RANDO_SETTING_COUNT: break;
	}
	assert(!"setting out of range");
	return -1;
}

void rando_setting_set(RandoOptions *o, RandoHeartBeep *heart_beep, RandoSettingId id, int v)
{
	assert(v >= 0 && v < info(id)->count && "setting value out of range");
	switch (id) {
	case RANDO_SETTING_GLITCHES: o->glitches = (RandoGlitches)v; break;
	case RANDO_SETTING_ITEM_PLACEMENT: o->item_placement = (RandoOptionPlacement)v; break;
	case RANDO_SETTING_DUNGEON_ITEMS: o->dungeon_items = (RandoOptionDungeonItems)v; break;
	case RANDO_SETTING_ACCESSIBILITY: o->accessibility = (RandoOptionAccessibility)v; break;
	case RANDO_SETTING_GOAL: o->goal = (RandoOptionGoal)v; break;
	case RANDO_SETTING_CRYSTALS_TOWER: o->crystals_tower = (RandoCrystals)v; break;
	case RANDO_SETTING_CRYSTALS_GANON: o->crystals_ganon = (RandoCrystals)v; break;
	case RANDO_SETTING_STATE: o->state = (RandoOptionState)v; break;
	case RANDO_SETTING_WEAPONS: o->weapons = (RandoOptionWeapons)v; break;
	case RANDO_SETTING_ITEM_POOL: o->item_pool = (RandoItemPool)v; break;
	case RANDO_SETTING_ITEM_FUNCTIONALITY: o->item_functionality = (RandoItemFunctionality)v; break;
	case RANDO_SETTING_HINTS: o->hints = v == 0; break;
	case RANDO_SETTING_HEART_BEEP: *heart_beep = (RandoHeartBeep)v; break;
	case RANDO_SETTING_COUNT: break;
	}
}

RandoSettingId rando_setting_sorted(int rank)
{
	int id, other;

	assert(rank >= 0 && rank < RANDO_SETTING_COUNT);
	for (id = 0; id < RANDO_SETTING_COUNT; id++) {
		int below = 0;

		for (other = 0; other < RANDO_SETTING_COUNT; other++)
			if (strcmp(settings[other].key, settings[id].key) < 0)
				below++;
		if (below == rank)
			return (RandoSettingId)id;
	}
	assert(!"setting keys not distinct");
	return RANDO_SETTING_COUNT;
}

const char *rando_heart_beep_name(RandoHeartBeep heart_beep)
{
	return rando_setting_value_name(RANDO_SETTING_HEART_BEEP, (int)heart_beep);
}

bool rando_heart_beep_find(const char *name, RandoHeartBeep *out)
{
	int v = rando_setting_value_find(RANDO_SETTING_HEART_BEEP, name);

	if (v < 0)
		return false;
	*out = (RandoHeartBeep)v;
	return true;
}

bool rando_parse_uint32(const char *text, size_t len, uint32_t *out)
{
	size_t digits = len > 0 && text[len - 1] == '\n' ? len - 1 : len;
	unsigned long long v = 0;
	size_t i;

	if (digits < 1 || digits > 10)
		return false;
	for (i = 0; i < digits; i++) {
		if (text[i] < '0' || text[i] > '9')
			return false;
		v = v * 10 + (unsigned)(text[i] - '0');
	}
	if (v > 0xFFFFFFFFull)
		return false;
	*out = (uint32_t)v;
	return true;
}

/* The final row is a sentinel, not a pair: C99 has no empty array. */
static const RandoSettingPair unsupported[] = {
	{RANDO_SETTING_COUNT, 0, RANDO_SETTING_COUNT, 0},
};

int rando_setting_unsupported_count(void)
{
	return (int)(sizeof unsupported / sizeof unsupported[0]) - 1;
}

const RandoSettingPair *rando_setting_unsupported(int i)
{
	assert(i >= 0 && i < rando_setting_unsupported_count());
	return &unsupported[i];
}
