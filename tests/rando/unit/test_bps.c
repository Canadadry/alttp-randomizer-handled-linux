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

#define TARGET_MAX 64
#define SENTINEL 0xEE

static void put_action(Patch *p, unsigned type, size_t length)
{
	put_number(p, (uint64_t)(length - 1) << 2 | type);
}

static void put_offset(Patch *p, long delta)
{
	put_number(p, delta < 0 ? (uint64_t)(-delta) << 1 | 1 : (uint64_t)delta << 1);
}

/* Footer with the source's real CRC32 and the expected output's. */
static void patch_finish(Patch *p, const uint8_t *source, size_t source_size, const uint8_t *expected,
			 size_t expected_size)
{
	patch_footer(p, rando_crc32(source, source_size), rando_crc32(expected, expected_size));
}

/* Applies into a sentinel-filled buffer of TARGET_MAX bytes; checks the result, the reported size,
 * the output bytes on OK, and that nothing past the declared size was written. */
static void check_apply_cap(const char *what, const Patch *p, const uint8_t *source, size_t source_size,
			    size_t capacity, RandoBpsResult expected, const uint8_t *output, size_t output_size)
{
	uint8_t target[TARGET_MAX];
	size_t size = 12345, i, from;
	RandoBpsResult r;

	CHECK(capacity <= TARGET_MAX);
	memset(target, SENTINEL, sizeof target);
	r = rando_bps_apply(p->bytes, p->size, source, source_size, target, capacity, &size);
	if (r != expected)
		test_fail(TEST, "%s: result %d, expected %d", what, (int)r, (int)expected);
	if (expected != RANDO_BPS_OK) {
		if (size != 0)
			test_fail(TEST, "%s: target size %lu on failure", what, (unsigned long)size);
		from = capacity;
	} else {
		if (size != output_size)
			test_fail(TEST, "%s: target size %lu, expected %lu", what, (unsigned long)size,
				  (unsigned long)output_size);
		for (i = 0; i < output_size; i++)
			if (target[i] != output[i])
				test_fail(TEST, "%s: byte %lu is %02X, expected %02X", what, (unsigned long)i,
					  target[i], output[i]);
		from = output_size;
	}
	for (i = from; i < TARGET_MAX; i++)
		if (target[i] != SENTINEL)
			test_fail(TEST, "%s: byte %lu past the target written", what, (unsigned long)i);
}

static void check_apply(const char *what, const Patch *p, const uint8_t *source, size_t source_size,
			RandoBpsResult expected, const uint8_t *output, size_t output_size)
{
	check_apply_cap(what, p, source, source_size, TARGET_MAX, expected, output, output_size);
}

