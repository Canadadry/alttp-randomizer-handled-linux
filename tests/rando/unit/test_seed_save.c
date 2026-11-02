#define _POSIX_C_SOURCE 200809L

#include "rando_assemble.h"
#include "rando_record.h"
#include "rando_seed_save.h"
#include "support.h"
#include "synthetic_base.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_seed_save"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef struct Tree {
	char root[64], rom_dir[128], history_dir[128];
} Tree;

static RandoAssembleWork work;
static uint8_t delivered[SYNTH_ROM_SIZE];
static uint8_t file_bytes[SYNTH_ROM_SIZE + 1];
static uint8_t other_bytes[SYNTH_ROM_SIZE + 1];
static char text[RANDO_RECORD_TEXT_MAX + 1];
static const RandoRecordDate date = {2026, 11, 1, 14, 3, 27};

static void make_tree(Tree *t, bool rom_dir, bool history_dir)
{
	strcpy(t->root, "/tmp/test_seed_save_XXXXXX");
	if (mkdtemp(t->root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	snprintf(t->rom_dir, sizeof t->rom_dir, "%s/roms", t->root);
	snprintf(t->history_dir, sizeof t->history_dir, "%s/history", t->root);
	if ((rom_dir && mkdir(t->rom_dir, 0700) != 0) || (history_dir && mkdir(t->history_dir, 0700) != 0))
		test_fail(TEST, "mkdir failed under %s", t->root);
}

/* The number of entries in dir, -1 when it does not exist. */
static int count_files(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (d == NULL)
		return -1;
	while ((e = readdir(d)) != NULL)
		if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
			n++;
	closedir(d);
	return n;
}

static void empty_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char path[512];

	if (d == NULL)
		return;
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
		remove(path);
	}
	closedir(d);
	rmdir(dir);
}

static void remove_tree(Tree *t)
{
	empty_dir(t->rom_dir);
	empty_dir(t->history_dir);
	rmdir(t->root);
}

/* The file's size, -1 when it cannot be opened; up to max bytes in buf. */
static long read_file(const char *path, void *buf, size_t max)
{
	FILE *f = fopen(path, "rb");
	size_t n;

	if (f == NULL)
		return -1;
	n = fread(buf, 1, max, f);
	fclose(f);
	return (long)n;
}

