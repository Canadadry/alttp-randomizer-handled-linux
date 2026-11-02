#include "rando_assemble.h"
#include "rando_options.h"
#include "rando_settings.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_settings"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static const struct {
	const char *key;
	int count;
} expected_table[] = {
	{"glitches", 5},     {"item_placement", 2}, {"dungeon_items", 4}, {"accessibility", 3}, {"goal", 7},
	{"crystals_tower", 9}, {"crystals_ganon", 9}, {"state", 4},	  {"weapons", 4},	 {"item_pool", 4},
	{"item_functionality", 3}, {"hints", 2},     {"heart_beep", 5},
};

static void check_table(void)
{
	int id;

	CHECK(RANDO_SETTING_COUNT == 13);
	CHECK(sizeof expected_table / sizeof expected_table[0] == RANDO_SETTING_COUNT);
	CHECK((int)RANDO_SETTING_GLITCHES == (int)RANDO_OPTION_GLITCHES);
	CHECK((int)RANDO_SETTING_ITEM_FUNCTIONALITY == (int)RANDO_OPTION_ITEM_FUNCTIONALITY);
	CHECK(RANDO_SETTING_HINTS == 11 && RANDO_SETTING_HEART_BEEP == 12);
	for (id = 0; id < RANDO_SETTING_COUNT; id++) {
		if (strcmp(rando_setting_key((RandoSettingId)id), expected_table[id].key) != 0)
			test_fail(TEST, "setting %d: key %s, expected %s", id, rando_setting_key((RandoSettingId)id),
				  expected_table[id].key);
		if (rando_setting_value_count((RandoSettingId)id) != expected_table[id].count)
			test_fail(TEST, "%s: %d values, expected %d", expected_table[id].key,
				  rando_setting_value_count((RandoSettingId)id), expected_table[id].count);
	}
}

/* The old public functions are the reference for every name. */
static const char *old_value_name(int id, int value)
{
	if (id < RANDO_OPTION_KEY_COUNT)
		return rando_option_value_name((RandoOptionKey)id, value);
	if (id == RANDO_SETTING_HINTS)
		return rando_hints_name(value == 0);
	return rando_heart_beep_name((RandoHeartBeep)value);
}

static void check_names(void)
{
	int id, v;

	for (id = 0; id < RANDO_SETTING_COUNT; id++) {
		RandoSettingId s = (RandoSettingId)id;

		if (id < RANDO_OPTION_KEY_COUNT)
			CHECK(strcmp(rando_setting_key(s), rando_option_key_name((RandoOptionKey)id)) == 0);
		CHECK(rando_setting_find(rando_setting_key(s)) == s);
		for (v = 0; v < rando_setting_value_count(s); v++) {
			const char *name = rando_setting_value_name(s, v);

			if (strcmp(name, old_value_name(id, v)) != 0)
				test_fail(TEST, "%s value %d: %s, expected %s", rando_setting_key(s), v, name,
					  old_value_name(id, v));
			if (rando_setting_value_find(s, name) != v)
				test_fail(TEST, "%s: value_find(%s) is not %d", rando_setting_key(s), name, v);
		}
		CHECK(rando_setting_value_find(s, "") == -1);
		CHECK(rando_setting_value_find(s, "bogus") == -1);
	}
	CHECK(rando_setting_value_find(RANDO_SETTING_HINTS, "On") == -1);
	CHECK(rando_setting_value_find(RANDO_SETTING_CRYSTALS_TOWER, "8") == -1);
	CHECK(rando_setting_find("bogus") == RANDO_SETTING_COUNT);
	CHECK(rando_setting_find("") == RANDO_SETTING_COUNT);
	CHECK(rando_setting_find("seed") == RANDO_SETTING_COUNT);
	CHECK(rando_setting_find("spoiler") == RANDO_SETTING_COUNT);
}

static void check_defaults(void)
{
	RandoOptions options;
	RandoAssembleRequest request;
	int id;

	rando_options_init(&options);
	rando_assemble_request_init(&request);
	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		if (rando_setting_default((RandoSettingId)id) !=
		    rando_setting_get(&options, request.heart_beep, (RandoSettingId)id))
			test_fail(TEST, "%s: default %d differs from rando_options_init's",
				  rando_setting_key((RandoSettingId)id), rando_setting_default((RandoSettingId)id));
	CHECK(rando_setting_default(RANDO_SETTING_CRYSTALS_TOWER) == RANDO_CRYSTALS_7);
	CHECK(rando_setting_default(RANDO_SETTING_HINTS) == 0);
	CHECK(rando_setting_default(RANDO_SETTING_HEART_BEEP) == RANDO_HEART_BEEP_HALF);
}

