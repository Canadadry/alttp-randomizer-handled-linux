#include "rando_options.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_options"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

/* The oracle wrapper's World::factory arguments for the baseline, on top of rando_config_init's. */
static void baseline_config(RandoConfig *c)
{
	rando_config_init(c);
	c->logic = RANDO_LOGIC_NO_GLITCHES;
	c->item_placement = RANDO_PLACEMENT_ADVANCED;
	c->dungeon_items = RANDO_DUNGEON_ITEMS_STANDARD;
	c->accessibility = RANDO_ACCESSIBILITY_ITEMS;
	c->goal = RANDO_GOAL_GANON;
	c->mode_state = RANDO_STATE_OPEN;
	c->mode_weapons = RANDO_WEAPONS_RANDOMIZED;
	c->crystals_ganon = 7;
	c->crystals_tower = 7;
	c->item_pool = RANDO_ITEM_POOL_NORMAL;
	c->item_functionality = RANDO_ITEM_FUNCTIONALITY_NORMAL;
}

static void check_baseline(void)
{
	RandoOptions options;
	RandoConfig expected, actual;

	rando_options_init(&options);
	baseline_config(&expected);
	rando_config_from_options(&options, 7, 7, &actual);
	CHECK(memcmp(&expected, &actual, sizeof expected) == 0);
}

static RandoOptions options_after(const char *key, const char *value)
{
	RandoOptions options;

	rando_options_init(&options);
	if (!rando_options_set(&options, key, value))
		test_fail(TEST, "%s=%s rejected", key, value);
	return options;
}

static void check_sets(const char *key, const char *value, const RandoConfig *expected)
{
	RandoOptions options = options_after(key, value);
	RandoConfig actual;

	rando_config_from_options(&options, 7, 7, &actual);
	if (memcmp(expected, &actual, sizeof actual) != 0)
		test_fail(TEST, "%s=%s: the config differs from the baseline's plus that option", key, value);
}

#define CHECK_SETS(key, value, field, v) \
	do { \
		RandoConfig e_; \
		baseline_config(&e_); \
		e_.field = (v); \
		check_sets(key, value, &e_); \
	} while (0)

/* Each value of the test matrix, through the oracle wrapper's World::factory arguments. */
static void check_every_value(void)
{
	static const char *const counts[] = {"0", "1", "2", "3", "4", "5", "6", "7"};
	int n;

	CHECK_SETS("glitches", "none", logic, RANDO_LOGIC_NO_GLITCHES);
	CHECK_SETS("glitches", "overworld_glitches", logic, RANDO_LOGIC_OVERWORLD_GLITCHES);
	CHECK_SETS("glitches", "hybrid_major_glitches", logic, RANDO_LOGIC_HYBRID_MAJOR_GLITCHES);
	CHECK_SETS("glitches", "major_glitches", logic, RANDO_LOGIC_MAJOR_GLITCHES);
	CHECK_SETS("glitches", "no_logic", logic, RANDO_LOGIC_NO_LOGIC);
	CHECK_SETS("item_placement", "advanced", item_placement, RANDO_PLACEMENT_ADVANCED);
	CHECK_SETS("item_placement", "basic", item_placement, RANDO_PLACEMENT_BASIC);
	CHECK_SETS("dungeon_items", "standard", dungeon_items, RANDO_DUNGEON_ITEMS_STANDARD);
	CHECK_SETS("dungeon_items", "mc", dungeon_items, RANDO_DUNGEON_ITEMS_MC);
	CHECK_SETS("dungeon_items", "mcs", dungeon_items, RANDO_DUNGEON_ITEMS_MCS);
	CHECK_SETS("dungeon_items", "full", dungeon_items, RANDO_DUNGEON_ITEMS_FULL);
	CHECK_SETS("accessibility", "items", accessibility, RANDO_ACCESSIBILITY_ITEMS);
	CHECK_SETS("accessibility", "locations", accessibility, RANDO_ACCESSIBILITY_LOCATIONS);
	CHECK_SETS("accessibility", "none", accessibility, RANDO_ACCESSIBILITY_NONE);
	CHECK_SETS("goal", "ganon", goal, RANDO_GOAL_GANON);
	CHECK_SETS("goal", "fast_ganon", goal, RANDO_GOAL_FAST_GANON);
	CHECK_SETS("goal", "dungeons", goal, RANDO_GOAL_DUNGEONS);
	CHECK_SETS("goal", "pedestal", goal, RANDO_GOAL_PEDESTAL);
	CHECK_SETS("goal", "triforce-hunt", goal, RANDO_GOAL_TRIFORCE_HUNT);
	CHECK_SETS("goal", "ganonhunt", goal, RANDO_GOAL_GANONHUNT);
	CHECK_SETS("goal", "completionist", goal, RANDO_GOAL_COMPLETIONIST);
	CHECK_SETS("state", "open", mode_state, RANDO_STATE_OPEN);
	CHECK_SETS("state", "standard", mode_state, RANDO_STATE_STANDARD);
	CHECK_SETS("state", "inverted", mode_state, RANDO_STATE_INVERTED);
	CHECK_SETS("state", "retro", mode_state, RANDO_STATE_RETRO);
	CHECK_SETS("weapons", "randomized", mode_weapons, RANDO_WEAPONS_RANDOMIZED);
	CHECK_SETS("weapons", "assured", mode_weapons, RANDO_WEAPONS_ASSURED);
	CHECK_SETS("weapons", "vanilla", mode_weapons, RANDO_WEAPONS_VANILLA);
	CHECK_SETS("weapons", "swordless", mode_weapons, RANDO_WEAPONS_SWORDLESS);
	CHECK_SETS("item_pool", "normal", item_pool, RANDO_ITEM_POOL_NORMAL);
	CHECK_SETS("item_pool", "hard", item_pool, RANDO_ITEM_POOL_HARD);
	CHECK_SETS("item_pool", "expert", item_pool, RANDO_ITEM_POOL_EXPERT);
	CHECK_SETS("item_functionality", "normal", item_functionality, RANDO_ITEM_FUNCTIONALITY_NORMAL);
	CHECK_SETS("item_functionality", "hard", item_functionality, RANDO_ITEM_FUNCTIONALITY_HARD);
	CHECK_SETS("item_functionality", "expert", item_functionality, RANDO_ITEM_FUNCTIONALITY_EXPERT);

	for (n = 0; n < 8; n++) {
		CHECK(options_after("crystals_tower", counts[n]).crystals_tower == (RandoCrystals)n);
		CHECK(options_after("crystals_ganon", counts[n]).crystals_ganon == (RandoCrystals)n);
	}
	CHECK(options_after("crystals_tower", "random").crystals_tower == RANDO_CRYSTALS_RANDOM);
	CHECK(options_after("crystals_ganon", "random").crystals_ganon == RANDO_CRYSTALS_RANDOM);
}

