#include "ladx_settings.h"
#include "support.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_settings"
#define SETTINGS "oracle/settings.txt"
#define VALIDATE "oracle/settings_validate.txt"
#define TIER1 "oracle/tier1.txt"
#define SCOPE_CUTS "../../notes/plan3-scope-cuts.md"
#define MAX_LINE 1024
#define MAX_FIELDS 16

static char line_buf[MAX_LINE];
static char line_shown[MAX_LINE];
static const char *line_now = "(no fixture line)";

static void fail(const char *what)
{
	test_fail(TEST, "%s: %s", what, line_now);
}

typedef struct Fields {
	char *at[MAX_FIELDS];
	int count;
} Fields;

static void split(char *text, char sep, Fields *f)
{
	char *p = text;

	f->count = 0;
	for (;;) {
		char *next = strchr(p, sep);

		if (f->count == MAX_FIELDS)
			fail("too many fields");
		f->at[f->count++] = p;
		if (!next)
			break;
		*next = '\0';
		p = next + 1;
	}
}

/* The fixtures write the empty string as "". */
static const char *unquote(const char *s)
{
	return strcmp(s, "\"\"") == 0 ? "" : s;
}

static int next_line(FILE *file, const char *path)
{
	size_t len;

	if (!fgets(line_buf, sizeof line_buf, file))
		return 0;
	len = strlen(line_buf);
	if (len == 0 || line_buf[len - 1] != '\n')
		test_fail(TEST, "%s: line too long or unterminated", path);
	line_buf[len - 1] = '\0';
	strcpy(line_shown, line_buf);
	line_now = line_shown;
	return 1;
}

static FILE *open_fixture(const char *path)
{
	FILE *file = fopen(path, "r");

	if (!file)
		test_fail(TEST, "cannot open %s", path);
	return file;
}

static const char *kind_of(char *line, Fields *f)
{
	char *space = strchr(line, ' ');

	if (!space)
		fail("no kind");
	*space = '\0';
	split(space + 1, '\t', f);
	return line;
}

static int value_of(LadxSettingId id, const char *name)
{
	int v = ladx_setting_value_find(id, name);

	if (v < 0)
		fail("value not in the table");
	return v;
}

static void check_visible_if(LadxSettingId id, char *text)
{
	Fields f;
	LadxSettingId controller;
	int v, i;

	if (strcmp(text, "-") == 0) {
		if (ladx_setting_visible_if(id) != LADX_SETTING_COUNT)
			fail("visible_if: a controller where LADXR has none");
		return;
	}
	split(text, ',', &f);
	controller = ladx_setting_find(f.at[0]);
	if (controller == LADX_SETTING_COUNT || ladx_setting_visible_if(id) != controller)
		fail("visible_if: controller");
	for (v = 0; v < ladx_setting_value_count(controller); v++) {
		bool shown = false;

		for (i = 1; i < f.count; i++)
			shown = shown || strcmp(f.at[i], ladx_setting_value_name(controller, v)) == 0;
		if (ladx_setting_visible_for(id, v) != shown)
			fail("visible_if: a value of the controller");
	}
}

static LadxSettingId table_now = LADX_SETTING_COUNT;
static int options_now;

static void end_setting(void)
{
	if (table_now == LADX_SETTING_COUNT)
		return;
	if (ladx_setting_is_bool(table_now)) {
		if (options_now != 0 || ladx_setting_value_count(table_now) != 2 ||
		    strcmp(ladx_setting_value_name(table_now, 0), "0") != 0 ||
		    strcmp(ladx_setting_value_name(table_now, 1), "1") != 0 ||
		    strcmp(ladx_setting_value_short(table_now, 0), "") != 0 ||
		    strcmp(ladx_setting_value_short(table_now, 1), "") != 0)
			test_fail(TEST, "boolean %s: values not 0 and 1", ladx_setting_key(table_now));
	} else if (options_now != ladx_setting_value_count(table_now)) {
		test_fail(TEST, "%s: %d options in the fixture, %d in the table", ladx_setting_key(table_now),
			  options_now, ladx_setting_value_count(table_now));
	}
	table_now = LADX_SETTING_COUNT;
}

