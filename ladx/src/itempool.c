#include "ladx_itempool.h"

#include <stdio.h>
#include <string.h>

static bool valid(int item)
{
	return item >= 0 && item < LADX_ITEM_COUNT;
}

static int find(const LadxItemPool *pool, int item)
{
	int i;

	for (i = 0; i < pool->size; i++)
		if (pool->item[i] == item)
			return i;
	return -1;
}

void ladx_itempool_clear(LadxItemPool *pool)
{
	pool->size = 0;
}

int ladx_itempool_size(const LadxItemPool *pool)
{
	return pool->size;
}

bool ladx_itempool_entry(const LadxItemPool *pool, int i, int *item, int *count)
{
	if (i < 0 || i >= pool->size)
		return false;
	*item = pool->item[i];
	*count = pool->count[i];
	return true;
}

bool ladx_itempool_contains(const LadxItemPool *pool, int item)
{
	return find(pool, item) >= 0;
}

int ladx_itempool_get(const LadxItemPool *pool, int item)
{
	int i = find(pool, item);

	return i < 0 ? 0 : pool->count[i];
}

bool ladx_itempool_set(LadxItemPool *pool, int item, int count)
{
	int i;

	if (!valid(item))
		return false;
	i = find(pool, item);
	if (i < 0) {
		i = pool->size++;
		pool->item[i] = item;
	}
	pool->count[i] = count;
	return true;
}

bool ladx_itempool_del(LadxItemPool *pool, int item)
{
	int i;

	if (!valid(item))
		return false;
	i = find(pool, item);
	if (i < 0)
		return true;
	pool->size--;
	memmove(&pool->item[i], &pool->item[i + 1], (size_t)(pool->size - i) * sizeof pool->item[0]);
	memmove(&pool->count[i], &pool->count[i + 1], (size_t)(pool->size - i) * sizeof pool->count[0]);
	return true;
}

bool ladx_itempool_add(LadxItemPool *pool, int item, int count)
{
	return valid(item) && ladx_itempool_set(pool, item, ladx_itempool_get(pool, item) + count);
}

bool ladx_itempool_remove(LadxItemPool *pool, int item, int count)
{
	int left;

	if (!valid(item))
		return false;
	left = ladx_itempool_get(pool, item) - count;
	ladx_itempool_set(pool, item, left);
	if (left == 0)
		ladx_itempool_del(pool, item);
	return true;
}

typedef struct Stock {
	int item, count;
} Stock;

#define I(name) LADX_ITEM_##name

