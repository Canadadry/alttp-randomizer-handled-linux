#include "rando.h"
#include "rando_case.h"
#include "rando_record.h"
#include "rando_spoiler.h"
#include "support.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_record"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void read_stream(FILE *f, const char *what, char text[RANDO_RECORD_TEXT_MAX])
{
	size_t n = fread(text, 1, RANDO_RECORD_TEXT_MAX, f);

	if (ferror(f) || n == RANDO_RECORD_TEXT_MAX)
		test_fail(TEST, "%s: read error or longer than %d bytes", what, RANDO_RECORD_TEXT_MAX - 1);
	text[n] = '\0';
}

/* Baseline settings, seed 1, date 2026-11-01T14:03:27, no spoiler. */
static void baseline_record(RandoRecord *record)
{
	memset(record, 0, sizeof *record);
	rando_options_init(&record->options);
	record->heart_beep = RANDO_HEART_BEEP_HALF;
	record->seed = 1;
	rando_name_make(&record->options, rando_name_code(&record->options, record->heart_beep, record->seed), 1,
			&record->name);
	record->date.year = 2026;
	record->date.month = 11;
	record->date.day = 1;
	record->date.hour = 14;
	record->date.minute = 3;
	record->date.second = 27;
}

static void write_record(const RandoRecord *record, const RandoWorld *world, const RandoPlaythrough *playthrough,
			 char text[RANDO_RECORD_TEXT_MAX])
{
	FILE *f = tmpfile();

	CHECK(f != NULL);
	CHECK(rando_record_write(f, record, world, playthrough));
	rewind(f);
	read_stream(f, "record", text);
	fclose(f);
}

static const char baseline_text[] = "{\n"
				    "    \"code\": \"1CA10A\",\n"
				    "    \"date\": \"2026-11-01T14:03:27\",\n"
				    "    \"format\": 1,\n"
				    "    \"rom\": \"Open-Ganon-1CA10A.sfc\",\n"
				    "    \"seed\": 1,\n"
				    "    \"settings\": {\n"
				    "        \"accessibility\": \"items\",\n"
				    "        \"crystals_ganon\": \"7\",\n"
				    "        \"crystals_tower\": \"7\",\n"
				    "        \"dungeon_items\": \"standard\",\n"
				    "        \"glitches\": \"none\",\n"
				    "        \"goal\": \"ganon\",\n"
				    "        \"heart_beep\": \"half\",\n"
				    "        \"hints\": \"on\",\n"
				    "        \"item_functionality\": \"normal\",\n"
				    "        \"item_placement\": \"advanced\",\n"
				    "        \"item_pool\": \"normal\",\n"
				    "        \"state\": \"open\",\n"
				    "        \"weapons\": \"randomized\"\n"
				    "    }\n"
				    "}\n";

static char text[RANDO_RECORD_TEXT_MAX];

static void test_writer_without_spoiler(void)
{
	RandoRecord record;

	baseline_record(&record);
	write_record(&record, NULL, NULL, text);
	if (strcmp(text, baseline_text) != 0)
		test_fail(TEST, "record without spoiler\n%s\nexpected\n%s", text, baseline_text);
}

static RandoGen gen;
static RandoPlaythrough playthrough;
static char golden[RANDO_RECORD_TEXT_MAX];

static void read_golden(const char *case_dir)
{
	char path[512];
	FILE *f;

	snprintf(path, sizeof path, "%s/spoiler.json", case_dir);
	f = fopen(path, "rb");
	if (!f)
		test_fail(TEST, "cannot open %s", path);
	read_stream(f, path, golden);
	fclose(f);
}

/* The case's winning attempt, as rando_gen runs it, and its playthrough. */
static void generate_case(const char *case_dir, RandoRecord *record)
{
	RandoCase c;
	char error[RANDO_CASE_ERROR_MAX];

	if (!rando_case_read(case_dir, &c, error))
		test_fail(TEST, "%s rejected: %s", case_dir, error);
	CHECK(c.has_attempts);
	CHECK(rando_generate(&gen, &c.options, c.seed, c.attempts + 1, NULL, NULL) == RANDO_COMPLETE);
	rando_playthrough(&gen.world, &playthrough);
	record->options = c.options;
	record->heart_beep = c.heart_beep;
	record->seed = c.seed;
}

/* The spoiler, after its key up to the record's final "}\n", is the golden spoiler.json byte for byte;
 * before it, the text is the spoiler-free record up to the settings' closing brace. */
