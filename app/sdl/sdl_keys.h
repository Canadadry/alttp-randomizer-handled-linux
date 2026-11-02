#ifndef SDL_KEYS_H
#define SDL_KEYS_H

#include "ui_input.h"

#include <SDL/SDL.h>
#include <stdbool.h>

typedef enum SdlInputKind {
	SDLKEYS_NONE,
	SDLKEYS_BUTTON,
	SDLKEYS_QUIT
} SdlInputKind;

typedef struct SdlInput {
	SdlInputKind kind;
	UiButton button;
	bool down;
} SdlInput;

bool sdl_keys_button(SDLKey key, UiButton *out);

SdlInputKind sdl_keys_event(const SDL_Event *event, SdlInput *out);

#endif
