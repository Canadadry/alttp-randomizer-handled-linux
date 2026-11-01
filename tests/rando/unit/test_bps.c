#include "rando_bps.h"
#include "rando_crc32.h"
#include "support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_bps"
#define PATCH_MAX 256

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef struct Patch {
	uint8_t bytes[PATCH_MAX];
	size_t size;
} Patch;

static void put_bytes(Patch *p, const void *bytes, size_t count)
{
	CHECK(p->size + count <= PATCH_MAX);
	memcpy(p->bytes + p->size, bytes, count);
	p->size += count;
}

static void put_byte(Patch *p, uint8_t byte)
{
	put_bytes(p, &byte, 1);
}

static void put_u32(Patch *p, uint32_t v)
{
	uint8_t b[4];
	b[0] = (uint8_t)v;
	b[1] = (uint8_t)(v >> 8);
	b[2] = (uint8_t)(v >> 16);
	b[3] = (uint8_t)(v >> 24);
	put_bytes(p, b, 4);
}

static void put_number(Patch *p, uint64_t v)
{
	for (;;) {
		uint8_t x = (uint8_t)(v & 0x7F);
		v >>= 7;
		if (v == 0) {
			put_byte(p, (uint8_t)(0x80 | x));
			return;
		}
		put_byte(p, x);
		v--;
	}
}

static Patch patch_start(void)
{
	Patch p;
	p.size = 0;
	put_bytes(&p, "BPS1", 4);
	return p;
}

/* Magic, then the three header numbers and the metadata. */
static Patch patch_header(uint64_t source_size, uint64_t target_size, const char *metadata)
{
	Patch p = patch_start();
	put_number(&p, source_size);
	put_number(&p, target_size);
	put_number(&p, strlen(metadata));
	put_bytes(&p, metadata, strlen(metadata));
	return p;
}

static void patch_footer(Patch *p, uint32_t source_crc, uint32_t target_crc)
{
	put_u32(p, source_crc);
	put_u32(p, target_crc);
	put_u32(p, rando_crc32(p->bytes, p->size));
}

static void check_sizes(const char *what, const uint8_t *number, size_t count, size_t expected)
{
	Patch p = patch_start();
	RandoBpsHeader h;
	RandoBpsResult r;

	put_bytes(&p, number, count);
	put_bytes(&p, number, count);
	put_byte(&p, 0x80);
	patch_footer(&p, 0, 0);
	r = rando_bps_read_header(p.bytes, p.size, &h);
	if (r != RANDO_BPS_OK || h.source_size != expected || h.target_size != expected)
		test_fail(TEST, "%s: result %d, sizes %lu %lu, expected %lu", what, (int)r,
			  (unsigned long)h.source_size, (unsigned long)h.target_size, (unsigned long)expected);
}

static void test_header_sizes_decode(void)
{
	static const uint8_t n0[] = {0x80}, n1[] = {0x81}, n127[] = {0xFF}, n128[] = {0x00, 0x80},
			     n255[] = {0x7F, 0x80}, n1m[] = {0x00, 0x7F, 0xBE}, n2m[] = {0x00, 0x7F, 0xFE};
	check_sizes("80", n0, sizeof n0, 0);
	check_sizes("81", n1, sizeof n1, 1);
	check_sizes("FF", n127, sizeof n127, 127);
	check_sizes("00 80", n128, sizeof n128, 128);
	check_sizes("7F 80", n255, sizeof n255, 255);
	check_sizes("00 7F BE", n1m, sizeof n1m, 1048576);
	check_sizes("00 7F FE", n2m, sizeof n2m, 2097152);
}

static RandoBpsHeader read_ok(const Patch *p)
{
	RandoBpsHeader h;
	RandoBpsResult r = rando_bps_read_header(p->bytes, p->size, &h);
	if (r != RANDO_BPS_OK)
		test_fail(TEST, "read_header: result %d, expected OK", (int)r);
	return h;
}

static void test_header_metadata_span(void)
{
	Patch empty = patch_header(3, 5, ""), with = patch_header(3, 5, "{\"a\":1}");
	RandoBpsHeader h;

	patch_footer(&empty, 0, 0);
	h = read_ok(&empty);
	CHECK(h.metadata_offset == 7 && h.metadata_size == 0 && h.actions_offset == 7);
	patch_footer(&with, 0, 0);
	h = read_ok(&with);
	CHECK(h.metadata_offset == 7 && h.metadata_size == 7 && h.actions_offset == 14);
	CHECK(memcmp(with.bytes + h.metadata_offset, "{\"a\":1}", 7) == 0);
}

static void test_header_footer_crcs(void)
{
	Patch p = patch_header(3, 5, "");
	RandoBpsHeader h;

	patch_footer(&p, 0xDEADBEEFu, 0x01234567u);
	h = read_ok(&p);
	CHECK(h.source_crc == 0xDEADBEEFu);
	CHECK(h.target_crc == 0x01234567u);
	CHECK(h.patch_crc == rando_crc32(p.bytes, p.size - 4));
}

