#include "ladx_sha1.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_sha1"

static void to_hex(const uint8_t digest[LADX_SHA1_SIZE], char hex[2 * LADX_SHA1_SIZE + 1])
{
	size_t i;

	for (i = 0; i < LADX_SHA1_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
}

static void check_sha1(const char *what, const uint8_t *bytes, size_t size, const char *expected)
{
	uint8_t digest[LADX_SHA1_SIZE];
	char hex[2 * LADX_SHA1_SIZE + 1];

	ladx_sha1(bytes, size, digest);
	to_hex(digest, hex);
	if (strcmp(hex, expected) != 0)
		test_fail(TEST, "%s: sha1 %s, expected %s", what, hex, expected);
}

static void check_text(const char *what, const char *text, const char *expected)
{
	check_sha1(what, (const uint8_t *)text, strlen(text), expected);
}

static const char empty_digest[] = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

static void test_fips_vectors(void)
{
	check_text("empty", "", empty_digest);
	check_text("abc", "abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
	check_text("448-bit message", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		   "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

static void test_empty_with_null(void)
{
	LadxSha1 ctx;
	uint8_t digest[LADX_SHA1_SIZE];
	char hex[2 * LADX_SHA1_SIZE + 1];

	check_sha1("NULL, 0", NULL, 0, empty_digest);
	ladx_sha1_init(&ctx);
	ladx_sha1_update(&ctx, NULL, 0);
	ladx_sha1_final(&ctx, digest);
	to_hex(digest, hex);
	if (strcmp(hex, empty_digest) != 0)
		test_fail(TEST, "streaming NULL, 0: sha1 %s, expected the empty digest", hex);
}

static void test_million_a(void)
{
	uint8_t chunk[1000];
	uint8_t digest[LADX_SHA1_SIZE];
	char hex[2 * LADX_SHA1_SIZE + 1];
	LadxSha1 ctx;
	int i;

	memset(chunk, 'a', sizeof chunk);
	ladx_sha1_init(&ctx);
	for (i = 0; i < 1000; i++)
		ladx_sha1_update(&ctx, chunk, sizeof chunk);
	ladx_sha1_final(&ctx, digest);
	to_hex(digest, hex);
	if (strcmp(hex, "34aa973cd4c4daa4f61eeb2bdbad27316534016f") != 0)
		test_fail(TEST, "one million 'a': sha1 %s", hex);
}

/* python3 -c "import hashlib; print(hashlib.sha1(bytes((i*7+3)&0xFF for i in range(N))).hexdigest())" */
static void test_padding_boundaries(void)
{
	static const struct {
		size_t size;
		const char *sha1;
	} cases[] = {
		{ 55, "ddf57317ef34bfee3b6df83d359098930eb278bc" },
		{ 56, "a0d492bb0fc889d0eca3bc137066ab6f4f74f369" },
		{ 63, "c55856749bef509bdfe6bfebfc7bf4e793e82132" },
		{ 64, "bede92be29c3874e1b54ddc77988d606fc857a8e" },
		{ 119, "504e27376a6e0f0dba8295b85cb25dc4dfa17d23" },
		{ 120, "82134b02fb3f702491be9bed581eeab59334acb2" },
	};
	uint8_t bytes[120];
	char what[32];
	size_t i;

	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)((i * 7 + 3) & 0xFF);
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		sprintf(what, "pattern of %lu bytes", (unsigned long)cases[i].size);
		check_sha1(what, bytes, cases[i].size, cases[i].sha1);
	}
}

static void test_streaming_splits(void)
{
	uint8_t bytes[200];
	uint8_t whole[LADX_SHA1_SIZE], split[LADX_SHA1_SIZE];
	LadxSha1 ctx;
	size_t i, at;

	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)((i * 31 + 5) & 0xFF);
	ladx_sha1(bytes, sizeof bytes, whole);
	for (at = 0; at <= sizeof bytes; at++) {
		ladx_sha1_init(&ctx);
		ladx_sha1_update(&ctx, bytes, at);
		ladx_sha1_update(&ctx, bytes + at, sizeof bytes - at);
		ladx_sha1_final(&ctx, split);
		if (memcmp(whole, split, sizeof whole) != 0)
			test_fail(TEST, "200 bytes split at %lu: digest differs from the one-shot one",
				  (unsigned long)at);
	}
}

int main(void)
{
	test_fips_vectors();
	test_empty_with_null();
	test_million_a();
	test_padding_boundaries();
	test_streaming_splits();
	printf("test_sha1: FIPS 180-4 vectors, padding boundaries and streaming splits ok\n");
	return 0;
}
