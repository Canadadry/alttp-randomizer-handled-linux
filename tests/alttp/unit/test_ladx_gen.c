#define _POSIX_C_SOURCE 200809L

#include "rando_ladx_gen.h"
#include "rando_ladx_record.h"
#include "rando_out.h"
#include "support.h"
#include "ui_strings.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_ladx_gen"
#define TABLES_SRC "../../data/ladx"
#define SEED_HEX "0102030405060708090a0b0c0d0e0f10"
#define SPOILER_TEXT "{\n  \"accessibleItems\": []\n}"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static RandoLadxGen gen;
static char root[64];
static uint8_t base[LADX_ROM_SIZE + 1];
static uint8_t base_sha1[LADX_SHA1_SIZE];
static uint8_t buf[LADX_ROM_SIZE + 1];
static char text[RANDO_RECORD_TEXT_MAX + 1];
static const RandoRecordDate fixed_date = {2026, 11, 8, 10, 20, 30};

/* What the fakes saw and do. */
static int allocs, releases, assembles, spoilers;
static bool alloc_fails, seed_fails, spoiler_fails;
static LadxAssembleStatus fake_status;
static LadxPipelineStatus fake_pipeline;
static int fake_built;
static LadxSettings seen_settings;

static bool fixed_seed(uint8_t seed[RANDO_LADX_SEED_BYTES])
{
	int i;

	for (i = 0; i < RANDO_LADX_SEED_BYTES; i++)
		seed[i] = (uint8_t)(i + 1);
	return !seed_fails;
}

static void fixed_date_fn(RandoRecordDate *out)
{
	*out = fixed_date;
}

static void *counting_alloc(size_t size)
{
	if (alloc_fails)
		return NULL;
	allocs++;
	return malloc(size);
}

static void counting_release(void *p)
{
	releases++;
	free(p);
}

static void fake_image(uint8_t *out)
{
	size_t i;

	for (i = 0; i < LADX_ROM_SIZE; i++)
		out[i] = (uint8_t)(i * 7 + 3);
}

static LadxAssembleStatus fake_assemble(LadxAssembler *a, const LadxAssembleInput *in, uint8_t *out)
{
	int i;

	assembles++;
	CHECK(in->base_len == LADX_ROM_SIZE && memcmp(in->base, base, LADX_ROM_SIZE) == 0);
	CHECK(in->base_sha1 != NULL && memcmp(in->base_sha1, base_sha1, LADX_SHA1_SIZE) == 0);
	CHECK(in->tables != NULL && ladx_tables_group_count(in->tables) > 0 && ladx_tables_unit_count(in->tables) > 0);
	CHECK(in->seed_len == RANDO_LADX_SEED_BYTES);
	for (i = 0; i < RANDO_LADX_SEED_BYTES; i++)
		CHECK(in->seed[i] == i + 1);
	seen_settings = *in->settings;
	a->spoiler.built = fake_built;
	a->pipeline_error.status = fake_pipeline;
	if (fake_status == LADX_ASSEMBLE_OK)
		fake_image(out);
	return fake_status;
}

static bool fake_spoiler(const LadxAssembler *a, FILE *out)
{
	(void)a;
	spoilers++;
	return !spoiler_fails && fputs(SPOILER_TEXT, out) != EOF;
}

static const char *at(const char *rel)
{
	static char path[1024];

	snprintf(path, sizeof path, "%s/%s", root, rel);
	return path;
}

static bool exists(const char *rel)
{
	struct stat st;

	return lstat(at(rel), &st) == 0;
}

static void make_dir(const char *rel)
{
	if (mkdir(at(rel), 0700) != 0)
		test_fail(TEST, "cannot create %s", at(rel));
}

