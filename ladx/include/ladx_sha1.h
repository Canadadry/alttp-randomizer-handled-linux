#ifndef LADX_SHA1_H
#define LADX_SHA1_H

#include <stddef.h>
#include <stdint.h>

#define LADX_SHA1_SIZE 20
#define LADX_SHA1_BLOCK 64

typedef struct LadxSha1 {
	uint32_t state[5];
	uint64_t length;
	uint8_t block[LADX_SHA1_BLOCK];
	size_t fill;
} LadxSha1;

void ladx_sha1_init(LadxSha1 *ctx);

/* bytes may be NULL when size is 0; any number of calls of any size. */
void ladx_sha1_update(LadxSha1 *ctx, const uint8_t *bytes, size_t size);

/* ctx must be initialised again before reuse. */
void ladx_sha1_final(LadxSha1 *ctx, uint8_t digest[LADX_SHA1_SIZE]);

void ladx_sha1(const uint8_t *bytes, size_t size, uint8_t digest[LADX_SHA1_SIZE]);

#endif
