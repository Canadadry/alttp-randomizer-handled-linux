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

/* Every setting, hints and heart beep away from its default, a suffixed code, a date
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

static char outline[RANDO_RECORD_TEXT_MAX];

/* The parsed record's spoiler outlines whole, with no top-level meta line. */
static void check_golden_outline(const char *case_dir, const RandoRecord *parsed)
{
	size_t len, pos;
	bool cut;

	if (!rando_record_spoiler_outline(text, parsed->spoiler_offset, parsed->spoiler_size, outline, sizeof outline,
					  &len, &cut))
		test_fail(TEST, "%s: spoiler outline refused", case_dir);
	if (cut)
		test_fail(TEST, "%s: spoiler outline cut", case_dir);
	for (pos = 0; pos < len; pos = (size_t)((const char *)memchr(outline + pos, '\n', len - pos) - outline) + 1)
		if (len - pos >= 5 && memcmp(outline + pos, "meta\n", 5) == 0)
			test_fail(TEST, "%s: spoiler outline has a meta line", case_dir);
	CHECK(len > 0 && outline[len - 1] == '\n');
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
	check_golden_outline(case_dir, &parsed);
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

/* Every setting away from its default, as round_trip_record sets them. */
static void non_default_settings(RandoOptions *options, RandoHeartBeep *heart_beep)
{
	RandoRecord record;

	round_trip_record(&record, false);
	*options = record.options;
	*heart_beep = record.heart_beep;
}

static void write_settings(const RandoOptions *options, RandoHeartBeep heart_beep, int depth,
			   char out[RANDO_RECORD_TEXT_MAX])
{
	FILE *f = tmpfile();

	CHECK(f != NULL);
	CHECK(rando_record_settings_write(f, options, heart_beep, depth));
	rewind(f);
	read_stream(f, "settings", out);
	fclose(f);
}

static const char baseline_settings[] = "{\n"
					"    \"accessibility\": \"items\",\n"
					"    \"crystals_ganon\": \"7\",\n"
					"    \"crystals_tower\": \"7\",\n"
					"    \"dungeon_items\": \"standard\",\n"
					"    \"glitches\": \"none\",\n"
					"    \"goal\": \"ganon\",\n"
					"    \"heart_beep\": \"half\",\n"
					"    \"hints\": \"on\",\n"
					"    \"item_functionality\": \"normal\",\n"
					"    \"item_placement\": \"advanced\",\n"
					"    \"item_pool\": \"normal\",\n"
					"    \"state\": \"open\",\n"
					"    \"weapons\": \"randomized\"\n"
					"}";

static const char non_default_text[] = "{\n"
				       "    \"accessibility\": \"none\",\n"
				       "    \"crystals_ganon\": \"random\",\n"
				       "    \"crystals_tower\": \"5\",\n"
				       "    \"dungeon_items\": \"mc\",\n"
				       "    \"glitches\": \"overworld_glitches\",\n"
				       "    \"goal\": \"fast_ganon\",\n"
				       "    \"heart_beep\": \"quarter\",\n"
				       "    \"hints\": \"off\",\n"
				       "    \"item_functionality\": \"hard\",\n"
				       "    \"item_placement\": \"basic\",\n"
				       "    \"item_pool\": \"hard\",\n"
				       "    \"state\": \"standard\",\n"
				       "    \"weapons\": \"assured\"\n"
				       "}";

static void test_settings_writer(void)
{
	static char out[RANDO_RECORD_TEXT_MAX];
	RandoOptions options;
	RandoHeartBeep heart_beep;
	const char *start = strstr(baseline_text, "\"settings\": ") + strlen("\"settings\": ");
	const char *end = strstr(baseline_text, "    }\n}\n") + strlen("    }");

	rando_options_init(&options);
	write_settings(&options, RANDO_HEART_BEEP_HALF, 0, out);
	if (strcmp(out, baseline_settings) != 0)
		test_fail(TEST, "baseline settings at depth 0\n%s\nexpected\n%s", out, baseline_settings);
	non_default_settings(&options, &heart_beep);
	write_settings(&options, heart_beep, 0, out);
	if (strcmp(out, non_default_text) != 0)
		test_fail(TEST, "non-default settings at depth 0\n%s\nexpected\n%s", out, non_default_text);
	rando_options_init(&options);
	write_settings(&options, RANDO_HEART_BEEP_HALF, 1, out);
	if (strlen(out) != (size_t)(end - start) || memcmp(out, start, (size_t)(end - start)) != 0)
		test_fail(TEST, "baseline settings at depth 1\n%s\nare not the record's settings", out);
}

static void settings_refuse(const char *what, const char *t, const char *word)
{
	RandoOptions options;
	RandoHeartBeep heart_beep;
	char error[RANDO_RECORD_ERROR_MAX];

	error[0] = '\0';
	if (rando_record_settings_parse(t, strlen(t), &options, &heart_beep, error))
		test_fail(TEST, "settings %s: accepted", what);
	if (!strstr(error, word))
		test_fail(TEST, "settings %s: message \"%s\" does not name \"%s\"", what, error, word);
}

static void test_settings_reader(void)
{
	static char scratch[RANDO_RECORD_TEXT_MAX];
	RandoOptions expected, options;
	RandoHeartBeep expected_beep, heart_beep;
	char error[RANDO_RECORD_ERROR_MAX];

	non_default_settings(&expected, &expected_beep);
	rando_options_init(&options);
	heart_beep = RANDO_HEART_BEEP_HALF;
	if (!rando_record_settings_parse(non_default_text, strlen(non_default_text), &options, &heart_beep, error))
		test_fail(TEST, "non-default settings refused: %s", error);
	CHECK(memcmp(&options, &expected, sizeof options) == 0);
	CHECK(heart_beep == expected_beep);
	snprintf(scratch, sizeof scratch, " \n%s\n\n", baseline_settings);
	if (!rando_record_settings_parse(scratch, strlen(scratch), &options, &heart_beep, error))
		test_fail(TEST, "baseline settings with whitespace refused: %s", error);
	rando_options_init(&expected);
	CHECK(memcmp(&options, &expected, sizeof options) == 0);
	CHECK(heart_beep == RANDO_HEART_BEEP_HALF);

	settings_refuse("missing key", cut(baseline_settings, "    \"accessibility\"", "    \"crystals_ganon\""),
			"accessibility");
	settings_refuse("missing heart_beep", cut(baseline_settings, "    \"heart_beep\"", "    \"hints\""),
			"heart_beep");
	settings_refuse("duplicate key", edit(baseline_settings, "    \"goal\"", "    \"goal\": \"ganon\",\n    \"goal\""),
			"goal");
	settings_refuse("unknown key", edit(baseline_settings, "    \"goal\"", "    \"extra\": \"x\",\n    \"goal\""),
			"extra");
	settings_refuse("spoiler key", edit(baseline_settings, "    \"goal\"", "    \"spoiler\": \"on\",\n    \"goal\""),
			"spoiler");
	settings_refuse("unknown value", edit(baseline_settings, "\"state\": \"open\"", "\"state\": \"bogus\""),
			"state");
	strcpy(scratch, baseline_settings);
	strcat(scratch, "\n{}");
	settings_refuse("trailing data", scratch, "trailing");
	strcpy(scratch, baseline_settings);
	strcat(scratch, "x");
	settings_refuse("trailing byte", scratch, "trailing");
	settings_refuse("empty", "", "settings");
	settings_refuse("not an object", "[]", "settings");
	settings_refuse("a whole record", baseline_text, "code");
}

/* span is outlined from offset 3 of a text that prefixes it with junk, so the offset is honoured. */
static bool outline_span(const char *span, size_t cap, size_t *len, bool *cut)
{
	static char t[RANDO_RECORD_TEXT_MAX];

	CHECK(strlen(span) + 4 < sizeof t);
	snprintf(t, sizeof t, "x{]%s", span);
	*len = (size_t)-1;
	*cut = true;
	return rando_record_spoiler_outline(t, 3, strlen(span), outline, cap, len, cut);
}

static void outline_is(const char *what, const char *span, const char *expected)
{
	size_t len;
	bool cut;

	if (!outline_span(span, sizeof outline, &len, &cut))
		test_fail(TEST, "%s: outline refused", what);
	if (cut || len != strlen(expected) || memcmp(outline, expected, len) != 0)
		test_fail(TEST, "%s: outline is \"%.*s\"%s, expected \"%s\"", what, (int)len, outline,
			  cut ? " (cut)" : "", expected);
}

static void test_outline_layout(void)
{
	outline_is("layout",
		   "{\"Bosses\": {\"Ganon\": \"Ganon\", \"Moth\": \"Mothula\"},\n \"Shops\": [{\"item\": \"Heart\", "
		   "\"price\": 10}, 7, []], \"empty\": {}, \"n\": null, \"t\": true, \"f\": -1.5e3}",
		   "Bosses\n"
		   "  Ganon: Ganon\n"
		   "  Moth: Mothula\n"
		   "Shops\n"
		   "  1\n"
		   "    item: Heart\n"
		   "    price: 10\n"
		   "  2: 7\n"
		   "  3\n"
		   "empty\n"
		   "n: null\n"
		   "t: true\n"
		   "f: -1.5e3\n");
	outline_is("empty spoiler", " { } ", "");
}

static void test_outline_names(void)
{
	outline_is("world suffix dropped in keys and values",
		   "{\"Dark World\": {\"Blacksmith:1\": \"KeyA1:12\", \"a:\": \"b:1x\", \":1\": \"x:1:2\"}}",
		   "Dark World\n"
		   "  Blacksmith: KeyA1\n"
		   "  a:: b:1x\n"
		   "  :1: x:1\n");
	outline_is("escapes kept as written", "{\"say\": \"a \\\"b\\\" \\\\\"}", "say: a \\\"b\\\" \\\\\n");
	outline_is("only the top-level meta skipped",
		   "{\"a\": 1, \"meta\": {\"logic\": \"NoGlitches\"}, \"b\": {\"meta\": 2}, \"metas\": 3}",
		   "a: 1\n"
		   "b\n"
		   "  meta: 2\n"
		   "metas: 3\n");
	outline_is("numeric keys first, in numeric order",
		   "{\"playthrough\": {\"1\": 1, \"10\": 10, \"11\": 11, \"2\": 2, \"chain\": 3, \"02\": 4, \"x1\": 5, "
		   "\"9\": {\"3\": 0, \"b\": 0, \"1\": 0}}}",
		   "playthrough\n"
		   "  1: 1\n"
		   "  2: 2\n"
		   "  02: 4\n"
		   "  9\n"
		   "    1: 0\n"
		   "    3: 0\n"
		   "    b: 0\n"
		   "  10: 10\n"
		   "  11: 11\n"
		   "  chain: 3\n"
		   "  x1: 5\n");
}

static void outline_refused(const char *what, const char *span)
{
	size_t len;
	bool cut;

	if (outline_span(span, sizeof outline, &len, &cut))
		test_fail(TEST, "%s: outline accepted", what);
}

static void test_outline_refusals(void)
{
	outline_is("nested 8 deep", "{\"a\": {\"b\": [{\"d\": {\"e\": {\"f\": [7]}}}]}}",
		   "a\n  b\n    1\n      d\n        e\n          f\n            1: 7\n");
	outline_refused("nested 9 deep", "{\"a\": {\"b\": [{\"d\": {\"e\": {\"f\": [[7]]}}}]}}");
	outline_refused("nested 9 deep, empty", "{\"a\": {\"b\": [{\"d\": {\"e\": {\"f\": [[[]]]}}}]}}");
	outline_refused("not an object", "[1]");
	outline_refused("empty span", "");
	outline_refused("trailing text", "{} 1");
	outline_refused("unclosed", "{\"a\": 1");
	outline_refused("no colon", "{\"a\" 1}");
	outline_refused("key not a string", "{a: 1}");
	outline_refused("bad value", "{\"a\": nope}");
	outline_refused("unterminated string", "{\"a\": \"b}");
	outline_refused("missing comma", "{\"a\": [1 2]}");
}

/* The span's outline, from "a: 1\n" to "d: 4444\n", is 5 + 6 + 7 + 8 = 26 bytes. */
static void test_outline_cut(void)
{
	static const char span[] = "{\"a\": 1, \"b\": 22, \"c\": 333, \"d\": 4444}";
	static const struct {
		size_t cap, len;
		bool cut;
	} cases[] = {{26, 26, false}, {27, 26, false}, {25, 18, true}, {18, 18, true}, {12, 11, true},
		     {11, 11, true},  {5, 5, true},    {4, 0, true},   {0, 0, true}};
	size_t i, len;
	bool cut;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		memset(outline, '#', sizeof outline);
		CHECK(outline_span(span, cases[i].cap, &len, &cut));
		if (len != cases[i].len || cut != cases[i].cut)
			test_fail(TEST, "cap %lu: outline of %lu bytes, cut %d", (unsigned long)cases[i].cap,
				  (unsigned long)len, cut);
		CHECK(memcmp(outline, "a: 1\nb: 22\nc: 333\nd: 4444\n", len) == 0);
		CHECK(outline[cases[i].cap] == '#');
	}
}

