#include "ui_app.h"

#include <assert.h>
#include <string.h>

static void scan_history(UiAppSide *side)
{
	memset(&side->list, 0, sizeof side->list);
	side->scan(side->history_init.ctx, &side->list);
	ui_history_init(&side->history, &side->history_init);
}

static void run_and_note(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	UiApp *app = ctx;
	UiAppSide *side = &app->sides[request->game];

	side->run(side->run_ctx, request, outcome);
	app->generated = outcome->ok;
}

static void init_side(UiAppSide *side, const UiAppInit *init, UiGame game, UiGenRunFn run, void *run_ctx,
		      UiHistoryScanFn scan, UiHistorySpoilerFn spoiler_fn, UiHistoryDeleteFn delete_fn,
		      void *history_ctx)
{
	assert(run != NULL && scan != NULL);
	side->run = run;
	side->run_ctx = run_ctx;
	side->history_init.strings = init->strings;
	side->history_init.game = game;
	side->history_init.page = init->page;
	side->history_init.width = init->width;
	side->history_init.list = &side->list;
	side->history_init.spoiler_fn = spoiler_fn;
	side->history_init.delete_fn = delete_fn;
	side->history_init.ctx = history_ctx;
	side->scan = scan;
	scan_history(side);
	side->tab = UI_APP_TAB_GENERATE;
}

void ui_app_init(UiApp *app, const UiAppInit *init)
{
	UiGenerateInit generate = init->generate;
	UiLadxGenerateInit ladx_generate = init->ladx.generate;
	const UiAppLadxInit *ladx = &init->ladx;

	ui_input_init(&app->input);
	generate.strings = init->strings;
	generate.page = init->page;
	ui_generate_init(&app->generate, &generate);
	ladx_generate.strings = init->strings;
	ladx_generate.page = init->page;
	ui_ladx_generate_init(&app->ladx_generate, &ladx_generate);
	ui_genflow_init(&app->flow, init->strings, run_and_note, app);
	init_side(&app->sides[UI_GAME_ALTTP], init, UI_GAME_ALTTP, init->run, init->run_ctx, init->scan,
		  init->spoiler_fn, init->delete_fn, init->history_ctx);
	init_side(&app->sides[UI_GAME_LADX], init, UI_GAME_LADX, ladx->run, ladx->run_ctx, ladx->scan,
		  ladx->spoiler_fn, ladx->delete_fn, ladx->history_ctx);
	ui_games_init(&app->games, init->strings, init->game);
	app->view = UI_APP_VIEW_GAMES;
	app->remember = init->remember;
	app->remember_ctx = init->remember_ctx;
	app->strings = init->strings;
	app->quit = false;
}

static UiGame shown_game(const UiApp *app)
{
	assert(app->view != UI_APP_VIEW_GAMES);
	return app->view == UI_APP_VIEW_LADX ? UI_GAME_LADX : UI_GAME_ALTTP;
}

static UiAppSide *shown_side(UiApp *app)
{
	return &app->sides[shown_game(app)];
}

static UiAppEvent quit(UiApp *app)
{
	app->quit = true;
	return UI_APP_QUIT;
}

static UiGenerateEvent generate_input(UiApp *app, UiButton button, UiGenRequest *request)
{
	UiGenerateEvent event;

	memset(request, 0, sizeof *request);
	request->game = shown_game(app);
	if (request->game == UI_GAME_LADX) {
		event = ui_ladx_generate_key(&app->ladx_generate, button);
		ui_ladx_generate_settings(&app->ladx_generate, &request->ladx, &request->spoiler);
	} else {
		event = ui_generate_key(&app->generate, button);
		ui_generate_settings(&app->generate, &request->options, &request->heart_beep, &request->spoiler);
	}
	return event;
}

static UiAppEvent generate_key(UiApp *app, UiButton button)
{
	UiGenRequest request;

	switch (generate_input(app, button, &request)) {
	case UI_GENERATE_GENERATE: ui_genflow_start(&app->flow, &request); break;
	case UI_GENERATE_BACK: app->view = UI_APP_VIEW_GAMES; break;
	case UI_GENERATE_QUIT: return quit(app);
	case UI_GENERATE_TAB_NEXT:
	case UI_GENERATE_TAB_PREV: shown_side(app)->tab = UI_APP_TAB_HISTORY; break;
	case UI_GENERATE_NONE: break;
	}
	return UI_APP_NONE;
}

static UiAppEvent history_key(UiApp *app, UiButton button)
{
	UiAppSide *side = shown_side(app);

	switch (ui_history_key(&side->history, button)) {
	case UI_HISTORY_BACK: app->view = UI_APP_VIEW_GAMES; break;
	case UI_HISTORY_QUIT: return quit(app);
	case UI_HISTORY_TAB_NEXT:
	case UI_HISTORY_TAB_PREV: side->tab = UI_APP_TAB_GENERATE; break;
	case UI_HISTORY_NONE: break;
	}
	return UI_APP_NONE;
}

static void open_game(UiApp *app)
{
	UiGame game = ui_games_selected(&app->games);

	if (app->remember != NULL)
		app->remember(app->remember_ctx, game);
	app->view = game == UI_GAME_LADX ? UI_APP_VIEW_LADX : UI_APP_VIEW_ALTTP;
	app->sides[game].tab = UI_APP_TAB_GENERATE;
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
	if (shown_side(app)->tab == UI_APP_TAB_HISTORY)
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
		scan_history(&app->sides[app->flow.request.game]);
}

void ui_app_screen(UiApp *app, UiScreen *out)
{
	if (ui_genflow_phase(&app->flow) != UI_GENFLOW_PHASE_IDLE)
		ui_genflow_screen(&app->flow, out);
	else if (app->view == UI_APP_VIEW_GAMES)
		ui_games_screen(&app->games, out);
	else if (shown_side(app)->tab == UI_APP_TAB_HISTORY)
		ui_history_screen(&shown_side(app)->history, out);
	else if (app->view == UI_APP_VIEW_LADX)
		ui_ladx_generate_screen(&app->ladx_generate, out);
	else
		ui_generate_screen(&app->generate, out);
}

UiAppTab ui_app_tab(const UiApp *app)
{
	return app->sides[UI_GAME_ALTTP].tab;
}

UiAppView ui_app_view(const UiApp *app)
{
	return app->view;
}
