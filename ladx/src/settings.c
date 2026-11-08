#include "ladx_settings.h"

#include <assert.h>
#include <string.h>

typedef struct Setting {
	const char *key;
	LadxSettingCategory category;
	const char *short_key;
	bool aesthetic;
	bool is_bool;
	const char *const *names;
	const char *const *shorts;
	int count;
	int default_value;
	LadxSettingId visible_if;
	uint32_t visible_mask;
} Setting;

static const char *const bool_names[] = {"0", "1"};
static const char *const bool_shorts[] = {"", ""};

#define COUNT_OF(a) (int)(sizeof a / sizeof a[0])
#define BOOL true, bool_names, bool_shorts, 2
#define VALUES(names, shorts) false, names, shorts, COUNT_OF(names)
#define ALWAYS LADX_SETTING_COUNT, 0

static const char *const logic_names[] = {"casual", "", "hard", "glitched", "hell"};
static const char *const logic_shorts[] = {"c", "", "h", "g", "H"};
static const char *const accessibility_names[] = {"all", "goal"};
static const char *const accessibility_shorts[] = {"a", "g"};
static const char *const boomerang_names[] = {"default", "trade", "gift"};
static const char *const boomerang_shorts[] = {"d", "t", "g"};
static const char *const dungeon_keys_names[] = {"", "keysanity", "removed"};
static const char *const dungeon_keys_shorts[] = {"", "k", "r"};
static const char *const nightmare_keys_names[] = {"", "keysanity", "removed"};
static const char *const nightmare_keys_shorts[] = {"", "k", "r"};
static const char *const dungeon_beaks_names[] = {"", "keysanity", "removed"};
static const char *const dungeon_beaks_shorts[] = {"", "k", "r"};
static const char *const dungeon_maps_names[] = {"", "keysanity", "removed"};
static const char *const dungeon_maps_shorts[] = {"", "k", "r"};
static const char *const entranceshuffle_names[] = {
	"none", "simple", "split", "mixed", "wild", "chaos", "insane", "madness"};
static const char *const entranceshuffle_shorts[] = {"", "s", "S", "m", "w", "c", "i", "M"};
static const char *const boss_names[] = {"default", "shuffle", "random"};
static const char *const boss_shorts[] = {"", "s", "r"};
static const char *const miniboss_names[] = {"default", "shuffle", "random"};
static const char *const miniboss_shorts[] = {"", "s", "r"};
static const char *const enemies_names[] = {"default", "overworld"};
static const char *const enemies_shorts[] = {"", "o"};
static const char *const goal_names[] = {
	"vanilla", "instruments", "specific", "open", "open-4", "5-8", "seashells", "bingo", "bingo-double",
	"bingo-triple", "bingo-full", "maze"};
static const char *const goal_shorts[] = {"v", "i", "s", "O", "<", ">", "S", "b", "d", "t", "B", "m"};
static const char *const goalcount_names[] = {"7", "6", "5", "4", "3", "2", "1", "0", "random"};
static const char *const goalcount_shorts[] = {"7", "6", "5", "4", "3", "2", "1", "0", "R"};
static const char *const itempool_names[] = {"", "casual", "pain", "keyup"};
static const char *const itempool_shorts[] = {"", "c", "p", "k"};
static const char *const hpmode_names[] = {"default", "inverted", "1", "low", "5hit"};
static const char *const hpmode_shorts[] = {"", "i", "1", "l", "5"};
static const char *const hardmode_names[] = {"none", "oracle", "hero", "ohko"};
static const char *const hardmode_shorts[] = {"", "O", "H", "1"};
static const char *const steal_names[] = {"always", "never", "default", "ggs"};
static const char *const steal_shorts[] = {"a", "n", "", "g"};
static const char *const evilshop_names[] = {"", "enabled"};
static const char *const evilshop_shorts[] = {"", "e"};
static const char *const bowwow_names[] = {"normal", "always", "swordless"};
static const char *const bowwow_shorts[] = {"", "a", "s"};
static const char *const overworld_names[] = {
	"normal", "dungeondive", "nodungeons", "dungeonchain", "random", "alttp"};
