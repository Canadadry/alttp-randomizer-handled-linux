#include "support.h"
#include "ui_grid.h"
#include "ui_screen.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_ui_grid"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void test_grid_bounds_and_page(void)
{
	UiGrid g;

	CHECK(ui_grid_init(&g, UI_GRID_COLS_MIN, UI_GRID_LINES_MIN));
	CHECK(g.cols == 20 && g.lines == 5);
	CHECK(ui_grid_page(&g) == 1);
	CHECK(ui_grid_init(&g, UI_GRID_COLS_MAX, UI_GRID_LINES_MAX));
	CHECK(g.cols == 96 && g.lines == 40);
	CHECK(ui_grid_page(&g) == 36);
	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(g.cols == 40 && g.lines == 15);
	CHECK(ui_grid_page(&g) == 11);
	CHECK(!ui_grid_init(&g, 19, 15));
	CHECK(!ui_grid_init(&g, 97, 15));
	CHECK(!ui_grid_init(&g, 40, 4));
	CHECK(!ui_grid_init(&g, 40, 41));
	CHECK(g.cols == 40 && g.lines == 15);
}

static void test_text_form_of_a_blank_grid(void)
{
	static const char expected[] =
		" +--------------------+\n"
		" |                    |\n"
		" |                    |\n"
		" |                    |\n"
		" |                    |\n"
		" |                    |\n"
		" +--------------------+\n";
	char buf[UI_GRID_TEXT_MAX];
	UiGrid g;
	size_t n = sizeof expected - 1;

	CHECK(ui_grid_init(&g, 20, 5));
	CHECK(ui_grid_text(&g, buf, sizeof buf) == n);
	CHECK(strcmp(buf, expected) == 0);
	memset(buf, '@', sizeof buf);
	CHECK(ui_grid_text(&g, buf, n + 1) == n);
	CHECK(strcmp(buf, expected) == 0);
	CHECK(ui_grid_text(&g, buf, n) == 0);
	CHECK(ui_grid_text(&g, buf, 0) == 0);

	CHECK(ui_grid_init(&g, UI_GRID_COLS_MAX, UI_GRID_LINES_MAX));
	CHECK(ui_grid_text(&g, buf, sizeof buf) == UI_GRID_TEXT_MAX - 1);
}

#define SNAPSHOTS "unit/snapshots/"

static void check_snapshot(const UiScreen *s, int cols, int lines, const char *name)
{
	char buf[UI_GRID_TEXT_MAX];
	UiGrid g;

	CHECK(ui_grid_init(&g, cols, lines));
	CHECK(ui_grid_render(&g, s));
	CHECK(ui_grid_text(&g, buf, sizeof buf) > 0);
	test_snapshot(TEST, name, buf);
}

static void test_snapshot_empty(void)
{
	UiScreen s;

	ui_screen_init(&s);
	check_snapshot(&s, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES, SNAPSHOTS "ui_empty.txt");
}

static void add(UiScreen *s, UiRowKind kind, const char *label, const char *value, bool disabled)
{
	UiRow row;

	row.kind = kind;
	row.label = label;
	row.value = value;
	row.disabled = disabled;
	CHECK(ui_screen_add_row(s, row));
}

static void hint(UiScreen *s, UiHintButton button, const char *key, const char *label, bool primary)
{
	UiHint h;

	h.button = button;
	h.key = key;
	h.label = label;
	h.primary = primary;
	CHECK(ui_screen_add_hint(s, h));
}

/* 18 rows, the cursor moved from the top to row 13 (a setting), scroll following it on a page of page. */
static void generate_screen(UiScreen *s, int page)
{
	ui_screen_init(s);
	s->tabs[0] = "Generate";
	s->tabs[1] = "History";
	s->tab_count = 2;
	s->active_tab = 0;
	s->title = "Generate a seed";
	add(s, UI_ROW_SETTING, "Mode", "Open", false);
	add(s, UI_ROW_SETTING, "Goal", "Defeat Ganon", false);
	add(s, UI_ROW_SETTING, "Logic", "No Glitches", false);
	add(s, UI_ROW_SETTING, "Swords", "Randomized", false);
	add(s, UI_ROW_SETTING, "Item placement", "Advanced", false);
	add(s, UI_ROW_SETTING, "Dungeon items", "Standard", false);
	add(s, UI_ROW_SETTING, "Accessibility", "Items", false);
	add(s, UI_ROW_SETTING, "Enemy shuffle", "None", true);
	add(s, UI_ROW_SETTING, "Boss shuffle", "None", false);
	add(s, UI_ROW_ACTION, "Generate", NULL, false);
	add(s, UI_ROW_SETTING, "Item functionality", "Normal", false);
	add(s, UI_ROW_SETTING, "ROM", "Zelda no Densetsu (J).sfc", false);
	add(s, UI_ROW_SETTING, "Heart beep", "Normal", false);
	add(s, UI_ROW_SETTING, "Spoiler log", "Off", false);
	add(s, UI_ROW_SETTING, "Menu speed", "Normal", false);
	add(s, UI_ROW_SETTING, "Quickswap", "Off", false);
	add(s, UI_ROW_SETTING, "Palette", "Default", false);
	add(s, UI_ROW_SETTING, "Music", "On", false);
	s->cursor = 13;
	s->scroll = ui_scroll_follow(0, s->cursor, s->row_count, page);
	s->message_kind = UI_MESSAGE_INFO;
	s->message = "Left and right change the value";
	hint(s, UI_HINT_A, "A", "Select", false);
	hint(s, UI_HINT_B, "B", "Back", false);
	hint(s, UI_HINT_LEFT_RIGHT, "L/R", "Tab", false);
}

