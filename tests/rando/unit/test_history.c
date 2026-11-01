#define _POSIX_C_SOURCE 200809L

#include "rando_history.h"
#include "rando_record.h"
#include "rando_seed_save.h"
#include "support.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_history"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static RandoHistory history;
static UiHistoryList list;
static UiHistorySpoiler spoiler;
static char root[64];
static char text[RANDO_RECORD_TEXT_MAX + 1], golden[RANDO_RECORD_TEXT_MAX + 1];
static char outline[RANDO_RECORD_TEXT_MAX];

static const char *at(const char *rel)
{
	static char path[512];

	snprintf(path, sizeof path, "%s/%s", root, rel);
	return path;
}

static bool exists(const char *rel)
{
	struct stat st;

	return stat(at(rel), &st) == 0;
}

static void make_dir(const char *rel)
{
	if (mkdir(at(rel), 0700) != 0)
		test_fail(TEST, "cannot create %s", at(rel));
}

static void write_text(const char *rel, const char *s)
{
	FILE *f = fopen(at(rel), "wb");

	if (f == NULL || fwrite(s, 1, strlen(s), f) != strlen(s) || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", at(rel));
}

/* The file's bytes, NUL-terminated, into buf[RANDO_RECORD_TEXT_MAX + 1]. */
static void read_text(const char *path, char *buf)
{
	FILE *f = fopen(path, "rb");
	size_t n;

	if (f == NULL)
		test_fail(TEST, "cannot open %s", path);
	n = fread(buf, 1, RANDO_RECORD_TEXT_MAX, f);
	if (ferror(f) || n == RANDO_RECORD_TEXT_MAX)
		test_fail(TEST, "cannot read %s", path);
	buf[n] = '\0';
	fclose(f);
}

static void remove_all(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *e;
	char child[512];

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

/* A fresh SD root with the ROM folder and, when with_history, the history folder. */
static void make_root(bool with_history)
{
	strcpy(root, "/tmp/test_history_XXXXXX");
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	make_dir("Roms");
	make_dir("Roms/SFC");
	make_dir(RANDO_SD_ROM_DIR);
	make_dir("App");
	make_dir("App/ALttPR");
	if (with_history)
		make_dir(RANDO_SD_HISTORY_DIR);
	CHECK(rando_history_init(&history, root));
}

static void drop_root(void)
{
	if (root[0] != '\0')
		remove_all(root);
	root[0] = '\0';
}

/* Baseline settings, the given seed and date, suffix 1. */
static RandoRecord record_of(uint32_t seed, int month, int day, int hour, int minute, int second)
{
	RandoRecord r;

	memset(&r, 0, sizeof r);
	rando_options_init(&r.options);
	r.heart_beep = RANDO_HEART_BEEP_HALF;
	r.seed = seed;
	rando_name_make(&r.options, rando_name_code(&r.options, r.heart_beep, seed), 1, &r.name);
	r.date.year = 2026;
	r.date.month = month;
	r.date.day = day;
	r.date.hour = hour;
	r.date.minute = minute;
	r.date.second = second;
	return r;
}

static const char *record_rel(const char *code)
{
	static char rel[128];

	snprintf(rel, sizeof rel, RANDO_SD_HISTORY_DIR "/%s.json", code);
	return rel;
}

static const char *rom_rel(const char *rom)
{
	static char rel[128];

	snprintf(rel, sizeof rel, RANDO_SD_ROM_DIR "/%s", rom);
	return rel;
}

/* cases/baseline/spoiler.json, its trailing newline included. */
static void read_golden(void)
{
	read_text("cases/baseline/spoiler.json", golden);
}

/* The record's file, holding the baseline golden spoiler when with_spoiler, and its ROM when with_rom. */
static void write_record(const RandoRecord *r, bool with_spoiler, bool with_rom)
{
	static const char settings_end[] = "    }\n}\n";
	FILE *f = tmpfile();
	char *end;
	size_t n;

	CHECK(!exists(record_rel(r->name.code)));
	CHECK(f != NULL);
	CHECK(rando_record_write(f, r, NULL, NULL));
	rewind(f);
	n = fread(text, 1, RANDO_RECORD_TEXT_MAX, f);
	fclose(f);
	text[n] = '\0';
	if (with_spoiler) {
		read_golden();
		end = strstr(text, settings_end);
		CHECK(end != NULL);
		snprintf(end, sizeof text - (size_t)(end - text), "    },\n    \"spoiler\": %s}\n", golden);
	}
	write_text(record_rel(r->name.code), text);
	if (with_rom)
		write_text(rom_rel(r->name.rom), "rom");
}

static void check_entry(const UiHistoryEntry *e, const RandoRecord *r, bool has_spoiler)
{
	CHECK(strcmp(e->name.code, r->name.code) == 0);
	CHECK(strcmp(e->name.rom, r->name.rom) == 0);
	CHECK(e->seed == r->seed);
	CHECK(e->date.year == r->date.year && e->date.month == r->date.month && e->date.day == r->date.day);
	CHECK(e->date.hour == r->date.hour && e->date.minute == r->date.minute && e->date.second == r->date.second);
	CHECK(memcmp(&e->options, &r->options, sizeof e->options) == 0);
	CHECK(e->heart_beep == r->heart_beep);
	CHECK(e->has_spoiler == has_spoiler);
}

static void check_no_notice(void)
{
	CHECK(!list.unreadable && !list.truncated && !list.folder_unreadable);
}

static void test_init(void)
{
	char long_root[RANDO_SEED_DIR_MAX + 8];

	memset(long_root, 'a', sizeof long_root - 1);
	long_root[sizeof long_root - 1] = '\0';
	CHECK(!rando_history_init(&history, long_root));
	long_root[RANDO_SEED_DIR_MAX - 1 - strlen("/" RANDO_SD_GAME_CONFIG_DIR)] = '\0';
	CHECK(rando_history_init(&history, long_root));
	long_root[strlen(long_root) + 1] = '\0';
	long_root[strlen(long_root)] = 'a';
	CHECK(!rando_history_init(&history, long_root));
}

static void test_missing_folder(void)
{
	make_root(false);
	memset(&list, 0xA5, sizeof list);
	rando_history_scan(&history, &list);
	CHECK(list.count == 0);
	check_no_notice();
	CHECK(!exists(RANDO_SD_HISTORY_DIR));
	drop_root();
}

static void test_unlistable_folder(void)
{
	make_root(false);
	write_text(RANDO_SD_HISTORY_DIR, "not a folder");
	rando_history_scan(&history, &list);
	CHECK(list.count == 0);
	CHECK(list.folder_unreadable && !list.unreadable && !list.truncated);
	drop_root();
}

static void test_newest_first(void)
{
	RandoRecord a = record_of(1, 11, 1, 10, 0, 0), b = record_of(2, 11, 2, 9, 0, 0);
	RandoRecord c = record_of(3, 10, 1, 12, 0, 0), d = record_of(4, 10, 1, 12, 0, 0);
	const RandoRecord *first = strcmp(c.name.code, d.name.code) > 0 ? &c : &d;
	const RandoRecord *second = first == &c ? &d : &c;

	make_root(true);
	write_record(&a, false, true);
	write_record(&c, false, true);
	write_record(&b, true, true);
	write_record(&d, false, true);
	rando_history_scan(&history, &list);
	check_no_notice();
	CHECK(list.count == 4);
	check_entry(&list.entries[0], &b, true);
	check_entry(&list.entries[1], &a, false);
	check_entry(&list.entries[2], first, false);
	check_entry(&list.entries[3], second, false);
	drop_root();
}

static void test_hidden_records(void)
{
	RandoRecord shown = record_of(1, 11, 1, 10, 0, 0), absent = record_of(2, 11, 2, 9, 0, 0);
	RandoRecord folder = record_of(3, 11, 3, 9, 0, 0);

	make_root(true);
	write_record(&shown, false, true);
	write_record(&absent, false, false);
	write_record(&folder, false, false);
	make_dir(rom_rel(folder.name.rom));
	rando_history_scan(&history, &list);
	check_no_notice();
	CHECK(list.count == 1);
	check_entry(&list.entries[0], &shown, false);
	CHECK(exists(record_rel(absent.name.code)));
	CHECK(exists(record_rel(folder.name.code)));
	write_text(rom_rel(absent.name.rom), "back");
	rando_history_scan(&history, &list);
	CHECK(list.count == 2);
	check_entry(&list.entries[0], &absent, false);
	drop_root();
}

static void check_kept(const char *rel, const char *content)
{
	static char got[RANDO_RECORD_TEXT_MAX + 1];

	read_text(at(rel), got);
	if (strcmp(got, content) != 0)
		test_fail(TEST, "%s was changed", rel);
}

static void test_unreadable_records(void)
{
	static char mismatch[RANDO_RECORD_TEXT_MAX + 1], other_code[RANDO_RECORD_TEXT_MAX + 1];
	RandoRecord good = record_of(1, 11, 1, 10, 0, 0), bad_rom = record_of(2, 11, 2, 9, 0, 0);
	RandoRecord moved = record_of(3, 11, 3, 9, 0, 0);
	char *rom, from[512];

	make_root(true);
	write_record(&good, false, true);
	write_text(RANDO_SD_HISTORY_DIR "/1A2B3C.json", "{ garbage");
	write_record(&bad_rom, false, true);
	read_text(at(record_rel(bad_rom.name.code)), mismatch);
	rom = strstr(mismatch, "Open-Ganon");
	CHECK(rom != NULL);
	memcpy(rom, "Open-Hunts", 10);
	write_text(record_rel(bad_rom.name.code), mismatch);
	write_record(&moved, false, true);
	read_text(at(record_rel(moved.name.code)), other_code);
	snprintf(from, sizeof from, "%s", at(record_rel(moved.name.code)));
	CHECK(rename(from, at(RANDO_SD_HISTORY_DIR "/ABCDEF.json")) == 0);
	CHECK(!exists(record_rel(moved.name.code)));
	rando_history_scan(&history, &list);
	CHECK(list.unreadable && !list.truncated && !list.folder_unreadable);
	CHECK(list.count == 1);
	check_entry(&list.entries[0], &good, false);
	check_kept(RANDO_SD_HISTORY_DIR "/1A2B3C.json", "{ garbage");
	check_kept(record_rel(bad_rom.name.code), mismatch);
	check_kept(RANDO_SD_HISTORY_DIR "/ABCDEF.json", other_code);
	CHECK(exists(rom_rel(bad_rom.name.rom)) && exists(rom_rel(moved.name.rom)));
	drop_root();
}

static void test_ignored_names(void)
{
	RandoRecord good = record_of(1, 11, 1, 10, 0, 0);

	make_root(true);
	write_record(&good, false, true);
	write_text(RANDO_SD_HISTORY_DIR "/._1CA10A.json", "\x00\x05\x16\x07");
	write_text(RANDO_SD_HISTORY_DIR "/notes.txt", "notes");
	write_text(RANDO_SD_HISTORY_DIR "/1CA10A.json.tmp", "{");
	write_text(RANDO_SD_HISTORY_DIR "/.json", "{");
	make_dir(RANDO_SD_HISTORY_DIR "/sub");
	make_dir(RANDO_SD_HISTORY_DIR "/ABCDEF.json");
	rando_history_scan(&history, &list);
	check_no_notice();
	CHECK(list.count == 1);
	check_entry(&list.entries[0], &good, false);
	CHECK(exists(RANDO_SD_HISTORY_DIR "/notes.txt") && exists(RANDO_SD_HISTORY_DIR "/1CA10A.json.tmp"));
	drop_root();
}

static void test_truncated(void)
{
	static RandoRecord records[UI_HISTORY_MAX + 1];
	int i;

	make_root(true);
	for (i = 0; i <= UI_HISTORY_MAX; i++) {
		records[i] = record_of((uint32_t)(1000 + i), 11, 1, 10, i / 60, i % 60);
		write_record(&records[i], false, true);
	}
	rando_history_scan(&history, &list);
	CHECK(list.truncated && !list.unreadable && !list.folder_unreadable);
	CHECK(list.count == UI_HISTORY_MAX);
	for (i = 0; i < UI_HISTORY_MAX; i++)
		check_entry(&list.entries[i], &records[UI_HISTORY_MAX - i], false);
	remove(at(record_rel(records[UI_HISTORY_MAX].name.code)));
	rando_history_scan(&history, &list);
	check_no_notice();
	CHECK(list.count == UI_HISTORY_MAX);
	check_entry(&list.entries[UI_HISTORY_MAX - 1], &records[0], false);
	drop_root();
}

static void test_spoiler(void)
{
	RandoRecord with = record_of(1, 11, 2, 10, 0, 0), without = record_of(2, 11, 1, 10, 0, 0);
	UiHistoryEntry entry;
	size_t len;
	bool cut;

	make_root(true);
	write_record(&with, true, true);
	write_record(&without, false, true);
	rando_history_scan(&history, &list);
	CHECK(list.count == 2 && list.entries[0].has_spoiler && !list.entries[1].has_spoiler);
	CHECK(rando_history_spoiler(&history, &list.entries[0], &spoiler));
	read_golden();
	CHECK(rando_record_spoiler_outline(golden, 0, strlen(golden) - 1, outline, sizeof outline, &len, &cut));
	CHECK(!cut && !spoiler.cut);
	CHECK(spoiler.len == len && memcmp(spoiler.text, outline, len) == 0);
	CHECK(!rando_history_spoiler(&history, &list.entries[1], &spoiler));

	entry = list.entries[0];
	snprintf(entry.name.rom, sizeof entry.name.rom, "%s", without.name.rom);
	CHECK(!rando_history_spoiler(&history, &entry, &spoiler));
	read_text(at(record_rel(with.name.code)), text);
	text[strlen(text) - 3] = '\0';
	write_text(record_rel(with.name.code), text);
	CHECK(!rando_history_spoiler(&history, &list.entries[0], &spoiler));
	remove(at(record_rel(with.name.code)));
	CHECK(!rando_history_spoiler(&history, &list.entries[0], &spoiler));
	drop_root();
}

static UiHistoryEntry entry_from(const RandoRecord *r)
{
	UiHistoryEntry e;

	memset(&e, 0, sizeof e);
	e.name = r->name;
	e.seed = r->seed;
	e.options = r->options;
	e.heart_beep = r->heart_beep;
	return e;
}

static UiHistoryDeleteOutcome delete_seed(const RandoRecord *r)
{
	UiHistoryEntry e = entry_from(r);
	UiHistoryDeleteOutcome o;

	o.ok = true;
	o.list_stale = true;
	o.status = UI_STR_COUNT;
	rando_history_delete(&history, &e, &o);
	return o;
}

#define BASE "Open-Ganon-1CA10A"
#define SIBLING "Open-Ganon-1CA10A-2"

static const char *const profiles[] = {RANDO_SD_PROFILE_CURRENT, RANDO_SD_PROFILE_MAIN, RANDO_SD_PROFILE_GUEST};

static const char *const state_names[] = {".state",  ".state1",	    ".state10",	      ".state27",
					  ".state.auto", ".state3.png", ".state.auto.png"};

static void make_dirs(const char *rel)
{
	char path[256], *slash;

	snprintf(path, sizeof path, "%s", rel);
	for (slash = strchr(path, '/'); slash != NULL; slash = strchr(slash + 1, '/')) {
		*slash = '\0';
		if (!exists(path))
			make_dir(path);
		*slash = '/';
	}
	if (!exists(path))
		make_dir(path);
}

/* Writes rel, creating its folders. */
static void put(const char *rel)
{
	char dir[256], *slash;

	snprintf(dir, sizeof dir, "%s", rel);
	slash = strrchr(dir, '/');
	CHECK(slash != NULL);
	*slash = '\0';
	make_dirs(dir);
	write_text(rel, "x");
}

typedef struct Paths {
	char rel[64][128];
	int count;
} Paths;

static Paths removed, kept;

static void add(Paths *paths, const char *fmt, const char *a, const char *b)
{
	CHECK(paths->count < 64);
	snprintf(paths->rel[paths->count++], sizeof paths->rel[0], fmt, a, b);
}

/* The seed's files in the first profile_count profiles, the look-alikes and stand-ins Onion keeps;
 * removed and kept list them. */
static void full_tree(const RandoRecord *seed, const RandoRecord *sibling, const RandoRecord *other, int profile_count)
{
	int p, i;

	removed.count = kept.count = 0;
	add(&removed, "%s/%s", RANDO_SD_ROM_DIR, seed->name.rom);
	add(&removed, "%s/%s.cfg", RANDO_SD_GAME_CONFIG_DIR, BASE);
	for (p = 0; p < profile_count; p++) {
		add(&removed, "%s/saves/Supafaust/%s.srm", profiles[p], BASE);
		for (i = 0; i < (int)(sizeof state_names / sizeof state_names[0]); i++) {
			char rel[128];

			snprintf(rel, sizeof rel, "%s/states/Supafaust/" BASE "%s", profiles[p], state_names[i]);
			add(&removed, "%s%s", rel, "");
		}
		add(&kept, "%s/saves/Supafaust/%s.srm", profiles[p], SIBLING);
		add(&kept, "%s/states/Supafaust/%s.state", profiles[p], SIBLING);
		add(&kept, "%s/saves/Supafaust/%s.srm.bak", profiles[p], BASE);
		add(&kept, "%s/states/Supafaust/%s.state9/inside", profiles[p], BASE);
		add(&kept, "%s/states/Supafaust/%s.stat", profiles[p], BASE);
		add(&kept, "%s/states/Supafaust/x%s.state", profiles[p], BASE);
		add(&kept, "%s/saves/Snes9x/%s.srm", profiles[p], BASE);
		add(&kept, "%s/states/Snes9x/%s.state", profiles[p], BASE);
		add(&kept, "%s/content_history.lpl%s", profiles[p], "");
	}
	add(&kept, "%s/%s", RANDO_SD_ROM_DIR, sibling->name.rom);
	add(&kept, "%s/%s.cfg", RANDO_SD_GAME_CONFIG_DIR, SIBLING);
	add(&kept, "%s/%s", RANDO_SD_ROM_DIR, other->name.rom);
	add(&kept, "Roms/SFC/%s%s", seed->name.rom, "");
	add(&kept, "Roms/SFC/Hacks/%s%s", seed->name.rom, "");
	add(&kept, "%s%s", RANDO_SD_SETTINGS_FILE, "");
	add(&kept, "Roms/recentlist.json%s%s", "", "");
	for (i = 0; i < removed.count; i++)
		put(removed.rel[i]);
	for (i = 0; i < kept.count; i++)
		put(kept.rel[i]);
	write_record(seed, false, false);
	write_record(sibling, false, false);
	write_record(other, false, false);
	add(&kept, "%s%s", record_rel(sibling->name.code), "");
	add(&kept, "%s%s", record_rel(other->name.code), "");
	write_text(RANDO_SD_LIST_CACHE6, "x");
	write_text(RANDO_SD_LIST_CACHE2, "x");
}

static void check_paths(const Paths *paths, bool present)
{
	int i;

	for (i = 0; i < paths->count; i++)
		if (exists(paths->rel[i]) != present)
			test_fail(TEST, "%s %s", paths->rel[i], present ? "was removed" : "is still there");
}

static RandoRecord seed_record, sibling_record, other_record;

static void three_records(void)
{
	seed_record = record_of(1, 11, 1, 10, 0, 0);
	CHECK(strcmp(seed_record.name.rom, BASE ".sfc") == 0);
	sibling_record = seed_record;
	rando_name_make(&sibling_record.options, rando_name_code(&sibling_record.options, sibling_record.heart_beep, 1),
			2, &sibling_record.name);
	CHECK(strcmp(sibling_record.name.rom, SIBLING ".sfc") == 0);
	other_record = record_of(2, 11, 1, 10, 0, 0);
}

static void check_outcome(UiHistoryDeleteOutcome o, bool ok, bool list_stale, RandoHistoryDeleteStatus status)
{
	CHECK(o.ok == ok);
	CHECK(o.list_stale == list_stale);
	CHECK(o.status == rando_history_delete_text(status));
}

static void test_delete_full_tree(void)
{
	int profile_count;

	three_records();
	for (profile_count = 3; profile_count >= 0; profile_count -= 2) {
		make_root(true);
		full_tree(&seed_record, &sibling_record, &other_record, profile_count);
		check_outcome(delete_seed(&seed_record), true, false, RANDO_HISTORY_DELETE_OK);
		check_paths(&removed, false);
		check_paths(&kept, true);
		CHECK(!exists(record_rel(seed_record.name.code)));
		CHECK(!exists(RANDO_SD_LIST_CACHE6) && !exists(RANDO_SD_LIST_CACHE2));
		rando_history_scan(&history, &list);
		CHECK(list.count == 2);
		drop_root();
	}
}

/* Makes rel a non-empty folder, so remove cannot take it. */
static void block(const char *rel)
{
	char inside[256];

	if (exists(rel))
		CHECK(remove(at(rel)) == 0);
	snprintf(inside, sizeof inside, "%s/inside", rel);
	put(inside);
}

static void unblock(const char *rel)
{
	remove_all(at(rel));
}

static void test_delete_failures(void)
{
	char srm[128], rom_path[128];

	three_records();
	snprintf(rom_path, sizeof rom_path, "%s", rom_rel(seed_record.name.rom));

	make_root(true);
	full_tree(&seed_record, &sibling_record, &other_record, 3);
	block(rom_path);
	check_outcome(delete_seed(&seed_record), false, false, RANDO_HISTORY_DELETE_ROM);
	check_paths(&kept, true);
	CHECK(exists(record_rel(seed_record.name.code)));
	CHECK(exists(RANDO_SD_LIST_CACHE6) && exists(RANDO_SD_LIST_CACHE2));
	CHECK(exists(RANDO_SD_GAME_CONFIG_DIR "/" BASE ".cfg"));
	CHECK(exists(RANDO_SD_PROFILE_GUEST "/states/Supafaust/" BASE ".state.auto.png"));
	unblock(rom_path);
	check_outcome(delete_seed(&seed_record), true, false, RANDO_HISTORY_DELETE_OK);
	drop_root();

	make_root(true);
	full_tree(&seed_record, &sibling_record, &other_record, 3);
	snprintf(srm, sizeof srm, RANDO_SD_PROFILE_MAIN "/saves/Supafaust/%s.srm", BASE);
	block(srm);
	check_outcome(delete_seed(&seed_record), false, false, RANDO_HISTORY_DELETE_FILES);
	CHECK(!exists(rom_path));
	CHECK(!exists(RANDO_SD_GAME_CONFIG_DIR "/" BASE ".cfg"));
	CHECK(!exists(RANDO_SD_PROFILE_MAIN "/states/Supafaust/" BASE ".state10"));
	CHECK(!exists(RANDO_SD_PROFILE_GUEST "/saves/Supafaust/" BASE ".srm"));
	CHECK(exists(record_rel(seed_record.name.code)));
	CHECK(!exists(RANDO_SD_LIST_CACHE6) && !exists(RANDO_SD_LIST_CACHE2));
	check_paths(&kept, true);
	unblock(srm);
	write_text(RANDO_SD_LIST_CACHE6, "x");
	check_outcome(delete_seed(&seed_record), true, false, RANDO_HISTORY_DELETE_OK);
	check_paths(&removed, false);
	CHECK(!exists(RANDO_SD_LIST_CACHE6));
	drop_root();

	make_root(true);
	full_tree(&seed_record, &sibling_record, &other_record, 3);
	block(RANDO_SD_GAME_CONFIG_DIR "/" BASE ".cfg");
	check_outcome(delete_seed(&seed_record), false, false, RANDO_HISTORY_DELETE_FILES);
	CHECK(!exists(RANDO_SD_PROFILE_CURRENT "/saves/Supafaust/" BASE ".srm"));
	CHECK(exists(record_rel(seed_record.name.code)));
	drop_root();

	make_root(true);
	full_tree(&seed_record, &sibling_record, &other_record, 3);
	block(record_rel(seed_record.name.code));
	check_outcome(delete_seed(&seed_record), false, false, RANDO_HISTORY_DELETE_RECORD);
	check_paths(&removed, false);
	check_paths(&kept, true);
	CHECK(!exists(RANDO_SD_LIST_CACHE6) && !exists(RANDO_SD_LIST_CACHE2));
	unblock(record_rel(seed_record.name.code));
	check_outcome(delete_seed(&seed_record), true, false, RANDO_HISTORY_DELETE_OK);
	drop_root();

	make_root(true);
	full_tree(&seed_record, &sibling_record, &other_record, 3);
	block(RANDO_SD_LIST_CACHE2);
	check_outcome(delete_seed(&seed_record), true, true, RANDO_HISTORY_DELETE_OK);
	check_paths(&removed, false);
	CHECK(!exists(RANDO_SD_LIST_CACHE6) && !exists(record_rel(seed_record.name.code)));
	drop_root();
}

static void test_delete_table(void)
{
	int s, t;

	for (s = 0; s < RANDO_HISTORY_DELETE_STATUS_COUNT; s++) {
		UiStrId id = rando_history_delete_text((RandoHistoryDeleteStatus)s);

		CHECK(id != UI_STR_NONE && (int)id > 0 && (int)id < UI_STR_COUNT);
		CHECK(ui_str_arity(id) == 0);
		for (t = 0; t < s; t++)
			CHECK(rando_history_delete_text((RandoHistoryDeleteStatus)t) != id);
	}
	CHECK(rando_history_delete_text(RANDO_HISTORY_DELETE_OK) == UI_STR_HISTORY_DELETED);
	CHECK(rando_history_delete_text(RANDO_HISTORY_DELETE_FILES) == UI_STR_HISTORY_DELETE_FILES);
}

int main(void)
{
	atexit(drop_root);
	test_init();
	test_missing_folder();
	test_unlistable_folder();
	test_newest_first();
	test_hidden_records();
	test_unreadable_records();
	test_ignored_names();
	test_truncated();
	test_spoiler();
	test_delete_full_tree();
	test_delete_failures();
	test_delete_table();
	printf(TEST ": passed\n");
	return 0;
}
