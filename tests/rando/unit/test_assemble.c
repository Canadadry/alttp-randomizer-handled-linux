#define _POSIX_C_SOURCE 200809L

#include "rando_assemble.h"
#include "rando_case.h"
#include "rando_crc32.h"
#include "support.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_assemble"
#define SOURCE_SIZE 0x100000u
#define ROM_SIZE 0x200000u
#define PATCH_MAX 0x10000u
#define OPS_MAX 32
#define LINE_MAX_BYTES 0x10000
#define ORACLE_WATCHDOG 1000u
#define VERSION_ADDRESS 0x7FE2u

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef enum OpKind { SOURCE_READ = 0, TARGET_READ = 1, TARGET_COPY = 3 } OpKind;

typedef struct Op {
	OpKind kind;
	size_t length;
	const uint8_t *bytes; /* TARGET_READ */
	size_t from;          /* TARGET_COPY: absolute target offset */
} Op;

/* One synthetic base build: the ops, in target order, from the 1 MiB source to the 2 MiB image. */
typedef struct Recipe {
	Op ops[OPS_MAX];
	int nops;
	size_t at;
} Recipe;

typedef struct Patch {
	uint8_t bytes[PATCH_MAX];
	size_t size;
} Patch;

static uint8_t source[SOURCE_SIZE];
static uint8_t source_before[SOURCE_SIZE];
static uint8_t other_source[SOURCE_SIZE];
static uint8_t base[ROM_SIZE];
static uint8_t sprite_base[ROM_SIZE];
static uint8_t expected[ROM_SIZE];
static uint8_t delivered[ROM_SIZE];
static uint8_t gfx_bytes[0x7000];
static uint8_t palette_bytes[120];
static uint8_t gloves_bytes[4];
static Patch good_patch, sprite_patch;
static uint8_t base_md5[RANDO_MD5_SIZE], sprite_base_md5[RANDO_MD5_SIZE], source_md5[RANDO_MD5_SIZE];
static RandoAssembleWork work;

static const uint8_t version_byte[1] = { 0x05 };
static const uint8_t guard[2] = { 0x02, 0x37 };
static const uint8_t filler[16] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
				    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xF0, 0x0F };

static void add(Recipe *r, OpKind kind, size_t length, const uint8_t *bytes, size_t from)
{
	CHECK(r->nops < OPS_MAX && length > 0);
	r->ops[r->nops].kind = kind;
	r->ops[r->nops].length = length;
	r->ops[r->nops].bytes = bytes;
	r->ops[r->nops].from = from;
	r->nops++;
	r->at += length;
}

static void source_read_to(Recipe *r, size_t end)
{
	CHECK(end > r->at);
	add(r, SOURCE_READ, end - r->at, NULL, 0);
}

static void copy_filler_to(Recipe *r, size_t end)
{
	CHECK(end > r->at);
	add(r, TARGET_COPY, end - r->at, NULL, SOURCE_SIZE);
}

/* The source with the version byte (and, for the sprite variant, distinct sprite bytes), then a
 * repeated filler with the two `02 37` author guards. */
static Recipe recipe(int sprite)
{
	Recipe r;
	r.nops = 0;
	r.at = 0;
	source_read_to(&r, VERSION_ADDRESS);
	add(&r, TARGET_READ, 1, version_byte, 0);
	if (sprite) {
		source_read_to(&r, 0x80000u);
		add(&r, TARGET_READ, sizeof gfx_bytes, gfx_bytes, 0);
		source_read_to(&r, 0xDD308u);
		add(&r, TARGET_READ, sizeof palette_bytes, palette_bytes, 0);
		source_read_to(&r, 0xDEDF5u);
		add(&r, TARGET_READ, sizeof gloves_bytes, gloves_bytes, 0);
	}
	source_read_to(&r, SOURCE_SIZE);
	add(&r, TARGET_READ, sizeof filler, filler, 0);
	copy_filler_to(&r, 0x118000u);
	add(&r, TARGET_READ, 2, guard, 0);
	copy_filler_to(&r, 0x11801Eu);
	add(&r, TARGET_READ, 2, guard, 0);
	copy_filler_to(&r, ROM_SIZE);
	return r;
}