static void test_table(void)
{
	FILE *file = open_fixture(SETTINGS);
	int next_id = 0, left_out = 0, options = 0;
	bool skipping = false;

	while (next_line(file, SETTINGS)) {
		Fields f;
		const char *kind;

		if (line_buf[0] == '#')
			continue;
		kind = kind_of(line_buf, &f);
		if (strcmp(kind, "setting") == 0) {
			LadxSettingId id = (LadxSettingId)next_id;
			bool is_bool;

			if (f.count != 8)
				fail("setting: wrong field count");
			end_setting();
			skipping = strcmp(f.at[0], "seed") == 0 || strcmp(f.at[0], "forwardfactor") == 0;
			if (skipping) {
				if ((strcmp(f.at[0], "forwardfactor") == 0) != (strcmp(f.at[3], "float") == 0))
					fail("the float setting is not forwardfactor");
				if (ladx_setting_find(f.at[0]) != LADX_SETTING_COUNT)
					fail("a left-out setting is in the table");
				left_out++;
				continue;
			}
			if (strcmp(f.at[3], "float") == 0)
				fail("a float setting other than forwardfactor");
			if (next_id == LADX_SETTING_COUNT)
				fail("more settings in the fixture than in the table");
			if (strcmp(ladx_setting_key(id), f.at[0]) != 0 || ladx_setting_find(f.at[0]) != id)
				fail("key or order");
			if (strcmp(ladx_setting_category_name(ladx_setting_category(id)), f.at[1]) != 0)
				fail("category");
			if (strcmp(ladx_setting_short_key(id), f.at[2]) != 0)
				fail("short key");
			is_bool = strcmp(f.at[3], "bool") == 0;
			if (!is_bool && strcmp(f.at[3], "str") != 0)
				fail("unknown type");
			if (ladx_setting_is_bool(id) != is_bool)
				fail("type");
			if (strcmp(ladx_setting_value_name(id, ladx_setting_default(id)), unquote(f.at[4])) != 0)
				fail("default");
			if (strcmp(f.at[6], "0") != 0 && strcmp(f.at[6], "1") != 0)
				fail("malformed aesthetic");
			if (ladx_setting_aesthetic(id) != (f.at[6][0] == '1'))
				fail("aesthetic");
			check_visible_if(id, f.at[7]);
			table_now = id;
			options_now = 0;
			next_id++;
		} else if (strcmp(kind, "option") == 0) {
			if (f.count != 5)
				fail("option: wrong field count");
			if (skipping)
				fail("an option of a left-out setting");
			if (table_now == LADX_SETTING_COUNT || strcmp(f.at[0], ladx_setting_key(table_now)) != 0)
				fail("option outside its setting");
			if (options_now >= ladx_setting_value_count(table_now))
				fail("more options than the table's values");
			if (strcmp(ladx_setting_value_name(table_now, options_now), unquote(f.at[1])) != 0)
				fail("option value or order");
			if (value_of(table_now, unquote(f.at[1])) != options_now)
				fail("value find");
			if (strcmp(ladx_setting_value_short(table_now, options_now), unquote(f.at[2])) != 0)
				fail("option short");
			options_now++;
			options++;
		} else {
			fail("unknown kind");
		}
	}
	end_setting();
	fclose(file);
	line_now = "(end of " SETTINGS ")";
	if (next_id != LADX_SETTING_COUNT)
		fail("settings of the table missing from the fixture");
	if (left_out != 2)
		fail("seed and forwardfactor not both in the fixture");
	if (ladx_setting_find("nosuchsetting") != LADX_SETTING_COUNT || ladx_setting_find("") != LADX_SETTING_COUNT)
		fail("find of an unknown key");
	if (ladx_setting_value_find(LADX_SETTING_GOAL, "nosuchvalue") != -1 ||
	    ladx_setting_value_find(LADX_SETTING_GOAL, "") != -1)
		fail("find of an unknown value");
	printf("%s: table equal to %s (%d settings, %d options, seed and forwardfactor left out)\n", TEST,
	       SETTINGS, next_id, options);
}

