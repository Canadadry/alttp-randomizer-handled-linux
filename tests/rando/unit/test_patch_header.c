#define _POSIX_C_SOURCE 200809L

#include "rando_assemble.h"
#include "rando_crc32.h"
#include "support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST "test_patch_header"
#define PATCH_MAX 64
#define FILE_MAX (4u << 20)

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef struct Patch {
	uint8_t bytes[PATCH_MAX];
	size_t size;
} Patch;

static void put_byte(Patch *p, uint8_t byte)
{
	CHECK(p->size < PATCH_MAX);
	p->bytes[p->size++] = byte;
}

static void put_u32(Patch *p, uint32_t v)
{
	put_byte(p, (uint8_t)v);
	put_byte(p, (uint8_t)(v >> 8));
	put_byte(p, (uint8_t)(v >> 16));
	put_byte(p, (uint8_t)(v >> 24));
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

/* A header with no metadata and no action, then a footer whose patch CRC32 is valid. */
static Patch header_only(const char *magic, uint64_t source_size, uint64_t target_size)
{
	Patch p;
	p.size = 0;
	put_byte(&p, (uint8_t)magic[0]);
	put_byte(&p, (uint8_t)magic[1]);
	put_byte(&p, (uint8_t)magic[2]);
	put_byte(&p, (uint8_t)magic[3]);
	put_number(&p, source_size);
	put_number(&p, target_size);
	put_number(&p, 0);
	put_u32(&p, 0x12345678u);
	put_u32(&p, 0x9ABCDEF0u);
	put_u32(&p, rando_crc32(p.bytes, p.size));
	return p;
}

static void check_md5_hex(const char *what, const uint8_t digest[RANDO_MD5_SIZE], const char *expected)
{
	char hex[2 * RANDO_MD5_SIZE + 1];
	size_t i;
	for (i = 0; i < RANDO_MD5_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
	if (strcmp(hex, expected) != 0)
		test_fail(TEST, "%s: %s, expected %s", what, hex, expected);
}

static void test_digest_constants_are_jp10_and_vt_base(void)
{
	check_md5_hex("rando_assemble_jp10_md5", rando_assemble_jp10_md5, "03a63945398191337e896e5771f77173");
	check_md5_hex("rando_assemble_base_md5", rando_assemble_base_md5, "edc01f3db798ae4dfe21101311598d44");
}

static void test_request_init_fills_defaults(void)
{
	RandoAssembleRequest request;
	RandoOptions defaults;
	memset(&request, 0xA5, sizeof request);
	rando_assemble_request_init(&request);
	rando_options_init(&defaults);
	check_md5_hex("request source_md5", request.source_md5, "03a63945398191337e896e5771f77173");
	check_md5_hex("request base_md5", request.base_md5, "edc01f3db798ae4dfe21101311598d44");
	CHECK(memcmp(&request.options, &defaults, sizeof defaults) == 0);
	CHECK(request.options.hints);
	CHECK(request.heart_beep == RANDO_HEART_BEEP_HALF);
	CHECK(request.max_attempts == 1000u);
	CHECK(request.source == NULL && request.source_size == 0);
	CHECK(request.patch == NULL && request.patch_size == 0);
	CHECK(request.seed == 0);
	CHECK(request.trace == NULL && request.trace_ctx == NULL);
	CHECK(request.output == NULL && request.output_ctx == NULL);
}

static void test_jp_to_base_sizes_pass(void)
{
	static const uint8_t numbers[] = { 0x00, 0x7F, 0xBE, 0x00, 0x7F, 0xFE };
	Patch p = header_only("BPS1", 0x100000u, 0x200000u);
	RandoBpsResult bps = RANDO_BPS_TARGET_CRC;
	CHECK(memcmp(p.bytes + 4, numbers, sizeof numbers) == 0);
	CHECK(rando_assemble_check_patch(p.bytes, p.size, &bps) == RANDO_ASSEMBLE_OK);
	CHECK(bps == RANDO_BPS_OK);
	CHECK(rando_assemble_check_patch(p.bytes, p.size, NULL) == RANDO_ASSEMBLE_OK);
}

static void check_sizes_refused(uint64_t source_size, uint64_t target_size)
{
	Patch p = header_only("BPS1", source_size, target_size);
	RandoBpsResult bps = RANDO_BPS_TARGET_CRC;
	RandoAssembleStatus status = rando_assemble_check_patch(p.bytes, p.size, &bps);
	if (status != RANDO_ASSEMBLE_PATCH_SIZES || bps != RANDO_BPS_OK)
		test_fail(TEST, "%lu -> %lu: status %d, bps %d, expected PATCH_SIZES and OK",
			  (unsigned long)source_size, (unsigned long)target_size, (int)status, (int)bps);
}

static void test_other_sizes_are_refused(void)
{
	check_sizes_refused(0x200000u, 0x200000u);
	check_sizes_refused(0x100000u, 0x100000u);
	check_sizes_refused(0x100001u, 0x200000u);
	check_sizes_refused(0x100000u, 0x1FFFFFu);
}

static void test_wrong_magic_forwards_the_bps_code(void)
{
	Patch p = header_only("UPS1", 0x100000u, 0x200000u);
	RandoBpsResult bps = RANDO_BPS_OK;
	CHECK(rando_assemble_check_patch(p.bytes, p.size, &bps) == RANDO_ASSEMBLE_PATCH);
	CHECK(bps == RANDO_BPS_NOT_BPS);
}

static void test_damaged_patch_forwards_the_bps_code(void)
{
	Patch p = header_only("BPS1", 0x100000u, 0x200000u);
	RandoBpsResult bps = RANDO_BPS_OK;
	p.bytes[p.size - 6] ^= 0x01;
	CHECK(rando_assemble_check_patch(p.bytes, p.size, &bps) == RANDO_ASSEMBLE_PATCH);
	CHECK(bps == RANDO_BPS_PATCH_CRC);
}

/* The --file mode: 0 when the file's header declares the JP ROM -> base sizes, 1 otherwise. */
static int check_file(const char *path, FILE *out, FILE *err)
{
	static uint8_t bytes[FILE_MAX + 1];
	RandoBpsHeader header;
	RandoBpsResult bps = RANDO_BPS_OK;
	RandoAssembleStatus status;
	size_t size;
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(err, TEST ": cannot open %s\n", path);
		return 1;
	}
	size = fread(bytes, 1, sizeof bytes, f);
	if (ferror(f) || size > FILE_MAX) {
		fprintf(err, TEST ": %s: unreadable or larger than %u bytes\n", path, FILE_MAX);
		fclose(f);
		return 1;
	}
	fclose(f);
	status = rando_assemble_check_patch(bytes, size, &bps);
	if (status == RANDO_ASSEMBLE_PATCH) {
		fprintf(err, TEST ": %s: not a valid BPS (BPS code %d)\n", path, (int)bps);
		return 1;
	}
	CHECK(rando_bps_read_header(bytes, size, &header) == RANDO_BPS_OK);
	fprintf(out, TEST ": %s: source %lu bytes, target %lu bytes, source CRC32 %08lX\n", path,
		(unsigned long)header.source_size, (unsigned long)header.target_size,
		(unsigned long)header.source_crc);
	if (status != RANDO_ASSEMBLE_OK) {
		fprintf(err, TEST ": %s: declares other sizes than %u -> %u\n", path, RANDO_ROM_SOURCE_SIZE,
			RANDO_ROM_SIZE);
		return 1;
	}
	return 0;
}

static void write_file(const char *path, const Patch *p)
{
	FILE *f = fopen(path, "wb");
	if (f == NULL || fwrite(p->bytes, 1, p->size, f) != p->size || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", path);
}

static void test_file_mode(void)
{
	char root[] = "/tmp/test_patch_header_XXXXXX", good[256], sizes[256], missing[256];
	Patch ok = header_only("BPS1", 0x100000u, 0x200000u);
	Patch other = header_only("BPS1", 0x200000u, 0x200000u);
	FILE *sink = fopen("/dev/null", "w");
	int good_code, sizes_code, missing_code;

	CHECK(sink != NULL);
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	snprintf(good, sizeof good, "%s/base.bps", root);
	snprintf(sizes, sizeof sizes, "%s/sizes.bps", root);
	snprintf(missing, sizeof missing, "%s/missing.bps", root);
	write_file(good, &ok);
	write_file(sizes, &other);
	good_code = check_file(good, sink, sink);
	sizes_code = check_file(sizes, sink, sink);
	missing_code = check_file(missing, sink, sink);
	remove(good);
	remove(sizes);
	rmdir(root);
	fclose(sink);
	if (good_code != 0 || sizes_code != 1 || missing_code != 1)
		test_fail(TEST, "--file: exit %d for a good file, %d for other sizes, %d for a missing one",
			  good_code, sizes_code, missing_code);
}

int main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "--file") == 0)
		return check_file(argv[2], stdout, stderr);
	(void)test_fixtures_mode(TEST, argc, argv);
	test_digest_constants_are_jp10_and_vt_base();
	test_request_init_fills_defaults();
	test_jp_to_base_sizes_pass();
	test_other_sizes_are_refused();
	test_wrong_magic_forwards_the_bps_code();
	test_damaged_patch_forwards_the_bps_code();
	test_file_mode();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
