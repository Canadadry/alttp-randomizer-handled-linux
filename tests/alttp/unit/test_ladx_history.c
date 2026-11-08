#define _POSIX_C_SOURCE 200809L

#include "rando_ladx_history.h"
#include "rando_ladx_record.h"
#include "rando_seed_save.h"
#include "support.h"
#include "ui_strings.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_ladx_history"
#define SEED_A "e8515e7064fb5fd6458e72d56122673b"
#define SPOILER "{\n  \"accessibleItems\": [\n    {\n      \"id\": \"0x2A3\",\n      \"sphere\": 0\n    }\n  ]\n}"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static RandoLadxHistory history;
static RandoLadxHistoryList list;
static UiHistorySpoiler spoiler;
static char root[64];
static char outline[UI_HISTORY_SPOILER_MAX];

static const char *at(const char *rel)
{
	static char path[1024];

	snprintf(path, sizeof path, "%s/%s", root, rel);
	return path;
}

static bool exists(const char *rel)
{
	struct stat st;

	return lstat(at(rel), &st) == 0;
}

static void make_dir(const char *rel)
{
	if (mkdir(at(rel), 0700) != 0)
		test_fail(TEST, "cannot create %s", at(rel));
}

static void make_dirs(const char *rel)
{
	char path[512], *p;

	snprintf(path, sizeof path, "%s", rel);
	for (p = strchr(path, '/'); p != NULL; p = strchr(p + 1, '/')) {
		*p = '\0';
		if (!exists(path))
			make_dir(path);
		*p = '/';
	}
	if (!exists(path))
		make_dir(path);
}

