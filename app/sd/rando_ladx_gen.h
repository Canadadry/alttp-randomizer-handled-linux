#ifndef RANDO_LADX_GEN_H
#define RANDO_LADX_GEN_H

#include "ladx_assemble.h"
#include "ladx_inputs.h"
#include "ladx_settings.h"
#include "rando_app_gen.h"
#include "rando_ladx_name.h"
#include "rando_record.h"
#include "rando_seed_save.h"
#include "ui_genflow.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum RandoLadxBaseStatus {
	RANDO_LADX_BASE_OK,
	RANDO_LADX_BASE_MISSING,
	RANDO_LADX_BASE_READ,
	RANDO_LADX_BASE_SIZE,
	RANDO_LADX_BASE_SHA1,
	RANDO_LADX_BASE_STATUS_COUNT,
} RandoLadxBaseStatus;

RandoLadxBaseStatus rando_ladx_base_check(const char *path, const uint8_t sha1[LADX_SHA1_SIZE], uint8_t *buf);

typedef enum RandoLadxGenStatus {
	RANDO_LADX_GEN_OK,
	RANDO_LADX_GEN_BASE_MISSING,
	RANDO_LADX_GEN_BASE_READ,
	RANDO_LADX_GEN_BASE_SIZE,
	RANDO_LADX_GEN_BASE_SHA1,
	RANDO_LADX_GEN_MEMORY,
	RANDO_LADX_GEN_TABLES,
	RANDO_LADX_GEN_UNSUPPORTED,
	RANDO_LADX_GEN_FOLDERS,
	RANDO_LADX_GEN_SEED,
	RANDO_LADX_GEN_NO_NAME,
	RANDO_LADX_GEN_GAVE_UP,
	RANDO_LADX_GEN_BROKEN,
	RANDO_LADX_GEN_ROM_WRITE,
	RANDO_LADX_GEN_RECORD,
	RANDO_LADX_GEN_STATUS_COUNT,
} RandoLadxGenStatus;

RandoAppText rando_ladx_gen_text(RandoLadxGenStatus status);

typedef struct RandoLadxWork {
	LadxAssembler assembler;
	LadxTablesDir tables;
	uint8_t base[LADX_ROM_SIZE + 1];
	uint8_t image[LADX_ROM_SIZE];
} RandoLadxWork;

typedef struct RandoLadxGenRequest {
	LadxSettings settings;
	bool spoiler;
} RandoLadxGenRequest;

typedef bool (*RandoLadxSeedFn)(uint8_t seed[RANDO_LADX_SEED_BYTES]);
typedef void *(*RandoLadxAllocFn)(size_t size);
typedef void (*RandoLadxReleaseFn)(void *work);
typedef LadxAssembleStatus (*RandoLadxAssembleFn)(LadxAssembler *a, const LadxAssembleInput *in, uint8_t *out);
typedef bool (*RandoLadxSpoilerFn)(const LadxAssembler *a, FILE *out);

typedef struct RandoLadxGen {
	char root[RANDO_SEED_DIR_MAX];
	char rom_dir[RANDO_SEED_DIR_MAX], history_dir[RANDO_SEED_DIR_MAX];
	char base_rom[RANDO_SEED_PATH_MAX], tables_dir[RANDO_SEED_PATH_MAX];
	uint8_t base_sha1[LADX_SHA1_SIZE];
	RandoLadxSeedFn seed;
	RandoAppDateFn date;
	RandoLadxAllocFn alloc;
	RandoLadxReleaseFn release;
	RandoLadxAssembleFn assemble;
	RandoLadxSpoilerFn spoiler;
	const LadxPipelineGroup *groups;
	int group_count;
	RandoLadxGenStatus status;
	LadxAssembleStatus assembled;
	RandoName name;
	char rom_path[RANDO_SEED_PATH_MAX], record_path[RANDO_SEED_PATH_MAX], spoiler_path[RANDO_SEED_PATH_MAX];
} RandoLadxGen;

bool rando_ladx_gen_init(RandoLadxGen *gen, const char *sd_root);

void rando_ladx_gen_run(RandoLadxGen *gen, const RandoLadxGenRequest *request, UiGenOutcome *outcome);

#endif
