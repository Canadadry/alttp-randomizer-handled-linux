#define _POSIX_C_SOURCE 200809L

#include "rando_app_gen.h"
#include "rando_assemble.h"
#include "support.h"
#include "synthetic_base.h"
#include "ui_generate.h"
#include "ui_genflow.h"
#include "ui_grid.h"
#include "ui_strings.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_app_flow"
#define SNAPSHOTS "unit/snapshots/"
#define SEED 1234567u

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static RandoAppGen gen;
static UiGenFlow flow;
static UiGrid grid;
static char root[64];
static char text[UI_GRID_TEXT_MAX];

static RandoSaveStatus fake_status;
static RandoAssembleStatus fake_assemble;

static uint32_t fixed_seed(void)
{
	return SEED;
}

static void fixed_date(RandoRecordDate *out)
{
	static const RandoRecordDate date = {2026, 11, 1, 14, 3, 27};

	*out = date;
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
	memset(result, 0, sizeof *result);
	result->assemble = fake_assemble;
	result->bps = RANDO_BPS_OK;
	return fake_status;
}

static const char *at(const char *rel)
{
	static char path[512];

	snprintf(path, sizeof path, "%s/%s", root, rel);
	return path;
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

static void remove_all(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *e;
	char child[512];

	if (lstat(path, &st) != 0)
		return;
	if (!S_ISDIR(st.st_mode)) {
		remove(path);
		return;
	}
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
}

/* An SD root holding App/ZeldaRando/alttp/ with the synthetic base.sfc and base.bps; the runner on it with the synthetic
 * digests, a fixed seed and date, and the flow on the runner. */
static void make_root(void)
{
	strcpy(root, "/tmp/test_app_flow_XXXXXX");
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	make_dir("App");
	make_dir(RANDO_SD_APP_DIR);
	make_dir(RANDO_SD_ALTTP_DIR);
	write_bytes(RANDO_SD_BASE_ROM_FILE, synth_source, SYNTH_SOURCE_SIZE);
	write_bytes(RANDO_SD_BASE_PATCH_FILE, synth_good_patch.bytes, synth_good_patch.size);
	CHECK(rando_app_gen_init(&gen, root));
	memcpy(gen.source_md5, synth_source_md5, RANDO_MD5_SIZE);
	memcpy(gen.base_md5, synth_base_md5, RANDO_MD5_SIZE);
	gen.seed = fixed_seed;
	gen.date = fixed_date;
	ui_genflow_init(&flow, ui_str_english(), rando_app_gen_run, &gen);
}

static const char *render_screen(const UiScreen *screen)
{
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, screen));
	CHECK(ui_grid_text(&grid, text, sizeof text) > 0);
	return text;
}

static const char *render_flow(void)
{
	static UiScreen screen;

	ui_genflow_screen(&flow, &screen);
	return render_screen(&screen);
}

static const char *render_tab(const UiGenerate *tab)
{
	static UiScreen screen;

	ui_generate_screen(tab, &screen);
	return render_screen(&screen);
}

static void test_end_to_end(void)
{
	static UiGenerate tab;
	static char before[UI_GRID_TEXT_MAX];
	UiGenerateInit init;
	UiGenRequest request;

	make_root();
	ui_generate_init_defaults(&init, ui_str_english(), UI_GRID_DEFAULT_LINES - 4);
	ui_generate_init(&tab, &init);
	snprintf(before, sizeof before, "%s", render_tab(&tab));
	CHECK(ui_generate_key(&tab, UI_BUTTON_START) == UI_GENERATE_GENERATE);
	ui_generate_settings(&tab, &request.options, &request.heart_beep, &request.spoiler);
	ui_genflow_start(&flow, &request);
	CHECK(ui_genflow_pending(&flow));
	test_snapshot(TEST, SNAPSHOTS "genflow_generating.txt", render_flow());
	ui_genflow_run(&flow);
	test_snapshot(TEST, SNAPSHOTS "genflow_e2e_ready.txt", render_flow());
	CHECK(ui_genflow_key(&flow, UI_BUTTON_A) == UI_GENFLOW_BACK);
	if (strcmp(render_tab(&tab), before) != 0)
		test_fail(TEST, "the Generate tab changed across a generation");
	remove_all(root);
}

/* Starts and runs the flow with the defaults; the root is removed by the caller. */
static void generate(void)
{
	UiGenerateInit init;
	UiGenRequest request;

	ui_generate_init_defaults(&init, ui_str_english(), UI_GRID_DEFAULT_LINES - 4);
	request.options = init.options;
	request.heart_beep = init.heart_beep;
	request.spoiler = false;
	ui_genflow_start(&flow, &request);
	ui_genflow_run(&flow);
}

