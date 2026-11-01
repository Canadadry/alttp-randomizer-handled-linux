#define _POSIX_C_SOURCE 200809L

#include "rando_history.h"
#include "rando_out.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef char spoiler_fits_the_outline[UI_HISTORY_SPOILER_MAX == RANDO_RECORD_TEXT_MAX ? 1 : -1];

static bool join(char *out, size_t cap, const char *root, const char *rel)
{
	int n = snprintf(out, cap, "%s/%s", root, rel);

	return n >= 0 && (size_t)n < cap;
}

bool rando_history_init(RandoHistory *h, const char *sd_root)
{
	static const char *const profiles[RANDO_HISTORY_PROFILES] = {RANDO_SD_PROFILE_CURRENT, RANDO_SD_PROFILE_MAIN,
								     RANDO_SD_PROFILE_GUEST};
	int i;

	assert(sd_root != NULL);
	if (strlen(sd_root) >= sizeof h->root)
		return false;
	strcpy(h->root, sd_root);
	if (!join(h->rom_dir, sizeof h->rom_dir, sd_root, RANDO_SD_ROM_DIR) ||
	    !join(h->history_dir, sizeof h->history_dir, sd_root, RANDO_SD_HISTORY_DIR) ||
	    !join(h->game_config_dir, sizeof h->game_config_dir, sd_root, RANDO_SD_GAME_CONFIG_DIR))
		return false;
	for (i = 0; i < RANDO_HISTORY_PROFILES; i++)
		if (!join(h->profiles[i], sizeof h->profiles[i], sd_root, profiles[i]))
			return false;
	return true;
}

/* Reads and strictly parses <history>/<code>.json into h->record; false on any failure or another code. */
static bool read_record(RandoHistory *h, const char *code)
{
	char path[RANDO_SEED_PATH_MAX], error[RANDO_RECORD_ERROR_MAX];
	size_t size;
	long total;

	snprintf(path, sizeof path, "%s/%s.json", h->history_dir, code);
	if (rando_out_read_capped(path, (uint8_t *)h->buf, RANDO_RECORD_TEXT_MAX, &size, &total) != RANDO_OUT_READ_OK)
		return false;
	return rando_record_parse(h->buf, size, &h->record, error) && strcmp(h->record.name.code, code) == 0;
}

