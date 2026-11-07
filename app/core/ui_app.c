#include "ui_app.h"

#include <assert.h>
#include <string.h>

static void scan_history(UiApp *app)
{
	memset(&app->list, 0, sizeof app->list);
	app->scan(app->history_init.ctx, &app->list);
	ui_history_init(&app->history, &app->history_init);
}

static void run_and_note(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	UiApp *app = ctx;

	app->run(app->run_ctx, request, outcome);
	app->generated = outcome->ok;
}

void ui_app_init(UiApp *app, const UiAppInit *init)
{
	UiGenerateInit generate = init->generate;

	assert(init->run != NULL && init->scan != NULL);
	ui_input_init(&app->input);
	generate.strings = init->strings;
	generate.page = init->page;
	ui_generate_init(&app->generate, &generate);
	app->run = init->run;
	app->run_ctx = init->run_ctx;
	ui_genflow_init(&app->flow, init->strings, run_and_note, app);
	app->history_init.strings = init->strings;
	app->history_init.page = init->page;
	app->history_init.width = init->width;
	app->history_init.list = &app->list;
	app->history_init.spoiler_fn = init->spoiler_fn;
	app->history_init.delete_fn = init->delete_fn;
	app->history_init.ctx = init->history_ctx;
	app->scan = init->scan;
	scan_history(app);
	app->tab = UI_APP_TAB_GENERATE;
	ui_games_init(&app->games, init->strings, init->game);
	app->view = UI_APP_VIEW_GAMES;
	app->remember = init->remember;
	app->remember_ctx = init->remember_ctx;
	app->strings = init->strings;
	app->quit = false;
}

static UiAppEvent quit(UiApp *app)
{
	app->quit = true;
	return UI_APP_QUIT;
}

static UiAppEvent generate_key(UiApp *app, UiButton button)
{
	UiGenRequest request;

	switch (ui_generate_key(&app->generate, button)) {
	case UI_GENERATE_GENERATE:
		ui_generate_settings(&app->generate, &request.options, &request.heart_beep, &request.spoiler);
		ui_genflow_start(&app->flow, &request);
		break;
	case UI_GENERATE_BACK: app->view = UI_APP_VIEW_GAMES; break;
	case UI_GENERATE_QUIT: return quit(app);
	case UI_GENERATE_TAB_NEXT:
	case UI_GENERATE_TAB_PREV: app->tab = UI_APP_TAB_HISTORY; break;
	case UI_GENERATE_NONE: break;
	}
	return UI_APP_NONE;
}

static UiAppEvent history_key(UiApp *app, UiButton button)
{
	switch (ui_history_key(&app->history, button)) {
	case UI_HISTORY_BACK: app->view = UI_APP_VIEW_GAMES; break;
	case UI_HISTORY_QUIT: return quit(app);
	case UI_HISTORY_TAB_NEXT:
	case UI_HISTORY_TAB_PREV: app->tab = UI_APP_TAB_GENERATE; break;
	case UI_HISTORY_NONE: break;
	}
	return UI_APP_NONE;
}

static void open_game(UiApp *app)
{
	UiGame game = ui_games_selected(&app->games);

	if (app->remember != NULL)
		app->remember(app->remember_ctx, game);
	if (game == UI_GAME_ALTTP) {
		app->view = UI_APP_VIEW_ALTTP;
		app->tab = UI_APP_TAB_GENERATE;
	} else {
		app->view = UI_APP_VIEW_LADX;
	}
}

static UiAppEvent games_key(UiApp *app, UiButton button)
{
	switch (ui_games_key(&app->games, button)) {
	case UI_GAMES_OPEN: open_game(app); break;
	case UI_GAMES_QUIT: return quit(app);
	case UI_GAMES_NONE: break;
	}
	return UI_APP_NONE;
}

static UiAppEvent ladx_key(UiApp *app, UiButton button)
{
	if (button == UI_BUTTON_MENU)
		return quit(app);
	if (button == UI_BUTTON_B)
		app->view = UI_APP_VIEW_GAMES;
	return UI_APP_NONE;
}

UiAppEvent ui_app_key(UiApp *app, UiButton button)
{
	if (app->quit)
		return UI_APP_QUIT;
	switch (ui_genflow_phase(&app->flow)) {
	case UI_GENFLOW_PHASE_GENERATING: return UI_APP_NONE;
	case UI_GENFLOW_PHASE_DONE:
		return ui_genflow_key(&app->flow, button) == UI_GENFLOW_QUIT ? quit(app) : UI_APP_NONE;
	case UI_GENFLOW_PHASE_IDLE: break;
	}
	if (app->view == UI_APP_VIEW_GAMES)
		return games_key(app, button);
	if (app->view == UI_APP_VIEW_LADX)
		return ladx_key(app, button);
	if (app->tab == UI_APP_TAB_HISTORY)
		return history_key(app, button);
	return generate_key(app, button);
}

UiAppEvent ui_app_input(UiApp *app, UiButton button, bool down)
{
	UiButton filtered;

	if (app->quit)
		return UI_APP_QUIT;
	if (!ui_input_event(&app->input, button, down, &filtered))
		return UI_APP_NONE;
	return ui_app_key(app, filtered);
}

bool ui_app_pending(const UiApp *app)
{
	return ui_genflow_pending(&app->flow);
}

void ui_app_run(UiApp *app)
{
	assert(ui_app_pending(app));
	app->generated = false;
	ui_genflow_run(&app->flow);
	if (app->generated)
		scan_history(app);
}

/* TODO: Phase 4 replaces this placeholder with LADX's real tabs. */
static void ladx_screen(const UiStrTable *t, UiScreen *out)
{
	UiRow notice = {UI_ROW_TEXT, NULL, NULL, false, false};

	ui_screen_init(out);
	out->tabs[0] = ui_str(t, UI_STR_TAB_GENERATE);
	out->tab_count = 1;
	out->active_tab = 0;
	out->title = ui_str(t, UI_STR_GAME_LADX);
	out->body_kind = UI_BODY_NOTICE;
	notice.label = ui_str(t, UI_STR_GAME_UNAVAILABLE);
	ui_screen_add_row(out, notice);
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_B, UI_STR_HINT_BACK, false));
}

void ui_app_screen(UiApp *app, UiScreen *out)
{
	if (ui_genflow_phase(&app->flow) != UI_GENFLOW_PHASE_IDLE)
		ui_genflow_screen(&app->flow, out);
	else if (app->view == UI_APP_VIEW_GAMES)
		ui_games_screen(&app->games, out);
	else if (app->view == UI_APP_VIEW_LADX)
		ladx_screen(app->strings, out);
	else if (app->tab == UI_APP_TAB_HISTORY)
		ui_history_screen(&app->history, out);
	else
		ui_generate_screen(&app->generate, out);
}

UiAppTab ui_app_tab(const UiApp *app)
{
	return app->tab;
}

UiAppView ui_app_view(const UiApp *app)
{
	return app->view;
}