static void test_writer_with_spoiler(void)
{
	static const char key[] = "    },\n    \"spoiler\": ";
	RandoRecord record;
	const char *settings_end = strstr(baseline_text, "    }\n}\n");
	size_t start = (size_t)(settings_end - baseline_text), len;

	baseline_record(&record);
	generate_case("cases/baseline", &record);
	record.has_spoiler = true;
	write_record(&record, &gen.world, &playthrough, text);
	read_golden("cases/baseline");
	len = strlen(text);
	CHECK(strncmp(text, baseline_text, start + strlen("    }")) == 0);
	CHECK(strncmp(text + start, key, strlen(key)) == 0);
	CHECK(len >= 2 && strcmp(text + len - 2, "}\n") == 0);
	text[len - 2] = '\0';
	if (strcmp(text + start + strlen(key), golden) != 0)
		test_fail(TEST, "record spoiler differs from cases/baseline/spoiler.json");
}

static void check_options_equal(const char *what, const RandoOptions *got, const RandoOptions *expected)
{
	RandoOptionKey key;

	for (key = 0; key < RANDO_OPTION_KEY_COUNT; key++)
		if (rando_options_get(got, key) != rando_options_get(expected, key))
			test_fail(TEST, "%s: %s is %s, expected %s", what, rando_option_key_name(key),
				  rando_option_value_name(key, rando_options_get(got, key)),
				  rando_option_value_name(key, rando_options_get(expected, key)));
	if (got->hints != expected->hints)
		test_fail(TEST, "%s: hints differ", what);
}

static void check_record_equal(const char *what, const RandoRecord *got, const RandoRecord *expected)
{
	const RandoRecordDate *g = &got->date, *e = &expected->date;

	if (strcmp(got->name.code, expected->name.code) != 0 || strcmp(got->name.rom, expected->name.rom) != 0)
		test_fail(TEST, "%s: name %s / %s, expected %s / %s", what, got->name.code, got->name.rom,
			  expected->name.code, expected->name.rom);
	if (g->year != e->year || g->month != e->month || g->day != e->day || g->hour != e->hour ||
	    g->minute != e->minute || g->second != e->second)
		test_fail(TEST, "%s: date differs", what);
	if (got->seed != expected->seed)
		test_fail(TEST, "%s: seed %lu", what, (unsigned long)got->seed);
	check_options_equal(what, &got->options, &expected->options);
	if (got->heart_beep != expected->heart_beep)
		test_fail(TEST, "%s: heart_beep %s", what, rando_heart_beep_name(got->heart_beep));
	if (got->has_spoiler != expected->has_spoiler)
		test_fail(TEST, "%s: has_spoiler %d", what, (int)got->has_spoiler);
}

static void parse_or_fail(const char *what, const char *t, size_t len, RandoRecord *out)
{
	char error[RANDO_RECORD_ERROR_MAX];

	if (!rando_record_parse(t, len, out, error))
		test_fail(TEST, "%s: refused: %s", what, error);
}

/* The plan's check: every setting, hints and heart beep away from its default, a suffixed code, a date
 * with every field non-zero and two digits. */
static void round_trip_record(RandoRecord *record, bool spoiler)
{
	static const char *const settings[][2] = {
		{"glitches", "overworld_glitches"}, {"item_placement", "basic"}, {"dungeon_items", "mc"},
		{"accessibility", "none"},	    {"goal", "fast_ganon"},	   {"crystals_tower", "5"},
		{"crystals_ganon", "random"},	    {"state", "standard"},	   {"weapons", "assured"},
		{"item_pool", "hard"},		    {"item_functionality", "hard"},
	};
	size_t i;

	memset(record, 0, sizeof *record);
	rando_options_init(&record->options);
	for (i = 0; i < sizeof settings / sizeof settings[0]; i++)
		CHECK(rando_options_set(&record->options, settings[i][0], settings[i][1]));
	record->options.hints = false;
	record->heart_beep = RANDO_HEART_BEEP_QUARTER;
	record->seed = 1;
	rando_name_make(&record->options, rando_name_code(&record->options, record->heart_beep, record->seed), 12,
			&record->name);
	record->date.year = 2031;
	record->date.month = 12;
	record->date.day = 25;
	record->date.hour = 23;
	record->date.minute = 58;
	record->date.second = 47;
	record->has_spoiler = spoiler;
	if (spoiler) {
		CHECK(rando_generate(&gen, &record->options, record->seed, 1000, NULL, NULL) == RANDO_COMPLETE);
		rando_playthrough(&gen.world, &playthrough);
	}
}

static char again[RANDO_RECORD_TEXT_MAX];

