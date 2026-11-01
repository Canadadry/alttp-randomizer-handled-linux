#ifndef UI_GENERATE_H
#define UI_GENERATE_H

#include "rando_options.h"
#include "rando_settings.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>

/* The table's settings, then the spoiler row, then the Generate action. */
#define UI_GENERATE_ROW_SPOILER RANDO_SETTING_COUNT
#define UI_GENERATE_ROW_ACTION (RANDO_SETTING_COUNT + 1)
#define UI_GENERATE_ROW_COUNT (RANDO_SETTING_COUNT + 2)
#define UI_GENERATE_REASON_MAX 96

/* What the caller does next: the tab never generates, quits or switches tab itself. */
typedef enum UiGenerateEvent {
	UI_GENERATE_NONE,
	UI_GENERATE_GENERATE,
	UI_GENERATE_QUIT,
	UI_GENERATE_TAB_NEXT,
	UI_GENERATE_TAB_PREV
} UiGenerateEvent;

typedef struct UiGenerateInit {
	/* Borrowed for the tab's life; NULL means English. */
	const UiStrTable *strings;
	/* The grid's list lines, at least 1. */
	int page;
	RandoOptions options;
	RandoHeartBeep heart_beep;
	/* Shows the unreadable-settings notice until the first key. */
	bool settings_unreadable;
	/* Borrowed array of unsupported_count pairs (NULL when 0); defaults to the library's list. */
	const RandoSettingPair *unsupported;
	int unsupported_count;
} UiGenerateInit;

/* Caller-owned, no allocation. Read only through the functions below. */
typedef struct UiGenerate {
	const UiStrTable *strings;
	int page;
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool spoiler;
	int cursor, scroll;
	const RandoSettingPair *unsupported;
	int unsupported_count;
	/* The one-shot message, cleared by the next key; UI_STR_GENERATE_UNSUPPORTED shows refusal. */
	UiMessageKind message_kind;
	UiStrId message;
	/* Filled reasons the screen borrows: the refusal's and the cursor row's ("" when not greyed). */
	char refusal[UI_GENERATE_REASON_MAX], reason[UI_GENERATE_REASON_MAX];
} UiGenerate;

/* strings and page as given, the table's default settings, no notice, the library's unsupported list. */
void ui_generate_init_defaults(UiGenerateInit *init, const UiStrTable *strings, int page);

/* Cursor on the first row, scroll 0, spoiler off. Asserts page >= 1 and a sound pair list. */
void ui_generate_init(UiGenerate *state, const UiGenerateInit *init);

/* Acts on one filtered button (ui_input_event's output). While an unsupported pair holds, its two rows and
 * the action are greyed but still change, and Generate gives UI_GENERATE_NONE with the refusal shown. */
UiGenerateEvent ui_generate_key(UiGenerate *state, UiButton button);

/* Fills out; its texts are borrowed from the string table, valid while it lives. */
void ui_generate_screen(const UiGenerate *state, UiScreen *out);

void ui_generate_settings(const UiGenerate *state, RandoOptions *options, RandoHeartBeep *heart_beep, bool *spoiler);

#endif
