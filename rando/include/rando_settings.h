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

/* The case format's name: "off", "half", "quarter", "double", "normal". Asserts the range. */
const char *rando_heart_beep_name(RandoHeartBeep heart_beep);
/* Returns false and leaves *out unchanged on an unknown name. */
bool rando_heart_beep_find(const char *name, RandoHeartBeep *out);

/* The settings of a seed: the RandoOptionKey values first, then hints, then heart beep. */
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

/* A value index is the option's or heart beep's enum value; for hints 0 is "on", 1 "off". Every function
 * asserts its id and value ranges; names are static strings, never freed. */
const char *rando_setting_key(RandoSettingId id);
int rando_setting_value_count(RandoSettingId id);
const char *rando_setting_value_name(RandoSettingId id, int value);
/* RANDO_SETTING_COUNT when the key is unknown. */
RandoSettingId rando_setting_find(const char *key);
/* -1 when the name is not one of id's values. */
int rando_setting_value_find(RandoSettingId id, const char *name);
/* rando_options_init's value, heart beep half. */
int rando_setting_default(RandoSettingId id);
int rando_setting_get(const RandoOptions *options, RandoHeartBeep heart_beep, RandoSettingId id);
void rando_setting_set(RandoOptions *options, RandoHeartBeep *heart_beep, RandoSettingId id, int value);
/* The setting whose key is the rank-th in bytewise order, rank in 0..RANDO_SETTING_COUNT - 1. */
RandoSettingId rando_setting_sorted(int rank);

/* `^[0-9]{1,10}\n?$` over text[0, len), at most 2^32 - 1. Leaves *out unchanged on refusal. */
bool rando_parse_uint32(const char *text, size_t len, uint32_t *out);

/* Unsupported when setting a holds value_a and setting b holds value_b at once. */
typedef struct RandoSettingPair {
	RandoSettingId a;
	int value_a;
	RandoSettingId b;
	int value_b;
} RandoSettingPair;

int rando_setting_unsupported_count(void);
/* Asserts i < rando_setting_unsupported_count(). */
const RandoSettingPair *rando_setting_unsupported(int i);

#endif
