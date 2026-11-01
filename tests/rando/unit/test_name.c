#include "rando_name.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_name"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void check_str(const char *what, const char *got, const char *expected)
{
	if (strcmp(got, expected) != 0)
		test_fail(TEST, "%s: \"%s\", expected \"%s\"", what, got, expected);
}

/* ^[A-Za-z]+-[A-Za-z]+-[0-9A-F]{6}(-[1-9][0-9]?)?\.sfc$ */
static bool rom_name_shaped(const char *s)
{
	int part, n;

	for (part = 0; part < 2; part++) {
		for (n = 0; (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'); s++)
			n++;
		if (n == 0 || *s++ != '-')
			return false;
	}
	for (n = 0; n < 6; n++, s++)
		if (!((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'F')))
			return false;
	if (*s == '-') {
		s++;
		if (*s < '1' || *s > '9')
			return false;
		s++;
		if (*s >= '0' && *s <= '9')
			s++;
	}
	return strcmp(s, ".sfc") == 0;
}

static void test_every_mode_and_goal(void)
{
	static const char *const expected[4][7] = {
		{"Open-Ganon-1CA10A.sfc", "Open-FastGanon-1CA10A.sfc", "Open-Dungeons-1CA10A.sfc",
		 "Open-Pedestal-1CA10A.sfc", "Open-TriforceHunt-1CA10A.sfc", "Open-GanonHunt-1CA10A.sfc",
		 "Open-Completionist-1CA10A.sfc"},
		{"Standard-Ganon-1CA10A.sfc", "Standard-FastGanon-1CA10A.sfc", "Standard-Dungeons-1CA10A.sfc",
		 "Standard-Pedestal-1CA10A.sfc", "Standard-TriforceHunt-1CA10A.sfc", "Standard-GanonHunt-1CA10A.sfc",
		 "Standard-Completionist-1CA10A.sfc"},
		{"Inverted-Ganon-1CA10A.sfc", "Inverted-FastGanon-1CA10A.sfc", "Inverted-Dungeons-1CA10A.sfc",
		 "Inverted-Pedestal-1CA10A.sfc", "Inverted-TriforceHunt-1CA10A.sfc", "Inverted-GanonHunt-1CA10A.sfc",
		 "Inverted-Completionist-1CA10A.sfc"},
		{"Retro-Ganon-1CA10A.sfc", "Retro-FastGanon-1CA10A.sfc", "Retro-Dungeons-1CA10A.sfc",
		 "Retro-Pedestal-1CA10A.sfc", "Retro-TriforceHunt-1CA10A.sfc", "Retro-GanonHunt-1CA10A.sfc",
		 "Retro-Completionist-1CA10A.sfc"},
	};
	RandoName names[4][7];
	RandoOptions options;
	int state, goal, other;

	rando_options_init(&options);
	for (state = 0; state < 4; state++)
		for (goal = 0; goal < 7; goal++) {
			RandoName *name = &names[state][goal];

			options.state = (RandoOptionState)state;
			options.goal = (RandoOptionGoal)goal;
			rando_name_make(&options, 0x1CA10Au, 1, name);
			check_str("rom", name->rom, expected[state][goal]);
			check_str("code", name->code, "1CA10A");
			CHECK(rom_name_shaped(name->rom));
			CHECK(strlen(name->rom) < RANDO_NAME_ROM_MAX);
		}
	for (state = 0; state < 28; state++)
		for (other = state + 1; other < 28; other++)
			CHECK(strcmp(names[state / 7][state % 7].rom, names[other / 7][other % 7].rom) != 0);
}

static void check_code(const char *what, const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed,
		       const char *expected)
{
	RandoName name;

	rando_name_make(options, rando_name_code(options, heart_beep, seed), 1, &name);
	check_str(what, name.code, expected);
}

/* The PRD's worked example: CRC32 DB1CA10A of the baseline text with seed 1. */
static void test_baseline_code(void)
{
	RandoOptions options;
	RandoName name;

	rando_options_init(&options);
	check_code("baseline, seed 1", &options, RANDO_HEART_BEEP_HALF, 1, "1CA10A");
	rando_name_make(&options, rando_name_code(&options, RANDO_HEART_BEEP_HALF, 1), 1, &name);
	check_str("baseline rom", name.rom, "Open-Ganon-1CA10A.sfc");
}

static void set(RandoOptions *options, const char *key, const char *value)
{
	if (!rando_options_set(options, key, value))
		test_fail(TEST, "%s=%s rejected", key, value);
}

static void test_all_non_default_code(void)
{
	RandoOptions options;

	rando_options_init(&options);
	set(&options, "glitches", "overworld_glitches");
	set(&options, "item_placement", "basic");
	set(&options, "dungeon_items", "mc");
	set(&options, "accessibility", "none");
	set(&options, "goal", "fast_ganon");
	set(&options, "crystals_tower", "5");
	set(&options, "crystals_ganon", "random");
	set(&options, "state", "standard");
	set(&options, "weapons", "assured");
	set(&options, "item_pool", "hard");
	set(&options, "item_functionality", "hard");
	options.hints = false;
	check_code("all non-default, seed 1", &options, RANDO_HEART_BEEP_QUARTER, 1, "A179E3");
}

static void test_largest_seed_code(void)
{
	RandoOptions options;

	rando_options_init(&options);
	check_code("baseline, seed 4294967295", &options, RANDO_HEART_BEEP_HALF, 4294967295u, "BA9976");
}

/* Seed 15 is the first whose code starts with 0. */
static void test_code_keeps_leading_zero(void)
{
	RandoOptions options;

	rando_options_init(&options);
	check_code("baseline, seed 15", &options, RANDO_HEART_BEEP_HALF, 15, "0833E7");
}

/* From the baseline with seed 1, one change at a time. */
static void test_every_setting_changes_code(void)
{
	static const struct {
		const char *key, *value, *code;
	} changes[] = {
		{"glitches", "major_glitches", "10B3B0"},  {"item_placement", "basic", "CB2DBD"},
		{"dungeon_items", "full", "A78413"},	    {"accessibility", "locations", "EE6CF2"},
		{"goal", "pedestal", "3B1C47"},		    {"crystals_tower", "0", "CC734C"},
		{"crystals_ganon", "random", "70F04B"},	    {"state", "inverted", "4E83F8"},
		{"weapons", "swordless", "1A22AA"},	    {"item_pool", "expert", "EC9392"},
		{"item_functionality", "expert", "B4225C"},
	};
	RandoOptions options;
	size_t i;

	CHECK(sizeof changes / sizeof changes[0] == RANDO_OPTION_KEY_COUNT);
	for (i = 0; i < sizeof changes / sizeof changes[0]; i++) {
		rando_options_init(&options);
		set(&options, changes[i].key, changes[i].value);
		check_code(changes[i].key, &options, RANDO_HEART_BEEP_HALF, 1, changes[i].code);
	}
	rando_options_init(&options);
	check_code("seed 2", &options, RANDO_HEART_BEEP_HALF, 2, "910B6B");
	check_code("heart_beep normal", &options, RANDO_HEART_BEEP_NORMAL, 1, "F3A1CB");
	options.hints = false;
	check_code("hints off", &options, RANDO_HEART_BEEP_HALF, 1, "73B29D");
}

static void test_suffixes(void)
{
	RandoOptions options;
	RandoName name;

	rando_options_init(&options);
	rando_name_make(&options, 0x1CA10Au, 2, &name);
	check_str("suffix 2 code", name.code, "1CA10A-2");
	check_str("suffix 2 rom", name.rom, "Open-Ganon-1CA10A-2.sfc");
	options.state = RANDO_OPTION_STATE_STANDARD;
	options.goal = RANDO_OPTION_GOAL_COMPLETIONIST;
	rando_name_make(&options, 0x1CA10Au, 99, &name);
	check_str("suffix 99 code", name.code, "1CA10A-99");
	check_str("suffix 99 rom", name.rom, "Standard-Completionist-1CA10A-99.sfc");
	CHECK(rom_name_shaped(name.rom));
	CHECK(strlen(name.rom) < RANDO_NAME_ROM_MAX);
}

/* Marks the first `count` candidates taken, by ROM name or, with by_code, by code alone. */
typedef struct FakeTaken {
	int count;
	bool by_code;
	int calls;
	char seen[RANDO_NAME_SUFFIX_MAX + 1][RANDO_NAME_CODE_MAX];
	char taken_rom[RANDO_NAME_SUFFIX_MAX][RANDO_NAME_ROM_MAX];
	char taken_code[RANDO_NAME_SUFFIX_MAX][RANDO_NAME_CODE_MAX];
} FakeTaken;

static bool fake_taken(void *ctx, const RandoName *candidate)
{
	FakeTaken *fake = ctx;
	int i;

	if (fake->calls > RANDO_NAME_SUFFIX_MAX)
		test_fail(TEST, "taken called more than %d times", RANDO_NAME_SUFFIX_MAX);
	strcpy(fake->seen[fake->calls++], candidate->code);
	for (i = 0; i < fake->count; i++)
		if (fake->by_code ? strcmp(fake->taken_code[i], candidate->code) == 0
				  : strcmp(fake->taken_rom[i], candidate->rom) == 0)
			return true;
	return false;
}

static void fake_init(FakeTaken *fake, const RandoOptions *options, int count, bool by_code)
{
	RandoName name;
	int i;

	memset(fake, 0, sizeof *fake);
	fake->count = count;
	fake->by_code = by_code;
	for (i = 0; i < count; i++) {
		rando_name_make(options, 0x1CA10Au, i + 1, &name);
		strcpy(fake->taken_rom[i], name.rom);
		strcpy(fake->taken_code[i], name.code);
	}
}

static void check_choose(const char *what, int count, bool by_code, const char *expected_code,
			 const char *expected_rom)
{
	RandoOptions options;
	RandoName name, nth;
	FakeTaken fake;
	int i;

	rando_options_init(&options);
	fake_init(&fake, &options, count, by_code);
	if (!rando_name_choose(&options, RANDO_HEART_BEEP_HALF, 1, fake_taken, &fake, &name))
		test_fail(TEST, "%s: no name chosen", what);
	check_str(what, name.code, expected_code);
	check_str(what, name.rom, expected_rom);
	if (fake.calls != count + 1)
		test_fail(TEST, "%s: taken called %d times, expected %d", what, fake.calls, count + 1);
	for (i = 0; i < fake.calls; i++) {
		rando_name_make(&options, 0x1CA10Au, i + 1, &nth);
		check_str("candidate order", fake.seen[i], nth.code);
	}
}

static void test_choose(void)
{
	check_choose("nothing taken", 0, false, "1CA10A", "Open-Ganon-1CA10A.sfc");
	check_choose("first taken", 1, false, "1CA10A-2", "Open-Ganon-1CA10A-2.sfc");
	check_choose("first four taken", 4, false, "1CA10A-5", "Open-Ganon-1CA10A-5.sfc");
	check_choose("code taken by a lone record", 1, true, "1CA10A-2", "Open-Ganon-1CA10A-2.sfc");
	check_choose("98 taken", 98, false, "1CA10A-99", "Open-Ganon-1CA10A-99.sfc");
}

static void test_choose_all_taken(void)
{
	RandoOptions options;
	RandoName name;
	FakeTaken fake;

	rando_options_init(&options);
	fake_init(&fake, &options, RANDO_NAME_SUFFIX_MAX, false);
	CHECK(!rando_name_choose(&options, RANDO_HEART_BEEP_HALF, 1, fake_taken, &fake, &name));
	CHECK(fake.calls == RANDO_NAME_SUFFIX_MAX);
}

static void test_code_valid(void)
{
	static const char *const accepted[] = {"1CA10A", "1CA10A-2", "1CA10A-9", "1CA10A-10", "1CA10A-99",
					       "000000", "FFFFFF"};
	static const char *const refused[] = {"1ca10a", "1CA10a", "1CA10",    "1CA10A0",  "1CA10A-1", "1CA10A-0",
					      "1CA10A-02", "1CA10A-100", "",       "1CA10A ", "1CA10A-",  "1CA10A-2x",
					      "1CA10G",	   "-1CA10A",	 "1CA10A--2"};
	size_t i;

	for (i = 0; i < sizeof accepted / sizeof accepted[0]; i++)
		if (!rando_name_code_valid(accepted[i]))
			test_fail(TEST, "code \"%s\" refused", accepted[i]);
	for (i = 0; i < sizeof refused / sizeof refused[0]; i++)
		if (rando_name_code_valid(refused[i]))
			test_fail(TEST, "code \"%s\" accepted", refused[i]);
}

/* Every code rando_name_make writes passes the check. */
static void test_made_codes_are_valid(void)
{
	RandoOptions options;
	RandoName name;
	int suffix;

	rando_options_init(&options);
	for (suffix = 1; suffix <= RANDO_NAME_SUFFIX_MAX; suffix++) {
		rando_name_make(&options, 0x0833E7u, suffix, &name);
		if (!rando_name_code_valid(name.code))
			test_fail(TEST, "made code \"%s\" refused", name.code);
		CHECK(rom_name_shaped(name.rom));
	}
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_every_mode_and_goal();
	test_baseline_code();
	test_all_non_default_code();
	test_largest_seed_code();
	test_code_keeps_leading_zero();
	test_every_setting_changes_code();
	test_suffixes();
	test_choose();
	test_choose_all_taken();
	test_code_valid();
	test_made_codes_are_valid();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
