#define _POSIX_C_SOURCE 200809L

#include "rando_app_shell.h"
#include "ladx_assemble.h"
#include "ladx_inputs.h"
#include "rando_ladx_last_settings.h"
#include "rando_last_game.h"
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
	int len = snprintf(path, sizeof path, "%s/%s", root, rel);

	if (len < 0 || (size_t)len >= sizeof path)
		test_fail(TEST, "path too long: %s/%s", root, rel);
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

/* True when rel holds exactly s. */
static bool holds(const char *rel, const char *s)
{
	static char buf[1024];
	FILE *f = fopen(at(rel), "rb");
	size_t n;

	if (f == NULL)
		return false;
	n = fread(buf, 1, sizeof buf - 1, f);
	fclose(f);
	buf[n] = '\0';
	return strcmp(buf, s) == 0;
}

#define ALTTP_TEXT "{\"last_game\":\"alttp\"}\n"
#define LADX_TEXT "{\"last_game\":\"ladx\"}\n"

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

/* A fresh SD root holding only App/ZeldaRando/alttp/. */
static void make_root(void)
{
	drop_root();
	strcpy(root, "/tmp/test_app_shell_XXXXXX");
	if (mkdtemp(root) == NULL) {
		root[0] = '\0';
		test_fail(TEST, "mkdtemp failed");
	}
	make_dir("App");
	make_dir(RANDO_SD_APP_DIR);
	make_dir(RANDO_SD_ALTTP_DIR);
}

static void start(void)
{
	CHECK(rando_app_shell_init(&shell, root, NULL, NULL, 0, PAGE, WIDTH));
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

/* The shell with ALttP opened from the choice screen, its cursor on ALttP. */
static void start_alttp(void)
{
	start();
	CHECK(ui_app_view(&shell.app) == UI_APP_VIEW_GAMES);
	key(UI_BUTTON_A);
	CHECK(ui_app_view(&shell.app) == UI_APP_VIEW_ALTTP);
}

static void test_fresh_root(void)
{
	UiGenerateInit init = defaults();

	make_root();
	start();
	test_snapshot(TEST, SNAPSHOTS "games_first_launch.txt", render());
	CHECK(!exists(RANDO_SD_LAST_GAME_FILE));
	CHECK(ui_app_key(&shell.app, UI_BUTTON_B) == UI_APP_QUIT);
	CHECK(!exists(RANDO_SD_LAST_GAME_FILE));
	start_alttp();
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, ALTTP_TEXT));
	CHECK(ui_app_tab(&shell.app) == UI_APP_TAB_GENERATE);
	CHECK(shows_generate(&init));
	key(UI_BUTTON_R1);
	memset(&ref_list, 0, sizeof ref_list);
	CHECK(shows_history(&ref_list));
	CHECK(!exists("Roms"));
	CHECK(!exists(RANDO_SD_HISTORY_DIR));
	CHECK(!exists(RANDO_SD_SETTINGS_FILE));
}

static void test_remembered_game(void)
{
	static const char spaced[] = " { \"last_game\" : \"ladx\" }\n";

	make_root();
	write_text(RANDO_SD_LAST_GAME_FILE, spaced);
	start();
	test_snapshot(TEST, SNAPSHOTS "games_ladx_remembered.txt", render());
	key(UI_BUTTON_A);
	CHECK(ui_app_view(&shell.app) == UI_APP_VIEW_LADX);
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, spaced));
	key(UI_BUTTON_B);
	key(UI_BUTTON_UP);
	key(UI_BUTTON_A);
	CHECK(ui_app_view(&shell.app) == UI_APP_VIEW_ALTTP);
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, ALTTP_TEXT));
	key(UI_BUTTON_B);
	key(UI_BUTTON_DOWN);
	key(UI_BUTTON_A);
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, LADX_TEXT));
	write_text(RANDO_SD_LAST_GAME_FILE, spaced);
	key(UI_BUTTON_B);
	key(UI_BUTTON_A);
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, spaced));
	CHECK(!exists(RANDO_SD_SETTINGS_FILE));
	CHECK(!exists(RANDO_SD_APP_DIR "/ladx"));
}

static void test_invalid_memory_replaced(void)
{
	make_root();
	write_text(RANDO_SD_LAST_GAME_FILE, "{\"last_game\":\"zelda1\"}\n");
	start();
	test_snapshot(TEST, SNAPSHOTS "games_first_launch.txt", render());
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, "{\"last_game\":\"zelda1\"}\n"));
	key(UI_BUTTON_A);
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, ALTTP_TEXT));
}

static void test_failed_memory_write(void)
{
	make_root();
	make_dir(RANDO_SD_LAST_GAME_FILE);
	start();
	key(UI_BUTTON_A);
	CHECK(ui_app_view(&shell.app) == UI_APP_VIEW_ALTTP);
	key(UI_BUTTON_B);
	CHECK(rmdir(at(RANDO_SD_LAST_GAME_FILE)) == 0);
	key(UI_BUTTON_A);
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, ALTTP_TEXT));
}

