#ifndef LADX_SETTINGS_H
#define LADX_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

const char *ladx_setting_category_name(LadxSettingCategory category);

const char *ladx_setting_key(LadxSettingId id);
LadxSettingCategory ladx_setting_category(LadxSettingId id);
const char *ladx_setting_short_key(LadxSettingId id);
bool ladx_setting_is_bool(LadxSettingId id);
bool ladx_setting_aesthetic(LadxSettingId id);
int ladx_setting_value_count(LadxSettingId id);
const char *ladx_setting_value_name(LadxSettingId id, int value);
const char *ladx_setting_value_short(LadxSettingId id, int value);
int ladx_setting_default(LadxSettingId id);
LadxSettingId ladx_setting_visible_if(LadxSettingId id);
bool ladx_setting_visible_for(LadxSettingId id, int controller_value);
LadxSettingId ladx_setting_find(const char *key);
int ladx_setting_value_find(LadxSettingId id, const char *name);

typedef struct LadxSettings {
	uint8_t value[LADX_SETTING_COUNT];
} LadxSettings;

void ladx_settings_defaults(LadxSettings *settings);

typedef enum LadxSettingsStatus {
	LADX_SETTINGS_OK = 0,
	LADX_SETTINGS_UNKNOWN_KEY,
	LADX_SETTINGS_UNKNOWN_VALUE,
	LADX_SETTINGS_MALFORMED
} LadxSettingsStatus;

LadxSettingsStatus ladx_settings_set(LadxSettings *settings, const char *assignment);

void ladx_settings_validate(LadxSettings *settings);

#define LADX_SETTINGS_SHORT_MAX 94

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

const char *ladx_reason_id(LadxReason reason);

LadxReason ladx_setting_value_reason(LadxSettingId id, int value);

typedef struct LadxSettingPair {
	LadxSettingId a;
	int value_a;
	LadxSettingId b;
	int value_b;
	LadxReason reason;
} LadxSettingPair;

int ladx_setting_pair_count(void);
const LadxSettingPair *ladx_setting_pair(int i);

bool ladx_settings_supported(const LadxSettings *settings, LadxSettingPair *finding);

#endif
