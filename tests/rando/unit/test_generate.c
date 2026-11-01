#include "rando.h"
#include "rando_out.h"
#include "rando_playthrough.h"
#include "rando_spoiler.h"
#include "support.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_generate"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define TRACE_MAX 65536
#define STAGE_TABLE "stages.txt"

/* The hook's draws in trace.txt's format. */
typedef struct Trace {
	char text[TRACE_MAX];
	size_t len;
	int lines;
} Trace;

static void record(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min, int64_t max,
		   int64_t r)
{
	Trace *t = ctx;
	int n = snprintf(t->text + t->len, TRACE_MAX - t->len, "%" PRIu32 " %" PRIu32 " %s %" PRId64 " %" PRId64
			 " -> %" PRId64 "\n", attempt, draw, stage, min, max, r);

	if (n < 0 || (size_t)n >= TRACE_MAX - t->len)
		test_fail(TEST, "trace longer than %d bytes", TRACE_MAX);
	t->len += (size_t)n;
	t->lines++;
}

static RandoGen gen;
static RandoPlaythrough playthrough;

#define SPOILER_MAX (128 * 1024)

/* Reads at most SPOILER_MAX - 1 bytes of f into text, NUL-terminated. */
static void read_all(FILE *f, const char *what, char text[SPOILER_MAX])
{
	size_t n;

	if (!f)
		test_fail(TEST, "%s: cannot open", what);
	n = fread(text, 1, SPOILER_MAX, f);
	if (ferror(f) || n == SPOILER_MAX)
		test_fail(TEST, "%s: read error or longer than %d bytes", what, SPOILER_MAX - 1);
	text[n] = '\0';
	fclose(f);
}

#define BUILD_DIR "build"

/* rando_gen's outputs on a complete run: the full spoiler.json equals the golden case's, and the
 * attempts file holds attempts. */
static void check_complete_outputs(const char *case_dir, const char *what, const char *attempts)
{
	static char golden[SPOILER_MAX], spoiler[SPOILER_MAX];
	char path[256], error[RANDO_OUT_ERROR_MAX];
	FILE *f = tmpfile();

	CHECK(f != NULL);
	rando_playthrough(&gen.world, &playthrough);
	CHECK(rando_spoiler_write_json(f, &gen.world, &gen.options, &playthrough));
	rewind(f);
	read_all(f, "spoiler", spoiler);
	snprintf(path, sizeof path, "%s/spoiler.json", case_dir);
	read_all(fopen(path, "rb"), path, golden);
	if (strcmp(spoiler, golden) != 0)
		test_fail(TEST, "%s: spoiler.json differs from %s", what, path);
	if (!rando_attempts_write(BUILD_DIR, gen.attempt, error))
		test_fail(TEST, "%s", error);
	read_all(fopen(BUILD_DIR "/attempts", "rb"), BUILD_DIR "/attempts", spoiler);
	if (strcmp(spoiler, attempts) != 0)
		test_fail(TEST, "%s: attempts %s", what, spoiler);
}

static RandoStatus generate(const char *key, const char *value, uint32_t seed, Trace *trace)
{
	RandoOptions options;

	rando_options_init(&options);
	if (key)
		CHECK(rando_options_set(&options, key, value));
	memset(trace, 0, sizeof *trace);
	return rando_generate_until(&gen, &options, seed, 1, record, trace, RANDO_STAGE_PREPARE_WORLD);
}

/* The C draws must be the first lines of the case's golden trace; with next_stage, the golden's next
 * line must belong to that stage, GOLDEN_END meaning there is none. */
static const char GOLDEN_END[] = "";

