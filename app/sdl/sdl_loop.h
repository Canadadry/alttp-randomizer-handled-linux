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

/* Drives app with SDL events until it quits, SDL_QUIT arrives, max_frames frames were presented (0: no
 * limit) or SDL_WaitEvent fails. grid is initialised by the caller, its size kept; each frame is the app's
 * screen rendered on it and handed to present. Key events queued during a generation are dropped. */
SdlLoopResult sdl_loop_run(UiApp *app, UiGrid *grid, SdlPresentFn present, void *present_ctx, long max_frames);

#endif
