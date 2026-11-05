#ifndef RANDO_APP_SHELL_H
#define RANDO_APP_SHELL_H

#include "rando_app_gen.h"
#include "rando_history.h"
#include "ui_app.h"
#include "ui_strings.h"

#include <stdbool.h>

typedef struct RandoAppShell {
	RandoAppGen gen;
	RandoHistory history;
	UiApp app;
} RandoAppShell;

/* unsupported NULL: the library's list; otherwise its unsupported_count pairs, kept by pointer, so
   they must outlive the shell. */
bool rando_app_shell_init(RandoAppShell *shell, const char *sd_root, const UiStrTable *strings,
			  const RandoSettingPair *unsupported, int unsupported_count, int page, int width);

#endif
