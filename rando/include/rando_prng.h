#ifndef RANDO_PRNG_H
#define RANDO_PRNG_H

#include <stddef.h>
#include <stdint.h>

typedef void (*RandoDrawObserver)(void *ctx, int64_t min, int64_t max, int64_t r);

/* Caller-owned; zero-initialize it so a draw before rando_prng_seed() asserts.
 * observe is optional (NULL: no tracing) and rando_prng_seed() leaves it untouched. */
typedef struct RandoPrng {
	uint32_t s[4];
	RandoDrawObserver observe;
	void *observe_ctx;
} RandoPrng;

uint32_t rando_splitmix32(uint32_t x);
void rando_prng_seed(RandoPrng *prng, uint32_t v);
uint32_t rando_prng_next(RandoPrng *prng);
/* VT's get_random_int: min + next() % (max - min + 1), one next() even when min == max.
 * Asserts min <= max and a span of at most 2^32. */
int64_t rando_random_int(RandoPrng *prng, int64_t min, int64_t max);
/* VT's fy_shuffle, in place on count elements of size bytes: exactly count draws. */
void rando_fy_shuffle(RandoPrng *prng, void *base, size_t count, size_t size);
/* VT's Collection::randomCollection: one draw per id taken. ids[0..n) is consumed; returns the
 * number written to out, fewer than number when ids runs out. */
int rando_random_collection(RandoPrng *prng, uint8_t *ids, int n, int number, uint8_t *out);

#endif
