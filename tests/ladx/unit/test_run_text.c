#include "ladx_logic_builder.h"
#include "ladx_randomizer.h"
#include "ladx_rng.h"
#include "ladx_run_text.h"
#include "support.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_run_text"
#define BASELINE "cases/baseline/run.txt"
#define RACE "cases/race-1/run.txt"
#define GOAL_SPECIFIC "cases/goal-specific/run.txt"
#define GOAL_MAZE "cases/goal-maze/run.txt"
#define BOSS_RANDOM "cases/boss-random/run.txt"
#define KEYSANITY "combos/dungeon_keys-keysanity+nightmare_keys-keysanity+dungeon_beaks-keysanity+dungeon_maps-keysanity+keyholesanity-1/run.txt"

static int checks;
static int golden_lines;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void golden_line(const char *path, const char *prefix, char *out, size_t cap)
{
	FILE *f = fopen(path, "r");
	char line[LADX_RUN_TEXT_MAX_LINE];

	if (!f)
		test_fail(TEST, "cannot open %s", path);
	while (fgets(line, sizeof line, f)) {
		if (strncmp(line, prefix, strlen(prefix)) == 0) {
			line[strcspn(line, "\n")] = '\0';
			if (strlen(line) >= cap)
				test_fail(TEST, "%s: line too long", path);
			strcpy(out, line);
			fclose(f);
			return;
		}
	}
	fclose(f);
	test_fail(TEST, "%s: no line starting with '%s'", path, prefix);
}

static void check_golden(const char *path, const char *prefix, const char *got)
{
	char want[LADX_RUN_TEXT_MAX_LINE];

	golden_line(path, prefix, want, sizeof want);
	checks++;
	golden_lines++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: '%s' rendered\n  %s\n  golden:\n  %s", path, prefix, got, want);
}

static void check_text(const char *got, const char *want, const char *what)
{
	checks++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: rendered '%s', expected '%s'", what, got, want);
}

static LadxRngRecord record(LadxRngGen gen, LadxRngStage stage, LadxRngMethod method, uint64_t words)
{
	LadxRngRecord r;

	memset(&r, 0, sizeof r);
	r.gen = gen;
	r.stage = stage;
	r.method = method;
	r.words = words;
	return r;
}

static void trace_line(char *buf, unsigned long n, const LadxRngRecord *r)
{
	size_t len = ladx_run_text_trace_line(buf, LADX_RUN_TEXT_MAX_LINE, n, r);

	check(len > 0 && len == strlen(buf), "trace line rendered, length returned");
}

/* Strings CPython 3 printed for float.hex. */
static void test_float_hex(void)
{
	static const struct {
		double x;
		const char *hex;
	} vectors[] = {
		{0.5, "0x1.0000000000000p-1"},
		{0.0, "0x0.0p+0"},
		{-0.0, "-0x0.0p+0"},
		{5e-324, "0x0.0000000000001p-1022"},
		{1.0 - 1.0 / 9007199254740992.0, "0x1.fffffffffffffp-1"},
		{-1.5, "-0x1.8000000000000p+0"},
		{DBL_MAX, "0x1.fffffffffffffp+1023"},
		{DBL_MIN, "0x1.0000000000000p-1022"},
		{0.1, "0x1.999999999999ap-4"},
		{1.0, "0x1.0000000000000p+0"},
		{HUGE_VAL, "inf"},
		{-HUGE_VAL, "-inf"},
	};
	char hex[LADX_FLOAT_HEX_SIZE];
	size_t i;

	for (i = 0; i < sizeof vectors / sizeof vectors[0]; i++) {
		ladx_float_hex(vectors[i].x, hex);
		check_text(hex, vectors[i].hex, "float.hex");
	}
	ladx_float_hex(NAN, hex);
	check_text(hex, "nan", "float.hex of nan");
}