static void apply(LadxSettings *s, const char *assignment)
{
	if (ladx_settings_set(s, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "%s refused: %s", assignment, line_now);
}

static void apply_fixture(LadxSettings *s, char *text)
{
	char assignment[MAX_LINE];
	char *eq = strchr(text, '=');

	if (!eq)
		fail("assignment without =");
	*eq = '\0';
	snprintf(assignment, sizeof assignment, "%s=%s", text, unquote(eq + 1));
	*eq = '=';
	apply(s, assignment);
}

static void test_defaults_and_set(void)
{
	LadxSettings s, before;
	int id;

	line_now = "(defaults and set)";
	ladx_settings_defaults(&s);
	for (id = 0; id < LADX_SETTING_COUNT; id++)
		if (s.value[id] != ladx_setting_default((LadxSettingId)id))
			fail("a fresh set is not the defaults");
	apply(&s, "goal=maze");
	if (s.value[LADX_SETTING_GOAL] != ladx_setting_value_find(LADX_SETTING_GOAL, "maze"))
		fail("set goal=maze");
	apply(&s, "logic=");
	if (strcmp(ladx_setting_value_name(LADX_SETTING_LOGIC, s.value[LADX_SETTING_LOGIC]), "") != 0)
		fail("logic= is not the empty string");
	apply(&s, "race=1");
	apply(&s, "heartpiece=0");
	if (s.value[LADX_SETTING_RACE] != 1 || s.value[LADX_SETTING_HEARTPIECE] != 0)
		fail("booleans 1 and 0");
	before = s;
	if (ladx_settings_set(&s, "nosuch=1") != LADX_SETTINGS_UNKNOWN_KEY ||
	    ladx_settings_set(&s, "=1") != LADX_SETTINGS_UNKNOWN_KEY ||
	    ladx_settings_set(&s, "goal=nosuch") != LADX_SETTINGS_UNKNOWN_VALUE ||
	    ladx_settings_set(&s, "goal= maze") != LADX_SETTINGS_UNKNOWN_VALUE ||
	    ladx_settings_set(&s, "goal=maze=") != LADX_SETTINGS_UNKNOWN_VALUE ||
	    ladx_settings_set(&s, "race=2") != LADX_SETTINGS_UNKNOWN_VALUE ||
	    ladx_settings_set(&s, "race=") != LADX_SETTINGS_UNKNOWN_VALUE ||
	    ladx_settings_set(&s, "race=true") != LADX_SETTINGS_UNKNOWN_VALUE ||
	    ladx_settings_set(&s, "race") != LADX_SETTINGS_MALFORMED ||
	    ladx_settings_set(&s, "") != LADX_SETTINGS_MALFORMED)
		fail("a refused assignment's status");
	if (memcmp(&s, &before, sizeof s) != 0)
		fail("a refused assignment changed the settings");
}

static void test_short_string_buffer(void)
{
	LadxSettings s;
	char buf[LADX_SETTINGS_SHORT_MAX + 1];
	int id, longest = 0;

	line_now = "(short string buffer)";
	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		LadxSettingId i = (LadxSettingId)id;
		int v, most = 0;

		if (ladx_setting_is_bool(i)) {
			longest += (int)strlen(ladx_setting_short_key(i));
			continue;
		}
		for (v = 0; v < ladx_setting_value_count(i); v++)
			if (v != ladx_setting_default(i) && (int)strlen(ladx_setting_value_short(i, v)) > most)
				most = (int)strlen(ladx_setting_value_short(i, v));
		longest += (int)strlen(ladx_setting_short_key(i)) + most;
	}
	if (longest != LADX_SETTINGS_SHORT_MAX)
		fail("LADX_SETTINGS_SHORT_MAX is not the longest short string");
	ladx_settings_defaults(&s);
	if (ladx_settings_short_string(&s, buf, 7) != -1 || buf[0] != '\0')
		fail("a short buffer is not refused with an empty string");
	if (ladx_settings_short_string(&s, buf, 8) != 7 || strcmp(buf, "hsHTWRl") != 0)
		fail("the defaults' short string in an exact buffer");
}

