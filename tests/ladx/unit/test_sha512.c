#include "ladx_sha512.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_sha512"

static void to_hex(const uint8_t digest[LADX_SHA512_SIZE], char hex[2 * LADX_SHA512_SIZE + 1])
{
	size_t i;

	for (i = 0; i < LADX_SHA512_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
}

static void check_sha512(const char *what, const uint8_t *bytes, size_t size, const char *expected)
{
	uint8_t digest[LADX_SHA512_SIZE];
	char hex[2 * LADX_SHA512_SIZE + 1];

	ladx_sha512(bytes, size, digest);
	to_hex(digest, hex);
	if (strcmp(hex, expected) != 0)
		test_fail(TEST, "%s: sha512 %s, expected %s", what, hex, expected);
}

static void check_text(const char *what, const char *text, const char *expected)
{
	check_sha512(what, (const uint8_t *)text, strlen(text), expected);
}

static const char empty_digest[] = "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
				   "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e";

static void test_fips_vectors(void)
{
	check_text("empty", "", empty_digest);
	check_text("abc", "abc",
		   "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
		   "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
	check_text("896-bit message",
		   "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
		   "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
		   "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
		   "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
}

static void test_empty_with_null(void)
{
	LadxSha512 ctx;
	uint8_t digest[LADX_SHA512_SIZE];
	char hex[2 * LADX_SHA512_SIZE + 1];

	check_sha512("NULL, 0", NULL, 0, empty_digest);
	ladx_sha512_init(&ctx);
	ladx_sha512_update(&ctx, NULL, 0);
	ladx_sha512_final(&ctx, digest);
	to_hex(digest, hex);
	if (strcmp(hex, empty_digest) != 0)
		test_fail(TEST, "streaming NULL, 0: sha512 %s, expected the empty digest", hex);
}

static void test_million_a(void)
{
	static const char expected[] = "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
				       "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b";
	uint8_t chunk[1000];
	uint8_t digest[LADX_SHA512_SIZE];
	char hex[2 * LADX_SHA512_SIZE + 1];
	LadxSha512 ctx;
	int i;

	memset(chunk, 'a', sizeof chunk);
	ladx_sha512_init(&ctx);
	for (i = 0; i < 1000; i++)
		ladx_sha512_update(&ctx, chunk, sizeof chunk);
	ladx_sha512_final(&ctx, digest);
	to_hex(digest, hex);
	if (strcmp(hex, expected) != 0)
		test_fail(TEST, "one million 'a': sha512 %s, expected %s", hex, expected);
}

/* python3 -c "import hashlib; print(hashlib.sha512(bytes((i*7+3)&0xFF for i in range(N))).hexdigest())" */
static void test_padding_boundaries(void)
{
	static const struct {
		size_t size;
		const char *sha512;
	} cases[] = {
		{ 111, "68cffa6d0d76f309c9ce0d35280939f8e25990c43b7b086ccdf709be35b07d4d"
		       "dba599541ff2b1c19d34ea49aeafb9659adb7ac3c0b078bb30a22d57fc6687ef" },
		{ 112, "d0865c524d1dddf7c23b799c413f5adcd7caefd3f66a9b49750ec81066012c25"
		       "a8bcf94ddea6dc525691673097ca40e0101e897fc97218cfdb0704084e2bef4b" },
		{ 127, "e0b6a20f1c0c88970a9340152cd5a1c1ecf3d3b8de55102741879438079473540"
		       "133b812706e5dbec322c8c9523b6fc8c6d16ee626e87ad5fe3d2916afedc369" },
		{ 128, "99b16f17aa0b969a5b8f08f367719d516e330ccd2660b6f0688ec031dbc783de"
		       "50a1cd185a2568dba75070a2403d17d4741d163578515dfd2ff756ddfe4d47b1" },
		{ 239, "18ee83f30261c3c645d52aee6a209105b25bba39d33845ef48984cc238e4f216"
		       "61fb7bd7dd4336f71c40fe87d95e5115d6c7be52e0d3e7e7877d24500b5b58df" },
		{ 240, "9d60ee60d29ec4fa0b9690c04c1c29413bbe3ed345639182d9d53dcc05926b77"
		       "b04f4fec1562fb85182954c96b7cbb5d5e4410251ff4f352d09a2da90419fb13" },
	};
	uint8_t bytes[240];
	char what[32];
	size_t i;

	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)((i * 7 + 3) & 0xFF);
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		sprintf(what, "pattern of %lu bytes", (unsigned long)cases[i].size);
		check_sha512(what, bytes, cases[i].size, cases[i].sha512);
	}
}

static void test_streaming_splits(void)
{
	uint8_t bytes[300];
	uint8_t whole[LADX_SHA512_SIZE], split[LADX_SHA512_SIZE];
	LadxSha512 ctx;
	size_t i, at;

	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)((i * 31 + 5) & 0xFF);
	ladx_sha512(bytes, sizeof bytes, whole);
	for (at = 0; at <= sizeof bytes; at++) {
		ladx_sha512_init(&ctx);
		ladx_sha512_update(&ctx, bytes, at);
		ladx_sha512_update(&ctx, bytes + at, sizeof bytes - at);
		ladx_sha512_final(&ctx, split);
		if (memcmp(whole, split, sizeof whole) != 0)
			test_fail(TEST, "300 bytes split at %lu: digest differs from the one-shot one",
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
	printf("test_sha512: FIPS 180-4 vectors, padding boundaries and streaming splits ok\n");
	return 0;
}
