#include "rando_settings.h"
#include "support.h"
#include "ui_app.h"
#include "ui_grid.h"
#include "ui_strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_ui_app"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define SNAPSHOTS "unit/snapshots/"
#define PAGE (UI_GRID_DEFAULT_LINES - 4)
#define WIDTH (UI_GRID_DEFAULT_COLS - 2)

/* The fake SD side: counts every call, scans the scripted lists in turn, replies as scripted. */
typedef struct Fake {
	int run_calls, scan_calls, spoiler_calls, delete_calls, remember_calls;
	UiGame remembered;
	UiGenRequest request;
	UiGenOutcome reply;
	UiHistoryList scans[2];
	int ladx_run_calls, ladx_scan_calls;
	UiGenRequest ladx_request;
	UiHistoryList ladx_scans[2];
} Fake;

static UiApp app;
static UiAppInit init;
static Fake fake;
static UiGenerate ref_generate;
static UiHistory ref_history;
static char text[UI_GRID_TEXT_MAX];

static void fake_run(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	Fake *f = ctx;

	f->run_calls++;
	f->request = *request;
	*outcome = f->reply;
}

static void fake_scan(void *ctx, UiHistoryList *out)
{
	Fake *f = ctx;

	*out = f->scans[f->scan_calls < 2 ? f->scan_calls : 1];
	f->scan_calls++;
}

static void fake_ladx_run(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	Fake *f = ctx;

	f->ladx_run_calls++;
	f->ladx_request = *request;
	*outcome = f->reply;
}

static void fake_ladx_scan(void *ctx, UiHistoryList *out)
{
	Fake *f = ctx;

	*out = f->ladx_scans[f->ladx_scan_calls < 2 ? f->ladx_scan_calls : 1];
	f->ladx_scan_calls++;
}

static bool fake_spoiler(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out)
{
	static const char outline[] = "Bosses\n  Ganon: Ganon\n";
	Fake *f = ctx;

	(void)entry;
	f->spoiler_calls++;
	out->len = strlen(outline);
	memcpy(out->text, outline, out->len);
	out->cut = false;
	return true;
}

static void fake_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome)
{
	Fake *f = ctx;

	(void)entry;
	f->delete_calls++;
	outcome->ok = true;
	outcome->list_stale = false;
	outcome->status = UI_STR_NONE;
}

static void fake_remember(void *ctx, UiGame game)
{
	Fake *f = ctx;

	f->remember_calls++;
	f->remembered = game;
}

static UiHistoryEntry entry_of(const char *code, const char *rom, int day, int hour, int minute, uint32_t seed)
{
	UiHistoryEntry e;
	int id;

	memset(&e, 0, sizeof e);
	snprintf(e.name.code, sizeof e.name.code, "%s", code);
	snprintf(e.name.rom, sizeof e.name.rom, "%s", rom);
	e.date.year = 2026;
	e.date.month = day > 20 ? 10 : 11;
	e.date.day = day;
	e.date.hour = hour;
	e.date.minute = minute;
	e.date.second = 27;
	e.seed = seed;
	rando_options_init(&e.options);
	e.heart_beep = RANDO_HEART_BEEP_HALF;
	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		rando_setting_set(&e.options, &e.heart_beep, (RandoSettingId)id, rando_setting_default((RandoSettingId)id));
	e.has_spoiler = true;
	return e;
}

static UiHistoryEntry ladx_entry_of(const char *code, const char *rom, const char *seed, bool spoiler)
{
	UiHistoryEntry e;

	memset(&e, 0, sizeof e);
	e.game = UI_GAME_LADX;
	snprintf(e.name.code, sizeof e.name.code, "%s", code);
	snprintf(e.name.rom, sizeof e.name.rom, "%s", rom);
	e.date.year = 2026;
	e.date.month = 11;
	e.date.day = 7;
	e.date.hour = 18;
	e.date.minute = 5;
	snprintf(e.ladx_seed, sizeof e.ladx_seed, "%s", seed);
	ladx_settings_defaults(&e.ladx);
	e.ladx.value[LADX_SETTING_GOAL] = (uint8_t)ladx_setting_value_find(LADX_SETTING_GOAL, "instruments");
	e.has_spoiler = spoiler;
	return e;
}

