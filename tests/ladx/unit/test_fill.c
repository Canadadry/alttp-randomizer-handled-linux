#include "ladx_logic_builder.h"
#include "ladx_randomizer.h"
#include "ladx_run_text.h"
#include "ladx_settings.h"
#include "ladx_sha512.h"
#include "support.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_fill"
#define OUTCOMES "oracle/fill_outcomes.txt"
#define I(name) LADX_ITEM_##name

static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

/* The draws, as LADXR's were logged around random.Random: `s<n>` per shuffle, `c<n>:<index>` per choice. */
static char draws[1024];

static void log_draw(void *ctx, const LadxRngRecord *record)
{
	size_t len = strlen(draws);

	(void)ctx;
	if (record->method == LADX_RNG_SHUFFLE)
		snprintf(draws + len, sizeof draws - len, "%ss%lu", len ? " " : "", (unsigned long)record->n);
	else if (record->method == LADX_RNG_CHOICE)
		snprintf(draws + len, sizeof draws - len, "%sc%lu:%lu", len ? " " : "", (unsigned long)record->n,
			 (unsigned long)record->index);
}

static void check_text(const char *got, const char *want, const char *what)
{
	checks++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s:\n  got  %s\n  want %s", what, got, want);
}

static LadxLogic logic;
static LadxPlacer placer;
static LadxRng rnd;
static LadxOutcome outcome;

static void seed_rnd(uint8_t seed)
{
	draws[0] = '\0';
	ladx_rng_init(&rnd, LADX_RNG_RND, log_draw, NULL);
	ladx_rng_seed(&rnd, LADX_STAGE_RANDOMIZER_INIT, &seed, 1);
	draws[0] = '\0';
	memset(&outcome, 0, sizeof outcome);
}

static void placer_on(const LadxSettings *s)
{
	int i;

	ladx_placer_init(&placer, &logic, s);
	for (i = 0; i < ladx_logic_spot_count(&logic); i++)
		ladx_placer_add_spot(&placer, i);
}

static void placement_text(char *out, size_t cap)
{
	size_t len = 0;
	int i;

	out[0] = '\0';
	for (i = 0; i < ladx_logic_spot_count(&logic); i++) {
		const LadxSpot *spot = ladx_logic_spot(&logic, i);

		len += (size_t)snprintf(out + len, cap - len, "%s%s=%s", i ? " " : "", spot->name_id,
					spot->item == LADX_ITEM_NONE ? "None" : ladx_item_name(spot->item));
	}
}

/*
 * S holds 0x003 and 0x001, B behind SWORD 0x002, dungeon 1's D 0x010 and
 * 0x00F; pool SWORD BOMB=2 FEATHER KEY1: KEY1 placed first, failed
 * placements undone and their spot re-appended, dungeon spots scored 0.
 */
static void build_five_spots(void)
{
	static const int sbf[] = {I(SWORD), I(BOMB), I(FEATHER)}, bfs[] = {I(BOMB), I(FEATHER), I(SWORD)};
	static const int kb[] = {I(KEY1), I(BOMB)}, kf[] = {I(KEY1), I(FEATHER)};
	int s, b, d;

	ladx_logic_begin(&logic);
	s = ladx_logic_new_location(&logic, -1, "S");
	b = ladx_logic_new_location(&logic, -1, "B");
	d = ladx_logic_new_location(&logic, 1, "D");
	ladx_logic_connect(&logic, s, b, ladx_req_item(I(SWORD)));
	ladx_logic_connect(&logic, s, d, ladx_req_none());
	ladx_logic_add_spot(&logic, s, LADX_SPOT_CHEST, "0x003", sbf, 3);
	ladx_logic_add_spot(&logic, s, LADX_SPOT_CHEST, "0x001", bfs, 3);
	ladx_logic_add_spot(&logic, b, LADX_SPOT_CHEST, "0x002", sbf, 3);
	ladx_logic_add_spot(&logic, d, LADX_SPOT_DUNGEON_CHEST, "0x010", kb, 2);
	ladx_logic_add_spot(&logic, d, LADX_SPOT_DUNGEON_CHEST, "0x00F", kf, 2);
	check(ladx_logic_finish(&logic, s), "the five-spot graph");
}

