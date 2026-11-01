#define _POSIX_C_SOURCE 200809L

#include "rando_app_gen.h"
#include "rando_assemble.h"
#include "rando_last_settings.h"
#include "rando_out.h"
#include "rando_record.h"
#include "rando_seed_save.h"
#include "support.h"
#include "synthetic_base.h"
#include "ui_strings.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_app_gen"
#define SEED 1234567u

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static RandoAppGen gen;
static char root[64];
static char text[RANDO_RECORD_TEXT_MAX + 1];
static const RandoRecordDate fixed_date = {2026, 11, 1, 14, 3, 27};

/* What the injected save function returns, and how many times it ran. */
static RandoSaveStatus fake_status;
static RandoAssembleStatus fake_assemble;
static RandoBpsResult fake_bps;
static int fake_calls;

static uint32_t fixed_seed(void)
{
	return SEED;
}

static void fixed_date_fn(RandoRecordDate *out)
{
	*out = fixed_date;
}

static RandoSaveStatus fake_save(const char *rom_dir, const char *history_dir, const RandoAssembleRequest *request,
				 RandoAssembleWork *work, const RandoRecordDate *date, bool spoiler,
				 RandoSaveResult *result)
{
	(void)rom_dir;
	(void)history_dir;
	(void)request;
	(void)work;
	(void)date;
	(void)spoiler;
	fake_calls++;
	memset(result, 0, sizeof *result);
	result->assemble = fake_assemble;
	result->bps = fake_bps;
	return fake_status;
}

static const char *at(const char *rel)
{
	static char path[512];

	snprintf(path, sizeof path, "%s/%s", root, rel);
	return path;
}

static bool exists(const char *rel)
{
	struct stat st;

	return stat(at(rel), &st) == 0;
}

static void make_dir(const char *rel)
{
	if (mkdir(at(rel), 0700) != 0)
		test_fail(TEST, "cannot create %s", at(rel));
}