/* Scan 0: two seeds on the card at start; scan 1: a new seed on top of them. LADX: one, then two. */
static void script_scans(void)
{
	fake.ladx_scans[0].entries[0] =
		ladx_entry_of("3E4F50", "Normal-Instruments-3E4F50.gbc", "00112233445566778899aabbccddeeff", true);
	fake.ladx_scans[0].count = 1;
	fake.ladx_scans[1].entries[0] =
		ladx_entry_of("0A1B2C", "Hard-Instruments-0A1B2C.gbc", "ffeeddccbbaa99887766554433221100", false);
	fake.ladx_scans[1].entries[1] = fake.ladx_scans[0].entries[0];
	fake.ladx_scans[1].count = 2;

	UiHistoryList *first = &fake.scans[0], *second = &fake.scans[1];

	first->entries[0] = entry_of("2B3C4D", "Open-Ganon-2B3C4D.sfc", 30, 9, 41, 7);
	first->entries[1] = entry_of("1CA10A", "Standard-Ganon-1CA10A.sfc", 28, 23, 59, 1);
	first->count = 2;
	second->entries[0] = entry_of("1CA10A", "Open-Ganon-1CA10A.sfc", 1, 14, 3, 1);
	second->entries[1] = first->entries[0];
	second->entries[2] = first->entries[1];
	second->count = 3;
}

static UiGenOutcome success(void)
{
	UiGenOutcome o;

	memset(&o, 0, sizeof o);
	o.ok = true;
	snprintf(o.rom, sizeof o.rom, "%s", "Open-Ganon-1CA10A.sfc");
	o.spoiler = true;
	o.status = UI_STR_NONE;
	o.status_arg = UI_STR_NONE;
	o.advice = UI_STR_NONE;
	return o;
}

static UiGenOutcome failure(void)
{
	UiGenOutcome o = success();

	o.ok = false;
	o.rom[0] = '\0';
	o.status = UI_STR_GEN_BASE_MISSING;
	o.advice = UI_STR_ADVICE_BASE;
	return o;
}

static void start(void)
{
	memset(&fake, 0, sizeof fake);
	script_scans();
	fake.reply = success();
	memset(&init, 0, sizeof init);
	init.strings = ui_str_english();
	init.page = PAGE;
	init.width = WIDTH;
	ui_generate_init_defaults(&init.generate, ui_str_english(), PAGE);
	init.run = fake_run;
	init.run_ctx = &fake;
	init.scan = fake_scan;
	init.spoiler_fn = fake_spoiler;
	init.delete_fn = fake_delete;
	init.history_ctx = &fake;
	init.game = UI_GAME_ALTTP;
	init.remember = fake_remember;
	init.remember_ctx = &fake;
	ui_ladx_generate_init_defaults(&init.ladx.generate, ui_str_english(), PAGE);
	init.ladx.run = fake_ladx_run;
	init.ladx.run_ctx = &fake;
	init.ladx.scan = fake_ladx_scan;
	init.ladx.spoiler_fn = fake_spoiler;
	init.ladx.delete_fn = fake_delete;
	init.ladx.history_ctx = &fake;
	ui_app_init(&app, &init);
}

static UiAppEvent key(UiButton button)
{
	return ui_app_key(&app, button);
}

/* The app with ALttP opened from the choice screen, as every launch reaches it. */
static void start_alttp(void)
{
	start();
	CHECK(key(UI_BUTTON_A) == UI_APP_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
}

/* The reference tabs, initialised as the app's are, driven alongside it by the tests. */
static void start_refs(int scan)
{
	UiHistoryInit h;

	ui_generate_init(&ref_generate, &init.generate);
	h.strings = ui_str_english();
	h.page = PAGE;
	h.width = WIDTH;
	h.list = &fake.scans[scan];
	h.spoiler_fn = fake_spoiler;
	h.delete_fn = fake_delete;
	h.ctx = &fake;
	ui_history_init(&ref_history, &h);
}

static const char *render_screen(const UiScreen *screen, char *buf, size_t size)
{
	static UiGrid grid;

	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, screen));
	CHECK(ui_grid_text(&grid, buf, size) > 0);
	return buf;
}