/* The oracle wrapper passes itemPlacement `advanced` under crowd_control, whatever item_placement says. */
static void check_crowd_control(void)
{
	RandoOptions options;
	RandoConfig expected, actual;

	CHECK_SETS("item_pool", "crowd_control", item_pool, RANDO_ITEM_POOL_CROWD_CONTROL);
	rando_options_init(&options);
	CHECK(rando_options_set(&options, "item_placement", "basic"));
	CHECK(rando_options_set(&options, "item_pool", "crowd_control"));
	baseline_config(&expected);
	expected.item_pool = RANDO_ITEM_POOL_CROWD_CONTROL;
	rando_config_from_options(&options, 7, 7, &actual);
	CHECK(memcmp(&expected, &actual, sizeof actual) == 0);
}

static void check_resolved_crystals(void)
{
	RandoOptions options;
	RandoConfig config;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "crystals_ganon", "random"));
	rando_config_from_options(&options, 2, 5, &config);
	CHECK(config.crystals_ganon == 2);
	CHECK(config.crystals_tower == 5);
}

static void check_rejected(const char *key, const char *value)
{
	RandoOptions options, before;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "goal", "pedestal"));
	before = options;
	if (rando_options_set(&options, key, value))
		test_fail(TEST, "%s=%s accepted", key, value);
	if (memcmp(&before, &options, sizeof options) != 0)
		test_fail(TEST, "%s=%s changed the options", key, value);
}

static void check_rejections(void)
{
	check_rejected("goals", "ganon");
	check_rejected("Goal", "ganon");
	check_rejected("", "");
	check_rejected("goal", "triforce_hunt");
	check_rejected("goal", "Ganon");
	check_rejected("goal", "");
	check_rejected("crystals_tower", "8");
	check_rejected("crystals_tower", "07");
	check_rejected("item_functionality", "crowd_control");
	check_rejected("item_pool", "superexpert");
}

static void check_keys(void)
{
	int k;

	for (k = 0; k < RANDO_OPTION_KEY_COUNT; k++)
		CHECK(rando_option_key_find(rando_option_key_name((RandoOptionKey)k)) == (RandoOptionKey)k);
	CHECK(rando_option_key_find("glitches") == RANDO_OPTION_GLITCHES);
	CHECK(rando_option_key_find("item_functionality") == RANDO_OPTION_ITEM_FUNCTIONALITY);
	CHECK(rando_option_key_find("logic") == RANDO_OPTION_KEY_COUNT);
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	check_baseline();
	check_every_value();
	check_crowd_control();
	check_resolved_crystals();
	check_rejections();
	check_keys();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
