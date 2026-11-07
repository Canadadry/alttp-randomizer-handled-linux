#ifndef LADX_SETTINGS_H
#define LADX_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* LADXR's settings.py at 4325207e, in its order, less seed and forwardfactor. */
typedef enum LadxSettingId {
	LADX_SETTING_LOGIC,
	LADX_SETTING_ACCESSIBILITY,
	LADX_SETTING_RACE,
	LADX_SETTING_HEARTPIECE,
	LADX_SETTING_SEASHELLS,
	LADX_SETTING_HEARTCONTAINERS,
	LADX_SETTING_INSTRUMENTS,
	LADX_SETTING_TRADEQUEST,
	LADX_SETTING_WITCH,
	LADX_SETTING_ROOSTER,
	LADX_SETTING_BOOMERANG,
	LADX_SETTING_DUNGEON_KEYS,
	LADX_SETTING_NIGHTMARE_KEYS,
	LADX_SETTING_DUNGEON_BEAKS,
	LADX_SETTING_DUNGEON_MAPS,
	LADX_SETTING_RANDOMSTARTLOCATION,
	LADX_SETTING_DUNGEONSHUFFLE,
	LADX_SETTING_ENTRANCESHUFFLE,
	LADX_SETTING_SHUFFLEJUNK,
	LADX_SETTING_SHUFFLEANNOYING,
	LADX_SETTING_SHUFFLEWATER,
	LADX_SETTING_BOSS,
	LADX_SETTING_MINIBOSS,
	LADX_SETTING_ENEMIES,
	LADX_SETTING_GOAL,
	LADX_SETTING_GOALCOUNT,
	LADX_SETTING_ITEMPOOL,
	LADX_SETTING_HPMODE,
	LADX_SETTING_HARDMODE,
	LADX_SETTING_STEAL,
	LADX_SETTING_EVILSHOP,
	LADX_SETTING_BOWWOW,
	LADX_SETTING_OVERWORLD,
	LADX_SETTING_DUNGEONCHAINLENGTH,
	LADX_SETTING_OWLSTATUES,
	LADX_SETTING_KEYHOLESANITY,
	LADX_SETTING_SHOPSANITY,
	LADX_SETTING_SUPERWEAPONS,
	LADX_SETTING_QUICKSWAP,
	LADX_SETTING_TEXTMODE,
	LADX_SETTING_LOWHPBEEP,
	LADX_SETTING_NOFLASH,
	LADX_SETTING_NAGMESSAGES,
	LADX_SETTING_GFXMOD,
	LADX_SETTING_FOLLOWER,
	LADX_SETTING_LINKSPALETTE,
	LADX_SETTING_MUSIC,
	LADX_SETTING_COUNT
} LadxSettingId;

typedef enum LadxSettingCategory {
	LADX_CATEGORY_MAIN,
	LADX_CATEGORY_ITEMS,
	LADX_CATEGORY_DUNGEON_ITEMS,
	LADX_CATEGORY_ENTRANCES,
	LADX_CATEGORY_GAMEPLAY,
	LADX_CATEGORY_SPECIAL,
	LADX_CATEGORY_USER_OPTIONS,
	LADX_CATEGORY_COUNT
} LadxSettingCategory;

/* LADXR's category name ("Dungeon Items"), an identifier, not a label. */
const char *ladx_setting_category_name(LadxSettingCategory category);

/*
 * A value is an index into its setting's values, in LADXR's option order;
 * a boolean has two, 0 named "0" (false) and 1 named "1" (true).
 */
const char *ladx_setting_key(LadxSettingId id);
LadxSettingCategory ladx_setting_category(LadxSettingId id);
const char *ladx_setting_short_key(LadxSettingId id);
bool ladx_setting_is_bool(LadxSettingId id);
bool ladx_setting_aesthetic(LadxSettingId id);
int ladx_setting_value_count(LadxSettingId id);
const char *ladx_setting_value_name(LadxSettingId id, int value);
/* The option's short; "" for both values of a boolean (a true one adds its short key alone). */
const char *ladx_setting_value_short(LadxSettingId id, int value);
int ladx_setting_default(LadxSettingId id);
/* LADX_SETTING_COUNT when the setting is always shown. */
LadxSettingId ladx_setting_visible_if(LadxSettingId id);
/* Whether the controller's value shows the setting; always true without a controller. */
bool ladx_setting_visible_for(LadxSettingId id, int controller_value);
/* LADX_SETTING_COUNT when unknown. */
LadxSettingId ladx_setting_find(const char *key);
/* -1 when unknown. */
int ladx_setting_value_find(LadxSettingId id, const char *name);

/* One value per setting; copy by assignment. */
typedef struct LadxSettings {
	uint8_t value[LADX_SETTING_COUNT];
} LadxSettings;

void ladx_settings_defaults(LadxSettings *settings);

/* Every non-OK return leaves the settings untouched. */
typedef enum LadxSettingsStatus {
	LADX_SETTINGS_OK = 0,
	LADX_SETTINGS_UNKNOWN_KEY,
	LADX_SETTINGS_UNKNOWN_VALUE,
	LADX_SETTINGS_MALFORMED
} LadxSettingsStatus;

/*
 * One "key=value": the value is the text after the first '=', taken
 * literally ("key=" is the empty string). No '=' is MALFORMED; a boolean
 * takes only "0" and "1" (LADXR takes any integer).
 */
LadxSettingsStatus ladx_settings_set(LadxSettings *settings, const char *assignment);

/* LADXR's Settings.validate, in place, its warnings not printed. */
void ladx_settings_validate(LadxSettings *settings);

/* The longest getShortString of any settings, without its NUL. */
#define LADX_SETTINGS_SHORT_MAX 94

/*
 * LADXR's getShortString, NUL-terminated. Returns its length, or -1 when
 * cap is too small, buf then "" (when cap > 0).
 */
int ladx_settings_short_string(const LadxSettings *settings, char *buf, size_t cap);

typedef enum LadxReason {
	LADX_REASON_NONE,
	LADX_REASON_TIER2_ENTRANCES,
	LADX_REASON_TIER3_OVERWORLD,
	LADX_REASON_TIER3_SHOPSANITY,
	LADX_REASON_GFXMOD_LICENCE,
	LADX_REASON_MUSIC_FFA,
	LADX_REASON_GOAL_BINGO,
	LADX_REASON_LADXR_REFUSES,
	LADX_REASON_COUNT
} LadxReason;

/* A stable id ("tier2-entrances"), never shown: Phase 4 maps it to text. */
const char *ladx_reason_id(LadxReason reason);

/* Why the value is unsupported alone; LADX_REASON_NONE when supported. */
LadxReason ladx_setting_value_reason(LadxSettingId id, int value);

/* A value of a unsupported together with b's; b is LADX_SETTING_COUNT for a single value. */
typedef struct LadxSettingPair {
	LadxSettingId a;
	int value_a;
	LadxSettingId b;
	int value_b;
	LadxReason reason;
} LadxSettingPair;

int ladx_setting_pair_count(void);
/* NULL out of range. */
const LadxSettingPair *ladx_setting_pair(int i);

/*
 * Whether validated settings are supported; when not, *finding (if not
 * NULL) is the first unsupported single value in setting order, else the
 * first pair in list order. Validate first: this does not.
 */
bool ladx_settings_supported(const LadxSettings *settings, LadxSettingPair *finding);

#endif
