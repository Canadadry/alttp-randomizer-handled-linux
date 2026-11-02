#define _POSIX_C_SOURCE 200809L

#include "rando_case.h"
#include "support.h"

#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_case_reader"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void check_baseline(void)
{
	RandoCase c;
	RandoOptions baseline;
	char error[RANDO_CASE_ERROR_MAX];

	if (!rando_case_read("cases/baseline", &c, error))
		test_fail(TEST, "cases/baseline rejected: %s", error);
	rando_options_init(&baseline);
	CHECK(memcmp(&c.options, &baseline, sizeof baseline) == 0);
	CHECK(c.seed == 1);
	CHECK(c.has_attempts && c.attempts == 0);
	CHECK(c.options.hints);
	CHECK(c.heart_beep == RANDO_HEART_BEEP_HALF);
}

#define ENTRIES_MAX 128
#define CASES_MIN 50
#define COMBOS_MIN 23

typedef struct CaseEntry {
	const char *dir;
	char name[256];
	RandoCase c;
} CaseEntry;

/* Reads every case of dir into set from *n on. On an unreadable case returns false with its error. */
static bool read_dir(const char *dir, CaseEntry *set, size_t *n, char error[RANDO_CASE_ERROR_MAX])
{
	DIR *d = opendir(dir);
	struct dirent *entry;
	char path[512];

	if (d == NULL)
		test_fail(TEST, "cannot open %s/", dir);
	while ((entry = readdir(d)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		if (*n == ENTRIES_MAX)
			test_fail(TEST, "more than %d cases, raise ENTRIES_MAX", ENTRIES_MAX);
		snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
		if (!rando_case_read(path, &set[*n].c, error)) {
			closedir(d);
			return false;
		}
		set[*n].dir = dir;
		snprintf(set[*n].name, sizeof set[*n].name, "%s", entry->d_name);
		(*n)++;
	}
	closedir(d);
	return true;
}

/* A synthetic case: the baseline with up to two keys set (NULL for none). */
static void add_entry(CaseEntry *set, size_t *n, const char *dir, const char *name, const char *k1, const char *v1,
		      const char *k2, const char *v2)
{
	CaseEntry *e = &set[(*n)++];

	e->dir = dir;
	snprintf(e->name, sizeof e->name, "%s", name);
	rando_options_init(&e->c.options);
		e->c.heart_beep = RANDO_HEART_BEEP_HALF;
	e->c.seed = 1;
	e->c.has_attempts = false;
	if ((k1 && !rando_options_set(&e->c.options, k1, v1)) || (k2 && !rando_options_set(&e->c.options, k2, v2)))
		test_fail(TEST, "bad synthetic case %s", name);
}

typedef struct Pair {
	RandoOptionKey key;
	int value;
} Pair;

/* The required pairs, each with state; in this order. */
static size_t required_pairs(Pair out[][2])
{
	static const RandoOptionGoal goals[] = {RANDO_OPTION_GOAL_TRIFORCE_HUNT, RANDO_OPTION_GOAL_GANONHUNT};
	size_t n = 0;
	int s, v;
	size_t g;

	for (s = RANDO_OPTION_STATE_OPEN; s <= RANDO_OPTION_STATE_RETRO; s++)
		for (v = RANDO_GLITCHES_NONE; v <= RANDO_GLITCHES_NO_LOGIC; v++) {
			out[n][0] = (Pair){RANDO_OPTION_GLITCHES, v};
			out[n++][1] = (Pair){RANDO_OPTION_STATE, s};
		}
	for (s = RANDO_OPTION_STATE_OPEN; s <= RANDO_OPTION_STATE_RETRO; s++) {
		out[n][0] = (Pair){RANDO_OPTION_DUNGEON_ITEMS, RANDO_OPTION_DUNGEON_ITEMS_FULL};
		out[n++][1] = (Pair){RANDO_OPTION_STATE, s};
	}
	for (g = 0; g < sizeof goals / sizeof goals[0]; g++)
		for (s = RANDO_OPTION_STATE_OPEN; s <= RANDO_OPTION_STATE_RETRO; s++) {
			out[n][0] = (Pair){RANDO_OPTION_GOAL, (int)goals[g]};
			out[n++][1] = (Pair){RANDO_OPTION_STATE, s};
		}
	return n;
}

static void pair_name(const Pair p[2], char *out, size_t cap)
{
	snprintf(out, cap, "%s-%s+%s-%s", rando_option_key_name(p[0].key), rando_option_value_name(p[0].key, p[0].value),
		 rando_option_key_name(p[1].key), rando_option_value_name(p[1].key, p[1].value));
}

/* The baseline with the pair set: the only options a case covering the pair may have. */
static void pair_options(const Pair p[2], RandoOptions *out)
{
	int i;

	rando_options_init(out);
	for (i = 0; i < 2; i++)
		if (!rando_options_set(out, rando_option_key_name(p[i].key), rando_option_value_name(p[i].key, p[i].value)))
			test_fail(TEST, "cannot set pair key %s", rando_option_key_name(p[i].key));
}

/* Writes the first required pair no case covers into missing (128 bytes) and returns true; false if
 * every pair is covered. */
static bool missing_pair(const CaseEntry *set, size_t n, char *missing)
{
	Pair pairs[64][2];
	size_t np = required_pairs(pairs), i, j;
	RandoOptions want;

	missing[0] = '\0';
	for (i = 0; i < np; i++) {
		pair_options(pairs[i], &want);
		for (j = 0; j < n; j++) {
			want.hints = set[j].c.options.hints;
			if (memcmp(&set[j].c.options, &want, sizeof want) == 0)
				break;
		}
		if (j == n) {
			pair_name(pairs[i], missing, 128);
			return true;
		}
	}
	return false;
}

/* One case per required pair, in required_pairs' order. */
static void add_required(CaseEntry *set, size_t *n)
{
	Pair pairs[64][2];
	size_t np = required_pairs(pairs), i;
	char name[128];

	for (i = 0; i < np; i++) {
		pair_name(pairs[i], name, sizeof name);
		add_entry(set, n, "combos", name, rando_option_key_name(pairs[i][0].key),
			  rando_option_value_name(pairs[i][0].key, pairs[i][0].value), rando_option_key_name(pairs[i][1].key),
			  rando_option_value_name(pairs[i][1].key, pairs[i][1].value));
	}
}

/* The first name present in two directories, or NULL. */
static const char *duplicate_name(const CaseEntry *set, size_t n)
{
	size_t i, j;

	for (i = 0; i < n; i++)
		for (j = i + 1; j < n; j++)
			if (strcmp(set[i].name, set[j].name) == 0 && strcmp(set[i].dir, set[j].dir) != 0)
				return set[i].name;
	return NULL;
}

/* Every committed case and combination must be readable, uniquely named, and cover the required pairs. */
static void check_every_case(void)
{
	static CaseEntry set[ENTRIES_MAX];
	char error[RANDO_CASE_ERROR_MAX], missing[128];
	size_t n = 0, ncases;
	const char *dup;

	if (!read_dir("cases", set, &n, error))
		test_fail(TEST, "%s", error);
	ncases = n;
	if (!read_dir("combos", set, &n, error))
		test_fail(TEST, "%s", error);
	if (ncases < CASES_MIN)
		test_fail(TEST, "%zu cases read in cases/, the matrix has %d", ncases, CASES_MIN);
	if (n - ncases < COMBOS_MIN)
		test_fail(TEST, "%zu cases read in combos/, the matrix has %d", n - ncases, COMBOS_MIN);
	dup = duplicate_name(set, n);
	if (dup != NULL)
		test_fail(TEST, "case in both cases/ and combos/: %s", dup);
	if (missing_pair(set, n, missing))
		test_fail(TEST, "no case covers %s with every other setting at the baseline", missing);
}

static const char baseline_json[] =
	"{\n    \"glitches\": \"none\",\n    \"item_placement\": \"advanced\",\n"
	"    \"dungeon_items\": \"standard\",\n    \"accessibility\": \"items\",\n    \"goal\": \"ganon\",\n"
	"    \"crystals_tower\": \"7\",\n    \"crystals_ganon\": \"7\",\n    \"state\": \"open\",\n"
	"    \"weapons\": \"randomized\",\n    \"item_pool\": \"normal\",\n"
	"    \"item_functionality\": \"normal\"\n}\n";

/* baseline_json with its first occurrence of from replaced by to. */
static const char *edited(char *buf, size_t cap, const char *from, const char *to)
{
	const char *at = strstr(baseline_json, from);
	size_t head;

	if (at == NULL)
		test_fail(TEST, "%s not in the baseline config", from);
	head = (size_t)(at - baseline_json);
	if (head + strlen(to) + strlen(at + strlen(from)) >= cap)
		test_fail(TEST, "edited config too long");
	memcpy(buf, baseline_json, head);
	strcpy(buf + head, to);
	strcat(buf, at + strlen(from));
	return buf;
}

/* The error must name the file and contain says. */
static void check_config_refused(const char *text, const char *why, const char *says)
{
	RandoCase c;
	char error[RANDO_CASE_ERROR_MAX];

	if (rando_case_parse_config(text, strlen(text), "dir/config.json", &c, error))
		test_fail(TEST, "accepted a config with %s:\n%s", why, text);
	if (strncmp(error, "dir/config.json: ", 17) != 0 || strstr(error, says) == NULL)
		test_fail(TEST, "the error for %s does not name the file or say \"%s\": %s", why, says, error);
}

static void check_config_rejected(const char *text, const char *why)
{
	check_config_refused(text, why, "");
}

static void check_config_accepted(const char *text, RandoCase *c)
{
	char error[RANDO_CASE_ERROR_MAX];

	if (!rando_case_parse_config(text, strlen(text), "dir/config.json", c, error))
		test_fail(TEST, "rejected %s\n%s", error, text);
}

static void check_config(void)
{
	char buf[1024];
	RandoCase c;
	RandoOptions *options = &c.options;

	check_config_accepted("{\"glitches\":\"no_logic\",\"item_placement\":\"basic\",\"dungeon_items\":\"full\","
			      "\"accessibility\":\"none\",\"goal\":\"ganonhunt\",\"crystals_tower\":\"random\","
			      "\"crystals_ganon\":\"0\",\"state\":\"retro\",\"weapons\":\"swordless\","
			      "\"item_pool\":\"crowd_control\",\"item_functionality\":\"expert\"}",
			      &c);
	CHECK(options->glitches == RANDO_GLITCHES_NO_LOGIC);
	CHECK(options->item_placement == RANDO_OPTION_PLACEMENT_BASIC);
	CHECK(options->dungeon_items == RANDO_OPTION_DUNGEON_ITEMS_FULL);
	CHECK(options->accessibility == RANDO_OPTION_ACCESSIBILITY_NONE);
	CHECK(options->goal == RANDO_OPTION_GOAL_GANONHUNT);
	CHECK(options->crystals_tower == RANDO_CRYSTALS_RANDOM);
	CHECK(options->crystals_ganon == RANDO_CRYSTALS_0);
	CHECK(options->state == RANDO_OPTION_STATE_RETRO);
	CHECK(options->weapons == RANDO_OPTION_WEAPONS_SWORDLESS);
	CHECK(options->item_pool == RANDO_ITEM_POOL_CROWD_CONTROL);
	CHECK(options->item_functionality == RANDO_ITEM_FUNCTIONALITY_EXPERT);
	check_config_accepted(edited(buf, sizeof buf, "{", " \r\n\t{ \t"), &c);

	check_config_rejected(edited(buf, sizeof buf, "{", "{\"seed\": \"1\","), "an extra key");
	check_config_rejected(edited(buf, sizeof buf, "{", "{\"goal\": \"ganon\","), "a duplicate key");
	check_config_rejected(edited(buf, sizeof buf, "\"7\",\n    \"crystals_ganon\"", "7,\n    \"crystals_ganon\""),
			      "a number value");
	check_config_rejected(edited(buf, sizeof buf, "\"ganon\"", "null"), "a null value");
	check_config_refused(edited(buf, sizeof buf, "\"ganon\"", "\"gan\\u006fn\""), "an escape", "escape");
	check_config_refused(edited(buf, sizeof buf, "\"goal\"", "\"go\\u0061l\""), "an escaped key", "escape");
	check_config_refused(edited(buf, sizeof buf, "    \"goal\": \"ganon\",\n", ""), "a missing key", "missing key goal");
	check_config_refused(edited(buf, sizeof buf, "\"ganon\"", "\"pyramid\""), "an unknown value", "goal pyramid");
	check_config_rejected(edited(buf, sizeof buf, "}\n", "}\n{}"), "trailing garbage");
	check_config_rejected(edited(buf, sizeof buf, "}\n", "}x"), "a trailing character");
	check_config_rejected(edited(buf, sizeof buf, "\"normal\"\n}", "\"normal\",\n}"), "a trailing comma");
	check_config_rejected(edited(buf, sizeof buf, "}\n", ""), "no closing brace");
	check_config_rejected("[]", "an array");
	check_config_rejected("", "an empty file");
	check_config_rejected("{}", "an empty object");
}

static void check_case_keys(void)
{
	static const struct {
		const char *name;
		RandoHeartBeep value;
	} beeps[] = {
		{"off", RANDO_HEART_BEEP_OFF},		 {"half", RANDO_HEART_BEEP_HALF},
		{"quarter", RANDO_HEART_BEEP_QUARTER}, {"double", RANDO_HEART_BEEP_DOUBLE},
		{"normal", RANDO_HEART_BEEP_NORMAL},
	};
	char buf[1024], key[64];
	RandoCase c, plain;
	size_t i;

	check_config_accepted(baseline_json, &plain);
	CHECK(plain.options.hints);
	CHECK(plain.heart_beep == RANDO_HEART_BEEP_HALF);

	check_config_accepted(edited(buf, sizeof buf, "{", "{\"hints\": \"off\","), &c);
	CHECK(!c.options.hints);
	CHECK(c.heart_beep == RANDO_HEART_BEEP_HALF);
	c.options.hints = true;
	CHECK(memcmp(&c.options, &plain.options, sizeof c.options) == 0);
	check_config_accepted(edited(buf, sizeof buf, "{", "{\"hints\": \"on\","), &c);
	CHECK(c.options.hints);
	for (i = 0; i < sizeof beeps / sizeof beeps[0]; i++) {
		snprintf(key, sizeof key, "{\"heart_beep\": \"%s\",", beeps[i].name);
		check_config_accepted(edited(buf, sizeof buf, "{", key), &c);
		if (c.heart_beep != beeps[i].value || !c.options.hints)
			test_fail(TEST, "heart_beep %s read as %d", beeps[i].name, (int)c.heart_beep);
	}
	check_config_accepted(edited(buf, sizeof buf, "\"normal\"\n}", "\"normal\",\"heart_beep\":\"off\",\"hints\":\"off\"}"),
			      &c);
	CHECK(!c.options.hints && c.heart_beep == RANDO_HEART_BEEP_OFF);

	check_config_refused(edited(buf, sizeof buf, "{", "{\"hints\": \"maybe\","), "an unknown hints", "hints maybe");
	check_config_refused(edited(buf, sizeof buf, "{", "{\"hints\": \"On\","), "a capitalised hints", "hints On");
	check_config_refused(edited(buf, sizeof buf, "{", "{\"heart_beep\": \"fast\","), "an unknown heart_beep",
			     "heart_beep fast");
	check_config_refused(edited(buf, sizeof buf, "{", "{\"hints\": true,"), "a boolean hints", "hints is not a string");
	check_config_refused(edited(buf, sizeof buf, "{", "{\"heart_beep\": 2,"), "a number heart_beep",
			     "heart_beep is not a string");
	check_config_refused(edited(buf, sizeof buf, "{", "{\"hints\": \"on\", \"hints\": \"on\","), "a duplicate hints",
			     "duplicate key hints");
	check_config_refused(edited(buf, sizeof buf, "{", "{\"heart_beep\": \"half\", \"heart_beep\": \"off\","),
			     "a duplicate heart_beep", "duplicate key heart_beep");
	check_config_refused(edited(buf, sizeof buf, "{", "{\"hints\": \"on\", \"heartbeep\": \"half\","),
			     "an extra key next to hints", "unknown key heartbeep");
	check_config_refused("{\"hints\": \"on\", \"heart_beep\": \"half\"}", "only the case keys", "missing key");
}

static void check_seed_rejected(const char *text, size_t len)
{
	uint32_t seed;
	char error[RANDO_CASE_ERROR_MAX];

	if (rando_case_parse_uint32(text, len, "dir/seed", &seed, error))
		test_fail(TEST, "accepted the seed \"%.*s\"", (int)len, text);
	if (strncmp(error, "dir/seed: ", 10) != 0)
		test_fail(TEST, "the seed error does not name the file: %s", error);
}

static uint32_t seed_of(const char *text)
{
	uint32_t seed;
	char error[RANDO_CASE_ERROR_MAX];

	if (!rando_case_parse_uint32(text, strlen(text), "dir/seed", &seed, error))
		test_fail(TEST, "rejected the seed \"%s\": %s", text, error);
	return seed;
}

static void check_seed(void)
{
	CHECK(seed_of("1\n") == 1);
	CHECK(seed_of("0") == 0);
	CHECK(seed_of("0000000042\n") == 42);
	CHECK(seed_of("4294967295\n") == 4294967295u);
	check_seed_rejected("4294967296\n", 11);
	check_seed_rejected("99999999999", 11);
	check_seed_rejected("", 0);
	check_seed_rejected("\n", 1);
	check_seed_rejected("1\n\n", 3);
	check_seed_rejected(" 1\n", 3);
	check_seed_rejected("1 \n", 3);
	check_seed_rejected("-1\n", 3);
	check_seed_rejected("+1\n", 3);
	check_seed_rejected("1\r\n", 3);
	check_seed_rejected("0x10", 4);
	check_seed_rejected("1\0", 2);
}

static void check_missing_case(void)
{
	RandoCase c;
	char error[RANDO_CASE_ERROR_MAX];

	CHECK(!rando_case_read("cases/no-such-case", &c, error));
	CHECK(strncmp(error, "cases/no-such-case/config.json: ", 32) == 0);
}

static void check_coverage_guard(void)
{
	static CaseEntry set[ENTRIES_MAX];
	char missing[128];
	size_t n = 0;
	const char *dup;

	add_entry(set, &n, "cases", "baseline", NULL, NULL, NULL, NULL);
	if (!missing_pair(set, n, missing) || strcmp(missing, "glitches-overworld_glitches+state-open") != 0)
		test_fail(TEST, "a set with only the baseline should miss glitches-overworld_glitches+state-open, got %s",
			  missing);
	n = 0;
	add_required(set, &n);
	if (missing_pair(set, n, missing))
		test_fail(TEST, "the complete synthetic set misses %s", missing);
	set[n - 1] = set[n - 2];
	if (!missing_pair(set, n, missing) || strcmp(missing, "goal-ganonhunt+state-retro") != 0)
		test_fail(TEST, "dropping goal-ganonhunt+state-retro gave %s", missing);
	n = 0;
	add_required(set, &n);
	rando_options_set(&set[n - 1].c.options, "weapons", "swordless");
	if (!missing_pair(set, n, missing) || strcmp(missing, "goal-ganonhunt+state-retro") != 0)
		test_fail(TEST, "a pair with a third setting changed still counted: %s", missing);
	n = 0;
	add_required(set, &n);
	set[n - 1].c.options.hints = false;
	set[n - 1].c.heart_beep = RANDO_HEART_BEEP_OFF;
	if (missing_pair(set, n, missing))
		test_fail(TEST, "hints or heart_beep stopped a pair counting: %s", missing);

	n = 0;
	add_entry(set, &n, "cases", "baseline", NULL, NULL, NULL, NULL);
	add_entry(set, &n, "combos", "state-retro", "state", "retro", NULL, NULL);
	if (duplicate_name(set, n) != NULL)
		test_fail(TEST, "distinct names reported as duplicate");
	add_entry(set, &n, "cases", "state-retro", "state", "retro", NULL, NULL);
	dup = duplicate_name(set, n);
	if (dup == NULL || strcmp(dup, "state-retro") != 0)
		test_fail(TEST, "a name in both directories not reported");
}

static void check_unreadable_combination(void)
{
	static CaseEntry set[ENTRIES_MAX];
	char root[] = "/tmp/test_case_reader_XXXXXX", path[512], error[RANDO_CASE_ERROR_MAX];
	size_t n = 0;
	FILE *f;
	bool ok;

	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	snprintf(path, sizeof path, "%s/broken", root);
	if (mkdir(path, 0700) != 0)
		test_fail(TEST, "mkdir %s failed", path);
	snprintf(path, sizeof path, "%s/broken/config.json", root);
	f = fopen(path, "w");
	if (f == NULL)
		test_fail(TEST, "cannot write %s", path);
	fputs("{}", f);
	fclose(f);
	ok = read_dir(root, set, &n, error);
	remove(path);
	snprintf(path, sizeof path, "%s/broken", root);
	rmdir(path);
	rmdir(root);
	if (ok)
		test_fail(TEST, "a directory with a broken case was read");
	if (strstr(error, "broken/config.json") == NULL)
		test_fail(TEST, "the error does not name the broken case: %s", error);
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	check_baseline();
	check_coverage_guard();
	check_unreadable_combination();
	check_every_case();
	check_config();
	check_case_keys();
	check_seed();
	check_missing_case();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