/* The text form of the app's screen at 40 x 15. */
static const char *render(void)
{
	static UiScreen screen;

	ui_app_screen(&app, &screen);
	return render_screen(&screen, text, sizeof text);
}

static char trace[64 * 1024];

static void trace_reset(void)
{
	trace[0] = '\0';
}

static void trace_screen(const char *header)
{
	size_t used = strlen(trace);

	snprintf(trace + used, sizeof trace - used, "@ %s\n%s", header, render());
}

static bool shows_ref_generate(void)
{
	static UiScreen screen;
	static char ref[UI_GRID_TEXT_MAX];

	ui_generate_screen(&ref_generate, &screen);
	return strcmp(render(), render_screen(&screen, ref, sizeof ref)) == 0;
}

static bool shows_ref_history(void)
{
	static UiScreen screen;
	static char ref[UI_GRID_TEXT_MAX];

	ui_history_screen(&ref_history, &screen);
	return strcmp(render(), render_screen(&screen, ref, sizeof ref)) == 0;
}

/* A flow run alone on request with the same reply, through its own fake. */
static bool shows_ref_flow(const UiGenRequest *request)
{
	static Fake ref_fake;
	static UiGenFlow ref_flow;
	static UiScreen screen;
	static char ref[UI_GRID_TEXT_MAX];

	memset(&ref_fake, 0, sizeof ref_fake);
	ref_fake.reply = fake.reply;
	ui_genflow_init(&ref_flow, ui_str_english(), fake_run, &ref_fake);
	ui_genflow_start(&ref_flow, request);
	ui_genflow_run(&ref_flow);
	ui_genflow_screen(&ref_flow, &screen);
	return strcmp(render(), render_screen(&screen, ref, sizeof ref)) == 0;
}

static void test_init(void)
{
	start();
	CHECK(fake.scan_calls == 1);
	CHECK(fake.run_calls == 0 && fake.remember_calls == 0);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
	CHECK(!ui_app_pending(&app));
	test_snapshot(TEST, SNAPSHOTS "games_first_launch.txt", render());
	init.game = UI_GAME_LADX;
	ui_app_init(&app, &init);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
	test_snapshot(TEST, SNAPSHOTS "games_ladx_remembered.txt", render());
}

static void test_open_alttp(void)
{
	start();
	start_refs(0);
	CHECK(key(UI_BUTTON_A) == UI_APP_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	CHECK(fake.remember_calls == 1 && fake.remembered == UI_GAME_ALTTP);
	CHECK(fake.scan_calls == 1);
	CHECK(shows_ref_generate());
	init.remember = NULL;
	ui_app_init(&app, &init);
	CHECK(key(UI_BUTTON_A) == UI_APP_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
	CHECK(fake.remember_calls == 1);
	CHECK(shows_ref_generate());
}

static void keys(UiButton button, int n)
{
	while (n-- > 0)
		CHECK(key(button) == UI_APP_NONE);
}

/* The rendered screen, kept for comparison. */
static char before[UI_GRID_TEXT_MAX];

static void remember(void)
{
	snprintf(before, sizeof before, "%s", render());
}

static bool unchanged(void)
{
	return strcmp(before, render()) == 0;
}

static void test_tabs_switch_both_ways(void)
{
	UiButton b[2] = {UI_BUTTON_L1, UI_BUTTON_R1};
	int i;

	for (i = 0; i < 2; i++) {
		start_alttp();
		start_refs(0);
		keys(b[i], 1);
		CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
		CHECK(shows_ref_history());
		keys(b[i], 1);
		CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
		CHECK(shows_ref_generate());
	}
	CHECK(fake.scan_calls == 1);
}

static void test_tabs_keep_their_view(void)
{
	start_alttp();
	start_refs(0);
	keys(UI_BUTTON_DOWN, 3);
	CHECK(ui_generate_key(&ref_generate, UI_BUTTON_DOWN) == UI_GENERATE_NONE);
	CHECK(ui_generate_key(&ref_generate, UI_BUTTON_DOWN) == UI_GENERATE_NONE);
	CHECK(ui_generate_key(&ref_generate, UI_BUTTON_DOWN) == UI_GENERATE_NONE);
	keys(UI_BUTTON_R1, 1);
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_DOWN) == UI_HISTORY_NONE);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_A) == UI_HISTORY_NONE);
	CHECK(fake.spoiler_calls == 2);
	CHECK(shows_ref_history());
	keys(UI_BUTTON_L1, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	CHECK(shows_ref_generate());
	keys(UI_BUTTON_R1, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	CHECK(shows_ref_history());
	keys(UI_BUTTON_X, 1);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_X) == UI_HISTORY_NONE);
	remember();
	keys(UI_BUTTON_L1, 1);
	keys(UI_BUTTON_R1, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	CHECK(unchanged());
	CHECK(shows_ref_history());
	CHECK(fake.delete_calls == 0);
}