static void test_validate(void)
{
	FILE *file = open_fixture(VALIDATE);
	int lines = 0, changed = 0;

	while (next_line(file, VALIDATE)) {
		LadxSettings s, input;
		Fields in;
		char *arrow, *tab, *outputs;
		char got[MAX_LINE] = "";
		char buf[LADX_SETTINGS_SHORT_MAX + 1];
		int i, id;

		if (line_buf[0] == '#')
			continue;
		if (strncmp(line_buf, "validate ", 9) != 0)
			fail("unknown kind");
		arrow = strstr(line_buf, " -> ");
		tab = strchr(line_buf, '\t');
		if (!arrow || !tab || tab < arrow || strchr(tab + 1, '\t'))
			fail("malformed validate line");
		*arrow = '\0';
		*tab = '\0';
		outputs = arrow + 4;
		ladx_settings_defaults(&s);
		split(line_buf + 9, ',', &in);
		for (i = 0; i < in.count; i++)
			apply_fixture(&s, in.at[i]);
		input = s;
		ladx_settings_validate(&s);
		for (id = 0; id < LADX_SETTING_COUNT; id++) {
			const char *name;

			if (s.value[id] == input.value[id])
				continue;
			name = ladx_setting_value_name((LadxSettingId)id, s.value[id]);
			snprintf(got + strlen(got), sizeof got - strlen(got), "%s%s=%s", got[0] ? "," : "",
				 ladx_setting_key((LadxSettingId)id), name[0] ? name : "\"\"");
		}
		if (strcmp(outputs, "=") == 0 ? got[0] != '\0' : strcmp(got, outputs) != 0)
			test_fail(TEST, "validate changed %s: %s", got[0] ? got : "nothing", line_now);
		changed += got[0] != '\0';
		if (ladx_settings_short_string(&s, buf, sizeof buf) != (int)strlen(unquote(tab + 1)) ||
		    strcmp(buf, unquote(tab + 1)) != 0)
			test_fail(TEST, "short string %s: %s", buf, line_now);
		lines++;
	}
	fclose(file);
	line_now = "(end of " VALIDATE ")";
	if (lines == 0)
		fail("no validate line");
	printf("%s: %d validate lines replayed from %s (%d rewritten), short strings equal\n", TEST, lines,
	       VALIDATE, changed);
}

/* Tier 1 written out by hand: NULL stands for every value of the setting but its default. */
static const struct {
	const char *key;
	const char *value;
	const char *reason;
} unsupported_values[] = {
	{"randomstartlocation", "1", "tier2-entrances"},
	{"dungeonshuffle", "1", "tier2-entrances"},
	{"entranceshuffle", "simple", "tier2-entrances"},
	{"entranceshuffle", "split", "tier2-entrances"},
	{"entranceshuffle", "mixed", "tier2-entrances"},
	{"entranceshuffle", "wild", "tier2-entrances"},
	{"entranceshuffle", "chaos", "tier2-entrances"},
	{"entranceshuffle", "insane", "tier2-entrances"},
	{"entranceshuffle", "madness", "tier2-entrances"},
	{"shufflejunk", "1", "tier2-entrances"},
	{"shuffleannoying", "1", "tier2-entrances"},
	{"shufflewater", "1", "tier2-entrances"},
	{"goal", "bingo", "goal-bingo"},
	{"goal", "bingo-double", "goal-bingo"},
	{"goal", "bingo-triple", "goal-bingo"},
	{"goal", "bingo-full", "goal-bingo"},
	{"overworld", "dungeondive", "tier3-overworld"},
	{"overworld", "nodungeons", "tier3-overworld"},
	{"overworld", "dungeonchain", "tier3-overworld"},
	{"overworld", "random", "tier3-overworld"},
	{"overworld", "alttp", "tier3-overworld"},
	{"dungeonchainlength", "3", "tier3-overworld"},
	{"dungeonchainlength", "4", "tier3-overworld"},
	{"dungeonchainlength", "6", "tier3-overworld"},
	{"dungeonchainlength", "7", "tier3-overworld"},
	{"dungeonchainlength", "8", "tier3-overworld"},
	{"shopsanity", "basic", "tier3-shopsanity"},
	{"shopsanity", "important", "tier3-shopsanity"},
	{"gfxmod", NULL, "gfxmod-licence"},
	{"music", "ffa", "music-ffa"},
};

static const struct {
	const char *key_a;
	const char *value_a;
	const char *key_b;
	const char *value_b;
	const char *reason;
} unsupported_pairs[] = {
	{"boomerang", "default", "accessibility", "all", "ladxr-refuses"},
};

#define COUNT_OF(a) (int)(sizeof a / sizeof a[0])