static void test_alttp_settings_untouched(void)
{
	UiGenerateInit init = defaults();
	static char before[2048];
	FILE *f;
	size_t n;

	make_root();
	init.heart_beep = RANDO_HEART_BEEP_QUARTER;
	CHECK(rando_last_settings_save(at(RANDO_SD_SETTINGS_FILE), &init.options, init.heart_beep));
	f = fopen(at(RANDO_SD_SETTINGS_FILE), "rb");
	CHECK(f != NULL);
	n = fread(before, 1, sizeof before - 1, f);
	fclose(f);
	before[n] = '\0';
	start();
	key(UI_BUTTON_DOWN);
	key(UI_BUTTON_A);
	key(UI_BUTTON_B);
	key(UI_BUTTON_UP);
	key(UI_BUTTON_A);
	CHECK(holds(RANDO_SD_LAST_GAME_FILE, ALTTP_TEXT));
	CHECK(holds(RANDO_SD_SETTINGS_FILE, before));
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
	start_alttp();
	CHECK(shows_generate(&init));
	CHECK(strstr(render(), "Inverted") != NULL);
}

static void test_unreadable_settings(void)
{
	UiGenerateInit init = defaults();

	make_root();
	write_text(RANDO_SD_SETTINGS_FILE, "{ not json");
	start_alttp();
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
	start_alttp();
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
	CHECK(!rando_app_shell_init(&shell, long_root, NULL, NULL, 0, PAGE, WIDTH));
}

static void test_generate_then_history(void)
{
	make_root();
	write_bytes(RANDO_SD_BASE_ROM_FILE, synth_source, SYNTH_SOURCE_SIZE);
	write_bytes(RANDO_SD_BASE_PATCH_FILE, synth_good_patch.bytes, synth_good_patch.size);
	start_alttp();
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

static uint8_t ladx_base[LADX_ROM_SIZE];

static bool ladx_seed(uint8_t seed[RANDO_LADX_SEED_BYTES])
{
	memset(seed, 0xAB, RANDO_LADX_SEED_BYTES);
	return true;
}

static LadxAssembleStatus ladx_assemble_fake(LadxAssembler *a, const LadxAssembleInput *in, uint8_t *out)
{
	CHECK(in->settings->value[LADX_SETTING_LOGIC] == 2);
	memset(out, 0x11, LADX_ROM_SIZE);
	a->spoiler.built = 1;
	return LADX_ASSEMBLE_OK;
}

static bool ladx_spoiler_fake(const LadxAssembler *a, FILE *out)
{
	(void)a;
	return fputs("{\"Bosses\": {\"D1\": \"Moldorm\"}}\n", out) != EOF;
}

/* LADX through the shell: settings restored, a run with a fake assembler, its seed in LADX's History. */
static void test_ladx(void)
{
	LadxSettings settings;
	char cwd[512];

	make_root();
	ladx_settings_defaults(&settings);
	settings.value[LADX_SETTING_LOGIC] = 2;
	make_dir(RANDO_SD_LADX_DIR);
	CHECK(rando_ladx_last_settings_save(at(RANDO_SD_LADX_SETTINGS_FILE), &settings));
	CHECK(getcwd(cwd, sizeof cwd) != NULL);
	snprintf(text, sizeof text, "%s/../../data/ladx", cwd);
	CHECK(symlink(text, at(RANDO_SD_LADX_TABLES_DIR)) == 0);
	memset(ladx_base, 0x5A, sizeof ladx_base);
	write_bytes(RANDO_SD_LADX_BASE_FILE, ladx_base, sizeof ladx_base);
	start();
	ladx_sha1(ladx_base, sizeof ladx_base, shell.ladx_gen.base_sha1);
	shell.ladx_gen.seed = ladx_seed;
	shell.ladx_gen.date = fixed_date;
	shell.ladx_gen.assemble = ladx_assemble_fake;
	shell.ladx_gen.spoiler = ladx_spoiler_fake;
	key(UI_BUTTON_DOWN);
	key(UI_BUTTON_A);
	CHECK(ui_app_view(&shell.app) == UI_APP_VIEW_LADX);
	CHECK(strstr(render(), "< Hard >") != NULL);
	key(UI_BUTTON_R2);
	key(UI_BUTTON_R2);
	key(UI_BUTTON_R2);
	key(UI_BUTTON_R2);
	key(UI_BUTTON_R2);
	key(UI_BUTTON_R2);
	key(UI_BUTTON_R2);
	key(UI_BUTTON_RIGHT);
	key(UI_BUTTON_START);
	ui_app_run(&shell.app);
	test_snapshot(TEST, SNAPSHOTS "app_shell_ladx_generated.txt", render());
	key(UI_BUTTON_A);
	key(UI_BUTTON_R1);
	key(UI_BUTTON_A);
	test_snapshot(TEST, SNAPSHOTS "app_shell_ladx_history.txt", render());
	CHECK(rando_ladx_last_settings_load(at(RANDO_SD_LADX_SETTINGS_FILE), &settings) == RANDO_LAST_SETTINGS_LOADED);
	CHECK(settings.value[LADX_SETTING_LOGIC] == 2);
	key(UI_BUTTON_X);
	key(UI_BUTTON_A);
	CHECK(strstr(render(), "No seeds yet") != NULL);
	CHECK(remove(at(RANDO_SD_LADX_TABLES_DIR)) == 0);
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
	test_remembered_game();
	test_invalid_memory_replaced();
	test_failed_memory_write();
	test_alttp_settings_untouched();
	test_generate_then_history();
	test_ladx();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
