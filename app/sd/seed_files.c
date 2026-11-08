#define _POSIX_C_SOURCE 200809L

#include "rando_seed_files.h"
#include "rando_seed_save.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* 512: "/states/<core>/" plus any file name (NAME_MAX 255); a shorter one skips long state files. */
#define DELETE_PATH_MAX (RANDO_SEED_PATH_MAX + 512)

static bool present(const char *path)
{
	struct stat st;

	return lstat(path, &st) == 0;
}

static bool gone(const char *dir, const char *name, const char *ext)
{
	char path[DELETE_PATH_MAX];

	snprintf(path, sizeof path, "%s/%s%s", dir, name, ext);
	remove(path);
	return !present(path);
}

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

static bool delete_profile(const RandoSeedPlace *place, const char *profile, const char *base)
{
	char dir[DELETE_PATH_MAX], prefix[RANDO_NAME_ROM_MAX + 8], path[DELETE_PATH_MAX];
	struct stat st;
	bool ok = true;
	int i;

	if (stat(profile, &st) != 0 || !S_ISDIR(st.st_mode))
		return true;
	snprintf(dir, sizeof dir, "%s/" RANDO_SD_SAVES_DIR "/%s", profile, place->core);
	for (i = 0; i < place->save_ext_count; i++)
		if (!gone(dir, base, place->save_exts[i]))
			ok = false;
	snprintf(dir, sizeof dir, "%s/" RANDO_SD_STATES_DIR "/%s", profile, place->core);
	snprintf(prefix, sizeof prefix, "%s.state", base);
	/* One rescan per removal: removing while dir is open leaves readdir's result unspecified. */
	while (find_state(dir, prefix, path)) {
		remove(path);
		if (present(path))
			return false;
	}
	return ok;
}

RandoHistoryDeleteStatus rando_seed_files_delete(const RandoSeedPlace *place, const RandoName *name)
{
	char base[RANDO_NAME_ROM_MAX];
	size_t n = strlen(name->rom), ext = strlen(place->rom_ext);
	bool ok;
	int i;

	assert(rando_name_code_valid(name->code));
	assert(n > ext && n < sizeof base && strcmp(name->rom + n - ext, place->rom_ext) == 0);
	memcpy(base, name->rom, n - ext);
	base[n - ext] = '\0';
	if (!gone(place->rom_dir, name->rom, ""))
		return RANDO_HISTORY_DELETE_ROM;
	ok = gone(place->game_config_dir, base, ".cfg");
	for (i = 0; i < place->profile_count; i++)
		if (!delete_profile(place, place->profiles[i], base))
			ok = false;
	if (!ok)
		return RANDO_HISTORY_DELETE_FILES;
	for (i = 0; i < place->record_ext_count; i++)
		if (!gone(place->history_dir, name->code, place->record_exts[i]))
			return RANDO_HISTORY_DELETE_RECORD;
	return RANDO_HISTORY_DELETE_OK;
}

static bool remove_cache(const char *sd_root, const char *rel)
{
	char path[DELETE_PATH_MAX];
	int n = snprintf(path, sizeof path, "%s/%s", sd_root, rel);

	assert(n >= 0 && (size_t)n < sizeof path);
	remove(path);
	return present(path);
}

bool rando_sd_remove_caches(const char *sd_root, const char *cache6, const char *cache2)
{
	bool stale = remove_cache(sd_root, cache6);

	return remove_cache(sd_root, cache2) || stale;
}