static const char *const overworld_shorts[] = {"", "D", "N", "C", "R", "A"};
static const char *const dungeonchainlength_names[] = {"3", "4", "5", "6", "7", "8"};
static const char *const dungeonchainlength_shorts[] = {"3", "4", "5", "6", "7", "8"};
static const char *const owlstatues_names[] = {"", "dungeon", "overworld", "both"};
static const char *const owlstatues_shorts[] = {"", "D", "O", "B"};
static const char *const shopsanity_names[] = {"", "basic", "important"};
static const char *const shopsanity_shorts[] = {"", "b", "i"};
static const char *const quickswap_names[] = {"none", "a", "b"};
static const char *const quickswap_shorts[] = {"", "a", "b"};
static const char *const textmode_names[] = {"fast", "default", "none", "notextbox"};
static const char *const textmode_shorts[] = {"", "d", "n", "b"};
static const char *const lowhpbeep_names[] = {"none", "slow", "default"};
static const char *const lowhpbeep_shorts[] = {"D", "S", "N"};
static const char *const gfxmod_names[] = {
	"", "AgesGirl.bin", "Bowwow.bin", "Braixen.bin", "Bunny.bin", "GrandmaUlrira.bin", "Kirby.bin",
	"Kris.bin", "LuckwurstJoe.bin", "Luigi.bin", "MMLink.bin", "Marin.bin", "MarinAlpha.bin", "Mario.bin",
	"Martha.bin", "Matty.bin", "Meme.bin", "Moblin.bin", "NESLink.bin", "Ninten.bin", "Noelle.bin",
	"Ralsei.bin", "Richard.bin", "Ricky.bin", "Rooster.bin", "Rosa.bin", "Saria.bin", "Sig.bin",
	"SpongeBob.bin", "Subrosian.bin", "Tarin.bin", "X.bin"};
static const char *const gfxmod_shorts[] = {
	"", "AgesGirl.bin>", "Bowwow.bin>", "Braixen.bin>", "Bunny.bin>", "GrandmaUlrira.bin>", "Kirby.bin>",
	"Kris.bin>", "LuckwurstJoe.bin>", "Luigi.bin>", "MMLink.bin>", "Marin.bin>", "MarinAlpha.bin>",
	"Mario.bin>", "Martha.bin>", "Matty.bin>", "Meme.bin>", "Moblin.bin>", "NESLink.bin>", "Ninten.bin>",
	"Noelle.bin>", "Ralsei.bin>", "Richard.bin>", "Ricky.bin>", "Rooster.bin>", "Rosa.bin>", "Saria.bin>",
	"Sig.bin>", "SpongeBob.bin>", "Subrosian.bin>", "Tarin.bin>", "X.bin>"};
static const char *const follower_names[] = {"", "fox", "navi", "ghost", "yipyip"};
static const char *const follower_shorts[] = {"", "f", "n", "g", "y"};
static const char *const linkspalette_names[] = {"-1", "0", "1", "2", "3", "4", "5", "6", "7"};
static const char *const linkspalette_shorts[] = {"-", "0", "1", "2", "3", "4", "5", "6", "7"};
static const char *const music_names[] = {"", "random", "off", "shifted", "ffa"};
static const char *const music_shorts[] = {"", "r", "o", "s", "f"};

