#ifndef UI_GENERATE_H
#define UI_GENERATE_H

#include "rando_options.h"
#include "rando_settings.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>

#define UI_GENERATE_ROW_SPOILER RANDO_SETTING_COUNT
#define UI_GENERATE_ROW_ACTION (RANDO_SETTING_COUNT + 1)
#define UI_GENERATE_ROW_COUNT (RANDO_SETTING_COUNT + 2)
#define UI_GENERATE_REASON_MAX 96

typedef enum UiGenerateEvent {
	UI_GENERATE_NONE,
	UI_GENERATE_GENERATE,
	UI_GENERATE_QUIT,
	UI_GENERATE_TAB_NEXT,
	UI_GENERATE_TAB_PREV
} UiGenerateEvent;

typedef struct UiGenerateInit {
	const UiStrTable *strings;
	int page;
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool settings_unreadable;
	const RandoSettingPair *unsupported;
	int unsupported_count;
} UiGenerateInit;

typedef struct UiGenerate {
	const UiStrTable *strings;
	int page;
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool spoiler;
	int cursor, scroll;
	const RandoSettingPair *unsupported;
	int unsupported_count;
	UiMessageKind message_kind;
	UiStrId message;
	char refusal[UI_GENERATE_REASON_MAX], reason[UI_GENERATE_REASON_MAX];
} UiGenerate;

void ui_generate_init_defaults(UiGenerateInit *init, const UiStrTable *strings, int page);

void ui_generate_init(UiGenerate *state, const UiGenerateInit *init);

UiGenerateEvent ui_generate_key(UiGenerate *state, UiButton button);

void ui_generate_screen(const UiGenerate *state, UiScreen *out);

void ui_generate_settings(const UiGenerate *state, RandoOptions *options, RandoHeartBeep *heart_beep, bool *spoiler);

#endif
