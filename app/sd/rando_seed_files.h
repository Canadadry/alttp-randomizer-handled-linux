#ifndef RANDO_SEED_FILES_H
#define RANDO_SEED_FILES_H

#include "rando_name.h"

#include <stdbool.h>

typedef enum RandoHistoryDeleteStatus {
	RANDO_HISTORY_DELETE_OK,
	RANDO_HISTORY_DELETE_ROM,
	RANDO_HISTORY_DELETE_FILES,
	RANDO_HISTORY_DELETE_RECORD,
	RANDO_HISTORY_DELETE_STATUS_COUNT,
} RandoHistoryDeleteStatus;

typedef struct RandoSeedPlace {
	const char *rom_dir, *game_config_dir, *history_dir;
	const char *const *profiles;
	int profile_count;
	const char *core;
	const char *rom_ext;
	const char *const *save_exts;
	int save_ext_count;
	const char *const *record_exts;
	int record_ext_count;
} RandoSeedPlace;

RandoHistoryDeleteStatus rando_seed_files_delete(const RandoSeedPlace *place, const RandoName *name);

bool rando_sd_remove_caches(const char *sd_root, const char *cache6, const char *cache2);

#endif
