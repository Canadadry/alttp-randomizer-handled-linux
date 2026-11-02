#include "rando_crc32.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>

#define TEST "test_crc32"

static void check_crc(const char *what, const uint8_t *bytes, size_t size, uint32_t expected)
{
	uint32_t got = rando_crc32(bytes, size);
	if (got != expected)
		test_fail(TEST, "%s: crc32 %08lX, expected %08lX", what, (unsigned long)got,
			  (unsigned long)expected);
}

static void test_empty_is_zero(void)
{
	check_crc("NULL, 0", NULL, 0, 0x00000000u);
}

static void test_zlib_check_values(void)
{
	check_crc("\"a\"", (const uint8_t *)"a", 1, 0xE8B7BE43u);
	check_crc("\"123456789\"", (const uint8_t *)"123456789", 9, 0xCBF43926u);
}

/* python3 -c "import zlib; print(hex(zlib.crc32(bytes(range(256)))))" */
static void test_every_byte_value(void)
{
	uint8_t bytes[256];
	size_t i;
	for (i = 0; i < sizeof bytes; i++)
		bytes[i] = (uint8_t)i;
	check_crc("0..255", bytes, sizeof bytes, 0x29058C73u);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_empty_is_zero();
	test_zlib_check_values();
	test_every_byte_value();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
