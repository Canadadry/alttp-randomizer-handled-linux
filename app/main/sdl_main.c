#define _POSIX_C_SOURCE 200809L

#include "rando_app_gen.h"
#include "rando_app_shell.h"
#include "rando_fixed_run.h"
#include "sdl_loop.h"
#include "sdl_render.h"
#include "theme.h"
#include "ui_grid.h"
#include "ui_history.h"

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCREEN_W 640
#define SCREEN_H 480
#define CWD_MAX 4096
#define DEFAULT_THEME "themes/default"

static const char usage[] = "usage: alttpr [--sd <root>] [--theme <dir>] [--frames <n>] [--shot <file.bmp>] "
			    "[--shot-text <file>] [--seed <n>] [--date <YYYY-MM-DDTHH:MM:SS>] [--source-md5 <32 hex>] "
			    "[--base-md5 <32 hex>] [--unsupported <setting>=<value>,<setting>=<value>] [--] [<key>...]\n"
			    "keys: up down left right a b x y l1 r1 l2 r2 select start menu, <key>*<n> repeats, "
			    "- reads keys from standard input; with keys, the run ends after the last one\n";

typedef struct Options {
	const char *sd, *theme, *shot, *shot_text;
	long frames;
	bool scripted;
	RandoFixedRun run;
	RandoFixedKeys keys;
} Options;

typedef struct ScriptPos {
	const RandoFixedKeys *keys;
	int key, repeat;
} ScriptPos;

/* tab_count is that of the screen render refused. */
typedef struct Presenter {
	SdlRender *render;
	SDL_Surface *offscreen, *screen;
	int tab_count;
} Presenter;

/* About 7.3 MiB, near Linux's 8 MiB default stack: keep it static. */
static RandoAppShell shell;
static Options opt;
static char text[UI_GRID_TEXT_MAX];
static SdlRender render;
static UiGrid grid;
static Theme theme;
static char root[RANDO_SEED_DIR_MAX];
static char cwd[CWD_MAX];
static char default_theme[CWD_MAX];

static void fail_usage(const char *what, const char *arg)
{
	fprintf(stderr, "alttpr: %s%s%s\n%s", what, arg != NULL ? ": " : "", arg != NULL ? arg : "", usage);
	exit(2);
}

/* Printed after SDL_Quit restores the device's console; why is copied first, SDL_Quit clears SDL's error. */
static void fail_setup(const char *what, const char *why)
{
	static char line[CWD_MAX + 64];

	snprintf(line, sizeof line, "alttpr: %s%s%s\n", what, why != NULL ? ": " : "", why != NULL ? why : "");
	IMG_Quit();
	TTF_Quit();
	SDL_Quit();
	fputs(line, stderr);
	exit(1);
}

static const char *value_of(int argc, char **argv, int *i)
{
	if (*i + 1 >= argc)
		fail_usage("missing value for", argv[*i]);
	*i += 1;
	return argv[*i];
}

static void add_keys(const char *arg)
{
	char buf[RANDO_FIXED_TOKEN_MAX + 1];
	const char *bad = NULL;

	opt.scripted = true;
	switch (rando_fixed_keys_arg(&opt.keys, arg, stdin, buf, &bad)) {
	case RANDO_FIXED_KEY_OK:
		break;
	case RANDO_FIXED_KEY_UNKNOWN:
		fail_usage("unknown key", bad);
		break;
	case RANDO_FIXED_KEY_BAD_REPEAT:
		fail_usage("bad repeat count", bad);
		break;
	case RANDO_FIXED_KEY_TOO_MANY:
		fail_usage("too many keys", NULL);
		break;
	}
}