static void test_golden_trace(void)
{
	static const uint8_t seed[16] = {0xe8, 0x51, 0x5e, 0x70, 0x64, 0xfb, 0x5f, 0xd6,
					 0x45, 0x8e, 0x72, 0xd5, 0x61, 0x22, 0x67, 0x3b};
	static const int weights[12] = {20, 20, 20, 50, 50, 20, 10, 5, 5, 20, 10, 10};
	static const size_t perm47[47] = {32, 25, 30, 35, 8,  36, 6,  20, 19, 1,  45, 4,  3,  22, 16, 10,
					  44, 2,  28, 5,  39, 41, 34, 42, 27, 15, 0,  43, 14, 18, 21, 31,
					  38, 17, 26, 40, 33, 29, 23, 24, 37, 9,  7,  12, 46, 13, 11};
	static const size_t perm8[8] = {0, 6, 5, 3, 4, 1, 2, 7};
	static const size_t pick1 = 1;
	char line[LADX_RUN_TEXT_MAX_LINE];
	LadxRngRecord r;

	r = record(LADX_RNG_RND, LADX_STAGE_RANDOMIZER_INIT, LADX_RNG_SEED, 0);
	r.seed = seed;
	r.seed_len = sizeof seed;
	trace_line(line, 0, &r);
	check_golden(BASELINE, "trace 0 ", line);

	r = record(LADX_RNG_RND, LADX_STAGE_WORLD_SETUP_RANDOMIZE, LADX_RNG_CHOICES, 2);
	r.n = 12;
	r.weights = weights;
	r.k = 1;
	r.list = &pick1;
	trace_line(line, 1, &r);
	check_golden(BASELINE, "trace 1 ", line);

	r = record(LADX_RNG_RND, LADX_STAGE_RANDOMIZER_INIT, LADX_RNG_GETSTATE, 0);
	trace_line(line, 2, &r);
	check_golden(BASELINE, "trace 2 ", line);

	r = record(LADX_RNG_GLOBAL, LADX_STAGE_RANDOMIZER_INIT, LADX_RNG_SETSTATE, 0);
	trace_line(line, 3, &r);
	check_golden(BASELINE, "trace 3 ", line);

	r = record(LADX_RNG_RND, LADX_STAGE_PLACER_RUN, LADX_RNG_SHUFFLE, 67);
	r.n = 47;
	r.list = perm47;
	trace_line(line, 16, &r);
	check_golden(BASELINE, "trace 16 ", line);

	r = record(LADX_RNG_RND, LADX_STAGE_PLACER_PLACE_SPECIFIC_ITEM, LADX_RNG_CHOICE, 1);
	r.n = 18;
	r.index = 14;
	trace_line(line, 17, &r);
	check_golden(BASELINE, "trace 17 ", line);

	r = record(LADX_RNG_RND, LADX_STAGE_RANDOMIZER_INIT, LADX_RNG_RANDOM, 2);
	r.real = 0x1.1d12acb9114b1p-1;
	trace_line(line, 1, &r);
	check_golden(RACE, "trace 1 ", line);

	r = record(LADX_RNG_RND, LADX_STAGE_WORLD_SETUP_RANDOMIZE, LADX_RNG_SHUFFLE, 11);
	r.n = 8;
	r.list = perm8;
	trace_line(line, 1, &r);
	check_golden(GOAL_SPECIFIC, "trace 1 ", line);

	r = record(LADX_RNG_RND, LADX_STAGE_PLACER_RUN, LADX_RNG_SHUFFLE, 0);
	trace_line(line, 16, &r);
	check_golden(KEYSANITY, "trace 16 ", line);
}