static void check_golden_stop(const char *name, const Trace *trace, const char *next_stage)
{
	char path[256], golden[TRACE_MAX], next[128] = "", stage[64] = "";
	size_t len = 0;
	int lines = 0;
	FILE *f;

	snprintf(path, sizeof path, "cases/%s/trace.txt", name);
	f = fopen(path, "r");
	if (!f)
		test_fail(TEST, "%s: cannot open", path);
	while (lines < trace->lines && fgets(golden + len, (int)(sizeof golden - len), f)) {
		len += strlen(golden + len);
		lines++;
	}
	if (!fgets(next, sizeof next, f))
		next[0] = '\0';
	fclose(f);
	golden[len] = '\0';
	if (strcmp(trace->text, golden) != 0)
		test_fail(TEST, "%s: C draws\n%s\ndiffer from the golden's first %d lines\n%s", name, trace->text,
			  trace->lines, golden);
	if (next_stage == GOLDEN_END) {
		if (next[0] != '\0')
			test_fail(TEST, "%s: the golden goes on with \"%s\" after the C draws", name, next);
	} else if (next_stage && (sscanf(next, "%*s %*s %63s", stage) != 1 || strcmp(stage, next_stage) != 0))
		test_fail(TEST, "%s: the golden goes on with \"%s\" after the C draws, not stage %s", name, next,
			  next_stage);
}

static void check_golden_prefix(const char *name, const Trace *trace)
{
	check_golden_stop(name, trace, NULL);
}

static const struct {
	RandoLocationId location;
	RandoItemId prize;
} baseline_prizes[] = {
	{RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_Crystal6},
	{RANDO_LOC_DESERT_PALACE_PRIZE, RANDO_ITEM_Crystal1},
	{RANDO_LOC_TOWER_OF_HERA_PRIZE, RANDO_ITEM_Crystal3},
	{RANDO_LOC_PALACE_OF_DARKNESS_PRIZE, RANDO_ITEM_Crystal4},
	{RANDO_LOC_SWAMP_PALACE_PRIZE, RANDO_ITEM_PendantOfCourage},
	{RANDO_LOC_SKULL_WOODS_PRIZE, RANDO_ITEM_Crystal7},
	{RANDO_LOC_THIEVES_TOWN_PRIZE, RANDO_ITEM_PendantOfWisdom},
	{RANDO_LOC_ICE_PALACE_PRIZE, RANDO_ITEM_Crystal5},
	{RANDO_LOC_MISERY_MIRE_PRIZE, RANDO_ITEM_PendantOfPower},
	{RANDO_LOC_TURTLE_ROCK_PRIZE, RANDO_ITEM_Crystal2},
};

/* tests/rando/cases/baseline: its trace through prepareWorld, its ten "- Prize:1" entries and two bottles;
 * packs 0 and 5 are VT's rows 2 and 0 (VT dump behind test_prepare's golden case). */
static void check_baseline(void)
{
	Trace trace;
	size_t p;
	int i;

	CHECK(generate(NULL, NULL, 1, &trace) == RANDO_INCOMPLETE);
	CHECK(gen.attempt == 0);
	CHECK(trace.lines == 232);
	check_golden_prefix("baseline", &trace);
	CHECK(strstr(trace.text, "0 232 prepareWorld 0 0 -> 0\n") != NULL);
	CHECK(strstr(trace.text, "0 6 setMedallions 0 2 -> 0\n0 7 fillPrizes 0 13 -> 12\n") != NULL);
	CHECK(strstr(trace.text, "0 71 setFountains 1 6 -> 3\n0 72 setFountains 1 6 -> 6\n") != NULL);
	CHECK(strstr(trace.text, "0 79 shufflePrizePacks 0 0 -> 0\n") != NULL);
	CHECK(rando_world_drop(&gen.world, RANDO_PRIZE_PACK_0, 0) == RANDO_DROP_MAGIC_REFILL_FULL);
	CHECK(rando_world_drop(&gen.world, RANDO_PRIZE_PACK_5, 0) == RANDO_DROP_HEART);
	CHECK(rando_world_item(&gen.world, RANDO_LOC_WATERFALL_BOTTLE) == RANDO_ITEM_BottleWithBluePotion);
	CHECK(rando_world_item(&gen.world, RANDO_LOC_PYRAMID_BOTTLE) == RANDO_ITEM_BottleWithFairy);
	CHECK(rando_world_item(&gen.world, RANDO_LOC_TURTLE_ROCK_MEDALLION) == RANDO_ITEM_Ether);
	CHECK(rando_world_item(&gen.world, RANDO_LOC_MISERY_MIRE_MEDALLION) == RANDO_ITEM_Ether);
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		CHECK(rando_world_shop_active(&gen.world, (RandoShopId)i) ==
		      (rando_shop_info((RandoShopId)i)->kind != RANDO_SHOP_KIND_TAKE_ANY));
	CHECK(rando_world_item(&gen.world, RANDO_LOC_GANON) == RANDO_ITEM_Triforce);
	CHECK(rando_items_has(&gen.world.pre_collected, RANDO_ITEM_RescueZelda));
	CHECK(rando_world_boss(&gen.world, RANDO_REGION_HYRULE_CASTLE_TOWER, RANDO_BOSS_SLOT_BOSS) == RANDO_BOSS_AGAHNIM);
	for (p = 0; p < sizeof baseline_prizes / sizeof baseline_prizes[0]; p++)
		CHECK(rando_world_item(&gen.world, baseline_prizes[p].location) == baseline_prizes[p].prize);
}

