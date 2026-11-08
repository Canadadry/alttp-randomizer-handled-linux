#ifndef UI_APP_H
#define UI_APP_H

#include "ui_generate.h"
#include "ui_games.h"
#include "ui_genflow.h"
#include "ui_history.h"
#include "ui_ladx_generate.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>

typedef enum UiAppTab {
	UI_APP_TAB_GENERATE,
	UI_APP_TAB_HISTORY,
	UI_APP_TAB_COUNT
} UiAppTab;

typedef enum UiAppView {
	UI_APP_VIEW_GAMES,
	UI_APP_VIEW_ALTTP,
	UI_APP_VIEW_LADX
} UiAppView;

typedef enum UiAppEvent {
	UI_APP_NONE,
	UI_APP_QUIT
} UiAppEvent;

typedef void (*UiHistoryScanFn)(void *ctx, UiHistoryList *out);

typedef void (*UiAppRememberFn)(void *ctx, UiGame game);

/* LADX's tabs; every function is required. */
typedef struct UiAppLadxInit {
	UiLadxGenerateInit generate;
	UiGenRunFn run;
	void *run_ctx;
	UiHistoryScanFn scan;
	UiHistorySpoilerFn spoiler_fn;
	UiHistoryDeleteFn delete_fn;
	void *history_ctx;
} UiAppLadxInit;

/* One game's History tab and runner. */
typedef struct UiAppSide {
	UiHistory history;
	UiHistoryList list;
	UiHistoryInit history_init;
	UiHistoryScanFn scan;
	UiGenRunFn run;
	void *run_ctx;
	UiAppTab tab;
} UiAppSide;

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
	UiGame game;
	/* NULL: nothing is remembered. Called with the game the player opens, before it opens. */
	UiAppRememberFn remember;
	void *remember_ctx;
	UiAppLadxInit ladx;
} UiAppInit;

typedef struct UiApp {
	const UiStrTable *strings;
	UiInput input;
	UiGenerate generate;
	UiLadxGenerate ladx_generate;
	UiGenFlow flow;
	UiAppSide sides[UI_GAME_COUNT];
	bool generated;
	UiGames games;
	UiAppView view;
	UiAppRememberFn remember;
	void *remember_ctx;
	bool quit;
} UiApp;

void ui_app_init(UiApp *app, const UiAppInit *init);

UiAppEvent ui_app_input(UiApp *app, UiButton button, bool down);

UiAppEvent ui_app_key(UiApp *app, UiButton button);

bool ui_app_pending(const UiApp *app);

void ui_app_run(UiApp *app);

void ui_app_screen(UiApp *app, UiScreen *out);

/* ALttP's tab, whichever view is shown. */
UiAppTab ui_app_tab(const UiApp *app);

UiAppView ui_app_view(const UiApp *app);

#endif
