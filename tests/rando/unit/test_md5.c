#include "rando_md5.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_md5"

static void check_md5(const char *what, const uint8_t *bytes, size_t size, const char *expected)
{
	uint8_t digest[RANDO_MD5_SIZE];
	char hex[2 * RANDO_MD5_SIZE + 1];
	size_t i;
	rando_md5(bytes, size, digest);
	for (i = 0; i < RANDO_MD5_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
	if (strcmp(hex, expected) != 0)
		test_fail(TEST, "%s: md5 %s, expected %s", what, hex, expected);
}

static void check_text(const char *text, const char *expected)
{
	check_md5(text, (const uint8_t *)text, strlen(text), expected);
}

static void test_empty_with_null(void)
{
	check_md5("NULL, 0", NULL, 0, "d41d8cd98f00b204e9800998ecf8427e");
}

static void test_rfc1321_suite(void)
{
	check_text("", "d41d8cd98f00b204e9800998ecf8427e");
	check_text("a", "0cc175b9c0f1b6a831c399e269772661");
	check_text("abc", "900150983cd24fb0d6963f7d28e17f72");
	check_text("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
	check_text("abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b");
	check_text("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
		   "d174ab98d277d9f5a5611c2c9f419d9f");
	check_text("1234567890123456789012345678901234567890"
		   "1234567890123456789012345678901234567890",
		   "57edf4a22be3c955ac49da2e2107b67a");
}

/* python3 -c "import hashlib; print(hashlib.md5(bytes((i*7+3)&0xFF for i in range(N))).hexdigest())" */
static void test_padding_boundaries(void)
{
	static const struct {
		size_t size;
		const char *md5;
	} cases[] = {
		{ 55, "52c0e574e1198de5fe3f8f11440dcb1b" },  { 56, "46c9907fc908ee68b1e7b8e71286a518" },
		{ 63, "a62f6d59e837867693f042f5b8f5a236" },  { 64, "7160b8fb5e9e4023d549c3971fbaeead" },
		{ 65, "70bd662e7aefbda85a0f7244167b7897" },  { 119, "e84905d4214f4d1ca56c2cdcc152b143" },
		{ 120, "e3eb5a6c8669ea01a8c185b8abc8a5dc" },
	};
	uint8_t bytes[120];
	size_t i;
	char what[32];
	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)((i * 7 + 3) & 0xFF);
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		sprintf(what, "pattern of %lu bytes", (unsigned long)cases[i].size);
		check_md5(what, bytes, cases[i].size, cases[i].md5);
	}
}

/* python3 -c "import hashlib; print(hashlib.md5(bytes((i*31+(i>>9))&0xFF for i in range((1<<20)+13))).hexdigest())" */
static void test_long_buffer(void)
{
	static uint8_t bytes[(1u << 20) + 13];
	size_t i;
	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)((i * 31 + (i >> 9)) & 0xFF);
	check_md5("1 MiB + 13 bytes", bytes, sizeof bytes, "1b192d8708a82b1f0ceffaeb61a07ffa");
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_empty_with_null();
	test_rfc1321_suite();
	test_padding_boundaries();
	test_long_buffer();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
