#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <stdbool.h>

typedef enum UiButton {
	UI_BUTTON_UP,
	UI_BUTTON_DOWN,
	UI_BUTTON_LEFT,
	UI_BUTTON_RIGHT,
	UI_BUTTON_A,
	UI_BUTTON_B,
	UI_BUTTON_X,
	UI_BUTTON_Y,
	UI_BUTTON_L1,
	UI_BUTTON_R1,
	UI_BUTTON_L2,
	UI_BUTTON_R2,
	UI_BUTTON_SELECT,
	UI_BUTTON_START,
	UI_BUTTON_MENU,
	UI_BUTTON_COUNT
} UiButton;

typedef struct UiInput {
	bool held[UI_BUTTON_COUNT];
	bool used[UI_BUTTON_COUNT];
} UiInput;

void ui_input_init(UiInput *input);

bool ui_input_event(UiInput *input, UiButton button, bool down, UiButton *out);

#endif
