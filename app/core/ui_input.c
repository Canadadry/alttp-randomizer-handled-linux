#include "ui_input.h"

#include <assert.h>
#include <string.h>

static bool is_starter(UiButton b)
{
	return b == UI_BUTTON_START || b == UI_BUTTON_SELECT || b == UI_BUTTON_MENU;
}

void ui_input_init(UiInput *input)
{
	memset(input, 0, sizeof *input);
}

static bool use_held_starters(UiInput *input, UiButton except)
{
	bool any = false;
	int b;

	for (b = 0; b < UI_BUTTON_COUNT; b++)
		if (b != (int)except && input->held[b]) {
			input->used[b] = true;
			any = true;
		}
	return any;
}

bool ui_input_event(UiInput *input, UiButton button, bool down, UiButton *out)
{
	assert((unsigned)button < UI_BUTTON_COUNT);
	if (is_starter(button)) {
		if (down) {
			if (!input->held[button]) {
				input->held[button] = true;
				input->used[button] = use_held_starters(input, button);
			}
			return false;
		}
		if (!input->held[button])
			return false;
		input->held[button] = false;
		if (input->used[button]) {
			input->used[button] = false;
			return false;
		}
		*out = button;
		return true;
	}
	if (!down || use_held_starters(input, UI_BUTTON_COUNT))
		return false;
	*out = button;
	return true;
}
