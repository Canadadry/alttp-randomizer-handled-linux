#include "rando_ladx_record.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_ladx_record"
#define SEED_A "e8515e7064fb5fd6458e72d56122673b"
#define TEXT_MAX (RANDO_RECORD_TEXT_MAX + 1)

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static char text[TEXT_MAX], edited[TEXT_MAX];

static void set(LadxSettings *s, const char *assignment)
{
	if (ladx_settings_set(s, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
}

static void sample(RandoLadxRecord *r)
{
	static const RandoRecordDate date = {2026, 11, 8, 9, 5, 7};

	memset(r, 0, sizeof *r);
	ladx_settings_defaults(&r->settings);
	set(&r->settings, "logic=hard");
	set(&r->settings, "goal=seashells");
	set(&r->settings, "dungeon_keys=keysanity");
	strcpy(r->seed, SEED_A);
	rando_ladx_name_make(&r->settings, rando_ladx_name_code(&r->settings, r->seed), 2, &r->name);
	r->date = date;
	r->has_spoiler = true;
}

static size_t render(const RandoLadxRecord *r)
{
	FILE *f = tmpfile();
	size_t n;

	CHECK(f != NULL);
	CHECK(rando_ladx_record_write(f, r));
	rewind(f);
	n = fread(text, 1, sizeof text - 1, f);
	fclose(f);
	text[n] = '\0';
	return n;
}

static void test_written_text(void)
{
	static const char header[] = "{\n"
				     "    \"code\": \"D5E4D9-2\",\n"
				     "    \"date\": \"2026-11-08T09:05:07\",\n"
				     "    \"format\": 1,\n"
				     "    \"rom\": \"Hard-Seashells-D5E4D9-2.gbc\",\n"
				     "    \"seed\": \"" SEED_A "\",\n"
				     "    \"settings\": {\n"
				     "        \"logic\": \"hard\",\n"
				     "        \"accessibility\": \"all\",\n";
	RandoLadxRecord r;
	char line[128];
	int k;

	sample(&r);
	render(&r);
	CHECK(strncmp(text, header, strlen(header)) == 0);
	for (k = 0; k < LADX_SETTING_COUNT; k++) {
		snprintf(line, sizeof line, "\n        \"%s\": \"%s\"%s\n", ladx_setting_key((LadxSettingId)k),
			 ladx_setting_value_name((LadxSettingId)k, r.settings.value[k]),
			 k + 1 < LADX_SETTING_COUNT ? "," : "");
		if (strstr(text, line) == NULL)
			test_fail(TEST, "no settings line %s", line);
	}
	CHECK(strstr(text, "\n    },\n    \"spoiler\": true\n}\n") != NULL);
	r.has_spoiler = false;
	render(&r);
	CHECK(strstr(text, "\n    \"spoiler\": false\n}\n") != NULL);
}

static void test_round_trip(void)
{
	RandoLadxRecord r, back;
	char error[RANDO_RECORD_ERROR_MAX];
	size_t n;

	sample(&r);
	n = render(&r);
	if (!rando_ladx_record_parse(text, n, &back, error))
		test_fail(TEST, "parse: %s", error);
	CHECK(strcmp(back.name.code, r.name.code) == 0 && strcmp(back.name.rom, r.name.rom) == 0);
	CHECK(memcmp(&back.date, &r.date, sizeof r.date) == 0);
	CHECK(strcmp(back.seed, SEED_A) == 0);
	CHECK(memcmp(&back.settings, &r.settings, sizeof r.settings) == 0);
	CHECK(back.has_spoiler);
	r.has_spoiler = false;
	n = render(&r);
	CHECK(rando_ladx_record_parse(text, n, &back, error) && !back.has_spoiler);
}

/* text with the first `from` replaced by `to`, parsed: must fail with error containing `why`. */
static void refused(const char *from, const char *to, const char *why)
{
	RandoLadxRecord r;
	char error[RANDO_RECORD_ERROR_MAX];
	const char *at = strstr(text, from);
	size_t head;

	if (at == NULL)
		test_fail(TEST, "no \"%s\" in the text", from);
	head = (size_t)(at - text);
	snprintf(edited, sizeof edited, "%.*s%s%s", (int)head, text, to, at + strlen(from));
	if (rando_ladx_record_parse(edited, strlen(edited), &r, error))
		test_fail(TEST, "accepted with \"%s\" -> \"%s\"", from, to);
	if (strstr(error, why) == NULL)
		test_fail(TEST, "\"%s\" -> \"%s\": error \"%s\", expected \"%s\"", from, to, error, why);
}

static void test_refusals(void)
{
	RandoLadxRecord r;
	char error[RANDO_RECORD_ERROR_MAX];

	sample(&r);
	render(&r);
	refused("\"format\": 1", "\"format\": 2", "format");
	refused("\"code\": \"D5E4D9-2\"", "\"code\": \"1cb4f8\"", "code");
	refused("\"code\": \"D5E4D9-2\",\n", "", "missing key \"code\"");
	refused("\"spoiler\": true", "\"spoiler\": 1", "spoiler");
	refused("},\n    \"spoiler\": true", "}", "missing key \"spoiler\"");
	refused("\"spoiler\": true\n", "\"spoiler\": true, \"extra\": 1\n", "unknown key \"extra\"");
	refused("\"format\": 1,", "\"format\": 1, \"format\": 1,", "duplicate key \"format\"");
	refused(SEED_A, "E8515E7064FB5FD6458E72D56122673B", "seed");
	refused(SEED_A, "e8515e", "seed");
	refused("\"logic\": \"hard\"", "\"logic\": \"easy\"", "unknown value");
	refused("\"logic\": \"hard\"", "\"logik\": \"hard\"", "unknown key \"logik\"");
	refused("\"accessibility\": \"all\",\n", "", "missing key \"accessibility\"");
	refused("\"logic\": \"hard\",", "\"logic\": \"hard\", \"logic\": \"hard\",", "duplicate key \"logic\"");
	refused("\"rom\": \"Hard-", "\"rom\": \"Hell-", "rom");
	refused("2026-11-08T09:05:07", "2026-13-08T09:05:07", "date");
	refused("\"rom\": \"Hard", "\"rom\": \"\\u0048ard", "escape");
	refused("\n}\n", "\n}\n}", "trailing");
	CHECK(!rando_ladx_record_parse(text, 0, &r, error));
}

int main(void)
{
	test_written_text();
	test_round_trip();
	test_refusals();
	printf("test_ladx_record: ok\n");
	return 0;
}