static void place_file(const char *dir, const char *name, const char *content)
{
	char path[512];
	FILE *f;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	f = fopen(path, "wb");
	if (f == NULL || fputs(content, f) == EOF || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", path);
}

static bool capture(void *ctx, const uint8_t *rom, size_t size)
{
	(void)ctx;
	CHECK(size == SYNTH_ROM_SIZE);
	memcpy(delivered, rom, size);
	return true;
}

static bool must_not_write(void *ctx, const uint8_t *rom, size_t size)
{
	(void)ctx;
	(void)rom;
	(void)size;
	test_fail(TEST, "the request's own output was called");
	return false;
}

/* The synthetic base and patch with their digests, non-default settings, seed 1. */
static RandoAssembleRequest synthetic_request(void)
{
	RandoAssembleRequest request;

	rando_assemble_request_init(&request);
	request.source = synth_source;
	request.source_size = SYNTH_SOURCE_SIZE;
	request.patch = synth_good_patch.bytes;
	request.patch_size = synth_good_patch.size;
	memcpy(request.source_md5, synth_source_md5, RANDO_MD5_SIZE);
	memcpy(request.base_md5, synth_base_md5, RANDO_MD5_SIZE);
	CHECK(rando_options_set(&request.options, "goal", "fast_ganon"));
	request.options.hints = false;
	request.heart_beep = RANDO_HEART_BEEP_QUARTER;
	request.seed = 1;
	request.output = must_not_write;
	return request;
}

static RandoName expected_name(const RandoAssembleRequest *request, int suffix)
{
	RandoName name;

	rando_name_make(&request->options, rando_name_code(&request->options, request->heart_beep, request->seed),
			suffix, &name);
	return name;
}

static RandoSaveStatus save(const Tree *t, const RandoAssembleRequest *request, bool spoiler, RandoSaveResult *result)
{
	return rando_seed_save(t->rom_dir, t->history_dir, request, &work, &date, spoiler, result);
}

static void check_saved(const Tree *t, const RandoSaveResult *result, const RandoAssembleRequest *request,
			bool spoiler, int suffix)
{
	RandoName name = expected_name(request, suffix);
	char path[512];
	RandoRecord record;
	char error[RANDO_RECORD_ERROR_MAX];
	long n;

	if (strcmp(result->name.rom, name.rom) != 0)
		test_fail(TEST, "saved as %s, expected %s", result->name.rom, name.rom);
	snprintf(path, sizeof path, "%s/%s", t->rom_dir, name.rom);
	CHECK(strcmp(result->rom_path, path) == 0);
	CHECK(read_file(path, file_bytes, sizeof file_bytes) == (long)SYNTH_ROM_SIZE);
	snprintf(path, sizeof path, "%s/%s.json", t->history_dir, name.code);
	CHECK(strcmp(result->record_path, path) == 0);
	n = read_file(path, text, RANDO_RECORD_TEXT_MAX);
	CHECK(n > 0);
	if (!rando_record_parse(text, (size_t)n, &record, error))
		test_fail(TEST, "%s: %s", path, error);
	CHECK(strcmp(record.name.rom, name.rom) == 0 && strcmp(record.name.code, name.code) == 0);
	CHECK(record.seed == request->seed && record.heart_beep == request->heart_beep);
	CHECK(memcmp(&record.date, &date, sizeof date) == 0);
	CHECK(rando_options_get(&record.options, RANDO_OPTION_GOAL) == RANDO_OPTION_GOAL_FAST_GANON);
	CHECK(record.options.hints == request->options.hints);
	CHECK(record.has_spoiler == spoiler);
}

static void test_success(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoSaveResult result;
	Tree t;

	make_tree(&t, true, true);
	CHECK(save(&t, &request, false, &result) == RANDO_SAVE_OK);
	check_saved(&t, &result, &request, false, 1);
	CHECK(count_files(t.rom_dir) == 1 && count_files(t.history_dir) == 1);
	request.output = capture;
	CHECK(rando_assemble(&request, &work, NULL) == RANDO_ASSEMBLE_OK);
	CHECK(memcmp(file_bytes, delivered, SYNTH_ROM_SIZE) == 0);
	remove_tree(&t);
}

/* The spoiler option changes the record only: same ROM bytes, nothing but the ROM in rom_dir. */
static void test_spoiler_only_in_record(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoSaveResult off, on;
	Tree a, b;

	make_tree(&a, true, true);
	make_tree(&b, true, true);
	CHECK(save(&a, &request, false, &off) == RANDO_SAVE_OK);
	CHECK(save(&b, &request, true, &on) == RANDO_SAVE_OK);
	check_saved(&a, &off, &request, false, 1);
	check_saved(&b, &on, &request, true, 1);
	CHECK(strcmp(off.name.rom, on.name.rom) == 0);
	CHECK(read_file(off.rom_path, file_bytes, sizeof file_bytes) == (long)SYNTH_ROM_SIZE);
	CHECK(read_file(on.rom_path, other_bytes, sizeof other_bytes) == (long)SYNTH_ROM_SIZE);
	CHECK(memcmp(file_bytes, other_bytes, SYNTH_ROM_SIZE) == 0);
	CHECK(count_files(a.rom_dir) == 1 && count_files(b.rom_dir) == 1);
	CHECK(count_files(a.history_dir) == 1 && count_files(b.history_dir) == 1);
	remove_tree(&a);
	remove_tree(&b);
}

/* A repeat takes -2 and leaves the first ROM untouched. */
static void test_repeat_takes_a_suffix(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoSaveResult first, second;
	Tree t;

	make_tree(&t, true, true);
	CHECK(save(&t, &request, false, &first) == RANDO_SAVE_OK);
	CHECK(read_file(first.rom_path, other_bytes, sizeof other_bytes) == (long)SYNTH_ROM_SIZE);
	CHECK(save(&t, &request, false, &second) == RANDO_SAVE_OK);
	check_saved(&t, &first, &request, false, 1);
	check_saved(&t, &second, &request, false, 2);
	CHECK(read_file(first.rom_path, file_bytes, sizeof file_bytes) == (long)SYNTH_ROM_SIZE);
	CHECK(memcmp(file_bytes, other_bytes, SYNTH_ROM_SIZE) == 0);
	CHECK(count_files(t.rom_dir) == 2 && count_files(t.history_dir) == 2);
	remove_tree(&t);
}

/* A lone record, or a lone ROM, placed by hand takes the unsuffixed name. */
static void test_lone_file_takes_the_name(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoName name = expected_name(&request, 1);
	RandoSaveResult result;
	char record[64];
	Tree t;

	snprintf(record, sizeof record, "%s.json", name.code);
	make_tree(&t, true, true);
	place_file(t.history_dir, record, "hand");
	CHECK(save(&t, &request, false, &result) == RANDO_SAVE_OK);
	check_saved(&t, &result, &request, false, 2);
	CHECK(count_files(t.rom_dir) == 1 && count_files(t.history_dir) == 2);
	remove_tree(&t);

	make_tree(&t, true, true);
	place_file(t.rom_dir, name.rom, "hand");
	CHECK(save(&t, &request, false, &result) == RANDO_SAVE_OK);
	check_saved(&t, &result, &request, false, 2);
	CHECK(count_files(t.rom_dir) == 2 && count_files(t.history_dir) == 1);
	remove_tree(&t);
}

static void test_assemble_failure_leaves_nothing(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoSaveResult result;
	Tree t;

	make_tree(&t, true, true);
	request.source_md5[0] ^= 0x01;
	CHECK(save(&t, &request, true, &result) == RANDO_SAVE_ASSEMBLE);
	CHECK(result.assemble == RANDO_ASSEMBLE_SOURCE_MD5);
	CHECK(count_files(t.rom_dir) == 0 && count_files(t.history_dir) == 0);
	remove_tree(&t);
}

static void test_missing_rom_dir_is_an_output_failure(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoSaveResult result;
	Tree t;

	make_tree(&t, false, true);
	CHECK(save(&t, &request, true, &result) == RANDO_SAVE_ASSEMBLE);
	CHECK(result.assemble == RANDO_ASSEMBLE_OUTPUT);
	CHECK(count_files(t.rom_dir) == -1 && count_files(t.history_dir) == 0);
	remove_tree(&t);
}

static void test_record_failure_removes_the_rom(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoSaveResult result;
	Tree t;

	make_tree(&t, true, false);
	CHECK(save(&t, &request, true, &result) == RANDO_SAVE_RECORD);
	CHECK(result.assemble == RANDO_ASSEMBLE_OK);
	CHECK(count_files(t.rom_dir) == 0 && count_files(t.history_dir) == -1);
	remove_tree(&t);
}

/* Every suffix taken by a hand-placed record: nothing written, nothing touched. */
static void test_no_free_name(void)
{
	RandoAssembleRequest request = synthetic_request();
	RandoSaveResult result;
	char record[256];
	int suffix;
	Tree t;

	make_tree(&t, true, true);
	for (suffix = 1; suffix <= RANDO_NAME_SUFFIX_MAX; suffix++) {
		RandoName name = expected_name(&request, suffix);

		snprintf(record, sizeof record, "%s.json", name.code);
		place_file(t.history_dir, record, "hand");
	}
	CHECK(save(&t, &request, false, &result) == RANDO_SAVE_NO_NAME);
	CHECK(strcmp(result.name.code, expected_name(&request, 1).code) == 0);
	CHECK(count_files(t.rom_dir) == 0 && count_files(t.history_dir) == RANDO_NAME_SUFFIX_MAX);
	snprintf(record, sizeof record, "%s/%s.json", t.history_dir, expected_name(&request, 7).code);
	CHECK(read_file(record, text, RANDO_RECORD_TEXT_MAX) == 4 && memcmp(text, "hand", 4) == 0);
	remove_tree(&t);
}

int main(void)
{
	synth_build();
	test_success();
	test_spoiler_only_in_record();
	test_repeat_takes_a_suffix();
	test_lone_file_takes_the_name();
	test_assemble_failure_leaves_nothing();
	test_missing_rom_dir_is_an_output_failure();
	test_record_failure_removes_the_rom();
	test_no_free_name();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
