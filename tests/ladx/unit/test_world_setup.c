#include "ladx_randomizer.h"
#include "ladx_settings.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_world_setup"

static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void settings_of(LadxSettings *s, const char *assignment)
{
	ladx_settings_defaults(s);
	if (ladx_settings_set(s, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
	ladx_settings_validate(s);
}

static LadxRandomizer r;

/* rnd's MT words when ItemPool was entered; 0 when it did not return. */
static uint64_t words_before(const LadxRandomizer *run)
{
	uint64_t before = 0;

	ladx_randomizer_pool(run, &before, NULL);
	return before;
}

/*
 * item_pools.txt's three goal=maze runs: buildMaze crashes on the third
 * seed; the other two reach ItemPool with words_before 80 and 77.
 */
static void test_maze_seeds(void)
{
	static const uint8_t seed_a[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	static const uint8_t seed_b[] = {0x4c, 0x41, 0x44, 0x58, 0x52};
	static const uint8_t seed_c[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
					 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	const LadxWorldSetup *ws;
	const LadxOutcome *o;
	LadxSettings s;

	settings_of(&s, "goal=maze");

	check(ladx_randomize(&r, &s, seed_a, sizeof seed_a, NULL, NULL) == LADX_RANDOMIZE_OK, "maze seed a: run");
	o = ladx_randomizer_outcome(&r);
	check(ladx_randomizer_world_setup(&r) != NULL, "maze seed a: buildMaze returned");
	check(words_before(&r) == 80, "maze seed a: words_before 80");
	ws = ladx_randomizer_world_setup(&r);
	check(ws && ws->goal == LADX_GOAL_MAZE && ws->sign_count == LADX_SIGN_MAZE_SIGNS,
	      "maze seed a: goal maze, 22 signs");

	check(ladx_randomize(&r, &s, seed_b, sizeof seed_b, NULL, NULL) == LADX_RANDOMIZE_OK, "maze seed b: run");
	check(ladx_randomizer_world_setup(&r) != NULL, "maze seed b: buildMaze returned");
	check(words_before(&r) == 77, "maze seed b: words_before 77");

	check(ladx_randomize(&r, &s, seed_c, sizeof seed_c, NULL, NULL) == LADX_RANDOMIZE_OK, "maze seed c: run");
	o = ladx_randomizer_outcome(&r);
	check(o->kind == LADX_OUTCOME_CRASH && strcmp(o->type, "IndexError") == 0 &&
		      strcmp(o->message, "Cannot choose from an empty sequence") == 0,
	      "maze seed c: crash IndexError Cannot choose from an empty sequence");
	check(ladx_randomizer_world_setup(&r) == NULL, "maze seed c: no world setup");
}

static void sign_maze_text(const LadxWorldSetup *ws, char *out, size_t cap)
{
	size_t len = 0;
	int i;

	out[0] = '\0';
	for (i = 0; i < ws->sign_count && len < cap; i++)
		len += (size_t)snprintf(out + len, cap - len, i ? ",%d:%d" : "%d:%d", ws->signs[i].x, ws->signs[i].y);
	for (i = 0; i + 1 < ws->sign_count && len < cap; i++)
		len += (size_t)snprintf(out + len, cap - len, i ? ",%d" : ";%d", ws->directions[i]);
}

static void seed_of(const char *hex, uint8_t seed[16])
{
	int i;
	unsigned v;

	for (i = 0; i < 16; i++) {
		if (sscanf(hex + 2 * i, "%2x", &v) != 1)
			test_fail(TEST, "bad seed %s", hex);
		seed[i] = (uint8_t)v;
	}
}

/*
 * maze.buildMaze(random.Random(seed)) as CPython ran it at 4325207e: a seed
 * that rolls exactly 20 for a flying sign, one that retries six times.
 */
static void test_maze_vectors(void)
{
	static const struct {
		const char *seed, *sign_maze, *what;
	} vectors[] = {
		{"0d4a15118e78b9b7eb751f8ca4247036",
		 "27:84,132:84,132:26,101:26,101:76,86:76,18:76,18:28,72:28,72:35,63:35,48:35,48:121,113:121,113:106,"
		 "46:106,46:57,46:19,18:19,18:5,56:5,65:5;2,1,3,0,3,3,1,130,0,3,3,0,2,1,131,1,1,3,129,2,2",
		 "a roll of exactly 20 is not a flying sign"},
		{"7480949437dd197b9ed2fcbdcbb43b52",
		 "27:84,73:84,101:84,101:53,101:1,111:1,111:123,136:123,136:26,118:26,88:26,88:2,78:2,78:21,82:21,"
		 "82:106,24:106,24:99,36:99,36:45,65:45,65:5;2,2,1,1,2,0,2,1,3,3,1,3,128,2,0,131,1,130,1,2,1",
		 "six tries without a connection to the egg"},
	};
	char got[1024], what[128];
	const LadxWorldSetup *ws;
	LadxSettings s;
	uint8_t seed[16];
	size_t i;

	settings_of(&s, "goal=maze");
	for (i = 0; i < sizeof vectors / sizeof vectors[0]; i++) {
		seed_of(vectors[i].seed, seed);
		snprintf(what, sizeof what, "maze vector %s: %s", vectors[i].seed, vectors[i].what);
		check(ladx_randomize(&r, &s, seed, sizeof seed, NULL, NULL) == LADX_RANDOMIZE_OK, what);
		ws = ladx_randomizer_world_setup(&r);
		check(ws != NULL, what);
		sign_maze_text(ws, got, sizeof got);
		if (strcmp(got, vectors[i].sign_maze) != 0)
			test_fail(TEST, "%s\n  got  %s\n  want %s", what, got, vectors[i].sign_maze);
		checks++;
	}
}

/* WorldSetup().randomize(settings, random.Random(seed)) as CPython ran it: goal=specific, goalcount=0 is 1 instrument. */
static void test_specific_count_zero(void)
{
	static const uint8_t seed[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
				       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
	const LadxWorldSetup *ws;
	LadxSettings s;

	settings_of(&s, "goal=specific");
	if (ladx_settings_set(&s, "goalcount=0") != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set goalcount=0");
	ladx_settings_validate(&s);
	check(ladx_randomize(&r, &s, seed, sizeof seed, NULL, NULL) == LADX_RANDOMIZE_OK, "specific, 0: run");
	ws = ladx_randomizer_world_setup(&r);
	check(ws && ws->goal == LADX_GOAL_SPECIFIC && strcmp(ws->goal_instruments, "2") == 0 && ws->goal_count == 1 &&
		      ws->multichest == LADX_ITEM_MEDICINE,
	      "specific, 0: goal =2, goal_count 1, multichest MEDICINE");
}

/* goal=bingo is the scope cut goal-bingo: the library refuses it, drawing nothing past the seed. */
static void test_bingo_unsupported(void)
{
	static const uint8_t seed[] = {0x01};
	LadxSettings s;

	settings_of(&s, "goal=bingo");
	check(ladx_randomize(&r, &s, seed, sizeof seed, NULL, NULL) == LADX_RANDOMIZE_UNSUPPORTED,
	      "goal=bingo: unsupported");
}

int main(void)
{
	test_maze_seeds();
	test_maze_vectors();
	test_specific_count_zero();
	test_bingo_unsupported();
	printf("%s: goal=maze's crash and words on item_pools.txt's seeds, buildMaze's and goal=specific's vectors, goal=bingo unsupported (%d checks)\n", TEST,
	       checks);
	return 0;
}
