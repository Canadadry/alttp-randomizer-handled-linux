#ifndef LADX_PYRANDOM_H
#define LADX_PYRANDOM_H

#include <stddef.h>
#include <stdint.h>

#define LADX_PYRANDOM_N 624

typedef struct LadxPyRandom {
	uint32_t mt[LADX_PYRANDOM_N];
	uint32_t index;
	uint64_t words;
} LadxPyRandom;

typedef enum LadxPyRandomStatus {
	LADX_PYRANDOM_OK = 0,
	LADX_PYRANDOM_INDEX_ERROR,
	LADX_PYRANDOM_VALUE_ERROR,
	LADX_PYRANDOM_UNSUPPORTED
} LadxPyRandomStatus;

void ladx_pyrandom_seed(LadxPyRandom *rng, const uint8_t *seed, size_t size);

double ladx_pyrandom_random(LadxPyRandom *rng);

LadxPyRandomStatus ladx_pyrandom_getrandbits(LadxPyRandom *rng, int k, uint64_t *out);

LadxPyRandomStatus ladx_pyrandom_randrange(LadxPyRandom *rng, int64_t start, int64_t stop, int64_t *out);

LadxPyRandomStatus ladx_pyrandom_randint(LadxPyRandom *rng, int64_t a, int64_t b, int64_t *out);

LadxPyRandomStatus ladx_pyrandom_choice(LadxPyRandom *rng, size_t n, size_t *index);

void ladx_pyrandom_shuffle(LadxPyRandom *rng, void *base, size_t count, size_t size);

LadxPyRandomStatus ladx_pyrandom_choices(LadxPyRandom *rng, const double *weights, size_t n, size_t k,
					 size_t *out);

double ladx_pyrandom_uniform(LadxPyRandom *rng, double a, double b);

#endif
