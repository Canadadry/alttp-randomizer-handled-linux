#include "sdl_keys.h"

#include <assert.h>
#include <stddef.h>

typedef struct KeyMap {
	SDLKey key;
	UiButton button;
} KeyMap;

/* Device keys first, so the first row of a button is its device key. */
static const KeyMap keys[] = {
	{ SDLK_UP, UI_BUTTON_UP },
	{ SDLK_DOWN, UI_BUTTON_DOWN },
	{ SDLK_LEFT, UI_BUTTON_LEFT },
	{ SDLK_RIGHT, UI_BUTTON_RIGHT },
	{ SDLK_SPACE, UI_BUTTON_A },
	{ SDLK_LCTRL, UI_BUTTON_B },
	{ SDLK_LSHIFT, UI_BUTTON_X },
	{ SDLK_LALT, UI_BUTTON_Y },
	{ SDLK_e, UI_BUTTON_L1 },
	{ SDLK_t, UI_BUTTON_R1 },
	{ SDLK_TAB, UI_BUTTON_L2 },
	{ SDLK_BACKSPACE, UI_BUTTON_R2 },
	{ SDLK_RCTRL, UI_BUTTON_SELECT },
	{ SDLK_RETURN, UI_BUTTON_START },
	{ SDLK_ESCAPE, UI_BUTTON_MENU },
	{ SDLK_a, UI_BUTTON_A },
	{ SDLK_b, UI_BUTTON_B },
	{ SDLK_x, UI_BUTTON_X },
	{ SDLK_y, UI_BUTTON_Y },
	{ SDLK_PAGEUP, UI_BUTTON_L2 },
	{ SDLK_PAGEDOWN, UI_BUTTON_R2 },
};

bool sdl_keys_button(SDLKey key, UiButton *out)
{
	size_t i;

	assert(out != NULL);
	for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		if (keys[i].key == key) {
			*out = keys[i].button;
			return true;
		}
	}
	return false;
}

bool sdl_keys_key(UiButton button, SDLKey *out)
{
	size_t i;

	assert(out != NULL);
	for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		if (keys[i].button == button) {
			*out = keys[i].key;
			return true;
		}
	}
	return false;
}

SdlInputKind sdl_keys_event(const SDL_Event *event, SdlInput *out)
{
	assert(event != NULL && out != NULL);
	out->kind = SDLKEYS_NONE;
	switch (event->type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		if (sdl_keys_button(event->key.keysym.sym, &out->button)) {
			out->kind = SDLKEYS_BUTTON;
			out->down = event->type == SDL_KEYDOWN;
		}
		break;
	case SDL_QUIT:
		out->kind = SDLKEYS_QUIT;
		break;
	default:
		break;
	}
	return out->kind;
}
