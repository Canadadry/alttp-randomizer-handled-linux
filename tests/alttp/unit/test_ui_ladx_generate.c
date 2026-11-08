#include "ladx_settings.h"
#include "support.h"
#include "ui_grid.h"
#include "ui_ladx_generate.h"
#include "ui_ladx_settings.h"
#include "ui_strings.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_ui_ladx_generate"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define SNAPSHOTS "unit/snapshots/"
#define PAGE (UI_GRID_DEFAULT_LINES - 4)
#define TRACE_MAX (64 * 1024)

static UiLadxGenerate gen;
static UiScreen screen;
static char text[UI_GRID_TEXT_MAX];
static char trace[TRACE_MAX];

static void start_with(const LadxSettings *settings, bool unreadable)
{
	UiLadxGenerateInit init;

	ui_ladx_generate_init_defaults(&init, ui_str_english(), PAGE);
	if (settings != NULL)
		init.settings = *settings;
	init.settings_unreadable = unreadable;
	ui_ladx_generate_init(&gen, &init);
}

static void start(void)
{
	start_with(NULL, false);
}

/* The text form of the current screen at 40 x 15. */
static const char *render(void)
{
	static UiGrid grid;

	ui_ladx_generate_screen(&gen, &screen);
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, &screen));
	CHECK(ui_grid_text(&grid, text, sizeof text) > 0);
	return text;
}

static void trace_reset(void)
{
	trace[0] = '\0';
}

static void trace_screen(const char *header)
{
	size_t used = strlen(trace);

	snprintf(trace + used, sizeof trace - used, "@ %s\n%s", header, render());
}

static UiGenerateEvent key(UiButton b)
{
	return ui_ladx_generate_key(&gen, b);
}

static void keys(UiButton b, int times)
{
	while (times-- > 0)
		CHECK(key(b) == UI_GENERATE_NONE);
}

static const char *str(UiStrId id)
{
	return ui_str(ui_str_english(), id);
}

/* The screen's row for setting id, NULL when not shown. */
static const UiRow *row_of(LadxSettingId id)
{
	int i;

	render();
	for (i = 0; i < screen.row_count; i++)
		if (screen.rows[i].kind == UI_ROW_SETTING && strcmp(screen.rows[i].label, str(ui_ladx_setting_label(id))) == 0)
			return &screen.rows[i];
	return NULL;
}

static const UiRow *action_row(void)
{
	render();
	return &screen.rows[screen.row_count - 1];
}

/* Moves the cursor down onto id. */
static void select_setting(LadxSettingId id)
{
	int guard = 0;

	start();
	while (strcmp(screen.rows[screen.cursor].label, str(ui_ladx_setting_label(id))) != 0) {
		CHECK(guard++ < 2 * UI_LADX_ROWS_MAX);
		key(UI_BUTTON_DOWN);
		render();
	}
}

static void test_map(void)
{
	int id, v, c, r;

	for (c = 0; c < LADX_CATEGORY_COUNT; c++)
		CHECK(ui_ladx_category_label((LadxSettingCategory)c) != UI_STR_NONE);
	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		LadxSettingId s = (LadxSettingId)id;
		int label = (int)strlen(str(ui_ladx_setting_label(s)));

		CHECK(ui_ladx_setting_label(s) != UI_STR_NONE && ui_ladx_setting_help(s) != UI_STR_NONE);
		for (v = 0; v < ladx_setting_value_count(s); v++) {
			const char *value = str(ui_ladx_setting_value(s, v));

			if (value[0] == '\0' || label > UI_GRID_DEFAULT_COLS - 7 - (int)strlen(value))
				test_fail(TEST, "%s=%s: \"%s\" does not fit beside \"%s\"", ladx_setting_key(s),
					  ladx_setting_value_name(s, v), value, str(ui_ladx_setting_label(s)));
		}
	}
	CHECK(ui_ladx_reason(LADX_REASON_NONE) == UI_STR_NONE);
	for (r = 1; r < LADX_REASON_COUNT; r++)
		CHECK(ui_ladx_reason((LadxReason)r) != UI_STR_NONE &&
		      ui_str_arity(ui_ladx_reason((LadxReason)r)) == (r == LADX_REASON_LADXR_REFUSES));
}

static void test_initial(void)
{
	start();
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_initial.txt", render());
}

