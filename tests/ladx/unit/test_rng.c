#include "ladx_pyrandom.h"
#include "ladx_rng.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_rng"
#define MAX_RECORDS 8

static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

/* The sink keeps each record and copies its lists, valid only during the call. */
typedef struct Kept {
	LadxRngRecord record;
	uint8_t seed[64];
	int weights[LADX_RNG_MAX_LIST];
	size_t list[LADX_RNG_MAX_LIST];
} Kept;

typedef struct Log {
	Kept kept[MAX_RECORDS];
	int count;
} Log;

static void sink(void *ctx, const LadxRngRecord *r)
{
	Log *log = ctx;
	Kept *k;
	size_t list_len = r->method == LADX_RNG_CHOICES ? r->k : r->method == LADX_RNG_SHUFFLE ? r->n : 0;

	if (log->count == MAX_RECORDS)
		test_fail(TEST, "more than %d records", MAX_RECORDS);
	k = &log->kept[log->count++];
	k->record = *r;
	if (r->seed_len)
		memcpy(k->seed, r->seed, r->seed_len);
	if (r->weights)
		memcpy(k->weights, r->weights, r->n * sizeof *r->weights);
	if (list_len)
		memcpy(k->list, r->list, list_len * sizeof *r->list);
}

static const uint8_t seed_bytes[] = {0xe8, 0x51, 0x5e, 0x70, 0x64, 0xfb, 0x5f, 0xd6,
				     0x45, 0x8e, 0x72, 0xd5, 0x61, 0x22, 0x67, 0x3b};

static void pair(LadxRng *rng, LadxPyRandom *py, Log *log)
{
	memset(log, 0, sizeof *log);
	ladx_rng_init(rng, LADX_RNG_RND, sink, log);
	ladx_rng_seed(rng, LADX_STAGE_RANDOMIZER_INIT, seed_bytes, sizeof seed_bytes);
	ladx_pyrandom_seed(py, seed_bytes, sizeof seed_bytes);
	log->count = 0;
}

static void check_same_state(const LadxRng *rng, const LadxPyRandom *py, const char *what)
{
	check(memcmp(rng->py.mt, py->mt, sizeof py->mt) == 0 && rng->py.index == py->index &&
		      rng->py.words == py->words,
	      what);
}

static void check_one_record(const Log *log, LadxRngStage stage, LadxRngMethod method, uint64_t words,
			     const char *what)
{
	char text[160];

	snprintf(text, sizeof text, "%s: one record, stage, method and words", what);
	check(log->count == 1 && log->kept[0].record.gen == LADX_RNG_RND && log->kept[0].record.stage == stage &&
		      log->kept[0].record.method == method && log->kept[0].record.words == words,
	      text);
}

static void test_names(void)
{
	static const char *const stages[LADX_STAGE_COUNT] = {
		"randomizer:Randomizer.__init__",	   "worldSetup:WorldSetup.randomize",
		"patches.maze:buildMaze",		   "itempool:ItemPool.__randomizeRupees",
		"randomizer:RandomItemPlacer.run",	   "randomizer:RandomItemPlacer.placeSpecificItem",
		"randomizer:RandomItemPlacer.__placeItem", "patches.aesthetics:getMarinStartingText",
		"utils:randomNumber",			   "hints:addHints",
		"patches.music:randomizeMusic",		   "patches.endscreen:updateEndScreen",
		"patches.enemies:randomizeEnemies",
	};
	static const char *const methods[LADX_RNG_METHOD_COUNT] = {
		"seed", "random", "choice", "choices", "shuffle", "randint", "getstate", "setstate", "randrange",
	};
	int i;

	for (i = 0; i < LADX_STAGE_COUNT; i++)
		check(strcmp(ladx_rng_stage_name((LadxRngStage)i), stages[i]) == 0, "stage name");
	for (i = 0; i < LADX_RNG_METHOD_COUNT; i++)
		check(strcmp(ladx_rng_method_name((LadxRngMethod)i), methods[i]) == 0, "method name");
	check(strcmp(ladx_rng_gen_name(LADX_RNG_RND), "rnd") == 0, "gen rnd");
	check(strcmp(ladx_rng_gen_name(LADX_RNG_GLOBAL), "global") == 0, "gen global");
}

