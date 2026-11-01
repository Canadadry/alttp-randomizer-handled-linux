#include "rando_settings.h"
#include "support.h"
#include "ui_generate.h"
#include "ui_grid.h"
#include "ui_settings.h"
#include "ui_strings.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_ui_generate"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define SNAPSHOTS "unit/snapshots/"
#define ROW_SPOILER RANDO_SETTING_COUNT
#define ROW_ACTION (RANDO_SETTING_COUNT + 1)

static UiGenerate gen;
static char text[UI_GRID_TEXT_MAX];

static void init_defaults(void)
{
	UiGenerateInit init;

	ui_generate_init_defaults(&init, ui_str_english(), UI_GRID_DEFAULT_LINES - 4);
	ui_generate_init(&gen, &init);
}

/* The text form of the current screen at 40 x 15. */
static const char *render(void)
{
	static UiScreen screen;
	static UiGrid grid;

	ui_generate_screen(&gen, &screen);
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, &screen));
	CHECK(ui_grid_text(&grid, text, sizeof text) > 0);
	return text;
}

static UiGenerateEvent key(UiButton b)
{
	return ui_generate_key(&gen, b);
}

static void keys(UiButton b, int times)
{
	while (times-- > 0)
		CHECK(key(b) == UI_GENERATE_NONE);
}

static void test_map(void)
{
	int id, other, v, w;

	CHECK(UI_SETTING_VALUES_MAX >= 9);
	for (id = 0; id < RANDO_SETTING_COUNT; id++) {
		RandoSettingId s = (RandoSettingId)id;

		CHECK(rando_setting_value_count(s) <= UI_SETTING_VALUES_MAX);
		if (ui_setting_label(s) == UI_STR_NONE)
			test_fail(TEST, "%s has no label id", rando_setting_key(s));
		for (other = 0; other < id; other++)
			if (ui_setting_label(s) == ui_setting_label((RandoSettingId)other))
				test_fail(TEST, "%s and %s share a label id", rando_setting_key(s),
					  rando_setting_key((RandoSettingId)other));
		for (v = 0; v < rando_setting_value_count(s); v++) {
			UiStrId vid = ui_setting_value(s, v);

			if (vid == UI_STR_NONE)
				test_fail(TEST, "%s value %d has no id", rando_setting_key(s), v);
			for (other = 0; other <= id; other++)
				for (w = 0; w < (other == id ? v : rando_setting_value_count((RandoSettingId)other)); w++)
					if (vid == ui_setting_value((RandoSettingId)other, w))
						test_fail(TEST, "%s value %d shares its id", rando_setting_key(s), v);
		}
	}
	CHECK(strcmp(ui_str(NULL, ui_setting_label(RANDO_SETTING_GOAL)), "Goal") == 0);
	CHECK(strcmp(ui_str(NULL, ui_setting_value(RANDO_SETTING_GOAL, RANDO_OPTION_GOAL_FAST_GANON)), "Fast Ganon") == 0);
	CHECK(strcmp(ui_str(NULL, ui_setting_value(RANDO_SETTING_HINTS, 1)), "Off") == 0);
	CHECK(strcmp(ui_str(NULL, ui_setting_value(RANDO_SETTING_HEART_BEEP, RANDO_HEART_BEEP_HALF)), "Half Speed") == 0);
}

/* The cursor's grid line, from the text form: its tag is '>' or 'X'. */
static const char *cursor_line(char line[UI_GRID_COLS_MAX + 1])
{
	const char *p = render();

	for (;;) {
		const char *end = strchr(p, '\n');

		if (end == NULL)
			test_fail(TEST, "no cursor line in\n%s", text);
		if (*p == '>' || *p == 'X') {
			memcpy(line, p + 2, UI_GRID_DEFAULT_COLS);
			line[UI_GRID_DEFAULT_COLS] = '\0';
			return line;
		}
		p = end + 1;
	}
}

static void test_every_row_and_value(void)
{
	char line[UI_GRID_COLS_MAX + 1];
	int row, v, count;

	for (row = 0; row <= ROW_SPOILER; row++) {
		const char *label = row == ROW_SPOILER ? "Spoiler in History"
						       : ui_str(NULL, ui_setting_label((RandoSettingId)row));

		init_defaults();
		keys(UI_BUTTON_DOWN, row);
		count = row == ROW_SPOILER ? 2 : rando_setting_value_count((RandoSettingId)row);
		for (v = 0; v < count; v++) {
			RandoOptions options;
			RandoHeartBeep heart_beep;
			bool spoiler;
			const char *value;
			int now;

			ui_generate_settings(&gen, &options, &heart_beep, &spoiler);
			now = row == ROW_SPOILER ? (spoiler ? 1 : 0)
						 : rando_setting_get(&options, heart_beep, (RandoSettingId)row);
			value = row == ROW_SPOILER ? (spoiler ? "On" : "Off")
						   : ui_str(NULL, ui_setting_value((RandoSettingId)row, now));
			cursor_line(line);
			if (strstr(line, label) == NULL || strstr(line, value) == NULL || strchr(line, '~') != NULL)
				test_fail(TEST, "row %d value %d: \"%s\" does not show \"%s\" and \"%s\" uncut", row, v, line,
					  label, value);
			CHECK(key(UI_BUTTON_RIGHT) == UI_GENERATE_NONE);
		}
	}
	init_defaults();
	keys(UI_BUTTON_UP, 1);
	CHECK(strstr(cursor_line(line), "Generate Seed") != NULL);
}

