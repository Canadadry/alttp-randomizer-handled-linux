#include "rando_app_shell.h"

#include "rando_ladx_last_settings.h"
#include "rando_last_game.h"
#include "rando_last_settings.h"

#include <stdio.h>
#include <string.h>

static void scan(void *ctx, UiHistoryList *out)
{
	rando_history_scan(ctx, out);
}

typedef char ladx_seed_fits[sizeof(((UiHistoryEntry *)0)->ladx_seed) == RANDO_LADX_SEED_HEX + 1 ? 1 : -1];

static void ladx_run(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	RandoAppShell *shell = ctx;
	RandoLadxGenRequest ladx;

	(void)rando_ladx_last_settings_save(shell->ladx_settings, &request->ladx);
	ladx.settings = request->ladx;
	ladx.spoiler = request->spoiler;
	rando_ladx_gen_run(&shell->ladx_gen, &ladx, outcome);
}

static void ladx_scan(void *ctx, UiHistoryList *out)
{
	RandoAppShell *shell = ctx;
	RandoLadxHistoryList *list = &shell->ladx_list;
	int i;

	rando_ladx_history_scan(&shell->ladx_history, list);
	out->count = list->count;
	out->unreadable = list->unreadable;
	out->truncated = list->truncated;
	out->folder_unreadable = list->folder_unreadable;
	for (i = 0; i < list->count; i++) {
		const RandoLadxRecord *r = &list->entries[i];
		UiHistoryEntry *e = &out->entries[i];

		memset(e, 0, sizeof *e);
		e->game = UI_GAME_LADX;
		e->name = r->name;
		e->date.year = r->date.year;
		e->date.month = r->date.month;
		e->date.day = r->date.day;
		e->date.hour = r->date.hour;
		e->date.minute = r->date.minute;
		e->date.second = r->date.second;
		memcpy(e->ladx_seed, r->seed, sizeof e->ladx_seed);
		e->ladx = r->settings;
		e->has_spoiler = r->has_spoiler;
	}
}

static bool ladx_spoiler(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out)
{
	RandoAppShell *shell = ctx;

	return rando_ladx_history_spoiler(&shell->ladx_history, &entry->name, out);
}

static void ladx_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome)
{
	RandoAppShell *shell = ctx;

	rando_ladx_history_delete(&shell->ladx_history, &entry->name, outcome);
}

static void remember(void *ctx, UiGame game)
{
	RandoAppShell *shell = ctx;

	if (game != shell->remembered && rando_last_game_save(shell->last_game, game))
		shell->remembered = game;
}

bool rando_app_shell_init(RandoAppShell *shell, const char *sd_root, const UiStrTable *strings,
			  const RandoSettingPair *unsupported, int unsupported_count, int page, int width)
{
	UiAppInit init;
	int n;

	if (!rando_app_gen_init(&shell->gen, sd_root) || !rando_history_init(&shell->history, sd_root) ||
	    !rando_ladx_gen_init(&shell->ladx_gen, sd_root) || !rando_ladx_history_init(&shell->ladx_history, sd_root))
		return false;
	n = snprintf(shell->ladx_settings, sizeof shell->ladx_settings, "%s/%s", sd_root, RANDO_SD_LADX_SETTINGS_FILE);
	if (n < 0 || (size_t)n >= sizeof shell->ladx_settings)
		return false;
	n = snprintf(shell->last_game, sizeof shell->last_game, "%s/%s", sd_root, RANDO_SD_LAST_GAME_FILE);
	if (n < 0 || (size_t)n >= sizeof shell->last_game)
		return false;
	memset(&init, 0, sizeof init);
	init.strings = strings;
	init.page = page;
	init.width = width;
	ui_generate_init_defaults(&init.generate, strings, page);
	if (unsupported != NULL) {
		init.generate.unsupported = unsupported;
		init.generate.unsupported_count = unsupported_count;
	}
	init.generate.settings_unreadable = rando_last_settings_load(shell->gen.settings, &init.generate.options,
								     &init.generate.heart_beep) ==
					    RANDO_LAST_SETTINGS_INVALID;
	init.run = rando_app_gen_run;
	init.run_ctx = &shell->gen;
	init.scan = scan;
	init.spoiler_fn = rando_history_spoiler;
	init.delete_fn = rando_history_delete;
	init.history_ctx = &shell->history;
	shell->remembered = rando_last_game_load(shell->last_game, &init.game) == RANDO_LAST_GAME_LOADED
				    ? init.game
				    : UI_GAME_COUNT;
	init.remember = remember;
	init.remember_ctx = shell;
	ui_ladx_generate_init_defaults(&init.ladx.generate, strings, page);
	init.ladx.generate.settings_unreadable =
		rando_ladx_last_settings_load(shell->ladx_settings, &init.ladx.generate.settings) ==
		RANDO_LAST_SETTINGS_INVALID;
	init.ladx.run = ladx_run;
	init.ladx.run_ctx = shell;
	init.ladx.scan = ladx_scan;
	init.ladx.spoiler_fn = ladx_spoiler;
	init.ladx.delete_fn = ladx_delete;
	init.ladx.history_ctx = shell;
	ui_app_init(&shell->app, &init);
	return true;
}
