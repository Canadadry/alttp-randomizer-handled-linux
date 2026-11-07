#ifndef UI_HISTORY_H
#define UI_HISTORY_H

#include "rando_name.h"
#include "rando_options.h"
#include "ui_grid.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_HISTORY_MAX 256
#define UI_HISTORY_SPOILER_MAX (64 * 1024)
#define UI_HISTORY_LINES_MAX 8192
#define UI_HISTORY_WIDTH_MIN 20
#define UI_HISTORY_WIDTH_MAX (UI_GRID_COLS_MAX - 2)
#define UI_HISTORY_TEXT_MAX 96

typedef struct UiHistoryDate {
	int year, month, day, hour, minute, second;
} UiHistoryDate;

typedef struct UiHistoryEntry {
	RandoName name;
	UiHistoryDate date;
	uint32_t seed;
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool has_spoiler;
} UiHistoryEntry;

typedef struct UiHistoryList {
	UiHistoryEntry entries[UI_HISTORY_MAX];
	int count;
	bool unreadable, truncated, folder_unreadable;
} UiHistoryList;

typedef struct UiHistorySpoiler {
	char text[UI_HISTORY_SPOILER_MAX];
	size_t len;
	bool cut;
} UiHistorySpoiler;

typedef bool (*UiHistorySpoilerFn)(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out);

typedef struct UiHistoryDeleteOutcome {
	bool ok;
	bool list_stale;
	UiStrId status;
} UiHistoryDeleteOutcome;

typedef void (*UiHistoryDeleteFn)(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome);

typedef enum UiHistoryEvent {
	UI_HISTORY_NONE,
	UI_HISTORY_BACK,
	UI_HISTORY_QUIT,
	UI_HISTORY_TAB_NEXT,
	UI_HISTORY_TAB_PREV
} UiHistoryEvent;

typedef enum UiHistoryView {
	UI_HISTORY_VIEW_LIST,
	UI_HISTORY_VIEW_DETAIL,
	UI_HISTORY_VIEW_CONFIRM
} UiHistoryView;

typedef enum UiHistorySpoilerState {
	UI_HISTORY_SPOILER_NONE,
	UI_HISTORY_SPOILER_OK,
	UI_HISTORY_SPOILER_UNREADABLE,
	UI_HISTORY_SPOILER_CUT
} UiHistorySpoilerState;

typedef struct UiHistoryInit {
	const UiStrTable *strings;
	int page;
	int width;
	const UiHistoryList *list;
	UiHistorySpoilerFn spoiler_fn;
	UiHistoryDeleteFn delete_fn;
	void *ctx;
} UiHistoryInit;

typedef struct UiHistoryLine {
	uint32_t offset;
	uint16_t len, indent;
} UiHistoryLine;

typedef struct UiHistory {
	const UiStrTable *strings;
	int page, width;
	UiHistoryList list;
	UiHistoryView view, confirm_from;
	int cursor, scroll, detail_scroll;
	UiHistorySpoiler spoiler;
	UiHistoryLine lines[UI_HISTORY_LINES_MAX];
	int line_count;
	UiHistorySpoilerState spoiler_state;
	UiMessageKind message_kind;
	char message[UI_HISTORY_TEXT_MAX];
	UiHistorySpoilerFn spoiler_fn;
	UiHistoryDeleteFn delete_fn;
	void *ctx;
	char date_line[UI_HISTORY_TEXT_MAX], seed_line[UI_HISTORY_TEXT_MAX], title[UI_HISTORY_TEXT_MAX];
	char rows[UI_ROWS_MAX][UI_HISTORY_WIDTH_MAX + 1];
} UiHistory;

void ui_history_init(UiHistory *state, const UiHistoryInit *init);

UiHistoryEvent ui_history_key(UiHistory *state, UiButton button);

void ui_history_screen(UiHistory *state, UiScreen *out);

#endif