static void build_image(const Recipe *r, const uint8_t *src, uint8_t *out)
{
	size_t at = 0, i;
	int k;
	for (k = 0; k < r->nops; k++) {
		const Op *op = &r->ops[k];
		for (i = 0; i < op->length; i++) {
			if (op->kind == SOURCE_READ)
				out[at + i] = src[at + i];
			else if (op->kind == TARGET_READ)
				out[at + i] = op->bytes[i];
			else
				out[at + i] = out[op->from + i];
		}
		at += op->length;
	}
	CHECK(at == ROM_SIZE);
}

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

static void encode(Patch *p, const Recipe *r, size_t source_size, size_t target_size, const uint8_t *image)
{
	size_t target_offset = 0, i;
	int k;
	p->size = 0;
	put_byte(p, 'B');
	put_byte(p, 'P');
	put_byte(p, 'S');
	put_byte(p, '1');
	put_number(p, source_size);
	put_number(p, target_size);
	put_number(p, 0);
	for (k = 0; k < r->nops; k++) {
		const Op *op = &r->ops[k];
		put_number(p, (uint64_t)(op->length - 1) << 2 | (uint64_t)op->kind);
		if (op->kind == TARGET_READ) {
			for (i = 0; i < op->length; i++)
				put_byte(p, op->bytes[i]);
		} else if (op->kind == TARGET_COPY) {
			if (op->from >= target_offset)
				put_number(p, (uint64_t)(op->from - target_offset) << 1);
			else
				put_number(p, (uint64_t)(target_offset - op->from) << 1 | 1u);
			target_offset = op->from + op->length;
		}
	}
	put_u32(p, rando_crc32(source, SOURCE_SIZE));
	put_u32(p, rando_crc32(image, ROM_SIZE));
	put_u32(p, rando_crc32(p->bytes, p->size));
}

/* A valid BPS header and footer with no action. */
static void header_only(Patch *p, size_t source_size, size_t target_size)
{
	p->size = 0;
	put_byte(p, 'B');
	put_byte(p, 'P');
	put_byte(p, 'S');
	put_byte(p, '1');
	put_number(p, source_size);
	put_number(p, target_size);
	put_number(p, 0);
	put_u32(p, 0);
	put_u32(p, 0);
	put_u32(p, rando_crc32(p->bytes, p->size));
}

typedef struct Calls {
	int outputs;
	size_t size;
	int result;
	long traces;
} Calls;

static bool count_output(void *ctx, const uint8_t *rom, size_t size)
{
	Calls *calls = ctx;
	calls->outputs++;
	calls->size = size;
	if (size == ROM_SIZE)
		memcpy(delivered, rom, ROM_SIZE);
	return calls->result != 0;
}

static void count_trace(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min, int64_t max,
			int64_t r)
{
	Calls *calls = ctx;
	(void)attempt;
	(void)draw;
	(void)stage;
	(void)min;
	(void)max;
	(void)r;
	calls->traces++;
}

static void setup(void)
{
	Recipe r;
	size_t i;
	for (i = 0; i < SOURCE_SIZE; i++)
		source[i] = (uint8_t)((i * 31 + (i >> 9)) & 0xFF);
	for (i = 0; i < sizeof gfx_bytes; i++)
		gfx_bytes[i] = (uint8_t)~source[0x80000u + i];
	for (i = 0; i < sizeof palette_bytes; i++)
		palette_bytes[i] = (uint8_t)~source[0xDD308u + i];
	for (i = 0; i < sizeof gloves_bytes; i++)
		gloves_bytes[i] = (uint8_t)~source[0xDEDF5u + i];
	memcpy(source_before, source, SOURCE_SIZE);
	memcpy(other_source, source, SOURCE_SIZE);
	other_source[0x1234] ^= 0x01;
	rando_md5(source, SOURCE_SIZE, source_md5);

	r = recipe(0);
	build_image(&r, source, base);
	encode(&good_patch, &r, SOURCE_SIZE, ROM_SIZE, base);
	rando_md5(base, ROM_SIZE, base_md5);
	r = recipe(1);
	build_image(&r, source, sprite_base);
	encode(&sprite_patch, &r, SOURCE_SIZE, ROM_SIZE, sprite_base);
	rando_md5(sprite_base, ROM_SIZE, sprite_base_md5);
}

