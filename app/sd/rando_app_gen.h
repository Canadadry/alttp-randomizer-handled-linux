#ifndef RANDO_APP_GEN_H
#define RANDO_APP_GEN_H

#include "rando_assemble.h"
#include "rando_bps.h"
#include "rando_out.h"
#include "rando_record.h"
#include "rando_seed_save.h"
#include "ui_genflow.h"
#include "ui_strings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RANDO_SD_DEFAULT_ROOT "/mnt/SDCARD"

typedef enum RandoAppGenStatus {
	RANDO_APP_GEN_OK,
	RANDO_APP_GEN_BASE_MISSING,
	RANDO_APP_GEN_BASE_READ,
	RANDO_APP_GEN_PATCH_MISSING,
	RANDO_APP_GEN_PATCH_READ,
	RANDO_APP_GEN_PATCH_TOO_LARGE,
	RANDO_APP_GEN_FOLDERS,
	RANDO_APP_GEN_SAVE,
	RANDO_APP_GEN_STATUS_COUNT,
} RandoAppGenStatus;

typedef struct RandoAppText {
	UiStrId status, advice;
} RandoAppText;

RandoAppText rando_app_gen_text(RandoAppGenStatus status);
RandoAppText rando_app_save_text(RandoSaveStatus status);
RandoAppText rando_app_assemble_text(RandoAssembleStatus status);
RandoAppText rando_app_bps_text(RandoBpsResult result);

typedef uint32_t (*RandoAppSeedFn)(void);
typedef void (*RandoAppDateFn)(RandoRecordDate *out);
typedef RandoSaveStatus (*RandoAppSaveFn)(const char *rom_dir, const char *history_dir,
					  const RandoAssembleRequest *request, RandoAssembleWork *work,
					  const RandoRecordDate *date, bool spoiler, RandoSaveResult *result);

bool rando_app_sd_root(const char *app_dir, char *out, size_t cap);

typedef struct RandoAppGen {
	char root[RANDO_SEED_DIR_MAX];
	char rom_dir[RANDO_SEED_DIR_MAX], history_dir[RANDO_SEED_DIR_MAX];
	char settings[RANDO_SEED_PATH_MAX], base_rom[RANDO_SEED_PATH_MAX], base_patch[RANDO_SEED_PATH_MAX];
	uint8_t source_md5[RANDO_MD5_SIZE], base_md5[RANDO_MD5_SIZE];
	RandoAppSeedFn seed;
	RandoAppDateFn date;
	RandoAppSaveFn save;
	uint8_t source[RANDO_ROM_SOURCE_SIZE + 1];
	uint8_t patch[RANDO_OUT_PATCH_MAX + 1];
	RandoAssembleWork work;
	RandoSaveResult result;
} RandoAppGen;

bool rando_app_gen_init(RandoAppGen *gen, const char *sd_root);

void rando_app_gen_run(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome);

#endif