/* LADXR's RandomItemPlacer on the same graph, random.Random(bytes([seed])). */
static void test_five_spots(void)
{
	static const struct {
		uint8_t seed;
		const char *draws, *placement;
	} want[] = {
		{5, "s1 c2:0 c4:0 c3:2 c4:2 c1:0 c3:2 c2:1 c2:1 c2:0 c2:0 c2:0 c2:1 c2:0 c2:0 c2:1 c1:0 c1:0",
		 "0x003=BOMB 0x001=SWORD 0x002=BOMB 0x010=KEY1 0x00F=FEATHER"},
		{7, "s1 c2:0 c4:3 c1:0 c3:2 c2:0 c3:0 c2:1 c2:1 c2:0 c2:0 c2:0 c2:0 c2:0 c2:0 c2:0 c2:0 c2:1 c1:0 c1:0",
		 "0x003=BOMB 0x001=SWORD 0x002=BOMB 0x010=KEY1 0x00F=FEATHER"},
		{8, "s1 c2:0 c4:1 c3:1 c4:1 c3:2 c4:1 c1:0 c3:1 c2:1 c2:0 c1:0 c1:0 c1:0",
		 "0x003=BOMB 0x001=SWORD 0x002=BOMB 0x010=KEY1 0x00F=FEATHER"},
		{10, "s1 c2:1 c4:3 c1:0 c3:2 c3:1 c2:0 c2:0 c1:0 c1:0",
		 "0x003=SWORD 0x001=FEATHER 0x002=BOMB 0x010=BOMB 0x00F=KEY1"},
	};
	char text[256], what[64];
	LadxSettings s;
	size_t i;

	ladx_settings_defaults(&s);
	ladx_settings_validate(&s);
	for (i = 0; i < sizeof want / sizeof want[0]; i++) {
		build_five_spots();
		placer_on(&s);
		ladx_placer_add_item(&placer, I(SWORD), 1);
		ladx_placer_add_item(&placer, I(BOMB), 2);
		ladx_placer_add_item(&placer, I(FEATHER), 1);
		ladx_placer_add_item(&placer, I(KEY1), 1);
		seed_rnd(want[i].seed);
		snprintf(what, sizeof what, "five spots, seed %u", want[i].seed);
		check(ladx_placer_run(&placer, &rnd, &outcome) == LADX_RANDOMIZE_OK && outcome.kind == LADX_OUTCOME_OK,
		      what);
		check_text(draws, want[i].draws, what);
		placement_text(text, sizeof text);
		check_text(text, want[i].placement, what);
	}
}

/* S's 0x0A0 takes only BOMB, 0x0B0 behind SWORD only SWORD. */
static void build_two_spots(void)
{
	static const int bomb[] = {I(BOMB)}, sword[] = {I(SWORD)};
	int s, b;

	ladx_logic_begin(&logic);
	s = ladx_logic_new_location(&logic, -1, "S");
	b = ladx_logic_new_location(&logic, -1, "B");
	ladx_logic_connect(&logic, s, b, ladx_req_item(I(SWORD)));
	ladx_logic_add_spot(&logic, s, LADX_SPOT_CHEST, "0x0A0", bomb, 1);
	ladx_logic_add_spot(&logic, b, LADX_SPOT_CHEST, "0x0B0", sword, 1);
	check(ladx_logic_finish(&logic, s), "the two-spot graph");
}