static const char *expected_reason(LadxSettingId id, int value)
{
	int i;

	for (i = 0; i < COUNT_OF(unsupported_values); i++) {
		if (strcmp(unsupported_values[i].key, ladx_setting_key(id)) != 0)
			continue;
		if (unsupported_values[i].value ? strcmp(unsupported_values[i].value, ladx_setting_value_name(id, value)) == 0
						: value != ladx_setting_default(id))
			return unsupported_values[i].reason;
	}
	return "none";
}

static void test_unsupported_list(void)
{
	int id, v, i, unsupported = 0;

	line_now = "(unsupported list)";
	for (i = 0; i < COUNT_OF(unsupported_values); i++) {
		LadxSettingId sid = ladx_setting_find(unsupported_values[i].key);

		if (sid == LADX_SETTING_COUNT ||
		    (unsupported_values[i].value && ladx_setting_value_find(sid, unsupported_values[i].value) < 0))
			test_fail(TEST, "expected unsupported %s=%s not in the table", unsupported_values[i].key,
				  unsupported_values[i].value ? unsupported_values[i].value : "*");
	}
	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		LadxSettingId sid = (LadxSettingId)id;

		for (v = 0; v < ladx_setting_value_count(sid); v++) {
			const char *want = expected_reason(sid, v);
			const char *got = ladx_reason_id(ladx_setting_value_reason(sid, v));

			if (strcmp(want, got) != 0)
				test_fail(TEST, "%s=%s: reason %s, expected %s", ladx_setting_key(sid),
					  ladx_setting_value_name(sid, v), got, want);
			unsupported += strcmp(want, "none") != 0;
		}
		if (ladx_setting_value_reason(sid, ladx_setting_default(sid)) != LADX_REASON_NONE)
			test_fail(TEST, "the default of %s is unsupported", ladx_setting_key(sid));
	}
	if (ladx_setting_pair_count() != COUNT_OF(unsupported_pairs) || ladx_setting_pair(-1) ||
	    ladx_setting_pair(ladx_setting_pair_count()))
		fail("pair count");
	for (i = 0; i < COUNT_OF(unsupported_pairs); i++) {
		const LadxSettingPair *p = ladx_setting_pair(i);

		if (!p || strcmp(ladx_setting_key(p->a), unsupported_pairs[i].key_a) != 0 ||
		    strcmp(ladx_setting_value_name(p->a, p->value_a), unsupported_pairs[i].value_a) != 0 ||
		    strcmp(ladx_setting_key(p->b), unsupported_pairs[i].key_b) != 0 ||
		    strcmp(ladx_setting_value_name(p->b, p->value_b), unsupported_pairs[i].value_b) != 0 ||
		    strcmp(ladx_reason_id(p->reason), unsupported_pairs[i].reason) != 0)
			fail("pair differs from the expected list");
	}
	printf("%s: %d values and %d pair unsupported, as plan 3 section 2 lists\n", TEST, unsupported,
	       COUNT_OF(unsupported_pairs));
}

static void test_tier1(void)
{
	FILE *file = open_fixture(TIER1);
	bool listed[LADX_SETTING_COUNT] = {false};
	bool tier1[LADX_SETTING_COUNT][64] = {{false}};
	int id, v, values = 0;

	while (next_line(file, TIER1)) {
		Fields f;
		LadxSettingId sid;
		int i;

		if (line_buf[0] == '#')
			continue;
		split(line_buf, ' ', &f);
		sid = ladx_setting_find(f.at[0]);
		if (sid == LADX_SETTING_COUNT || listed[sid] || f.count < 2)
			fail("tier-1 line");
		listed[sid] = true;
		for (v = 0; v < ladx_setting_value_count(sid); v++)
			tier1[sid][v] = f.count == 2 && strcmp(f.at[1], "all") == 0;
		for (i = 1; i < f.count && strcmp(f.at[1], "all") != 0; i++)
			tier1[sid][value_of(sid, unquote(f.at[i]))] = true;
	}
	fclose(file);
	line_now = "(tier-1 consistency)";
	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		LadxSettingId sid = (LadxSettingId)id;

		for (v = 0; v < ladx_setting_value_count(sid); v++) {
			bool supported = ladx_setting_value_reason(sid, v) == LADX_REASON_NONE;
			bool want = tier1[id][v] || (!listed[id] && v == ladx_setting_default(sid));

			if (supported != want)
				test_fail(TEST, "%s=%s %s alone, tier1.txt says otherwise", ladx_setting_key(sid),
					  ladx_setting_value_name(sid, v), supported ? "supported" : "unsupported");
			values += tier1[id][v];
		}
	}
	printf("%s: %d tier-1 values supported alone, every other non-default value unsupported\n", TEST, values);
}

