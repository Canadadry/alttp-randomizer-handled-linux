#include "rando_settings.h"
#include "support.h"
#include "ui_genflow.h"
#include "ui_grid.h"
#include "ui_strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_ui_genflow"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define SNAPSHOTS "unit/snapshots/"

/* The fake generator: counts its calls, records the request and the outcome it was handed, then copies
 * reply into the outcome. */
typedef struct Fake {
	int calls;
	UiGenRequest request;
	UiGenOutcome handed;
	UiGenOutcome reply;
} Fake;

static UiGenFlow flow;
static Fake fake;
static char text[UI_GRID_TEXT_MAX];

static void fake_run(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	Fake *f = ctx;

	f->calls++;
	f->request = *request;
	f->handed = *outcome;
	*outcome = f->reply;
}

static UiGenOutcome success(const char *rom, bool spoiler, bool list_stale)
{
	UiGenOutcome o;

	memset(&o, 0, sizeof o);
	o.ok = true;
	snprintf(o.rom, sizeof o.rom, "%s", rom);
	o.spoiler = spoiler;
	o.list_stale = list_stale;
	o.status = UI_STR_NONE;
	o.status_arg = UI_STR_NONE;
	o.advice = UI_STR_NONE;
	return o;
}

static UiGenOutcome failure(UiStrId status, UiStrId status_arg, UiStrId advice)
{
	UiGenOutcome o = success("", false, false);

	o.ok = false;
	o.status = status;
	o.status_arg = status_arg;
	o.advice = advice;
	return o;
}

static UiGenRequest request_of(int goal, RandoHeartBeep heart_beep, bool spoiler)
{
	UiGenRequest r;
	int id;

	rando_options_init(&r.options);
	r.heart_beep = RANDO_HEART_BEEP_HALF;
	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		rando_setting_set(&r.options, &r.heart_beep, (RandoSettingId)id,
				  rando_setting_default((RandoSettingId)id));
	rando_setting_set(&r.options, &r.heart_beep, RANDO_SETTING_GOAL, goal);
	r.heart_beep = heart_beep;
	r.spoiler = spoiler;
	return r;
}

static void reset(UiGenOutcome reply)
{
	memset(&fake, 0, sizeof fake);
	fake.reply = reply;
	ui_genflow_init(&flow, ui_str_english(), fake_run, &fake);
}

/* The text form of the flow's screen at 40 x 15. */
static const char *render(void)
{
	static UiScreen screen;
	static UiGrid grid;

	ui_genflow_screen(&flow, &screen);
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, &screen));
	CHECK(ui_grid_text(&grid, text, sizeof text) > 0);
	return text;
}

static void generate(UiGenOutcome reply)
{
	UiGenRequest r = request_of(0, RANDO_HEART_BEEP_HALF, false);

	reset(reply);
	ui_genflow_start(&flow, &r);
	ui_genflow_run(&flow);
}

static void test_start_does_not_run(void)
{
	UiGenRequest r = request_of(1, RANDO_HEART_BEEP_QUARTER, true);

	reset(success("Open-Ganon-1CA10A.sfc", true, false));
	CHECK(ui_genflow_phase(&flow) == UI_GENFLOW_PHASE_IDLE);
	CHECK(!ui_genflow_pending(&flow));
	ui_genflow_start(&flow, &r);
	CHECK(ui_genflow_phase(&flow) == UI_GENFLOW_PHASE_GENERATING);
	CHECK(ui_genflow_pending(&flow));
	CHECK(fake.calls == 0);
	test_snapshot(TEST, SNAPSHOTS "genflow_generating.txt", render());
	CHECK(fake.calls == 0);
}

static void test_run_calls_the_generator_once_with_the_request(void)
{
	UiGenRequest r = request_of(1, RANDO_HEART_BEEP_QUARTER, true);
	int id;

	reset(success("Open-Ganon-1CA10A.sfc", true, false));
	ui_genflow_start(&flow, &r);
	ui_genflow_run(&flow);
	CHECK(fake.calls == 1);
	CHECK(!ui_genflow_pending(&flow));
	CHECK(ui_genflow_phase(&flow) == UI_GENFLOW_PHASE_DONE);
	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		if (rando_setting_get(&fake.request.options, fake.request.heart_beep, (RandoSettingId)id) !=
		    rando_setting_get(&r.options, r.heart_beep, (RandoSettingId)id))
			test_fail(TEST, "setting %d not passed to the generator", id);
	CHECK(fake.request.options.hints == r.options.hints);
	CHECK(fake.request.heart_beep == RANDO_HEART_BEEP_QUARTER);
	CHECK(fake.request.spoiler);
	render();
	CHECK(ui_genflow_key(&flow, UI_BUTTON_START) == UI_GENFLOW_NONE);
	CHECK(fake.calls == 1);
}

static void test_run_hands_a_cleared_outcome(void)
{
	UiGenRequest r = request_of(0, RANDO_HEART_BEEP_HALF, false);

	generate(success("Open-Ganon-1CA10A.sfc", true, true));
	CHECK(ui_genflow_key(&flow, UI_BUTTON_A) == UI_GENFLOW_BACK);
	fake.reply = failure(UI_STR_GENERATE_UNSUPPORTED, UI_STR_SETTING_GOAL, UI_STR_GENERATE_ACTION);
	ui_genflow_start(&flow, &r);
	ui_genflow_run(&flow);
	CHECK(fake.calls == 2);
	CHECK(!fake.handed.ok && fake.handed.rom[0] == '\0' && !fake.handed.spoiler && !fake.handed.list_stale);
	CHECK(fake.handed.status == UI_STR_NONE && fake.handed.status_arg == UI_STR_NONE &&
	      fake.handed.advice == UI_STR_NONE);
}

