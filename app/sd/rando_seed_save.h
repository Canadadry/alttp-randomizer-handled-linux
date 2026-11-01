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
#define RANDO_SEED_PATH_MAX 1024
/* rom_dir and history_dir are shorter: room for "/<rom>" and "/<code>.json". */
#define RANDO_SEED_DIR_MAX (RANDO_SEED_PATH_MAX - RANDO_NAME_ROM_MAX - 2)

typedef enum RandoSaveStatus {
	RANDO_SAVE_OK,
	RANDO_SAVE_NO_NAME,  /* every suffix taken */
	RANDO_SAVE_ASSEMBLE, /* rando_assemble failed: see result->assemble and result->bps */
	RANDO_SAVE_RECORD,   /* the record could not be written; the ROM was removed */
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
RandoSaveStatus rando_seed_save(const char *rom_dir, const char *history_dir, const RandoAssembleRequest *request,
				RandoAssembleWork *work, const RandoRecordDate *date, bool spoiler,
				RandoSaveResult *result);

#endif