/* tests/rando/cases/crystals_ganon-random/trace.txt. */
static void check_random_ganon(void)
{
	Trace trace;

	CHECK(generate("crystals_ganon", "random", 1, &trace) == RANDO_INCOMPLETE);
	CHECK(trace.lines == 233);
	check_golden_prefix("crystals_ganon-random", &trace);
}

/* tests/rando/cases/crystals_tower-random/trace.txt. */
static void check_random_crystals(void)
{
	Trace trace;

	CHECK(generate("crystals_tower", "random", 1, &trace) == RANDO_INCOMPLETE);
	CHECK(trace.lines == 233);
	check_golden_prefix("crystals_tower-random", &trace);
	CHECK(strncmp(trace.text, "0 1 crystals 0 7 -> 5\n", 22) == 0);
	CHECK(gen.world.config.crystals_tower == 5);
	CHECK(gen.world.config.crystals_ganon == 7);
}

/* The oracle wrapper draws ganon's count before tower's. */
static void check_crystals_order(void)
{
	RandoOptions options;
	Trace trace;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "crystals_ganon", "random"));
	CHECK(rando_options_set(&options, "crystals_tower", "random"));
	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate_until(&gen, &options, 1, 1, record, &trace, RANDO_STAGE_PREPARE_WORLD) ==
	      RANDO_INCOMPLETE);
	CHECK(trace.lines == 234);
	CHECK(strncmp(trace.text, "0 1 crystals 0 7 -> 5\n0 2 crystals 0 7 -> ", 42) == 0);
	CHECK(gen.world.config.crystals_ganon == 5);
	CHECK(gen.world.config.crystals_tower == trace.text[42] - '0');
}

/* `state` picks the world variant and mode.state. */
static void check_states(void)
{
	RandoOptions options;
	Trace trace;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "state", "inverted"));
	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate_until(&gen, &options, 1, 1, record, &trace, RANDO_STAGE_SET_MEDALLIONS)
	      == RANDO_INCOMPLETE);
	CHECK(gen.world.variant == RANDO_VARIANT_INVERTED);
	CHECK(gen.world.config.mode_state == RANDO_STATE_INVERTED);
	check_golden_stop("state-inverted", &trace, "fillPrizes");
	rando_options_init(&options);
	CHECK(rando_options_set(&options, "state", "retro"));
	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate_until(&gen, &options, 1, 1, record, &trace, RANDO_STAGE_SET_MEDALLIONS)
	      == RANDO_INCOMPLETE);
	CHECK(gen.world.variant == RANDO_VARIANT_RETRO);
	CHECK(gen.world.config.mode_state == RANDO_STATE_RETRO);
	check_golden_stop("state-retro", &trace, "fillPrizes");
	CHECK(generate("state", "standard", 1, &trace) == RANDO_INCOMPLETE);
	CHECK(gen.world.variant == RANDO_VARIANT_STANDARD);
	CHECK(gen.world.config.mode_state == RANDO_STATE_STANDARD);
	CHECK(trace.lines == 232);
	check_golden_prefix("state-standard", &trace);
	CHECK(!rando_items_has(&gen.world.pre_collected, RANDO_ITEM_RescueZelda));
	CHECK(generate(NULL, NULL, 1, &trace) == RANDO_INCOMPLETE);
	CHECK(gen.world.variant == RANDO_VARIANT_OPEN);
}

