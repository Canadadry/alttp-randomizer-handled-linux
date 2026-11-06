#include "sdl_loop.h"

#include "sdl_keys.h"

#include <SDL/SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct Loop {
	UiApp *app;
	UiGrid *grid;
	SdlPresentFn present;
	void *ctx;
	long max_frames, frames;
	bool failed;
} Loop;

/* True when the run must end: the frame cap reached or the presenter failed. */
static bool frame(Loop *loop)
{
	static UiScreen screen;
	bool ok;

	ui_app_screen(loop->app, &screen);
	ok = ui_grid_render(loop->grid, &screen);
	assert(ok);
	(void)ok;
	loop->failed = !loop->present(loop->ctx, &screen, loop->grid);
	loop->frames++;
	return loop->failed || (loop->max_frames > 0 && loop->frames >= loop->max_frames);
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

typedef enum Step {
	STEP_GO,
	STEP_QUIT,
	STEP_FRAMES
} Step;

static Step button(Loop *loop, const SdlInput *in, bool drain)
{
	bool quit = false;

	if (ui_app_input(loop->app, in->button, in->down) == UI_APP_QUIT)
		return STEP_QUIT;
	if (ui_app_pending(loop->app)) {
		if (frame(loop))
			return STEP_FRAMES;
		ui_app_run(loop->app);
		quit = drain && drain_keeps_quit();
	}
	if (frame(loop))
		return STEP_FRAMES;
	return quit ? STEP_QUIT : STEP_GO;
}

static SdlLoopResult result_of(const Loop *loop, Step step)
{
	if (step == STEP_QUIT)
		return SDLLOOP_QUIT;
	return loop->failed ? SDLLOOP_PRESENT_FAILED : SDLLOOP_FRAMES;
}

static SdlLoopResult scripted(Loop *loop, const SdlScript *script)
{
	UiButton b;

	while (script->next(script->ctx, &b)) {
		SDL_Event event;
		SdlInput in;
		SDLKey key;
		bool ok;
		int i;

		ok = sdl_keys_key(b, &key);
		assert(ok);
		for (i = 0; i < 2; i++) {
			Step step;

			memset(&event, 0, sizeof(event));
			event.type = i == 0 ? SDL_KEYDOWN : SDL_KEYUP;
			event.key.type = event.type;
			event.key.state = i == 0 ? SDL_PRESSED : SDL_RELEASED;
			event.key.keysym.sym = key;
			ok = sdl_keys_event(&event, &in) == SDLKEYS_BUTTON && in.button == b;
			assert(ok);
			step = button(loop, &in, false);
			if (step != STEP_GO)
				return result_of(loop, step);
		}
		(void)ok;
	}
	return SDLLOOP_SCRIPT_END;
}

SdlLoopResult sdl_loop_run(UiApp *app, UiGrid *grid, SdlPresentFn present, void *present_ctx, long max_frames,
			   const SdlScript *script)
{
	Loop loop;

	assert(app != NULL && grid != NULL && present != NULL && max_frames >= 0);
	assert(script == NULL || script->next != NULL);
	loop.app = app;
	loop.grid = grid;
	loop.present = present;
	loop.ctx = present_ctx;
	loop.max_frames = max_frames;
	loop.frames = 0;
	loop.failed = false;
	if (frame(&loop))
		return result_of(&loop, STEP_FRAMES);
	if (script != NULL)
		return scripted(&loop, script);
	for (;;) {
		SDL_Event event;
		SdlInput in;
		Step step;

		if (!SDL_WaitEvent(&event))
			return SDLLOOP_ERROR;
		switch (sdl_keys_event(&event, &in)) {
		case SDLKEYS_QUIT:
			return SDLLOOP_QUIT;
		case SDLKEYS_BUTTON:
			step = button(&loop, &in, true);
			if (step != STEP_GO)
				return result_of(&loop, step);
			break;
		case SDLKEYS_NONE:
			if (event.type == SDL_VIDEOEXPOSE && frame(&loop))
				return result_of(&loop, STEP_FRAMES);
			break;
		}
	}
}