static void write_bytes(const char *rel, const void *bytes, size_t size)
{
	FILE *f = fopen(at(rel), "wb");

	if (f == NULL || fwrite(bytes, 1, size, f) != size || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", at(rel));
}

static size_t read_file(const char *rel, void *out, size_t cap)
{
	FILE *f = fopen(at(rel), "rb");
	size_t n;

	if (f == NULL)
		test_fail(TEST, "cannot read %s", at(rel));
	n = fread(out, 1, cap, f);
	fclose(f);
	return n;
}

static void copy_tables(void)
{
	DIR *d = opendir(TABLES_SRC);
	struct dirent *e;
	static char bytes[2 * 1024 * 1024];

	if (d == NULL)
		test_fail(TEST, "cannot open %s", TABLES_SRC);
	make_dir(RANDO_SD_LADX_TABLES_DIR);
	while ((e = readdir(d)) != NULL) {
		char src[512], rel[512];
		FILE *f;
		size_t n;

		if (e->d_name[0] == '.')
			continue;
		snprintf(src, sizeof src, "%s/%s", TABLES_SRC, e->d_name);
		f = fopen(src, "rb");
		if (f == NULL)
			test_fail(TEST, "cannot read %s", src);
		n = fread(bytes, 1, sizeof bytes, f);
		fclose(f);
		snprintf(rel, sizeof rel, "%s/%s", RANDO_SD_LADX_TABLES_DIR, e->d_name);
		write_bytes(rel, bytes, n);
	}
	closedir(d);
}

static void remove_all(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *e;
	char child[1024];

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

/* A synthetic base: PRNG bytes, never ROM content. */
static void make_base(void)
{
	uint32_t x = 0x1ADC0DE5u;
	size_t i;

	for (i = 0; i < LADX_ROM_SIZE; i++) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		base[i] = (uint8_t)(x >> 24);
	}
	ladx_sha1(base, LADX_ROM_SIZE, base_sha1);
}

/* A fresh SD with App/ZeldaRando/ladx/ holding the base and the tables; the runner on fakes. */
static void make_root(void)
{
	strcpy(root, "/tmp/test_ladx_gen_XXXXXX");
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	make_dir("App");
	make_dir(RANDO_SD_APP_DIR);
	make_dir(RANDO_SD_LADX_DIR);
	write_bytes(RANDO_SD_LADX_BASE_FILE, base, LADX_ROM_SIZE);
	copy_tables();
	CHECK(rando_ladx_gen_init(&gen, root));
	memcpy(gen.base_sha1, base_sha1, LADX_SHA1_SIZE);
	gen.seed = fixed_seed;
	gen.date = fixed_date_fn;
	gen.alloc = counting_alloc;
	gen.release = counting_release;
	gen.assemble = fake_assemble;
	gen.spoiler = fake_spoiler;
	allocs = releases = assembles = spoilers = 0;
	alloc_fails = seed_fails = spoiler_fails = false;
	fake_status = LADX_ASSEMBLE_OK;
	fake_pipeline = LADX_PIPELINE_OK;
	fake_built = 1;
}

static void drop_root(void)
{
	CHECK(allocs == releases);
	remove_all(root);
}

static RandoLadxGenRequest request_of(bool spoiler)
{
	RandoLadxGenRequest r;

	ladx_settings_defaults(&r.settings);
	CHECK(ladx_settings_set(&r.settings, "logic=hard") == LADX_SETTINGS_OK);
	CHECK(ladx_settings_set(&r.settings, "goal=seashells") == LADX_SETTINGS_OK);
	r.spoiler = spoiler;
	return r;
}

static UiGenOutcome run_request(const RandoLadxGenRequest *r)
{
	UiGenOutcome o;

	memset(&o, 0x55, sizeof o);
	rando_ladx_gen_run(&gen, r, &o);
	return o;
}

static UiGenOutcome run(bool spoiler)
{
	RandoLadxGenRequest r = request_of(spoiler);

	return run_request(&r);
}

/* The tables reader reports its failure on stderr, as on the device: kept out of make test's output. */
static UiGenOutcome run_quiet(bool spoiler)
{
	int saved = dup(2), null = open("/dev/null", O_WRONLY);
	UiGenOutcome o;

	CHECK(saved >= 0 && null >= 0);
	fflush(stderr);
	dup2(null, 2);
	o = run(spoiler);
	fflush(stderr);
	dup2(saved, 2);
	close(saved);
	close(null);
	return o;
}

static void check_failure(UiGenOutcome o, RandoLadxGenStatus status, UiStrId text, UiStrId advice)
{
	if (o.ok || gen.status != status || o.status != text || o.advice != advice)
		test_fail(TEST, "status %d (outcome %d/%d), expected %d (%d/%d)", (int)gen.status, (int)o.status,
			  (int)o.advice, (int)status, (int)text, (int)advice);
	CHECK(rando_ladx_gen_text(status).status == text && rando_ladx_gen_text(status).advice == advice);
}

static void rom_rel(char *out, size_t cap, const char *rom)
{
	snprintf(out, cap, "%s/%s", RANDO_SD_LADX_ROM_DIR, rom);
}

/* ------------------------------------------------------------------------------------------------- */

static void test_base_check(void)
{
	static const uint8_t short_file[100];

	make_root();
	CHECK(rando_ladx_base_check(at(RANDO_SD_LADX_BASE_FILE), base_sha1, buf) == RANDO_LADX_BASE_OK);
	CHECK(memcmp(buf, base, LADX_ROM_SIZE) == 0);
	CHECK(rando_ladx_base_check(at("App/none.gbc"), base_sha1, buf) == RANDO_LADX_BASE_MISSING);
	CHECK(rando_ladx_base_check(at(RANDO_SD_LADX_DIR), base_sha1, buf) == RANDO_LADX_BASE_READ);
	write_bytes("short.gbc", short_file, sizeof short_file);
	CHECK(rando_ladx_base_check(at("short.gbc"), base_sha1, buf) == RANDO_LADX_BASE_SIZE);
	write_bytes("long.gbc", base, LADX_ROM_SIZE + 1);
	CHECK(rando_ladx_base_check(at("long.gbc"), base_sha1, buf) == RANDO_LADX_BASE_SIZE);
	base[5] ^= 1;
	write_bytes("other.gbc", base, LADX_ROM_SIZE);
	base[5] ^= 1;
	CHECK(rando_ladx_base_check(at("other.gbc"), base_sha1, buf) == RANDO_LADX_BASE_SHA1);
	drop_root();
}

static void test_generates_rom_record_and_spoiler(void)
{
	RandoLadxGenRequest r = request_of(true);
	RandoLadxRecord record;
	LadxSettings validated = r.settings;
	char error[RANDO_RECORD_ERROR_MAX], rel[512];
	static uint8_t image[LADX_ROM_SIZE + 1], expected[LADX_ROM_SIZE];
	UiGenOutcome o;
	size_t n;

	CHECK(ladx_settings_set(&r.settings, "accessibility=goal") == LADX_SETTINGS_OK);
	CHECK(ladx_settings_set(&r.settings, "goal=maze") == LADX_SETTINGS_OK); /* validate sets accessibility=all */
	validated = r.settings;
	make_root();
	make_dir("Roms");
	make_dir("Roms/SFC");
	write_bytes(RANDO_SD_LIST_CACHE6, "x", 1); /* an SFC cache is ALttP's: kept */
	o = run_request(&r);
	CHECK(o.ok && gen.status == RANDO_LADX_GEN_OK && o.status == UI_STR_GEN_OK && o.advice == UI_STR_ADVICE_NONE);
	CHECK(o.spoiler && !o.list_stale);
	CHECK(assembles == 1 && spoilers == 1 && allocs == 1 && releases == 1);
	ladx_settings_validate(&validated);
	CHECK(validated.value[LADX_SETTING_ACCESSIBILITY] == ladx_setting_value_find(LADX_SETTING_ACCESSIBILITY, "all"));
	CHECK(memcmp(&seen_settings, &validated, sizeof validated) == 0);

	CHECK(strcmp(o.rom, gen.name.rom) == 0);
	CHECK(strncmp(o.rom, "Hard-SignMaze-", 14) == 0);
	CHECK(gen.name.code[6] == '\0');
	rom_rel(rel, sizeof rel, o.rom);
	n = read_file(rel, image, sizeof image);
	fake_image(expected);
	CHECK(n == LADX_ROM_SIZE && memcmp(image, expected, LADX_ROM_SIZE) == 0);
	CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 1);

	snprintf(rel, sizeof rel, "%s/%s.json", RANDO_SD_LADX_HISTORY_DIR, gen.name.code);
	n = read_file(rel, text, sizeof text - 1);
	if (!rando_ladx_record_parse(text, n, &record, error))
		test_fail(TEST, "record: %s", error);
	CHECK(strcmp(record.name.rom, o.rom) == 0 && strcmp(record.seed, SEED_HEX) == 0);
	CHECK(memcmp(&record.date, &fixed_date, sizeof fixed_date) == 0);
	CHECK(memcmp(&record.settings, &validated, sizeof validated) == 0);
	CHECK(record.has_spoiler);
	CHECK(rando_ladx_name_code(&validated, SEED_HEX) == strtoul(gen.name.code, NULL, 16));

	snprintf(rel, sizeof rel, "%s/%s.spoiler.json", RANDO_SD_LADX_HISTORY_DIR, gen.name.code);
	n = read_file(rel, text, sizeof text - 1);
	CHECK(n == strlen(SPOILER_TEXT) && memcmp(text, SPOILER_TEXT, n) == 0);
	CHECK(count_files(RANDO_SD_LADX_HISTORY_DIR) == 2);
	CHECK(exists(RANDO_SD_LIST_CACHE6));

	o = run_request(&r);
	CHECK(o.ok && strcmp(gen.name.code + 6, "-2") == 0);
	CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 2 && count_files(RANDO_SD_LADX_HISTORY_DIR) == 4);
	drop_root();
}

