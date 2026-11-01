#ifndef UI_APP_H
#define UI_APP_H

#include "ui_generate.h"
#include "ui_genflow.h"
#include "ui_history.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>

/* The tabs' order on screen. */
typedef enum UiAppTab {
	UI_APP_TAB_GENERATE,
	UI_APP_TAB_HISTORY,
	UI_APP_TAB_COUNT
} UiAppTab;

typedef enum UiAppEvent {
	UI_APP_NONE,
	UI_APP_QUIT
} UiAppEvent;

/* Fills out, zeroed before the call, with the seeds on the card, newest first. */
typedef void (*UiHistoryScanFn)(void *ctx, UiHistoryList *out);

typedef struct UiAppInit {
	/* Borrowed for the app's life; NULL means English. */
	const UiStrTable *strings;
	/* ui_grid_page, and the text field (cols - 2), as UiHistoryInit takes them. */
	int page, width;
	/* Its strings and page are replaced by the app's. */
	UiGenerateInit generate;
	UiGenRunFn run;
	void *run_ctx;
	/* The three share history_ctx. */
	UiHistoryScanFn scan;
	UiHistorySpoilerFn spoiler_fn;
	UiHistoryDeleteFn delete_fn;
	void *history_ctx;
} UiAppInit;

/* Caller-owned, no allocation, about 210 KiB: a static object. Read only through the functions below. */
typedef struct UiApp {
	UiInput input;
	UiGenerate generate;
	UiGenFlow flow;
	UiHistory history;
	UiHistoryList list;
	UiGenRunFn run;
	void *run_ctx;
	/* The last run's outcome was ok. */
	bool generated;
	UiHistoryInit history_init;
	UiHistoryScanFn scan;
	UiAppTab tab;
	bool quit;
} UiApp;

/* The Generate tab active, the flow idle, History initialised from one scan. */
void ui_app_init(UiApp *app, const UiAppInit *init);

/* Feeds one raw press (down) or release through the button filter; acts as ui_app_key when it emits. */
UiAppEvent ui_app_input(UiApp *app, UiButton button, bool down);

/* Acts on one filtered button. Once QUIT is returned, every later call returns QUIT and changes nothing. */
UiAppEvent ui_app_key(UiApp *app, UiButton button);

/* True while a generation waits for ui_app_run: the shell draws ui_app_screen once first, since the run
 * blocks. */
bool ui_app_pending(const UiApp *app);

/* Runs the pending generation; after an ok outcome, rescans History and re-initialises it, the Generate
 * tab still active. Asserts a pending run. */
void ui_app_run(UiApp *app);

/* Fills out with the flow's screen while it is not idle, else the active tab's; texts borrowed from app,
 * valid until the next call on it. */
void ui_app_screen(UiApp *app, UiScreen *out);

UiAppTab ui_app_tab(const UiApp *app);

#endif