static void test_other_lines(void)
{
	static const uint8_t seed[16] = {0xe8, 0x51, 0x5e, 0x70, 0x64, 0xfb, 0x5f, 0xd6,
					 0x45, 0x8e, 0x72, 0xd5, 0x61, 0x22, 0x67, 0x3b};
	static const size_t picks[3] = {4, 0, 11};
	static const int weights[2] = {-3, 7};
	char line[LADX_RUN_TEXT_MAX_LINE];
	LadxRngRecord r;

	check(ladx_run_text_seed_line(line, sizeof line, seed, sizeof seed) > 0, "seed line rendered");
	check_golden(BASELINE, "seed ", line);
	check(ladx_run_text_seed_line(line, sizeof line, NULL, 0) > 0, "empty seed line rendered");
	check_text(line, "seed -", "empty seed");
	check(ladx_run_text_settings_line(line, sizeof line, "hsHTWRl") > 0, "settings line rendered");
	check_golden(BASELINE, "settings ", line);
	check(ladx_run_text_settings_line(line, sizeof line, "") > 0, "empty settings line rendered");
	check_text(line, "settings \"\"", "empty settings");

	r = record(LADX_RNG_GLOBAL, LADX_STAGE_MAZE_BUILD_MAZE, LADX_RNG_RANDINT, 3);
	r.a = -4;
	r.b = 35;
	r.value = -2;
	trace_line(line, 1234, &r);
	check_text(line, "trace 1234 global patches.maze:buildMaze randint -4 35 -> -2 [3]", "randint");

	/* Golden nightmare_keys-keysanity, trace 476. */
	r = record(LADX_RNG_GLOBAL, LADX_STAGE_UTILS_RANDOM_NUMBER, LADX_RNG_RANDRANGE, 1);
	r.a = 3;
	r.b = 10000;
	r.value = 9625;
	trace_line(line, 476, &r);
	check_text(line, "trace 476 global utils:randomNumber randrange 3 10000 -> 9625 [1]", "randrange");

	r = record(LADX_RNG_RND, LADX_STAGE_MARIN_STARTING_TEXT, LADX_RNG_CHOICE, 1);
	r.n = 480;
	r.index = 343;
	trace_line(line, 475, &r);
	check_text(line, "trace 475 rnd patches.aesthetics:getMarinStartingText choice 480 -> 343 [1]", "marin choice");

	r = record(LADX_RNG_RND, LADX_STAGE_ITEMPOOL_RANDOMIZE_RUPEES, LADX_RNG_CHOICES, 6);
	r.n = 2;
	r.weights = weights;
	r.k = 3;
	r.list = picks;
	trace_line(line, 5, &r);
	check_text(line, "trace 5 rnd itempool:ItemPool.__randomizeRupees choices 2 -3,7 3 -> 4,0,11 [6]",
		   "choices k=3");
	r.k = 0;
	trace_line(line, 5, &r);
	check_text(line, "trace 5 rnd itempool:ItemPool.__randomizeRupees choices 2 -3,7 0 -> - [6]", "choices k=0");

	r = record(LADX_RNG_RND, LADX_STAGE_PLACER_PLACE_ITEM, LADX_RNG_RANDOM, 2);
	r.real = 0.0;
	trace_line(line, 7, &r);
	check_text(line, "trace 7 rnd randomizer:RandomItemPlacer.__placeItem random - -> 0x0.0p+0 [2]", "random 0");
}

static void test_capacity(void)
{
	static size_t perm[LADX_RNG_MAX_LIST];
	static int weights[LADX_RNG_MAX_LIST];
	char line[LADX_RUN_TEXT_MAX_LINE];
	char small[16];
	LadxRngRecord r;
	size_t i;

	for (i = 0; i < LADX_RNG_MAX_LIST; i++) {
		perm[i] = LADX_RNG_MAX_LIST - 1 - i;
		weights[i] = -2147483647 - 1;
	}
	r = record(LADX_RNG_GLOBAL, LADX_STAGE_PLACER_PLACE_SPECIFIC_ITEM, LADX_RNG_CHOICES, UINT64_MAX);
	r.n = LADX_RNG_MAX_LIST;
	r.weights = weights;
	r.k = LADX_RNG_MAX_LIST;
	r.list = perm;
	trace_line(line, (unsigned long)-1, &r);
	check(ladx_run_text_trace_line(small, sizeof small, 0, &r) == 0 && small[0] == '\0',
	      "a line over cap renders nothing");
	r.method = LADX_RNG_METHOD_COUNT;
	check(ladx_run_text_trace_line(line, sizeof line, 0, &r) == 0, "an unknown method renders nothing");
}

static void test_sink(void)
{
	char text[LADX_RUN_TEXT_MAX_LINE];
	LadxRunTextTrace trace;
	LadxRng rnd, glob;
	FILE *f = tmpfile();
	size_t got;

	if (!f)
		test_fail(TEST, "tmpfile failed");
	ladx_run_text_trace_init(&trace, f);
	ladx_rng_init(&rnd, LADX_RNG_RND, ladx_run_text_trace_sink, &trace);
	ladx_rng_init(&glob, LADX_RNG_GLOBAL, ladx_run_text_trace_sink, &trace);
	ladx_rng_seed(&rnd, LADX_STAGE_RANDOMIZER_INIT, (const uint8_t *)"\x01\xff", 2);
	ladx_rng_copy_state(&glob, &rnd, LADX_STAGE_RANDOMIZER_INIT);
	check(trace.count == 3 && !trace.failed, "sink: three lines counted");
	rewind(f);
	got = fread(text, 1, sizeof text - 1, f);
	text[got] = '\0';
	fclose(f);
	check_text(text,
		   "trace 0 rnd randomizer:Randomizer.__init__ seed 01ff -> - [0]\n"
		   "trace 1 rnd randomizer:Randomizer.__init__ getstate - -> - [0]\n"
		   "trace 2 global randomizer:Randomizer.__init__ setstate - -> - [0]\n",
		   "sink: lines numbered across both generators");
}

