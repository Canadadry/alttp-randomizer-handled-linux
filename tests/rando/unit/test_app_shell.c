#define _POSIX_C_SOURCE 200809L

#include "rando_app_shell.h"
#include "rando_last_settings.h"
#include "rando_name.h"
#include "rando_record.h"
#include "rando_settings.h"
#include "support.h"
#include "synthetic_base.h"
#include "ui_app.h"
#include "ui_generate.h"
#include "ui_grid.h"
#include "ui_history.h"
#include "ui_strings.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_app_shell"
#define SNAPSHOTS "unit/snapshots/"
#define PAGE (UI_GRID_DEFAULT_LINES - 4)
#define WIDTH (UI_GRID_DEFAULT_COLS - 2)

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static RandoAppShell shell;
static RandoHistory ref_store;
static UiGenerate ref_generate;
static UiHistory ref_history;
static UiHistoryList ref_list;
static char root[64];
static char text[UI_GRID_TEXT_MAX];

static uint32_t fixed_seed(void)
{
	return 1;
}

static void fixed_date(RandoRecordDate *out)
{
	static const RandoRecordDate date = {2026, 11, 1, 14, 3, 27};

	*out = date;
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

static void write_bytes(const char *rel, const void *bytes, size_t size)
{
	FILE *f = fopen(at(rel), "wb");

	if (f == NULL || fwrite(bytes, 1, size, f) != size || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", at(rel));
}

static void write_text(const char *rel, const char *s)
{
	write_bytes(rel, s, strlen(s));
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

/* Registered with atexit, so a failing check still removes the tree. */
static void drop_root(void)
{
	if (root[0] != '\0')
		remove_all(root);
	root[0] = '\0';
}

/* A fresh SD root holding only App/ALttPR/. */
static void make_root(void)
{
	drop_root();
	strcpy(root, "/tmp/test_app_shell_XXXXXX");
	if (mkdtemp(root) == NULL) {
		root[0] = '\0';
		test_fail(TEST, "mkdtemp failed");
	}
	make_dir("App");
	make_dir("App/ALttPR");
}

static void start(void)
{
	CHECK(rando_app_shell_init(&shell, root, NULL, PAGE, WIDTH));
}

static const char *render_screen(const UiScreen *screen, char *buf, size_t size)
{
	static UiGrid grid;

	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, screen));
	CHECK(ui_grid_text(&grid, buf, size) > 0);
	return buf;
}

static const char *render(void)
{
	static UiScreen screen;

	ui_app_screen(&shell.app, &screen);
	return render_screen(&screen, text, sizeof text);
}

static UiGenerateInit defaults(void)
{
	UiGenerateInit init;

	ui_generate_init_defaults(&init, ui_str_english(), PAGE);
	return init;
}

static bool shows_generate(const UiGenerateInit *init)
{
	static UiScreen screen;
	static char ref[UI_GRID_TEXT_MAX];

	ui_generate_init(&ref_generate, init);
	ui_generate_screen(&ref_generate, &screen);
	return strcmp(render(), render_screen(&screen, ref, sizeof ref)) == 0;
}

/* The History tab on list, as the app's must look right after a tab switch. */
static bool shows_history(const UiHistoryList *list)
{
	static UiScreen screen;
	static char ref[UI_GRID_TEXT_MAX];
	UiHistoryInit init;

	init.strings = ui_str_english();
	init.page = PAGE;
	init.width = WIDTH;
	init.list = list;
	init.spoiler_fn = rando_history_spoiler;
	init.delete_fn = rando_history_delete;
	init.ctx = &ref_store;
	ui_history_init(&ref_history, &init);
	ui_history_screen(&ref_history, &screen);
	return strcmp(render(), render_screen(&screen, ref, sizeof ref)) == 0;
}

static void key(UiButton button)
{
	CHECK(ui_app_key(&shell.app, button) == UI_APP_NONE);
}

static void test_fresh_root(void)
{
	UiGenerateInit init = defaults();

	make_root();
	start();
	CHECK(ui_app_tab(&shell.app) == UI_APP_TAB_GENERATE);
	CHECK(shows_generate(&init));
	key(UI_BUTTON_R1);
	memset(&ref_list, 0, sizeof ref_list);
	CHECK(shows_history(&ref_list));
	CHECK(!exists("Roms"));
	CHECK(!exists(RANDO_SD_HISTORY_DIR));
	CHECK(!exists(RANDO_SD_SETTINGS_FILE));
}

static void test_saved_settings(void)
{
	UiGenerateInit init = defaults();

	make_root();
	rando_setting_set(&init.options, &init.heart_beep, RANDO_SETTING_GOAL,
			  rando_setting_value_find(RANDO_SETTING_GOAL, "dungeons"));
	rando_setting_set(&init.options, &init.heart_beep, RANDO_SETTING_STATE,
			  rando_setting_value_find(RANDO_SETTING_STATE, "inverted"));
	init.heart_beep = RANDO_HEART_BEEP_QUARTER;
	CHECK(rando_last_settings_save(at(RANDO_SD_SETTINGS_FILE), &init.options, init.heart_beep));
	start();
	CHECK(shows_generate(&init));
	CHECK(strstr(render(), "Inverted") != NULL);
}

static void test_unreadable_settings(void)
{
	UiGenerateInit init = defaults();

	make_root();
	write_text(RANDO_SD_SETTINGS_FILE, "{ not json");
	start();
	init.settings_unreadable = true;
	CHECK(shows_generate(&init));
	init.settings_unreadable = false;
	CHECK(!shows_generate(&init));
}

/* A seed already on the card, its record written as test_history writes one, without a spoiler. */
static void test_seed_on_card(void)
{
	RandoRecord r;
	FILE *f;

	make_root();
	make_dir("Roms");
	make_dir("Roms/SFC");
	make_dir(RANDO_SD_ROM_DIR);
	make_dir(RANDO_SD_HISTORY_DIR);
	memset(&r, 0, sizeof r);
	rando_options_init(&r.options);
	r.heart_beep = RANDO_HEART_BEEP_HALF;
	r.seed = 7;
	rando_name_make(&r.options, rando_name_code(&r.options, r.heart_beep, r.seed), 1, &r.name);
	r.date.year = 2026;
	r.date.month = 10;
	r.date.day = 30;
	r.date.hour = 9;
	r.date.minute = 41;
	r.date.second = 5;
	snprintf(text, sizeof text, "%s/%s.json", RANDO_SD_HISTORY_DIR, r.name.code);
	f = fopen(at(text), "w");
	CHECK(f != NULL);
	CHECK(rando_record_write(f, &r, NULL, NULL));
	CHECK(fclose(f) == 0);
	snprintf(text, sizeof text, "%s/%s", RANDO_SD_ROM_DIR, r.name.rom);
	write_text(text, "rom");
	start();
	key(UI_BUTTON_R1);
	CHECK(rando_history_init(&ref_store, root));
	rando_history_scan(&ref_store, &ref_list);
	CHECK(ref_list.count == 1 && strcmp(ref_list.entries[0].name.rom, r.name.rom) == 0);
	CHECK(shows_history(&ref_list));
	CHECK(strstr(render(), r.name.rom) != NULL);
}

static void test_root_too_long(void)
{
	static char long_root[RANDO_SEED_DIR_MAX + 8];

	memset(long_root, 'a', sizeof long_root - 1);
	long_root[0] = '/';
	CHECK(!rando_app_shell_init(&shell, long_root, NULL, PAGE, WIDTH));
}

static void test_generate_then_history(void)
{
	make_root();
	write_bytes(RANDO_SD_BASE_ROM_FILE, synth_source, SYNTH_SOURCE_SIZE);
	write_bytes(RANDO_SD_BASE_PATCH_FILE, synth_good_patch.bytes, synth_good_patch.size);
	start();
	memcpy(shell.gen.source_md5, synth_source_md5, RANDO_MD5_SIZE);
	memcpy(shell.gen.base_md5, synth_base_md5, RANDO_MD5_SIZE);
	shell.gen.seed = fixed_seed;
	shell.gen.date = fixed_date;
	key(UI_BUTTON_START);
	CHECK(ui_app_pending(&shell.app));
	ui_app_run(&shell.app);
	CHECK(strstr(render(), "Open-Ganon-1CA10A.sfc") != NULL);
	key(UI_BUTTON_A);
	CHECK(ui_app_tab(&shell.app) == UI_APP_TAB_GENERATE);
	key(UI_BUTTON_R1);
	test_snapshot(TEST, SNAPSHOTS "app_shell_generated.txt", render());
	CHECK(exists(RANDO_SD_ROM_DIR "/Open-Ganon-1CA10A.sfc"));
	CHECK(exists(RANDO_SD_HISTORY_DIR "/1CA10A.json"));
	CHECK(exists(RANDO_SD_SETTINGS_FILE));
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	atexit(drop_root);
	synth_build();
	test_fresh_root();
	test_saved_settings();
	test_unreadable_settings();
	test_seed_on_card();
	test_root_too_long();
	test_generate_then_history();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
