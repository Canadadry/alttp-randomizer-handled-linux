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
/* RANDO_RECORD_TEXT_MAX, which the SD layer asserts. */
#define UI_HISTORY_SPOILER_MAX (64 * 1024)
#define UI_HISTORY_LINES_MAX 8192
#define UI_HISTORY_WIDTH_MIN 20
#define UI_HISTORY_WIDTH_MAX (UI_GRID_COLS_MAX - 2)
#define UI_HISTORY_TEXT_MAX 96

/* The record's local date. */
typedef struct UiHistoryDate {
	int year, month, day, hour, minute, second;
} UiHistoryDate;

typedef struct UiHistoryEntry {
	RandoName name;
	UiHistoryDate date;
	uint32_t seed;
	/* hints included. */
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool has_spoiler;
} UiHistoryEntry;

/* Newest first. */
typedef struct UiHistoryList {
	UiHistoryEntry entries[UI_HISTORY_MAX];
	int count;
	/* At least one record skipped; more than UI_HISTORY_MAX listable seeds; the folder exists but cannot be
	 * listed. */
	bool unreadable, truncated, folder_unreadable;
} UiHistoryList;

/* text[0, len): lines separated by '\n', each starting with its indentation in spaces; cut when the outline
 * did not fit. */
typedef struct UiHistorySpoiler {
	char text[UI_HISTORY_SPOILER_MAX];
	size_t len;
	bool cut;
} UiHistorySpoiler;

/* Called only for an entry with has_spoiler; false when the record or its spoiler cannot be read. */
typedef bool (*UiHistorySpoilerFn)(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out);

typedef struct UiHistoryDeleteOutcome {
	/* The record is gone: the entry leaves the list. */
	bool ok;
	/* A SNES list cache is still present. */
	bool list_stale;
	/* The result line when not ok. */
	UiStrId status;
} UiHistoryDeleteOutcome;

/* Synchronous; must always fill outcome. */
typedef void (*UiHistoryDeleteFn)(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome);

/* What the caller does next: the tab never quits or switches tab itself. */
typedef enum UiHistoryEvent {
	UI_HISTORY_NONE,
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
	/* Borrowed for the tab's life; NULL means English. */
	const UiStrTable *strings;
	/* ui_grid_page: 1..UI_ROWS_MAX. */
	int page;
	/* The text field, cols - 2: UI_HISTORY_WIDTH_MIN..UI_HISTORY_WIDTH_MAX. */
	int width;
	/* Copied. */
	const UiHistoryList *list;
	UiHistorySpoilerFn spoiler_fn;
	UiHistoryDeleteFn delete_fn;
	void *ctx;
} UiHistoryInit;

/* One wrapped row of the spoiler: indent spaces, then text[offset, +len). */
typedef struct UiHistoryLine {
	uint32_t offset;
	uint16_t len, indent;
} UiHistoryLine;

/* Caller-owned, no allocation, about 200 KiB: a static object. Read only through the functions below. */
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
	/* The one-shot message, cleared by the next key, and its filled text. */
	UiMessageKind message_kind;
	char message[UI_HISTORY_TEXT_MAX];
	UiHistorySpoilerFn spoiler_fn;
	UiHistoryDeleteFn delete_fn;
	void *ctx;
	/* Filled texts the screen borrows. */
	char date_line[UI_HISTORY_TEXT_MAX], seed_line[UI_HISTORY_TEXT_MAX], title[UI_HISTORY_TEXT_MAX];
	char rows[UI_ROWS_MAX][UI_HISTORY_WIDTH_MAX + 1];
} UiHistory;

/* View LIST, cursor and scroll 0; the first set notice (folder_unreadable, unreadable, truncated) shows until
 * the first key. Asserts the init's ranges. Called again after each successful generation. */
void ui_history_init(UiHistory *state, const UiHistoryInit *init);

/* Acts on one filtered button (ui_input_event's output). */
UiHistoryEvent ui_history_key(UiHistory *state, UiButton button);

/* Fills out; its texts are borrowed from state and its string table, valid until the next call on state. */
void ui_history_screen(UiHistory *state, UiScreen *out);

#endif
