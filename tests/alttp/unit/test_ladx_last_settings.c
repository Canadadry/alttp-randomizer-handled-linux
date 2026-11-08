#define _POSIX_C_SOURCE 200809L

#include "rando_ladx_last_settings.h"
#include "rando_ladx_record.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_ladx_last_settings"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static char root[64], path[128], tmp_path[160];

static void make_root(void)
{
	strcpy(root, "/tmp/test_ladx_last_settings_XXXXXX");
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

static size_t read_file(const char *p, char *buf, size_t cap)
{
	FILE *f = fopen(p, "rb");
	size_t n;

	CHECK(f != NULL);
	n = fread(buf, 1, cap - 1, f);
	fclose(f);
	buf[n] = '\0';
	return n;
}

/* Every setting off its default, the last value of each. */
static void non_default(LadxSettings *s)
{
	int id;

	for (id = 0; id < LADX_SETTING_COUNT; id++)
		s->value[id] = (uint8_t)(ladx_setting_value_count((LadxSettingId)id) - 1 ==
						 ladx_setting_default((LadxSettingId)id)
					 ? 0
					 : ladx_setting_value_count((LadxSettingId)id) - 1);
}

static bool is_defaults(const LadxSettings *s)
{
	LadxSettings d;

	ladx_settings_defaults(&d);
	return memcmp(s, &d, sizeof d) == 0;
}

static void test_round_trip(void)
{
	LadxSettings saved, loaded;
	char text[4096];

	make_root();
	non_default(&saved);
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_MISSING && is_defaults(&loaded));
	CHECK(rando_ladx_last_settings_save(path, &saved));
	CHECK(!exists(tmp_path));
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_LOADED);
	CHECK(memcmp(&saved, &loaded, sizeof saved) == 0);
	read_file(path, text, sizeof text);
	CHECK(strncmp(text, "{\n    \"logic\": \"hell\",\n    \"accessibility\": \"goal\",\n", 52) == 0);
	CHECK(strstr(text, "    \"music\": \"ffa\"\n}\n") != NULL);
	ladx_settings_defaults(&saved);
	CHECK(rando_ladx_last_settings_save(path, &saved));
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_LOADED && is_defaults(&loaded));
	remove_root();
}

static void test_invalid(void)
{
	static const char *const bad[] = {
		"",
		"{}",
		"junk",
		"{\"logic\": \"hard\"}",
		"[]",
	};
	LadxSettings saved, loaded;
	char text[4096], edited[4200], *p;
	size_t i, n;

	make_root();
	for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		write_file(path, bad[i], strlen(bad[i]));
		non_default(&loaded);
		CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_INVALID);
		CHECK(is_defaults(&loaded));
	}
	non_default(&saved);
	CHECK(rando_ladx_last_settings_save(path, &saved));
	n = read_file(path, text, sizeof text);
	p = strstr(text, "\"hell\"");
	CHECK(p != NULL);
	snprintf(edited, sizeof edited, "%.*s\"nope\"%s", (int)(p - text), text, p + 6);
	write_file(path, edited, strlen(edited));
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_INVALID && is_defaults(&loaded));
	snprintf(edited, sizeof edited, "%s{}", text);
	write_file(path, edited, strlen(edited));
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_INVALID);
	p = strstr(text, "    \"race\"");
	CHECK(p != NULL);
	snprintf(edited, sizeof edited, "%.*s%s", (int)(p - text), text, strchr(p, '\n') + 1);
	write_file(path, edited, strlen(edited));
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_INVALID);
	write_file(path, text, n);
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_LOADED);
	remove(path);
	CHECK(mkdir(path, 0700) == 0);
	CHECK(rando_ladx_last_settings_load(path, &loaded) == RANDO_LAST_SETTINGS_INVALID && is_defaults(&loaded));
	CHECK(!rando_ladx_last_settings_save(path, &saved));
	rmdir(path);
	remove_root();
	CHECK(!rando_ladx_last_settings_save(path, &saved));
	CHECK(!exists(tmp_path));
}

int main(void)
{
	test_round_trip();
	test_invalid();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