/* Golden baseline: trace 0, the seed, words 0 and the counter reset. */
static void test_seed(void)
{
	LadxRng rng;
	LadxPyRandom py;
	Log log;

	memset(&log, 0, sizeof log);
	ladx_rng_init(&rng, LADX_RNG_RND, sink, &log);
	ladx_rng_seed(&rng, LADX_STAGE_RANDOMIZER_INIT, seed_bytes, sizeof seed_bytes);
	ladx_pyrandom_seed(&py, seed_bytes, sizeof seed_bytes);
	check_same_state(&rng, &py, "seed: state of ladx_pyrandom_seed");
	check_one_record(&log, LADX_STAGE_RANDOMIZER_INIT, LADX_RNG_SEED, 0, "seed");
	check(log.kept[0].record.seed_len == sizeof seed_bytes &&
		      memcmp(log.kept[0].seed, seed_bytes, sizeof seed_bytes) == 0,
	      "seed: the seed bytes recorded");
	ladx_rng_random(&rng, LADX_STAGE_RANDOMIZER_INIT);
	log.count = 0;
	ladx_rng_seed(&rng, LADX_STAGE_RANDOMIZER_INIT, NULL, 0);
	ladx_pyrandom_seed(&py, NULL, 0);
	check_same_state(&rng, &py, "seed: empty seed, words back to 0");
	check(log.count == 1 && log.kept[0].record.seed_len == 0 && log.kept[0].record.words == 0,
	      "seed: empty seed recorded, words 0");
}

static void test_random(void)
{
	LadxRng rng;
	LadxPyRandom py;
	Log log;
	double got;

	pair(&rng, &py, &log);
	got = ladx_rng_random(&rng, LADX_STAGE_RANDOMIZER_INIT);
	check(got == ladx_pyrandom_random(&py), "random: the value ladx_pyrandom draws");
	check_same_state(&rng, &py, "random: state");
	check_one_record(&log, LADX_STAGE_RANDOMIZER_INIT, LADX_RNG_RANDOM, 2, "random");
	check(log.kept[0].record.real == got, "random: result recorded");
}

static void test_choice(void)
{
	LadxRng rng;
	LadxPyRandom py;
	Log log;
	size_t got = 99, want, n;

	pair(&rng, &py, &log);
	for (n = 1; n <= 40; n += 13) {
		log.count = 0;
		check(ladx_rng_choice(&rng, LADX_STAGE_PLACER_PLACE_ITEM, n, &got) == LADX_RNG_OK, "choice: ok");
		ladx_pyrandom_choice(&py, n, &want);
		check(got == want, "choice: the index ladx_pyrandom picks");
		check_same_state(&rng, &py, "choice: state");
		check(log.count == 1 && log.kept[0].record.n == n && log.kept[0].record.index == got,
		      "choice: length and index recorded");
	}
	log.count = 0;
	got = 99;
	check(ladx_rng_choice(&rng, LADX_STAGE_PLACER_PLACE_ITEM, 0, &got) == LADX_RNG_INDEX_ERROR,
	      "choice: empty sequence is IndexError");
	check(got == 99 && log.count == 0, "choice: an error records nothing and leaves the index");
	check_same_state(&rng, &py, "choice: an error draws nothing");
}

/* Golden baseline, trace 1: choices 12 20,20,20,50,50,20,10,5,5,20,10,10 1 -> 1 [2]. */
static void test_choices(void)
{
	static const int weights[12] = {20, 20, 20, 50, 50, 20, 10, 5, 5, 20, 10, 10};
	double real[12];
	LadxRng rng;
	LadxPyRandom py;
	Log log;
	size_t got[3], want[3], i;

	pair(&rng, &py, &log);
	check(ladx_rng_choices(&rng, LADX_STAGE_WORLD_SETUP_RANDOMIZE, weights, 12, 1, got) == LADX_RNG_OK,
	      "choices: ok");
	check(got[0] == 1, "choices: golden baseline's multichest pick");
	check_one_record(&log, LADX_STAGE_WORLD_SETUP_RANDOMIZE, LADX_RNG_CHOICES, 2, "choices");
	check(log.kept[0].record.n == 12 && log.kept[0].record.k == 1 && log.kept[0].list[0] == 1 &&
		      memcmp(log.kept[0].weights, weights, sizeof weights) == 0,
	      "choices: n, weights, k and picks recorded");
	for (i = 0; i < 12; i++)
		real[i] = weights[i];
	ladx_pyrandom_choices(&py, real, 12, 1, want);
	log.count = 0;
	check(ladx_rng_choices(&rng, LADX_STAGE_WORLD_SETUP_RANDOMIZE, weights, 12, 3, got) == LADX_RNG_OK,
	      "choices: k=3 ok");
	ladx_pyrandom_choices(&py, real, 12, 3, want);
	check(memcmp(got, want, sizeof got) == 0, "choices: the picks ladx_pyrandom makes");
	check_same_state(&rng, &py, "choices: state");
	check_one_record(&log, LADX_STAGE_WORLD_SETUP_RANDOMIZE, LADX_RNG_CHOICES, 6, "choices k=3");
	log.count = 0;
	check(ladx_rng_choices(&rng, LADX_STAGE_WORLD_SETUP_RANDOMIZE, weights, 0, 1, got) == LADX_RNG_INDEX_ERROR,
	      "choices: empty population is IndexError");
	check(ladx_rng_choices(&rng, LADX_STAGE_WORLD_SETUP_RANDOMIZE, weights, LADX_RNG_MAX_LIST + 1, 1, got) ==
		      LADX_RNG_OVERFLOW,
	      "choices: a population over the cap overflows");
	check(log.count == 0, "choices: errors record nothing");
	check_same_state(&rng, &py, "choices: errors draw nothing");
}