static void test_spoiler_only_when_asked_and_built(void)
{
	char rel[512], error[RANDO_RECORD_ERROR_MAX];
	RandoLadxRecord record;
	UiGenOutcome o;
	size_t n;

	make_root();
	o = run(false);
	CHECK(o.ok && !o.spoiler && spoilers == 0);
	CHECK(count_files(RANDO_SD_LADX_HISTORY_DIR) == 1);
	snprintf(rel, sizeof rel, "%s/%s.json", RANDO_SD_LADX_HISTORY_DIR, gen.name.code);
	n = read_file(rel, text, sizeof text - 1);
	CHECK(rando_ladx_record_parse(text, n, &record, error) && !record.has_spoiler);
	drop_root();

	make_root();
	fake_built = 0; /* a race seed: LADXR writes no spoiler */
	o = run(true);
	CHECK(o.ok && !o.spoiler && spoilers == 0);
	CHECK(count_files(RANDO_SD_LADX_HISTORY_DIR) == 1);
	drop_root();
}

static void test_clears_gbc_list_caches(void)
{
	UiGenOutcome o;

	make_root();
	make_dir("Roms");
	make_dir("Roms/GBC");
	write_bytes(RANDO_SD_LADX_LIST_CACHE6, "x", 1);
	write_bytes(RANDO_SD_LADX_LIST_CACHE2, "x", 1);
	o = run(false);
	CHECK(o.ok && !o.list_stale);
	CHECK(!exists(RANDO_SD_LADX_LIST_CACHE6) && !exists(RANDO_SD_LADX_LIST_CACHE2));
	drop_root();
}