/* The synthetic inputs, seed 1, with a counting output and trace. */
static RandoAssembleRequest request_for(Calls *calls, const Patch *patch)
{
	RandoAssembleRequest request;
	rando_assemble_request_init(&request);
	memset(calls, 0, sizeof *calls);
	calls->result = 1;
	request.source = source;
	request.source_size = SOURCE_SIZE;
	request.patch = patch->bytes;
	request.patch_size = patch->size;
	memcpy(request.source_md5, source_md5, RANDO_MD5_SIZE);
	memcpy(request.base_md5, patch == &sprite_patch ? sprite_base_md5 : base_md5, RANDO_MD5_SIZE);
	request.seed = 1;
	request.trace = count_trace;
	request.trace_ctx = calls;
	request.output = count_output;
	request.output_ctx = calls;
	return request;
}

static void check_source_unchanged(const char *what)
{
	if (memcmp(source, source_before, SOURCE_SIZE) != 0)
		test_fail(TEST, "%s: the source buffer changed", what);
}

static void check_refused(const char *what, const RandoAssembleRequest *request, const Calls *calls,
			  RandoAssembleStatus want, RandoBpsResult want_bps)
{
	RandoBpsResult bps = (RandoBpsResult)99;
	RandoAssembleStatus status = rando_assemble(request, &work, &bps);
	if (status != want || bps != want_bps)
		test_fail(TEST, "%s: status %d, bps %d, expected %d and %d", what, (int)status, (int)bps, (int)want,
			  (int)want_bps);
	if (calls->outputs != 0)
		test_fail(TEST, "%s: output called %d times", what, calls->outputs);
	if (want != RANDO_ASSEMBLE_GAVE_UP && calls->traces != 0)
		test_fail(TEST, "%s: %ld draws before generation", what, calls->traces);
	check_source_unchanged(what);
}

static void test_source_size_refused(void)
{
	static const size_t sizes[] = { SOURCE_SIZE - 1, SOURCE_SIZE + 1, ROM_SIZE };
	static uint8_t big[ROM_SIZE];
	Calls calls;
	size_t i;
	memcpy(big, source, SOURCE_SIZE);
	for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
		RandoAssembleRequest request = request_for(&calls, &good_patch);
		request.source = sizes[i] > SOURCE_SIZE ? big : source;
		request.source_size = sizes[i];
		check_refused("source size", &request, &calls, RANDO_ASSEMBLE_SOURCE_SIZE, RANDO_BPS_OK);
	}
}

static void test_source_md5_refused(void)
{
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &good_patch);
	request.source_md5[7] ^= 0x01;
	check_refused("source md5", &request, &calls, RANDO_ASSEMBLE_SOURCE_MD5, RANDO_BPS_OK);
}

static void test_truncated_patch_refused(void)
{
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &good_patch);
	request.patch_size = 10;
	check_refused("truncated patch", &request, &calls, RANDO_ASSEMBLE_PATCH, RANDO_BPS_MALFORMED);
}

static void test_damaged_patch_refused(void)
{
	static Patch damaged;
	Calls calls;
	RandoAssembleRequest request;
	damaged = good_patch;
	damaged.bytes[20] ^= 0x01;
	request = request_for(&calls, &damaged);
	check_refused("damaged patch", &request, &calls, RANDO_ASSEMBLE_PATCH, RANDO_BPS_PATCH_CRC);
}

static void test_source_crc_refused(void)
{
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &good_patch);
	request.source = other_source;
	rando_md5(other_source, SOURCE_SIZE, request.source_md5);
	check_refused("source crc", &request, &calls, RANDO_ASSEMBLE_PATCH, RANDO_BPS_SOURCE_CRC);
}

static void test_patch_sizes_refused(void)
{
	static Patch p;
	Calls calls;
	RandoAssembleRequest request;
	header_only(&p, ROM_SIZE, ROM_SIZE);
	request = request_for(&calls, &p);
	check_refused("2 MiB -> 2 MiB", &request, &calls, RANDO_ASSEMBLE_PATCH_SIZES, RANDO_BPS_OK);
	header_only(&p, SOURCE_SIZE, ROM_SIZE - 1);
	request = request_for(&calls, &p);
	check_refused("1 MiB -> 2 MiB - 1", &request, &calls, RANDO_ASSEMBLE_PATCH_SIZES, RANDO_BPS_OK);
}

static void test_base_md5_refused(void)
{
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &good_patch);
	request.base_md5[0] ^= 0x80;
	check_refused("base md5", &request, &calls, RANDO_ASSEMBLE_BASE_MD5, RANDO_BPS_OK);
}

