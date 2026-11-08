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

/*
 * The whole generation of a LADX seed (tests/ladx/README.md, "Assembly"):
 * the base checked, parsed and its KS built, the fill, the pipeline
 * (table entries, every C group, the header) and the image read out of
 * the encoder, its header fixed. Caller-owned, no allocation.
 */

typedef struct LadxAssembleInput {
	const uint8_t *base; /* the source ROM, read until ladx_assemble returns */
	size_t base_len;
	const uint8_t *base_sha1; /* the digest the base must have; NULL: LADX_ROM_SHA1_HEX's */
	const LadxTables *tables;
	const LadxSettings *settings; /* validated */
	const uint8_t *seed;
	size_t seed_len;
	LadxRngSink trace; /* every draw, may be NULL */
	void *trace_ctx;
	LadxWlogSink log; /* the write log's text; NULL: discarded */
	void *log_ctx;
	const LadxPipelineGroup *groups; /* NULL: ladx_pipeline_groups' */
	int group_count;
} LadxAssembleInput;

typedef enum LadxAssembleStatus {
	LADX_ASSEMBLE_OK = 0,
	LADX_ASSEMBLE_BAD_INPUT,   /* a NULL pointer, or out overlapping the base */
	LADX_ASSEMBLE_WRONG_BASE,  /* not LADX_ROM_SIZE bytes, or another SHA-1: base_digest */
	LADX_ASSEMBLE_BAD_BASE,	   /* its tables do not parse, or KS fails */
	LADX_ASSEMBLE_UNSUPPORTED, /* settings outside tier 1 */
	LADX_ASSEMBLE_BROKEN,	   /* ladx_randomize broke: a C bug */
	LADX_ASSEMBLE_OUTCOME,	   /* LADXR's fill ended in an error or a raise: ladx_randomizer_outcome(&a->randomizer) */
	LADX_ASSEMBLE_PIPELINE,	   /* the pipeline stopped: a->pipeline_error (RAISED: LADXR raised) */
	LADX_ASSEMBLE_IMAGE	   /* a byte of the image unreadable or the header fix failed: a C bug */
} LadxAssembleStatus;

/*
 * Every buffer of a run, about 21 MiB (the encoder and the log most of
 * it). Read randomizer, pipeline, pipeline_error, spoiler and base_digest
 * after a run; the rest belongs to assemble.c.
 */
typedef struct LadxAssembler {
	LadxRandomizer randomizer;
	LadxEncoder encoder;
	LadxWlog log;
	LadxPipeline pipeline;
	LadxPipelineError pipeline_error;
	LadxRomTables rom_tables;
	LadxKs ks;
	LadxPipelineRom rom;
	LadxSpoiler spoiler; /* built when the pipeline's spoiler group ran and race is off (spoiler.built) */
	uint8_t base_digest[LADX_SHA1_SIZE];
} LadxAssembler;

/*
 * The patched ROM of in into out (LADX_ROM_SIZE bytes, not overlapping
 * in->base): untouched before the image step, the image only on OK. Steps in order, each after the one before
 * succeeded: the base's size and SHA-1, its tables parsed and KS built,
 * the settings' support, ladx_randomize, the pipeline, the image read out
 * of the encoder, ladx_header_fix(name "LADXR") on the image.
 */
LadxAssembleStatus ladx_assemble(LadxAssembler *a, const LadxAssembleInput *in, uint8_t *out);

const char *ladx_assemble_status_name(LadxAssembleStatus status);

#endif
