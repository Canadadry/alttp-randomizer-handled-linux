#define _POSIX_C_SOURCE 200809L

#include "rando_app_gen.h"
#include "rando_app_shell.h"
#include "sdl_loop.h"
#include "sdl_view.h"
#include "ui_grid.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCREEN_W 640
#define SCREEN_H 480
#define CWD_MAX 4096
#define DEFAULT_FONT "DejaVuSansMono.ttf"

static const char usage[] = "usage: alttpr [--sd <root>] [--font <file>] [--frames <n>] [--shot <file.bmp>]\n";

typedef struct Options {
	const char *sd, *font, *shot;
	long frames;
} Options;

typedef struct Presenter {
	SdlView *view;
	SDL_Surface *offscreen, *screen;
} Presenter;

/* About 7.3 MiB, near Linux's 8 MiB default stack: keep it static. */
static RandoAppShell shell;
static SdlView view;
static UiGrid grid;
static char root[RANDO_SEED_DIR_MAX];
static char cwd[CWD_MAX];

static void fail_usage(const char *what, const char *arg)
{
	fprintf(stderr, "alttpr: %s%s%s\n%s", what, arg != NULL ? ": " : "", arg != NULL ? arg : "", usage);
	exit(2);
}

/* Printed after SDL_Quit restores the device's console; why is copied first, SDL_Quit clears SDL's error. */
static void fail_setup(const char *what, const char *why)
{
	static char line[1024];

	snprintf(line, sizeof line, "alttpr: %s%s%s\n", what, why != NULL ? ": " : "", why != NULL ? why : "");
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

static void parse_args(int argc, char **argv, Options *opt)
{
	int i;

	opt->sd = NULL;
	opt->font = DEFAULT_FONT;
	opt->shot = NULL;
	opt->frames = 0;
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "--sd") == 0) {
			opt->sd = value_of(argc, argv, &i);
		} else if (strcmp(a, "--font") == 0) {
			opt->font = value_of(argc, argv, &i);
		} else if (strcmp(a, "--shot") == 0) {
			opt->shot = value_of(argc, argv, &i);
		} else if (strcmp(a, "--frames") == 0) {
			const char *v = value_of(argc, argv, &i);
			char *end = NULL;

			if (v[0] < '0' || v[0] > '9' || (opt->frames = strtol(v, &end, 10)) < 1 || opt->frames > 1000000 ||
			    *end != '\0')
				fail_usage("bad --frames", v);
		} else {
			fail_usage("unknown option", a);
		}
	}
}

static void present(void *ctx, const UiGrid *g)
{
	Presenter *p = ctx;

	sdl_view_draw(p->view, g, p->offscreen);
	SDL_BlitSurface(p->offscreen, NULL, p->screen, NULL);
	SDL_Flip(p->screen);
}

int main(int argc, char **argv)
{
	Options opt;
	Presenter p;
	SdlViewStatus st;
	SdlLoopResult result;

	parse_args(argc, argv, &opt);
	if (opt.sd != NULL) {
		if (strlen(opt.sd) + 1 > sizeof root)
			fail_setup("SD root does not fit", opt.sd);
		strcpy(root, opt.sd);
	} else if (getcwd(cwd, sizeof cwd) == NULL || !rando_app_sd_root(cwd, root, sizeof root)) {
		fail_setup("SD root does not fit", NULL);
	}
	if (!ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES))
		fail_setup("grid", NULL);
	if (!rando_app_shell_init(&shell, root, NULL, ui_grid_page(&grid), grid.cols - 2))
		fail_setup("SD root does not fit", root);
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		fail_setup("SDL_Init", SDL_GetError());
	p.screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 32, SDL_HWSURFACE);
	if (p.screen == NULL)
		fail_setup("SDL_SetVideoMode", SDL_GetError());
	SDL_ShowCursor(SDL_DISABLE);
	SDL_EnableKeyRepeat(300, 50);
	p.offscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, p.screen->w, p.screen->h, 32, 0x00FF0000, 0x0000FF00,
					   0x000000FF, 0);
	if (p.offscreen == NULL)
		fail_setup("SDL_CreateRGBSurface", SDL_GetError());
	if (TTF_Init() != 0)
		fail_setup("TTF_Init", TTF_GetError());
	st = sdl_view_open(&view, opt.font, p.offscreen->w, p.offscreen->h, grid.cols, grid.lines);
	if (st == SDLVIEW_FONT_OPEN)
		fail_setup(opt.font, TTF_GetError());
	if (st != SDLVIEW_OK)
		fail_setup(opt.font, "no font size fits the screen's cells");
	p.view = &view;
	result = sdl_loop_run(&shell.app, &grid, present, &p, opt.frames);
	if (result == SDLLOOP_ERROR)
		fail_setup("SDL_WaitEvent", SDL_GetError());
	if (opt.shot != NULL && SDL_SaveBMP(p.offscreen, opt.shot) != 0)
		fail_setup(opt.shot, SDL_GetError());
	sdl_view_close(&view);
	SDL_FreeSurface(p.offscreen);
	TTF_Quit();
	SDL_Quit();
	return 0;
}
