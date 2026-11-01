#ifndef UI_SCREEN_H
#define UI_SCREEN_H

#include <stdbool.h>

#define UI_TABS_MAX 4
#define UI_ROWS_MAX 64

typedef enum UiRowKind {
	UI_ROW_SETTING,
	UI_ROW_ACTION,
	UI_ROW_TEXT
} UiRowKind;

/* Every text is borrowed: the caller keeps it alive until the render returns. NULL draws as empty. */
typedef struct UiRow {
	UiRowKind kind;
	const char *label;
	/* Settings only. */
	const char *value;
	bool disabled;
} UiRow;

typedef enum UiMessageKind {
	UI_MESSAGE_NONE,
	UI_MESSAGE_INFO,
	UI_MESSAGE_ERROR
} UiMessageKind;

typedef struct UiScreen {
	const char *tabs[UI_TABS_MAX];
	int tab_count;
	int active_tab;
	const char *title;
	UiRow rows[UI_ROWS_MAX];
	int row_count;
	/* A row index, or -1 for none. */
	int cursor;
	/* Index of the first row drawn. */
	int scroll;
	/* Set by a state machine handing over a slice of a longer list. */
	bool more_above, more_below;
	UiMessageKind message_kind;
	const char *message;
	const char *footer;
} UiScreen;

/* No tabs, no title, no rows, cursor -1, scroll 0, no message, no footer. */
void ui_screen_init(UiScreen *screen);

/* Appends a copy of row (its texts still borrowed); false, adding nothing, when the screen holds UI_ROWS_MAX. */
bool ui_screen_add_row(UiScreen *screen, UiRow row);

/* True when tab_count is 0..UI_TABS_MAX, row_count 0..UI_ROWS_MAX, active_tab a tab index (not read
 * with no tabs), cursor -1 or a row index, and scroll in [0, max(0, row_count - 1)]. */
bool ui_screen_valid(const UiScreen *screen);

/* The scroll that keeps cursor (-1 for none: clamp only) on a page of page rows with the least
 * movement, clamped to [0, max(0, rows - page)]. Asserts page >= 1, rows >= 0, cursor in -1..rows-1. */
int ui_scroll_follow(int scroll, int cursor, int rows, int page);

#endif