static void test_snapshot_generate_default(void)
{
	UiScreen s;

	generate_screen(&s, UI_GRID_DEFAULT_LINES - 4);
	check_snapshot(&s, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES, SNAPSHOTS "ui_generate_40x15.txt");
}

static void test_snapshot_generate_small(void)
{
	UiScreen s;

	generate_screen(&s, 8 - 4);
	check_snapshot(&s, 24, 8, SNAPSHOTS "ui_generate_24x8.txt");
}

static void test_snapshot_generate_large(void)
{
	UiScreen s;

	generate_screen(&s, 24 - 4);
	check_snapshot(&s, 60, 24, SNAPSHOTS "ui_generate_60x24.txt");
}

/* A slice of a longer spoiler: the state machine says more follows. */
static void test_snapshot_detail(void)
{
	static const char *const lines[] = {
		"Seed 1CA10A", "Mode: Open", "Goal: Defeat Ganon", "Logic: No Glitches", NULL, "Light World",
		"Link's House: Bow", "Sahasrahla: Moon Pearl", "Pedestal: Master Sword",
		"Kakariko Well - Top: Bombos", "Blind's Hideout - Far Left: Progressive Glove",
	};
	UiScreen s;
	size_t i;

	ui_screen_init(&s);
	s.tabs[0] = "Generate";
	s.tabs[1] = "History";
	s.tab_count = 2;
	s.active_tab = 1;
	s.title = "Open-Ganon-1CA10A.sfc";
	for (i = 0; i < sizeof lines / sizeof lines[0]; i++)
		add(&s, UI_ROW_TEXT, lines[i], NULL, false);
	s.more_below = true;
	s.message_kind = UI_MESSAGE_ERROR;
	s.message = "Cannot read the record";
	hint(&s, UI_HINT_B, "B", "Back", false);
	check_snapshot(&s, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES, SNAPSHOTS "ui_detail_40x15.txt");
}

static void test_snapshot_cursor_on_disabled(void)
{
	UiScreen s;

	ui_screen_init(&s);
	s.tabs[0] = "Generate";
	s.tabs[1] = "History";
	s.tab_count = 2;
	add(&s, UI_ROW_SETTING, "Mode", "Open", false);
	add(&s, UI_ROW_SETTING, "Enemy shuffle", "None", true);
	add(&s, UI_ROW_ACTION, "Generate", NULL, false);
	s.cursor = 1;
	hint(&s, UI_HINT_A, "A", "Select", false);
	check_snapshot(&s, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES, SNAPSHOTS "ui_disabled_cursor_40x15.txt");
}

static void check_line(const UiGrid *g, int line, UiLineStyle style, const char *expected)
{
	if ((int)strlen(expected) != g->cols || memcmp(g->cells[line], expected, (size_t)g->cols) != 0)
		test_fail(TEST, "line %d: \"%.*s\", expected \"%s\"", line, g->cols, g->cells[line], expected);
	if (g->styles[line] != style)
		test_fail(TEST, "line %d: style %d, expected %d", line, (int)g->styles[line], (int)style);
}

static void test_invalid_screen_leaves_a_blank_grid(void)
{
	char blank[UI_GRID_TEXT_MAX], buf[UI_GRID_TEXT_MAX];
	UiScreen s;
	UiGrid g;

	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_text(&g, blank, sizeof blank) > 0);
	generate_screen(&s, ui_grid_page(&g));
	CHECK(ui_grid_render(&g, &s));
	s.cursor = s.row_count;
	CHECK(!ui_grid_render(&g, &s));
	CHECK(g.cols == UI_GRID_DEFAULT_COLS && g.lines == UI_GRID_DEFAULT_LINES);
	CHECK(ui_grid_text(&g, buf, sizeof buf) > 0);
	CHECK(strcmp(buf, blank) == 0);
}

static void test_null_text_draws_as_empty(void)
{
	UiScreen s;
	UiGrid g;

	ui_screen_init(&s);
	s.tabs[0] = NULL;
	s.tabs[1] = "B";
	s.tab_count = 2;
	s.title = "";
	add(&s, UI_ROW_SETTING, NULL, NULL, false);
	add(&s, UI_ROW_ACTION, NULL, NULL, false);
	s.cursor = 0;
	s.message_kind = UI_MESSAGE_INFO;
	CHECK(ui_grid_init(&g, 20, 6));
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 0, UI_LINE_TABS, "[]  B               ");
	check_line(&g, 1, UI_LINE_BLANK, "                    ");
	check_line(&g, 2, UI_LINE_SELECTED, "               <  > ");
	check_line(&g, 3, UI_LINE_ROW, "                    ");
	check_line(&g, 4, UI_LINE_INFO, "                    ");
	check_line(&g, 5, UI_LINE_BLANK, "                    ");
	hint(&s, UI_HINT_A, NULL, NULL, false);
	hint(&s, UI_HINT_B, NULL, "Back", false);
	hint(&s, UI_HINT_X, "X", NULL, false);
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 5, UI_LINE_FOOTER, "     Back  X        ");
}

