#include "rando_rom.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_rom_cosmetics"
#define ROM_SIZE 0x200000u
#define SOURCE_SIZE 0x100000u

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static uint8_t rom_bytes[ROM_SIZE + 1];
static uint8_t expected[ROM_SIZE + 1];
static uint8_t source_bytes[ROM_SIZE];
static uint8_t source_copy[ROM_SIZE];

static RandoRomBuffer rom_buffer(size_t size)
{
	RandoRomBuffer rom;
	rom.bytes = rom_bytes;
	rom.size = size;
	return rom;
}

static uint8_t rom_pattern(size_t i)
{
	return (uint8_t)(((i ^ (i >> 8) ^ (i >> 16)) + 0x33) & 0xFF);
}

/* Fills the ROM and the source with patterns that differ at every offset, and expected with the ROM. */
static void fill(void)
{
	size_t i;
	for (i = 0; i < ROM_SIZE + 1; i++)
		rom_bytes[i] = rom_pattern(i);
	for (i = 0; i < ROM_SIZE; i++)
		source_bytes[i] = (uint8_t)(rom_pattern(i) ^ 0xA5);
	memcpy(expected, rom_bytes, ROM_SIZE + 1);
}

static void set_rom(uint32_t address, uint8_t value)
{
	rom_bytes[address] = value;
	expected[address] = value;
}

/* A byte the call must write: it must change, so that the comparison proves the write. */
static void expect(uint32_t address, uint8_t value)
{
	if (rom_bytes[address] == value)
		test_fail(TEST, "pattern already holds %02X at %06X", value, (unsigned)address);
	expected[address] = value;
}

static void expect_copy(uint32_t address, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++)
		expect(address + (uint32_t)i, source_bytes[address + i]);
}

static void check_rom(int line)
{
	size_t i;
	for (i = 0; i < ROM_SIZE + 1; i++)
		if (rom_bytes[i] != expected[i])
			test_fail(TEST, "line %d: %06X is %02X, expected %02X", line, (unsigned)i, rom_bytes[i],
				  expected[i]);
}

static void run(int line)
{
	RandoRomBuffer rom = rom_buffer(ROM_SIZE);
	memcpy(source_copy, source_bytes, ROM_SIZE);
	if (!rando_rom_write_cosmetic_defaults(&rom, source_bytes, SOURCE_SIZE))
		test_fail(TEST, "line %d: refused a 2 MiB ROM and a 1 MiB source", line);
	CHECK(memcmp(source_bytes, source_copy, ROM_SIZE) == 0);
	check_rom(line);
}

static void expect_settings(void)
{
	expect(0x18004B, 0x00);
	expect(0x18021A, 0x00);
	expect(0x18017F, 0x00);
	expect(0x180048, 0x08);
	expect(0x6DD9A, 0x11);
	expect(0x6DF2A, 0x12);
	expect(0x6E0E9, 0x12);
}

static void expect_heart_color(uint8_t version)
{
	uint32_t address;
	if (version <= 0x03 || version == 0xFF) {
		for (address = 0x6FA1E; address <= 0x6FA30; address += 2)
			expect(address, 0x24);
		expect(0x65561, 0x05);
	} else {
		expect(0x187020, 0x00);
	}
}

static void expect_sprite(void)
{
	expect_copy(0x80000, 0x7000);
	expect_copy(0xDD308, 120);
	expect_copy(0xDEDF5, 4);
}

/* Every write at VT's defaults, on a ROM whose version byte at 0x7FE2 is version. */
static void check_defaults(uint8_t version, int line)
{
	fill();
	set_rom(0x7FE2, version);
	expect_settings();
	expect_heart_color(version);
	expect_sprite();
	run(line);
}

static void test_heart_color_branches(void)
{
	static const uint8_t versions[] = {0x00, 0x03, 0xFF, 0x04, 0x05, 0xFE};
	size_t i;
	for (i = 0; i < sizeof versions / sizeof versions[0]; i++)
		check_defaults(versions[i], __LINE__);
}

static void test_sprite_edges(void)
{
	static const uint32_t kept[] = {0x7FFFF, 0x87000, 0xDD307, 0xDD380, 0xDEDF4, 0xDEDF9};
	size_t i;
	check_defaults(0x04, __LINE__);
	CHECK(memcmp(rom_bytes + 0x80000, source_bytes + 0x80000, 0x7000) == 0);
	CHECK(memcmp(rom_bytes + 0xDD308, source_bytes + 0xDD308, 120) == 0);
	CHECK(memcmp(rom_bytes + 0xDEDF5, source_bytes + 0xDEDF5, 4) == 0);
	for (i = 0; i < sizeof kept / sizeof kept[0]; i++)
		CHECK(rom_bytes[kept[i]] == rom_pattern(kept[i]));
}

static const uint32_t guard_address[4] = {0x118000, 0x118001, 0x11801E, 0x11801F};
static const uint8_t guard_value[4] = {0x02, 0x37, 0x02, 0x37};

/* center-align 1.0.1 pads "NINTENDO" with ten blanks on both sides, to 28 cells. */
static void expect_author(void)
{
	static const uint8_t hi[8] = {0x6A, 0x65, 0x6A, 0x70, 0x61, 0x6A, 0x60, 0x6B};
	static const uint8_t lo[8] = {0x90, 0x8B, 0x90, 0x96, 0x87, 0x90, 0x86, 0x91};
	uint32_t i;
	for (i = 0; i < 28; i++) {
		int blank = i < 10 || i >= 18;
		expect(0x118002 + i, blank ? 0x9F : hi[i - 10]);
		expect(0x118020 + i, blank ? 0x9F : lo[i - 10]);
	}
}

/* With the guard bytes set, byte broken (0..3) off by one, or none broken when broken is 4. */
static void check_guard(int broken, int line)
{
	int k;
	fill();
	set_rom(0x7FE2, 0x04);
	for (k = 0; k < 4; k++)
		set_rom(guard_address[k], (uint8_t)(guard_value[k] + (k == broken)));
	expect_settings();
	expect_heart_color(0x04);
	if (broken == 4)
		expect_author();
	expect_sprite();
	run(line);
}

static void test_author_guard(void)
{
	int broken;
	for (broken = 0; broken <= 4; broken++)
		check_guard(broken, __LINE__);
}

static void test_wrong_sizes(void)
{
	static const struct {
		size_t rom;
		size_t source;
	} sizes[] = {
		{0x100000u, SOURCE_SIZE},
		{ROM_SIZE + 1, SOURCE_SIZE},
		{ROM_SIZE, SOURCE_SIZE - 1},
		{ROM_SIZE, ROM_SIZE},
	};
	size_t i;
	fill();
	for (i = 0; i < 4; i++)
		set_rom(guard_address[i], guard_value[i]);
	memcpy(source_copy, source_bytes, ROM_SIZE);
	for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
		RandoRomBuffer rom = rom_buffer(sizes[i].rom);
		CHECK(!rando_rom_write_cosmetic_defaults(&rom, source_bytes, sizes[i].source));
		check_rom(__LINE__);
		CHECK(memcmp(source_bytes, source_copy, ROM_SIZE) == 0);
	}
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_heart_color_branches();
	test_sprite_edges();
	test_author_guard();
	test_wrong_sizes();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
