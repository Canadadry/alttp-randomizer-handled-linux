#ifndef RANDO_PRNG_H
#define RANDO_PRNG_H

#include <stddef.h>
#include <stdint.h>

typedef void (*RandoDrawObserver)(void *ctx, int64_t min, int64_t max, int64_t r);

typedef struct RandoPrng {
	uint32_t s[4];
	RandoDrawObserver observe;
	void *observe_ctx;
} RandoPrng;

uint32_t rando_splitmix32(uint32_t x);
void rando_prng_seed(RandoPrng *prng, uint32_t v);
uint32_t rando_prng_next(RandoPrng *prng);
int64_t rando_random_int(RandoPrng *prng, int64_t min, int64_t max);
void rando_fy_shuffle(RandoPrng *prng, void *base, size_t count, size_t size);
int rando_random_collection(RandoPrng *prng, uint8_t *ids, int n, int number, uint8_t *out);

#endif
