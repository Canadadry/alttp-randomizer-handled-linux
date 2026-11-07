#include "ladx_md5.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_md5"

static void to_hex(const uint8_t digest[LADX_MD5_SIZE], char hex[2 * LADX_MD5_SIZE + 1])
{
	size_t i;

	for (i = 0; i < LADX_MD5_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
}

static void check_md5(const char *what, const uint8_t *bytes, size_t size, const char *expected)
{
	uint8_t digest[LADX_MD5_SIZE];
	char hex[2 * LADX_MD5_SIZE + 1];

	ladx_md5(bytes, size, digest);
	to_hex(digest, hex);
	if (strcmp(hex, expected) != 0)
		test_fail(TEST, "%s: md5 %s, expected %s", what, hex, expected);
}

/* RFC 1321, A.5 "Test suite". */
static void test_rfc1321_suite(void)
{
	static const struct {
		const char *text, *digest;
	} suite[] = {
		{"", "d41d8cd98f00b204e9800998ecf8427e"},
		{"a", "0cc175b9c0f1b6a831c399e269772661"},
		{"abc", "900150983cd24fb0d6963f7d28e17f72"},
		{"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
		{"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
		{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f"},
		{"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
		 "57edf4a22be3c955ac49da2e2107b67a"},
	};
	size_t i;

	for (i = 0; i < sizeof suite / sizeof suite[0]; i++)
		check_md5(suite[i].text, (const uint8_t *)suite[i].text, strlen(suite[i].text), suite[i].digest);
}

/* python3 -c "import hashlib; print(hashlib.md5(bytes((i*7+3)&0xFF for i in range(N))).hexdigest())" */
static void test_padding_boundaries(void)
{
	static const struct {
		size_t size;
		const char *digest;
	} cases[] = {
		{0, "d41d8cd98f00b204e9800998ecf8427e"},	{1, "8666683506aacd900bbd5a74ac4edf68"},
		{55, "52c0e574e1198de5fe3f8f11440dcb1b"},	{56, "46c9907fc908ee68b1e7b8e71286a518"},
		{57, "1c805dd236c35cab25fcb1bc73802c51"},	{63, "a62f6d59e837867693f042f5b8f5a236"},
		{64, "7160b8fb5e9e4023d549c3971fbaeead"},	{65, "70bd662e7aefbda85a0f7244167b7897"},
		{119, "e84905d4214f4d1ca56c2cdcc152b143"}, {120, "e3eb5a6c8669ea01a8c185b8abc8a5dc"},
		{127, "acce2474d6cc8302120d09c818d17ef7"}, {128, "10b2da1a82f16d99a81a7203fe9f02cb"},
		{129, "03fefbbcebe2959cf2270241aafe0250"},
	};
	uint8_t bytes[129], digest[LADX_MD5_SIZE];
	char hex[2 * LADX_MD5_SIZE + 1], what[64];
	LadxMd5 ctx;
	size_t i, split;

	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)(i * 7 + 3);
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		sprintf(what, "%u bytes", (unsigned)cases[i].size);
		check_md5(what, bytes, cases[i].size, cases[i].digest);
		for (split = 0; split <= cases[i].size; split++) {
			ladx_md5_init(&ctx);
			ladx_md5_update(&ctx, bytes, split);
			ladx_md5_update(&ctx, split == cases[i].size ? NULL : bytes + split, cases[i].size - split);
			ladx_md5_final(&ctx, digest);
			to_hex(digest, hex);
			if (strcmp(hex, cases[i].digest) != 0)
				test_fail(TEST, "%u bytes split at %u: md5 %s", (unsigned)cases[i].size, (unsigned)split,
					  hex);
		}
	}
}

static void test_million_a(void)
{
	uint8_t chunk[1000], digest[LADX_MD5_SIZE];
	char hex[2 * LADX_MD5_SIZE + 1];
	LadxMd5 ctx;
	int i;

	memset(chunk, 'a', sizeof chunk);
	ladx_md5_init(&ctx);
	for (i = 0; i < 1000; i++)
		ladx_md5_update(&ctx, chunk, sizeof chunk);
	ladx_md5_final(&ctx, digest);
	to_hex(digest, hex);
	if (strcmp(hex, "7707d6ae4e027c70eea2a935c2296f21") != 0)
		test_fail(TEST, "one million 'a': md5 %s", hex);
}

int main(void)
{
	test_rfc1321_suite();
	test_padding_boundaries();
	test_million_a();
	printf("%s: RFC 1321's suite, padding boundaries at every split, one million 'a'\n", TEST);
	return 0;
}
