#include "ui_ladx_settings.h"

#include <assert.h>

typedef struct Row {
	UiStrId label, help;
	const UiStrId *values;
	int count;
} Row;

#define ROW(key, values) \
	[LADX_SETTING_##key] = {UI_STR_LADX_SET_##key, UI_STR_LADX_HELP_##key, values, (int)(sizeof values / sizeof values[0])}

static const UiStrId values_logic[] = {UI_STR_LADX_VAL_CASUAL, UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_HARD, UI_STR_LADX_VAL_GLITCHED, UI_STR_LADX_VAL_HELL};
static const UiStrId values_accessibility[] = {UI_STR_LADX_VAL_ALL, UI_STR_LADX_VAL_BEATABLE};
static const UiStrId values_race[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_heartpiece[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_seashells[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_heartcontainers[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_instruments[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_tradequest[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_witch[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_rooster[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_boomerang[] = {UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_TRADE, UI_STR_LADX_VAL_GIFT};
static const UiStrId values_dungeon_keys[] = {UI_STR_LADX_VAL_STANDARD, UI_STR_LADX_VAL_KEYSANITY, UI_STR_LADX_VAL_REMOVED};
static const UiStrId values_nightmare_keys[] = {UI_STR_LADX_VAL_STANDARD, UI_STR_LADX_VAL_KEYSANITY, UI_STR_LADX_VAL_REMOVED};
static const UiStrId values_dungeon_beaks[] = {UI_STR_LADX_VAL_STANDARD, UI_STR_LADX_VAL_KEYSANITY, UI_STR_LADX_VAL_REMOVED};
static const UiStrId values_dungeon_maps[] = {UI_STR_LADX_VAL_STANDARD, UI_STR_LADX_VAL_KEYSANITY, UI_STR_LADX_VAL_REMOVED};
static const UiStrId values_randomstartlocation[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_dungeonshuffle[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_entranceshuffle[] = {UI_STR_LADX_VAL_DEFAULT, UI_STR_LADX_VAL_SIMPLE, UI_STR_LADX_VAL_SPLIT, UI_STR_LADX_VAL_MIXED, UI_STR_LADX_VAL_WILD, UI_STR_LADX_VAL_CHAOS, UI_STR_LADX_VAL_INSANE, UI_STR_LADX_VAL_MADNESS};
static const UiStrId values_shufflejunk[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_shuffleannoying[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_shufflewater[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_boss[] = {UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_SHUFFLE, UI_STR_LADX_VAL_RANDOMIZE};
static const UiStrId values_miniboss[] = {UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_SHUFFLE, UI_STR_LADX_VAL_RANDOMIZE};
static const UiStrId values_enemies[] = {UI_STR_LADX_VAL_NONE, UI_STR_LADX_VAL_OVERWORLD};
static const UiStrId values_goal[] = {UI_STR_LADX_VAL_VANILLA, UI_STR_LADX_VAL_X_INSTRUMENTS, UI_STR_LADX_VAL_X_SPECIFIC, UI_STR_LADX_VAL_EGG_OPEN, UI_STR_LADX_VAL_SHORT_GAME, UI_STR_LADX_VAL_LONG_GAME, UI_STR_LADX_VAL_SEASHELL_HUNT, UI_STR_LADX_VAL_BINGO, UI_STR_LADX_VAL_BINGO_DOUBLE, UI_STR_LADX_VAL_BINGO_TRIPLE, UI_STR_LADX_VAL_BINGO_FULL, UI_STR_LADX_VAL_SIGN_MAZE};
static const UiStrId values_goalcount[] = {UI_STR_LADX_VAL_N7, UI_STR_LADX_VAL_N6, UI_STR_LADX_VAL_N5, UI_STR_LADX_VAL_N4, UI_STR_LADX_VAL_N3, UI_STR_LADX_VAL_N2, UI_STR_LADX_VAL_N1, UI_STR_LADX_VAL_N0, UI_STR_LADX_VAL_RANDOM};
static const UiStrId values_itempool[] = {UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_CASUAL, UI_STR_LADX_VAL_PATH_OF_PAIN, UI_STR_LADX_VAL_MORE_KEYS};
static const UiStrId values_hpmode[] = {UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_INVERTED, UI_STR_LADX_VAL_ONE_HEART, UI_STR_LADX_VAL_LOW_MAX, UI_STR_LADX_VAL_FIVE_HIT};
static const UiStrId values_hardmode[] = {UI_STR_LADX_VAL_DISABLED, UI_STR_LADX_VAL_ORACLE, UI_STR_LADX_VAL_HERO, UI_STR_LADX_VAL_ONE_HIT_KO};
static const UiStrId values_steal[] = {UI_STR_LADX_VAL_ALWAYS, UI_STR_LADX_VAL_NEVER, UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_GGS};
static const UiStrId values_evilshop[] = {UI_STR_LADX_VAL_DISABLED, UI_STR_LADX_VAL_ENABLED};
static const UiStrId values_bowwow[] = {UI_STR_LADX_VAL_DISABLED, UI_STR_LADX_VAL_ENABLED, UI_STR_LADX_VAL_SWORDLESS};
static const UiStrId values_overworld[] = {UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_DUNGEON_DIVE, UI_STR_LADX_VAL_NO_DUNGEONS, UI_STR_LADX_VAL_DUNGEON_CHAIN, UI_STR_LADX_VAL_RANDOMIZED, UI_STR_LADX_VAL_ALTTP};
static const UiStrId values_dungeonchainlength[] = {UI_STR_LADX_VAL_N3, UI_STR_LADX_VAL_N4, UI_STR_LADX_VAL_N5, UI_STR_LADX_VAL_N6, UI_STR_LADX_VAL_N7, UI_STR_LADX_VAL_N8};
static const UiStrId values_owlstatues[] = {UI_STR_LADX_VAL_NEVER, UI_STR_LADX_VAL_DUNGEONS, UI_STR_LADX_VAL_OVERWORLD, UI_STR_LADX_VAL_BOTH};
static const UiStrId values_keyholesanity[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_shopsanity[] = {UI_STR_LADX_VAL_DISABLED, UI_STR_LADX_VAL_BASIC, UI_STR_LADX_VAL_IMPORTANT};
static const UiStrId values_superweapons[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_quickswap[] = {UI_STR_LADX_VAL_DISABLED, UI_STR_LADX_VAL_SWAP_A, UI_STR_LADX_VAL_SWAP_B};
static const UiStrId values_textmode[] = {UI_STR_LADX_VAL_FAST, UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_NO_TEXT, UI_STR_LADX_VAL_NO_TEXT_BOXES};
static const UiStrId values_lowhpbeep[] = {UI_STR_LADX_VAL_DISABLED, UI_STR_LADX_VAL_SLOW, UI_STR_LADX_VAL_NORMAL};
static const UiStrId values_noflash[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_nagmessages[] = {UI_STR_LADX_VAL_OFF, UI_STR_LADX_VAL_ON};
static const UiStrId values_gfxmod[] = {UI_STR_LADX_VAL_DEFAULT, UI_STR_LADX_VAL_GFX_AGESGIRL, UI_STR_LADX_VAL_GFX_BOWWOW, UI_STR_LADX_VAL_GFX_BRAIXEN, UI_STR_LADX_VAL_GFX_BUNNY, UI_STR_LADX_VAL_GFX_GRANDMAULRIRA, UI_STR_LADX_VAL_GFX_KIRBY, UI_STR_LADX_VAL_GFX_KRIS, UI_STR_LADX_VAL_GFX_LUCKWURSTJOE, UI_STR_LADX_VAL_GFX_LUIGI, UI_STR_LADX_VAL_GFX_MMLINK, UI_STR_LADX_VAL_GFX_MARIN, UI_STR_LADX_VAL_GFX_MARINALPHA, UI_STR_LADX_VAL_GFX_MARIO, UI_STR_LADX_VAL_GFX_MARTHA, UI_STR_LADX_VAL_GFX_MATTY, UI_STR_LADX_VAL_GFX_MEME, UI_STR_LADX_VAL_GFX_MOBLIN, UI_STR_LADX_VAL_GFX_NESLINK, UI_STR_LADX_VAL_GFX_NINTEN, UI_STR_LADX_VAL_GFX_NOELLE, UI_STR_LADX_VAL_GFX_RALSEI, UI_STR_LADX_VAL_GFX_RICHARD, UI_STR_LADX_VAL_GFX_RICKY, UI_STR_LADX_VAL_GFX_ROOSTER, UI_STR_LADX_VAL_GFX_ROSA, UI_STR_LADX_VAL_GFX_SARIA, UI_STR_LADX_VAL_GFX_SIG, UI_STR_LADX_VAL_GFX_SPONGEBOB, UI_STR_LADX_VAL_GFX_SUBROSIAN, UI_STR_LADX_VAL_GFX_TARIN, UI_STR_LADX_VAL_GFX_X};
static const UiStrId values_follower[] = {UI_STR_LADX_VAL_NONE, UI_STR_LADX_VAL_FOX, UI_STR_LADX_VAL_NAVI, UI_STR_LADX_VAL_GHOST, UI_STR_LADX_VAL_YIPYIP};
static const UiStrId values_linkspalette[] = {UI_STR_LADX_VAL_NORMAL, UI_STR_LADX_VAL_GREEN, UI_STR_LADX_VAL_YELLOW, UI_STR_LADX_VAL_RED, UI_STR_LADX_VAL_BLUE, UI_STR_LADX_VAL_INVERTED_RED, UI_STR_LADX_VAL_INVERTED_BLUE, UI_STR_LADX_VAL_COLOUR_C, UI_STR_LADX_VAL_COLOUR_D};
static const UiStrId values_music[] = {UI_STR_LADX_VAL_DEFAULT, UI_STR_LADX_VAL_RANDOM, UI_STR_LADX_VAL_DISABLED, UI_STR_LADX_VAL_TONE_SHIFTED, UI_STR_LADX_VAL_FF_ADVENTURE};

static const Row rows[LADX_SETTING_COUNT] = {
	ROW(LOGIC, values_logic),
	ROW(ACCESSIBILITY, values_accessibility),
	ROW(RACE, values_race),
	ROW(HEARTPIECE, values_heartpiece),
	ROW(SEASHELLS, values_seashells),
	ROW(HEARTCONTAINERS, values_heartcontainers),
	ROW(INSTRUMENTS, values_instruments),
	ROW(TRADEQUEST, values_tradequest),
	ROW(WITCH, values_witch),
	ROW(ROOSTER, values_rooster),
	ROW(BOOMERANG, values_boomerang),
	ROW(DUNGEON_KEYS, values_dungeon_keys),
	ROW(NIGHTMARE_KEYS, values_nightmare_keys),
	ROW(DUNGEON_BEAKS, values_dungeon_beaks),
	ROW(DUNGEON_MAPS, values_dungeon_maps),
	ROW(RANDOMSTARTLOCATION, values_randomstartlocation),
	ROW(DUNGEONSHUFFLE, values_dungeonshuffle),
	ROW(ENTRANCESHUFFLE, values_entranceshuffle),
	ROW(SHUFFLEJUNK, values_shufflejunk),
	ROW(SHUFFLEANNOYING, values_shuffleannoying),
	ROW(SHUFFLEWATER, values_shufflewater),
	ROW(BOSS, values_boss),
	ROW(MINIBOSS, values_miniboss),
	ROW(ENEMIES, values_enemies),
	ROW(GOAL, values_goal),
	ROW(GOALCOUNT, values_goalcount),
	ROW(ITEMPOOL, values_itempool),
	ROW(HPMODE, values_hpmode),
	ROW(HARDMODE, values_hardmode),
	ROW(STEAL, values_steal),
	ROW(EVILSHOP, values_evilshop),
	ROW(BOWWOW, values_bowwow),
	ROW(OVERWORLD, values_overworld),
	ROW(DUNGEONCHAINLENGTH, values_dungeonchainlength),
	ROW(OWLSTATUES, values_owlstatues),
	ROW(KEYHOLESANITY, values_keyholesanity),
	ROW(SHOPSANITY, values_shopsanity),
	ROW(SUPERWEAPONS, values_superweapons),
	ROW(QUICKSWAP, values_quickswap),
	ROW(TEXTMODE, values_textmode),
	ROW(LOWHPBEEP, values_lowhpbeep),
	ROW(NOFLASH, values_noflash),
	ROW(NAGMESSAGES, values_nagmessages),
	ROW(GFXMOD, values_gfxmod),
	ROW(FOLLOWER, values_follower),
	ROW(LINKSPALETTE, values_linkspalette),
	ROW(MUSIC, values_music),
};

static const UiStrId categories[LADX_CATEGORY_COUNT] = {
	[LADX_CATEGORY_MAIN] = UI_STR_LADX_CAT_MAIN,
	[LADX_CATEGORY_ITEMS] = UI_STR_LADX_CAT_ITEMS,
	[LADX_CATEGORY_DUNGEON_ITEMS] = UI_STR_LADX_CAT_DUNGEON_ITEMS,
	[LADX_CATEGORY_ENTRANCES] = UI_STR_LADX_CAT_ENTRANCES,
	[LADX_CATEGORY_GAMEPLAY] = UI_STR_LADX_CAT_GAMEPLAY,
	[LADX_CATEGORY_SPECIAL] = UI_STR_LADX_CAT_SPECIAL,
	[LADX_CATEGORY_USER_OPTIONS] = UI_STR_LADX_CAT_USER_OPTIONS,
};

static const UiStrId reasons[LADX_REASON_COUNT] = {
	[LADX_REASON_NONE] = UI_STR_NONE,
	[LADX_REASON_TIER2_ENTRANCES] = UI_STR_LADX_REASON_TIER2_ENTRANCES,
	[LADX_REASON_TIER3_OVERWORLD] = UI_STR_LADX_REASON_TIER3_OVERWORLD,
	[LADX_REASON_TIER3_SHOPSANITY] = UI_STR_LADX_REASON_TIER3_SHOPSANITY,
	[LADX_REASON_GFXMOD_LICENCE] = UI_STR_LADX_REASON_GFXMOD_LICENCE,
	[LADX_REASON_MUSIC_FFA] = UI_STR_LADX_REASON_MUSIC_FFA,
	[LADX_REASON_GOAL_BINGO] = UI_STR_LADX_REASON_GOAL_BINGO,
	[LADX_REASON_LADXR_REFUSES] = UI_STR_LADX_REASON_LADXR_REFUSES,
};

static const Row *row(LadxSettingId id)
{
	assert((unsigned)id < LADX_SETTING_COUNT);
	assert(rows[id].count == ladx_setting_value_count(id));
	return &rows[id];
}

UiStrId ui_ladx_category_label(LadxSettingCategory category)
{
	assert((unsigned)category < LADX_CATEGORY_COUNT);
	return categories[category];
}

UiStrId ui_ladx_setting_label(LadxSettingId id)
{
	return row(id)->label;
}

UiStrId ui_ladx_setting_value(LadxSettingId id, int value)
{
	assert(value >= 0 && value < row(id)->count);
	return row(id)->values[value];
}

UiStrId ui_ladx_setting_help(LadxSettingId id)
{
	return row(id)->help;
}

UiStrId ui_ladx_reason(LadxReason reason)
{
	assert((unsigned)reason < LADX_REASON_COUNT);
	return reasons[reason];
}

bool ui_ladx_setting_shown(const LadxSettings *settings, LadxSettingId id)
{
	LadxSettingId controller = ladx_setting_visible_if(id);

	return controller == LADX_SETTING_COUNT || ladx_setting_visible_for(id, settings->value[controller]);
}
