#ifndef RANDO_SEED_SAVE_H
#define RANDO_SEED_SAVE_H

#include "rando_assemble.h"
#include "rando_bps.h"
#include "rando_name.h"
#include "rando_record.h"

#include <stdbool.h>

#define RANDO_SD_ROM_DIR "Roms/SFC/ALttPR"
#define RANDO_SD_HISTORY_DIR "App/ALttPR/history"
#define RANDO_SD_SETTINGS_FILE "App/ALttPR/settings.json"
#define RANDO_SD_BASE_ROM_FILE "App/ALttPR/base.sfc"
#define RANDO_SD_BASE_PATCH_FILE "App/ALttPR/base.bps"
/* Onion's SNES list caches, removed after a new ROM so the list shows it. */
#define RANDO_SD_LIST_CACHE6 "Roms/SFC/SFC_cache6.db"
#define RANDO_SD_LIST_CACHE2 "Roms/SFC/SFC_cache2.db"
/* RetroArch's per-profile folders (Guest Mode renames them), the default core's folder name under their
 * saves and states folders, and the per-ROM core overrides of the ROM folder. */
#define RANDO_SD_PROFILE_CURRENT "Saves/CurrentProfile"
#define RANDO_SD_PROFILE_MAIN "Saves/MainProfile"
#define RANDO_SD_PROFILE_GUEST "Saves/GuestProfile"
#define RANDO_SD_CORE_DIR "Supafaust"
#define RANDO_SD_SAVES_DIR "saves"
#define RANDO_SD_STATES_DIR "states"
#define RANDO_SD_GAME_CONFIG_DIR RANDO_SD_ROM_DIR "/.game_config"
#define RANDO_SEED_PATH_MAX 1024
/* rom_dir and history_dir are shorter: room for "/<rom>" and "/<code>.json". */
#define RANDO_SEED_DIR_MAX (RANDO_SEED_PATH_MAX - RANDO_NAME_ROM_MAX - 2)

typedef enum RandoSaveStatus {
	RANDO_SAVE_OK,
	RANDO_SAVE_NO_NAME,  /* every suffix taken */
	RANDO_SAVE_ASSEMBLE, /* rando_assemble failed: see result->assemble and result->bps */
	RANDO_SAVE_RECORD,   /* the record could not be written; the ROM was removed */
	RANDO_SAVE_STATUS_COUNT, /* never returned */
} RandoSaveStatus;

typedef struct RandoSaveResult {
	/* The chosen name; on NO_NAME, the unsuffixed one. */
	RandoName name;
	char rom_path[RANDO_SEED_PATH_MAX], record_path[RANDO_SEED_PATH_MAX];
	RandoAssembleStatus assemble;
	RandoBpsResult bps;
} RandoSaveResult;

/* Chooses a free name (a ROM in rom_dir or a record in history_dir takes it), assembles request straight
 * into <rom_dir>/<rom>, then writes the record <history_dir>/<code>.json, holding the spoiler only when
 * spoiler. request's output and output_ctx are replaced by the save's file writer; every other field is
 * used as given. Both directories must exist and be shorter than RANDO_SEED_DIR_MAX (asserted). After OK
 * both files exist; after any other status, neither file of this seed does. Not reentrant. */
/* Removes both SNES list caches under sd_root; true when one is still present afterwards (an absent one is
 * fine). */
bool rando_sd_remove_list_caches(const char *sd_root);

RandoSaveStatus rando_seed_save(const char *rom_dir, const char *history_dir, const RandoAssembleRequest *request,
				RandoAssembleWork *work, const RandoRecordDate *date, bool spoiler,
				RandoSaveResult *result);

#endif