static void test_base_failures(void)
{
	static const uint8_t short_file[10];
	UiGenOutcome o;

	make_root();
	remove(at(RANDO_SD_LADX_BASE_FILE));
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_BASE_MISSING, UI_STR_GEN_BASE_MISSING, UI_STR_LADX_ADVICE_BASE);
	CHECK(assembles == 0 && !exists("Roms"));
	make_dir(RANDO_SD_LADX_BASE_FILE);
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_BASE_READ, UI_STR_GEN_BASE_READ, UI_STR_LADX_ADVICE_BASE);
	rmdir(at(RANDO_SD_LADX_BASE_FILE));
	write_bytes(RANDO_SD_LADX_BASE_FILE, short_file, sizeof short_file);
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_BASE_SIZE, UI_STR_ASSEMBLE_SOURCE_SIZE, UI_STR_LADX_ADVICE_BASE);
	base[0] ^= 1;
	write_bytes(RANDO_SD_LADX_BASE_FILE, base, LADX_ROM_SIZE);
	base[0] ^= 1;
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_BASE_SHA1, UI_STR_LADX_BASE_SHA1, UI_STR_LADX_ADVICE_BASE);
	CHECK(assembles == 0 && !exists("Roms"));
	drop_root();
}

static void test_setup_failures(void)
{
	char rel[512];
	RandoLadxGenRequest r = request_of(false);
	UiGenOutcome o;

	make_root();
	alloc_fails = true;
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_MEMORY, UI_STR_LADX_MEMORY, UI_STR_ADVICE_GENERATE_AGAIN);
	alloc_fails = false;

	snprintf(rel, sizeof rel, "%s/order.txt", RANDO_SD_LADX_TABLES_DIR);
	remove(at(rel));
	o = run_quiet(false);
	check_failure(o, RANDO_LADX_GEN_TABLES, UI_STR_LADX_TABLES, UI_STR_ADVICE_REINSTALL);
	write_bytes(rel, "garbage\n", 8);
	o = run_quiet(false);
	check_failure(o, RANDO_LADX_GEN_TABLES, UI_STR_LADX_TABLES, UI_STR_ADVICE_REINSTALL);
	CHECK(assembles == 0);
	drop_root();

	make_root();
	CHECK(ladx_settings_set(&r.settings, "entranceshuffle=simple") == LADX_SETTINGS_OK);
	o = run_request(&r);
	check_failure(o, RANDO_LADX_GEN_UNSUPPORTED, UI_STR_ASSEMBLE_UNSUPPORTED, UI_STR_ADVICE_CHANGE_SETTINGS);
	CHECK(assembles == 0 && !exists("Roms"));

	write_bytes("Roms", "x", 1);
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_FOLDERS, UI_STR_GEN_FOLDERS, UI_STR_ADVICE_SD_CARD);
	remove(at("Roms"));

	seed_fails = true;
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_SEED, UI_STR_LADX_SEED, UI_STR_ADVICE_GENERATE_AGAIN);
	seed_fails = false;
	CHECK(assembles == 0);
	drop_root();
}

