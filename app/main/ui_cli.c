#include "rando_app_shell.h"
#include "rando_fixed_run.h"
#include "rando_settings.h"
#include "ui_app.h"
#include "ui_grid.h"
#include "ui_history.h"
#include "ui_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char usage[] = "usage: ui_cli --sd <root> [--every] [--cols <n>] [--lines <n>] [--seed <n>] "
			    "[--date <YYYY-MM-DDTHH:MM:SS>] [--source-md5 <32 hex>] [--base-md5 <32 hex>] "
			    "[--unsupported <setting>=<value>,<setting>=<value>] [--] <key>...\n"
			    "keys: up down left right a b x y l1 r1 l2 r2 select start menu, <key>*<n> repeats, "
			    "- reads keys from standard input\n";

typedef struct Options {
	const char *sd;
	bool every;
	int cols, lines;
	RandoFixedRun run;
	RandoFixedKeys keys;
} Options;

static RandoAppShell shell;
static Options opt;
static UiGrid grid;
static char text[UI_GRID_TEXT_MAX];

static void fail_usage(const char *what, const char *arg)
{
	fprintf(stderr, "ui_cli: %s%s%s\n%s", what, arg != NULL ? ": " : "", arg != NULL ? arg : "", usage);
	exit(2);
}

static bool parse_number(const char *s, uint32_t *out)
{
	return rando_parse_uint32(s, strlen(s), out) && strchr(s, '\n') == NULL;
}

static void fail_key(RandoFixedKeyStatus status, const char *token)
{
	switch (status) {
	case RANDO_FIXED_KEY_OK:
		return;
	case RANDO_FIXED_KEY_UNKNOWN:
		fail_usage("unknown key", token);
		break;
	case RANDO_FIXED_KEY_BAD_REPEAT:
		fail_usage("bad repeat count", token);
		break;
	case RANDO_FIXED_KEY_TOO_MANY:
		fail_usage("too many keys", NULL);
		break;
	}
}

static void add_keys(const char *arg)
{
	char buf[RANDO_FIXED_TOKEN_MAX + 1];
	const char *bad;
	RandoFixedKeyStatus status = rando_fixed_keys_arg(&opt.keys, arg, stdin, buf, &bad);

	fail_key(status, bad);
}

static const char *value_of(int argc, char **argv, int *i)
{
	if (*i + 1 >= argc)
		fail_usage("missing value for", argv[*i]);
	*i += 1;
	return argv[*i];
}

static void parse_size(const char *s, const char *what, int min, int max, int *out)
{
	uint32_t v;

	if (!parse_number(s, &v) || v < (uint32_t)min || v > (uint32_t)max)
		fail_usage(what, s);
	*out = (int)v;
}

static void parse_args(int argc, char **argv)
{
	bool keys_only = false;
	int i;

	opt.cols = UI_GRID_DEFAULT_COLS;
	opt.lines = UI_GRID_DEFAULT_LINES;
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (keys_only || strncmp(a, "--", 2) != 0) {
			keys_only = true;
			add_keys(a);
		} else if (strcmp(a, "--") == 0) {
			keys_only = true;
		} else if (strcmp(a, "--sd") == 0) {
			opt.sd = value_of(argc, argv, &i);
		} else if (strcmp(a, "--every") == 0) {
			opt.every = true;
		} else if (strcmp(a, "--cols") == 0) {
			parse_size(value_of(argc, argv, &i), "bad --cols", UI_HISTORY_WIDTH_MIN + 2, UI_GRID_COLS_MAX,
				   &opt.cols);
		} else if (strcmp(a, "--lines") == 0) {
			parse_size(value_of(argc, argv, &i), "bad --lines", UI_GRID_LINES_MIN, UI_GRID_LINES_MAX,
				   &opt.lines);
		} else if (rando_fixed_run_takes(a)) {
			char what[32];
			const char *v = value_of(argc, argv, &i);

			if (!rando_fixed_run_option(&opt.run, a, v)) {
				snprintf(what, sizeof what, "bad %s", a);
				fail_usage(what, v);
			}
		} else {
			fail_usage("unknown option", a);
		}
	}
	if (opt.sd == NULL)
		fail_usage("missing --sd", NULL);
}

static void print_screen(const char *header)
{
	static UiScreen screen;

	if (header != NULL)
		printf("@ %s\n", header);
	ui_app_screen(&shell.app, &screen);
	if (!ui_grid_render(&grid, &screen) || ui_grid_text(&grid, text, sizeof text) == 0) {
		fprintf(stderr, "ui_cli: the screen does not fit the grid\n");
		exit(1);
	}
	fputs(text, stdout);
}

static void set_up(void)
{
	if (!ui_grid_init(&grid, opt.cols, opt.lines))
		fail_usage("bad grid size", NULL);
	if (!rando_app_shell_init(&shell, opt.sd, NULL, rando_fixed_run_unsupported(&opt.run), 1, ui_grid_page(&grid),
				  opt.cols - 2)) {
		fprintf(stderr, "ui_cli: cannot use SD root %s\n", opt.sd);
		exit(1);
	}
	rando_fixed_run_apply(&opt.run, &shell);
}

static UiAppEvent play(UiButton button)
{
	if (ui_app_input(&shell.app, button, true) == UI_APP_QUIT)
		return UI_APP_QUIT;
	return ui_app_input(&shell.app, button, false);
}

int main(int argc, char **argv)
{
	long played = 0;
	int k, r;

	parse_args(argc, argv);
	set_up();
	if (opt.every)
		print_screen("init");
	for (k = 0; k < opt.keys.count; k++) {
		for (r = 0; r < opt.keys.key[k].count; r++) {
			const char *name = rando_fixed_key_name(opt.keys.key[k].button);
			UiAppEvent event = play(opt.keys.key[k].button);

			played++;
			if (ui_app_pending(&shell.app)) {
				if (opt.every)
					print_screen("generating");
				ui_app_run(&shell.app);
			}
			if (opt.every)
				print_screen(name);
			if (event == UI_APP_QUIT) {
				if (!opt.every)
					print_screen(NULL);
				if (played < opt.keys.presses)
					fprintf(stderr, "ui_cli: quit at key %ld of %ld, %ld ignored\n", played, opt.keys.presses,
						opt.keys.presses - played);
				return EXIT_SUCCESS;
			}
		}
	}
	if (!opt.every)
		print_screen(NULL);
	return EXIT_SUCCESS;
}
