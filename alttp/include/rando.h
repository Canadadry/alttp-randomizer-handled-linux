#ifndef RANDO_H
#define RANDO_H

#include "rando_options.h"
#include "rando_pool.h"
#include "rando_postfill.h"
#include "rando_prng.h"
#include "rando_world.h"

#include <stdint.h>

typedef enum RandoStage {
	RANDO_STAGE_CRYSTALS,
	RANDO_STAGE_GET_ADVANCEMENT_ITEMS,
	RANDO_STAGE_SET_SHOPS,
	RANDO_STAGE_SET_MEDALLIONS,
	RANDO_STAGE_PLACE_BOSSES,
	RANDO_STAGE_FILL_PRIZES,
	RANDO_STAGE_SET_FOUNTAINS,
	RANDO_STAGE_SHUFFLE_PRIZE_PACKS,
	RANDO_STAGE_PREPARE_WORLD,
	RANDO_STAGE_FILL,
	RANDO_STAGE_SET_TEXTS,
	RANDO_STAGE_RANDOMIZE_CREDITS,
	RANDO_STAGE_APPLY_HINTS,
	RANDO_STAGE_WRITE_ITEM,
	RANDO_STAGE_SET_DIGGING_GAME_RNG,
	RANDO_STAGE_WRITE_RNG_BLOCK,
	RANDO_STAGE_WRITE_PRIZE_PACKS_TO_ROM,
	RANDO_STAGE_COUNT,
	RANDO_STAGE_NONE = RANDO_STAGE_COUNT,
} RandoStage;

const char *rando_stage_name(RandoStage stage);

typedef enum RandoStatus {
	RANDO_COMPLETE,
	RANDO_INCOMPLETE,
	RANDO_UNSUPPORTED,
	RANDO_GAVE_UP,
} RandoStatus;

typedef void (*RandoTraceFn)(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min,
			     int64_t max, int64_t r);

#ifdef RANDO_FAULTS
typedef enum RandoFault {
	RANDO_FAULT_NONE,
	RANDO_FAULT_MID_FILL,
	RANDO_FAULT_IS_WINNABLE,
} RandoFault;
#endif

typedef struct RandoGen {
	RandoOptions options;
	uint32_t seed;
	uint32_t attempt;
	RandoStage stage;
	RandoStage last;
	uint32_t draw;
	RandoTraceFn trace;
	void *trace_ctx;
	RandoPrng prng;
	RandoPools pools;
	uint8_t advancement[RANDO_POOL_CAPACITY];
	uint16_t nadvancement;
	RandoWorld world;
	RandoPostFill post;
#ifdef RANDO_FAULTS
	RandoFault fault;
	uint32_t fault_attempts;
	uint32_t fault_fired;
#endif
} RandoGen;

RandoStatus rando_generate(RandoGen *gen, const RandoOptions *options, uint32_t seed, uint32_t max_attempts,
			   RandoTraceFn trace, void *trace_ctx);

RandoStatus rando_generate_until(RandoGen *gen, const RandoOptions *options, uint32_t seed, uint32_t max_attempts,
				 RandoTraceFn trace, void *trace_ctx, RandoStage last);

#endif
