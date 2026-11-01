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
	UiButton button; /* set for SDLKEYS_BUTTON only */
	bool down;
} SdlInput;

/* The button a device key (docs/onion.md section 5) or a PC alias stands for; false, leaving *out
 * unchanged, for any other key, Power (SDLK_FIRST) included. */
bool sdl_keys_button(SDLKey key, UiButton *out);

/* Reads one SDL event into *out and returns out->kind: a mapped key's press or release, SDL_QUIT, or
 * none for everything else. Key repeat arrives as further presses. */
SdlInputKind sdl_keys_event(const SDL_Event *event, SdlInput *out);

#endif
