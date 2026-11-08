#ifndef UI_LADX_GENERATE_H
#define UI_LADX_GENERATE_H

#include "ladx_settings.h"
#include "ui_generate.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>

#define UI_LADX_ITEM_SPOILER LADX_SETTING_COUNT
#define UI_LADX_ITEM_ACTION (LADX_SETTING_COUNT + 1)
#define UI_LADX_ITEM_HEADING (LADX_SETTING_COUNT + 2)
#define UI_LADX_ROWS_MAX (LADX_SETTING_COUNT + LADX_CATEGORY_COUNT + 2)

typedef struct UiLadxGenerateInit {
	const UiStrTable *strings;
	int page;
	LadxSettings settings;
	bool settings_unreadable;
} UiLadxGenerateInit;

typedef struct UiLadxGenerate {
	const UiStrTable *strings;
	int page;
	LadxSettings settings;
	bool spoiler;
	int item, scroll;
	int rows[UI_LADX_ROWS_MAX];
	int row_count;
	UiMessageKind message_kind;
	UiStrId message;
	char refusal[UI_GENERATE_REASON_MAX], reason[UI_GENERATE_REASON_MAX];
} UiLadxGenerate;

void ui_ladx_generate_init_defaults(UiLadxGenerateInit *init, const UiStrTable *strings, int page);

void ui_ladx_generate_init(UiLadxGenerate *state, const UiLadxGenerateInit *init);

UiGenerateEvent ui_ladx_generate_key(UiLadxGenerate *state, UiButton button);

void ui_ladx_generate_screen(const UiLadxGenerate *state, UiScreen *out);

void ui_ladx_generate_settings(const UiLadxGenerate *state, LadxSettings *settings, bool *spoiler);

#endif