static void outcome_line(char *line, size_t cap, LadxOutcomeKind kind, const char *type, const char *message)
{
	LadxOutcome o;

	o.kind = kind;
	o.type = type;
	o.message = message;
	if (ladx_run_text_outcome_line(line, cap, &o) == 0)
		test_fail(TEST, "outcome line not rendered");
}

static void test_outcome_lines(void)
{
	char line[LADX_RUN_TEXT_MAX_LINE];
	LadxOutcome o;

	outcome_line(line, sizeof line, LADX_OUTCOME_OK, NULL, NULL);
	check_golden(BASELINE, "outcome ", line);
	outcome_line(line, sizeof line, LADX_OUTCOME_ERROR, NULL, "Failed to place an item for a bunch of retries");
	check_text(line, "outcome error Failed to place an item for a bunch of retries", "error outcome");
	outcome_line(line, sizeof line, LADX_OUTCOME_CRASH, "IndexError", "Cannot choose from an empty sequence");
	check_text(line, "outcome crash IndexError Cannot choose from an empty sequence", "crash outcome");
	outcome_line(line, sizeof line, LADX_OUTCOME_CRASH, "AssertionError", "");
	check_text(line, "outcome crash AssertionError \"\"", "crash with an empty message");
	outcome_line(line, sizeof line, LADX_OUTCOME_ERROR, NULL, "");
	check_text(line, "outcome error \"\"", "error with an empty message");

	o.kind = LADX_OUTCOME_UNFINISHED;
	o.type = NULL;
	o.message = NULL;
	check(ladx_run_text_outcome_line(line, sizeof line, &o) == 0 && line[0] == '\0',
	      "an unfinished run has no outcome line");
	o.kind = LADX_OUTCOME_OK;
	check(ladx_run_text_outcome_line(line, 5, &o) == 0 && line[0] == '\0', "outcome line over capacity");
}

static void check_ws_lines(const char *path, const LadxWorldSetup *ws)
{
	static const char *const fields[LADX_RUN_TEXT_WS_LINES] = {
		"entrance_mapping", "boss_mapping", "miniboss_mapping", "goal", "goal_count", "bingo_goals", "sign_maze",
		"multichest", "map", "dungeon_chain", "inside_to_outside", "keep_two_way", "one_on_one",
	};
	char line[LADX_RUN_TEXT_MAX_LINE], prefix[64];
	int i;

	for (i = 0; i < LADX_RUN_TEXT_WS_LINES; i++) {
		check(ladx_run_text_ws_line(line, sizeof line, ws, i) > 0, "ws line renders");
		snprintf(prefix, sizeof prefix, "ws %s ", fields[i]);
		check_golden(path, prefix, line);
	}
	check(ladx_run_text_ws_line(line, sizeof line, ws, LADX_RUN_TEXT_WS_LINES) == 0 && line[0] == '\0',
	      "no ws line past the last");
}

static void test_ws_lines(void)
{
	static const LadxSign signs[] = {
		{27, 84}, {27, 75}, {37, 75},	{37, 59},   {68, 59},  {68, 66},  {22, 66}, {7, 66},
		{7, 33},  {73, 33}, {73, 68},	{73, 73},   {56, 73},  {56, 84},  {56, 122}, {134, 122},
		{134, 67}, {17, 67}, {17, 99},	{24, 99},   {24, 5},   {65, 5}};
	static const int directions[] = {1, 2, 1, 2, 0, 3, 3, 1, 2, 0, 0, 3, 0, 0, 2, 129, 3, 128, 2, 1, 2};
	static const int bosses[9] = {2, 5, 0, 2, 3, 4, 6, 5, 8};
	LadxWorldSetup ws;
	char line[LADX_RUN_TEXT_MAX_LINE];
	int i;

	ladx_world_setup_defaults(&ws);
	ws.goal = LADX_GOAL_MAZE;
	ws.sign_count = (int)(sizeof signs / sizeof signs[0]);
	memcpy(ws.signs, signs, sizeof signs);
	memcpy(ws.directions, directions, sizeof directions);
	check_ws_lines(GOAL_MAZE, &ws);

	ladx_world_setup_defaults(&ws);
	for (i = 0; i < 9; i++)
		ws.boss_mapping[i] = bosses[i];
	ws.multichest = LADX_ITEM_BOMB;
	check_ws_lines(BOSS_RANDOM, &ws);

	ladx_world_setup_defaults(&ws);
	ws.goal = LADX_GOAL_SPECIFIC;
	strcpy(ws.goal_instruments, "1764");
	ws.goal_count = 4;
	ws.multichest = LADX_ITEM_RUPEES_100;
	check_ws_lines(GOAL_SPECIFIC, &ws);

	ws.goal = LADX_GOAL_OPEN;
	ws.goal_count = -1;
	ws.inside_to_outside = false;
	check(ladx_run_text_ws_line(line, sizeof line, &ws, 3) > 0, "ws goal open renders");
	check_text(line, "ws goal open", "goal open");
	check(ladx_run_text_ws_line(line, sizeof line, &ws, 4) > 0, "ws goal_count -1 renders");
	check_text(line, "ws goal_count -1", "goal_count -1");
	check(ladx_run_text_ws_line(line, sizeof line, &ws, 10) > 0, "ws flag renders");
	check_text(line, "ws inside_to_outside 0", "a false flag as 0");
	check(ladx_run_text_ws_line(line, 8, &ws, 1) == 0 && line[0] == '\0', "ws line over capacity");
}