static void test_generation_failures(void)
{
	static const struct {
		LadxAssembleStatus status;
		LadxPipelineStatus pipeline;
		RandoLadxGenStatus expected;
		UiStrId text;
	} cases[] = {
		{LADX_ASSEMBLE_OUTCOME, LADX_PIPELINE_OK, RANDO_LADX_GEN_GAVE_UP, UI_STR_ASSEMBLE_GAVE_UP},
		{LADX_ASSEMBLE_PIPELINE, LADX_PIPELINE_RAISED, RANDO_LADX_GEN_GAVE_UP, UI_STR_ASSEMBLE_GAVE_UP},
		{LADX_ASSEMBLE_PIPELINE, LADX_PIPELINE_GROUP, RANDO_LADX_GEN_BROKEN, UI_STR_LADX_BROKEN},
		{LADX_ASSEMBLE_BROKEN, LADX_PIPELINE_OK, RANDO_LADX_GEN_BROKEN, UI_STR_LADX_BROKEN},
		{LADX_ASSEMBLE_BAD_BASE, LADX_PIPELINE_OK, RANDO_LADX_GEN_BROKEN, UI_STR_LADX_BROKEN},
		{LADX_ASSEMBLE_IMAGE, LADX_PIPELINE_OK, RANDO_LADX_GEN_BROKEN, UI_STR_LADX_BROKEN},
		{LADX_ASSEMBLE_UNSUPPORTED, LADX_PIPELINE_OK, RANDO_LADX_GEN_BROKEN, UI_STR_LADX_BROKEN},
	};
	size_t i;

	make_root();
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		UiGenOutcome o;

		fake_status = cases[i].status;
		fake_pipeline = cases[i].pipeline;
		o = run(true);
		check_failure(o, cases[i].expected, cases[i].text, UI_STR_ADVICE_GENERATE_AGAIN);
		CHECK(gen.assembled == cases[i].status);
		CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 0 && count_files(RANDO_SD_LADX_HISTORY_DIR) == 0);
	}
	CHECK(spoilers == 0);
	drop_root();
}

