#include "sdl_loop.h"

#include "sdl_keys.h"

#include <SDL/SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct Loop {
	UiApp *app;
	UiGrid *grid;
	SdlPresentFn present;
	void *ctx;
	long max_frames, frames;
} Loop;

static bool frame(Loop *loop)
{
	static UiScreen screen;
	bool ok;

	ui_app_screen(loop->app, &screen);
	ok = ui_grid_render(loop->grid, &screen);
	assert(ok);
	(void)ok;
	loop->present(loop->ctx, loop->grid);
	loop->frames++;
	return loop->max_frames > 0 && loop->frames >= loop->max_frames;
}

/* Keys pressed during the blocking run would act on the result screen: drop them, keep a quit. */
static bool drain_keeps_quit(void)
{
	SDL_Event event;
	SdlInput in;
	bool quit = false;

	SDL_PumpEvents();
	while (SDL_PollEvent(&event))
		quit = sdl_keys_event(&event, &in) == SDLKEYS_QUIT || quit;
	return quit;
}

SdlLoopResult sdl_loop_run(UiApp *app, UiGrid *grid, SdlPresentFn present, void *present_ctx, long max_frames)
{
	Loop loop;

	assert(app != NULL && grid != NULL && present != NULL && max_frames >= 0);
	loop.app = app;
	loop.grid = grid;
	loop.present = present;
	loop.ctx = present_ctx;
	loop.max_frames = max_frames;
	loop.frames = 0;
	if (frame(&loop))
		return SDLLOOP_FRAMES;
	for (;;) {
		SDL_Event event;
		SdlInput in;
		bool quit = false;

		if (!SDL_WaitEvent(&event))
			return SDLLOOP_ERROR;
		switch (sdl_keys_event(&event, &in)) {
		case SDLKEYS_QUIT:
			return SDLLOOP_QUIT;
		case SDLKEYS_BUTTON:
			if (ui_app_input(app, in.button, in.down) == UI_APP_QUIT)
				return SDLLOOP_QUIT;
			if (ui_app_pending(app)) {
				if (frame(&loop))
					return SDLLOOP_FRAMES;
				ui_app_run(app);
				quit = drain_keeps_quit();
			}
			break;
		case SDLKEYS_NONE:
			if (event.type != SDL_VIDEOEXPOSE)
				continue;
			break;
		}
		if (frame(&loop))
			return SDLLOOP_FRAMES;
		if (quit)
			return SDLLOOP_QUIT;
	}
}
