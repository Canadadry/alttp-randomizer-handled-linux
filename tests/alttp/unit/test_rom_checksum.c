#include "rando_rom.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_rom_checksum"
#define ROM_SIZE 0x200000u
#define HEADER 0x7FDCu

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static uint8_t rom_bytes[ROM_SIZE + 1];
static uint8_t before[ROM_SIZE + 1];

static RandoRomBuffer rom_buffer(size_t size)
{
	RandoRomBuffer rom;
	rom.bytes = rom_bytes;
	rom.size = size;
	return rom;
}

static void check_header(const uint8_t expected[4])
{
	if (memcmp(rom_bytes + HEADER, expected, 4) != 0)
		test_fail(TEST, "header %02X %02X %02X %02X, expected %02X %02X %02X %02X", rom_bytes[HEADER],
			  rom_bytes[HEADER + 1], rom_bytes[HEADER + 2], rom_bytes[HEADER + 3], expected[0],
			  expected[1], expected[2], expected[3]);
}

static void check_outside_header_unchanged(void)
{
	CHECK(memcmp(rom_bytes, before, HEADER) == 0);
	CHECK(memcmp(rom_bytes + HEADER + 4, before + HEADER + 4, ROM_SIZE - HEADER - 4) == 0);
}

static uint16_t written_checksum(void)
{
	return (uint16_t)(rom_bytes[HEADER + 2] | rom_bytes[HEADER + 3] << 8);
}

static void check_sum_property(void)
{
	uint32_t sum = 0;
	size_t i;
	for (i = 0; i < ROM_SIZE; i++)
		sum += rom_bytes[i];
	CHECK((sum & 0xFFFFu) == written_checksum());
	CHECK(rom_bytes[HEADER] + rom_bytes[HEADER + 2] == 0xFF);
	CHECK(rom_bytes[HEADER + 1] + rom_bytes[HEADER + 3] == 0xFF);
}

/* Fixes the checksum of the buffer already filled, then checks every property against expected. */
static void check_buffer(const uint8_t expected[4])
{
	uint8_t first[4];
	RandoRomBuffer rom = rom_buffer(ROM_SIZE);
	memcpy(before, rom_bytes, ROM_SIZE);
	CHECK(rando_rom_update_checksum(&rom));
	check_header(expected);
	check_outside_header_unchanged();
	check_sum_property();
	memcpy(first, rom_bytes + HEADER, 4);
	CHECK(rando_rom_update_checksum(&rom));
	CHECK(memcmp(rom_bytes + HEADER, first, 4) == 0);
	check_outside_header_unchanged();
}

static void set_header(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
	rom_bytes[HEADER] = b0;
	rom_bytes[HEADER + 1] = b1;
	rom_bytes[HEADER + 2] = b2;
	rom_bytes[HEADER + 3] = b3;
}

static void test_all_zero(void)
{
	static const uint8_t expected[4] = {0x01, 0xFE, 0xFE, 0x01};
	memset(rom_bytes, 0x00, ROM_SIZE);
	check_buffer(expected);
}

static void test_all_ff(void)
{
	static const uint8_t expected[4] = {0xFD, 0x01, 0x02, 0xFE};
	memset(rom_bytes, 0xFF, ROM_SIZE);
	check_buffer(expected);
}

static void test_position_pattern(void)
{
	static const uint8_t expected[4] = {0x77, 0x01, 0x88, 0xFE};
	uint32_t closed = 8192u * 32640u - (0xDCu + 0xDDu + 0xDEu + 0xDFu) + 0x1FEu;
	uint16_t checksum = (uint16_t)(closed & 0xFFFFu);
	uint16_t inverse = (uint16_t)(checksum ^ 0xFFFFu);
	size_t i;
	for (i = 0; i < ROM_SIZE; i++)
		rom_bytes[i] = (uint8_t)(i & 0xFF);
	set_header(0x12, 0x34, 0x56, 0x78);
	CHECK(closed == 267386504u);
	CHECK(expected[0] == (inverse & 0xFF) && expected[1] == (inverse >> 8));
	CHECK(expected[2] == (checksum & 0xFF) && expected[3] == (checksum >> 8));
	check_buffer(expected);
}

static void test_mixed_pattern(void)
{
	static const uint8_t expected[4] = {0x43, 0xFE, 0xBC, 0x01};
	size_t i;
	for (i = 0; i < ROM_SIZE; i++)
		rom_bytes[i] = (uint8_t)(((i * 7) ^ (i >> 11)) & 0xFF);
	set_header(0xAA, 0xBB, 0xCC, 0xDD);
	check_buffer(expected);
}

static void test_one_byte_changes(void)
{
	static const uint32_t outside[] = {0x0, 0x7FDB, 0x7FE0, 0x1FFFFF};
	static const uint32_t inside[] = {0x7FDC, 0x7FDD, 0x7FDE, 0x7FDF};
	RandoRomBuffer rom = rom_buffer(ROM_SIZE);
	uint16_t base;
	uint8_t header[4];
	size_t i;
	memset(rom_bytes, 0x00, ROM_SIZE);
	CHECK(rando_rom_update_checksum(&rom));
	base = written_checksum();
	memcpy(header, rom_bytes + HEADER, 4);
	for (i = 0; i < sizeof outside / sizeof outside[0]; i++) {
		rom_bytes[outside[i]]++;
		CHECK(rando_rom_update_checksum(&rom));
		CHECK(written_checksum() == (uint16_t)(base + 1));
		rom_bytes[outside[i]]--;
	}
	for (i = 0; i < sizeof inside / sizeof inside[0]; i++) {
		CHECK(rando_rom_update_checksum(&rom));
		rom_bytes[inside[i]]++;
		CHECK(rando_rom_update_checksum(&rom));
		CHECK(memcmp(rom_bytes + HEADER, header, 4) == 0);
	}
}

static void test_wrong_sizes(void)
{
	static const size_t sizes[] = {ROM_SIZE - 1, ROM_SIZE + 1, 0x100000u, 0};
	size_t i, j;
	for (i = 0; i < ROM_SIZE + 1; i++)
		rom_bytes[i] = (uint8_t)((i * 13) & 0xFF);
	memcpy(before, rom_bytes, ROM_SIZE + 1);
	for (j = 0; j < sizeof sizes / sizeof sizes[0]; j++) {
		RandoRomBuffer rom = rom_buffer(sizes[j]);
		CHECK(!rando_rom_update_checksum(&rom));
		CHECK(memcmp(rom_bytes, before, ROM_SIZE + 1) == 0);
	}
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_all_zero();
	test_all_ff();
	test_position_pattern();
	test_mixed_pattern();
	test_one_byte_changes();
	test_wrong_sizes();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