static const Stock default_pool[] = {
	{I(SWORD), 2}, {I(FEATHER), 1}, {I(HOOKSHOT), 1}, {I(BOW), 1}, {I(BOMB), 1}, {I(MAGIC_POWDER), 1},
	{I(MAGIC_ROD), 1}, {I(OCARINA), 1}, {I(PEGASUS_BOOTS), 1}, {I(POWER_BRACELET), 2}, {I(SHIELD), 2},
	{I(SHOVEL), 1}, {I(ROOSTER), 1}, {I(TOADSTOOL), 1},
	{I(TAIL_KEY), 1}, {I(SLIME_KEY), 1}, {I(ANGLER_KEY), 1}, {I(FACE_KEY), 1}, {I(BIRD_KEY), 1},
	{I(GOLD_LEAF), 5},
	{I(FLIPPERS), 1}, {I(BOWWOW), 1}, {I(SONG1), 1}, {I(SONG2), 1}, {I(SONG3), 1},
	{I(BLUE_TUNIC), 1}, {I(RED_TUNIC), 1}, {I(MAX_ARROWS_UPGRADE), 1}, {I(MAX_BOMBS_UPGRADE), 1},
	{I(MAX_POWDER_UPGRADE), 1},
	{I(HEART_CONTAINER), 8}, {I(HEART_PIECE), 12},
	{I(RUPEES_100), 3}, {I(RUPEES_20), 6}, {I(RUPEES_200), 3}, {I(RUPEES_50), 19},
	{I(SEASHELL), 26}, {I(MEDICINE), 3}, {I(GEL), 4}, {I(MESSAGE), 1},
	{I(COMPASS1), 1}, {I(COMPASS2), 1}, {I(COMPASS3), 1}, {I(COMPASS4), 1}, {I(COMPASS5), 1},
	{I(COMPASS6), 1}, {I(COMPASS7), 1}, {I(COMPASS8), 1}, {I(COMPASS0), 1},
	{I(KEY1), 3}, {I(KEY2), 5}, {I(KEY3), 9}, {I(KEY4), 5}, {I(KEY5), 3}, {I(KEY6), 3}, {I(KEY7), 3},
	{I(KEY8), 7}, {I(KEY0), 3},
	{I(MAP1), 1}, {I(MAP2), 1}, {I(MAP3), 1}, {I(MAP4), 1}, {I(MAP5), 1}, {I(MAP6), 1}, {I(MAP7), 1},
	{I(MAP8), 1}, {I(MAP0), 1},
	{I(NIGHTMARE_KEY1), 1}, {I(NIGHTMARE_KEY2), 1}, {I(NIGHTMARE_KEY3), 1}, {I(NIGHTMARE_KEY4), 1},
	{I(NIGHTMARE_KEY5), 1}, {I(NIGHTMARE_KEY6), 1}, {I(NIGHTMARE_KEY7), 1}, {I(NIGHTMARE_KEY8), 1},
	{I(NIGHTMARE_KEY0), 1},
	{I(STONE_BEAK1), 1}, {I(STONE_BEAK2), 1}, {I(STONE_BEAK3), 1}, {I(STONE_BEAK4), 1}, {I(STONE_BEAK5), 1},
	{I(STONE_BEAK6), 1}, {I(STONE_BEAK7), 1}, {I(STONE_BEAK8), 1}, {I(STONE_BEAK0), 1},
	{I(INSTRUMENT1), 1}, {I(INSTRUMENT2), 1}, {I(INSTRUMENT3), 1}, {I(INSTRUMENT4), 1}, {I(INSTRUMENT5), 1},
	{I(INSTRUMENT6), 1}, {I(INSTRUMENT7), 1}, {I(INSTRUMENT8), 1},
	{I(TRADING_ITEM_YOSHI_DOLL), 1}, {I(TRADING_ITEM_RIBBON), 1}, {I(TRADING_ITEM_DOG_FOOD), 1},
	{I(TRADING_ITEM_BANANAS), 1}, {I(TRADING_ITEM_STICK), 1}, {I(TRADING_ITEM_HONEYCOMB), 1},
	{I(TRADING_ITEM_PINEAPPLE), 1}, {I(TRADING_ITEM_HIBISCUS), 1}, {I(TRADING_ITEM_LETTER), 1},
	{I(TRADING_ITEM_BROOM), 1}, {I(TRADING_ITEM_FISHING_HOOK), 1}, {I(TRADING_ITEM_NECKLACE), 1},
	{I(TRADING_ITEM_SCALE), 1}, {I(TRADING_ITEM_MAGNIFYING_GLASS), 1},
	{I(MEDICINE2), 1}, {I(TOADSTOOL2), 1}, {I(RAFT), 1},
	{I(TAIL_CAVE_OPENED), 1}, {I(KEY_CAVERN_OPENED), 1}, {I(ANGLER_TUNNEL_OPENED), 1},
	{I(FACE_SHRINE_OPENED), 1}, {I(CASTLE_GATE_OPENED), 1}, {I(EAGLE_TOWER_OPENED), 1},
};

static const Stock static_dungeon_items[] = {
	{I(D3_BOMBWALL), 1}, {I(D3_GEL_CLEAR), 1}, {I(D3_STALFOS_CLEAR), 1}, {I(D3_ZOLS_CLEAR), 1}, {I(SWITCH3), 1},
	{I(D4_BOSS_CLEAR), 1}, {I(D4_PITKEY), 1}, {I(D4_PUZZLE_CLUE), 1},
	{I(D5_ZOL_CLEAR), 1}, {I(MS1_KILL), 1}, {I(MS2_KILL), 1}, {I(MS3_KILL), 1},
	{I(D6_THREE_WIZROBE_CLEAR), 1}, {I(SWITCH6A), 1}, {I(SWITCH6A_RANGE), 1}, {I(SWITCH6B_MIDRANGE), 1},
	{I(SWITCH6B_RANGE), 1}, {I(SWITCH6C), 1}, {I(SWITCH6D), 1}, {I(SWITCH6E), 1}, {I(SWITCH6F), 1},
	{I(D7_BALL), 1}, {I(D7_BOSS_CLEAR), 1}, {I(D7_PILLAR), 4}, {I(D7_TOAK_CLEAR), 1}, {I(SWITCH7A), 1},
	{I(SWITCH7A_RANGE), 1}, {I(SWITCH7B), 1}, {I(SWITCH7B_RANGE), 1}, {I(SWITCH7C), 1},
	{I(SWITCH7C_MIDRANGE), 1}, {I(SWITCH7C_RANGE), 1},
	{I(SWITCH8), 1},
};

