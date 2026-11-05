#ifndef SDL_LOOP_H
#define SDL_LOOP_H

#include "ui_app.h"
#include "ui_grid.h"
#include "ui_input.h"

#include <stdbool.h>

typedef void (*SdlPresentFn)(void *ctx, const UiGrid *grid);

/* The next scripted button into out; false once the script is exhausted. */
typedef bool (*SdlScriptFn)(void *ctx, UiButton *out);

typedef struct SdlScript {
	SdlScriptFn next;
	void *ctx;
} SdlScript;

typedef enum SdlLoopResult {
	SDLLOOP_QUIT,
	SDLLOOP_FRAMES,
	SDLLOOP_ERROR,
	SDLLOOP_SCRIPT_END
} SdlLoopResult;

/* script NULL: interactive, from SDL's event queue. Otherwise each button is played as its device
   key's down then up event and SDL's queue is never read. */
SdlLoopResult sdl_loop_run(UiApp *app, UiGrid *grid, SdlPresentFn present, void *present_ctx, long max_frames,
			   const SdlScript *script);

#endif
