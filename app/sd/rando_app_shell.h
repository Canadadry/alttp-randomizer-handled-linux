#ifndef RANDO_APP_SHELL_H
#define RANDO_APP_SHELL_H

#include "rando_app_gen.h"
#include "rando_history.h"
#include "rando_ladx_gen.h"
#include "rando_ladx_history.h"
#include "rando_seed_save.h"
#include "ui_app.h"
#include "ui_strings.h"

#include <stdbool.h>

typedef struct RandoAppShell {
	RandoAppGen gen;
	RandoHistory history;
	RandoLadxGen ladx_gen;
	RandoLadxHistory ladx_history;
	RandoLadxHistoryList ladx_list;
	char ladx_settings[RANDO_SEED_PATH_MAX];
	UiApp app;
	char last_game[RANDO_SEED_PATH_MAX];
	/* The game the memory file holds, UI_GAME_COUNT when it is missing, invalid or unwritten. */
	UiGame remembered;
} RandoAppShell;

/* unsupported NULL: the library's list; otherwise its unsupported_count pairs, kept by pointer, so
   they must outlive the shell. */
bool rando_app_shell_init(RandoAppShell *shell, const char *sd_root, const UiStrTable *strings,
			  const RandoSettingPair *unsupported, int unsupported_count, int page, int width);

#endif
