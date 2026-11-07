#ifndef LADX_MD5_H
#define LADX_MD5_H

#include <stddef.h>
#include <stdint.h>

/* MD5 (RFC 1321), for patches/titleScreen.py's non-hex seed text. */
#define LADX_MD5_SIZE 16
#define LADX_MD5_BLOCK 64

typedef struct LadxMd5 {
	uint32_t state[4];
	uint64_t length;
	uint8_t block[LADX_MD5_BLOCK];
	size_t fill;
} LadxMd5;

void ladx_md5_init(LadxMd5 *ctx);

/* bytes may be NULL when size is 0; any number of calls of any size. */
void ladx_md5_update(LadxMd5 *ctx, const uint8_t *bytes, size_t size);

/* ctx must be initialised again before reuse. */
void ladx_md5_final(LadxMd5 *ctx, uint8_t digest[LADX_MD5_SIZE]);

void ladx_md5(const uint8_t *bytes, size_t size, uint8_t digest[LADX_MD5_SIZE]);

#endif
