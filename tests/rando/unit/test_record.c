#include "rando.h"
#include "rando_case.h"
#include "rando_record.h"
#include "support.h"

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

int main(void)
{
	test_writer_without_spoiler();
	test_writer_with_spoiler();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
