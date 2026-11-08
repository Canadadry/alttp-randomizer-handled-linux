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

/* Where one game keeps a seed's files on the card; every path absolute or relative to the cwd. */
typedef struct RandoSeedPlace {
	const char *rom_dir, *game_config_dir, *history_dir;
	const char *const *profiles;
	int profile_count;
	const char *core;    /* the saves/ and states/ folder name */
	const char *rom_ext; /* the ROM name's extension, ".sfc" */
	const char *const *save_exts;
	int save_ext_count;
	const char *const *record_exts; /* "<code><ext>" in history_dir, removed in order */
	int record_ext_count;
} RandoSeedPlace;

/*
 * The ROM, then its .game_config file, saves and states in every profile
 * that exists, then the records. Stops at ROM when the ROM stays; FILES
 * when a save or state stays (records kept); RECORD when a record stays.
 */
RandoHistoryDeleteStatus rando_seed_files_delete(const RandoSeedPlace *place, const RandoName *name);

/* Removes <sd_root>/<cache6> and <cache2>; true when one is still there. */
bool rando_sd_remove_caches(const char *sd_root, const char *cache6, const char *cache2);

#endif
