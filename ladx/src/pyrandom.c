#include "ladx_pyrandom.h"

#include "ladx_sha512.h"

#include <float.h>

#define M 397

static void init_genrand(LadxPyRandom *rng, uint32_t s)
{
	uint32_t i;

	rng->mt[0] = s;
	for (i = 1; i < LADX_PYRANDOM_N; i++)
		rng->mt[i] = 1812433253u * (rng->mt[i - 1] ^ (rng->mt[i - 1] >> 30)) + i;
	rng->index = LADX_PYRANDOM_N;
	rng->words = 0;
}

/* Key word i (least significant first) of the integer read big-endian from seed || digest. */
static uint32_t key_word(const uint8_t *seed, size_t size, const uint8_t *digest, size_t i)
{
	size_t length = size + LADX_SHA512_SIZE;
	uint32_t word = 0;
	int b;

	for (b = 3; b >= 0; b--) {
		size_t from_end = 4 * i + (size_t)b;
		size_t at;

		word <<= 8;
		if (from_end >= length)
			continue;
		at = length - 1 - from_end;
		word |= at < size ? seed[at] : digest[at - size];
	}
	return word;
}

void ladx_pyrandom_seed(LadxPyRandom *rng, const uint8_t *seed, size_t size)
{
	uint8_t digest[LADX_SHA512_SIZE];
	size_t key_length = (size + LADX_SHA512_SIZE + 3) / 4;
	size_t i = 1, j = 0, k;

	ladx_sha512(seed, size, digest);
	while (key_length > 1 && key_word(seed, size, digest, key_length - 1) == 0)
		key_length--;
	init_genrand(rng, 19650218u);
	for (k = LADX_PYRANDOM_N > key_length ? LADX_PYRANDOM_N : key_length; k; k--) {
		rng->mt[i] = (rng->mt[i] ^ ((rng->mt[i - 1] ^ (rng->mt[i - 1] >> 30)) * 1664525u)) +
			     key_word(seed, size, digest, j) + (uint32_t)j;
		i++;
		j++;
		if (i >= LADX_PYRANDOM_N) {
			rng->mt[0] = rng->mt[LADX_PYRANDOM_N - 1];
			i = 1;
		}
		if (j >= key_length)
			j = 0;
	}
	for (k = LADX_PYRANDOM_N - 1; k; k--) {
		rng->mt[i] = (rng->mt[i] ^ ((rng->mt[i - 1] ^ (rng->mt[i - 1] >> 30)) * 1566083941u)) - (uint32_t)i;
		i++;
		if (i >= LADX_PYRANDOM_N) {
			rng->mt[0] = rng->mt[LADX_PYRANDOM_N - 1];
			i = 1;
		}
	}
	rng->mt[0] = 0x80000000u;
}

static uint32_t twist(uint32_t upper, uint32_t lower, uint32_t far)
{
	uint32_t y = (upper & 0x80000000u) | (lower & 0x7fffffffu);

	return far ^ (y >> 1) ^ (y & 1u ? 0x9908b0dfu : 0u);
}

static uint32_t genrand(LadxPyRandom *rng)
{
	uint32_t y;

	if (rng->index >= LADX_PYRANDOM_N) {
		uint32_t i;

		for (i = 0; i < LADX_PYRANDOM_N; i++)
			rng->mt[i] = twist(rng->mt[i], rng->mt[(i + 1) % LADX_PYRANDOM_N],
					   rng->mt[(i + M) % LADX_PYRANDOM_N]);
		rng->index = 0;
	}
	y = rng->mt[rng->index++];
	rng->words++;
	y ^= y >> 11;
	y ^= (y << 7) & 0x9d2c5680u;
	y ^= (y << 15) & 0xefc60000u;
	y ^= y >> 18;
	return y;
}

double ladx_pyrandom_random(LadxPyRandom *rng)
{
	uint32_t a = genrand(rng) >> 5;
	uint32_t b = genrand(rng) >> 6;

	return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
}

