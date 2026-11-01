#define _POSIX_C_SOURCE 200809L

#include "rando_last_settings.h"
#include "rando_seed_save.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_last_settings"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static char root[64], path[128], tmp_path[160];

static void make_root(void)
{
	strcpy(root, "/tmp/test_last_settings_XXXXXX");
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	snprintf(path, sizeof path, "%s/settings.json", root);
	snprintf(tmp_path, sizeof tmp_path, "%s.tmp", path);
}

static void remove_root(void)
{
	remove(tmp_path);
	remove(path);
	rmdir(root);
}

static bool exists(const char *p)
{
	struct stat st;

	return stat(p, &st) == 0;
}

static void write_file(const char *p, const char *bytes, size_t n)
{
	FILE *f = fopen(p, "wb");

	CHECK(f != NULL);
	CHECK(fwrite(bytes, 1, n, f) == n);
	CHECK(fclose(f) == 0);
}

static void non_default(RandoOptions *o, RandoHeartBeep *heart_beep)
{
	rando_options_init(o);
	o->glitches = RANDO_GLITCHES_MAJOR_GLITCHES;
	o->item_placement = RANDO_OPTION_PLACEMENT_BASIC;
	o->dungeon_items = RANDO_OPTION_DUNGEON_ITEMS_FULL;
	o->accessibility = RANDO_OPTION_ACCESSIBILITY_LOCATIONS;
	o->goal = RANDO_OPTION_GOAL_COMPLETIONIST;
	o->crystals_tower = RANDO_CRYSTALS_0;
	o->crystals_ganon = RANDO_CRYSTALS_RANDOM;
	o->state = RANDO_OPTION_STATE_RETRO;
	o->weapons = RANDO_OPTION_WEAPONS_SWORDLESS;
	o->item_pool = RANDO_ITEM_POOL_CROWD_CONTROL;
	o->item_functionality = RANDO_ITEM_FUNCTIONALITY_EXPERT;
	o->hints = false;
	*heart_beep = RANDO_HEART_BEEP_NORMAL;
}

static void check_defaults(const RandoOptions *o, RandoHeartBeep heart_beep)
{
	RandoOptions expected;

	rando_options_init(&expected);
	CHECK(memcmp(o, &expected, sizeof expected) == 0);
	CHECK(heart_beep == RANDO_HEART_BEEP_HALF);
}

static void test_round_trip(void)
{
	RandoOptions saved, loaded;
	RandoHeartBeep saved_beep, loaded_beep = RANDO_HEART_BEEP_HALF;

	make_root();
	non_default(&saved, &saved_beep);
	CHECK(rando_last_settings_save(path, &saved, saved_beep));
	CHECK(!exists(tmp_path));
	rando_options_init(&loaded);
	CHECK(rando_last_settings_load(path, &loaded, &loaded_beep) == RANDO_LAST_SETTINGS_LOADED);
	CHECK(memcmp(&loaded, &saved, sizeof saved) == 0);
	CHECK(loaded_beep == saved_beep);

	rando_options_init(&saved);
	saved.goal = RANDO_OPTION_GOAL_PEDESTAL;
	saved_beep = RANDO_HEART_BEEP_OFF;
	CHECK(rando_last_settings_save(path, &saved, saved_beep));
	CHECK(!exists(tmp_path));
	CHECK(rando_last_settings_load(path, &loaded, &loaded_beep) == RANDO_LAST_SETTINGS_LOADED);
	CHECK(memcmp(&loaded, &saved, sizeof saved) == 0);
	CHECK(loaded_beep == RANDO_HEART_BEEP_OFF);
	remove_root();
}

/* The file is the record's settings object at depth 0 and a newline. */
static void test_file_text(void)
{
	static const char expected[] = "{\n"
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
				       "}\n";
	char text[1024];
	RandoOptions o;
	FILE *f;
	size_t n;

	make_root();
	rando_options_init(&o);
	CHECK(rando_last_settings_save(path, &o, RANDO_HEART_BEEP_HALF));
	f = fopen(path, "rb");
	CHECK(f != NULL);
	n = fread(text, 1, sizeof text - 1, f);
	fclose(f);
	text[n] = '\0';
	if (strcmp(text, expected) != 0)
		test_fail(TEST, "settings file\n%s\nexpected\n%s", text, expected);
	remove_root();
}

static void expect_load(const char *what, RandoLastSettingsStatus expected)
{
	RandoOptions o;
	RandoHeartBeep heart_beep;

	non_default(&o, &heart_beep);
	if (rando_last_settings_load(path, &o, &heart_beep) != expected)
		test_fail(TEST, "%s: unexpected status", what);
	check_defaults(&o, heart_beep);
}

static void expect_invalid(const char *what, const char *bytes, size_t n)
{
	make_root();
	write_file(path, bytes, n);
	expect_load(what, RANDO_LAST_SETTINGS_INVALID);
	remove_root();
}

static void test_missing_and_invalid(void)
{
	static const char record[] = "{\n    \"code\": \"1CA10A\",\n    \"date\": \"2026-11-01T14:03:27\",\n"
				     "    \"format\": 1,\n    \"rom\": \"Open-Ganon-1CA10A.sfc\",\n    \"seed\": 1,\n"
				     "    \"settings\": {}\n}\n";
	static char big[RANDO_LAST_SETTINGS_MAX + 2];
	RandoOptions o;
	RandoHeartBeep heart_beep;

	make_root();
	expect_load("missing", RANDO_LAST_SETTINGS_MISSING);
	remove_root();
	expect_invalid("empty", "", 0);
	expect_invalid("garbage", "not json\n", 9);
	expect_invalid("a history record", record, strlen(record));
	{
		static const char partial[] = "{\"goal\": \"pedestal\", \"heart_beep\": \"off\"}\n";

		expect_invalid("a partial object", partial, strlen(partial));
	}

	make_root();
	rando_options_init(&o);
	o.goal = RANDO_OPTION_GOAL_DUNGEONS;
	CHECK(rando_last_settings_save(path, &o, RANDO_HEART_BEEP_DOUBLE));
	{
		FILE *f = fopen(path, "rb");
		size_t n;

		CHECK(f != NULL);
		n = fread(big, 1, sizeof big, f);
		fclose(f);
		memset(big + n, ' ', sizeof big - n);
	}
	remove_root();
	expect_invalid("over the cap", big, sizeof big);
	(void)heart_beep;
}

static void test_failed_save(void)
{
	char missing_dir[192], other[160];
	RandoOptions o, loaded;
	RandoHeartBeep heart_beep;

	make_root();
	rando_options_init(&o);
	CHECK(rando_last_settings_save(path, &o, RANDO_HEART_BEEP_HALF));
	snprintf(missing_dir, sizeof missing_dir, "%s/no_such_dir/settings.json", root);
	snprintf(other, sizeof other, "%s/no_such_dir/settings.json.tmp", root);
	non_default(&o, &heart_beep);
	CHECK(!rando_last_settings_save(missing_dir, &o, heart_beep));
	CHECK(!exists(missing_dir) && !exists(other));
	CHECK(rando_last_settings_load(path, &loaded, &heart_beep) == RANDO_LAST_SETTINGS_LOADED);
	check_defaults(&loaded, heart_beep);
	remove_root();
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	CHECK(strcmp(RANDO_SD_SETTINGS_FILE, "App/ALttPR/settings.json") == 0);
	test_round_trip();
	test_file_text();
	test_missing_and_invalid();
	test_failed_save();
	printf("%s: passed\n", TEST);
	return 0;
}