static void parse_args(int argc, char **argv)
{
	bool keys_only = false;
	int i;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (keys_only || strncmp(a, "--", 2) != 0) {
			keys_only = true;
			add_keys(a);
		} else if (strcmp(a, "--") == 0) {
			keys_only = true;
		} else if (strcmp(a, "--sd") == 0) {
			opt.sd = value_of(argc, argv, &i);
		} else if (strcmp(a, "--theme") == 0) {
			opt.theme = value_of(argc, argv, &i);
		} else if (strcmp(a, "--shot") == 0) {
			opt.shot = value_of(argc, argv, &i);
		} else if (strcmp(a, "--shot-text") == 0) {
			opt.shot_text = value_of(argc, argv, &i);
		} else if (rando_fixed_run_takes(a)) {
			char what[32];
			const char *v = value_of(argc, argv, &i);

			if (!rando_fixed_run_option(&opt.run, a, v)) {
				snprintf(what, sizeof what, "bad %s", a);
				fail_usage(what, v);
			}
		} else if (strcmp(a, "--frames") == 0) {
			const char *v = value_of(argc, argv, &i);
			char *end = NULL;

			if (v[0] < '0' || v[0] > '9' || (opt.frames = strtol(v, &end, 10)) < 1 || opt.frames > 1000000 ||
			    *end != '\0')
				fail_usage("bad --frames", v);
		} else {
			fail_usage("unknown option", a);
		}
	}
}

/* Without --theme: themes/default next to the binary, argv[0]'s directory, or . when it has no slash. */
static void use_default_theme(const char *argv0)
{
	const char *slash = argv0 != NULL ? strrchr(argv0, '/') : NULL;
	int n;

	if (slash == NULL)
		n = snprintf(default_theme, sizeof default_theme, "./%s", DEFAULT_THEME);
	else
		n = snprintf(default_theme, sizeof default_theme, "%.*s/%s", (int)(slash - argv0), argv0, DEFAULT_THEME);
	if (n < 0 || (size_t)n >= sizeof default_theme)
		fail_setup("default theme path does not fit", argv0);
	opt.theme = default_theme;
}

static void fail_render(const SdlRenderFailure *f)
{
	static char line[CWD_MAX];
	static ThemeAsset assets[THEME_ASSETS_MAX];
	int count = theme_assets(&theme, assets);
	const ThemeAsset *a;
	const ThemeCell *cell;
	int n;

	if (f->asset < 0 || f->asset >= count)
		fail_setup(opt.theme, "theme asset out of range");
	a = &assets[f->asset];
	n = snprintf(line, sizeof line, "%s/%s: %s: ", opt.theme, a->file, a->key);
	if (n < 0 || (size_t)n >= sizeof line)
		n = 0;
	switch (f->reason) {
	case SDLRENDER_OPEN:
		snprintf(line + n, sizeof line - (size_t)n, "%s", f->error);
		break;
	case SDLRENDER_FIT:
		cell = &theme.fonts[a->font].cell;
		snprintf(line + n, sizeof line - (size_t)n, "no size from 64 to 8 fits the %dx%d cell", cell->w, cell->h);
		break;
	case SDLRENDER_SIZE:
		snprintf(line + n, sizeof line - (size_t)n, "%dx%d, the screen is %dx%d", f->w, f->h, SCREEN_W, SCREEN_H);
		break;
	case SDLRENDER_PATH:
		snprintf(line + n, sizeof line - (size_t)n, "path too long");
		break;
	}
	fail_setup(line, NULL);
}

/* The grid is width + 2 by page + 4, so the grid drawn and the page the core scrolls by agree. */
static void open_theme(int *page, int *width)
{
	static char line[THEME_ERROR_MAX + 128];
	ThemeError err;
	SdlRenderFailure f;

	if (!theme_load(&theme, opt.theme, &err))
		fail_setup(err.message, NULL);
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		fail_setup("SDL_Init", SDL_GetError());
	if (TTF_Init() != 0)
		fail_setup("TTF_Init", TTF_GetError());
	if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
		fail_setup("IMG_Init", IMG_GetError());
	if (!sdl_render_open(&render, &theme, opt.theme, &f))
		fail_render(&f);
	*page = theme_page(&theme);
	*width = theme_columns(&theme, sdl_render_advance(&render));
	if (*page < 1 || *page > UI_GRID_LINES_MAX - 4 || *width < UI_HISTORY_WIDTH_MIN ||
	    *width > UI_HISTORY_WIDTH_MAX) {
		snprintf(line, sizeof line, "%s/theme.ini: page %d, width %d: the app shows page 1..%d, width %d..%d",
			 opt.theme, *page, *width, UI_GRID_LINES_MAX - 4, UI_HISTORY_WIDTH_MIN, UI_HISTORY_WIDTH_MAX);
		fail_setup(line, NULL);
	}
	if (!ui_grid_init(&grid, *width + 2, *page + 4))
		fail_setup("grid", NULL);
}

