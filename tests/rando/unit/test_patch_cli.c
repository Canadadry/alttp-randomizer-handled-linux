#define _POSIX_C_SOURCE 200809L

#include "rando_assemble.h"
#include "rando_crc32.h"
#include "rando_patch.h"
#include "support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST "test_patch_cli"
#define SOURCE_SIZE 0x100000u
#define ROM_SIZE 0x200000u
#define PATCH_MAX 256
#define TEXT_MAX 4096

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define ARGC(a) ((int)(sizeof(a) / sizeof((a)[0])))

static uint8_t source[SOURCE_SIZE];
static uint8_t big[ROM_SIZE];
static uint8_t image[ROM_SIZE];
static uint8_t written[ROM_SIZE + 1];
static uint8_t delivered[ROM_SIZE];
static RandoAssembleWork work;
static uint8_t patch[PATCH_MAX];
static size_t patch_size;
static char root[] = "/tmp/test_patch_cli_XXXXXX";
static char base_path[256], big_path[256], bps_path[256], damaged_path[256], out_path[256], missing_out[256],
	absent_path[256];

static void put_byte(uint8_t byte)
{
	CHECK(patch_size < PATCH_MAX);
	patch[patch_size++] = byte;
}

static void put_u32(uint32_t v)
{
	put_byte((uint8_t)v);
	put_byte((uint8_t)(v >> 8));
	put_byte((uint8_t)(v >> 16));
	put_byte((uint8_t)(v >> 24));
}

static void put_number(uint64_t v)
{
	for (;;) {
		uint8_t x = (uint8_t)(v & 0x7F);
		v >>= 7;
		if (v == 0) {
			put_byte((uint8_t)(0x80 | x));
			return;
		}
		put_byte(x);
		v--;
	}
}

/* 1 MiB -> 2 MiB: a SourceRead of the whole source, a 16-byte TargetRead, a TargetCopy of it to the
 * end. image is built in plain loops from the same recipe. */
static void build_inputs(void)
{
	static const uint8_t filler[16] = { 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 121, 98, 219, 61 };
	size_t i;
	for (i = 0; i < SOURCE_SIZE; i++)
		source[i] = (uint8_t)((i * 13 + (i >> 11)) & 0xFF);
	memcpy(image, source, SOURCE_SIZE);
	memcpy(image + SOURCE_SIZE, filler, sizeof filler);
	for (i = SOURCE_SIZE + sizeof filler; i < ROM_SIZE; i++)
		image[i] = image[i - sizeof filler];
	memcpy(big, source, SOURCE_SIZE);

	patch_size = 0;
	put_byte('B');
	put_byte('P');
	put_byte('S');
	put_byte('1');
	put_number(SOURCE_SIZE);
	put_number(ROM_SIZE);
	put_number(0);
	put_number((uint64_t)(SOURCE_SIZE - 1) << 2);
	put_number((uint64_t)(sizeof filler - 1) << 2 | 1u);
	for (i = 0; i < sizeof filler; i++)
		put_byte(filler[i]);
	put_number((uint64_t)(ROM_SIZE - SOURCE_SIZE - sizeof filler - 1) << 2 | 3u);
	put_number((uint64_t)SOURCE_SIZE << 1);
	put_u32(rando_crc32(source, SOURCE_SIZE));
	put_u32(rando_crc32(image, ROM_SIZE));
	put_u32(rando_crc32(patch, patch_size));
}