static void observe(void *ctx, int64_t min, int64_t max, int64_t r)
{
	(void)min;
	(void)max;
	*(int64_t *)ctx = r;
}

/* Attempt 0 seeds splitmix32(seed + 0): the first getBottle draw of a fresh PRNG seeded so. */
static void check_reseed(uint32_t seed)
{
	RandoPrng prng;
	RandoOptions options;
	int64_t expected = -1;
	Trace trace;

	memset(&prng, 0, sizeof prng);
	rando_prng_seed(&prng, rando_splitmix32(seed));
	prng.observe = observe;
	prng.observe_ctx = &expected;
	rando_random_int(&prng, 0, 6);
	CHECK(generate(NULL, NULL, seed, &trace) == RANDO_INCOMPLETE);
	CHECK(trace.lines == 232);
	CHECK(trace.text[strlen("0 1 getAdvancementItems 0 6 -> ")] - '0' == expected);

	rando_options_init(&options);
	CHECK(rando_generate_until(&gen, &options, seed, 0, NULL, NULL, RANDO_STAGE_PREPARE_WORLD) ==
	      RANDO_INCOMPLETE);
	CHECK(gen.attempt == 0);
	CHECK(gen.draw == 232);
}

/* tests/rando/cases/baseline: the fill makes lines 233-805, the golden's last. */
static void check_baseline_fill(void)
{
	RandoOptions options;
	Trace trace;

	rando_options_init(&options);
	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate_until(&gen, &options, 1, 1, record, &trace, RANDO_STAGE_FILL) == RANDO_INCOMPLETE);
	CHECK(trace.lines == 805);
	check_golden_stop("baseline", &trace, GOLDEN_END);
}

/* tests/rando/cases/baseline: attempts 0 and the whole golden trace. */
static void check_baseline_complete(void)
{
	RandoOptions options;
	Trace trace;

	rando_options_init(&options);
	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate(&gen, &options, 1, 0, record, &trace) == RANDO_COMPLETE);
	CHECK(gen.attempt == 0);
	CHECK(trace.lines == 805);
	check_golden_stop("baseline", &trace, GOLDEN_END);
	check_complete_outputs("cases/baseline", "baseline", "0\n");
}

/* Attempt k of seed s is attempt 0 of seed s + k: seed 0 with attempt 0 failed at point must replay
 * tests/rando/cases/baseline (seed 1) at attempt 1. */
static void check_retry_identity(RandoFault point)
{
	static char golden[TRACE_MAX], expected[TRACE_MAX];
	RandoOptions options;
	Trace trace;
	const char *retry, *line;
	size_t len;
	FILE *f;

	f = fopen("cases/baseline/trace.txt", "r");
	if (!f)
		test_fail(TEST, "cases/baseline/trace.txt: cannot open");
	len = fread(golden, 1, sizeof golden - 1, f);
	fclose(f);
	golden[len] = '\0';
	memcpy(expected, golden, len + 1);
	for (line = golden; *line; line = strchr(line, '\n') + 1) {
		CHECK(line[0] == '0' && line[1] == ' ');
		expected[line - golden] = '1';
	}

	rando_options_init(&options);
	memset(&trace, 0, sizeof trace);
	gen.fault = point;
	gen.fault_attempts = 1;
	gen.fault_fired = 0;
	CHECK(rando_generate(&gen, &options, 0, 0, record, &trace) == RANDO_COMPLETE);
	gen.fault = RANDO_FAULT_NONE;
	CHECK(gen.attempt == 1);
	CHECK(gen.fault_fired == 1);
	retry = strstr(trace.text, "\n1 ");
	CHECK(retry != NULL);
	if (strcmp(retry + 1, expected) != 0)
		test_fail(TEST, "fault %d: attempt 1 of seed 0 is not baseline's golden trace at attempt 1", (int)point);
	line = retry - 1;
	while (line > trace.text && line[-1] != '\n')
		line--;
	CHECK(strstr(line, " fill ") != NULL && strstr(line, " fill ") < retry);
	check_complete_outputs("cases/baseline", point == RANDO_FAULT_MID_FILL ? "mid-fill retry" : "isWinnable retry",
			       "1\n");
}