static void write_text(const char *rel, const char *s)
{
	FILE *f = fopen(at(rel), "wb");

	if (f == NULL || fwrite(s, 1, strlen(s), f) != strlen(s) || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", at(rel));
}

static void remove_all(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *e;
	char child[1024];

	if (lstat(path, &st) != 0)
		return;
	if (S_ISDIR(st.st_mode)) {
		d = opendir(path);
		while (d != NULL && (e = readdir(d)) != NULL) {
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
				continue;
			snprintf(child, sizeof child, "%s/%s", path, e->d_name);
			remove_all(child);
		}
		if (d != NULL)
			closedir(d);
		rmdir(path);
	} else {
		remove(path);
	}
}

static void make_root(void)
{
	strcpy(root, "/tmp/test_ladx_history_XXXXXX");
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	CHECK(rando_ladx_history_init(&history, root));
}

static void drop_root(void)
{
	remove_all(root);
}

/* A seed's record (and ROM, and spoiler when spoiler) planted as the runner writes them; its name in *out. */
static void plant(int day, int salt, bool rom, bool with_spoiler, RandoName *out)
{
	RandoLadxRecord r;
	RandoRecordDate date = {2026, 11, day, 12, 0, 0};
	char rel[512];
	FILE *f;

	memset(&r, 0, sizeof r);
	ladx_settings_defaults(&r.settings);
	snprintf(r.seed, sizeof r.seed, "%032x", salt);
	rando_ladx_name_make(&r.settings, rando_ladx_name_code(&r.settings, r.seed), 1, &r.name);
	r.date = date;
	r.has_spoiler = with_spoiler;
	make_dirs(RANDO_SD_LADX_HISTORY_DIR);
	make_dirs(RANDO_SD_LADX_ROM_DIR);
	snprintf(rel, sizeof rel, "%s/%s.json", RANDO_SD_LADX_HISTORY_DIR, r.name.code);
	f = fopen(at(rel), "wb");
	CHECK(f != NULL && rando_ladx_record_write(f, &r) && fclose(f) == 0);
	if (rom) {
		snprintf(rel, sizeof rel, "%s/%s", RANDO_SD_LADX_ROM_DIR, r.name.rom);
		write_text(rel, "rom");
	}
	if (with_spoiler) {
		snprintf(rel, sizeof rel, "%s/%s.spoiler.json", RANDO_SD_LADX_HISTORY_DIR, r.name.code);
		write_text(rel, SPOILER);
	}
	if (out != NULL)
		*out = r.name;
}

static void test_missing_and_unlistable_folder(void)
{
	make_root();
	rando_ladx_history_scan(&history, &list);
	CHECK(list.count == 0 && !list.folder_unreadable && !list.unreadable && !list.truncated);
	make_dirs(RANDO_SD_LADX_DIR);
	write_text(RANDO_SD_LADX_HISTORY_DIR, "not a folder");
	rando_ladx_history_scan(&history, &list);
	CHECK(list.count == 0 && list.folder_unreadable);
	drop_root();
}

static void test_newest_first_rom_present_only(void)
{
	RandoName a, b, c;
	char rel[512];

	make_root();
	plant(3, 1, true, false, &a);
	plant(5, 2, true, true, &b);
	plant(4, 3, false, false, &c); /* its ROM is gone */
	write_text(RANDO_SD_LADX_HISTORY_DIR "/notes.txt", "x");
	write_text(RANDO_SD_LADX_HISTORY_DIR "/zz.json", "{}");
	rando_ladx_history_scan(&history, &list);
	CHECK(list.count == 2 && !list.unreadable && !list.truncated && !list.folder_unreadable);
	CHECK(strcmp(list.entries[0].name.rom, b.rom) == 0 && list.entries[0].has_spoiler);
	CHECK(strcmp(list.entries[1].name.rom, a.rom) == 0 && !list.entries[1].has_spoiler);
	CHECK(list.entries[0].date.day == 5 && strcmp(list.entries[0].seed, "00000000000000000000000000000002") == 0);

	snprintf(rel, sizeof rel, "%s/%s.json", RANDO_SD_LADX_HISTORY_DIR, a.code);
	write_text(rel, "{ broken");
	rando_ladx_history_scan(&history, &list);
	CHECK(list.count == 1 && list.unreadable);
	drop_root();
}

static void test_truncated(void)
{
	int i;

	make_root();
	for (i = 0; i <= UI_HISTORY_MAX; i++)
		plant(1 + i % 28, 100 + i, true, false, NULL);
	rando_ladx_history_scan(&history, &list);
	CHECK(list.count == UI_HISTORY_MAX && list.truncated);
	CHECK(list.entries[0].date.day == 28 && list.entries[UI_HISTORY_MAX - 1].date.day >= 1);
	drop_root();
}

static void test_spoiler_outline(void)
{
	RandoName with, without;
	char rel[512];
	size_t len;
	bool cut;

	make_root();
	plant(1, 1, true, true, &with);
	plant(2, 2, true, false, &without);
	CHECK(rando_ladx_history_spoiler(&history, &with, &spoiler));
	CHECK(rando_record_spoiler_outline(SPOILER, 0, strlen(SPOILER), outline, sizeof outline, &len, &cut));
	CHECK(spoiler.len == len && memcmp(spoiler.text, outline, len) == 0 && !spoiler.cut);
	CHECK(strstr(outline, "sphere: 0") != NULL);
	CHECK(!rando_ladx_history_spoiler(&history, &without, &spoiler));
	snprintf(rel, sizeof rel, "%s/%s.spoiler.json", RANDO_SD_LADX_HISTORY_DIR, with.code);
	remove(at(rel));
	CHECK(!rando_ladx_history_spoiler(&history, &with, &spoiler));
	drop_root();
}

static const char *const profiles[] = {RANDO_SD_PROFILE_CURRENT, RANDO_SD_PROFILE_MAIN, RANDO_SD_PROFILE_GUEST};

/* name's saves, states and override in every profile, plus an ALttP seed's of the same base name. */
static void plant_seed_files(const RandoName *name)
{
	char base[64], rel[512];
	size_t i;

	snprintf(base, sizeof base, "%.*s", (int)(strlen(name->rom) - 4), name->rom);
	for (i = 0; i < 3; i++) {
		static const char *const states[] = {".state", ".state1", ".state.auto", ".state.png", ".state10.png"};
		size_t k;

		snprintf(rel, sizeof rel, "%s/saves/Gambatte", profiles[i]);
		make_dirs(rel);
		snprintf(rel, sizeof rel, "%s/saves/Gambatte/%s.srm", profiles[i], base);
		write_text(rel, "s");
		snprintf(rel, sizeof rel, "%s/saves/Gambatte/%s.rtc", profiles[i], base);
		write_text(rel, "s");
		snprintf(rel, sizeof rel, "%s/states/Gambatte", profiles[i]);
		make_dirs(rel);
		for (k = 0; k < sizeof states / sizeof states[0]; k++) {
			snprintf(rel, sizeof rel, "%s/states/Gambatte/%s%s", profiles[i], base, states[k]);
			write_text(rel, "s");
		}
		snprintf(rel, sizeof rel, "%s/saves/Supafaust", profiles[i]);
		make_dirs(rel);
		snprintf(rel, sizeof rel, "%s/saves/Supafaust/%s.srm", profiles[i], base);
		write_text(rel, "alttp");
	}
	make_dirs(RANDO_SD_LADX_GAME_CONFIG_DIR);
	snprintf(rel, sizeof rel, "%s/%s.cfg", RANDO_SD_LADX_GAME_CONFIG_DIR, base);
	write_text(rel, "c");
	snprintf(rel, sizeof rel, "%s/saves/Gambatte/other.srm", profiles[0]);
	write_text(rel, "other");
}

static int count_files(const char *rel)
{
	DIR *d = opendir(at(rel));
	struct dirent *e;
	int n = 0;

	if (d == NULL)
		return -1;
	while ((e = readdir(d)) != NULL)
		if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
			n++;
	closedir(d);
	return n;
}

static void test_delete_everything(void)
{
	UiHistoryDeleteOutcome o;
	RandoName name, other;
	char rel[512];
	size_t i;

	make_root();
	plant(1, 1, true, true, &name);
	plant(2, 2, true, false, &other);
	plant_seed_files(&name);
	make_dirs("Roms/SFC");
	write_text(RANDO_SD_LADX_LIST_CACHE6, "c");
	write_text(RANDO_SD_LADX_LIST_CACHE2, "c");
	write_text(RANDO_SD_LIST_CACHE6, "c");
	rando_ladx_history_delete(&history, &name, &o);
	CHECK(o.ok && !o.list_stale && o.status == UI_STR_HISTORY_DELETED);
	snprintf(rel, sizeof rel, "%s/%s", RANDO_SD_LADX_ROM_DIR, name.rom);
	CHECK(!exists(rel));
	CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 2); /* the other seed's ROM and .game_config */
	CHECK(count_files(RANDO_SD_LADX_GAME_CONFIG_DIR) == 0);
	CHECK(count_files(RANDO_SD_LADX_HISTORY_DIR) == 1);
	for (i = 0; i < 3; i++) {
		snprintf(rel, sizeof rel, "%s/saves/Gambatte", profiles[i]);
		CHECK(count_files(rel) == (i == 0 ? 1 : 0));
		snprintf(rel, sizeof rel, "%s/states/Gambatte", profiles[i]);
		CHECK(count_files(rel) == 0);
		snprintf(rel, sizeof rel, "%s/saves/Supafaust", profiles[i]);
		CHECK(count_files(rel) == 1);
	}
	CHECK(!exists(RANDO_SD_LADX_LIST_CACHE6) && !exists(RANDO_SD_LADX_LIST_CACHE2) && exists(RANDO_SD_LIST_CACHE6));
	rando_ladx_history_scan(&history, &list);
	CHECK(list.count == 1 && strcmp(list.entries[0].name.rom, other.rom) == 0);
	drop_root();
}