/* Patch CRC32 from Python: zlib.crc32(b"BPS1\x00\x7f\xbe\x00\x7f\xfe\x80"
 * + struct.pack("<II", 0x11223344, 0x55667788)) == 0xA72E4556. */
static void test_header_base_patch_sizes_literal(void)
{
	static const uint8_t bytes[] = {
		'B', 'P', 'S', '1', 0x00, 0x7F, 0xBE, 0x00, 0x7F, 0xFE, 0x80,
		0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x56, 0x45, 0x2E, 0xA7,
	};
	RandoBpsHeader h;

	CHECK(rando_bps_read_header(bytes, sizeof bytes, &h) == RANDO_BPS_OK);
	CHECK(h.source_size == 1048576 && h.target_size == 2097152);
	CHECK(h.metadata_size == 0 && h.actions_offset == 11);
	CHECK(h.source_crc == 0x11223344u && h.target_crc == 0x55667788u && h.patch_crc == 0xA72E4556u);
}

static void check_header_result(const char *what, const uint8_t *bytes, size_t size, RandoBpsResult expected)
{
	RandoBpsHeader h, untouched;
	RandoBpsResult r;

	memset(&h, 0xA5, sizeof h);
	untouched = h;
	r = rando_bps_read_header(bytes, size, &h);
	if (r != expected)
		test_fail(TEST, "%s: result %d, expected %d", what, (int)r, (int)expected);
	if (memcmp(&h, &untouched, sizeof h) != 0)
		test_fail(TEST, "%s: header written on failure", what);
}

static void test_header_not_bps(void)
{
	Patch p = patch_header(3, 5, "");
	patch_footer(&p, 0, 0);
	check_header_result("3 bytes", (const uint8_t *)"BPS", 3, RANDO_BPS_NOT_BPS);
	p.bytes[3] = '2';
	check_header_result("BPS2", p.bytes, p.size, RANDO_BPS_NOT_BPS);
}

static void test_header_shorter_than_minimum(void)
{
	Patch p = patch_header(3, 5, "");
	patch_footer(&p, 0, 0);
	check_header_result("magic only", p.bytes, 4, RANDO_BPS_MALFORMED);
	check_header_result("magic and numbers", p.bytes, 7, RANDO_BPS_MALFORMED);
	p.size = 0;
	put_bytes(&p, "BPS1", 4);
	put_byte(&p, 0x80);
	put_byte(&p, 0x80);
	patch_footer(&p, 0, 0);
	check_header_result("two numbers then a valid footer", p.bytes, p.size, RANDO_BPS_MALFORMED);
}

static void test_header_number_cut_by_footer(void)
{
	Patch p = patch_start();
	put_byte(&p, 0x80);
	put_byte(&p, 0x80);
	put_byte(&p, 0x00);
	patch_footer(&p, 0x000000FFu, 0);
	check_header_result("metadata length continued into the footer", p.bytes, p.size, RANDO_BPS_MALFORMED);
}

static void test_header_number_overflows(void)
{
	Patch p = patch_start();
	int i;
	for (i = 0; i < 11; i++)
		put_byte(&p, 0x00);
	put_byte(&p, 0x80);
	put_byte(&p, 0x80);
	put_byte(&p, 0x80);
	patch_footer(&p, 0, 0);
	check_header_result("eleven 00 then 80", p.bytes, p.size, RANDO_BPS_MALFORMED);
}

static void test_header_metadata_runs_into_footer(void)
{
	Patch p = patch_start();
	put_number(&p, 3);
	put_number(&p, 5);
	put_number(&p, 4);
	put_bytes(&p, "abc", 3);
	patch_footer(&p, 0, 0);
	check_header_result("metadata one byte into the footer", p.bytes, p.size, RANDO_BPS_MALFORMED);
	p = patch_start();
	put_number(&p, 3);
	put_number(&p, 5);
	put_number(&p, (uint64_t)SIZE_MAX);
	patch_footer(&p, 0, 0);
	check_header_result("metadata of SIZE_MAX bytes", p.bytes, p.size, RANDO_BPS_MALFORMED);
}

static void test_header_patch_crc(void)
{
	Patch p = patch_header(3, 5, "meta");
	patch_footer(&p, 0x11111111u, 0x22222222u);
	p.bytes[p.size - 1] ^= 0x10;
	check_header_result("flipped footer bit", p.bytes, p.size, RANDO_BPS_PATCH_CRC);
	p.bytes[p.size - 1] ^= 0x10;
	p.bytes[8] ^= 0x01;
	check_header_result("flipped metadata byte", p.bytes, p.size, RANDO_BPS_PATCH_CRC);
	p.bytes[8] ^= 0x01;
	p.bytes[p.size - 12] ^= 0x01;
	check_header_result("flipped source crc byte", p.bytes, p.size, RANDO_BPS_PATCH_CRC);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_header_sizes_decode();
	test_header_metadata_span();
	test_header_footer_crcs();
	test_header_base_patch_sizes_literal();
	test_header_not_bps();
	test_header_shorter_than_minimum();
	test_header_number_cut_by_footer();
	test_header_number_overflows();
	test_header_metadata_runs_into_footer();
	test_header_patch_crc();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