/* f"{base}{n}" for n 0 to 8: the table holds <base>1 to <base>8, then <base>0. */
static int numbered(int base, int n)
{
	return n == 0 ? base + 9 : base + n;
}

static bool is(const LadxSettings *s, LadxSettingId id, const char *name)
{
	return strcmp(ladx_setting_value_name(id, s->value[id]), name) == 0;
}

static void crash(LadxOutcome *outcome, const char *message)
{
	outcome->kind = LADX_OUTCOME_CRASH;
	outcome->type = "RuntimeError";
	outcome->message = message;
}

static bool remove_rupee(LadxItemPool *pool, LadxOutcome *outcome)
{
	static const int rupees[] = {I(RUPEES_20), I(RUPEES_50), I(RUPEES_100), I(RUPEES_200), I(RUPEES_500)};
	size_t i;

	for (i = 0; i < sizeof rupees / sizeof rupees[0]; i++)
		if (ladx_itempool_get(pool, rupees[i]) > 0) {
			ladx_itempool_remove(pool, rupees[i], 1);
			return true;
		}
	crash(outcome, "Wanted to remove more rupees from the pool then we have");
	return false;
}

static bool remove_rupees(LadxItemPool *pool, int count, LadxOutcome *outcome)
{
	int n;

	for (n = 0; n < count; n++)
		if (!remove_rupee(pool, outcome))
			return false;
	return true;
}

static void maps_to_keys(LadxItemPool *pool)
{
	int n;

	for (n = 0; n < 9; n++) {
		ladx_itempool_remove(pool, numbered(I(MAP), n), 1);
		ladx_itempool_remove(pool, numbered(I(COMPASS), n), 1);
		ladx_itempool_add(pool, numbered(I(KEY), n), 1);
		ladx_itempool_add(pool, numbered(I(NIGHTMARE_KEY), n), 1);
	}
}

static void to_rupees(LadxItemPool *pool, int item, int add_back)
{
	if (ladx_itempool_contains(pool, item)) {
		int count = ladx_itempool_get(pool, item);

		ladx_itempool_add(pool, I(RUPEES_20), count);
		ladx_itempool_remove(pool, item, count);
	}
	if (add_back)
		ladx_itempool_add(pool, item, add_back);
}