static void test_delete_failures(void)
{
	UiHistoryDeleteOutcome o;
	RandoName name;
	char rel[512];

	make_root();
	plant(1, 1, false, true, &name);
	snprintf(rel, sizeof rel, "%s/%s/x", RANDO_SD_LADX_ROM_DIR, name.rom);
	make_dirs(rel); /* the ROM path is a folder that cannot be removed */
	rando_ladx_history_delete(&history, &name, &o);
	CHECK(!o.ok && !o.list_stale && o.status == UI_STR_HISTORY_DELETE_ROM);
	snprintf(rel, sizeof rel, "%s/%s", RANDO_SD_LADX_ROM_DIR, name.rom);
	remove_all(at(rel));
	write_text(rel, "rom");

	snprintf(rel, sizeof rel, "%s/saves/Gambatte/%.*s.rtc/x", RANDO_SD_PROFILE_GUEST, (int)(strlen(name.rom) - 4),
		 name.rom);
	make_dirs(rel);
	rando_ladx_history_delete(&history, &name, &o);
	CHECK(!o.ok && o.status == UI_STR_HISTORY_DELETE_FILES);
	CHECK(count_files(RANDO_SD_LADX_HISTORY_DIR) == 2);
	snprintf(rel, sizeof rel, "%s/saves", RANDO_SD_PROFILE_GUEST);
	remove_all(at(rel));

	plant(1, 1, true, true, &name);
	snprintf(rel, sizeof rel, "%s/%s.spoiler.json", RANDO_SD_LADX_HISTORY_DIR, name.code);
	remove(at(rel));
	snprintf(rel, sizeof rel, "%s/%s.spoiler.json/x", RANDO_SD_LADX_HISTORY_DIR, name.code);
	make_dirs(rel);
	rando_ladx_history_delete(&history, &name, &o);
	CHECK(!o.ok && o.status == UI_STR_HISTORY_DELETE_RECORD);
	snprintf(rel, sizeof rel, "%s/%s.json", RANDO_SD_LADX_HISTORY_DIR, name.code);
	CHECK(exists(rel));
	drop_root();
}

int main(void)
{
	test_missing_and_unlistable_folder();
	test_newest_first_rom_present_only();
	test_truncated();
	test_spoiler_outline();
	test_delete_everything();
	test_delete_failures();
	printf("test_ladx_history: ok\n");
	return 0;
}