/* LADXR's outcomes on the two-spot graph, seed 5: every placement fails, the eleventh gives the Error. */
static void test_outcomes(void)
{
	LadxSettings s;

	ladx_settings_defaults(&s);
	ladx_settings_validate(&s);

	build_two_spots();
	placer_on(&s);
	ladx_placer_add_item(&placer, I(SWORD), 1);
	ladx_placer_add_item(&placer, I(BOMB), 1);
	seed_rnd(5);
	check(ladx_placer_run(&placer, &rnd, &outcome) == LADX_RANDOMIZE_OK && outcome.kind == LADX_OUTCOME_ERROR &&
		      strcmp(outcome.message, "Failed to place an item for a bunch of retries") == 0,
	      "eleven failed placements: LADXR's Error");
	check_text(draws,
		   "s0 c2:0 c1:0 c2:1 c1:0 c2:1 c1:0 c2:0 c1:0 c2:0 c1:0 c2:0 c1:0 c2:1 c1:0 c2:0 c1:0 c2:0 c1:0 c2:1 "
		   "c1:0 c2:1 c1:0",
		   "eleven failed placements' draws");

	build_two_spots();
	placer_on(&s);
	ladx_placer_add_item(&placer, I(SWORD), 1);
	ladx_placer_add_item(&placer, I(BOMB), 2);
	seed_rnd(5);
	check(ladx_placer_run(&placer, &rnd, &outcome) == LADX_RANDOMIZE_OK && outcome.kind == LADX_OUTCOME_CRASH &&
		      strcmp(outcome.type, "AssertionError") == 0 && strcmp(outcome.message, "3 != 2") == 0 &&
		      draws[0] == '\0',
	      "three items for two spots: AssertionError 3 != 2, nothing drawn");

	build_two_spots();
	placer_on(&s);
	ladx_placer_add_item(&placer, I(BOMB), 2);
	seed_rnd(5);
	check(ladx_placer_run(&placer, &rnd, &outcome) == LADX_RANDOMIZE_OK && outcome.kind == LADX_OUTCOME_CRASH &&
		      strcmp(outcome.type, "AssertionError") == 0 &&
		      strcmp(outcome.message, "Sanity check failed: False") == 0 && draws[0] == '\0',
	      "B out of reach: AssertionError Sanity check failed: False, nothing drawn");
}

typedef struct GraphSpot {
	const char *name_id;
	int options[3];
	int option_count;
	bool dungeon;
} GraphSpot;

/*
 * canStillPlaceItemPool's sort on graphs found where its decision turns on
 * it: LADXR's sanity check passes on each, and fails when the nameId
 * tie-break flips, the dungeon score is dropped or the item order loses
 * its stability. N, the start, and dungeon 1's D, joined freely.
 */
static void test_sort_semantics(void)
{
	static const struct {
		const char *what;
		GraphSpot spots[4];
		int pool[3][2];
		const char *draws, *placement;
	} want[] = {
		{"the nameId tie-break",
		 {{"0x00F", {I(FEATHER)}, 1, false},
		  {"0x00B", {I(SWORD), I(FEATHER)}, 2, false},
		  {"0x008", {I(SWORD), I(FEATHER), I(BOMB)}, 3, true},
		  {"0x00A", {I(SWORD), I(BOMB)}, 2, true}},
		 {{I(FEATHER), 1}, {I(SWORD), 2}, {I(BOMB), 1}},
		 "s0 c4:0 c1:0 c3:0 c1:0 c2:1 c2:1 c1:0 c1:0",
		 "0x00F=FEATHER 0x00B=SWORD 0x008=SWORD 0x00A=BOMB"},
		{"the dungeon score",
		 {{"0x007", {I(FEATHER), I(BOMB)}, 2, false},
		  {"0x000", {I(BOMB)}, 1, false},
		  {"0x008", {I(SWORD)}, 1, false},
		  {"0x00B", {I(SWORD), I(FEATHER)}, 2, true}},
		 {{I(FEATHER), 1}, {I(SWORD), 1}, {I(BOMB), 2}},
		 "s0 c4:0 c2:0 c4:0 c1:0 c4:3 c1:0 c4:0 c1:0 c3:0 c1:0 c2:1 c1:0 c1:0 c1:0",
		 "0x007=BOMB 0x000=BOMB 0x008=SWORD 0x00B=FEATHER"},
		{"the stable item order",
		 {{"0x001", {I(BOMB)}, 1, false},
		  {"0x006", {I(FEATHER)}, 1, false},
		  {"0x007", {I(FEATHER), I(BOMB)}, 2, true},
		  {"0x00E", {I(FEATHER), I(SWORD), I(BOMB)}, 3, true}},
		 {{I(BOMB), 2}, {I(FEATHER), 1}, {I(SWORD), 1}},
		 "s0 c4:0 c1:0 c3:0 c1:0 c2:1 c2:1 c2:0 c1:0 c1:0 c1:0",
		 "0x001=BOMB 0x006=FEATHER 0x007=BOMB 0x00E=SWORD"},
	};
	char text[256];
	LadxSettings s;
	size_t i, k;

	ladx_settings_defaults(&s);
	ladx_settings_validate(&s);
	for (i = 0; i < sizeof want / sizeof want[0]; i++) {
		int n, d;

		ladx_logic_begin(&logic);
		n = ladx_logic_new_location(&logic, -1, "N");
		d = ladx_logic_new_location(&logic, 1, "D");
		ladx_logic_connect(&logic, n, d, ladx_req_none());
		for (k = 0; k < 4; k++)
			ladx_logic_add_spot(&logic, want[i].spots[k].dungeon ? d : n, LADX_SPOT_CHEST,
					    want[i].spots[k].name_id, want[i].spots[k].options,
					    want[i].spots[k].option_count);
		check(ladx_logic_finish(&logic, n), want[i].what);
		placer_on(&s);
		for (k = 0; k < 3; k++)
			ladx_placer_add_item(&placer, want[i].pool[k][0], want[i].pool[k][1]);
		seed_rnd(1);
		check(ladx_placer_run(&placer, &rnd, &outcome) == LADX_RANDOMIZE_OK && outcome.kind == LADX_OUTCOME_OK,
		      want[i].what);
		check_text(draws, want[i].draws, want[i].what);
		placement_text(text, sizeof text);
		check_text(text, want[i].placement, want[i].what);
	}
}