static void write_bytes(const char *rel, const uint8_t *bytes, size_t size)
{
	FILE *f = fopen(at(rel), "wb");

	if (f == NULL || fwrite(bytes, 1, size, f) != size || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", at(rel));
}

static void write_text(const char *rel, const char *s)
{
	write_bytes(rel, (const uint8_t *)s, strlen(s));
}

static void remove_all(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *e;
	char child[512];

	if (lstat(path, &st) != 0)
		return;
	if (S_ISDIR(st.st_mode)) {
		d = opendir(path);
		while (d != NULL && (e = readdir(d)) != NULL) {
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
				continue;
			snprintf(child, sizeof child, "%s/%s", path, e->d_name);
			remove_all(child);
		}
		if (d != NULL)
			closedir(d);
		rmdir(path);
	} else {
		remove(path);
	}
}

static int count_files(const char *rel)
{
	DIR *d = opendir(at(rel));
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

/* A fresh SD root holding App/ALttPR/ with the synthetic base.sfc and base.bps; the runner configured on it
 * with the synthetic digests, a fixed seed and date. */
static void make_root(void)
{
	strcpy(root, "/tmp/test_app_gen_XXXXXX");
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	make_dir("App");
	make_dir("App/ALttPR");
	write_bytes(RANDO_SD_BASE_ROM_FILE, synth_source, SYNTH_SOURCE_SIZE);
	write_bytes(RANDO_SD_BASE_PATCH_FILE, synth_good_patch.bytes, synth_good_patch.size);
	CHECK(rando_app_gen_init(&gen, root));
	memcpy(gen.source_md5, synth_source_md5, RANDO_MD5_SIZE);
	memcpy(gen.base_md5, synth_base_md5, RANDO_MD5_SIZE);
	gen.seed = fixed_seed;
	gen.date = fixed_date_fn;
}

static void drop_root(void)
{
	remove_all(root);
}

static UiGenRequest request_of(bool spoiler)
{
	UiGenRequest r;

	rando_options_init(&r.options);
	CHECK(rando_options_set(&r.options, "goal", "fast_ganon"));
	r.options.hints = false;
	r.heart_beep = RANDO_HEART_BEEP_QUARTER;
	r.spoiler = spoiler;
	return r;
}

static UiGenOutcome run(bool spoiler)
{
	UiGenRequest r = request_of(spoiler);
	UiGenOutcome o;

	memset(&o, 0, sizeof o);
	rando_app_gen_run(&gen, &r, &o);
	return o;
}

static void check_settings_saved(void)
{
	RandoOptions options;
	RandoHeartBeep heart_beep;

	CHECK(rando_last_settings_load(at(RANDO_SD_SETTINGS_FILE), &options, &heart_beep) == RANDO_LAST_SETTINGS_LOADED);
	CHECK(rando_options_get(&options, RANDO_OPTION_GOAL) == RANDO_OPTION_GOAL_FAST_GANON);
	CHECK(!options.hints && heart_beep == RANDO_HEART_BEEP_QUARTER);
}

static void check_failed(const char *what, UiGenOutcome o, UiStrId status, UiStrId status_arg, UiStrId advice)
{
	if (o.ok)
		test_fail(TEST, "%s: reported success", what);
	if (o.status != status || o.status_arg != status_arg || o.advice != advice)
		test_fail(TEST, "%s: ids %d %d %d, want %d %d %d", what, (int)o.status, (int)o.status_arg, (int)o.advice,
			  (int)status, (int)status_arg, (int)advice);
	if (count_files(RANDO_SD_ROM_DIR) > 0 || count_files(RANDO_SD_HISTORY_DIR) > 0)
		test_fail(TEST, "%s: a ROM or a record was left", what);
	check_settings_saved();
}

static void test_sd_root(void)
{
	static const char *const cases[][2] = {
		{"/mnt/SDCARD/App/ALttPR", "/mnt/SDCARD"},
		{"/mnt/SDCARD/App/ALttPR/", "/mnt/SDCARD"},
		{"App/ALttPR", "."},
		{"/x/y/App/ALttPR", "/x/y"},
		{"", "/mnt/SDCARD"},
		{"ALttPR", "/mnt/SDCARD"},
	};
	char out[64];
	size_t i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		if (!rando_app_sd_root(cases[i][0], out, sizeof out) || strcmp(out, cases[i][1]) != 0)
			test_fail(TEST, "sd root of \"%s\": \"%s\", want \"%s\"", cases[i][0], out, cases[i][1]);
	}
	CHECK(rando_app_sd_root(NULL, out, sizeof out) && strcmp(out, "/mnt/SDCARD") == 0);
	CHECK(!rando_app_sd_root("/mnt/SDCARD/App/ALttPR", out, 5));
	CHECK(rando_app_sd_root("/a/App/ALttPR", out, 3) && strcmp(out, "/a") == 0);
}

static void test_init_defaults(void)
{
	static char long_root[RANDO_SEED_PATH_MAX];
	static const uint8_t jp10[RANDO_MD5_SIZE] = {0x03, 0xa6, 0x39, 0x45, 0x39, 0x81, 0x91, 0x33,
						     0x7e, 0x89, 0x6e, 0x57, 0x71, 0xf7, 0x71, 0x73};
	static const uint8_t base[RANDO_MD5_SIZE] = {0xed, 0xc0, 0x1f, 0x3d, 0xb7, 0x98, 0xae, 0x4d,
						     0xfe, 0x21, 0x10, 0x13, 0x11, 0x59, 0x8d, 0x44};

	CHECK(rando_app_gen_init(&gen, "/mnt/SDCARD"));
	CHECK(memcmp(gen.source_md5, jp10, RANDO_MD5_SIZE) == 0);
	CHECK(memcmp(gen.base_md5, base, RANDO_MD5_SIZE) == 0);
	CHECK(gen.seed == rando_out_random_seed);
	CHECK(gen.save == rando_seed_save);
	CHECK(gen.date != NULL);
	memset(long_root, 'a', RANDO_SEED_DIR_MAX - 4);
	long_root[0] = '/';
	CHECK(!rando_app_gen_init(&gen, long_root));
}

static void test_success_from_an_empty_root(void)
{
	UiGenRequest r = request_of(true);
	UiGenOutcome o;
	RandoName name;
	RandoRecord record;
	char error[RANDO_RECORD_ERROR_MAX];
	char path[128];
	FILE *f;
	long n;

	make_root();
	o = run(true);
	CHECK(o.ok && o.spoiler && !o.list_stale);
	rando_name_make(&r.options, rando_name_code(&r.options, r.heart_beep, SEED), 1, &name);
	if (strcmp(o.rom, name.rom) != 0)
		test_fail(TEST, "saved as %s, want %s", o.rom, name.rom);
	snprintf(path, sizeof path, "%s/%s", RANDO_SD_ROM_DIR, name.rom);
	f = fopen(at(path), "rb");
	CHECK(f != NULL && fseek(f, 0, SEEK_END) == 0 && ftell(f) == (long)SYNTH_ROM_SIZE);
	if (f != NULL)
		fclose(f);
	snprintf(path, sizeof path, "%s/%s.json", RANDO_SD_HISTORY_DIR, name.code);
	f = fopen(at(path), "rb");
	CHECK(f != NULL);
	n = f != NULL ? (long)fread(text, 1, RANDO_RECORD_TEXT_MAX, f) : 0;
	if (f != NULL)
		fclose(f);
	if (!rando_record_parse(text, (size_t)n, &record, error))
		test_fail(TEST, "%s: %s", path, error);
	CHECK(record.seed == SEED && memcmp(&record.date, &fixed_date, sizeof fixed_date) == 0);
	CHECK(record.has_spoiler);
	check_settings_saved();
	CHECK(count_files(RANDO_SD_ROM_DIR) == 1 && count_files(RANDO_SD_HISTORY_DIR) == 1);
	drop_root();
}

static void test_caches(void)
{
	UiGenOutcome o;

	make_root();
	make_dir("Roms");
	make_dir("Roms/SFC");
	write_text(RANDO_SD_LIST_CACHE6, "x");
	write_text(RANDO_SD_LIST_CACHE2, "x");
	o = run(false);
	CHECK(o.ok && !o.spoiler && !o.list_stale);
	CHECK(!exists(RANDO_SD_LIST_CACHE6) && !exists(RANDO_SD_LIST_CACHE2));
	o = run(false);
	CHECK(o.ok && !o.list_stale);
	make_dir(RANDO_SD_LIST_CACHE6);
	write_text(RANDO_SD_LIST_CACHE6 "/keep", "x");
	write_text(RANDO_SD_LIST_CACHE2, "x");
	o = run(false);
	CHECK(o.ok && o.list_stale);
	CHECK(!exists(RANDO_SD_LIST_CACHE2));
	drop_root();

	make_root();
	memcpy(gen.source_md5, rando_assemble_jp10_md5, RANDO_MD5_SIZE);
	make_dir("Roms");
	make_dir("Roms/SFC");
	write_text(RANDO_SD_LIST_CACHE6, "x");
	write_text(RANDO_SD_LIST_CACHE2, "x");
	o = run(false);
	CHECK(!o.ok);
	CHECK(exists(RANDO_SD_LIST_CACHE6) && exists(RANDO_SD_LIST_CACHE2));
	drop_root();
}

static void test_real_failures(void)
{
	static uint8_t big[SYNTH_SOURCE_SIZE + 512];
	static uint8_t damaged[SYNTH_PATCH_MAX];
	FILE *f;

	make_root();
	remove(at(RANDO_SD_BASE_ROM_FILE));
	check_failed("no base.sfc", run(false), UI_STR_GEN_BASE_MISSING, UI_STR_NONE, UI_STR_ADVICE_BASE);
	make_dir(RANDO_SD_BASE_ROM_FILE);
	check_failed("base.sfc a directory", run(false), UI_STR_GEN_BASE_READ, UI_STR_NONE, UI_STR_ADVICE_BASE);
	drop_root();

	make_root();
	remove(at(RANDO_SD_BASE_PATCH_FILE));
	check_failed("no base.bps", run(false), UI_STR_GEN_PATCH_MISSING, UI_STR_NONE, UI_STR_ADVICE_REINSTALL);
	f = fopen(at(RANDO_SD_BASE_PATCH_FILE), "wb");
	CHECK(f != NULL && fseek(f, (long)RANDO_OUT_PATCH_MAX, SEEK_SET) == 0 && fputc(0, f) != EOF && fclose(f) == 0);
	check_failed("base.bps over the cap", run(false), UI_STR_GEN_PATCH_TOO_LARGE, UI_STR_NONE,
		     UI_STR_ADVICE_REINSTALL);
	drop_root();

	make_root();
	write_text("Roms", "x");
	check_failed("Roms a file", run(false), UI_STR_GEN_FOLDERS, UI_STR_NONE, UI_STR_ADVICE_SD_CARD);
	drop_root();

	make_root();
	memcpy(big, synth_source, SYNTH_SOURCE_SIZE);
	write_bytes(RANDO_SD_BASE_ROM_FILE, big, sizeof big);
	check_failed("1 MiB + 512 base", run(false), UI_STR_ASSEMBLE_SOURCE_SIZE, UI_STR_NONE, UI_STR_ADVICE_BASE);
	drop_root();

	make_root();
	memcpy(gen.source_md5, rando_assemble_jp10_md5, RANDO_MD5_SIZE);
	memcpy(gen.base_md5, rando_assemble_base_md5, RANDO_MD5_SIZE);
	check_failed("real digests", run(false), UI_STR_ASSEMBLE_SOURCE_MD5, UI_STR_NONE, UI_STR_ADVICE_BASE);
	drop_root();

	make_root();
	memcpy(damaged, synth_good_patch.bytes, synth_good_patch.size);
	damaged[synth_good_patch.size / 2] ^= 0xFF;
	write_bytes(RANDO_SD_BASE_PATCH_FILE, damaged, synth_good_patch.size);
	check_failed("damaged patch", run(false), UI_STR_ASSEMBLE_PATCH, UI_STR_BPS_PATCH_CRC, UI_STR_ADVICE_REINSTALL);
	drop_root();
}

static void check_injected(RandoSaveStatus status, RandoAssembleStatus assemble, RandoBpsResult bps, UiStrId want_status,
			   UiStrId want_arg, UiStrId want_advice)
{
	char what[64];

	fake_status = status;
	fake_assemble = assemble;
	fake_bps = bps;
	fake_calls = 0;
	snprintf(what, sizeof what, "injected save %d, assemble %d", (int)status, (int)assemble);
	check_failed(what, run(false), want_status, want_arg, want_advice);
	CHECK(fake_calls == 1);
}

static void test_unreachable_statuses(void)
{
	make_root();
	gen.save = fake_save;
	check_injected(RANDO_SAVE_NO_NAME, RANDO_ASSEMBLE_OK, RANDO_BPS_OK, UI_STR_SAVE_NO_NAME, UI_STR_NONE,
		       UI_STR_ADVICE_GENERATE_AGAIN);
	check_injected(RANDO_SAVE_RECORD, RANDO_ASSEMBLE_OK, RANDO_BPS_OK, UI_STR_SAVE_RECORD, UI_STR_NONE,
		       UI_STR_ADVICE_SD_CARD);
	check_injected(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_OUTPUT, RANDO_BPS_OK, UI_STR_ASSEMBLE_OUTPUT, UI_STR_NONE,
		       UI_STR_ADVICE_SD_CARD);
	check_injected(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_GAVE_UP, RANDO_BPS_OK, UI_STR_ASSEMBLE_GAVE_UP, UI_STR_NONE,
		       UI_STR_ADVICE_GENERATE_AGAIN);
	check_injected(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_UNSUPPORTED, RANDO_BPS_OK, UI_STR_ASSEMBLE_UNSUPPORTED,
		       UI_STR_NONE, UI_STR_ADVICE_CHANGE_SETTINGS);
	check_injected(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_BASE_MD5, RANDO_BPS_OK, UI_STR_ASSEMBLE_BASE_MD5, UI_STR_NONE,
		       UI_STR_ADVICE_REINSTALL);
	check_injected(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_PATCH_SIZES, RANDO_BPS_OK, UI_STR_ASSEMBLE_PATCH_SIZES,
		       UI_STR_NONE, UI_STR_ADVICE_REINSTALL);
	check_injected(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_PATCH, RANDO_BPS_TARGET_CRC, UI_STR_ASSEMBLE_PATCH,
		       UI_STR_BPS_TARGET_CRC, UI_STR_ADVICE_REINSTALL);
	drop_root();
}

/* status ids not UI_STR_NONE and distinct, advice ids not UI_STR_NONE unless none_advice, arity 1 only where
 * allowed (-1 for nowhere). */
static void check_table(const char *name, const RandoAppText *entries, int count, bool none_advice, int arity_one_at)
{
	int i, j;

	for (i = 0; i < count; i++) {
		if (entries[i].status == UI_STR_NONE)
			test_fail(TEST, "%s %d has no status text", name, i);
		if (!none_advice && entries[i].advice == UI_STR_NONE)
			test_fail(TEST, "%s %d has no advice", name, i);
		if ((ui_str_arity(entries[i].status) == 1) != (i == arity_one_at))
			test_fail(TEST, "%s %d: status arity %d", name, i, ui_str_arity(entries[i].status));
		if (ui_str_arity(entries[i].advice) != 0)
			test_fail(TEST, "%s %d: advice has a {}", name, i);
		for (j = 0; j < i; j++)
			if (entries[j].status == entries[i].status)
				test_fail(TEST, "%s %d and %d share a status id", name, j, i);
	}
}

static void test_tables(void)
{
	RandoAppText gen_t[RANDO_APP_GEN_STATUS_COUNT], save_t[RANDO_SAVE_STATUS_COUNT],
		assemble_t[RANDO_ASSEMBLE_STATUS_COUNT], bps_t[RANDO_BPS_RESULT_COUNT];
	const UiStrTable *en = ui_str_english();
	char filled[128];
	int i;

	for (i = 0; i < RANDO_APP_GEN_STATUS_COUNT; i++)
		gen_t[i] = rando_app_gen_text((RandoAppGenStatus)i);
	for (i = 0; i < RANDO_SAVE_STATUS_COUNT; i++)
		save_t[i] = rando_app_save_text((RandoSaveStatus)i);
	for (i = 0; i < RANDO_ASSEMBLE_STATUS_COUNT; i++)
		assemble_t[i] = rando_app_assemble_text((RandoAssembleStatus)i);
	for (i = 0; i < RANDO_BPS_RESULT_COUNT; i++)
		bps_t[i] = rando_app_bps_text((RandoBpsResult)i);
	check_table("app", gen_t, RANDO_APP_GEN_STATUS_COUNT, false, -1);
	check_table("save", save_t, RANDO_SAVE_STATUS_COUNT, false, -1);
	check_table("assemble", assemble_t, RANDO_ASSEMBLE_STATUS_COUNT, false, RANDO_ASSEMBLE_PATCH);
	check_table("bps", bps_t, RANDO_BPS_RESULT_COUNT, true, -1);
	for (i = 0; i < RANDO_BPS_RESULT_COUNT; i++) {
		CHECK(bps_t[i].advice == UI_STR_NONE);
		CHECK(ui_str_fill(ui_str(en, assemble_t[RANDO_ASSEMBLE_PATCH].status), ui_str(en, bps_t[i].status), filled,
				  sizeof filled));
		if (strlen(filled) > UI_STR_LEN_MAX)
			test_fail(TEST, "\"%s\" is longer than %d", filled, UI_STR_LEN_MAX);
	}
	CHECK(strcmp(ui_str(en, assemble_t[RANDO_ASSEMBLE_PATCH].status), "Patch refused: {}") == 0);
	CHECK(strcmp(ui_str(en, bps_t[RANDO_BPS_PATCH_CRC].status), "bad patch checksum") == 0);
	CHECK(strcmp(ui_str(en, gen_t[RANDO_APP_GEN_BASE_MISSING].status), "Base ROM not found") == 0);
	CHECK(strcmp(ui_str(en, gen_t[RANDO_APP_GEN_BASE_MISSING].advice), "Needs App/ALttPR/base.sfc (JP 1.0)") == 0);
	CHECK(strcmp(ui_str(en, save_t[RANDO_SAVE_RECORD].status), "History record write failed") == 0);
	CHECK(strcmp(ui_str(en, assemble_t[RANDO_ASSEMBLE_OUTPUT].status), "SD card write failed") == 0);
	CHECK(strcmp(ui_str(en, assemble_t[RANDO_ASSEMBLE_OUTPUT].advice), "Check that the SD card is writable") == 0);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	synth_build();
	test_sd_root();
	test_init_defaults();
	test_tables();
	test_success_from_an_empty_root();
	test_caches();
	test_real_failures();
	test_unreachable_statuses();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