static void test_round_trip_without_spoiler(void)
{
	RandoRecord record, parsed;

	round_trip_record(&record, false);
	write_record(&record, NULL, NULL, text);
	parse_or_fail("round trip", text, strlen(text), &parsed);
	check_record_equal("round trip", &parsed, &record);
	write_record(&parsed, NULL, NULL, again);
	CHECK(strcmp(again, text) == 0);
}

static char spoiler[RANDO_RECORD_TEXT_MAX];

static void test_round_trip_with_spoiler(void)
{
	RandoRecord record, parsed;
	FILE *f;
	size_t n;

	round_trip_record(&record, true);
	write_record(&record, &gen.world, &playthrough, text);
	parse_or_fail("round trip with spoiler", text, strlen(text), &parsed);
	check_record_equal("round trip with spoiler", &parsed, &record);
	f = tmpfile();
	CHECK(f != NULL);
	CHECK(rando_spoiler_write_json(f, &gen.world, &record.options, &playthrough));
	rewind(f);
	read_stream(f, "spoiler", spoiler);
	fclose(f);
	n = strlen(spoiler);
	CHECK(n > 0 && spoiler[n - 1] == '\n');
	CHECK(parsed.spoiler_size == n - 1);
	CHECK(parsed.spoiler_offset + parsed.spoiler_size <= strlen(text));
	CHECK(memcmp(text + parsed.spoiler_offset, spoiler, n - 1) == 0);
	write_record(&parsed, &gen.world, &playthrough, again);
	CHECK(strcmp(again, text) == 0);
}

static char edited[RANDO_RECORD_TEXT_MAX + 2];

/* base with its first old replaced by new, into edited. */
static const char *edit(const char *base, const char *old, const char *new_text)
{
	const char *at = strstr(base, old);

	if (!at)
		test_fail(TEST, "edit: \"%s\" not in the text", old);
	CHECK(strlen(base) - strlen(old) + strlen(new_text) < sizeof edited);
	memcpy(edited, base, (size_t)(at - base));
	strcpy(edited + (at - base), new_text);
	strcat(edited, at + strlen(old));
	return edited;
}

/* base without [first from, the next to after it), into edited. */
static const char *cut(const char *base, const char *from, const char *to)
{
	const char *start = strstr(base, from), *end = start ? strstr(start + 1, to) : NULL;

	if (!start || !end)
		test_fail(TEST, "cut: \"%s\" .. \"%s\" not in the text", from, to);
	memcpy(edited, base, (size_t)(start - base));
	strcpy(edited + (start - base), end);
	return edited;
}

static void refuse_len(const char *what, const char *t, size_t len, const char *word)
{
	RandoRecord out;
	char error[RANDO_RECORD_ERROR_MAX];

	error[0] = '\0';
	if (rando_record_parse(t, len, &out, error))
		test_fail(TEST, "%s: accepted", what);
	if (!strstr(error, word))
		test_fail(TEST, "%s: message \"%s\" does not name \"%s\"", what, error, word);
}

static void refuse(const char *what, const char *t, const char *word)
{
	refuse_len(what, t, strlen(t), word);
}

static char plain[RANDO_RECORD_TEXT_MAX], with_spoiler[RANDO_RECORD_TEXT_MAX];

static void test_refuses_a_missing_key(void)
{
	static const char *const keys[][3] = {
		{"code", "    \"code\"", "    \"date\""},	 {"date", "    \"date\"", "    \"format\""},
		{"format", "    \"format\"", "    \"rom\""}, {"rom", "    \"rom\"", "    \"seed\""},
		{"seed", "    \"seed\"", "    \"settings\""}, {"settings", ",\n    \"settings\"", "\n}\n"},
	};
	size_t i;

	for (i = 0; i < sizeof keys / sizeof keys[0]; i++)
		refuse(keys[i][0], cut(plain, keys[i][1], keys[i][2]), keys[i][0]);
}

static void test_refuses_bad_keys(void)
{
	refuse("duplicate key", edit(plain, "    \"seed\"", "    \"seed\": 1,\n    \"seed\""), "seed");
	refuse("unknown key", edit(plain, "    \"seed\"", "    \"extra\": 1,\n    \"seed\""), "extra");
	refuse("unknown settings key", edit(plain, "\"accessibility\"", "\"accessibilityx\""), "accessibilityx");
	refuse("missing settings key", cut(plain, "        \"accessibility\"", "        \"crystals_ganon\""),
	       "accessibility");
	refuse("missing heart_beep", cut(plain, "        \"heart_beep\"", "        \"hints\""), "heart_beep");
	refuse("duplicate settings key", edit(plain, "        \"goal\"", "        \"goal\": \"ganon\",\n        \"goal\""),
	       "goal");
}