static const Setting table[LADX_SETTING_COUNT] = {
	[LADX_SETTING_LOGIC] = {"logic", LADX_CATEGORY_MAIN, "L", false,
		VALUES(logic_names, logic_shorts), 1, ALWAYS},
	[LADX_SETTING_ACCESSIBILITY] = {"accessibility", LADX_CATEGORY_MAIN, "A", false,
		VALUES(accessibility_names, accessibility_shorts), 0, ALWAYS},
	[LADX_SETTING_RACE] = {"race", LADX_CATEGORY_MAIN, "V", false, BOOL, 0, ALWAYS},
	[LADX_SETTING_HEARTPIECE] = {"heartpiece", LADX_CATEGORY_ITEMS, "h", false, BOOL, 1, ALWAYS},
	[LADX_SETTING_SEASHELLS] = {"seashells", LADX_CATEGORY_ITEMS, "s", false, BOOL, 1, ALWAYS},
	[LADX_SETTING_HEARTCONTAINERS] = {"heartcontainers", LADX_CATEGORY_ITEMS, "H", false, BOOL, 1, ALWAYS},
	[LADX_SETTING_INSTRUMENTS] = {"instruments", LADX_CATEGORY_ITEMS, "I", false, BOOL, 0, ALWAYS},
	[LADX_SETTING_TRADEQUEST] = {"tradequest", LADX_CATEGORY_ITEMS, "T", false, BOOL, 1, ALWAYS},
	[LADX_SETTING_WITCH] = {"witch", LADX_CATEGORY_ITEMS, "W", false, BOOL, 1, ALWAYS},
	[LADX_SETTING_ROOSTER] = {"rooster", LADX_CATEGORY_ITEMS, "R", false, BOOL, 1, ALWAYS},
	[LADX_SETTING_BOOMERANG] = {"boomerang", LADX_CATEGORY_ITEMS, "Z", false,
		VALUES(boomerang_names, boomerang_shorts), 2, ALWAYS},
	[LADX_SETTING_DUNGEON_KEYS] = {"dungeon_keys", LADX_CATEGORY_DUNGEON_ITEMS, "k", false,
		VALUES(dungeon_keys_names, dungeon_keys_shorts), 0, ALWAYS},
	[LADX_SETTING_NIGHTMARE_KEYS] = {"nightmare_keys", LADX_CATEGORY_DUNGEON_ITEMS, "n", false,
		VALUES(nightmare_keys_names, nightmare_keys_shorts), 0, ALWAYS},
	[LADX_SETTING_DUNGEON_BEAKS] = {"dungeon_beaks", LADX_CATEGORY_DUNGEON_ITEMS, "Y", false,
		VALUES(dungeon_beaks_names, dungeon_beaks_shorts), 0, ALWAYS},
	[LADX_SETTING_DUNGEON_MAPS] = {"dungeon_maps", LADX_CATEGORY_DUNGEON_ITEMS, "D", false,
		VALUES(dungeon_maps_names, dungeon_maps_shorts), 0, ALWAYS},
	[LADX_SETTING_RANDOMSTARTLOCATION] = {"randomstartlocation", LADX_CATEGORY_ENTRANCES, "r", false,
		BOOL, 0, ALWAYS},
	[LADX_SETTING_DUNGEONSHUFFLE] = {"dungeonshuffle", LADX_CATEGORY_ENTRANCES, "u", false, BOOL, 0, ALWAYS},
	[LADX_SETTING_ENTRANCESHUFFLE] = {"entranceshuffle", LADX_CATEGORY_ENTRANCES, "E", false,
		VALUES(entranceshuffle_names, entranceshuffle_shorts), 0, ALWAYS},
	[LADX_SETTING_SHUFFLEJUNK] = {"shufflejunk", LADX_CATEGORY_ENTRANCES, "j", false,
		BOOL, 0, LADX_SETTING_ENTRANCESHUFFLE, 0xfeu},
	[LADX_SETTING_SHUFFLEANNOYING] = {"shuffleannoying", LADX_CATEGORY_ENTRANCES, "a", false,
		BOOL, 0, LADX_SETTING_ENTRANCESHUFFLE, 0xfeu},
	[LADX_SETTING_SHUFFLEWATER] = {"shufflewater", LADX_CATEGORY_ENTRANCES, "w", false,
		BOOL, 0, LADX_SETTING_ENTRANCESHUFFLE, 0xfeu},
	[LADX_SETTING_BOSS] = {"boss", LADX_CATEGORY_GAMEPLAY, "B", false,
		VALUES(boss_names, boss_shorts), 0, ALWAYS},
	[LADX_SETTING_MINIBOSS] = {"miniboss", LADX_CATEGORY_GAMEPLAY, "b", false,
		VALUES(miniboss_names, miniboss_shorts), 0, ALWAYS},
	[LADX_SETTING_ENEMIES] = {"enemies", LADX_CATEGORY_GAMEPLAY, "e", false,
		VALUES(enemies_names, enemies_shorts), 0, ALWAYS},
	[LADX_SETTING_GOAL] = {"goal", LADX_CATEGORY_GAMEPLAY, "G", false,
		VALUES(goal_names, goal_shorts), 0, ALWAYS},
	[LADX_SETTING_GOALCOUNT] = {"goalcount", LADX_CATEGORY_GAMEPLAY, "i", false,
		VALUES(goalcount_names, goalcount_shorts), 3, LADX_SETTING_GOAL, 0x6u},
	[LADX_SETTING_ITEMPOOL] = {"itempool", LADX_CATEGORY_GAMEPLAY, "P", false,
		VALUES(itempool_names, itempool_shorts), 0, ALWAYS},
	[LADX_SETTING_HPMODE] = {"hpmode", LADX_CATEGORY_GAMEPLAY, "m", false,
		VALUES(hpmode_names, hpmode_shorts), 0, ALWAYS},
	[LADX_SETTING_HARDMODE] = {"hardmode", LADX_CATEGORY_GAMEPLAY, "X", false,
		VALUES(hardmode_names, hardmode_shorts), 0, ALWAYS},
	[LADX_SETTING_STEAL] = {"steal", LADX_CATEGORY_GAMEPLAY, "t", false,
		VALUES(steal_names, steal_shorts), 2, ALWAYS},
	[LADX_SETTING_EVILSHOP] = {"evilshop", LADX_CATEGORY_SPECIAL, "v", false,
		VALUES(evilshop_names, evilshop_shorts), 0, ALWAYS},
	[LADX_SETTING_BOWWOW] = {"bowwow", LADX_CATEGORY_SPECIAL, "g", false,
		VALUES(bowwow_names, bowwow_shorts), 0, ALWAYS},
	[LADX_SETTING_OVERWORLD] = {"overworld", LADX_CATEGORY_SPECIAL, "O", false,
		VALUES(overworld_names, overworld_shorts), 0, ALWAYS},
	[LADX_SETTING_DUNGEONCHAINLENGTH] = {"dungeonchainlength", LADX_CATEGORY_SPECIAL, "d", false,
		VALUES(dungeonchainlength_names, dungeonchainlength_shorts), 2, LADX_SETTING_OVERWORLD, 0x8u},
	[LADX_SETTING_OWLSTATUES] = {"owlstatues", LADX_CATEGORY_SPECIAL, "o", false,
		VALUES(owlstatues_names, owlstatues_shorts), 0, ALWAYS},
	[LADX_SETTING_KEYHOLESANITY] = {"keyholesanity", LADX_CATEGORY_SPECIAL, "K", false, BOOL, 0, ALWAYS},
	[LADX_SETTING_SHOPSANITY] = {"shopsanity", LADX_CATEGORY_SPECIAL, "N", false,
		VALUES(shopsanity_names, shopsanity_shorts), 0, ALWAYS},
	[LADX_SETTING_SUPERWEAPONS] = {"superweapons", LADX_CATEGORY_SPECIAL, "q", false, BOOL, 0, ALWAYS},
	[LADX_SETTING_QUICKSWAP] = {"quickswap", LADX_CATEGORY_USER_OPTIONS, "Q", true,
		VALUES(quickswap_names, quickswap_shorts), 0, ALWAYS},
	[LADX_SETTING_TEXTMODE] = {"textmode", LADX_CATEGORY_USER_OPTIONS, "f", true,
		VALUES(textmode_names, textmode_shorts), 0, ALWAYS},
	[LADX_SETTING_LOWHPBEEP] = {"lowhpbeep", LADX_CATEGORY_USER_OPTIONS, "p", true,
		VALUES(lowhpbeep_names, lowhpbeep_shorts), 1, ALWAYS},
	[LADX_SETTING_NOFLASH] = {"noflash", LADX_CATEGORY_USER_OPTIONS, "l", true, BOOL, 1, ALWAYS},
	[LADX_SETTING_NAGMESSAGES] = {"nagmessages", LADX_CATEGORY_USER_OPTIONS, "S", true, BOOL, 0, ALWAYS},
	[LADX_SETTING_GFXMOD] = {"gfxmod", LADX_CATEGORY_USER_OPTIONS, "c", true,
		VALUES(gfxmod_names, gfxmod_shorts), 0, ALWAYS},
	[LADX_SETTING_FOLLOWER] = {"follower", LADX_CATEGORY_USER_OPTIONS, "x", true,
		VALUES(follower_names, follower_shorts), 0, ALWAYS},
	[LADX_SETTING_LINKSPALETTE] = {"linkspalette", LADX_CATEGORY_USER_OPTIONS, "C", true,
		VALUES(linkspalette_names, linkspalette_shorts), 0, ALWAYS},
	[LADX_SETTING_MUSIC] = {"music", LADX_CATEGORY_USER_OPTIONS, "M", true,
		VALUES(music_names, music_shorts), 0, ALWAYS},
};