static bool regular_file(const char *dir, const char *name)
{
	char path[RANDO_SEED_PATH_MAX];
	struct stat st;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* The code of a "<code>.json" name, or false. */
static bool record_code(const char *name, char code[RANDO_NAME_CODE_MAX])
{
	size_t n = strlen(name);

	if (n < 5 || n - 5 >= RANDO_NAME_CODE_MAX || strcmp(name + n - 5, ".json") != 0)
		return false;
	memcpy(code, name, n - 5);
	code[n - 5] = '\0';
	return rando_name_code_valid(code);
}

static int date_compare(const UiHistoryDate *a, const UiHistoryDate *b)
{
	const int x[6] = {a->year, a->month, a->day, a->hour, a->minute, a->second};
	const int y[6] = {b->year, b->month, b->day, b->hour, b->minute, b->second};
	int i;

	for (i = 0; i < 6; i++)
		if (x[i] != y[i])
			return x[i] < y[i] ? -1 : 1;
	return 0;
}

static bool newer(const UiHistoryEntry *a, const UiHistoryEntry *b)
{
	int c = date_compare(&a->date, &b->date);

	return c != 0 ? c > 0 : strcmp(a->name.code, b->name.code) > 0;
}

static void entry_of(const RandoRecord *r, UiHistoryEntry *e)
{
	memset(e, 0, sizeof *e);
	e->name = r->name;
	e->date.year = r->date.year;
	e->date.month = r->date.month;
	e->date.day = r->date.day;
	e->date.hour = r->date.hour;
	e->date.minute = r->date.minute;
	e->date.second = r->date.second;
	e->seed = r->seed;
	e->options = r->options;
	e->heart_beep = r->heart_beep;
	e->has_spoiler = r->has_spoiler;
}

/* Keeps out sorted newest first and at most UI_HISTORY_MAX long, setting truncated when one is dropped. */
static void insert(UiHistoryList *out, const UiHistoryEntry *e)
{
	int i;

	if (out->count == UI_HISTORY_MAX) {
		out->truncated = true;
		if (!newer(e, &out->entries[UI_HISTORY_MAX - 1]))
			return;
		out->count--;
	}
	for (i = out->count; i > 0 && newer(e, &out->entries[i - 1]); i--)
		out->entries[i] = out->entries[i - 1];
	out->entries[i] = *e;
	out->count++;
}

void rando_history_scan(RandoHistory *h, UiHistoryList *out)
{
	DIR *d;
	struct dirent *ent;

	out->count = 0;
	out->unreadable = false;
	out->truncated = false;
	out->folder_unreadable = false;
	d = opendir(h->history_dir);
	if (d == NULL) {
		out->folder_unreadable = errno != ENOENT;
		return;
	}
	while ((ent = readdir(d)) != NULL) {
		char code[RANDO_NAME_CODE_MAX];
		UiHistoryEntry e;

		if (!record_code(ent->d_name, code) || !regular_file(h->history_dir, ent->d_name))
			continue;
		if (!read_record(h, code)) {
			out->unreadable = true;
			continue;
		}
		if (!regular_file(h->rom_dir, h->record.name.rom))
			continue;
		entry_of(&h->record, &e);
		insert(out, &e);
	}
	closedir(d);
}

bool rando_history_spoiler(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out)
{
	RandoHistory *h = ctx;

	out->len = 0;
	out->cut = false;
	if (!rando_name_code_valid(entry->name.code) || !read_record(h, entry->name.code) ||
	    strcmp(h->record.name.rom, entry->name.rom) != 0 || !h->record.has_spoiler)
		return false;
	return rando_record_spoiler_outline(h->buf, h->record.spoiler_offset, h->record.spoiler_size, out->text,
					    sizeof out->text, &out->len, &out->cut);
}

static const UiStrId delete_texts[RANDO_HISTORY_DELETE_STATUS_COUNT] = {
	[RANDO_HISTORY_DELETE_OK] = UI_STR_HISTORY_DELETED,
	[RANDO_HISTORY_DELETE_ROM] = UI_STR_HISTORY_DELETE_ROM,
	[RANDO_HISTORY_DELETE_FILES] = UI_STR_HISTORY_DELETE_FILES,
	[RANDO_HISTORY_DELETE_RECORD] = UI_STR_HISTORY_DELETE_RECORD,
};

UiStrId rando_history_delete_text(RandoHistoryDeleteStatus status)
{
	assert((size_t)status < RANDO_HISTORY_DELETE_STATUS_COUNT);
	return delete_texts[status];
}

/* Room for a profile folder, "/states/<core>/" and any file name. */
#define DELETE_PATH_MAX (RANDO_SEED_PATH_MAX + 512)

static bool present(const char *path)
{
	struct stat st;

	return lstat(path, &st) == 0;
}

/* True when the file at <dir>/<name><ext> is absent afterwards, whatever it was. */
static bool gone(const char *dir, const char *name, const char *ext)
{
	char path[DELETE_PATH_MAX];

	snprintf(path, sizeof path, "%s/%s%s", dir, name, ext);
	remove(path);
	return !present(path);
}

/* The first regular file of dir starting with prefix, into path; false when none (or dir cannot be listed). */
static bool find_state(const char *dir, const char *prefix, char path[DELETE_PATH_MAX])
{
	size_t n = strlen(prefix);
	DIR *d = opendir(dir);
	struct dirent *ent;
	bool found = false;

	if (d == NULL)
		return false;
	while (!found && (ent = readdir(d)) != NULL) {
		struct stat st;
		int len;

		if (strncmp(ent->d_name, prefix, n) != 0)
			continue;
		len = snprintf(path, DELETE_PATH_MAX, "%s/%s", dir, ent->d_name);
		if (len < 0 || len >= DELETE_PATH_MAX)
			continue;
		found = lstat(path, &st) == 0 && S_ISREG(st.st_mode);
	}
	closedir(d);
	return found;
}

/* The .srm and the states of one profile folder; false when one stays. No removal runs while dir is open. */
static bool delete_profile(const char *profile, const char *base)
{
	char dir[DELETE_PATH_MAX], prefix[RANDO_NAME_ROM_MAX + 8], path[DELETE_PATH_MAX];
	struct stat st;
	bool ok;

	if (stat(profile, &st) != 0 || !S_ISDIR(st.st_mode))
		return true;
	snprintf(dir, sizeof dir, "%s/" RANDO_SD_SAVES_DIR "/" RANDO_SD_CORE_DIR, profile);
	ok = gone(dir, base, ".srm");
	snprintf(dir, sizeof dir, "%s/" RANDO_SD_STATES_DIR "/" RANDO_SD_CORE_DIR, profile);
	snprintf(prefix, sizeof prefix, "%s.state", base);
	while (find_state(dir, prefix, path)) {
		remove(path);
		if (present(path))
			return false;
	}
	return ok;
}

static RandoHistoryDeleteStatus delete_files(RandoHistory *h, const UiHistoryEntry *entry)
{
	char base[RANDO_NAME_ROM_MAX];
	size_t n = strlen(entry->name.rom);
	bool ok;
	int i;

	assert(rando_name_code_valid(entry->name.code));
	assert(n > 4 && n < sizeof base && strcmp(entry->name.rom + n - 4, ".sfc") == 0);
	memcpy(base, entry->name.rom, n - 4);
	base[n - 4] = '\0';
	if (!gone(h->rom_dir, entry->name.rom, ""))
		return RANDO_HISTORY_DELETE_ROM;
	ok = gone(h->game_config_dir, base, ".cfg");
	for (i = 0; i < RANDO_HISTORY_PROFILES; i++)
		if (!delete_profile(h->profiles[i], base))
			ok = false;
	if (!ok)
		return RANDO_HISTORY_DELETE_FILES;
	return gone(h->history_dir, entry->name.code, ".json") ? RANDO_HISTORY_DELETE_OK : RANDO_HISTORY_DELETE_RECORD;
}

void rando_history_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome)
{
	RandoHistory *h = ctx;
	RandoHistoryDeleteStatus status = delete_files(h, entry);

	outcome->ok = status == RANDO_HISTORY_DELETE_OK;
	outcome->list_stale = status != RANDO_HISTORY_DELETE_ROM && rando_sd_remove_list_caches(h->root);
	outcome->status = rando_history_delete_text(status);
}