static void test_refuses_bad_values(void)
{
	refuse("unknown value", edit(plain, "\"state\": \"standard\"", "\"state\": \"bogus\""), "state");
	refuse("unknown hints", edit(plain, "\"hints\": \"off\"", "\"hints\": \"no\""), "hints");
	refuse("unknown heart_beep", edit(plain, "\"heart_beep\": \"quarter\"", "\"heart_beep\": \"loud\""),
	       "heart_beep");
	refuse("setting not a string", edit(plain, "\"crystals_tower\": \"5\"", "\"crystals_tower\": 5"),
	       "crystals_tower");
	refuse("format 2", edit(plain, "\"format\": 1", "\"format\": 2"), "format");
	refuse("format string", edit(plain, "\"format\": 1", "\"format\": \"1\""), "format");
	refuse("seed too large", edit(plain, "\"seed\": 1,", "\"seed\": 4294967296,"), "seed");
	refuse("seed negative", edit(plain, "\"seed\": 1,", "\"seed\": -1,"), "seed");
	refuse("seed not an integer", edit(plain, "\"seed\": 1,", "\"seed\": 1.5,"), "seed");
	refuse("seed a string", edit(plain, "\"seed\": 1,", "\"seed\": \"1\","), "seed");
	refuse("escaped string", edit(plain, "\"state\": \"standard\"", "\"state\": \"stan\\u0064ard\""), "state");
}

static void test_refuses_a_bad_name(const RandoRecord *record)
{
	char old[64], bad[64];
	static const char *const codes[] = {"a179e3", "A179E", "A179E3F", "A179E3-1", "A179E3-0", "A179E3-02",
					    "A179E3-100", "", "A179E3-12x"};
	size_t i;

	CHECK(strcmp(record->name.code, "A179E3-12") == 0);
	snprintf(old, sizeof old, "\"code\": \"%s\"", record->name.code);
	for (i = 0; i < sizeof codes / sizeof codes[0]; i++) {
		snprintf(bad, sizeof bad, "\"code\": \"%s\"", codes[i]);
		refuse(codes[i], edit(plain, old, bad), "code");
	}
	refuse("rom of another code", edit(plain, "-A179E3-12.sfc", "-A179E3-13.sfc"), "rom");
	refuse("rom of another state", edit(plain, "Standard-FastGanon-", "Open-FastGanon-"), "rom");
	refuse("rom of another goal", edit(plain, "Standard-FastGanon-", "Standard-Ganon-"), "rom");
	refuse("code without its rom", edit(plain, old, "\"code\": \"A179E3\""), "rom");
}

static void test_refuses_a_bad_date(void)
{
	static const char *const dates[] = {
		"2031-12-25 23:58:47", "2031-12-25T23:58",    "2031-12-25T23:58:47Z", "31-12-25T23:58:47",
		"1969-12-25T23:58:47", "2031-13-25T23:58:47", "2031-00-25T23:58:47",  "2031-12-00T23:58:47",
		"2031-12-32T23:58:47", "2031-12-25T24:58:47", "2031-12-25T23:60:47",  "2031-12-25T23:58:60",
	};
	char bad[64];
	size_t i;

	for (i = 0; i < sizeof dates / sizeof dates[0]; i++) {
		snprintf(bad, sizeof bad, "\"date\": \"%s\"", dates[i]);
		refuse(dates[i], edit(plain, "\"date\": \"2031-12-25T23:58:47\"", bad), "date");
	}
}

static void test_refuses_a_bad_spoiler_or_text(void)
{
	size_t len = strlen(with_spoiler);
	static char big[RANDO_RECORD_TEXT_MAX + 1];

	refuse("spoiler array", edit(plain, "\n    }\n}\n", "\n    },\n    \"spoiler\": [1]\n}\n"), "spoiler");
	refuse("spoiler string", edit(plain, "\n    }\n}\n", "\n    },\n    \"spoiler\": \"x\"\n}\n"), "spoiler");
	CHECK(len > 4 && strcmp(with_spoiler + len - 4, "}\n}\n") == 0);
	strcpy(edited, with_spoiler);
	strcpy(edited + len - 4, "\n}\n");
	refuse("unbalanced spoiler", edited, "spoiler");
	refuse("spoiler with an unterminated string",
	       edit(plain, "\n    }\n}\n", "\n    },\n    \"spoiler\": {\"a\": \"b}\n}\n"), "spoiler");
	refuse("spoiler with a bad value",
	       edit(plain, "\n    }\n}\n", "\n    },\n    \"spoiler\": {\"a\": nul}\n}\n"), "spoiler");
	refuse("trailing text", edit(plain, "\n}\n", "\n}\nx"), "trailing");
	refuse("second object", edit(plain, "\n}\n", "\n}\n{}"), "trailing");
	refuse("truncated", edit(plain, "\n}\n", "\n"), "settings");
	refuse_len("empty", "", 0, "empty");
	memset(big, ' ', sizeof big);
	memcpy(big, plain, strlen(plain));
	refuse_len("over the maximum", big, sizeof big, "longer");
}

