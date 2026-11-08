#include "rando_ladx_name.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_ladx_name"
#define SEED_A "e8515e7064fb5fd6458e72d56122673b"
#define SEED_ZERO "00000000000000000000000000000000"

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

static void set(LadxSettings *s, const char *assignment)
{
	if (ladx_settings_set(s, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
}

/* Codes pinned from Python's zlib.crc32 of the same text: seed=<hex>, then key=value per setting in table order. */
static void test_code_is_crc_of_seed_and_settings(void)
{
	LadxSettings s;

	ladx_settings_defaults(&s);
	CHECK(rando_ladx_name_code(&s, SEED_A) == 0x0D21D7u);
	set(&s, "logic=hell");
	set(&s, "goal=maze");
	CHECK(rando_ladx_name_code(&s, SEED_ZERO) == 0x46940Au);
}

static void test_every_logic_and_goal_word(void)
{
	static const char *const logic[] = {"Casual", "Normal", "Hard", "Glitched", "Hell"};
	static const struct {
		const char *value, *word;
	} goals[] = {{"vanilla", "Vanilla"},	     {"instruments", "Instruments"}, {"specific", "Specific"},
		     {"open", "Open"},		     {"open-4", "Short"},	     {"5-8", "Long"},
		     {"seashells", "Seashells"},     {"bingo", "Bingo"},	     {"bingo-double", "DoubleBingo"},
		     {"bingo-triple", "TripleBingo"}, {"bingo-full", "Bingo25"},     {"maze", "SignMaze"}};
	LadxSettingId goal_id = ladx_setting_find("goal"), logic_id = ladx_setting_find("logic");
	size_t i;

	CHECK(ladx_setting_value_count(logic_id) == 5);
	for (i = 0; i < 5; i++)
		check_str("logic word", rando_ladx_name_logic((int)i), logic[i]);
	CHECK(ladx_setting_value_count(goal_id) == (int)(sizeof goals / sizeof goals[0]));
	for (i = 0; i < sizeof goals / sizeof goals[0]; i++)
		check_str(goals[i].value, rando_ladx_name_goal(ladx_setting_value_find(goal_id, goals[i].value)),
			  goals[i].word);
}

static void test_make(void)
{
	LadxSettings s;
	RandoName n;

	ladx_settings_defaults(&s);
	rando_ladx_name_make(&s, 0x1CA10Au, 1, &n);
	check_str("code", n.code, "1CA10A");
	check_str("rom", n.rom, "Normal-Vanilla-1CA10A.gbc");
	set(&s, "logic=glitched");
	set(&s, "goal=bingo-double");
	rando_ladx_name_make(&s, 0x00000Fu, 99, &n);
	check_str("code", n.code, "00000F-99");
	check_str("rom", n.rom, "Glitched-DoubleBingo-00000F-99.gbc");
	CHECK(strlen(n.rom) < RANDO_NAME_ROM_MAX);
	rando_ladx_name_make(&s, 0xABCDEFu, 2, &n);
	check_str("rom", n.rom, "Glitched-DoubleBingo-ABCDEF-2.gbc");
}

typedef struct Taken {
	int taken, calls;
} Taken;

static bool first_n_taken(void *ctx, const RandoName *candidate)
{
	Taken *t = ctx;

	(void)candidate;
	return t->calls++ < t->taken;
}

static void test_choose_collisions(void)
{
	LadxSettings s;
	RandoName n;
	Taken t = {0, 0};

	ladx_settings_defaults(&s);
	CHECK(rando_ladx_name_choose(&s, SEED_A, first_n_taken, &t, &n));
	check_str("free", n.rom, "Normal-Vanilla-0D21D7.gbc");
	t.taken = 1;
	t.calls = 0;
	CHECK(rando_ladx_name_choose(&s, SEED_A, first_n_taken, &t, &n));
	check_str("second", n.code, "0D21D7-2");
	t.taken = 98;
	t.calls = 0;
	CHECK(rando_ladx_name_choose(&s, SEED_A, first_n_taken, &t, &n));
	check_str("last", n.code, "0D21D7-99");
	t.taken = 99;
	t.calls = 0;
	CHECK(!rando_ladx_name_choose(&s, SEED_A, first_n_taken, &t, &n));
	CHECK(t.calls == 99);
}

int main(void)
{
	test_code_is_crc_of_seed_and_settings();
	test_every_logic_and_goal_word();
	test_make();
	test_choose_collisions();
	printf("test_ladx_name: ok\n");
	return 0;
}
