#ifndef LADX_RNG_H
#define LADX_RNG_H

#include "ladx_pyrandom.h"

#include <stddef.h>
#include <stdint.h>

/* The longest sequence shuffle, choice weights and choices picks take. */
#define LADX_RNG_MAX_LIST 256

typedef enum LadxRngGen {
	LADX_RNG_RND = 0,
	LADX_RNG_GLOBAL,
	LADX_RNG_GEN_COUNT
} LadxRngGen;

/* LADXR's calling frame, named by ladx_rng_stage_name as its `<module>:<qualname>`. */
typedef enum LadxRngStage {
	LADX_STAGE_RANDOMIZER_INIT = 0,
	LADX_STAGE_WORLD_SETUP_RANDOMIZE,
	LADX_STAGE_MAZE_BUILD_MAZE,
	LADX_STAGE_ITEMPOOL_RANDOMIZE_RUPEES,
	LADX_STAGE_PLACER_RUN,
	LADX_STAGE_PLACER_PLACE_SPECIFIC_ITEM,
	LADX_STAGE_PLACER_PLACE_ITEM,
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
	LADX_RNG_METHOD_COUNT
} LadxRngMethod;

/* Every non-OK return draws nothing, emits no record and leaves the out-parameters untouched. */
typedef enum LadxRngStatus {
	LADX_RNG_OK = 0,
	LADX_RNG_INDEX_ERROR,
	LADX_RNG_VALUE_ERROR,
	LADX_RNG_UNSUPPORTED,
	LADX_RNG_OVERFLOW
} LadxRngStatus;

/*
 * One traced call, as ladx_oracle's TracingRandom records it. Its pointers
 * are valid only while the sink runs. Fields a method does not use are 0.
 */
typedef struct LadxRngRecord {
	LadxRngGen gen;
	LadxRngStage stage;
	LadxRngMethod method;
	const uint8_t *seed;	 /* seed */
	size_t seed_len;
	size_t n;		 /* choice, choices, shuffle: the sequence's length */
	const int *weights;	 /* choices: n weights */
	size_t k;		 /* choices */
	int64_t a, b;		 /* randint */
	size_t index;		 /* choice's result */
	int64_t value;		 /* randint's result */
	double real;		 /* random's result */
	const size_t *list;	 /* choices' k picks, shuffle's n-long permutation */
	uint64_t words;		 /* MT words the call drew */
} LadxRngRecord;

typedef void (*LadxRngSink)(void *ctx, const LadxRngRecord *record);

typedef struct LadxRng {
	LadxPyRandom py;
	LadxRngGen gen;
	LadxRngSink sink;
	void *ctx;
} LadxRng;

/* sink may be NULL: nothing is recorded. The generator is unseeded until ladx_rng_seed. */
void ladx_rng_init(LadxRng *rng, LadxRngGen gen, LadxRngSink sink, void *ctx);

const char *ladx_rng_gen_name(LadxRngGen gen);
const char *ladx_rng_stage_name(LadxRngStage stage);
const char *ladx_rng_method_name(LadxRngMethod method);

/* seed may be NULL when size is 0. */
void ladx_rng_seed(LadxRng *rng, LadxRngStage stage, const uint8_t *seed, size_t size);

double ladx_rng_random(LadxRng *rng, LadxRngStage stage);

/* n == 0: INDEX_ERROR. */
LadxRngStatus ladx_rng_choice(LadxRng *rng, LadxRngStage stage, size_t n, size_t *index);

/* choices(range(n), weights, k=k) with integer weights; n or k over LADX_RNG_MAX_LIST: OVERFLOW. */
LadxRngStatus ladx_rng_choices(LadxRng *rng, LadxRngStage stage, const int *weights, size_t n, size_t k,
			       size_t *out);

/*
 * shuffle(x) on count elements of size bytes, as base[to] = before[perm[to]]
 * with perm shuffled; count over LADX_RNG_MAX_LIST: OVERFLOW.
 */
LadxRngStatus ladx_rng_shuffle(LadxRng *rng, LadxRngStage stage, void *base, size_t count, size_t size);

/* b < a: VALUE_ERROR; b == INT64_MAX: UNSUPPORTED. */
LadxRngStatus ladx_rng_randint(LadxRng *rng, LadxRngStage stage, int64_t a, int64_t b, int64_t *out);

/* random.setstate(rnd.getstate()): to's MT state becomes from's, one record each. */
void ladx_rng_copy_state(LadxRng *to, LadxRng *from, LadxRngStage stage);

#endif
