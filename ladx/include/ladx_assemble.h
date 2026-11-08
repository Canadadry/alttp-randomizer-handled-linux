#ifndef LADX_ASSEMBLE_H
#define LADX_ASSEMBLE_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_encoder.h"
#include "ladx_ks.h"
#include "ladx_pipeline.h"
#include "ladx_randomizer.h"
#include "ladx_rom.h"
#include "ladx_settings.h"
#include "ladx_spoiler.h"
#include "ladx_tables.h"
#include "ladx_wlog.h"

typedef struct LadxAssembleInput {
	const uint8_t *base;
	size_t base_len;
	const uint8_t *base_sha1;
	const LadxTables *tables;
	const LadxSettings *settings;
	const uint8_t *seed;
	size_t seed_len;
	LadxRngSink trace;
	void *trace_ctx;
	LadxWlogSink log;
	void *log_ctx;
	const LadxPipelineGroup *groups;
	int group_count;
} LadxAssembleInput;

typedef enum LadxAssembleStatus {
	LADX_ASSEMBLE_OK = 0,
	LADX_ASSEMBLE_BAD_INPUT,
	LADX_ASSEMBLE_WRONG_BASE,
	LADX_ASSEMBLE_BAD_BASE,
	LADX_ASSEMBLE_UNSUPPORTED,
	LADX_ASSEMBLE_BROKEN,
	LADX_ASSEMBLE_OUTCOME,
	LADX_ASSEMBLE_PIPELINE,
	LADX_ASSEMBLE_IMAGE
} LadxAssembleStatus;

typedef struct LadxAssembler {
	LadxRandomizer randomizer;
	LadxEncoder encoder;
	LadxWlog log;
	LadxPipeline pipeline;
	LadxPipelineError pipeline_error;
	LadxRomTables rom_tables;
	LadxKs ks;
	LadxPipelineRom rom;
	LadxSpoiler spoiler;
	uint8_t base_digest[LADX_SHA1_SIZE];
} LadxAssembler;

LadxAssembleStatus ladx_assemble(LadxAssembler *a, const LadxAssembleInput *in, uint8_t *out);

const char *ladx_assemble_status_name(LadxAssembleStatus status);

#endif