static void test_generation(void)
{
	UiGenRequest expected;
	UiButton b;

	memset(&expected, 0, sizeof expected);
	start_alttp();
	start_refs(0);
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_RIGHT, 1);
	keys(UI_BUTTON_UP, 3);
	keys(UI_BUTTON_RIGHT, 1);
	ui_generate_settings(&app.generate, &expected.options, &expected.heart_beep, &expected.spoiler);
	CHECK(expected.spoiler);
	remember();
	keys(UI_BUTTON_START, 1);
	CHECK(ui_app_pending(&app));
	CHECK(fake.run_calls == 0);
	test_snapshot(TEST, SNAPSHOTS "genflow_generating.txt", render());
	for (b = UI_BUTTON_UP; b < UI_BUTTON_COUNT; b = (UiButton)(b + 1))
		CHECK(key(b) == UI_APP_NONE);
	CHECK(ui_app_pending(&app));
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	CHECK(fake.run_calls == 0 && fake.scan_calls == 1 && fake.spoiler_calls == 0 && fake.delete_calls == 0);
	test_snapshot(TEST, SNAPSHOTS "genflow_generating.txt", render());
	ui_app_run(&app);
	CHECK(!ui_app_pending(&app));
	CHECK(fake.run_calls == 1);
	CHECK(memcmp(&fake.request.options, &expected.options, sizeof expected.options) == 0);
	CHECK(fake.request.heart_beep == expected.heart_beep);
	CHECK(fake.request.spoiler);
	CHECK(shows_ref_flow(&expected));
	keys(UI_BUTTON_A, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	CHECK(unchanged());
	CHECK(fake.run_calls == 1);
}

static void test_rescan_after_ok(void)
{
	start_alttp();
	keys(UI_BUTTON_START, 1);
	ui_app_run(&app);
	CHECK(fake.scan_calls == 2);
	keys(UI_BUTTON_B, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	keys(UI_BUTTON_R1, 1);
	start_refs(1);
	CHECK(shows_ref_history());
	test_snapshot(TEST, SNAPSHOTS "app_history_rescanned.txt", render());
}

static void test_no_rescan_after_failure(void)
{
	start_alttp();
	start_refs(0);
	keys(UI_BUTTON_R1, 1);
	keys(UI_BUTTON_DOWN, 1);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_DOWN) == UI_HISTORY_NONE);
	keys(UI_BUTTON_L1, 1);
	fake.reply = failure();
	keys(UI_BUTTON_START, 1);
	ui_app_run(&app);
	CHECK(fake.run_calls == 1 && fake.scan_calls == 1);
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_R1, 1);
	CHECK(shows_ref_history());
}