LadxPyRandomStatus ladx_pyrandom_getrandbits(LadxPyRandom *rng, int k, uint64_t *out)
{
	uint64_t value = 0;
	int shift;

	if (k < 0)
		return LADX_PYRANDOM_VALUE_ERROR;
	if (k > 64)
		return LADX_PYRANDOM_UNSUPPORTED;
	for (shift = 0; k > 0; shift += 32, k -= 32) {
		uint32_t word = genrand(rng);

		if (k < 32)
			word >>= 32 - k;
		value |= (uint64_t)word << shift;
	}
	*out = value;
	return LADX_PYRANDOM_OK;
}

static int bit_length(uint64_t n)
{
	int bits = 0;

	while (n) {
		bits++;
		n >>= 1;
	}
	return bits;
}

/* n > 0. */
static uint64_t randbelow(LadxPyRandom *rng, uint64_t n)
{
	int k = bit_length(n);
	uint64_t r;

	do
		ladx_pyrandom_getrandbits(rng, k, &r);
	while (r >= n);
	return r;
}

LadxPyRandomStatus ladx_pyrandom_randrange(LadxPyRandom *rng, int64_t start, int64_t stop, int64_t *out)
{
	uint64_t r;

	if (stop <= start)
		return LADX_PYRANDOM_VALUE_ERROR;
	r = randbelow(rng, (uint64_t)stop - (uint64_t)start);
	*out = (int64_t)((uint64_t)start + r);
	return LADX_PYRANDOM_OK;
}

LadxPyRandomStatus ladx_pyrandom_randint(LadxPyRandom *rng, int64_t a, int64_t b, int64_t *out)
{
	if (b == INT64_MAX)
		return LADX_PYRANDOM_UNSUPPORTED;
	return ladx_pyrandom_randrange(rng, a, b + 1, out);
}

LadxPyRandomStatus ladx_pyrandom_choice(LadxPyRandom *rng, size_t n, size_t *index)
{
	if (n == 0)
		return LADX_PYRANDOM_INDEX_ERROR;
	*index = (size_t)randbelow(rng, n);
	return LADX_PYRANDOM_OK;
}

void ladx_pyrandom_shuffle(LadxPyRandom *rng, void *base, size_t count, size_t size)
{
	unsigned char *bytes = base;
	size_t i;

	for (i = count; i > 1; i--) {
		size_t j = (size_t)randbelow(rng, i);
		unsigned char *a = bytes + (i - 1) * size;
		unsigned char *b = bytes + j * size;
		size_t at;

		for (at = 0; a != b && at < size; at++) {
			unsigned char t = a[at];

			a[at] = b[at];
			b[at] = t;
		}
	}
}

LadxPyRandomStatus ladx_pyrandom_choices(LadxPyRandom *rng, const double *weights, size_t n, size_t k,
					 size_t *out)
{
	double total = 0.0;
	size_t i, pick;

	if (n == 0)
		return LADX_PYRANDOM_INDEX_ERROR;
	if (!weights)
		return LADX_PYRANDOM_UNSUPPORTED;
	for (i = 0; i < n; i++) {
		if (!(weights[i] >= 0.0))
			return LADX_PYRANDOM_UNSUPPORTED;
		total += weights[i];
	}
	if (!(total > 0.0 && total <= DBL_MAX))
		return LADX_PYRANDOM_VALUE_ERROR;
	for (pick = 0; pick < k; pick++) {
		double x = ladx_pyrandom_random(rng) * total;
		double cum = 0.0;

		for (i = 0; i < n - 1; i++) {
			cum += weights[i];
			if (x < cum)
				break;
		}
		out[pick] = i;
	}
	return LADX_PYRANDOM_OK;
}

/* Trap: an FMA here changes the last bit; build with -ffp-contract=off. */
double ladx_pyrandom_uniform(LadxPyRandom *rng, double a, double b)
{
	return a + (b - a) * ladx_pyrandom_random(rng);
}
