#ifndef RANDO_APP_SHELL_H
#define RANDO_APP_SHELL_H

#include "rando_app_gen.h"
#include "rando_history.h"
#include "ui_app.h"
#include "ui_strings.h"

#include <stdbool.h>

/* Caller-owned, about 7.3 MiB: a static object. Read the app through ui_app_*; a test or a host tool may
 * replace gen's seed, date and digests after init, before the first run. */
typedef struct RandoAppShell {
	RandoAppGen gen;
	RandoHistory history;
	UiApp app;
} RandoAppShell;

/* The runner and the History store on sd_root, the last settings loaded (the unreadable-settings notice when
 * the file cannot be used, the defaults when it is missing), then the app on them. strings is borrowed (NULL
 * means English); page and width as UiAppInit takes them. Creates nothing on the card. False, with nothing a
 * caller may use, when a path under sd_root would not fit. */
bool rando_app_shell_init(RandoAppShell *shell, const char *sd_root, const UiStrTable *strings, int page,
			  int width);

#endif