static LadxRandomizer r;

/* boomerang=default with accessibility=all: LADXR's sanity check fails on every seed. */
static void test_boomerang_pair(void)
{
	static const uint8_t seeds[][4] = {{0, 0, 0, 0}, {0x4c, 0x41, 0x44, 0x58}, {0xff, 0xff, 0xff, 0xff}};
	const LadxOutcome *o;
	LadxSettingPair finding;
	LadxSettings s;
	size_t i;

	ladx_settings_defaults(&s);
	check(ladx_settings_set(&s, "boomerang=default") == LADX_SETTINGS_OK &&
		      ladx_settings_set(&s, "accessibility=all") == LADX_SETTINGS_OK,
	      "the boomerang pair is settable");
	ladx_settings_validate(&s);
	check(!ladx_settings_supported(&s, &finding), "the boomerang pair is unsupported");
	for (i = 0; i < sizeof seeds / sizeof seeds[0]; i++) {
		check(ladx_randomize(&r, &s, seeds[i], sizeof seeds[i], NULL, NULL) == LADX_RANDOMIZE_OK,
		      "the boomerang pair runs");
		o = ladx_randomizer_outcome(&r);
		check(o->kind == LADX_OUTCOME_CRASH && strcmp(o->type, "AssertionError") == 0 &&
			      strcmp(o->message, "Sanity check failed: False") == 0,
		      "the boomerang pair: AssertionError Sanity check failed: False");
		check(ladx_randomizer_placement(&r) == NULL, "the boomerang pair: no placement");
	}
}

#define FIXTURE_LINE 4096
#define DIGEST_HEX 32
#define MAX_SEED 64
#define MAX_FIELDS 64

static const char *const kind_names[] = {"unfinished", "ok", "error", "crash"};

/* The C run's fill projection written to a temporary file: its line count and digest. */
static void c_projection(const LadxSettings *s, const uint8_t *seed, size_t seed_len, const char *label,
			 long *lines, char digest_hex[DIGEST_HEX + 1])
{
	static uint8_t buf[1 << 16];
	uint8_t digest[LADX_SHA512_SIZE];
	LadxRunTextTrace sink;
	LadxSha512 sha;
	FILE *trace = tmpfile(), *out = tmpfile();
	size_t got, i;

	if (!trace || !out)
		test_fail(TEST, "%s: cannot open a temporary file", label);
	ladx_run_text_trace_init(&sink, trace);
	if (ladx_randomize(&r, s, seed, seed_len, ladx_run_text_trace_sink, &sink) != LADX_RANDOMIZE_OK ||
	    sink.failed)
		test_fail(TEST, "%s: broken run", label);
	if (!ladx_run_text_write_run(out, s, seed, seed_len, &r, trace, NULL))
		test_fail(TEST, "%s: the run text did not write", label);
	fclose(trace);
	rewind(out);
	ladx_sha512_init(&sha);
	*lines = 0;
	while ((got = fread(buf, 1, sizeof buf, out)) > 0) {
		ladx_sha512_update(&sha, buf, got);
		for (i = 0; i < got; i++)
			*lines += buf[i] == '\n';
	}
	fclose(out);
	ladx_sha512_final(&sha, digest);
	for (i = 0; i < DIGEST_HEX / 2; i++)
		sprintf(digest_hex + 2 * i, "%02x", digest[i]);
}