/* Line n of outline[0, len), without its newline; NULL past the end. */
static const char *outline_line(size_t len, int n, size_t *line_len)
{
	size_t pos = 0;

	for (; pos < len && n > 0; pos++)
		if (outline[pos] == '\n')
			n--;
	if (pos >= len)
		return NULL;
	*line_len = (size_t)((const char *)memchr(outline + pos, '\n', len - pos) - (outline + pos));
	return outline + pos;
}

/* Counts and lines from an independent outline of cases/baseline/spoiler.json in Python. */
static void test_outline_of_baseline_golden(void)
{
	static const struct {
		int n;
		const char *line;
	} pinned[] = {{0, "Bosses"},
		      {1, "  Desert Palace: Lanmolas"},
		      {18, "  Castle Tower - Dark Maze: KeyA1"},
		      {96, "  Equipment Slot 1: BossHeartContainer"},
		      {100, "  Ganon's Tower - Big Chest: ThreeBombs"},
		      {209, "Shops"},
		      {210, "  1"},
		      {212, "      item: RedPotion"},
		      {213, "      price: 150"},
		      {386, "playthrough"},
		      {389, "      Eastern Palace - Compass Chest: BigKeyP1"},
		      {397, "  2"},
		      {446, "  9"},
		      {449, "  10"},
		      {455, "  12"},
		      {458, "  longest_item_chain: 12"},
		      {459, "  regions_visited: 25"}};
	size_t len, line_len, i;
	const char *line;
	bool cut;
	int lines = 0;

	read_golden("cases/baseline");
	CHECK(rando_record_spoiler_outline(golden, 0, strlen(golden), outline, sizeof outline, &len, &cut));
	CHECK(!cut);
	CHECK(len == 13639);
	for (i = 0; i < len; i++)
		lines += outline[i] == '\n';
	CHECK(lines == 460);
	CHECK(outline[len - 1] == '\n');
	for (i = 0; i < sizeof pinned / sizeof pinned[0]; i++) {
		line = outline_line(len, pinned[i].n, &line_len);
		if (line == NULL || line_len != strlen(pinned[i].line) || memcmp(line, pinned[i].line, line_len) != 0)
			test_fail(TEST, "baseline outline line %d is \"%.*s\", expected \"%s\"", pinned[i].n,
				  line ? (int)line_len : 0, line ? line : "", pinned[i].line);
	}
}

static void test_outline(void)
{
	test_outline_of_baseline_golden();
	test_outline_layout();
	test_outline_names();
	test_outline_refusals();
	test_outline_cut();
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	test_writer_without_spoiler();
	test_writer_with_spoiler();
	test_round_trip_without_spoiler();
	test_round_trip_with_spoiler();
	test_refusals();
	test_settings_writer();
	test_settings_reader();
	test_outline();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	printf(TEST ": %d goldens' record spoilers matched and outlined\n", check_golden_dir("cases") + check_golden_dir("combos"));
	return 0;
}
