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

static RandoHistoryDeleteStatus delete_files(RandoHistory *h, const UiHistoryEntry *entry)
{
	static const char *const save_exts[] = {".srm"};
	static const char *const record_exts[] = {".json"};
	const char *profiles[RANDO_HISTORY_PROFILES];
	RandoSeedPlace place;
	int i;

	for (i = 0; i < RANDO_HISTORY_PROFILES; i++)
		profiles[i] = h->profiles[i];
	place.rom_dir = h->rom_dir;
	place.game_config_dir = h->game_config_dir;
	place.history_dir = h->history_dir;
	place.profiles = profiles;
	place.profile_count = RANDO_HISTORY_PROFILES;
	place.core = RANDO_SD_CORE_DIR;
	place.rom_ext = ".sfc";
	place.save_exts = save_exts;
	place.save_ext_count = 1;
	place.record_exts = record_exts;
	place.record_ext_count = 1;
	return rando_seed_files_delete(&place, &entry->name);
}

void rando_history_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome)
{
	RandoHistory *h = ctx;
	RandoHistoryDeleteStatus status = delete_files(h, entry);

	outcome->ok = status == RANDO_HISTORY_DELETE_OK;
	outcome->list_stale = status != RANDO_HISTORY_DELETE_ROM && rando_sd_remove_list_caches(h->root);
	outcome->status = rando_history_delete_text(status);
}