static void test_categories(void)
{
	int i;

	start();
	trace_reset();
	trace_screen("init");
	for (i = 0; i < LADX_CATEGORY_COUNT + 1; i++) {
		keys(UI_BUTTON_R2, 1);
		trace_screen("r2");
	}
	keys(UI_BUTTON_L2, 1);
	trace_screen("l2");
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_categories.txt", trace);
}

static void test_greyed_value(void)
{
	select_setting(LADX_SETTING_ENTRANCESHUFFLE);
	trace_reset();
	trace_screen("init");
	keys(UI_BUTTON_RIGHT, 1);
	trace_screen("right");
	CHECK(key(UI_BUTTON_START) == UI_GENERATE_NONE);
	trace_screen("start");
	keys(UI_BUTTON_LEFT, 1);
	trace_screen("left");
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_greyed_value.txt", trace);
}

static void test_greyed_pair(void)
{
	select_setting(LADX_SETTING_BOOMERANG);
	trace_reset();
	keys(UI_BUTTON_RIGHT, 1);
	trace_screen("right");
	CHECK(row_of(LADX_SETTING_BOOMERANG)->disabled && row_of(LADX_SETTING_ACCESSIBILITY)->disabled);
	keys(UI_BUTTON_R2, 6);
	keys(UI_BUTTON_DOWN, 1);
	trace_screen("r2*6 down");
	CHECK(key(UI_BUTTON_A) == UI_GENERATE_NONE);
	trace_screen("a");
	keys(UI_BUTTON_UP, 1);
	trace_screen("up");
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_greyed_pair.txt", trace);
}

/* A row is greyed exactly when its value is unsupported alone or the one pair holds. */
static void test_greyed_exactly_unsupported(void)
{
	int id, v;

	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		LadxSettingId s = (LadxSettingId)id;

		for (v = 0; v < ladx_setting_value_count(s); v++) {
			LadxSettings settings;
			const UiRow *row;
			bool unsupported;

			ladx_settings_defaults(&settings);
			settings.value[id] = (uint8_t)v;
			if (ladx_setting_visible_if(s) != LADX_SETTING_COUNT) {
				LadxSettingId c = ladx_setting_visible_if(s);
				int w;

				for (w = 0; !ladx_setting_visible_for(s, w); w++)
					;
				CHECK(w < ladx_setting_value_count(c));
				if (ladx_setting_value_reason(c, w) != LADX_REASON_NONE)
					continue;
				settings.value[c] = (uint8_t)w;
			}
			start_with(&settings, false);
			row = row_of(s);
			CHECK(row != NULL);
			unsupported = ladx_setting_value_reason(s, v) != LADX_REASON_NONE ||
				      (s == LADX_SETTING_BOOMERANG && v == 0);
			if (row->disabled != unsupported)
				test_fail(TEST, "%s=%s greyed %d, unsupported %d", ladx_setting_key(s),
					  ladx_setting_value_name(s, v), row->disabled, unsupported);
		}
	}
}

/* Validate's goal=maze sets accessibility=all: the pair then holds for the action only. */
static void test_action_validates_first(void)
{
	LadxSettings settings;

	ladx_settings_defaults(&settings);
	settings.value[LADX_SETTING_ACCESSIBILITY] = (uint8_t)ladx_setting_value_find(LADX_SETTING_ACCESSIBILITY, "goal");
	settings.value[LADX_SETTING_BOOMERANG] = (uint8_t)ladx_setting_value_find(LADX_SETTING_BOOMERANG, "default");
	start_with(&settings, false);
	CHECK(!row_of(LADX_SETTING_BOOMERANG)->disabled && !action_row()->disabled);
	CHECK(key(UI_BUTTON_START) == UI_GENERATE_GENERATE);
	settings.value[LADX_SETTING_GOAL] = (uint8_t)ladx_setting_value_find(LADX_SETTING_GOAL, "maze");
	start_with(&settings, false);
	CHECK(!row_of(LADX_SETTING_BOOMERANG)->disabled && !row_of(LADX_SETTING_GOAL)->disabled);
	CHECK(action_row()->disabled);
	CHECK(key(UI_BUTTON_START) == UI_GENERATE_NONE);
	render();
	CHECK(screen.message_kind == UI_MESSAGE_ERROR);
	CHECK(strcmp(screen.message, "LADXR refuses this with Accessibility") == 0);
}