static void test_place_lines(void)
{
	static LadxLogic logic;
	static const int options[] = {LADX_ITEM_BOMB, LADX_ITEM_SWORD};
	char line[LADX_RUN_TEXT_MAX_LINE];
	int a;

	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	ladx_logic_add_spot(&logic, a, LADX_SPOT_HEART_PIECE, "0x0A1", options, 2);
	ladx_logic_add_spot(&logic, a, LADX_SPOT_KEY_LOCATION, "", options, 2);
	check(ladx_logic_finish(&logic, a), "a two-spot logic");
	ladx_logic_place(&logic, 0, LADX_ITEM_SWORD);
	check(ladx_run_text_place_line(line, sizeof line, &logic, 0) > 0, "place line rendered");
	check_text(line, "place 0 HeartPiece 0x0A1 SWORD", "a placed spot");
	check(ladx_run_text_place_line(line, sizeof line, &logic, 1) > 0, "unplaced line rendered");
	check_text(line, "place 1 KeyLocation \"\" -", "an unplaced spot, an empty nameId");
	check(ladx_run_text_place_line(line, sizeof line, &logic, 2) == 0 && line[0] == '\0', "no spot 2");
	check(ladx_run_text_place_line(line, 10, &logic, 0) == 0, "a line past cap");
}

static void test_write_run_raised(void)
{
	static const uint8_t seed[16] = {0xe8, 0x51, 0x5e, 0x70, 0x64, 0xfb, 0x5f, 0xd6,
					 0x45, 0x8e, 0x72, 0xd5, 0x61, 0x22, 0x67, 0x3b};
	static LadxRandomizer r;
	static char text[1 << 20];
	LadxSettings settings;
	LadxOutcome raised;
	FILE *trace = tmpfile(), *out = tmpfile();
	size_t got;
	const char *last;

	check(trace && out, "temporary files");
	ladx_settings_defaults(&settings);
	ladx_settings_validate(&settings);
	check(ladx_randomize(&r, &settings, seed, sizeof seed, NULL, NULL) == LADX_RANDOMIZE_OK &&
		      ladx_randomizer_placement(&r),
	      "the baseline fills");
	fputs("trace 0 rnd x y - -> - [0]\n", trace);
	memset(&raised, 0, sizeof raised);
	raised.kind = LADX_OUTCOME_CRASH;
	raised.type = "UnicodeEncodeError";
	raised.message = "planted message";
	check(ladx_run_text_write_run(out, &settings, seed, sizeof seed, &r, trace, &raised), "written");
	rewind(out);
	got = fread(text, 1, sizeof text - 1, out);
	text[got] = '\0';
	last = strstr(text, "trace 0 rnd x y - -> - [0]\n");
	check(last && strcmp(last, "trace 0 rnd x y - -> - [0]\noutcome crash UnicodeEncodeError planted message\n") == 0,
	      "the raised outcome follows the trace");
	rewind(out);
	check(ladx_run_text_write_run(out, &settings, seed, sizeof seed, &r, trace, NULL), "written without");
	fclose(trace);
	fclose(out);
}

int main(void)
{
	test_write_run_raised();
	test_float_hex();
	test_golden_trace();
	test_other_lines();
	test_outcome_lines();
	test_ws_lines();
	test_place_lines();
	test_capacity();
	test_sink();
	printf("%s: float.hex as CPython's, %d golden lines rendered byte-identical, sink numbering ok (%d checks)\n",
	       TEST, golden_lines, checks);
	return 0;
}
