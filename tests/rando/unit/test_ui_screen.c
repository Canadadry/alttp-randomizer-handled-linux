#include "support.h"
#include "ui_screen.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_ui_screen"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void test_init_gives_the_empty_screen(void)
{
	UiScreen s;
	int i;

	memset(&s, 0x5A, sizeof s);
	ui_screen_init(&s);
	CHECK(s.tab_count == 0);
	CHECK(s.active_tab == 0);
	for (i = 0; i < UI_TABS_MAX; i++)
		CHECK(s.tabs[i] == NULL);
	CHECK(s.title == NULL);
	CHECK(s.row_count == 0);
	CHECK(s.cursor == -1);
	CHECK(s.scroll == 0);
	CHECK(!s.more_above && !s.more_below);
	CHECK(s.message_kind == UI_MESSAGE_NONE);
	CHECK(s.message == NULL);
	CHECK(s.hint_count == 0);
}

static void test_add_hint_appends_until_full(void)
{
	UiScreen s;
	UiHint hint = {UI_HINT_START, "START", "Generate", true};
	UiHint extra = {UI_HINT_A, "A", "Back", false};
	int i;

	ui_screen_init(&s);
	CHECK(ui_screen_add_hint(&s, hint));
	CHECK(s.hint_count == 1);
	CHECK(s.hints[0].button == UI_HINT_START && s.hints[0].primary);
	CHECK(s.hints[0].key == hint.key && s.hints[0].label == hint.label);
	for (i = 1; i < UI_HINTS_MAX; i++)
		CHECK(ui_screen_add_hint(&s, extra));
	CHECK(s.hint_count == UI_HINTS_MAX);
	CHECK(!ui_screen_add_hint(&s, extra));
	CHECK(s.hint_count == UI_HINTS_MAX);
}

static void test_valid_checks_the_hints(void)
{
	UiScreen s;
	UiHint plain = {UI_HINT_A, "A", "Back", false};
	UiHint primary = {UI_HINT_START, "START", "Generate", true};

	ui_screen_init(&s);
	CHECK(ui_screen_add_hint(&s, plain));
	CHECK(ui_screen_add_hint(&s, plain));
	CHECK(ui_screen_valid(&s));
	CHECK(ui_screen_add_hint(&s, primary));
	CHECK(ui_screen_valid(&s));
	s.hints[0].primary = true;
	CHECK(!ui_screen_valid(&s));
	s.hints[0].primary = false;

	s.hints[1].button = UI_HINT_BUTTON_COUNT;
	CHECK(!ui_screen_valid(&s));
	s.hints[1].button = (UiHintButton)-1;
	CHECK(!ui_screen_valid(&s));
	s.hints[1].button = UI_HINT_LEFT_RIGHT;
	CHECK(ui_screen_valid(&s));

	s.hint_count = UI_HINTS_MAX + 1;
	CHECK(!ui_screen_valid(&s));
	s.hint_count = -1;
	CHECK(!ui_screen_valid(&s));
}

static void test_add_row_appends_until_full(void)
{
	UiScreen s;
	UiRow row = {UI_ROW_SETTING, "Mode", "Open", true};
	UiRow extra = {UI_ROW_ACTION, "Generate", NULL, false};
	int i;

	ui_screen_init(&s);
	CHECK(ui_screen_add_row(&s, row));
	CHECK(s.row_count == 1);
	CHECK(s.rows[0].kind == UI_ROW_SETTING);
	CHECK(s.rows[0].label == row.label && s.rows[0].value == row.value && s.rows[0].disabled);
	for (i = 1; i < UI_ROWS_MAX; i++)
		CHECK(ui_screen_add_row(&s, row));
	CHECK(s.row_count == UI_ROWS_MAX);
	CHECK(!ui_screen_add_row(&s, extra));
	CHECK(s.row_count == UI_ROWS_MAX);
	CHECK(s.rows[UI_ROWS_MAX - 1].kind == UI_ROW_SETTING);
	CHECK(s.rows[UI_ROWS_MAX - 1].label == row.label);
}

/* Three rows, two tabs, cursor and scroll at their boundaries. */
static void valid_screen(UiScreen *s)
{
	UiRow row = {UI_ROW_TEXT, "line", NULL, false};
	int i;

	ui_screen_init(s);
	s->tabs[0] = "Generate";
	s->tabs[1] = "History";
	s->tab_count = 2;
	s->active_tab = 1;
	for (i = 0; i < 3; i++)
		ui_screen_add_row(s, row);
	s->cursor = 2;
	s->scroll = 2;
}

