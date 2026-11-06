#ifndef UI_SCREEN_H
#define UI_SCREEN_H

#include <stdbool.h>

#define UI_TABS_MAX 4
#define UI_ROWS_MAX 64
#define UI_HINTS_MAX 4

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
	bool emphasis;
} UiRow;

typedef enum UiMessageKind {
	UI_MESSAGE_NONE,
	UI_MESSAGE_INFO,
	UI_MESSAGE_ERROR,
	UI_MESSAGE_HELP
} UiMessageKind;

typedef enum UiTitleTone {
	UI_TITLE_NORMAL,
	UI_TITLE_ALERT,
	UI_TITLE_TONE_COUNT
} UiTitleTone;

typedef enum UiBodyKind {
	UI_BODY_LIST,
	UI_BODY_NOTICE,
	UI_BODY_KIND_COUNT
} UiBodyKind;

typedef enum UiHintButton {
	UI_HINT_LEFT_RIGHT,
	UI_HINT_A,
	UI_HINT_B,
	UI_HINT_X,
	UI_HINT_Y,
	UI_HINT_START,
	UI_HINT_BUTTON_COUNT
} UiHintButton;

/* At most one hint of a valid screen is primary. */
typedef struct UiHint {
	UiHintButton button;
	const char *key;
	const char *label;
	bool primary;
} UiHint;

typedef struct UiScreen {
	const char *tabs[UI_TABS_MAX];
	int tab_count;
	int active_tab;
	const char *title;
	UiTitleTone title_tone;
	UiBodyKind body_kind;
	UiRow rows[UI_ROWS_MAX];
	int row_count;
	int cursor;
	int scroll;
	bool more_above, more_below;
	UiMessageKind message_kind;
	const char *message;
	UiHint hints[UI_HINTS_MAX];
	int hint_count;
} UiScreen;

void ui_screen_init(UiScreen *screen);

bool ui_screen_add_row(UiScreen *screen, UiRow row);

bool ui_screen_add_hint(UiScreen *screen, UiHint hint);

bool ui_screen_valid(const UiScreen *screen);

int ui_scroll_follow(int scroll, int cursor, int rows, int page);

#endif
