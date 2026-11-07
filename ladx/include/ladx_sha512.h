#ifndef LADX_SHA512_H
#define LADX_SHA512_H

#include <stddef.h>
#include <stdint.h>

#define LADX_SHA512_SIZE 64
#define LADX_SHA512_BLOCK 128

typedef struct LadxSha512 {
	uint64_t state[8];
	uint64_t length_high;
	uint64_t length_low;
	uint8_t block[LADX_SHA512_BLOCK];
	size_t fill;
} LadxSha512;

void ladx_sha512_init(LadxSha512 *ctx);

/* bytes may be NULL when size is 0; any number of calls of any size. */
void ladx_sha512_update(LadxSha512 *ctx, const uint8_t *bytes, size_t size);

/* ctx must be initialised again before reuse. */
void ladx_sha512_final(LadxSha512 *ctx, uint8_t digest[LADX_SHA512_SIZE]);

void ladx_sha512(const uint8_t *bytes, size_t size, uint8_t digest[LADX_SHA512_SIZE]);

#endif