static void test_hints_join_into_the_footer(void)
{
	UiScreen s;
	UiGrid g;

	ui_screen_init(&s);
	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	hint(&s, UI_HINT_LEFT_RIGHT, "</>", "Change", false);
	hint(&s, UI_HINT_Y, "Y", "Default", false);
	hint(&s, UI_HINT_START, "START", "Generate", true);
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 14, UI_LINE_FOOTER, " </> Change  Y Default  START Generate  ");
	s.hints[2].primary = false;
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 14, UI_LINE_FOOTER, " </> Change  Y Default  START Generate  ");
	hint(&s, UI_HINT_A, "A", "Back", false);
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 14, UI_LINE_FOOTER, " </> Change  Y Default  START Generate~ ");

	ui_screen_init(&s);
	hint(&s, UI_HINT_A, "A", "Delete", false);
	hint(&s, UI_HINT_B, "B", "Keep", false);
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 14, UI_LINE_FOOTER, " A Delete  B Keep                       ");

	ui_screen_init(&s);
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 14, UI_LINE_BLANK, "                                        ");
}

static void test_bytes_outside_printable_ascii_draw_as_question_marks(void)
{
	UiScreen s;
	UiGrid g;

	ui_screen_init(&s);
	s.title = "a\tb\x7f\x80\xc3\xa9~ z";
	CHECK(ui_grid_init(&g, 20, 5));
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 1, UI_LINE_TITLE, " a?b????~ z         ");
}

static void test_tab_bar_is_cut(void)
{
	UiScreen s;
	UiGrid g;

	ui_screen_init(&s);
	s.tabs[0] = "Generate";
	s.tabs[1] = "History";
	s.tabs[2] = "Settings";
	s.tab_count = 3;
	s.active_tab = 2;
	CHECK(ui_grid_init(&g, 24, 5));
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 0, UI_LINE_TABS, " Generate   History  [S~");
}

/* One list line: the mark below wins over the mark above. */
static void test_smallest_grid(void)
{
	UiScreen s;
	UiGrid g;

	ui_screen_init(&s);
	add(&s, UI_ROW_TEXT, "first", NULL, false);
	add(&s, UI_ROW_TEXT, "second", NULL, true);
	add(&s, UI_ROW_TEXT, "third", NULL, false);
	s.scroll = 1;
	s.cursor = 1;
	CHECK(ui_grid_init(&g, UI_GRID_COLS_MIN, UI_GRID_LINES_MIN));
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 2, UI_LINE_SELECTED_DISABLED, " second            v");
	s.scroll = 2;
	s.cursor = 2;
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 2, UI_LINE_SELECTED, " third             ^");
	s.cursor = -1;
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 2, UI_LINE_TEXT, " third             ^");
}

/* Field of 18 columns at 20: a decorated value of 16 leaves the label one column, of 17 none. */
static void test_setting_cut_boundaries(void)
{
	UiScreen s;
	UiGrid g;

	ui_screen_init(&s);
	add(&s, UI_ROW_SETTING, "Mode", "123456789012", false);
	add(&s, UI_ROW_SETTING, "Mode", "1234567890123", false);
	add(&s, UI_ROW_SETTING, "Mode", "12345678901234", false);
	add(&s, UI_ROW_SETTING, "Mode", "123456789012345", false);
	CHECK(ui_grid_init(&g, 20, 8));
	CHECK(ui_grid_render(&g, &s));
	check_line(&g, 2, UI_LINE_ROW, " ~   123456789012   ");
	check_line(&g, 3, UI_LINE_ROW, "    1234567890123   ");
	check_line(&g, 4, UI_LINE_ROW, "   12345678901234   ");
	check_line(&g, 5, UI_LINE_ROW, "   1234567890123~   ");
}

int main(void)
{
	test_grid_bounds_and_page();
	test_text_form_of_a_blank_grid();
	test_snapshot_generate_default();
	test_snapshot_generate_small();
	test_snapshot_generate_large();
	test_snapshot_detail();
	test_snapshot_cursor_on_disabled();
	test_snapshot_empty();
	test_invalid_screen_leaves_a_blank_grid();
	test_null_text_draws_as_empty();
	test_hints_join_into_the_footer();
	test_bytes_outside_printable_ascii_draw_as_question_marks();
	test_tab_bar_is_cut();
	test_setting_cut_boundaries();
	test_smallest_grid();
	printf("%s: 6 snapshots matched\n", TEST);
	return 0;
}