static void write_file(const char *path, const uint8_t *bytes, size_t size)
{
	FILE *f = fopen(path, "wb");
	if (f == NULL || fwrite(bytes, 1, size, f) != size || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", path);
}

/* The file's size, or -1 when it cannot be opened; its bytes, up to ROM_SIZE + 1, in written. */
static long read_written(const char *path)
{
	FILE *f = fopen(path, "rb");
	size_t n;
	if (f == NULL)
		return -1;
	n = fread(written, 1, sizeof written, f);
	fclose(f);
	return (long)n;
}

static void setup(void)
{
	build_inputs();
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	snprintf(base_path, sizeof base_path, "%s/jp.sfc", root);
	snprintf(big_path, sizeof big_path, "%s/big.sfc", root);
	snprintf(bps_path, sizeof bps_path, "%s/base.bps", root);
	snprintf(damaged_path, sizeof damaged_path, "%s/damaged.bps", root);
	snprintf(out_path, sizeof out_path, "%s/out.sfc", root);
	snprintf(missing_out, sizeof missing_out, "%s/missing/out.sfc", root);
	snprintf(absent_path, sizeof absent_path, "%s/absent.sfc", root);
	write_file(base_path, source, SOURCE_SIZE);
	write_file(big_path, big, ROM_SIZE);
	write_file(bps_path, patch, patch_size);
	patch[patch_size - 20] ^= 0x01;
	write_file(damaged_path, patch, patch_size);
	patch[patch_size - 20] ^= 0x01;
}

static void teardown(void)
{
	remove(base_path);
	remove(big_path);
	remove(bps_path);
	remove(damaged_path);
	remove(out_path);
	rmdir(root);
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

static void test_defaults(void)
{
	char *argv[] = { "rando_patch", "--base", "jp.sfc", "--out", "seed.sfc" };
	char error[RANDO_PATCH_ERROR_MAX];
	RandoPatchArgs args;
	RandoAssembleRequest want;
	CHECK(rando_patch_parse_args(ARGC(argv), argv, &args, error));
	rando_assemble_request_init(&want);
	CHECK(strcmp(args.base_path, "jp.sfc") == 0);
	CHECK(strcmp(args.out_path, "seed.sfc") == 0);
	CHECK(strcmp(args.bps_path, "data/base.bps") == 0);
	CHECK(!args.has_seed);
	check_md5_hex("default source digest", args.request.source_md5, "03a63945398191337e896e5771f77173");
	check_md5_hex("default base digest", args.request.base_md5, "edc01f3db798ae4dfe21101311598d44");
	CHECK(memcmp(&args.request.options, &want.options, sizeof want.options) == 0);
	CHECK(args.request.heart_beep == RANDO_HEART_BEEP_HALF);
	CHECK(args.request.max_attempts == want.max_attempts);
}

static void test_every_key_lands(void)
{
	char *argv[] = { "rando_patch",
			 "--seed",
			 "42",
			 "glitches=overworld_glitches",
			 "item_placement=basic",
			 "--bps",
			 "other.bps",
			 "dungeon_items=full",
			 "accessibility=locations",
			 "goal=triforce-hunt",
			 "crystals_tower=3",
			 "crystals_ganon=random",
			 "--out",
			 "o.sfc",
			 "state=inverted",
			 "weapons=swordless",
			 "item_pool=hard",
			 "item_functionality=expert",
			 "hints=off",
			 "heart_beep=quarter",
			 "--base",
			 "b.sfc" };
	char error[RANDO_PATCH_ERROR_MAX];
	RandoPatchArgs args;
	const RandoOptions *o = &args.request.options;
	if (!rando_patch_parse_args(ARGC(argv), argv, &args, error))
		test_fail(TEST, "every key: refused: %s", error);
	CHECK(strcmp(args.bps_path, "other.bps") == 0 && strcmp(args.base_path, "b.sfc") == 0);
	CHECK(args.has_seed && args.seed == 42);
	CHECK(o->glitches == RANDO_GLITCHES_OVERWORLD_GLITCHES);
	CHECK(o->item_placement == RANDO_OPTION_PLACEMENT_BASIC);
	CHECK(o->dungeon_items == RANDO_OPTION_DUNGEON_ITEMS_FULL);
	CHECK(o->accessibility == RANDO_OPTION_ACCESSIBILITY_LOCATIONS);
	CHECK(o->goal == RANDO_OPTION_GOAL_TRIFORCE_HUNT);
	CHECK(o->crystals_tower == RANDO_CRYSTALS_3);
	CHECK(o->crystals_ganon == RANDO_CRYSTALS_RANDOM);
	CHECK(o->state == RANDO_OPTION_STATE_INVERTED);
	CHECK(o->weapons == RANDO_OPTION_WEAPONS_SWORDLESS);
	CHECK(o->item_pool == RANDO_ITEM_POOL_HARD);
	CHECK(o->item_functionality == RANDO_ITEM_FUNCTIONALITY_EXPERT);
	CHECK(!o->hints);
	CHECK(args.request.heart_beep == RANDO_HEART_BEEP_QUARTER);
}

static void check_parse_refused(int argc, char **argv, const char *word)
{
	char error[RANDO_PATCH_ERROR_MAX];
	RandoPatchArgs args;
	if (rando_patch_parse_args(argc, argv, &args, error))
		test_fail(TEST, "accepted, expected a refusal naming %s", word);
	if (strstr(error, word) == NULL)
		test_fail(TEST, "refusal \"%s\" does not name %s", error, word);
}

static void test_refusals(void)
{
	char *unknown_key[] = { "p", "--base", "b", "--out", "o", "color=red" };
	char *unknown_value[] = { "p", "--base", "b", "--out", "o", "goal=nope" };
	char *bad_hints[] = { "p", "--base", "b", "--out", "o", "hints=yes" };
	char *bad_beep[] = { "p", "--base", "b", "--out", "o", "heart_beep=Half" };
	char *key_twice[] = { "p", "--base", "b", "--out", "o", "goal=ganon", "goal=pedestal" };
	char *hints_twice[] = { "p", "--base", "b", "--out", "o", "hints=on", "hints=on" };
	char *flag_twice[] = { "p", "--base", "b", "--out", "o", "--base", "c" };
	char *no_value[] = { "p", "--base", "b", "--out" };
	char *stray[] = { "p", "--base", "b", "--out", "o", "stray" };
	char *no_base[] = { "p", "--out", "o" };
	char *no_out[] = { "p", "--base", "b" };
	char *bad_seed[] = { "p", "--base", "b", "--out", "o", "--seed", "x1" };
	char *big_seed[] = { "p", "--base", "b", "--out", "o", "--seed", "4294967296" };
	check_parse_refused(ARGC(unknown_key), unknown_key, "color");
	check_parse_refused(ARGC(unknown_value), unknown_value, "nope");
	check_parse_refused(ARGC(bad_hints), bad_hints, "yes");
	check_parse_refused(ARGC(bad_beep), bad_beep, "Half");
	check_parse_refused(ARGC(key_twice), key_twice, "goal");
	check_parse_refused(ARGC(hints_twice), hints_twice, "hints");
	check_parse_refused(ARGC(flag_twice), flag_twice, "--base");
	check_parse_refused(ARGC(no_value), no_value, "--out");
	check_parse_refused(ARGC(stray), stray, "stray");
	check_parse_refused(ARGC(no_base), no_base, "--base");
	check_parse_refused(ARGC(no_out), no_out, "--out");
	check_parse_refused(ARGC(bad_seed), bad_seed, "--seed");
	check_parse_refused(ARGC(big_seed), big_seed, "--seed");
}

typedef struct Run {
	int code;
	char out[TEXT_MAX];
	char err[TEXT_MAX];
} Run;

static void slurp(FILE *f, char *text)
{
	size_t n;
	rewind(f);
	n = fread(text, 1, TEXT_MAX - 1, f);
	text[n] = '\0';
	fclose(f);
}

/* Parses base, bps and out with seed 1 (none when seed is NULL); synthetic digests unless keep. */
static Run run(const char *base, const char *bps, const char *out, const char *seed, int keep_digests)
{
	char *argv[9];
	int argc = 0;
	char error[RANDO_PATCH_ERROR_MAX];
	RandoPatchArgs args;
	FILE *o = tmpfile(), *e = tmpfile();
	Run r;
	CHECK(o != NULL && e != NULL);
	argv[argc++] = "rando_patch";
	argv[argc++] = "--base";
	argv[argc++] = (char *)base;
	argv[argc++] = "--bps";
	argv[argc++] = (char *)bps;
	argv[argc++] = "--out";
	argv[argc++] = (char *)out;
	if (seed != NULL) {
		argv[argc++] = "--seed";
		argv[argc++] = (char *)seed;
	}
	if (!rando_patch_parse_args(argc, argv, &args, error))
		test_fail(TEST, "run: refused: %s", error);
	if (!keep_digests) {
		rando_md5(source, SOURCE_SIZE, args.request.source_md5);
		rando_md5(image, ROM_SIZE, args.request.base_md5);
	}
	r.code = rando_patch_run(&args, o, e);
	slurp(o, r.out);
	slurp(e, r.err);
	return r;
}

static bool keep_rom(void *ctx, const uint8_t *rom, size_t size)
{
	(void)ctx;
	CHECK(size == ROM_SIZE);
	memcpy(delivered, rom, ROM_SIZE);
	return true;
}

static void test_run_writes_the_assembled_rom(void)
{
	RandoAssembleRequest request;
	Run r = run(base_path, bps_path, out_path, "1", 0);
	if (r.code != 0)
		test_fail(TEST, "run: exit %d: %s", r.code, r.err);
	CHECK(strstr(r.out, "seed 1\n") != NULL);
	CHECK(strstr(r.out, out_path) != NULL);
	CHECK(read_written(out_path) == (long)ROM_SIZE);

	rando_assemble_request_init(&request);
	request.source = source;
	request.source_size = SOURCE_SIZE;
	request.patch = patch;
	request.patch_size = patch_size;
	rando_md5(source, SOURCE_SIZE, request.source_md5);
	rando_md5(image, ROM_SIZE, request.base_md5);
	request.seed = 1;
	request.output = keep_rom;
	CHECK(rando_assemble(&request, &work, NULL) == RANDO_ASSEMBLE_OK);
	CHECK(memcmp(written, delivered, ROM_SIZE) == 0);
	remove(out_path);
}

static void test_run_draws_a_seed(void)
{
	Run r = run(base_path, bps_path, out_path, NULL, 0);
	if (r.code != 0)
		test_fail(TEST, "drawn seed: exit %d: %s", r.code, r.err);
	CHECK(strncmp(r.out, "seed ", 5) == 0);
	CHECK(read_written(out_path) == (long)ROM_SIZE);
	remove(out_path);
}

static void check_run_refused(const char *what, Run r, const char *word1, const char *word2)
{
	if (r.code != 1)
		test_fail(TEST, "%s: exit %d, expected 1", what, r.code);
	if (strncmp(r.err, "rando_patch: ", 13) != 0 || strstr(r.err, word1) == NULL ||
	    (word2 != NULL && strstr(r.err, word2) == NULL))
		test_fail(TEST, "%s: message \"%s\" lacks %s", what, r.err, word2 != NULL ? word2 : word1);
	if (read_written(out_path) != -1)
		test_fail(TEST, "%s: an output file was left", what);
}

static void test_run_refusals(void)
{
	Run r;
	check_run_refused("missing base", run(absent_path, bps_path, out_path, "1", 0), absent_path, NULL);
	check_run_refused("2 MiB base", run(big_path, bps_path, out_path, "1", 0), big_path, "2097152");
	check_run_refused("wrong md5", run(base_path, bps_path, out_path, "1", 1), base_path, "MD5");
	check_run_refused("damaged patch", run(base_path, damaged_path, out_path, "1", 0), damaged_path,
			  "PATCH_CRC");
	r = run(base_path, bps_path, missing_out, "1", 0);
	check_run_refused("missing directory", r, missing_out, NULL);
	CHECK(read_written(missing_out) == -1);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_defaults();
	test_every_key_lands();
	test_refusals();
	setup();
	test_run_writes_the_assembled_rom();
	test_run_draws_a_seed();
	test_run_refusals();
	teardown();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
