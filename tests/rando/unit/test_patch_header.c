#define _POSIX_C_SOURCE 200809L

#include "rando_assemble.h"
#include "rando_crc32.h"
#include "rando_out.h"
#include "support.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST "test_patch_header"
#define PATCH_MAX 64
#define JP10_SOURCE_CRC 0x3322EFFCu
#define COMMITTED_PATCH "../../data/base.bps"

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
	put_u32(&p, JP10_SOURCE_CRC);
	put_u32(&p, 0x9ABCDEF0u);
	put_u32(&p, rando_crc32(p.bytes, p.size));
	return p;
}

static Patch with_source_crc(Patch p, uint32_t source_crc)
{
	p.size -= 12;
	put_u32(&p, source_crc);
	p.size += 4;
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

/* A valid JP -> base patch of exactly `total` bytes, padded with metadata. */
static void write_padded(const char *path, size_t total)
{
	static uint8_t bytes[RANDO_OUT_PATCH_MAX + 1];
	Patch prefix;
	size_t metadata = 0, k;
	FILE *f;
	CHECK(total <= sizeof bytes);
	for (k = 1; k < PATCH_MAX; k++) {
		metadata = total - 12 - k;
		prefix.size = 0;
		put_byte(&prefix, 'B');
		put_byte(&prefix, 'P');
		put_byte(&prefix, 'S');
		put_byte(&prefix, '1');
		put_number(&prefix, RANDO_ROM_SOURCE_SIZE);
		put_number(&prefix, RANDO_ROM_SIZE);
		put_number(&prefix, metadata);
		if (prefix.size == k)
			break;
	}
	CHECK(prefix.size == k);
	memcpy(bytes, prefix.bytes, k);
	memset(bytes + k, 'x', metadata);
	prefix.size = 0;
	put_u32(&prefix, JP10_SOURCE_CRC);
	put_u32(&prefix, 0x9ABCDEF0u);
	memcpy(bytes + k + metadata, prefix.bytes, 8);
	prefix.size = 0;
	put_u32(&prefix, rando_crc32(bytes, total - 4));
	memcpy(bytes + total - 4, prefix.bytes, 4);
	f = fopen(path, "wb");
	if (f == NULL || fwrite(bytes, 1, total, f) != total || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", path);
}

static bool output_called(void *ctx, const uint8_t *rom, size_t size)
{
	(void)rom;
	(void)size;
	*(bool *)ctx = true;
	return false;
}

static RandoAssembleStatus assemble_synthetic(const uint8_t *patch, size_t patch_size, size_t source_size,
					      RandoBpsResult *bps)
{
	static uint8_t source[RANDO_ROM_SOURCE_SIZE + 1];
	static RandoAssembleWork work;
	RandoAssembleRequest request;
	RandoAssembleStatus status;
	bool output = false;
	size_t i;
	for (i = 0; i < sizeof source; i++)
		source[i] = (uint8_t)(i * 31u + 7u);
	rando_assemble_request_init(&request);
	request.source = source;
	request.source_size = source_size;
	rando_md5(source, source_size, request.source_md5);
	request.patch = patch;
	request.patch_size = patch_size;
	request.output = output_called;
	request.output_ctx = &output;
	status = rando_assemble(&request, &work, bps);
	CHECK(!output);
	return status;
}

/*
 * A synthetic source never matches the source CRC32, so the pipeline must stop inside the applier at that
 * comparison: past every size check, which proves the patch is fed a source of the declared size.
 */
static int check_pipeline(const char *path, const uint8_t *patch, size_t patch_size, FILE *err)
{
	RandoBpsResult bps = RANDO_BPS_OK;
	RandoAssembleStatus status = assemble_synthetic(patch, patch_size, RANDO_ROM_SOURCE_SIZE, &bps);
	if (status != RANDO_ASSEMBLE_PATCH || bps != RANDO_BPS_SOURCE_CRC) {
		fprintf(err, TEST ": %s: a %u-byte synthetic source stops at status %d, BPS code %d, expected the source CRC32\n",
			path, RANDO_ROM_SOURCE_SIZE, (int)status, (int)bps);
		return 1;
	}
	status = assemble_synthetic(patch, patch_size, RANDO_ROM_SOURCE_SIZE + 1, &bps);
	if (status != RANDO_ASSEMBLE_SOURCE_SIZE) {
		fprintf(err, TEST ": %s: a %u-byte synthetic source stops at status %d, expected the source size\n", path,
			RANDO_ROM_SOURCE_SIZE + 1, (int)status);
		return 1;
	}
	return 0;
}

/* The --file mode: 0 when the file passes the gate, 1 otherwise. */
static int check_file(const char *path, FILE *out, FILE *err)
{
	static uint8_t bytes[RANDO_OUT_PATCH_MAX + 1];
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
	if (ferror(f) || size > RANDO_OUT_PATCH_MAX) {
		fprintf(err, TEST ": %s: unreadable or larger than the app's %u-byte patch cap\n", path, RANDO_OUT_PATCH_MAX);
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
	if (header.source_crc != JP10_SOURCE_CRC) {
		fprintf(err, TEST ": %s: source CRC32 %08lX, expected the JP 1.0 ROM's %08lX\n", path,
			(unsigned long)header.source_crc, (unsigned long)JP10_SOURCE_CRC);
		return 1;
	}
	return check_pipeline(path, bytes, size, err);
}

static void write_file(const char *path, const Patch *p)
{
	FILE *f = fopen(path, "wb");
	if (f == NULL || fwrite(p->bytes, 1, p->size, f) != p->size || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", path);
}

static void test_file_mode(void)
{
	char root[] = "/tmp/test_patch_header_XXXXXX", good[256], sizes[256], crc[256], cap[256], over[256],
	     missing[256];
	Patch ok = header_only("BPS1", 0x100000u, 0x200000u);
	Patch other = header_only("BPS1", 0x200000u, 0x200000u);
	Patch other_crc = with_source_crc(ok, JP10_SOURCE_CRC ^ 1u);
	FILE *sink = fopen("/dev/null", "w");
	int good_code, sizes_code, crc_code, cap_code, over_code, missing_code;

	CHECK(sink != NULL);
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	snprintf(good, sizeof good, "%s/base.bps", root);
	snprintf(sizes, sizeof sizes, "%s/sizes.bps", root);
	snprintf(crc, sizeof crc, "%s/crc.bps", root);
	snprintf(cap, sizeof cap, "%s/cap.bps", root);
	snprintf(over, sizeof over, "%s/over.bps", root);
	snprintf(missing, sizeof missing, "%s/missing.bps", root);
	write_file(good, &ok);
	write_padded(cap, RANDO_OUT_PATCH_MAX);
	write_padded(over, RANDO_OUT_PATCH_MAX + 1);
	write_file(sizes, &other);
	write_file(crc, &other_crc);
	good_code = check_file(good, sink, sink);
	sizes_code = check_file(sizes, sink, sink);
	crc_code = check_file(crc, sink, sink);
	cap_code = check_file(cap, sink, sink);
	over_code = check_file(over, sink, sink);
	missing_code = check_file(missing, sink, sink);
	remove(good);
	remove(sizes);
	remove(crc);
	remove(cap);
	remove(over);
	rmdir(root);
	fclose(sink);
	if (good_code != 0 || sizes_code != 1 || crc_code != 1 || cap_code != 0 || over_code != 1 || missing_code != 1)
		test_fail(TEST,
			  "--file: exit %d for a good file, %d for other sizes, %d for another source CRC32, %d at the cap, %d over it, %d for a missing one",
			  good_code, sizes_code, crc_code, cap_code, over_code, missing_code);
}

static void test_committed_patch_passes_the_gate(void)
{
	FILE *f = fopen(COMMITTED_PATCH, "rb");
	FILE *sink;
	if (f == NULL)
		test_fail(TEST, "%s missing: the plan's gate reads the committed base patch", COMMITTED_PATCH);
	fclose(f);
	sink = fopen("/dev/null", "w");
	CHECK(sink != NULL);
	if (check_file(COMMITTED_PATCH, sink, stderr) != 0)
		test_fail(TEST, "%s fails the gate", COMMITTED_PATCH);
	fclose(sink);
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
	test_committed_patch_passes_the_gate();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
