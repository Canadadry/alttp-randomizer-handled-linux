#ifndef LADX_PIPELINE_H
#define LADX_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_background.h"
#include "ladx_encoder.h"
#include "ladx_ks.h"
#include "ladx_randomizer.h"
#include "ladx_rom.h"
#include "ladx_room.h"
#include "ladx_settings.h"
#include "ladx_store.h"
#include "ladx_tables.h"

typedef struct LadxPipelineRom {
	const uint8_t *bytes;
	size_t len;
	const LadxRomTables *tables;
	const LadxKs *ks;
} LadxPipelineRom;

typedef struct LadxPipeline LadxPipeline;

#define LADX_SAVE_BYTES_MAX 131072

typedef struct LadxPipelineSave {
	uint8_t bytes[LADX_SAVE_BYTES_MAX];
	int32_t origins[LADX_SAVE_BYTES_MAX];
	uint32_t at[LADX_STORE_ENTRY_MAX];
	LadxStoreEntry entries[LADX_STORE_ENTRY_MAX];
	LadxStoreSpan spans[LADX_STORE_ENTRY_MAX];
	LadxStoreDone done;
} LadxPipelineSave;

typedef int (*LadxPipelineRun)(LadxEncoder *enc, LadxPipeline *p);

typedef struct LadxPipelineGroup {
	const char *name;
	int needs_rom;
	LadxPipelineRun run;
	LadxPipelineRun draw;
} LadxPipelineGroup;

#define LADX_PIPELINE_GROUP_MAX 32

const LadxPipelineGroup *ladx_pipeline_groups(int *count);

typedef enum LadxPipelineStatus {
	LADX_PIPELINE_OK = 0,
	LADX_PIPELINE_TABLES,
	LADX_PIPELINE_ENCODER,
	LADX_PIPELINE_GROUP,
	LADX_PIPELINE_RAISED,
	LADX_PIPELINE_BAD_INPUT
} LadxPipelineStatus;

typedef struct LadxPipelineError {
	LadxPipelineStatus status;
	LadxTablesError tables;
	LadxEncStatus encoder;
	const char *group;
	const LadxOutcome *outcome;
} LadxPipelineError;

struct LadxPipeline {
	const LadxTables *tables;
	const LadxSettings *settings;
	int spot_count;
	const LadxRandomizer *randomizer;
	LadxRng *rnd, *global;
	const LadxPipelineRom *rom;
	const LadxPipelineGroup *groups;
	int group_count;
	void *ctx;
	LadxOutcome raised;
	LadxAtom scratch[LADX_WLOG_ATOM_MAX];
	uint8_t values[LADX_WLOG_ATOM_MAX];
	int32_t origins[LADX_WLOG_ATOM_MAX];
	LadxBackground background;
	LadxRoom room;
	LadxPipelineSave save;
	struct LadxSpoiler *spoiler;
};

LadxPipelineStatus ladx_pipeline_run(LadxPipeline *p, LadxEncoder *enc, LadxPipelineError *err);

int ladx_pipeline_const(LadxEncoder *enc, LadxPipeline *p, const char *group);

int ladx_pipeline_const_units(LadxEncoder *enc, LadxPipeline *p, const char *group, int first, int count);

int ladx_pipeline_const_bank(LadxPipeline *p, const char *group, int unit, uint32_t offset, uint8_t *out, size_t n);

int ladx_pipeline_const_unit_count(const LadxPipeline *p, const char *group);

#endif
