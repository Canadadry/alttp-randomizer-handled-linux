#ifndef LADX_PYRANDOM_H
#define LADX_PYRANDOM_H

#include <stddef.h>
#include <stdint.h>

#define LADX_PYRANDOM_N 624

/*
 * CPython's random.Random. Copying the struct is getstate/setstate: the copy
 * carries words, the number of MT words drawn since seeding.
 */
typedef struct LadxPyRandom {
	uint32_t mt[LADX_PYRANDOM_N];
	uint32_t index;
	uint64_t words;
} LadxPyRandom;

/* Every non-OK return leaves the generator and the out-parameters untouched. */
typedef enum LadxPyRandomStatus {
	LADX_PYRANDOM_OK = 0,
	LADX_PYRANDOM_INDEX_ERROR,
	LADX_PYRANDOM_VALUE_ERROR,
	LADX_PYRANDOM_UNSUPPORTED
} LadxPyRandomStatus;

/* random.Random(bytes(seed)), version 2; seed may be NULL when size is 0. */
void ladx_pyrandom_seed(LadxPyRandom *rng, const uint8_t *seed, size_t size);

double ladx_pyrandom_random(LadxPyRandom *rng);

/* k < 0: VALUE_ERROR; k > 64: UNSUPPORTED. */
LadxPyRandomStatus ladx_pyrandom_getrandbits(LadxPyRandom *rng, int k, uint64_t *out);

/* randrange(start, stop), step 1; stop <= start: VALUE_ERROR. */
LadxPyRandomStatus ladx_pyrandom_randrange(LadxPyRandom *rng, int64_t start, int64_t stop, int64_t *out);

/* b == INT64_MAX: UNSUPPORTED. */
LadxPyRandomStatus ladx_pyrandom_randint(LadxPyRandom *rng, int64_t a, int64_t b, int64_t *out);

/* The index choice(seq) picks in a sequence of n; n == 0: INDEX_ERROR. */
LadxPyRandomStatus ladx_pyrandom_choice(LadxPyRandom *rng, size_t n, size_t *index);

void ladx_pyrandom_shuffle(LadxPyRandom *rng, void *base, size_t count, size_t size);

/*
 * The k indexes choices(range(n), weights, k=k) picks, into out[0..k-1].
 * n == 0: INDEX_ERROR; total not positive or not finite: VALUE_ERROR;
 * weights NULL, or a weight negative or NaN: UNSUPPORTED.
 */
LadxPyRandomStatus ladx_pyrandom_choices(LadxPyRandom *rng, const double *weights, size_t n, size_t k,
					 size_t *out);

double ladx_pyrandom_uniform(LadxPyRandom *rng, double a, double b);

#endif