static size_t parse_seed(const char *hex, uint8_t *seed, const char *label)
{
	size_t n = strlen(hex) / 2, i;
	unsigned v;

	if (strlen(hex) % 2 || n > MAX_SEED)
		test_fail(TEST, "%s: bad seed %s", label, hex);
	for (i = 0; i < n; i++) {
		if (sscanf(hex + 2 * i, "%2x", &v) != 1)
			test_fail(TEST, "%s: bad seed %s", label, hex);
		seed[i] = (uint8_t)v;
	}
	return n;
}

/* fill_outcomes.txt: every run's outcome kind, fill projection line count and digest equal. */
static void replay_outcomes(void)
{
	static char line[FIXTURE_LINE];
	int counts[4] = {0, 0, 0, 0}, runs = 0;
	long line_no = 0;
	FILE *f = fopen(OUTCOMES, "r");

	if (!f)
		test_fail(TEST, "cannot open %s", OUTCOMES);
	while (fgets(line, sizeof line, f)) {
		char label[320], *fields[MAX_FIELDS], *tok;
		char digest[DIGEST_HEX + 1];
		uint8_t seed[MAX_SEED];
		size_t seed_len, len = strlen(line);
		LadxSettings s;
		long lines, want_lines;
		int n = 0, kind, i;

		line_no++;
		if (len == 0 || line[len - 1] != '\n')
			test_fail(TEST, "%s line %ld: too long or unterminated", OUTCOMES, line_no);
		line[len - 1] = '\0';
		if (line[0] == '#')
			continue;
		for (tok = strtok(line, " "); tok; tok = strtok(NULL, " ")) {
			if (n == MAX_FIELDS)
				test_fail(TEST, "%s line %ld: too many fields", OUTCOMES, line_no);
			fields[n++] = tok;
		}
		if (n < 6)
			test_fail(TEST, "%s line %ld: want 6 fields and the settings", OUTCOMES, line_no);
		snprintf(label, sizeof label, "%s line %ld: %s k=%s", OUTCOMES, line_no, fields[0], fields[1]);
		ladx_settings_defaults(&s);
		for (i = 6; i < n; i++)
			if (ladx_settings_set(&s, fields[i]) != LADX_SETTINGS_OK)
				test_fail(TEST, "%s: setting %s refused", label, fields[i]);
		ladx_settings_validate(&s);
		seed_len = parse_seed(fields[2], seed, label);
		want_lines = strtol(fields[4], NULL, 10);
		c_projection(&s, seed, seed_len, label, &lines, digest);
		kind = (int)ladx_randomizer_outcome(&r)->kind;
		if (strcmp(kind_names[kind], fields[3]) != 0)
			test_fail(TEST, "%s: outcome %s, want %s", label, kind_names[kind], fields[3]);
		if (lines != want_lines || strcmp(digest, fields[5]) != 0)
			test_fail(TEST, "%s: fill projection %ld lines %s, want %ld lines %s", label, lines, digest,
				  want_lines, fields[5]);
		counts[kind]++;
		runs++;
	}
	fclose(f);
	if (runs == 0)
		test_fail(TEST, "%s: no run", OUTCOMES);
	printf("%s: %d runs of %s replayed, same outcome and fill projection (%d ok, %d error, %d crash)\n", TEST,
	       runs, OUTCOMES, counts[LADX_OUTCOME_OK], counts[LADX_OUTCOME_ERROR], counts[LADX_OUTCOME_CRASH]);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	test_five_spots();
	test_sort_semantics();
	test_outcomes();
	test_boomerang_pair();
	printf("%s: LADXR's placements, draws and outcomes on hand-built graphs, the boomerang pair's "
	       "AssertionError (%d checks)\n",
	       TEST, checks);
	if (fixtures)
		replay_outcomes();
	return 0;
}