/* Golden goal-specific, trace 1 after its seed: shuffle 8 -> 0,6,5,3,4,1,2,7 [11]. */
static void test_shuffle_golden(void)
{
	static const uint8_t seed[16] = {0x2f, 0xd6, 0xcc, 0xeb, 0xed, 0x9b, 0xc3, 0xc5,
					 0xf4, 0xe2, 0xea, 0xad, 0x12, 0x51, 0xcc, 0xa3};
	static const size_t golden[8] = {0, 6, 5, 3, 4, 1, 2, 7};
	char x[9] = "12345678";
	LadxRng rng;
	Log log;
	size_t i;

	memset(&log, 0, sizeof log);
	ladx_rng_init(&rng, LADX_RNG_RND, sink, &log);
	ladx_rng_seed(&rng, LADX_STAGE_RANDOMIZER_INIT, seed, sizeof seed);
	log.count = 0;
	check(ladx_rng_shuffle(&rng, LADX_STAGE_WORLD_SETUP_RANDOMIZE, x, 8, 1) == LADX_RNG_OK, "shuffle: ok");
	check_one_record(&log, LADX_STAGE_WORLD_SETUP_RANDOMIZE, LADX_RNG_SHUFFLE, 11, "shuffle golden");
	check(log.kept[0].record.n == 8 && memcmp(log.kept[0].list, golden, sizeof golden) == 0,
	      "shuffle: golden goal-specific permutation");
	for (i = 0; i < 8; i++)
		check(x[i] == (char)('1' + golden[i]), "shuffle: x[to] = before[perm[to]]");
}

static void test_shuffle(void)
{
	LadxRng rng;
	LadxPyRandom py;
	Log log;
	size_t perm[74], count, i;
	int32_t x[74];
	struct Wide {
		char bytes[7];
	} wide[74];

	for (count = 0; count <= 74; count += 37) {
		pair(&rng, &py, &log);
		for (i = 0; i < count; i++) {
			perm[i] = i;
			x[i] = (int32_t)(1000 + i);
		}
		check(ladx_rng_shuffle(&rng, LADX_STAGE_PLACER_RUN, x, count, sizeof *x) == LADX_RNG_OK,
		      "shuffle: ok");
		ladx_pyrandom_shuffle(&py, perm, count, sizeof *perm);
		check_same_state(&rng, &py, "shuffle: state");
		check(log.count == 1 && log.kept[0].record.n == count &&
			      (count == 0 || memcmp(log.kept[0].list, perm, count * sizeof *perm) == 0),
		      "shuffle: the permutation ladx_pyrandom shuffles");
		for (i = 0; i < count; i++)
			check(x[i] == (int32_t)(1000 + perm[i]), "shuffle: x[to] = before[perm[to]]");
	}
	pair(&rng, &py, &log);
	for (i = 0; i < 74; i++) {
		perm[i] = i;
		memset(wide[i].bytes, (int)i, sizeof wide[i].bytes);
	}
	ladx_rng_shuffle(&rng, LADX_STAGE_PLACER_RUN, wide, 74, sizeof *wide);
	ladx_pyrandom_shuffle(&py, perm, 74, sizeof *perm);
	for (i = 0; i < 74; i++)
		check(wide[i].bytes[0] == (char)perm[i] && wide[i].bytes[6] == (char)perm[i],
		      "shuffle: 7-byte elements moved whole");
	log.count = 0;
	check(ladx_rng_shuffle(&rng, LADX_STAGE_PLACER_RUN, NULL, LADX_RNG_MAX_LIST + 1, 1) == LADX_RNG_OVERFLOW,
	      "shuffle: over the cap overflows");
	check(log.count == 0, "shuffle: overflow records nothing");
	check_same_state(&rng, &py, "shuffle: overflow draws nothing");
}

