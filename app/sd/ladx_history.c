#define _POSIX_C_SOURCE 200809L

#include "rando_ladx_history.h"
#include "rando_out.h"
#include "rando_seed_files.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool join(char *out, size_t cap, const char *root, const char *rel)
{
	int n = snprintf(out, cap, "%s/%s", root, rel);

	return n >= 0 && (size_t)n < cap;
}

bool rando_ladx_history_init(RandoLadxHistory *h, const char *sd_root)
{
	static const char *const profiles[RANDO_HISTORY_PROFILES] = {RANDO_SD_PROFILE_CURRENT, RANDO_SD_PROFILE_MAIN,
								     RANDO_SD_PROFILE_GUEST};
	int i;

	if (strlen(sd_root) >= sizeof h->root)
		return false;
	strcpy(h->root, sd_root);
	if (!join(h->rom_dir, sizeof h->rom_dir, sd_root, RANDO_SD_LADX_ROM_DIR) ||
	    !join(h->history_dir, sizeof h->history_dir, sd_root, RANDO_SD_LADX_HISTORY_DIR) ||
	    !join(h->game_config_dir, sizeof h->game_config_dir, sd_root, RANDO_SD_LADX_GAME_CONFIG_DIR))
		return false;
	for (i = 0; i < RANDO_HISTORY_PROFILES; i++)
		if (!join(h->profiles[i], sizeof h->profiles[i], sd_root, profiles[i]))
			return false;
	return true;
}

static bool read_record(RandoLadxHistory *h, const char *code)
{
	char path[RANDO_SEED_PATH_MAX], error[RANDO_RECORD_ERROR_MAX];
	size_t size;
	long total;

	snprintf(path, sizeof path, "%s/%s.json", h->history_dir, code);
	if (rando_out_read_capped(path, (uint8_t *)h->buf, RANDO_RECORD_TEXT_MAX, &size, &total) != RANDO_OUT_READ_OK)
		return false;
	return rando_ladx_record_parse(h->buf, size, &h->record, error) && strcmp(h->record.name.code, code) == 0;
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

static bool newer(const RandoLadxRecord *a, const RandoLadxRecord *b)
{
	const int x[6] = {a->date.year, a->date.month, a->date.day, a->date.hour, a->date.minute, a->date.second};
	const int y[6] = {b->date.year, b->date.month, b->date.day, b->date.hour, b->date.minute, b->date.second};
	int i;

	for (i = 0; i < 6; i++)
		if (x[i] != y[i])
			return x[i] > y[i];
	return strcmp(a->name.code, b->name.code) > 0;
}

static void insert(RandoLadxHistoryList *out, const RandoLadxRecord *r)
{
	int i;

	if (out->count == UI_HISTORY_MAX) {
		out->truncated = true;
		if (!newer(r, &out->entries[UI_HISTORY_MAX - 1]))
			return;
		out->count--;
	}
	for (i = out->count; i > 0 && newer(r, &out->entries[i - 1]); i--)
		out->entries[i] = out->entries[i - 1];
	out->entries[i] = *r;
	out->count++;
}

void rando_ladx_history_scan(RandoLadxHistory *h, RandoLadxHistoryList *out)
{
	DIR *d;
	struct dirent *ent;

	out->count = 0;
	out->unreadable = out->truncated = out->folder_unreadable = false;
	d = opendir(h->history_dir);
	if (d == NULL) {
		out->folder_unreadable = errno != ENOENT;
		return;
	}
	while ((ent = readdir(d)) != NULL) {
		char code[RANDO_NAME_CODE_MAX];

		if (!record_code(ent->d_name, code) || !regular_file(h->history_dir, ent->d_name))
			continue;
		if (!read_record(h, code)) {
			out->unreadable = true;
			continue;
		}
		if (regular_file(h->rom_dir, h->record.name.rom))
			insert(out, &h->record);
	}
	closedir(d);
}

bool rando_ladx_history_spoiler(RandoLadxHistory *h, const RandoName *name, UiHistorySpoiler *out)
{
	char path[RANDO_SEED_PATH_MAX];
	size_t size;
	long total;

	out->len = 0;
	out->cut = false;
	if (!rando_name_code_valid(name->code) || !read_record(h, name->code) ||
	    strcmp(h->record.name.rom, name->rom) != 0 || !h->record.has_spoiler)
		return false;
	snprintf(path, sizeof path, "%s/%s.spoiler.json", h->history_dir, name->code);
	if (rando_out_read_capped(path, (uint8_t *)h->buf, RANDO_LADX_SPOILER_MAX, &size, &total) != RANDO_OUT_READ_OK)
		return false;
	return rando_record_spoiler_outline(h->buf, 0, size, out->text, sizeof out->text, &out->len, &out->cut);
}

void rando_ladx_history_delete(RandoLadxHistory *h, const RandoName *name, UiHistoryDeleteOutcome *outcome)
{
	static const char *const save_exts[] = {".srm", ".rtc"};
	static const char *const record_exts[] = {".spoiler.json", ".json"};
	const char *profiles[RANDO_HISTORY_PROFILES];
	RandoSeedPlace place;
	RandoHistoryDeleteStatus status;
	int i;

	for (i = 0; i < RANDO_HISTORY_PROFILES; i++)
		profiles[i] = h->profiles[i];
	place.rom_dir = h->rom_dir;
	place.game_config_dir = h->game_config_dir;
	place.history_dir = h->history_dir;
	place.profiles = profiles;
	place.profile_count = RANDO_HISTORY_PROFILES;
	place.core = RANDO_SD_LADX_CORE_DIR;
	place.rom_ext = ".gbc";
	place.save_exts = save_exts;
	place.save_ext_count = 2;
	place.record_exts = record_exts;
	place.record_ext_count = 2;
	status = rando_seed_files_delete(&place, name);
	outcome->ok = status == RANDO_HISTORY_DELETE_OK;
	outcome->list_stale = status != RANDO_HISTORY_DELETE_ROM &&
			      rando_sd_remove_caches(h->root, RANDO_SD_LADX_LIST_CACHE6, RANDO_SD_LADX_LIST_CACHE2);
	outcome->status = rando_history_delete_text(status);
}