static void test_gave_up(void)
{
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &good_patch);
	request.max_attempts = 1;
	work.gen.fault = RANDO_FAULT_MID_FILL;
	work.gen.fault_attempts = 1;
	work.gen.fault_fired = 0;
	check_refused("gave up", &request, &calls, RANDO_ASSEMBLE_GAVE_UP, RANDO_BPS_OK);
	CHECK(work.gen.fault_fired == 1);
	work.gen.fault = RANDO_FAULT_NONE;
}

static void test_output_failure_reported(void)
{
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &good_patch);
	RandoBpsResult bps = (RandoBpsResult)99;
	calls.result = 0;
	CHECK(rando_assemble(&request, &work, &bps) == RANDO_ASSEMBLE_OUTPUT);
	CHECK(bps == RANDO_BPS_OK);
	CHECK(calls.outputs == 1);
	check_source_unchanged("output");
}

static void test_order(void)
{
	static Patch damaged;
	Calls calls;
	RandoAssembleRequest request;

	damaged = good_patch;
	damaged.bytes[20] ^= 0x01;
	request = request_for(&calls, &damaged);
	request.source_md5[0] ^= 0x01;
	check_refused("md5 before patch", &request, &calls, RANDO_ASSEMBLE_SOURCE_MD5, RANDO_BPS_OK);

	request = request_for(&calls, &good_patch);
	request.source_size = SOURCE_SIZE - 1;
	request.source_md5[0] ^= 0x01;
	check_refused("size before md5", &request, &calls, RANDO_ASSEMBLE_SOURCE_SIZE, RANDO_BPS_OK);

	header_only(&damaged, ROM_SIZE, ROM_SIZE);
	request = request_for(&calls, &damaged);
	request.base_md5[0] ^= 0x01;
	check_refused("sizes before base md5", &request, &calls, RANDO_ASSEMBLE_PATCH_SIZES, RANDO_BPS_OK);

	request = request_for(&calls, &good_patch);
	request.base_md5[0] ^= 0x01;
	request.max_attempts = 1;
	work.gen.fault = RANDO_FAULT_MID_FILL;
	work.gen.fault_attempts = 1;
	work.gen.fault_fired = 0;
	check_refused("base md5 before generation", &request, &calls, RANDO_ASSEMBLE_BASE_MD5, RANDO_BPS_OK);
	CHECK(work.gen.fault_fired == 0);
	work.gen.fault = RANDO_FAULT_NONE;
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* Applies every `%06X <HEX> <method>` line of path to rom, in order. */
static void apply_rom_writes(const char *path, uint8_t *rom)
{
	static char line[LINE_MAX_BYTES];
	FILE *f = fopen(path, "r");
	long line_no = 0;
	if (f == NULL)
		test_fail(TEST, "cannot open %s", path);
	while (fgets(line, sizeof line, f) != NULL) {
		char *p = line, *end;
		unsigned long address;
		line_no++;
		if (strchr(line, '\n') == NULL && !feof(f))
			test_fail(TEST, "%s:%ld: line too long", path, line_no);
		address = strtoul(p, &end, 16);
		if (end != p + 6 || *end != ' ')
			test_fail(TEST, "%s:%ld: malformed address", path, line_no);
		p = end + 1;
		while (*p != ' ') {
			int hi = hex_digit(p[0]), lo = hi < 0 ? -1 : hex_digit(p[1]);
			if (hi < 0 || lo < 0 || address >= ROM_SIZE)
				test_fail(TEST, "%s:%ld: malformed bytes", path, line_no);
			rom[address++] = (uint8_t)(hi << 4 | lo);
			p += 2;
		}
	}
	fclose(f);
}

static void check_image(const char *what, const uint8_t *want)
{
	size_t i;
	for (i = 0; i < ROM_SIZE; i++)
		if (delivered[i] != want[i])
			test_fail(TEST, "%s: first difference at %06lX: %02X delivered, %02X expected", what,
				  (unsigned long)i, delivered[i], want[i]);
}

static void check_golden_case(const char *dir, const char *name)
{
	char path[512], error[RANDO_CASE_ERROR_MAX];
	RandoCase c;
	Calls calls;
	RandoAssembleRequest request;
	RandoBpsResult bps = (RandoBpsResult)99;
	RandoRomBuffer rom;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	if (!rando_case_read(path, &c, error))
		test_fail(TEST, "%s", error);
	request = request_for(&calls, &good_patch);
	request.options = c.options;
	request.heart_beep = c.heart_beep;
	request.seed = c.seed;
	request.max_attempts = !c.has_attempts ? ORACLE_WATCHDOG : c.attempts == UINT32_MAX ? 0 : c.attempts + 1;
	if (rando_assemble(&request, &work, &bps) != RANDO_ASSEMBLE_OK || bps != RANDO_BPS_OK)
		test_fail(TEST, "%s: the pipeline did not succeed", path);
	if (calls.outputs != 1 || calls.size != ROM_SIZE)
		test_fail(TEST, "%s: output called %d times, size %lu", path, calls.outputs, (unsigned long)calls.size);
	if (work.gen.seed != c.seed || (c.has_attempts && work.gen.attempt != c.attempts))
		test_fail(TEST, "%s: generation seed %lu attempt %lu", path, (unsigned long)work.gen.seed,
			  (unsigned long)work.gen.attempt);
	check_source_unchanged(path);

	memcpy(expected, base, ROM_SIZE);
	snprintf(path, sizeof path, "%s/%s/rom_writes.txt", dir, name);
	apply_rom_writes(path, expected);
	rom.bytes = expected;
	rom.size = ROM_SIZE;
	CHECK(rando_rom_write_cosmetic_defaults(&rom, source, SOURCE_SIZE));
	CHECK(rando_rom_update_checksum(&rom));
	snprintf(path, sizeof path, "%s/%s", dir, name);
	check_image(path, expected);
}

static void test_baseline_golden_replayed(void)
{
	check_golden_case("cases", "baseline");
}

static void test_checksum_ran_last(void)
{
	static uint8_t copy[ROM_SIZE];
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &good_patch);
	RandoRomBuffer rom;
	CHECK(rando_assemble(&request, &work, NULL) == RANDO_ASSEMBLE_OK);
	CHECK(calls.outputs == 1 && calls.size == ROM_SIZE);
	memcpy(copy, delivered, ROM_SIZE);
	rom.bytes = copy;
	rom.size = ROM_SIZE;
	CHECK(rando_rom_update_checksum(&rom));
	CHECK(memcmp(copy, delivered, ROM_SIZE) == 0);
	CHECK(memcmp(work.rom, delivered, ROM_SIZE) == 0);
	CHECK(calls.traces > 0);
	check_source_unchanged("success");
}