static void accept(const char *what, const char *t, const RandoRecord *expected, RandoRecord *out)
{
	parse_or_fail(what, t, strlen(t), out);
	check_record_equal(what, out, expected);
}

static void test_accepts_any_layout(const RandoRecord *record)
{
	static const char escaped[] = "{\"a\": \"q\\\"uote \\\\ back \\u0041\", \"b\": [[1, -2.5e3], null, true, false, {}], "
				      "\"c\": [], \"d\": {\"}\": \"]\"}}";
	static char scratch[RANDO_RECORD_TEXT_MAX];
	RandoRecord out, expected = *record;
	char block[256];

	strcpy(scratch, cut(plain, "    \"code\"", "    \"date\""));
	snprintf(block, sizeof block, "\n    },\n    \"code\": \"%s\"\n}\n", record->name.code);
	accept("keys reordered", edit(scratch, "\n    }\n}\n", block), &expected, &out);
	accept("extra whitespace", edit(plain, "{\n    \"code\": ", " \t\r\n{ \"code\"\n\t:\r"), &expected, &out);
	accept("whitespace around a number", edit(plain, "\"format\": 1,", "\"format\"  :  1  ,\n\n"), &expected, &out);
	expected.has_spoiler = true;
	snprintf(block, sizeof block, "\n    },\n    \"spoiler\": %s\n}\n", escaped);
	accept("spoiler with escapes and nesting", edit(plain, "\n    }\n}\n", block), &expected, &out);
	CHECK(out.spoiler_size == strlen(escaped));
	CHECK(memcmp(edited + out.spoiler_offset, escaped, out.spoiler_size) == 0);
}

static void test_refusals(void)
{
	RandoRecord record;

	round_trip_record(&record, false);
	write_record(&record, NULL, NULL, plain);
	round_trip_record(&record, true);
	write_record(&record, &gen.world, &playthrough, with_spoiler);
	record.has_spoiler = false;
	test_refuses_a_missing_key();
	test_refuses_bad_keys();
	test_refuses_bad_values();
	test_refuses_a_bad_name(&record);
	test_refuses_a_bad_date();
	test_refuses_a_bad_spoiler_or_text();
	test_accepts_any_layout(&record);
}

/* <dir>/<name>: the record of its winning attempt, with the spoiler, carries the golden spoiler.json and
 * parses back to the case's settings and seed. */
static void check_golden_case(const char *dir, const char *name)
{
	char case_dir[512];
	RandoRecord record, parsed;

	snprintf(case_dir, sizeof case_dir, "%s/%s", dir, name);
	baseline_record(&record);
	generate_case(case_dir, &record);
	rando_name_make(&record.options, rando_name_code(&record.options, record.heart_beep, record.seed), 1,
			&record.name);
	record.has_spoiler = true;
	write_record(&record, &gen.world, &playthrough, text);
	parse_or_fail(case_dir, text, strlen(text), &parsed);
	check_record_equal(case_dir, &parsed, &record);
	read_golden(case_dir);
	if (parsed.spoiler_size + 1 != strlen(golden) || golden[parsed.spoiler_size] != '\n' ||
	    memcmp(text + parsed.spoiler_offset, golden, parsed.spoiler_size) != 0)
		test_fail(TEST, "%s: record spoiler differs from its golden spoiler.json", case_dir);
}

static int check_golden_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *entry;
	int n = 0;

	if (d == NULL)
		test_fail(TEST, "cannot open %s/", dir);
	while ((entry = readdir(d)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		check_golden_case(dir, entry->d_name);
		n++;
	}
	closedir(d);
	return n;
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	test_writer_without_spoiler();
	test_writer_with_spoiler();
	test_round_trip_without_spoiler();
	test_round_trip_with_spoiler();
	test_refusals();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	printf(TEST ": %d goldens' record spoilers matched\n", check_golden_dir("cases") + check_golden_dir("combos"));
	return 0;
}