static const char *const category_names[LADX_CATEGORY_COUNT] = {
	[LADX_CATEGORY_MAIN] = "Main",
	[LADX_CATEGORY_ITEMS] = "Items",
	[LADX_CATEGORY_DUNGEON_ITEMS] = "Dungeon Items",
	[LADX_CATEGORY_ENTRANCES] = "Entrances",
	[LADX_CATEGORY_GAMEPLAY] = "Gameplay",
	[LADX_CATEGORY_SPECIAL] = "Special",
	[LADX_CATEGORY_USER_OPTIONS] = "User options",
};

static const Setting *info(LadxSettingId id)
{
	assert((unsigned)id < LADX_SETTING_COUNT && "setting out of range");
	return &table[id];
}

static void check_value(LadxSettingId id, int value)
{
	assert(value >= 0 && value < info(id)->count && "setting value out of range");
	(void)id;
	(void)value;
}

const char *ladx_setting_category_name(LadxSettingCategory category)
{
	assert((unsigned)category < LADX_CATEGORY_COUNT && "category out of range");
	return category_names[category];
}

const char *ladx_setting_key(LadxSettingId id)
{
	return info(id)->key;
}

LadxSettingCategory ladx_setting_category(LadxSettingId id)
{
	return info(id)->category;
}

const char *ladx_setting_short_key(LadxSettingId id)
{
	return info(id)->short_key;
}