static void check_failure_snapshot(const char *snapshot)
{
	char path[128];

	generate();
	snprintf(path, sizeof path, SNAPSHOTS "%s", snapshot);
	test_snapshot(TEST, path, render_flow());
	remove_all(root);
}

static void inject(RandoSaveStatus status, RandoAssembleStatus assemble)
{
	gen.save = fake_save;
	fake_status = status;
	fake_assemble = assemble;
}

static void test_failure_snapshots(void)
{
	static uint8_t damaged[SYNTH_PATCH_MAX];

	make_root();
	remove(at(RANDO_SD_BASE_ROM_FILE));
	check_failure_snapshot("genflow_failed_base_missing.txt");

	make_root();
	memcpy(gen.source_md5, rando_assemble_jp10_md5, RANDO_MD5_SIZE);
	check_failure_snapshot("genflow_failed_base_md5.txt");

	make_root();
	write_bytes("Roms", (const uint8_t *)"x", 1);
	check_failure_snapshot("genflow_failed_folders.txt");

	make_root();
	memcpy(damaged, synth_good_patch.bytes, synth_good_patch.size);
	damaged[synth_good_patch.size / 2] ^= 0xFF;
	write_bytes(RANDO_SD_BASE_PATCH_FILE, damaged, synth_good_patch.size);
	check_failure_snapshot("genflow_failed_patch_refused.txt");

	make_root();
	inject(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_GAVE_UP);
	check_failure_snapshot("genflow_failed_gave_up.txt");

	make_root();
	inject(RANDO_SAVE_ASSEMBLE, RANDO_ASSEMBLE_OUTPUT);
	check_failure_snapshot("genflow_failed_sd_write.txt");

	make_root();
	inject(RANDO_SAVE_RECORD, RANDO_ASSEMBLE_OK);
	check_failure_snapshot("genflow_failed_record.txt");
}

/* The flow's fake run for the every-value check. */
static UiGenOutcome shown;

static void show(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	(void)ctx;
	(void)request;
	*outcome = shown;
}

/* The grid line holds text whole: from column 1, then blanks, and no cut mark anywhere. */
static void check_line(const char *what, int line, const char *full)
{
	char expected[UI_GRID_COLS_MAX + 1];

	snprintf(expected, sizeof expected, " %-*s", grid.cols - 1, full);
	if (memcmp(grid.cells[line], expected, (size_t)grid.cols) != 0 ||
	    memchr(grid.cells[line], '~', (size_t)grid.cols) != NULL)
		test_fail(TEST, "%s: line %d is \"%.*s\", not \"%s\"", what, line, grid.cols, grid.cells[line], full);
}

static void check_rendered(const char *what, RandoAppText text_ids, UiStrId status_arg)
{
	static UiGenRequest request;
	const UiStrTable *en = ui_str_english();
	char full[128];

	memset(&shown, 0, sizeof shown);
	shown.status = text_ids.status;
	shown.status_arg = ui_str_arity(text_ids.status) == 1 ? status_arg : UI_STR_NONE;
	shown.advice = text_ids.advice;
	ui_genflow_init(&flow, en, show, NULL);
	ui_genflow_start(&flow, &request);
	ui_genflow_run(&flow);
	render_flow();
	CHECK(ui_str_fill(ui_str(en, shown.status), ui_str(en, shown.status_arg), full, sizeof full));
	check_line(what, grid.lines - 2, full);
	CHECK(grid.styles[grid.lines - 2] == UI_LINE_ERROR);
	check_line(what, 2, ui_str(en, shown.advice));
}

static void test_every_value_rendered(void)
{
	RandoAppText patch = rando_app_assemble_text(RANDO_ASSEMBLE_PATCH);
	char what[64];
	int i;

	for (i = 0; i < RANDO_APP_GEN_STATUS_COUNT; i++) {
		snprintf(what, sizeof what, "app status %d", i);
		check_rendered(what, rando_app_gen_text((RandoAppGenStatus)i), UI_STR_NONE);
	}
	for (i = 0; i < RANDO_SAVE_STATUS_COUNT; i++) {
		snprintf(what, sizeof what, "save status %d", i);
		check_rendered(what, rando_app_save_text((RandoSaveStatus)i), UI_STR_NONE);
	}
	for (i = 0; i < RANDO_ASSEMBLE_STATUS_COUNT; i++) {
		snprintf(what, sizeof what, "assemble status %d", i);
		check_rendered(what, rando_app_assemble_text((RandoAssembleStatus)i),
			       rando_app_bps_text(RANDO_BPS_TARGET_TOO_SMALL).status);
	}
	for (i = 0; i < RANDO_BPS_RESULT_COUNT; i++) {
		snprintf(what, sizeof what, "bps result %d", i);
		check_rendered(what, patch, rando_app_bps_text((RandoBpsResult)i).status);
	}
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	synth_build();
	test_end_to_end();
	test_failure_snapshots();
	test_every_value_rendered();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