static void test_scope_cut_heading(void)
{
	FILE *file = open_fixture(SCOPE_CUTS);
	char heading[64];
	int found = 0;

	snprintf(heading, sizeof heading, "## %s",
		 ladx_reason_id(ladx_setting_value_reason(LADX_SETTING_GOAL,
							  ladx_setting_value_find(LADX_SETTING_GOAL, "bingo"))));
	while (next_line(file, SCOPE_CUTS))
		found += strcmp(line_buf, heading) == 0;
	fclose(file);
	if (found != 1)
		test_fail(TEST, "%s: no single heading \"%s\"", SCOPE_CUTS, heading);
}

static void expect_finding(const LadxSettings *s, const char *a, const char *b, const char *reason)
{
	LadxSettingPair finding;

	if (ladx_settings_supported(s, &finding))
		fail("supported, expected unsupported");
	if (strcmp(ladx_setting_key(finding.a), a) != 0 || strcmp(ladx_reason_id(finding.reason), reason) != 0 ||
	    (b ? finding.b == LADX_SETTING_COUNT || strcmp(ladx_setting_key(finding.b), b) != 0
	       : finding.b != LADX_SETTING_COUNT) ||
	    finding.value_a != s->value[finding.a] || (b && finding.value_b != s->value[finding.b]))
		fail("finding");
}

static void test_supported(void)
{
	LadxSettings s;

	line_now = "(whole-set check)";
	ladx_settings_defaults(&s);
	if (!ladx_settings_supported(&s, NULL))
		fail("defaults unsupported");
	apply(&s, "boomerang=default");
	expect_finding(&s, "boomerang", "accessibility", "ladxr-refuses");
	apply(&s, "accessibility=goal");
	if (!ladx_settings_supported(&s, NULL))
		fail("boomerang=default with accessibility=goal unsupported");
	apply(&s, "goal=maze");
	ladx_settings_validate(&s);
	expect_finding(&s, "boomerang", "accessibility", "ladxr-refuses");
	apply(&s, "music=ffa");
	apply(&s, "gfxmod=Kirby.bin");
	expect_finding(&s, "gfxmod", NULL, "gfxmod-licence");
}

static int check_config(const char *dir, const char *name)
{
	char path[1024];
	FILE *file;
	LadxSettings s, input;

	snprintf(path, sizeof path, "%s/%s/config.txt", dir, name);
	file = open_fixture(path);
	ladx_settings_defaults(&s);
	while (next_line(file, path))
		if (ladx_settings_set(&s, line_buf) != LADX_SETTINGS_OK)
			test_fail(TEST, "%s: %s refused", path, line_buf);
	fclose(file);
	line_now = path;
	input = s;
	ladx_settings_validate(&s);
	if (memcmp(&s, &input, sizeof s) != 0)
		fail("validate changes the case's settings");
	if (!ladx_settings_supported(&s, NULL))
		fail("the case's settings are unsupported");
	return 1;
}

static int check_configs(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (!d)
		test_fail(TEST, "cannot open %s", dir);
	while ((e = readdir(d)) != NULL)
		if (e->d_name[0] != '.')
			n += check_config(dir, e->d_name);
	closedir(d);
	if (n == 0)
		test_fail(TEST, "no folder in %s", dir);
	return n;
}

static void test_cases(void)
{
	int cases = check_configs("cases");
	int combos = check_configs("combos");

	printf("%s: %d cases and %d combos parse, survive validate and are supported\n", TEST, cases, combos);
}

int main(void)
{
	test_table();
	test_defaults_and_set();
	test_short_string_buffer();
	test_validate();
	test_unsupported_list();
	test_tier1();
	test_scope_cut_heading();
	test_supported();
	test_cases();
	return 0;
}