static void test_events(void)
{
	int row;

	for (row = 0; row <= ROW_ACTION; row++) {
		init_defaults();
		keys(UI_BUTTON_DOWN, row);
		if (key(UI_BUTTON_START) != UI_GENERATE_GENERATE)
			test_fail(TEST, "Start on row %d does not generate", row);
		CHECK(key(UI_BUTTON_A) == (row == ROW_ACTION ? UI_GENERATE_GENERATE : UI_GENERATE_NONE));
	}
	init_defaults();
	CHECK(key(UI_BUTTON_B) == UI_GENERATE_QUIT);
	CHECK(key(UI_BUTTON_MENU) == UI_GENERATE_QUIT);
	CHECK(key(UI_BUTTON_L1) == UI_GENERATE_TAB_PREV);
	CHECK(key(UI_BUTTON_R1) == UI_GENERATE_TAB_NEXT);
	{
		static char before[UI_GRID_TEXT_MAX];

		strcpy(before, render());
		CHECK(key(UI_BUTTON_X) == UI_GENERATE_NONE);
		CHECK(key(UI_BUTTON_SELECT) == UI_GENERATE_NONE);
		CHECK(strcmp(before, render()) == 0);
		keys(UI_BUTTON_UP, 1);
		strcpy(before, render());
		keys(UI_BUTTON_LEFT, 1);
		keys(UI_BUTTON_RIGHT, 1);
		keys(UI_BUTTON_Y, 1);
		CHECK(strcmp(before, render()) == 0);
	}
}

static void test_settings_read_back(void)
{
	RandoOptions options, expected;
	RandoHeartBeep heart_beep;
	bool spoiler;

	init_defaults();
	ui_generate_settings(&gen, &options, &heart_beep, &spoiler);
	rando_options_init(&expected);
	CHECK(memcmp(&options, &expected, sizeof options) == 0);
	CHECK(heart_beep == RANDO_HEART_BEEP_HALF && !spoiler);

	keys(UI_BUTTON_LEFT, 1);
	keys(UI_BUTTON_DOWN, RANDO_SETTING_CRYSTALS_TOWER);
	keys(UI_BUTTON_RIGHT, 1);
	keys(UI_BUTTON_DOWN, RANDO_SETTING_HINTS - RANDO_SETTING_CRYSTALS_TOWER);
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_LEFT, 2);
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	ui_generate_settings(&gen, &options, &heart_beep, &spoiler);
	CHECK(options.glitches == RANDO_GLITCHES_NO_LOGIC);
	CHECK(options.crystals_tower == RANDO_CRYSTALS_RANDOM);
	CHECK(!options.hints);
	CHECK(heart_beep == RANDO_HEART_BEEP_NORMAL);
	CHECK(spoiler);
	options.glitches = RANDO_GLITCHES_NONE;
	options.crystals_tower = RANDO_CRYSTALS_7;
	options.hints = true;
	CHECK(memcmp(&options, &expected, sizeof options) == 0);
}

static void test_init_from_settings(void)
{
	UiGenerateInit init;
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool spoiler;

	ui_generate_init_defaults(&init, ui_str_english(), 11);
	init.options.goal = RANDO_OPTION_GOAL_TRIFORCE_HUNT;
	init.heart_beep = RANDO_HEART_BEEP_OFF;
	ui_generate_init(&gen, &init);
	ui_generate_settings(&gen, &options, &heart_beep, &spoiler);
	CHECK(options.goal == RANDO_OPTION_GOAL_TRIFORCE_HUNT && heart_beep == RANDO_HEART_BEEP_OFF && !spoiler);
}

static void test_snapshots(void)
{
	init_defaults();
	test_snapshot(TEST, SNAPSHOTS "generate_initial.txt", render());

	keys(UI_BUTTON_UP, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_up_from_top.txt", render());

	init_defaults();
	keys(UI_BUTTON_RIGHT, 1);
	keys(UI_BUTTON_DOWN, RANDO_SETTING_GOAL);
	keys(UI_BUTTON_LEFT, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_values_changed.txt", render());

	keys(UI_BUTTON_Y, 1);
	keys(UI_BUTTON_UP, RANDO_SETTING_GOAL);
	keys(UI_BUTTON_Y, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_initial.txt", render());

	keys(UI_BUTTON_R2, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_page_down.txt", render());
	keys(UI_BUTTON_R2, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_up_from_top.txt", render());
	keys(UI_BUTTON_L2, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_page_up.txt", render());
	keys(UI_BUTTON_L2, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_initial.txt", render());

	keys(UI_BUTTON_UP, 2);
	keys(UI_BUTTON_RIGHT, 1);
	test_snapshot(TEST, SNAPSHOTS "generate_spoiler_on.txt", render());
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	test_map();
	test_events();
	test_settings_read_back();
	test_init_from_settings();
	test_every_row_and_value();
	test_snapshots();
	printf("%s: passed\n", TEST);
	return 0;
}
