#ifndef LADX_MD5_H
#define LADX_MD5_H

#include <stddef.h>
#include <stdint.h>

#define LADX_MD5_SIZE 16
#define LADX_MD5_BLOCK 64

typedef struct LadxMd5 {
	uint32_t state[4];
	uint64_t length;
	uint8_t block[LADX_MD5_BLOCK];
	size_t fill;
} LadxMd5;

void ladx_md5_init(LadxMd5 *ctx);

void ladx_md5_update(LadxMd5 *ctx, const uint8_t *bytes, size_t size);

void ladx_md5_final(LadxMd5 *ctx, uint8_t digest[LADX_MD5_SIZE]);

void ladx_md5(const uint8_t *bytes, size_t size, uint8_t digest[LADX_MD5_SIZE]);

#endif