static void test_visible_if(void)
{
	select_setting(LADX_SETTING_GOAL);
	CHECK(row_of(LADX_SETTING_GOALCOUNT) == NULL);
	trace_reset();
	keys(UI_BUTTON_RIGHT, 1);
	trace_screen("right");
	CHECK(row_of(LADX_SETTING_GOALCOUNT) != NULL);
	CHECK(strcmp(screen.rows[screen.cursor].label, str(UI_STR_LADX_SET_GOAL)) == 0);
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_LEFT, 2);
	trace_screen("down left*2");
	keys(UI_BUTTON_UP, 1);
	keys(UI_BUTTON_RIGHT, 2);
	CHECK(row_of(LADX_SETTING_GOALCOUNT) == NULL);
	CHECK(strcmp(screen.rows[screen.cursor].label, str(UI_STR_LADX_SET_GOAL)) == 0);
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_goal_count.txt", trace);
}

static void test_cursor_skips_headings(void)
{
	int i, rows;

	start();
	render();
	rows = screen.row_count;
	CHECK(screen.cursor == 1);
	for (i = 0; i < 2 * rows; i++) {
		keys(UI_BUTTON_DOWN, 1);
		render();
		CHECK(screen.rows[screen.cursor].kind != UI_ROW_TEXT);
	}
	keys(UI_BUTTON_UP, 1);
	render();
	CHECK(screen.rows[screen.cursor].kind != UI_ROW_TEXT);
	start();
	keys(UI_BUTTON_UP, 1);
	render();
	CHECK(screen.cursor == screen.row_count - 1);
	keys(UI_BUTTON_DOWN, 1);
	render();
	CHECK(screen.cursor == 1);
}

static void test_settings_out_and_reset(void)
{
	LadxSettings settings;
	bool spoiler = true;

	start();
	keys(UI_BUTTON_RIGHT, 1);
	keys(UI_BUTTON_A, 1);
	ui_ladx_generate_settings(&gen, &settings, &spoiler);
	CHECK(settings.value[LADX_SETTING_LOGIC] == 3 && !spoiler);
	keys(UI_BUTTON_Y, 1);
	ui_ladx_generate_settings(&gen, &settings, &spoiler);
	CHECK(settings.value[LADX_SETTING_LOGIC] == ladx_setting_default(LADX_SETTING_LOGIC));
	keys(UI_BUTTON_R2, 7);
	render();
	CHECK(strcmp(screen.rows[screen.cursor].label, str(UI_STR_SETTING_SPOILER)) == 0);
	keys(UI_BUTTON_LEFT, 1);
	trace_reset();
	trace_screen("left");
	test_snapshot(TEST, SNAPSHOTS "ladx_generate_spoiler_on.txt", trace);
	ui_ladx_generate_settings(&gen, &settings, &spoiler);
	CHECK(spoiler);
	keys(UI_BUTTON_Y, 1);
	ui_ladx_generate_settings(&gen, &settings, &spoiler);
	CHECK(!spoiler);
	keys(UI_BUTTON_R2, 1);
	keys(UI_BUTTON_LEFT, 1);
	keys(UI_BUTTON_Y, 1);
	CHECK(key(UI_BUTTON_A) == UI_GENERATE_GENERATE);
}

static void test_events(void)
{
	start();
	CHECK(key(UI_BUTTON_B) == UI_GENERATE_BACK);
	CHECK(key(UI_BUTTON_MENU) == UI_GENERATE_QUIT);
	CHECK(key(UI_BUTTON_L1) == UI_GENERATE_TAB_PREV);
	CHECK(key(UI_BUTTON_R1) == UI_GENERATE_TAB_NEXT);
	CHECK(key(UI_BUTTON_X) == UI_GENERATE_NONE && key(UI_BUTTON_SELECT) == UI_GENERATE_NONE);
	CHECK(key(UI_BUTTON_START) == UI_GENERATE_GENERATE);
}

static void test_unreadable_notice(void)
{
	start_with(NULL, true);
	render();
	CHECK(screen.message_kind == UI_MESSAGE_INFO);
	CHECK(strcmp(screen.message, str(UI_STR_GENERATE_SETTINGS_UNREADABLE)) == 0);
	keys(UI_BUTTON_DOWN, 1);
	render();
	CHECK(screen.message_kind == UI_MESSAGE_HELP);
}

int main(void)
{
	test_map();
	test_initial();
	test_categories();
	test_greyed_value();
	test_greyed_pair();
	test_greyed_exactly_unsupported();
	test_action_validates_first();
	test_visible_if();
	test_cursor_skips_headings();
	test_settings_out_and_reset();
	test_events();
	test_unreadable_notice();
	printf("%s: passed\n", TEST);
	return 0;
}