static void test_write_failures_leave_nothing(void)
{
	char rel[512];
	UiGenOutcome o;

	make_root();
	o = run(false); /* learns the name, then frees it */
	CHECK(o.ok);
	rom_rel(rel, sizeof rel, o.rom);
	remove(at(rel));
	snprintf(rel, sizeof rel, "%s/%s.json", RANDO_SD_LADX_HISTORY_DIR, gen.name.code);
	remove(at(rel));

	rom_rel(rel, sizeof rel, gen.name.rom);
	strcat(rel, ".part");
	make_dir(rel);
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_ROM_WRITE, UI_STR_ASSEMBLE_OUTPUT, UI_STR_ADVICE_SD_CARD);
	CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 1 && count_files(RANDO_SD_LADX_HISTORY_DIR) == 0);
	rmdir(at(rel));

	spoiler_fails = true;
	o = run(true);
	check_failure(o, RANDO_LADX_GEN_RECORD, UI_STR_SAVE_RECORD, UI_STR_ADVICE_SD_CARD);
	CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 0 && count_files(RANDO_SD_LADX_HISTORY_DIR) == 0);
	spoiler_fails = false;

	snprintf(rel, sizeof rel, "%s/%s.spoiler.json", RANDO_SD_LADX_HISTORY_DIR, gen.name.code);
	make_dir(rel);
	snprintf(rel, sizeof rel, "%s/%s.spoiler.json/x", RANDO_SD_LADX_HISTORY_DIR, gen.name.code);
	write_bytes(rel, "x", 1);
	o = run(true);
	check_failure(o, RANDO_LADX_GEN_RECORD, UI_STR_SAVE_RECORD, UI_STR_ADVICE_SD_CARD);
	CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 0 && count_files(RANDO_SD_LADX_HISTORY_DIR) == 1);
	drop_root();
}

static void test_no_free_name(void)
{
	char rel[512];
	UiGenOutcome o;
	int suffix;

	make_root();
	o = run(false);
	CHECK(o.ok);
	for (suffix = 2; suffix <= RANDO_NAME_SUFFIX_MAX; suffix++) {
		RandoName n;
		LadxSettings s = request_of(false).settings;

		ladx_settings_validate(&s);
		rando_ladx_name_make(&s, rando_ladx_name_code(&s, SEED_HEX), suffix, &n);
		rom_rel(rel, sizeof rel, n.rom);
		write_bytes(rel, "x", 1);
	}
	o = run(false);
	check_failure(o, RANDO_LADX_GEN_NO_NAME, UI_STR_SAVE_NO_NAME, UI_STR_ADVICE_GENERATE_AGAIN);
	CHECK(assembles == 1);
	drop_root();
}

/* The defaults: LADXR's SHA-1 refuses the synthetic base; given its SHA-1, the real ladx_assemble refuses to parse it. */
static void test_default_wiring(void)
{
	UiGenOutcome o;

	make_root();
	CHECK(rando_ladx_gen_init(&gen, root));
	gen.date = fixed_date_fn;
	o = run(true);
	check_failure(o, RANDO_LADX_GEN_BASE_SHA1, UI_STR_LADX_BASE_SHA1, UI_STR_LADX_ADVICE_BASE);
	memcpy(gen.base_sha1, base_sha1, LADX_SHA1_SIZE);
	o = run(true);
	check_failure(o, RANDO_LADX_GEN_BROKEN, UI_STR_LADX_BROKEN, UI_STR_ADVICE_GENERATE_AGAIN);
	CHECK(gen.assembled == LADX_ASSEMBLE_BAD_BASE);
	CHECK(count_files(RANDO_SD_LADX_ROM_DIR) == 0);
	drop_root();
}

int main(void)
{
	make_base();
	test_base_check();
	test_generates_rom_record_and_spoiler();
	test_spoiler_only_when_asked_and_built();
	test_clears_gbc_list_caches();
	test_base_failures();
	test_setup_failures();
	test_generation_failures();
	test_write_failures_leave_nothing();
	test_no_free_name();
	test_default_wiring();
	printf("test_ladx_gen: ok\n");
	return 0;
}