static void test_randint(void)
{
	LadxRng rng;
	LadxPyRandom py;
	Log log;
	int64_t got = -5, want;

	pair(&rng, &py, &log);
	check(ladx_rng_randint(&rng, LADX_STAGE_WORLD_SETUP_RANDOMIZE, 4, 7, &got) == LADX_RNG_OK, "randint: ok");
	ladx_pyrandom_randint(&py, 4, 7, &want);
	check(got == want, "randint: the value ladx_pyrandom draws");
	check_same_state(&rng, &py, "randint: state");
	check_one_record(&log, LADX_STAGE_WORLD_SETUP_RANDOMIZE, LADX_RNG_RANDINT, 1, "randint");
	check(log.kept[0].record.a == 4 && log.kept[0].record.b == 7 && log.kept[0].record.value == got,
	      "randint: bounds and value recorded");
	log.count = 0;
	got = -5;
	check(ladx_rng_randint(&rng, LADX_STAGE_WORLD_SETUP_RANDOMIZE, 7, 4, &got) == LADX_RNG_VALUE_ERROR,
	      "randint: empty range is ValueError");
	check(got == -5 && log.count == 0, "randint: an error records nothing");
}

static void test_randrange(void)
{
	LadxRng rng;
	LadxPyRandom py;
	Log log;
	int64_t got = -5, want;

	pair(&rng, &py, &log);
	check(ladx_rng_randrange(&rng, LADX_STAGE_UTILS_RANDOM_NUMBER, 3, 10000, &got) == LADX_RNG_OK,
	      "randrange: ok");
	ladx_pyrandom_randrange(&py, 3, 10000, &want);
	check(got == want, "randrange: the value ladx_pyrandom draws");
	check_same_state(&rng, &py, "randrange: state");
	check_one_record(&log, LADX_STAGE_UTILS_RANDOM_NUMBER, LADX_RNG_RANDRANGE, 1, "randrange");
	check(log.kept[0].record.a == 3 && log.kept[0].record.b == 10000 && log.kept[0].record.value == got,
	      "randrange: bounds and value recorded");
	log.count = 0;
	got = -5;
	check(ladx_rng_randrange(&rng, LADX_STAGE_UTILS_RANDOM_NUMBER, 7, 7, &got) == LADX_RNG_VALUE_ERROR,
	      "randrange: empty range is ValueError");
	check(got == -5 && log.count == 0, "randrange: an error records nothing");
}

/* Golden baseline, traces 2 and 3: rnd getstate, then global setstate, both [0]. */
static void test_copy_state(void)
{
	LadxRng rng, glob;
	LadxPyRandom py;
	Log log;

	pair(&rng, &py, &log);
	ladx_rng_random(&rng, LADX_STAGE_RANDOMIZER_INIT);
	ladx_pyrandom_random(&py);
	ladx_rng_init(&glob, LADX_RNG_GLOBAL, sink, &log);
	log.count = 0;
	ladx_rng_copy_state(&glob, &rng, LADX_STAGE_RANDOMIZER_INIT);
	check(log.count == 2, "copy: two records");
	check(log.kept[0].record.gen == LADX_RNG_RND && log.kept[0].record.method == LADX_RNG_GETSTATE &&
		      log.kept[0].record.stage == LADX_STAGE_RANDOMIZER_INIT && log.kept[0].record.words == 0,
	      "copy: rnd getstate [0]");
	check(log.kept[1].record.gen == LADX_RNG_GLOBAL && log.kept[1].record.method == LADX_RNG_SETSTATE &&
		      log.kept[1].record.stage == LADX_STAGE_RANDOMIZER_INIT && log.kept[1].record.words == 0,
	      "copy: global setstate [0]");
	check_same_state(&rng, &py, "copy: rnd unchanged");
	check_same_state(&glob, &py, "copy: global holds rnd's state");
	check(ladx_rng_random(&glob, LADX_STAGE_RANDOMIZER_INIT) == ladx_pyrandom_random(&py),
	      "copy: global continues rnd's stream");
}

int main(void)
{
	test_names();
	test_seed();
	test_random();
	test_choice();
	test_choices();
	test_shuffle_golden();
	test_shuffle();
	test_randint();
	test_randrange();
	test_copy_state();
	printf("%s: seed, random, choice, choices, shuffle, randint, randrange and the state copy traced as ladx_pyrandom draws (%d checks)\n",
	       TEST, checks);
	return 0;
}