static bool setup(LadxItemPool *pool, const LadxSettings *s, LadxOutcome *outcome)
{
	int heart_container_type = I(HEART_CONTAINER);
	size_t i;
	int n;

	for (i = 0; i < sizeof default_pool / sizeof default_pool[0]; i++)
		ladx_itempool_add(pool, default_pool[i].item, default_pool[i].count);
	if (!is(s, LADX_SETTING_BOOMERANG, "default"))
		ladx_itempool_add(pool, I(BOOMERANG), 1);
	if (is(s, LADX_SETTING_OWLSTATUES, "both"))
		ladx_itempool_add(pool, I(RUPEES_20), 9 + 24);
	else if (is(s, LADX_SETTING_OWLSTATUES, "dungeon"))
		ladx_itempool_add(pool, I(RUPEES_20), 24);
	else if (is(s, LADX_SETTING_OWLSTATUES, "overworld"))
		ladx_itempool_add(pool, I(RUPEES_20), 9);

	if (is(s, LADX_SETTING_HPMODE, "inverted"))
		heart_container_type = I(BAD_HEART_CONTAINER);
	else if (is(s, LADX_SETTING_HPMODE, "low"))
		heart_container_type = I(HEART_PIECE);
	else if (is(s, LADX_SETTING_HPMODE, "5hit")) {
		heart_container_type = I(RUPEES_20);
		ladx_itempool_add(pool, I(RUPEES_20), ladx_itempool_get(pool, I(HEART_PIECE)));
		ladx_itempool_remove(pool, I(HEART_PIECE), ladx_itempool_get(pool, I(HEART_PIECE)));
	}

	if (is(s, LADX_SETTING_GOAL, "seashells"))
		ladx_itempool_remove(pool, I(SEASHELL), 2);

	if (is(s, LADX_SETTING_ITEMPOOL, "casual")) {
		static const int one_more[] = {I(SWORD), I(FLIPPERS), I(FEATHER), I(HOOKSHOT), I(BOW), I(BOMB),
					       I(MAGIC_POWDER), I(MAGIC_ROD), I(OCARINA), I(PEGASUS_BOOTS),
					       I(POWER_BRACELET), I(SHOVEL)};

		for (i = 0; i < sizeof one_more / sizeof one_more[0]; i++)
			ladx_itempool_add(pool, one_more[i], 1);
		ladx_itempool_add(pool, I(RUPEES_200), 2);
		if (!remove_rupees(pool, 14, outcome))
			return false;
		maps_to_keys(pool);
	} else if (is(s, LADX_SETTING_ITEMPOOL, "pain")) {
		ladx_itempool_add(pool, I(BAD_HEART_CONTAINER), 12);
		ladx_itempool_remove(pool, I(BLUE_TUNIC), 1);
		if (!remove_rupees(pool, 7 - ladx_itempool_get(pool, I(MEDICINE)), outcome))
			return false;
		ladx_itempool_remove(pool, I(MEDICINE), ladx_itempool_get(pool, I(MEDICINE)));
		ladx_itempool_remove(pool, I(HEART_PIECE), 4);
	} else if (is(s, LADX_SETTING_ITEMPOOL, "keyup")) {
		maps_to_keys(pool);
		if (is(s, LADX_SETTING_OWLSTATUES, "") || is(s, LADX_SETTING_OWLSTATUES, "overworld"))
			for (n = 0; n < 9; n++) {
				ladx_itempool_remove(pool, numbered(I(STONE_BEAK), n), 1);
				ladx_itempool_add(pool, numbered(I(KEY), n), 1);
			}
	}

	if (!s->value[LADX_SETTING_ROOSTER]) {
		ladx_itempool_remove(pool, I(ROOSTER), 1);
		ladx_itempool_add(pool, I(RUPEES_50), 1);
	}

	for (i = 0; i < sizeof static_dungeon_items / sizeof static_dungeon_items[0]; i++)
		ladx_itempool_add(pool, static_dungeon_items[i].item, static_dungeon_items[i].count);

	if (is(s, LADX_SETTING_DUNGEON_KEYS, "removed"))
		for (n = 0; n < 9; n++)
			to_rupees(pool, numbered(I(KEY), n), 9);
	if (is(s, LADX_SETTING_NIGHTMARE_KEYS, "removed"))
		for (n = 0; n < 9; n++)
			to_rupees(pool, numbered(I(NIGHTMARE_KEY), n), 1);
	if (is(s, LADX_SETTING_DUNGEON_BEAKS, "removed"))
		for (n = 0; n < 9; n++)
			to_rupees(pool, numbered(I(STONE_BEAK), n), 1);
	if (is(s, LADX_SETTING_DUNGEON_MAPS, "removed"))
		for (n = 0; n < 9; n++) {
			to_rupees(pool, numbered(I(MAP), n), 0);
			to_rupees(pool, numbered(I(COMPASS), n), 0);
		}

	if (is(s, LADX_SETTING_BOWWOW, "always")) {
		ladx_itempool_add(pool, I(SWORD), 1);
		ladx_itempool_remove(pool, I(BOWWOW), 1);
	} else if (is(s, LADX_SETTING_BOWWOW, "swordless")) {
		ladx_itempool_add(pool, I(RUPEES_20),
				  ladx_itempool_get(pool, I(BOWWOW)) + ladx_itempool_get(pool, I(SWORD)) - 1);
		ladx_itempool_remove(pool, I(SWORD), ladx_itempool_get(pool, I(SWORD)) - 1);
		ladx_itempool_remove(pool, I(BOWWOW), ladx_itempool_get(pool, I(BOWWOW)));
	}

	if (is(s, LADX_SETTING_GOAL, "seashells")) {
		for (n = 1; n <= 8; n++)
			ladx_itempool_remove(pool, I(INSTRUMENT1) + n - 1, 1);
		ladx_itempool_add(pool, I(SEASHELL), 8);
		if (ladx_itempool_get(pool, I(SEASHELL)) < 20) {
			snprintf(outcome->text, sizeof outcome->text, "Not enough seashells (%d) available in itempool",
				 ladx_itempool_get(pool, I(SEASHELL)));
			crash(outcome, outcome->text);
			return false;
		}
	}

	if (heart_container_type != I(HEART_CONTAINER)) {
		ladx_itempool_add(pool, heart_container_type, ladx_itempool_get(pool, I(HEART_CONTAINER)));
		ladx_itempool_remove(pool, I(HEART_CONTAINER), ladx_itempool_get(pool, I(HEART_CONTAINER)));
	}

	if (is(s, LADX_SETTING_LOGIC, "casual"))
		ladx_itempool_remove(pool, I(SWITCH7B_RANGE), ladx_itempool_get(pool, I(SWITCH7B_RANGE)));
	return true;
}