static int count_item(RandoPoolList list, RandoItemId item)
{
	int i, n = 0;

	for (i = 0; i < gen.pools.count[list]; i++)
		n += gen.pools.items[list][i] == item;
	return n;
}

/* A stop after getAdvancementItems leaves the getter's lists; one after shufflePrizePacks holds VT's Boots
 * edit (tests/rando/oracle/pool_lists.txt's glitches-overworld_glitches record holds one PegasusBoots). */
static void check_stop_stage(void)
{
	RandoOptions options;
	Trace trace;
	int items;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "glitches", "overworld_glitches"));
	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate_until(&gen, &options, 1, 1, record, &trace, RANDO_STAGE_GET_ADVANCEMENT_ITEMS) ==
	      RANDO_INCOMPLETE);
	check_golden_prefix("glitches-overworld_glitches", &trace);
	CHECK(strstr(trace.text, "setShops") == NULL);
	CHECK(count_item(RANDO_POOL_ADVANCEMENT, RANDO_ITEM_PegasusBoots) == 1);
	CHECK(!rando_items_has(&gen.world.pre_collected, RANDO_ITEM_PegasusBoots));
	items = gen.pools.count[RANDO_POOL_ITEMS];

	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate_until(&gen, &options, 1, 1, record, &trace, RANDO_STAGE_SHUFFLE_PRIZE_PACKS) ==
	      RANDO_INCOMPLETE);
	CHECK(trace.lines == 79);
	CHECK(count_item(RANDO_POOL_ADVANCEMENT, RANDO_ITEM_PegasusBoots) == 0);
	CHECK(rando_items_has(&gen.world.pre_collected, RANDO_ITEM_PegasusBoots));
	CHECK(gen.pools.count[RANDO_POOL_ITEMS] == items + 1);
	CHECK(gen.pools.items[RANDO_POOL_ITEMS][items] == RANDO_ITEM_TwentyRupees);

	memset(&trace, 0, sizeof trace);
	CHECK(rando_generate_until(&gen, &options, 1, 1, record, &trace, RANDO_STAGE_PREPARE_WORLD) ==
	      RANDO_INCOMPLETE);
	CHECK(trace.lines == 232);
	check_golden_prefix("glitches-overworld_glitches", &trace);
}

/* The gate labels claims with stages.txt's names: a renamed or reordered stage fails here. */
static void check_stage_table(void)
{
	Fixture fx;
	int n = 0;

	fixture_open(&fx, STAGE_TABLE);
	while (fixture_next(&fx)) {
		const char *kind = strtok(fx.line, " ");
		const char *name = strtok(NULL, " ");

		if (strcmp(kind, "stage") != 0)
			continue;
		if (!name || strtok(NULL, " "))
			fixture_fail(&fx, "a stage line holds one name");
		if (n >= RANDO_STAGE_COUNT)
			fixture_fail(&fx, "stage missing from RandoStage");
		if (strcmp(name, rando_stage_name((RandoStage)n)) != 0)
			fixture_fail(&fx, "RandoStage names this stage differently");
		n++;
	}
	fixture_close(&fx);
	if (n != RANDO_STAGE_COUNT)
		test_fail(TEST, "%s lists %d stages, RandoStage %d", STAGE_TABLE, n, RANDO_STAGE_COUNT);
}