static void test_quit(void)
{
	start();
	CHECK(key(UI_BUTTON_B) == UI_APP_QUIT);
	start();
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
	start_alttp();
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
	start_alttp();
	keys(UI_BUTTON_R1, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
	start_alttp();
	keys(UI_BUTTON_R1, 1);
	keys(UI_BUTTON_X, 1);
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
	CHECK(fake.delete_calls == 0);
	start();
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
	start_alttp();
	keys(UI_BUTTON_R1, 1);
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
	remember();
	CHECK(key(UI_BUTTON_DOWN) == UI_APP_QUIT);
	CHECK(key(UI_BUTTON_L1) == UI_APP_QUIT);
	CHECK(key(UI_BUTTON_B) == UI_APP_QUIT);
	CHECK(ui_app_input(&app, UI_BUTTON_A, true) == UI_APP_QUIT);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
	CHECK(unchanged());
	start_alttp();
	keys(UI_BUTTON_START, 1);
	ui_app_run(&app);
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
	remember();
	CHECK(key(UI_BUTTON_A) == UI_APP_QUIT);
	CHECK(key(UI_BUTTON_START) == UI_APP_QUIT);
	CHECK(!ui_app_pending(&app));
	CHECK(unchanged());
	CHECK(fake.run_calls == 1);
}

static void test_back_to_games(void)
{
	start_alttp();
	CHECK(key(UI_BUTTON_B) == UI_APP_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
	test_snapshot(TEST, SNAPSHOTS "app_back_to_games.txt", render());
	start_alttp();
	keys(UI_BUTTON_R1, 1);
	CHECK(key(UI_BUTTON_B) == UI_APP_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
	test_snapshot(TEST, SNAPSHOTS "app_back_to_games.txt", render());
	CHECK(key(UI_BUTTON_B) == UI_APP_QUIT);
}

static void test_back_inside_history(void)
{
	start_alttp();
	start_refs(0);
	keys(UI_BUTTON_R1, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_A) == UI_HISTORY_NONE);
	CHECK(shows_ref_history());
	keys(UI_BUTTON_B, 1);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_B) == UI_HISTORY_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
	CHECK(shows_ref_history());
	keys(UI_BUTTON_X, 1);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_X) == UI_HISTORY_NONE);
	keys(UI_BUTTON_B, 1);
	CHECK(ui_history_key(&ref_history, UI_BUTTON_B) == UI_HISTORY_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
	CHECK(shows_ref_history());
	CHECK(fake.delete_calls == 0);
	keys(UI_BUTTON_B, 1);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
}

static void test_round_trip_keeps_alttp(void)
{
	start_alttp();
	keys(UI_BUTTON_DOWN, 4);
	keys(UI_BUTTON_RIGHT, 2);
	remember();
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	CHECK(unchanged());
	CHECK(fake.remember_calls == 2 && fake.remembered == UI_GAME_ALTTP);
	keys(UI_BUTTON_R1, 1);
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	CHECK(unchanged());
	CHECK(fake.scan_calls == 1);
}

static void test_history_fresh_after_round_trip(void)
{
	start_alttp();
	keys(UI_BUTTON_START, 1);
	ui_app_run(&app);
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_B, 1);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_R1, 1);
	start_refs(1);
	CHECK(shows_ref_history());
	CHECK(fake.scan_calls == 2);
}

static void start_ladx(void)
{
	start();
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_LADX);
	CHECK(fake.remember_calls == 1 && fake.remembered == UI_GAME_LADX);
}

static void test_ladx_tabs(void)
{
	static UiScreen screen;

	start_ladx();
	CHECK(fake.scan_calls == 1 && fake.ladx_scan_calls == 1);
	ui_app_screen(&app, &screen);
	CHECK(ui_screen_valid(&screen));
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_initial.txt", render());
	keys(UI_BUTTON_R1, 1);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	test_snapshot(TEST, SNAPSHOTS "app_ladx_history.txt", render());
	keys(UI_BUTTON_A, 1);
	CHECK(fake.spoiler_calls == 1);
	trace_reset();
	trace_screen("a");
	keys(UI_BUTTON_R2, 4);
	trace_screen("r2*4");
	test_snapshot(TEST, SNAPSHOTS "app_ladx_history_detail.txt", trace);
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_L1, 1);
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_initial.txt", render());
	keys(UI_BUTTON_B, 1);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
	test_snapshot(TEST, SNAPSHOTS "games_ladx_remembered.txt", render());
	keys(UI_BUTTON_UP, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_GENERATE);
	CHECK(fake.remember_calls == 2 && fake.remembered == UI_GAME_ALTTP);
	CHECK(key(UI_BUTTON_B) == UI_APP_NONE);
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(key(UI_BUTTON_MENU) == UI_APP_QUIT);
}