static bool present(void *ctx, const UiScreen *screen, const UiGrid *g)
{
	Presenter *p = ctx;

	(void)g;
	if (!sdl_render_draw(p->render, screen, p->offscreen)) {
		p->tab_count = screen->tab_count;
		return false;
	}
	SDL_BlitSurface(p->offscreen, NULL, p->screen, NULL);
	SDL_Flip(p->screen);
	return true;
}

static bool script_next(void *ctx, UiButton *out)
{
	ScriptPos *pos = ctx;

	while (pos->key < pos->keys->count && pos->repeat >= pos->keys->key[pos->key].count) {
		pos->key++;
		pos->repeat = 0;
	}
	if (pos->key >= pos->keys->count)
		return false;
	pos->repeat++;
	*out = pos->keys->key[pos->key].button;
	return true;
}

static void write_shot_text(const char *path)
{
	FILE *f;
	size_t n = ui_grid_text(&grid, text, sizeof text);

	if (n == 0)
		fail_setup(path, "the screen does not fit the grid");
	f = fopen(path, "wb");
	if (f == NULL)
		fail_setup(path, strerror(errno));
	if (fwrite(text, 1, n, f) != n || fclose(f) != 0)
		fail_setup(path, "cannot write");
}

int main(int argc, char **argv)
{
	Presenter p;
	ScriptPos pos;
	SdlScript script;
	SdlLoopResult result;
	int page, width;

	parse_args(argc, argv);
	if (opt.theme == NULL)
		use_default_theme(argv[0]);
	pos.keys = &opt.keys;
	pos.key = 0;
	pos.repeat = 0;
	script.next = script_next;
	script.ctx = &pos;
	if (opt.sd != NULL) {
		if (strlen(opt.sd) + 1 > sizeof root)
			fail_setup("SD root does not fit", opt.sd);
		strcpy(root, opt.sd);
	} else if (getcwd(cwd, sizeof cwd) == NULL || !rando_app_sd_root(cwd, root, sizeof root)) {
		fail_setup("SD root does not fit", NULL);
	}
	open_theme(&page, &width);
	if (!rando_app_shell_init(&shell, root, NULL, rando_fixed_run_unsupported(&opt.run), 1, page, width))
		fail_setup("SD root does not fit", root);
	rando_fixed_run_apply(&opt.run, &shell);
	p.screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 32, SDL_HWSURFACE);
	if (p.screen == NULL)
		fail_setup("SDL_SetVideoMode", SDL_GetError());
	SDL_ShowCursor(SDL_DISABLE);
	SDL_EnableKeyRepeat(300, 50);
	p.offscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, p.screen->w, p.screen->h, 32, 0x00FF0000, 0x0000FF00,
					   0x000000FF, 0);
	if (p.offscreen == NULL)
		fail_setup("SDL_CreateRGBSurface", SDL_GetError());
	p.render = &render;
	p.tab_count = 0;
	result = sdl_loop_run(&shell.app, &grid, present, &p, opt.frames, opt.scripted ? &script : NULL);
	if (result == SDLLOOP_ERROR)
		fail_setup("SDL_WaitEvent", SDL_GetError());
	if (result == SDLLOOP_PRESENT_FAILED) {
		static char line[CWD_MAX];

		snprintf(line, sizeof line, "%s/theme.ini: the screen has %d tabs, the theme %d tab slots", opt.theme,
			 p.tab_count, theme.tabs.slot_count);
		fail_setup(line, NULL);
	}
	if (opt.shot != NULL && SDL_SaveBMP(p.offscreen, opt.shot) != 0)
		fail_setup(opt.shot, SDL_GetError());
	if (opt.shot_text != NULL)
		write_shot_text(opt.shot_text);
	sdl_render_close(&render);
	SDL_FreeSurface(p.offscreen);
	IMG_Quit();
	TTF_Quit();
	SDL_Quit();
	return 0;
}
