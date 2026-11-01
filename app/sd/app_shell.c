#include "rando_app_shell.h"

#include "rando_last_settings.h"

static void scan(void *ctx, UiHistoryList *out)
{
	rando_history_scan(ctx, out);
}

bool rando_app_shell_init(RandoAppShell *shell, const char *sd_root, const UiStrTable *strings, int page,
			  int width)
{
	UiAppInit init;

	if (!rando_app_gen_init(&shell->gen, sd_root) || !rando_history_init(&shell->history, sd_root))
		return false;
	init.strings = strings;
	init.page = page;
	init.width = width;
	ui_generate_init_defaults(&init.generate, strings, page);
	init.generate.settings_unreadable = rando_last_settings_load(shell->gen.settings, &init.generate.options,
								     &init.generate.heart_beep) ==
					    RANDO_LAST_SETTINGS_INVALID;
	init.run = rando_app_gen_run;
	init.run_ctx = &shell->gen;
	init.scan = scan;
	init.spoiler_fn = rando_history_spoiler;
	init.delete_fn = rando_history_delete;
	init.history_ctx = &shell->history;
	ui_app_init(&shell->app, &init);
	return true;
}