/* Python's a // b for b > 0. */
static int floor_div(int a, int b)
{
	return a >= 0 ? a / b : -((-a + b - 1) / b);
}

static LadxRandomizeStatus randomize_rupees(LadxItemPool *pool, LadxRng *rnd)
{
	static const int new_items[] = {I(BOMB), I(SINGLE_ARROW), I(ARROWS_10), I(MAGIC_POWDER), I(MEDICINE)};
	static const int new_weights[] = {10, 5, 10, 10, 1};
	int rupee_item[2], rupee_item_count[2];
	int kinds = 0, total = 0, rupee_chests = 0, i, n, item, count;
	size_t pick;

	for (i = 0; ladx_itempool_entry(pool, i, &item, &count); i++) {
		if ((item == I(RUPEES_20) || item == I(RUPEES_50)) && count > 0) {
			rupee_item[kinds] = item;
			rupee_item_count[kinds++] = count;
			total += count;
		}
		if (strncmp(ladx_item_name(item), "RUPEES_", 7) == 0)
			rupee_chests += count;
	}
	if (floor_div(rupee_chests, 5) > total)
		rupee_chests = 5 * total;
	for (n = 0; n < floor_div(rupee_chests, 5); n++) {
		int new_item, remove_item;

		if (ladx_rng_choices(rnd, LADX_STAGE_ITEMPOOL_RANDOMIZE_RUPEES, new_weights, 5, 1, &pick) != LADX_RNG_OK)
			return LADX_RANDOMIZE_BROKEN;
		new_item = new_items[pick];
		do {
			if (ladx_rng_choices(rnd, LADX_STAGE_ITEMPOOL_RANDOMIZE_RUPEES, rupee_item_count, (size_t)kinds, 1,
					     &pick) != LADX_RNG_OK)
				return LADX_RANDOMIZE_BROKEN;
			remove_item = rupee_item[pick];
		} while (ladx_itempool_get(pool, remove_item) <= 0);
		ladx_itempool_add(pool, new_item, 1);
		ladx_itempool_remove(pool, remove_item, 1);
	}
	return LADX_RANDOMIZE_OK;
}

LadxRandomizeStatus ladx_itempool_build(LadxItemPool *pool, const LadxSettings *validated, LadxRng *rnd,
					LadxOutcome *outcome)
{
	ladx_itempool_clear(pool);
	if (!is(validated, LADX_SETTING_OVERWORLD, "normal") || !is(validated, LADX_SETTING_SHOPSANITY, ""))
		return LADX_RANDOMIZE_UNSUPPORTED;
	if (!setup(pool, validated, outcome))
		return LADX_RANDOMIZE_OK;
	return randomize_rupees(pool, rnd);
}