static void check_get_set(void)
{
	int id, v, other;

	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		for (v = 0; v < rando_setting_value_count((RandoSettingId)id); v++) {
			RandoOptions options, before;
			RandoHeartBeep heart_beep = RANDO_HEART_BEEP_HALF, heart_before;

			rando_options_init(&options);
			before = options;
			heart_before = heart_beep;
			rando_setting_set(&options, &heart_beep, (RandoSettingId)id, v);
			if (rando_setting_get(&options, heart_beep, (RandoSettingId)id) != v)
				test_fail(TEST, "%s=%d does not read back", rando_setting_key((RandoSettingId)id), v);
			for (other = 0; other < RANDO_SETTING_COUNT; other++)
				if (other != id && rando_setting_get(&options, heart_beep, (RandoSettingId)other) !=
							   rando_setting_get(&before, heart_before, (RandoSettingId)other))
					test_fail(TEST, "%s=%d changed %s", rando_setting_key((RandoSettingId)id), v,
						  rando_setting_key((RandoSettingId)other));
		}
	{
		RandoOptions options;
		RandoHeartBeep heart_beep = RANDO_HEART_BEEP_HALF;

		rando_options_init(&options);
		rando_setting_set(&options, &heart_beep, RANDO_SETTING_HINTS, 1);
		CHECK(!options.hints);
		rando_setting_set(&options, &heart_beep, RANDO_SETTING_HEART_BEEP, RANDO_HEART_BEEP_DOUBLE);
		CHECK(heart_beep == RANDO_HEART_BEEP_DOUBLE);
		rando_setting_set(&options, &heart_beep, RANDO_SETTING_GOAL, RANDO_OPTION_GOAL_PEDESTAL);
		CHECK(options.goal == RANDO_OPTION_GOAL_PEDESTAL);
	}
}

/* The record's settings keys, in the order its text lists them. */
static const char *const record_order[] = {
	"accessibility", "crystals_ganon", "crystals_tower", "dungeon_items", "glitches", "goal", "heart_beep",
	"hints", "item_functionality", "item_placement", "item_pool", "state", "weapons",
};

static void check_sorted(void)
{
	int rank;

	for (rank = 0; rank < RANDO_SETTING_COUNT; rank++)
		if (strcmp(rando_setting_key(rando_setting_sorted(rank)), record_order[rank]) != 0)
			test_fail(TEST, "sorted rank %d: %s, expected %s", rank,
				  rando_setting_key(rando_setting_sorted(rank)), record_order[rank]);
}

static void accepts(const char *text, uint32_t expected)
{
	uint32_t out = 12345;

	if (!rando_parse_uint32(text, strlen(text), &out) || out != expected)
		test_fail(TEST, "uint32 \"%s\" not read as %lu", text, (unsigned long)expected);
}

static void refuses(const char *text)
{
	uint32_t out = 12345;

	if (rando_parse_uint32(text, strlen(text), &out))
		test_fail(TEST, "uint32 \"%s\" accepted", text);
}

static void check_uint32(void)
{
	accepts("0", 0);
	accepts("1", 1);
	accepts("4294967295", 4294967295u);
	accepts("4294967295\n", 4294967295u);
	accepts("0000000042", 42);
	accepts("7\n", 7);
	refuses("");
	refuses("\n");
	refuses("12345678901");
	refuses("4294967296");
	refuses("9999999999");
	refuses("1\n\n");
	refuses("-1");
	refuses("+1");
	refuses(" 1");
	refuses("1 ");
	refuses("1a");
	{
		uint32_t out = 9;

		CHECK(rando_parse_uint32("5x", 1, &out) && out == 5);
	}
}

static void check_unsupported(void)
{
	int i;

	CHECK(rando_setting_unsupported_count() == 0);
	for (i = 0; i < rando_setting_unsupported_count(); i++) {
		const RandoSettingPair *pair = rando_setting_unsupported(i);

		CHECK(pair->a < RANDO_SETTING_COUNT && pair->b < RANDO_SETTING_COUNT && pair->a != pair->b);
		CHECK(pair->value_a >= 0 && pair->value_a < rando_setting_value_count(pair->a));
		CHECK(pair->value_b >= 0 && pair->value_b < rando_setting_value_count(pair->b));
	}
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	check_table();
	check_names();
	check_defaults();
	check_get_set();
	check_sorted();
	check_uint32();
	check_unsupported();
	printf("%s: passed\n", TEST);
	return 0;
}
