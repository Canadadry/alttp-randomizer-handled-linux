#ifndef UI_APP_H
#define UI_APP_H

#include "ui_generate.h"
#include "ui_genflow.h"
#include "ui_history.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>

typedef enum UiAppTab {
	UI_APP_TAB_GENERATE,
	UI_APP_TAB_HISTORY,
	UI_APP_TAB_COUNT
} UiAppTab;

typedef enum UiAppEvent {
	UI_APP_NONE,
	UI_APP_QUIT
} UiAppEvent;

typedef void (*UiHistoryScanFn)(void *ctx, UiHistoryList *out);

typedef struct UiAppInit {
	const UiStrTable *strings;
	int page, width;
	UiGenerateInit generate;
	UiGenRunFn run;
	void *run_ctx;
	UiHistoryScanFn scan;
	UiHistorySpoilerFn spoiler_fn;
	UiHistoryDeleteFn delete_fn;
	void *history_ctx;
} UiAppInit;

typedef struct UiApp {
	UiInput input;
	UiGenerate generate;
	UiGenFlow flow;
	UiHistory history;
	UiHistoryList list;
	UiGenRunFn run;
	void *run_ctx;
	bool generated;
	UiHistoryInit history_init;
	UiHistoryScanFn scan;
	UiAppTab tab;
	bool quit;
} UiApp;

void ui_app_init(UiApp *app, const UiAppInit *init);

UiAppEvent ui_app_input(UiApp *app, UiButton button, bool down);

UiAppEvent ui_app_key(UiApp *app, UiButton button);

bool ui_app_pending(const UiApp *app);

void ui_app_run(UiApp *app);

void ui_app_screen(UiApp *app, UiScreen *out);

UiAppTab ui_app_tab(const UiApp *app);

#endif