bool ladx_setting_is_bool(LadxSettingId id)
{
	return info(id)->is_bool;
}

bool ladx_setting_aesthetic(LadxSettingId id)
{
	return info(id)->aesthetic;
}

int ladx_setting_value_count(LadxSettingId id)
{
	return info(id)->count;
}

const char *ladx_setting_value_name(LadxSettingId id, int value)
{
	check_value(id, value);
	return info(id)->names[value];
}

const char *ladx_setting_value_short(LadxSettingId id, int value)
{
	check_value(id, value);
	return info(id)->shorts[value];
}

int ladx_setting_default(LadxSettingId id)
{
	return info(id)->default_value;
}

LadxSettingId ladx_setting_visible_if(LadxSettingId id)
{
	return info(id)->visible_if;
}

bool ladx_setting_visible_for(LadxSettingId id, int controller_value)
{
	const Setting *s = info(id);

	if (s->visible_if == LADX_SETTING_COUNT)
		return true;
	check_value(s->visible_if, controller_value);
	return (s->visible_mask >> controller_value) & 1u;
}

LadxSettingId ladx_setting_find(const char *key)
{
	int id;

	for (id = 0; id < LADX_SETTING_COUNT; id++)
		if (strcmp(table[id].key, key) == 0)
			return (LadxSettingId)id;
	return LADX_SETTING_COUNT;
}

int ladx_setting_value_find(LadxSettingId id, const char *name)
{
	const Setting *s = info(id);
	int v;

	for (v = 0; v < s->count; v++)
		if (strcmp(s->names[v], name) == 0)
			return v;
	return -1;
}

void ladx_settings_defaults(LadxSettings *settings)
{
	int id;

	for (id = 0; id < LADX_SETTING_COUNT; id++)
		settings->value[id] = (uint8_t)table[id].default_value;
}

LadxSettingsStatus ladx_settings_set(LadxSettings *settings, const char *assignment)
{
	const char *eq = strchr(assignment, '=');
	size_t len;
	int id, v;

	if (!eq)
		return LADX_SETTINGS_MALFORMED;
	len = (size_t)(eq - assignment);
	for (id = 0; id < LADX_SETTING_COUNT; id++)
		if (strlen(table[id].key) == len && memcmp(table[id].key, assignment, len) == 0)
			break;
	if (id == LADX_SETTING_COUNT)
		return LADX_SETTINGS_UNKNOWN_KEY;
	v = ladx_setting_value_find((LadxSettingId)id, eq + 1);
	if (v < 0)
		return LADX_SETTINGS_UNKNOWN_VALUE;
	settings->value[id] = (uint8_t)v;
	return LADX_SETTINGS_OK;
}

static int value_named(LadxSettingId id, const char *name)
{
	int v = ladx_setting_value_find(id, name);

	assert(v >= 0 && "validate names a value the table lacks");
	return v;
}

static bool is(const LadxSettings *s, LadxSettingId id, const char *name)
{
	return s->value[id] == value_named(id, name);
}

