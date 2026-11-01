#ifndef SDL_LOOP_H
#define SDL_LOOP_H

#include "ui_app.h"
#include "ui_grid.h"

typedef void (*SdlPresentFn)(void *ctx, const UiGrid *grid);

typedef enum SdlLoopResult {
	SDLLOOP_QUIT,
	SDLLOOP_FRAMES,
	SDLLOOP_ERROR
} SdlLoopResult;

SdlLoopResult sdl_loop_run(UiApp *app, UiGrid *grid, SdlPresentFn present, void *present_ctx, long max_frames);

#endif
