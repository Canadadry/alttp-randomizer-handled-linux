#define _POSIX_C_SOURCE 200809L

#include "rando_case.h"
#include "support.h"

#include <dirent.h>
#include <stdio.h>
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
	CHECK(c.hints);
	CHECK(c.heart_beep == RANDO_HEART_BEEP_HALF);
}

/* Every committed case must be readable by the driver. */
static void check_every_case(void)
{
	DIR *dir = opendir("cases");
	struct dirent *entry;
	char path[512], error[RANDO_CASE_ERROR_MAX];
	RandoCase c;
	int n = 0;

	if (dir == NULL)
		test_fail(TEST, "cannot open cases/");
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		snprintf(path, sizeof path, "cases/%s", entry->d_name);
		if (!rando_case_read(path, &c, error))
			test_fail(TEST, "%s", error);
		n++;
	}
	closedir(dir);
	if (n < 44)
		test_fail(TEST, "%d cases read, the matrix has 44", n);
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
	CHECK(plain.hints);
	CHECK(plain.heart_beep == RANDO_HEART_BEEP_HALF);

	check_config_accepted(edited(buf, sizeof buf, "{", "{\"hints\": \"off\","), &c);
	CHECK(!c.hints);
	CHECK(c.heart_beep == RANDO_HEART_BEEP_HALF);
	CHECK(memcmp(&c.options, &plain.options, sizeof c.options) == 0);
	check_config_accepted(edited(buf, sizeof buf, "{", "{\"hints\": \"on\","), &c);
	CHECK(c.hints);
	for (i = 0; i < sizeof beeps / sizeof beeps[0]; i++) {
		snprintf(key, sizeof key, "{\"heart_beep\": \"%s\",", beeps[i].name);
		check_config_accepted(edited(buf, sizeof buf, "{", key), &c);
		if (c.heart_beep != beeps[i].value || !c.hints)
			test_fail(TEST, "heart_beep %s read as %d", beeps[i].name, (int)c.heart_beep);
	}
	check_config_accepted(edited(buf, sizeof buf, "\"normal\"\n}", "\"normal\",\"heart_beep\":\"off\",\"hints\":\"off\"}"),
			      &c);
	CHECK(!c.hints && c.heart_beep == RANDO_HEART_BEEP_OFF);

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

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	check_baseline();
	check_every_case();
	check_config();
	check_case_keys();
	check_seed();
	check_missing_case();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