static void put(LadxSettings *s, LadxSettingId id, const char *name)
{
	s->value[id] = (uint8_t)value_named(id, name);
}

static void dis(LadxSettings *s, LadxSettingId id, const char *name, const char *new_name)
{
	if (is(s, id, name))
		put(s, id, new_name);
}

void ladx_settings_validate(LadxSettings *s)
{
	if (is(s, LADX_SETTING_GOAL, "bingo") || is(s, LADX_SETTING_GOAL, "bingo-double") ||
	    is(s, LADX_SETTING_GOAL, "bingo-triple") || is(s, LADX_SETTING_GOAL, "bingo-full")) {
		put(s, LADX_SETTING_OVERWORLD, "normal");
		put(s, LADX_SETTING_ACCESSIBILITY, "all");
		dis(s, LADX_SETTING_STEAL, "never", "default");
		dis(s, LADX_SETTING_BOSS, "random", "shuffle");
		dis(s, LADX_SETTING_MINIBOSS, "random", "shuffle");
		dis(s, LADX_SETTING_ITEMPOOL, "casual", "");
		dis(s, LADX_SETTING_ITEMPOOL, "keyup", "");
		dis(s, LADX_SETTING_ITEMPOOL, "pain", "");
		dis(s, LADX_SETTING_DUNGEON_MAPS, "removed", "");
	}
	if (is(s, LADX_SETTING_GOAL, "maze")) {
		put(s, LADX_SETTING_OVERWORLD, "normal");
		put(s, LADX_SETTING_ACCESSIBILITY, "all");
	}
	if (is(s, LADX_SETTING_ITEMPOOL, "pain"))
		put(s, LADX_SETTING_HEARTPIECE, "1");
	if (is(s, LADX_SETTING_OVERWORLD, "dungeondive")) {
		dis(s, LADX_SETTING_OWLSTATUES, "overworld", "");
		dis(s, LADX_SETTING_OWLSTATUES, "both", "dungeon");
		if (is(s, LADX_SETTING_ITEMPOOL, "pain") && is(s, LADX_SETTING_OWLSTATUES, ""))
			put(s, LADX_SETTING_ACCESSIBILITY, "goal");
	}
	if (is(s, LADX_SETTING_OVERWORLD, "nodungeons")) {
		dis(s, LADX_SETTING_OWLSTATUES, "dungeon", "");
		dis(s, LADX_SETTING_OWLSTATUES, "both", "overworld");
	}
	if (!is(s, LADX_SETTING_OVERWORLD, "normal") && !is(s, LADX_SETTING_OVERWORLD, "nodungeons"))
		put(s, LADX_SETTING_SHOPSANITY, "");
	if (is(s, LADX_SETTING_OVERWORLD, "alttp"))
		put(s, LADX_SETTING_TRADEQUEST, "1");
	if (is(s, LADX_SETTING_OVERWORLD, "random")) {
		put(s, LADX_SETTING_GOAL, "instruments");
		put(s, LADX_SETTING_GOALCOUNT, "4");
	}
	if (is(s, LADX_SETTING_HPMODE, "5hit"))
		put(s, LADX_SETTING_HEARTPIECE, "1");
}

static bool append(char *buf, size_t cap, size_t *len, const char *text)
{
	size_t n = strlen(text);

	if (*len + n >= cap)
		return false;
	memcpy(buf + *len, text, n);
	*len += n;
	return true;
}

int ladx_settings_short_string(const LadxSettings *settings, char *buf, size_t cap)
{
	size_t len = 0;
	int id;

	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		const Setting *s = &table[id];
		int v = settings->value[id];

		check_value((LadxSettingId)id, v);
		if (s->is_bool ? v == 0 : v == s->default_value)
			continue;
		if (!append(buf, cap, &len, s->short_key) || !append(buf, cap, &len, s->shorts[v])) {
			if (cap > 0)
				buf[0] = '\0';
			return -1;
		}
	}
	if (len >= cap) {
		if (cap > 0)
			buf[0] = '\0';
		return -1;
	}
	buf[len] = '\0';
	return (int)len;
}

