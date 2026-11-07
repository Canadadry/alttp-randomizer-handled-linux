#include "rando_app_shell.h"

#include "rando_last_game.h"
#include "rando_last_settings.h"

#include <stdio.h>

static void scan(void *ctx, UiHistoryList *out)
{
	rando_history_scan(ctx, out);
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

	if (!rando_app_gen_init(&shell->gen, sd_root) || !rando_history_init(&shell->history, sd_root))
		return false;
	n = snprintf(shell->last_game, sizeof shell->last_game, "%s/%s", sd_root, RANDO_SD_LAST_GAME_FILE);
	if (n < 0 || (size_t)n >= sizeof shell->last_game)
		return false;
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
	ui_app_init(&shell->app, &init);
	return true;
}