static const char *const list_names[RANDO_POOL_LIST_COUNT] = {"dungeon", "advancement", "nice", "items"};

static void check_list(Fixture *fx, const char *name, RandoPoolList list)
{
	const char *expected;
	int i = 0;

	if (strcmp(strtok(fx->line, " "), list_names[list]) != 0)
		fixture_fail(fx, "expected the next list of the case");
	for (; (expected = strtok(NULL, " ")) != NULL; i++) {
		const char *actual = i < gen.pools.count[list] ? rando_item_info(gen.pools.items[list][i])->name : "(end)";

		if (strcmp(expected, actual) != 0)
			test_fail(TEST, "%s %s[%d]: expected %s, got %s", name, list_names[list], i, expected, actual);
	}
	if (i != gen.pools.count[list])
		test_fail(TEST, "%s %s[%d]: expected (end), got %s", name, list_names[list], i,
			  rando_item_info(gen.pools.items[list][i])->name);
}

/* Replays a pool_lists.txt-format fixture: per case, the generator stopped after last at attempt 0 must
 * hold the record's four lists in order, its draws the golden's up to next_stage (NULL: unchecked).
 * Returns the cases replayed. */
static int check_pool_records(const char *path, RandoStage last, const char *next_stage)
{
	int cases = 0;
	Fixture fx;

	fixture_open(&fx, path);
	while (fixture_next(&fx)) {
		char name[128];
		const char *kv;
		RandoOptions options;
		RandoPoolList list;
		unsigned long seed;
		Trace trace;

		if (strcmp(strtok(fx.line, " "), "case") != 0)
			fixture_fail(&fx, "expected a case record");
		snprintf(name, sizeof name, "%s", strtok(NULL, " "));
		seed = strtoul(strtok(NULL, " "), NULL, 10);
		rando_options_init(&options);
		while ((kv = strtok(NULL, " ")) != NULL) {
			char key[64];
			const char *eq = strchr(kv, '=');

			if (!eq || (size_t)(eq - kv) >= sizeof key)
				fixture_fail(&fx, "malformed option");
			memcpy(key, kv, (size_t)(eq - kv));
			key[eq - kv] = '\0';
			if (!rando_options_set(&options, key, eq + 1))
				fixture_fail(&fx, "unknown option");
		}
		memset(&trace, 0, sizeof trace);
		if (rando_generate_until(&gen, &options, (uint32_t)seed, 1, record, &trace, last) != RANDO_INCOMPLETE)
			fixture_fail(&fx, "generation did not stop incomplete");
		check_golden_stop(name, &trace, next_stage);
		cases++;
		for (list = 0; list < RANDO_POOL_LIST_COUNT; list++) {
			if (!fixture_next(&fx))
				fixture_fail(&fx, "case record without its four lists");
			check_list(&fx, name, list);
		}
	}
	fixture_close(&fx);
	return cases;
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	check_baseline();
	check_random_ganon();
	check_random_crystals();
	check_crystals_order();
	check_states();
	check_reseed(1);
	check_reseed(12345);
	check_reseed(UINT32_MAX);
	check_stop_stage();
	check_stage_table();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	check_baseline_fill();
	check_baseline_complete();
	printf(TEST ": baseline generated to completion\n");
	check_retry_identity(RANDO_FAULT_MID_FILL);
	check_retry_identity(RANDO_FAULT_IS_WINNABLE);
	printf(TEST ": seed 0 failed mid-fill, then at isWinnable, replayed baseline at attempt 1\n");
	printf(TEST ": %d pool list records replayed\n",
	       check_pool_records(FIXTURE_POOL_LISTS, RANDO_STAGE_GET_ADVANCEMENT_ITEMS, NULL));
	printf(TEST ": %d prepared pool records replayed\n",
	       check_pool_records(FIXTURE_PREPARED_POOLS, RANDO_STAGE_PREPARE_WORLD, "fill"));
	return 0;
}