static void test_keys_while_generating_change_nothing(void)
{
	UiGenRequest r = request_of(0, RANDO_HEART_BEEP_HALF, false);
	char before[UI_GRID_TEXT_MAX];
	int b;

	reset(success("Open-Ganon-1CA10A.sfc", true, false));
	ui_genflow_start(&flow, &r);
	snprintf(before, sizeof before, "%s", render());
	for (b = 0; b < UI_BUTTON_COUNT; b++) {
		if (ui_genflow_key(&flow, (UiButton)b) != UI_GENFLOW_NONE)
			test_fail(TEST, "button %d acts while generating", b);
		if (strcmp(render(), before) != 0)
			test_fail(TEST, "button %d changes the generating screen", b);
		CHECK(ui_genflow_phase(&flow) == UI_GENFLOW_PHASE_GENERATING && ui_genflow_pending(&flow));
	}
	CHECK(fake.calls == 0);
}

static void test_keys_when_done(void)
{
	char before[UI_GRID_TEXT_MAX];
	int b;

	generate(success("Open-Ganon-1CA10A.sfc", true, false));
	snprintf(before, sizeof before, "%s", render());
	for (b = 0; b < UI_BUTTON_COUNT; b++) {
		if (b == UI_BUTTON_A || b == UI_BUTTON_B || b == UI_BUTTON_MENU)
			continue;
		if (ui_genflow_key(&flow, (UiButton)b) != UI_GENFLOW_NONE)
			test_fail(TEST, "button %d acts on the result screen", b);
		if (strcmp(render(), before) != 0 || ui_genflow_phase(&flow) != UI_GENFLOW_PHASE_DONE)
			test_fail(TEST, "button %d changes the result screen", b);
	}
	CHECK(ui_genflow_key(&flow, UI_BUTTON_MENU) == UI_GENFLOW_QUIT);
	CHECK(ui_genflow_key(&flow, UI_BUTTON_A) == UI_GENFLOW_BACK);
	CHECK(ui_genflow_phase(&flow) == UI_GENFLOW_PHASE_IDLE);
	CHECK(!ui_genflow_pending(&flow));

	generate(failure(UI_STR_GENERATE_SETTINGS_UNREADABLE, UI_STR_NONE, UI_STR_GENERATE_ACTION));
	CHECK(ui_genflow_key(&flow, UI_BUTTON_B) == UI_GENFLOW_BACK);
	CHECK(ui_genflow_phase(&flow) == UI_GENFLOW_PHASE_IDLE);
	CHECK(fake.calls == 1);
}

static void test_screens(void)
{
	generate(success("Standard-Ganon-1CA10A.sfc", true, false));
	test_snapshot(TEST, SNAPSHOTS "genflow_ready_spoiler_kept.txt", render());
	generate(success("Open-Ganon-2B3C4D.sfc", false, true));
	test_snapshot(TEST, SNAPSHOTS "genflow_ready_list_stale.txt", render());
	generate(failure(UI_STR_GENERATE_UNSUPPORTED, UI_STR_SETTING_GOAL, UI_STR_GENERATE_ACTION));
	test_snapshot(TEST, SNAPSHOTS "genflow_failed_filled.txt", render());
}

static void check_back_hint(void)
{
	UiScreen screen;

	ui_genflow_screen(&flow, &screen);
	CHECK(screen.hint_count == 1);
	CHECK(screen.hints[0].button == UI_HINT_A && !screen.hints[0].primary);
	CHECK(strcmp(screen.hints[0].key, "A") == 0 && strcmp(screen.hints[0].label, "Back") == 0);
}

static void test_hints(void)
{
	UiGenRequest r = request_of(0, RANDO_HEART_BEEP_HALF, false);
	UiScreen screen;

	reset(success("Open-Ganon-1CA10A.sfc", true, false));
	ui_genflow_start(&flow, &r);
	ui_genflow_screen(&flow, &screen);
	CHECK(screen.hint_count == 0);
	generate(success("Open-Ganon-1CA10A.sfc", true, false));
	check_back_hint();
	generate(failure(UI_STR_GENERATE_UNSUPPORTED, UI_STR_SETTING_GOAL, UI_STR_GENERATE_ACTION));
	check_back_hint();
}

static void test_arity_zero_status_ignores_its_argument(void)
{
	UiScreen screen;

	generate(failure(UI_STR_GENERATE_SETTINGS_UNREADABLE, UI_STR_SETTING_GOAL, UI_STR_GENERATE_ACTION));
	ui_genflow_screen(&flow, &screen);
	CHECK(screen.message_kind == UI_MESSAGE_ERROR);
	CHECK(strcmp(screen.message, "Saved settings unreadable: defaults") == 0);
	CHECK(screen.tab_count == 2 && screen.active_tab == 0 && screen.cursor == -1);
	CHECK(screen.row_count == 1 && screen.rows[0].kind == UI_ROW_TEXT);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_start_does_not_run();
	test_run_calls_the_generator_once_with_the_request();
	test_run_hands_a_cleared_outcome();
	test_keys_while_generating_change_nothing();
	test_keys_when_done();
	test_screens();
	test_hints();
	test_arity_zero_status_ignores_its_argument();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
