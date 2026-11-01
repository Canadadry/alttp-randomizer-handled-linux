#ifndef RANDO_SETTINGS_H
#define RANDO_SETTINGS_H

#include "rando_options.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RandoHeartBeep {
	RANDO_HEART_BEEP_OFF,
	RANDO_HEART_BEEP_HALF,
	RANDO_HEART_BEEP_QUARTER,
	RANDO_HEART_BEEP_DOUBLE,
	RANDO_HEART_BEEP_NORMAL,
	RANDO_HEART_BEEP_COUNT,
} RandoHeartBeep;

const char *rando_heart_beep_name(RandoHeartBeep heart_beep);
bool rando_heart_beep_find(const char *name, RandoHeartBeep *out);

typedef enum RandoSettingId {
	RANDO_SETTING_GLITCHES = RANDO_OPTION_GLITCHES,
	RANDO_SETTING_ITEM_PLACEMENT,
	RANDO_SETTING_DUNGEON_ITEMS,
	RANDO_SETTING_ACCESSIBILITY,
	RANDO_SETTING_GOAL,
	RANDO_SETTING_CRYSTALS_TOWER,
	RANDO_SETTING_CRYSTALS_GANON,
	RANDO_SETTING_STATE,
	RANDO_SETTING_WEAPONS,
	RANDO_SETTING_ITEM_POOL,
	RANDO_SETTING_ITEM_FUNCTIONALITY,
	RANDO_SETTING_HINTS,
	RANDO_SETTING_HEART_BEEP,
	RANDO_SETTING_COUNT,
} RandoSettingId;

const char *rando_setting_key(RandoSettingId id);
int rando_setting_value_count(RandoSettingId id);
const char *rando_setting_value_name(RandoSettingId id, int value);
RandoSettingId rando_setting_find(const char *key);
int rando_setting_value_find(RandoSettingId id, const char *name);
int rando_setting_default(RandoSettingId id);
int rando_setting_get(const RandoOptions *options, RandoHeartBeep heart_beep, RandoSettingId id);
void rando_setting_set(RandoOptions *options, RandoHeartBeep *heart_beep, RandoSettingId id, int value);
RandoSettingId rando_setting_sorted(int rank);

bool rando_parse_uint32(const char *text, size_t len, uint32_t *out);

typedef struct RandoSettingPair {
	RandoSettingId a;
	int value_a;
	RandoSettingId b;
	int value_b;
} RandoSettingPair;

int rando_setting_unsupported_count(void);
const RandoSettingPair *rando_setting_unsupported(int i);

#endif