static const char *const reason_ids[LADX_REASON_COUNT] = {
	[LADX_REASON_NONE] = "none",
	[LADX_REASON_TIER2_ENTRANCES] = "tier2-entrances",
	[LADX_REASON_TIER3_OVERWORLD] = "tier3-overworld",
	[LADX_REASON_TIER3_SHOPSANITY] = "tier3-shopsanity",
	[LADX_REASON_GFXMOD_LICENCE] = "gfxmod-licence",
	[LADX_REASON_MUSIC_FFA] = "music-ffa",
	[LADX_REASON_GOAL_BINGO] = "goal-bingo",
	[LADX_REASON_LADXR_REFUSES] = "ladxr-refuses",
};

static const LadxReason non_default_reason[LADX_SETTING_COUNT] = {
	[LADX_SETTING_RANDOMSTARTLOCATION] = LADX_REASON_TIER2_ENTRANCES,
	[LADX_SETTING_DUNGEONSHUFFLE] = LADX_REASON_TIER2_ENTRANCES,
	[LADX_SETTING_ENTRANCESHUFFLE] = LADX_REASON_TIER2_ENTRANCES,
	[LADX_SETTING_SHUFFLEJUNK] = LADX_REASON_TIER2_ENTRANCES,
	[LADX_SETTING_SHUFFLEANNOYING] = LADX_REASON_TIER2_ENTRANCES,
	[LADX_SETTING_SHUFFLEWATER] = LADX_REASON_TIER2_ENTRANCES,
	[LADX_SETTING_OVERWORLD] = LADX_REASON_TIER3_OVERWORLD,
	[LADX_SETTING_DUNGEONCHAINLENGTH] = LADX_REASON_TIER3_OVERWORLD,
	[LADX_SETTING_SHOPSANITY] = LADX_REASON_TIER3_SHOPSANITY,
	[LADX_SETTING_GFXMOD] = LADX_REASON_GFXMOD_LICENCE,
};

static const struct {
	LadxSettingId id;
	const char *value;
	LadxReason reason;
} reason_values[] = {
	{LADX_SETTING_GOAL, "bingo", LADX_REASON_GOAL_BINGO},
	{LADX_SETTING_GOAL, "bingo-double", LADX_REASON_GOAL_BINGO},
	{LADX_SETTING_GOAL, "bingo-triple", LADX_REASON_GOAL_BINGO},
	{LADX_SETTING_GOAL, "bingo-full", LADX_REASON_GOAL_BINGO},
	{LADX_SETTING_MUSIC, "ffa", LADX_REASON_MUSIC_FFA},
};

/* LADXR fails every seed: D7's Pit Switch unreachable. */
static const LadxSettingPair pairs[] = {
	{LADX_SETTING_BOOMERANG, 0, LADX_SETTING_ACCESSIBILITY, 0, LADX_REASON_LADXR_REFUSES},
};

const char *ladx_reason_id(LadxReason reason)
{
	assert((unsigned)reason < LADX_REASON_COUNT && "reason out of range");
	return reason_ids[reason];
}

LadxReason ladx_setting_value_reason(LadxSettingId id, int value)
{
	size_t i;

	check_value(id, value);
	if (value != info(id)->default_value && non_default_reason[id] != LADX_REASON_NONE)
		return non_default_reason[id];
	for (i = 0; i < sizeof reason_values / sizeof reason_values[0]; i++)
		if (reason_values[i].id == id && strcmp(info(id)->names[value], reason_values[i].value) == 0)
			return reason_values[i].reason;
	return LADX_REASON_NONE;
}

int ladx_setting_pair_count(void)
{
	return COUNT_OF(pairs);
}

const LadxSettingPair *ladx_setting_pair(int i)
{
	return i >= 0 && i < COUNT_OF(pairs) ? &pairs[i] : NULL;
}

static bool report(LadxSettingPair *finding, LadxSettingPair found)
{
	if (finding)
		*finding = found;
	return false;
}

bool ladx_settings_supported(const LadxSettings *settings, LadxSettingPair *finding)
{
	int id, i;

	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		LadxReason reason = ladx_setting_value_reason((LadxSettingId)id, settings->value[id]);

		if (reason != LADX_REASON_NONE) {
			LadxSettingPair found = {(LadxSettingId)id, settings->value[id], LADX_SETTING_COUNT, -1, reason};

			return report(finding, found);
		}
	}
	for (i = 0; i < COUNT_OF(pairs); i++)
		if (settings->value[pairs[i].a] == pairs[i].value_a && settings->value[pairs[i].b] == pairs[i].value_b)
			return report(finding, pairs[i]);
	return true;
}