static void test_valid_accepts_the_boundaries(void)
{
	UiScreen s;

	ui_screen_init(&s);
	CHECK(ui_screen_valid(&s));
	s.active_tab = 3;
	CHECK(ui_screen_valid(&s));

	valid_screen(&s);
	CHECK(ui_screen_valid(&s));
	s.active_tab = 0;
	s.cursor = -1;
	s.scroll = 0;
	CHECK(ui_screen_valid(&s));
	s.cursor = 0;
	CHECK(ui_screen_valid(&s));

	valid_screen(&s);
	s.tab_count = UI_TABS_MAX;
	s.tabs[2] = s.tabs[3] = "Tab";
	s.active_tab = UI_TABS_MAX - 1;
	s.row_count = UI_ROWS_MAX;
	s.cursor = UI_ROWS_MAX - 1;
	s.scroll = UI_ROWS_MAX - 1;
	CHECK(ui_screen_valid(&s));
}

static void test_valid_refuses_each_field_out_of_range(void)
{
	UiScreen s;

	valid_screen(&s);
	s.cursor = -2;
	CHECK(!ui_screen_valid(&s));
	valid_screen(&s);
	s.cursor = 3;
	CHECK(!ui_screen_valid(&s));

	valid_screen(&s);
	s.scroll = 3;
	CHECK(!ui_screen_valid(&s));
	valid_screen(&s);
	s.scroll = -1;
	CHECK(!ui_screen_valid(&s));
	ui_screen_init(&s);
	s.scroll = 1;
	CHECK(!ui_screen_valid(&s));

	valid_screen(&s);
	s.active_tab = 2;
	CHECK(!ui_screen_valid(&s));
	valid_screen(&s);
	s.active_tab = -1;
	CHECK(!ui_screen_valid(&s));

	valid_screen(&s);
	s.tab_count = UI_TABS_MAX + 1;
	CHECK(!ui_screen_valid(&s));
	valid_screen(&s);
	s.tab_count = -1;
	CHECK(!ui_screen_valid(&s));
	valid_screen(&s);
	s.row_count = UI_ROWS_MAX + 1;
	CHECK(!ui_screen_valid(&s));
	valid_screen(&s);
	s.row_count = -1;
	s.cursor = -1;
	s.scroll = 0;
	CHECK(!ui_screen_valid(&s));

	ui_screen_init(&s);
	s.cursor = 0;
	CHECK(!ui_screen_valid(&s));
}

static void check_follow(int scroll, int cursor, int rows, int page, int expected)
{
	int got = ui_scroll_follow(scroll, cursor, rows, page);

	if (got != expected)
		test_fail(TEST, "scroll follow (scroll %d, cursor %d, rows %d, page %d): %d, expected %d", scroll,
			  cursor, rows, page, got, expected);
}

static void test_scroll_follow(void)
{
	/* 20 rows, page of 5, the page showing rows 6..10. */
	check_follow(6, 3, 20, 5, 3);
	check_follow(6, 6, 20, 5, 6);
	check_follow(6, 8, 20, 5, 6);
	check_follow(6, 10, 20, 5, 6);
	check_follow(6, 11, 20, 5, 7);
	check_follow(6, 17, 20, 5, 13);
	check_follow(6, 0, 20, 5, 0);
	check_follow(6, 19, 20, 5, 15);
	check_follow(0, 0, 20, 5, 0);
	check_follow(15, 19, 20, 5, 15);
	/* A stale scroll past the last page is pulled back, even with the cursor on the page. */
	check_follow(18, 19, 20, 5, 15);
	check_follow(-2, 1, 20, 5, 0);

	/* Fewer rows than the page: always 0. */
	check_follow(0, 2, 3, 5, 0);
	check_follow(2, 2, 3, 5, 0);
	check_follow(0, -1, 0, 5, 0);
	check_follow(4, -1, 0, 5, 0);

	/* No cursor: only clamped. */
	check_follow(7, -1, 20, 5, 7);
	check_follow(17, -1, 20, 5, 15);
	check_follow(-1, -1, 20, 5, 0);
	check_follow(3, -1, 20, 1, 3);
	check_follow(3, 12, 20, 1, 12);
}

int main(void)
{
	test_init_gives_the_empty_screen();
	test_add_row_appends_until_full();
	test_add_hint_appends_until_full();
	test_valid_checks_the_hints();
	test_valid_accepts_the_boundaries();
	test_valid_refuses_each_field_out_of_range();
	test_scroll_follow();
	printf("%s: ok\n", TEST);
	return 0;
}
