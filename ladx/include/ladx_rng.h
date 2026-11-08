#ifndef LADX_RNG_H
#define LADX_RNG_H

#include "ladx_pyrandom.h"

#include <stddef.h>
#include <stdint.h>

#define LADX_RNG_MAX_LIST 256

typedef enum LadxRngGen {
	LADX_RNG_RND = 0,
	LADX_RNG_GLOBAL,
	LADX_RNG_GEN_COUNT
} LadxRngGen;

typedef enum LadxRngStage {
	LADX_STAGE_RANDOMIZER_INIT = 0,
	LADX_STAGE_WORLD_SETUP_RANDOMIZE,
	LADX_STAGE_MAZE_BUILD_MAZE,
	LADX_STAGE_ITEMPOOL_RANDOMIZE_RUPEES,
	LADX_STAGE_PLACER_RUN,
	LADX_STAGE_PLACER_PLACE_SPECIFIC_ITEM,
	LADX_STAGE_PLACER_PLACE_ITEM,
	LADX_STAGE_MARIN_STARTING_TEXT,
	LADX_STAGE_UTILS_RANDOM_NUMBER,
	LADX_STAGE_HINTS_ADD_HINTS,
	LADX_STAGE_MUSIC_RANDOMIZE,
	LADX_STAGE_ENDSCREEN_UPDATE,
	LADX_STAGE_ENEMIES_RANDOMIZE,
	LADX_STAGE_COUNT
} LadxRngStage;

typedef enum LadxRngMethod {
	LADX_RNG_SEED = 0,
	LADX_RNG_RANDOM,
	LADX_RNG_CHOICE,
	LADX_RNG_CHOICES,
	LADX_RNG_SHUFFLE,
	LADX_RNG_RANDINT,
	LADX_RNG_GETSTATE,
	LADX_RNG_SETSTATE,
	LADX_RNG_RANDRANGE,
	LADX_RNG_METHOD_COUNT
} LadxRngMethod;

typedef enum LadxRngStatus {
	LADX_RNG_OK = 0,
	LADX_RNG_INDEX_ERROR,
	LADX_RNG_VALUE_ERROR,
	LADX_RNG_UNSUPPORTED,
	LADX_RNG_OVERFLOW
} LadxRngStatus;

typedef struct LadxRngRecord {
	LadxRngGen gen;
	LadxRngStage stage;
	LadxRngMethod method;
	const uint8_t *seed;
	size_t seed_len;
	size_t n;
	const int *weights;
	size_t k;
	int64_t a, b;
	size_t index;
	int64_t value;
	double real;
	const size_t *list;
	uint64_t words;
} LadxRngRecord;

typedef void (*LadxRngSink)(void *ctx, const LadxRngRecord *record);

typedef struct LadxRng {
	LadxPyRandom py;
	LadxRngGen gen;
	LadxRngSink sink;
	void *ctx;
} LadxRng;

void ladx_rng_init(LadxRng *rng, LadxRngGen gen, LadxRngSink sink, void *ctx);

const char *ladx_rng_gen_name(LadxRngGen gen);
const char *ladx_rng_stage_name(LadxRngStage stage);
const char *ladx_rng_method_name(LadxRngMethod method);

void ladx_rng_seed(LadxRng *rng, LadxRngStage stage, const uint8_t *seed, size_t size);

double ladx_rng_random(LadxRng *rng, LadxRngStage stage);

LadxRngStatus ladx_rng_choice(LadxRng *rng, LadxRngStage stage, size_t n, size_t *index);

LadxRngStatus ladx_rng_choices(LadxRng *rng, LadxRngStage stage, const int *weights, size_t n, size_t k,
			       size_t *out);

LadxRngStatus ladx_rng_shuffle(LadxRng *rng, LadxRngStage stage, void *base, size_t count, size_t size);

LadxRngStatus ladx_rng_randint(LadxRng *rng, LadxRngStage stage, int64_t a, int64_t b, int64_t *out);

LadxRngStatus ladx_rng_randrange(LadxRng *rng, LadxRngStage stage, int64_t start, int64_t stop, int64_t *out);

void ladx_rng_copy_state(LadxRng *to, LadxRng *from, LadxRngStage stage);

#endif