static void check_sprite_range(size_t address, size_t size)
{
	if (memcmp(sprite_base + address, source + address, size) == 0)
		test_fail(TEST, "sprite %06lX: the patch does not overwrite it", (unsigned long)address);
	if (memcmp(delivered + address, source + address, size) != 0)
		test_fail(TEST, "sprite %06lX: not the source's bytes", (unsigned long)address);
}

static void test_sprite_copied_from_source(void)
{
	Calls calls;
	RandoAssembleRequest request = request_for(&calls, &sprite_patch);
	CHECK(rando_assemble(&request, &work, NULL) == RANDO_ASSEMBLE_OK);
	CHECK(calls.outputs == 1);
	check_sprite_range(0x80000u, 0x7000u);
	check_sprite_range(0xDD308u, 120u);
	check_sprite_range(0xDEDF5u, 4u);
	check_source_unchanged("sprite");
}

static int check_golden_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *entry;
	int n = 0;
	if (d == NULL)
		test_fail(TEST, "cannot open %s/", dir);
	while ((entry = readdir(d)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		check_golden_case(dir, entry->d_name);
		n++;
	}
	closedir(d);
	return n;
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	setup();
	test_source_size_refused();
	test_source_md5_refused();
	test_truncated_patch_refused();
	test_damaged_patch_refused();
	test_source_crc_refused();
	test_patch_sizes_refused();
	test_base_md5_refused();
	test_gave_up();
	test_output_failure_reported();
	test_order();
	test_checksum_ran_last();
	test_sprite_copied_from_source();
	test_baseline_golden_replayed();
	printf(TEST ": passed\n");
	if (fixtures) {
		int n = check_golden_dir("cases") + check_golden_dir("combos");
		printf(TEST ": %d goldens assembled byte for byte\n", n);
	}
	return EXIT_SUCCESS;
}