static void test_ladx_generation(void)
{
	start_ladx();
	keys(UI_BUTTON_RIGHT, 1);
	remember();
	snprintf(fake.reply.rom, sizeof fake.reply.rom, "%s", "Hard-Vanilla-0A1B2C.gbc");
	keys(UI_BUTTON_START, 1);
	CHECK(ui_app_pending(&app));
	test_snapshot(TEST, SNAPSHOTS "genflow_generating.txt", render());
	ui_app_run(&app);
	CHECK(fake.ladx_run_calls == 1 && fake.run_calls == 0);
	CHECK(fake.ladx_request.game == UI_GAME_LADX && !fake.ladx_request.spoiler);
	CHECK(fake.ladx_request.ladx.value[LADX_SETTING_LOGIC] == 2);
	CHECK(fake.ladx_scan_calls == 2 && fake.scan_calls == 1);
	test_snapshot(TEST, SNAPSHOTS "genflow_ladx_ready.txt", render());
	keys(UI_BUTTON_A, 1);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_LADX);
	CHECK(unchanged());
	keys(UI_BUTTON_R1, 1);
	test_snapshot(TEST, SNAPSHOTS "app_ladx_history_rescanned.txt", render());
	start_ladx();
	fake.reply.list_stale = true;
	snprintf(fake.reply.rom, sizeof fake.reply.rom, "%s", "Normal-Vanilla-0A1B2C.gbc");
	keys(UI_BUTTON_START, 1);
	ui_app_run(&app);
	trace_reset();
	trace_screen("start");
	test_snapshot(TEST, SNAPSHOTS "genflow_ladx_ready_list_stale.txt", trace);
}

static void test_ladx_failures(void)
{
	static const struct {
		UiStrId status;
		const char *snapshot;
	} cases[] = {
		{UI_STR_GEN_BASE_MISSING, SNAPSHOTS "genflow_ladx_base_missing.txt"},
		{UI_STR_LADX_BASE_SHA1, SNAPSHOTS "genflow_ladx_base_wrong.txt"},
	};
	size_t i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		start_ladx();
		fake.reply = failure();
		fake.reply.status = cases[i].status;
		fake.reply.advice = UI_STR_LADX_ADVICE_BASE;
		keys(UI_BUTTON_START, 1);
		ui_app_run(&app);
		CHECK(fake.ladx_run_calls == 1 && fake.ladx_scan_calls == 1);
		test_snapshot(TEST, cases[i].snapshot, render());
		keys(UI_BUTTON_B, 1);
		CHECK(ui_app_view(&app) == UI_APP_VIEW_LADX);
		test_snapshot(TEST, SNAPSHOTS "ladx_generate_initial.txt", render());
	}
}

static void test_round_trip_keeps_ladx(void)
{
	start_ladx();
	keys(UI_BUTTON_R2, 2);
	keys(UI_BUTTON_RIGHT, 1);
	remember();
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_UP, 1);
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_DOWN, 2);
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_LADX);
	CHECK(unchanged());
}

static void test_raw_input_is_filtered(void)
{
	start_alttp();
	start_refs(0);
	CHECK(ui_app_input(&app, UI_BUTTON_MENU, true) == UI_APP_NONE);
	CHECK(ui_app_input(&app, UI_BUTTON_MENU, false) == UI_APP_QUIT);
	start_alttp();
	CHECK(ui_app_input(&app, UI_BUTTON_MENU, true) == UI_APP_NONE);
	CHECK(ui_app_input(&app, UI_BUTTON_DOWN, true) == UI_APP_NONE);
	CHECK(ui_app_input(&app, UI_BUTTON_DOWN, false) == UI_APP_NONE);
	CHECK(ui_app_input(&app, UI_BUTTON_MENU, false) == UI_APP_NONE);
	CHECK(shows_ref_generate());
	CHECK(ui_app_input(&app, UI_BUTTON_R1, true) == UI_APP_NONE);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	CHECK(ui_app_input(&app, UI_BUTTON_R1, false) == UI_APP_NONE);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	test_snapshot(TEST, SNAPSHOTS "app_history_two_seeds.txt", render());
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_init();
	test_open_alttp();
	test_tabs_switch_both_ways();
	test_tabs_keep_their_view();
	test_generation();
	test_rescan_after_ok();
	test_no_rescan_after_failure();
	test_quit();
	test_back_to_games();
	test_back_inside_history();
	test_round_trip_keeps_alttp();
	test_history_fresh_after_round_trip();
	test_ladx_tabs();
	test_ladx_generation();
	test_ladx_failures();
	test_round_trip_keeps_ladx();
	test_raw_input_is_filtered();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
