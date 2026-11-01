#include "sdl_keys.h"
#include "support.h"

#include <string.h>

#define TEST "test_sdl_keys"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef struct KeyRow {
	SDLKey key;
	UiButton button;
} KeyRow;

/* docs/onion.md section 5, one row per button. */
static const KeyRow device[] = {
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
};

static const KeyRow aliases[] = {
	{ SDLK_a, UI_BUTTON_A },
	{ SDLK_b, UI_BUTTON_B },
	{ SDLK_x, UI_BUTTON_X },
	{ SDLK_y, UI_BUTTON_Y },
	{ SDLK_PAGEUP, UI_BUTTON_L2 },
	{ SDLK_PAGEDOWN, UI_BUTTON_R2 },
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void check_rows(const KeyRow *rows, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		UiButton out = UI_BUTTON_COUNT;

		if (!sdl_keys_button(rows[i].key, &out) || out != rows[i].button)
			test_fail(TEST, "key %d: expected button %d, got %d", (int)rows[i].key, (int)rows[i].button,
				  (int)out);
	}
}

static void test_device_keys(void)
{
	bool reached[UI_BUTTON_COUNT];
	int i;

	check_rows(device, COUNT(device));
	memset(reached, 0, sizeof(reached));
	for (i = 0; i < COUNT(device); i++)
		reached[device[i].button] = true;
	for (i = 0; i < UI_BUTTON_COUNT; i++)
		if (!reached[i])
			test_fail(TEST, "button %d is reached by no device key", i);
}

static void test_aliases(void)
{
	int i, j;

	check_rows(aliases, COUNT(aliases));
	for (i = 0; i < COUNT(aliases); i++)
		for (j = 0; j < COUNT(device); j++)
			if (aliases[i].key == device[j].key)
				test_fail(TEST, "alias key %d is also a device key", (int)aliases[i].key);
}

static void test_unmapped(void)
{
	UiButton out = UI_BUTTON_COUNT;

	CHECK(!sdl_keys_button(SDLK_z, &out));
	CHECK(!sdl_keys_button(SDLK_UNKNOWN, &out));
	CHECK(!sdl_keys_button(SDLK_FIRST, &out));
	CHECK(!sdl_keys_button(SDLK_RSHIFT, &out));
	CHECK(!sdl_keys_button(SDLK_LAST, &out));
	CHECK(out == UI_BUTTON_COUNT);
}

static SdlInputKind key_event(Uint8 type, SDLKey key, SdlInput *in)
{
	SDL_Event e;

	memset(&e, 0, sizeof(e));
	e.type = type;
	e.key.type = type;
	e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
	e.key.keysym.sym = key;
	return sdl_keys_event(&e, in);
}

static void test_events(void)
{
	SdlInput in;
	SDL_Event e;

	CHECK(key_event(SDL_KEYDOWN, SDLK_RETURN, &in) == SDLKEYS_BUTTON);
	CHECK(in.kind == SDLKEYS_BUTTON && in.button == UI_BUTTON_START && in.down);
	CHECK(key_event(SDL_KEYUP, SDLK_a, &in) == SDLKEYS_BUTTON);
	CHECK(in.kind == SDLKEYS_BUTTON && in.button == UI_BUTTON_A && !in.down);
	CHECK(key_event(SDL_KEYDOWN, SDLK_z, &in) == SDLKEYS_NONE && in.kind == SDLKEYS_NONE);
	CHECK(key_event(SDL_KEYUP, SDLK_FIRST, &in) == SDLKEYS_NONE);

	memset(&e, 0, sizeof(e));
	e.type = SDL_QUIT;
	CHECK(sdl_keys_event(&e, &in) == SDLKEYS_QUIT && in.kind == SDLKEYS_QUIT);

	memset(&e, 0, sizeof(e));
	e.type = SDL_ACTIVEEVENT;
	e.active.gain = 1;
	e.active.state = SDL_APPINPUTFOCUS;
	CHECK(sdl_keys_event(&e, &in) == SDLKEYS_NONE && in.kind == SDLKEYS_NONE);

	memset(&e, 0, sizeof(e));
	e.type = SDL_MOUSEBUTTONDOWN;
	CHECK(sdl_keys_event(&e, &in) == SDLKEYS_NONE);
	e.type = SDL_VIDEOEXPOSE;
	CHECK(sdl_keys_event(&e, &in) == SDLKEYS_NONE);
}

int main(void)
{
	test_device_keys();
	test_aliases();
	test_unmapped();
	test_events();
	printf("%s: passed\n", TEST);
	return 0;
}
