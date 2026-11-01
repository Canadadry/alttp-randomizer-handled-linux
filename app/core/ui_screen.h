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

typedef struct UiRow {
	UiRowKind kind;
	const char *label;
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
	int cursor;
	int scroll;
	bool more_above, more_below;
	UiMessageKind message_kind;
	const char *message;
	const char *footer;
} UiScreen;

void ui_screen_init(UiScreen *screen);

bool ui_screen_add_row(UiScreen *screen, UiRow row);

bool ui_screen_valid(const UiScreen *screen);

int ui_scroll_follow(int scroll, int cursor, int rows, int page);

#endif