static const uint8_t SOURCE[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

static void test_apply_source_read_uses_output_offset(void)
{
	static const uint8_t expected[] = {0xA0, 0x11, 0x12, 0x13};
	Patch p = patch_header(8, 4, "");
	put_action(&p, 1, 1);
	put_byte(&p, 0xA0);
	put_action(&p, 0, 3);
	patch_finish(&p, SOURCE, 8, expected, 4);
	check_apply("source read", &p, SOURCE, 8, RANDO_BPS_OK, expected, 4);
}

static void test_apply_source_copy_offset_persists(void)
{
	static const uint8_t expected[] = {0x15, 0x16, 0x12, 0x13, 0x14, 0x13};
	Patch p = patch_header(8, 6, "");
	put_action(&p, 2, 2);
	put_offset(&p, 5);
	put_action(&p, 2, 3);
	put_offset(&p, -5);
	put_action(&p, 2, 1);
	put_offset(&p, -2);
	patch_finish(&p, SOURCE, 8, expected, 6);
	check_apply("source copy +5 then -5 then -2", &p, SOURCE, 8, RANDO_BPS_OK, expected, 6);
}

/* SourceCopy reads the source, never the output written over the same offset. */
static void test_apply_source_copy_below_output_reads_source(void)
{
	static const uint8_t expected[] = {0xA0, 0xA1, 0xA2, 0x10, 0x11, 0x12};
	Patch p = patch_header(8, 6, "");
	put_action(&p, 1, 3);
	put_bytes(&p, "\xA0\xA1\xA2", 3);
	put_action(&p, 2, 3);
	put_offset(&p, 0);
	patch_finish(&p, SOURCE, 8, expected, 6);
	check_apply("source copy back to offset 0", &p, SOURCE, 8, RANDO_BPS_OK, expected, 6);
}

static void test_apply_target_copy(void)
{
	static const uint8_t expected[] = {0xA0, 0xA1, 0xA2, 0xA1, 0xA2, 0xA0};
	Patch p = patch_header(8, 6, "");
	put_action(&p, 1, 3);
	put_bytes(&p, "\xA0\xA1\xA2", 3);
	put_action(&p, 3, 2);
	put_offset(&p, 1);
	put_action(&p, 3, 1);
	put_offset(&p, -3);
	patch_finish(&p, SOURCE, 8, expected, 6);
	check_apply("target copy, no overlap", &p, SOURCE, 8, RANDO_BPS_OK, expected, 6);
}

static void test_apply_target_copy_overlap_repeats(void)
{
	static const uint8_t expected[] = {0xA0, 0xA1, 0xA0, 0xA1, 0xA0, 0xA1, 0xA0};
	Patch p = patch_header(8, 7, "");
	put_action(&p, 1, 2);
	put_bytes(&p, "\xA0\xA1", 2);
	put_action(&p, 3, 5);
	put_offset(&p, 0);
	patch_finish(&p, SOURCE, 8, expected, 7);
	check_apply("target copy, overlapping", &p, SOURCE, 8, RANDO_BPS_OK, expected, 7);
}

static void test_apply_copy_offsets_independent(void)
{
	static const uint8_t expected[] = {0xA0, 0xA1, 0x14, 0x15, 0xA1, 0x16, 0x14};
	Patch p = patch_header(8, 7, "");
	put_action(&p, 1, 2);
	put_bytes(&p, "\xA0\xA1", 2);
	put_action(&p, 2, 2);
	put_offset(&p, 4);
	put_action(&p, 3, 1);
	put_offset(&p, 1);
	put_action(&p, 2, 1);
	put_offset(&p, 0);
	put_action(&p, 3, 1);
	put_offset(&p, 0);
	patch_finish(&p, SOURCE, 8, expected, 7);
	check_apply("source and target copy interleaved", &p, SOURCE, 8, RANDO_BPS_OK, expected, 7);
}

static void test_apply_empty_target(void)
{
	Patch p = patch_header(8, 0, "");
	patch_finish(&p, SOURCE, 8, NULL, 0);
	check_apply("empty target", &p, SOURCE, 8, RANDO_BPS_OK, NULL, 0);
	check_apply_cap("empty target, capacity 0", &p, SOURCE, 8, 0, RANDO_BPS_OK, NULL, 0);
}

/* TargetRead A0, then SourceRead of 3: 8-byte source, 4-byte target. */
static Patch valid_patch(void)
{
	static const uint8_t expected[] = {0xA0, 0x11, 0x12, 0x13};
	Patch p = patch_header(8, 4, "");
	put_action(&p, 1, 1);
	put_byte(&p, 0xA0);
	put_action(&p, 0, 3);
	patch_finish(&p, SOURCE, 8, expected, 4);
	return p;
}

/* Rewrites one footer CRC32 and recomputes the patch CRC32, so only that check fails. */
static void set_footer_crc(Patch *p, size_t index, uint32_t crc)
{
	size_t at = p->size - 12 + index * 4;
	uint32_t patch_crc;
	p->bytes[at] = (uint8_t)crc;
	p->bytes[at + 1] = (uint8_t)(crc >> 8);
	p->bytes[at + 2] = (uint8_t)(crc >> 16);
	p->bytes[at + 3] = (uint8_t)(crc >> 24);
	patch_crc = rando_crc32(p->bytes, p->size - 4);
	p->bytes[p->size - 4] = (uint8_t)patch_crc;
	p->bytes[p->size - 3] = (uint8_t)(patch_crc >> 8);
	p->bytes[p->size - 2] = (uint8_t)(patch_crc >> 16);
	p->bytes[p->size - 1] = (uint8_t)(patch_crc >> 24);
}

static void test_apply_source_size(void)
{
	static const uint8_t long_source[9] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
	Patch p = valid_patch();
	check_apply("source one byte short", &p, SOURCE, 7, RANDO_BPS_SOURCE_SIZE, NULL, 0);
	check_apply("source one byte long", &p, long_source, 9, RANDO_BPS_SOURCE_SIZE, NULL, 0);
}

static void test_apply_source_crc(void)
{
	uint8_t source[8];
	Patch p = valid_patch();
	memcpy(source, SOURCE, 8);
	source[7] ^= 0x01;
	check_apply("source byte changed", &p, source, 8, RANDO_BPS_SOURCE_CRC, NULL, 0);
}

static void test_apply_target_too_small(void)
{
	Patch p = valid_patch();
	check_apply_cap("capacity one below", &p, SOURCE, 8, 3, RANDO_BPS_TARGET_TOO_SMALL, NULL, 0);
}

static void test_apply_target_crc(void)
{
	Patch p = valid_patch();
	set_footer_crc(&p, 1, 0x12345678u);
	check_apply("target crc changed", &p, SOURCE, 8, RANDO_BPS_TARGET_CRC, NULL, 0);
}

static void check_malformed(const char *what, Patch *p, size_t source_size)
{
	patch_footer(p, rando_crc32(SOURCE, source_size), 0);
	check_apply(what, p, SOURCE, source_size, RANDO_BPS_MALFORMED, NULL, 0);
}

static void test_apply_action_number_cut_by_footer(void)
{
	Patch p = patch_header(8, 4, "");
	put_byte(&p, 0x00);
	check_malformed("action number continued into the footer", &p, 8);
	p = patch_header(8, 4, "");
	put_action(&p, 2, 4);
	check_malformed("source copy offset missing", &p, 8);
	p = patch_header(8, 4, "");
	put_action(&p, 1, 1);
	put_byte(&p, 0xA0);
	put_action(&p, 3, 3);
	put_byte(&p, 0x00);
	check_malformed("target copy offset continued into the footer", &p, 8);
}

static void test_apply_target_read_into_footer(void)
{
	Patch p = patch_header(8, 4, "");
	put_action(&p, 1, 4);
	put_bytes(&p, "\xA0\xA1", 2);
	check_malformed("target read of 4 with 2 bytes left", &p, 8);
}

/* Capacity equals the declared size, so a write past it reaches the sentinel. */
static void test_apply_write_past_declared_size(void)
{
	Patch p = patch_header(8, 3, "");
	put_action(&p, 0, 4);
	patch_footer(&p, rando_crc32(SOURCE, 8), 0);
	check_apply_cap("source read of 4 into 3", &p, SOURCE, 8, 3, RANDO_BPS_MALFORMED, NULL, 0);
	p = patch_header(8, 3, "");
	put_action(&p, 1, 2);
	put_bytes(&p, "\xA0\xA1", 2);
	put_action(&p, 3, 2);
	put_offset(&p, 0);
	patch_footer(&p, rando_crc32(SOURCE, 8), 0);
	check_apply_cap("target copy of 2 at 2 into 3", &p, SOURCE, 8, 3, RANDO_BPS_MALFORMED, NULL, 0);
}

static void test_apply_too_few_bytes(void)
{
	Patch p = patch_header(8, 5, "");
	put_action(&p, 0, 4);
	check_malformed("4 bytes produced, 5 declared", &p, 8);
}

static void test_apply_source_read_past_source(void)
{
	Patch p = patch_header(4, 6, "");
	put_action(&p, 0, 5);
	put_action(&p, 1, 1);
	put_byte(&p, 0xA0);
	check_malformed("source read of 5 from 4", &p, 4);
	p = patch_header(4, 6, "");
	put_action(&p, 1, 5);
	put_bytes(&p, "\xA0\xA1\xA2\xA3\xA4", 5);
	put_action(&p, 0, 1);
	check_malformed("source read at 5 from 4", &p, 4);
}

static void test_apply_source_copy_negative(void)
{
	Patch p = patch_header(8, 4, "");
	put_action(&p, 2, 4);
	put_offset(&p, -1);
	check_malformed("source copy at -1", &p, 8);
}

static void test_apply_source_copy_past_source(void)
{
	Patch p = patch_header(8, 4, "");
	put_action(&p, 2, 3);
	put_offset(&p, 6);
	put_action(&p, 1, 1);
	put_byte(&p, 0xA0);
	check_malformed("source copy of 3 at 6 from 8", &p, 8);
	p = patch_header(8, 1, "");
	put_action(&p, 2, 1);
	put_number(&p, (uint64_t)SIZE_MAX - 1);
	check_malformed("source copy at SIZE_MAX / 2", &p, 8);
}

static void test_apply_target_copy_negative(void)
{
	Patch p = patch_header(8, 4, "");
	put_action(&p, 1, 2);
	put_bytes(&p, "\xA0\xA1", 2);
	put_action(&p, 3, 2);
	put_offset(&p, -1);
	check_malformed("target copy at -1", &p, 8);
}

static void test_apply_target_copy_at_output(void)
{
	Patch p = patch_header(8, 4, "");
	put_action(&p, 1, 2);
	put_bytes(&p, "\xA0\xA1", 2);
	put_action(&p, 3, 2);
	put_offset(&p, 2);
	check_malformed("target copy at the output offset", &p, 8);
	p = patch_header(8, 2, "");
	put_action(&p, 3, 2);
	put_offset(&p, 0);
	check_malformed("target copy with nothing written", &p, 8);
}

static void check_apply_bytes(const char *what, const uint8_t *bytes, size_t size, RandoBpsResult expected)
{
	Patch p;
	CHECK(size <= PATCH_MAX);
	memcpy(p.bytes, bytes, size);
	p.size = size;
	check_apply(what, &p, SOURCE, 8, expected, NULL, 0);
}

static void test_apply_header_faults(void)
{
	Patch p = valid_patch();
	int i;

	check_apply_bytes("3 bytes", (const uint8_t *)"BPS", 3, RANDO_BPS_NOT_BPS);
	p.bytes[3] = '2';
	check_apply_bytes("BPS2", p.bytes, p.size, RANDO_BPS_NOT_BPS);
	check_apply_bytes("magic only", (const uint8_t *)"BPS1", 4, RANDO_BPS_MALFORMED);
	p = patch_start();
	for (i = 0; i < 11; i++)
		put_byte(&p, 0x00);
	put_bytes(&p, "\x80\x80\x80", 3);
	patch_footer(&p, rando_crc32(SOURCE, 8), 0);
	check_apply_bytes("overflowing header number", p.bytes, p.size, RANDO_BPS_MALFORMED);
	p = patch_start();
	put_bytes(&p, "\x88\x84\x84" "abc", 6);
	patch_footer(&p, rando_crc32(SOURCE, 8), 0);
	check_apply_bytes("metadata into the footer", p.bytes, p.size, RANDO_BPS_MALFORMED);
	p = valid_patch();
	p.bytes[p.size - 1] ^= 0x10;
	check_apply_bytes("patch crc", p.bytes, p.size, RANDO_BPS_PATCH_CRC);
}

static void test_apply_check_order(void)
{
	uint8_t source[8];
	Patch p = valid_patch();

	memcpy(source, SOURCE, 8);
	source[0] ^= 0x01;
	set_footer_crc(&p, 1, 0x12345678u);
	check_apply("bad source crc and bad target crc", &p, source, 8, RANDO_BPS_SOURCE_CRC, NULL, 0);
	check_apply_cap("bad source crc and capacity too small", &p, source, 8, 3, RANDO_BPS_SOURCE_CRC, NULL, 0);
	check_apply("wrong size and wrong bytes", &p, source, 7, RANDO_BPS_SOURCE_SIZE, NULL, 0);
	p.bytes[p.size - 1] ^= 0x10;
	check_apply("wrong size source and bad patch crc", &p, SOURCE, 7, RANDO_BPS_PATCH_CRC, NULL, 0);
	p = patch_header(8, 4, "");
	put_action(&p, 0, 9);
	check_malformed("source read past the source with capacity 64", &p, 8);
	check_apply_cap("malformed actions and capacity too small", &p, SOURCE, 8, 3, RANDO_BPS_TARGET_TOO_SMALL,
			NULL, 0);
}

/* Built by hand, independent of put_number and rando_crc32: metadata "{}", SourceRead 2,
 * TargetRead A0 A1, SourceCopy 2 at +5, SourceCopy 1 at -4, TargetCopy 3 at +5 (overlapping).
 * CRC32s from Python: s = zlib.crc32(bytes(range(0x10, 0x18))) == 0xEBB3A6B9,
 * t = zlib.crc32(bytes.fromhex("1011a0a1151613161316")) == 0x4789A280,
 * zlib.crc32(b"BPS1\x88\x8a\x82{}\x84\x85\xa0\xa1\x86\x8a\x82\x89\x8b\x8a"
 * + struct.pack("<II", s, t)) == 0x22E32B0C. */
static void test_apply_literal_patch(void)
{
	static const uint8_t bytes[] = {
		0x42, 0x50, 0x53, 0x31, 0x88, 0x8A, 0x82, 0x7B, 0x7D, 0x84, 0x85, 0xA0, 0xA1, 0x86, 0x8A, 0x82,
		0x89, 0x8B, 0x8A, 0xB9, 0xA6, 0xB3, 0xEB, 0x80, 0xA2, 0x89, 0x47, 0x0C, 0x2B, 0xE3, 0x22,
	};
	static const uint8_t expected[] = {0x10, 0x11, 0xA0, 0xA1, 0x15, 0x16, 0x13, 0x16, 0x13, 0x16};
	Patch p;

	memcpy(p.bytes, bytes, sizeof bytes);
	p.size = sizeof bytes;
	check_apply("literal patch", &p, SOURCE, 8, RANDO_BPS_OK, expected, sizeof expected);
	check_apply_cap("literal patch, exact capacity", &p, SOURCE, 8, sizeof expected, RANDO_BPS_OK, expected,
			sizeof expected);
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
	test_apply_source_read_uses_output_offset();
	test_apply_source_copy_offset_persists();
	test_apply_source_copy_below_output_reads_source();
	test_apply_target_copy();
	test_apply_target_copy_overlap_repeats();
	test_apply_copy_offsets_independent();
	test_apply_empty_target();
	test_apply_source_size();
	test_apply_source_crc();
	test_apply_target_too_small();
	test_apply_target_crc();
	test_apply_action_number_cut_by_footer();
	test_apply_target_read_into_footer();
	test_apply_write_past_declared_size();
	test_apply_too_few_bytes();
	test_apply_source_read_past_source();
	test_apply_source_copy_negative();
	test_apply_source_copy_past_source();
	test_apply_target_copy_negative();
	test_apply_target_copy_at_output();
	test_apply_header_faults();
	test_apply_check_order();
	test_apply_literal_patch();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
