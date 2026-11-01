#include "rando_prng.h"

#include <assert.h>
#include <string.h>

uint32_t rando_splitmix32(uint32_t x)
{
	uint32_t z = x + 0x9E3779B9u;

	z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
	z = (z ^ (z >> 13)) * 0xC2B2AE35u;
	return z ^ (z >> 16);
}

static uint32_t rotl(uint32_t x, int k)
{
	return (x << k) | (x >> (32 - k));
}

void rando_prng_seed(RandoPrng *prng, uint32_t v)
{
	uint32_t k;

	for (k = 0; k < 4; k++)
		prng->s[k] = rando_splitmix32(v + k * 0x9E3779B9u);
	assert((prng->s[0] | prng->s[1] | prng->s[2] | prng->s[3]) != 0 &&
	       "rando_prng_seed: all-zero xoshiro128** state");
}

uint32_t rando_prng_next(RandoPrng *prng)
{
	uint32_t *s = prng->s;
	uint32_t r, t;

	assert((s[0] | s[1] | s[2] | s[3]) != 0 &&
	       "rando_prng_next: rando_prng_seed() was never called");
	r = rotl(s[1] * 5u, 7) * 9u;
	t = s[1] << 9;
	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];
	s[2] ^= t;
	s[3] = rotl(s[3], 11);
	return r;
}

int64_t rando_random_int(RandoPrng *prng, int64_t min, int64_t max)
{
	uint64_t width;
	uint32_t r;
	int64_t result;

	assert(min <= max && "rando_random_int: min > max");
	width = (uint64_t)max - (uint64_t)min;
	assert(width <= 0xFFFFFFFFu && "rando_random_int: span above 2^32");
	r = rando_prng_next(prng);
	if (width < 0xFFFFFFFFu)
		r %= (uint32_t)width + 1u;
	result = min + (int64_t)r;
	if (prng->observe)
		prng->observe(prng->observe_ctx, min, max, result);
	return result;
}

static void swap_bytes(unsigned char *a, unsigned char *b, size_t size)
{
	while (size--) {
		unsigned char t = *a;

		*a++ = *b;
		*b++ = t;
	}
}

void rando_fy_shuffle(RandoPrng *prng, void *base, size_t count, size_t size)
{
	unsigned char *bytes = base;
	size_t i;

	/* VT draws get_random_int(0, 0) for i = 0 too; skipping it desyncs every later draw. */
	for (i = count; i-- > 0;) {
		size_t r = (size_t)rando_random_int(prng, 0, (int64_t)i);

		swap_bytes(bytes + i * size, bytes + r * size, size);
	}
}

int rando_random_collection(RandoPrng *prng, uint8_t *ids, int n, int number, uint8_t *out)
{
	int k = 0;

	while (number-- > 0 && n > 0) {
		int r = (int)rando_random_int(prng, 0, n - 1);

		out[k++] = ids[r];
		memmove(ids + r, ids + r + 1, (size_t)(n - r - 1));
		n--;
	}
	return k;
}
