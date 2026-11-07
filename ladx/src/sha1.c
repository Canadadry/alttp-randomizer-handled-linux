#include "ladx_sha1.h"

#include <string.h>

static uint32_t rotl(uint32_t x, int n)
{
	return (x << n) | (x >> (32 - n));
}

static void compress(uint32_t state[5], const uint8_t block[LADX_SHA1_BLOCK])
{
	uint32_t w[80], a, b, c, d, e, f, k, t;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16 |
		       (uint32_t)block[4 * i + 2] << 8 | (uint32_t)block[4 * i + 3];
	for (i = 16; i < 80; i++)
		w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	for (i = 0; i < 80; i++) {
		if (i < 20) {
			f = (b & c) | (~b & d);
			k = 0x5A827999u;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ED9EBA1u;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8F1BBCDCu;
		} else {
			f = b ^ c ^ d;
			k = 0xCA62C1D6u;
		}
		t = rotl(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = rotl(b, 30);
		b = a;
		a = t;
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

void ladx_sha1_init(LadxSha1 *ctx)
{
	ctx->state[0] = 0x67452301u;
	ctx->state[1] = 0xEFCDAB89u;
	ctx->state[2] = 0x98BADCFEu;
	ctx->state[3] = 0x10325476u;
	ctx->state[4] = 0xC3D2E1F0u;
	ctx->length = 0;
	ctx->fill = 0;
}

void ladx_sha1_update(LadxSha1 *ctx, const uint8_t *bytes, size_t size)
{
	size_t take;

	ctx->length += (uint64_t)size;
	while (size > 0) {
		take = LADX_SHA1_BLOCK - ctx->fill;
		if (take > size)
			take = size;
		memcpy(ctx->block + ctx->fill, bytes, take);
		ctx->fill += take;
		bytes += take;
		size -= take;
		if (ctx->fill == LADX_SHA1_BLOCK) {
			compress(ctx->state, ctx->block);
			ctx->fill = 0;
		}
	}
}

void ladx_sha1_final(LadxSha1 *ctx, uint8_t digest[LADX_SHA1_SIZE])
{
	uint64_t bits = ctx->length * 8u;
	int i;

	ctx->block[ctx->fill++] = 0x80;
	if (ctx->fill > LADX_SHA1_BLOCK - 8) {
		memset(ctx->block + ctx->fill, 0, LADX_SHA1_BLOCK - ctx->fill);
		compress(ctx->state, ctx->block);
		ctx->fill = 0;
	}
	memset(ctx->block + ctx->fill, 0, LADX_SHA1_BLOCK - 8 - ctx->fill);
	for (i = 0; i < 8; i++)
		ctx->block[LADX_SHA1_BLOCK - 1 - i] = (uint8_t)(bits >> (8 * i));
	compress(ctx->state, ctx->block);
	for (i = 0; i < LADX_SHA1_SIZE; i++)
		digest[i] = (uint8_t)(ctx->state[i / 4] >> (24 - 8 * (i % 4)));
}

void ladx_sha1(const uint8_t *bytes, size_t size, uint8_t digest[LADX_SHA1_SIZE])
{
	LadxSha1 ctx;

	ladx_sha1_init(&ctx);
	ladx_sha1_update(&ctx, bytes, size);
	ladx_sha1_final(&ctx, digest);
}
