#ifndef RANDO_SEED_SAVE_H
#define RANDO_SEED_SAVE_H

#include "rando_assemble.h"
#include "rando_bps.h"
#include "rando_name.h"
#include "rando_record.h"

#include <stdbool.h>

#define RANDO_SD_ROM_DIR "Roms/SFC/ALttPR"
#define RANDO_SD_HISTORY_DIR "App/ZeldaRando/history"
#define RANDO_SD_SETTINGS_FILE "App/ZeldaRando/settings.json"
#define RANDO_SD_BASE_ROM_FILE "App/ZeldaRando/base.sfc"
#define RANDO_SD_BASE_PATCH_FILE "App/ZeldaRando/base.bps"
#define RANDO_SD_LIST_CACHE6 "Roms/SFC/SFC_cache6.db"
#define RANDO_SD_LIST_CACHE2 "Roms/SFC/SFC_cache2.db"
#define RANDO_SD_PROFILE_CURRENT "Saves/CurrentProfile"
#define RANDO_SD_PROFILE_MAIN "Saves/MainProfile"
#define RANDO_SD_PROFILE_GUEST "Saves/GuestProfile"
#define RANDO_SD_CORE_DIR "Supafaust"
#define RANDO_SD_SAVES_DIR "saves"
#define RANDO_SD_STATES_DIR "states"
#define RANDO_SD_GAME_CONFIG_DIR RANDO_SD_ROM_DIR "/.game_config"
#define RANDO_SEED_PATH_MAX 1024
#define RANDO_SEED_DIR_MAX (RANDO_SEED_PATH_MAX - RANDO_NAME_ROM_MAX - 2)

typedef enum RandoSaveStatus {
	RANDO_SAVE_OK,
	RANDO_SAVE_NO_NAME,
	RANDO_SAVE_ASSEMBLE,
	RANDO_SAVE_RECORD,
	RANDO_SAVE_STATUS_COUNT,
} RandoSaveStatus;

typedef struct RandoSaveResult {
	RandoName name;
	char rom_path[RANDO_SEED_PATH_MAX], record_path[RANDO_SEED_PATH_MAX];
	RandoAssembleStatus assemble;
	RandoBpsResult bps;
} RandoSaveResult;

bool rando_sd_remove_list_caches(const char *sd_root);

RandoSaveStatus rando_seed_save(const char *rom_dir, const char *history_dir, const RandoAssembleRequest *request,
				RandoAssembleWork *work, const RandoRecordDate *date, bool spoiler,
				RandoSaveResult *result);

#endif
