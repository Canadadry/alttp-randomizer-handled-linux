#include "ui_generate.h"
#include "ui_settings.h"

#include <assert.h>

void ui_generate_init_defaults(UiGenerateInit *init, const UiStrTable *strings, int page)
{
	int id;

	init->strings = strings;
	init->page = page;
	rando_options_init(&init->options);
	init->heart_beep = RANDO_HEART_BEEP_HALF;
	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		rando_setting_set(&init->options, &init->heart_beep, (RandoSettingId)id,
				  rando_setting_default((RandoSettingId)id));
}

void ui_generate_init(UiGenerate *state, const UiGenerateInit *init)
{
	assert(init->page >= 1);
	state->strings = init->strings;
	state->page = init->page;
	state->options = init->options;
	state->heart_beep = init->heart_beep;
	state->spoiler = false;
	state->cursor = 0;
	state->scroll = 0;
}

static void move_to(UiGenerate *state, int cursor)
{
	state->cursor = cursor;
	state->scroll = ui_scroll_follow(state->scroll, cursor, UI_GENERATE_ROW_COUNT, state->page);
}

static int wrap(int value, int count)
{
	return (value % count + count) % count;
}

static void step_value(UiGenerate *state, int delta)
{
	RandoSettingId id = (RandoSettingId)state->cursor;

	if (state->cursor == UI_GENERATE_ROW_SPOILER) {
		state->spoiler = !state->spoiler;
		return;
	}
	rando_setting_set(&state->options, &state->heart_beep, id,
			  wrap(rando_setting_get(&state->options, state->heart_beep, id) + delta,
			       rando_setting_value_count(id)));
}

static void reset_row(UiGenerate *state)
{
	if (state->cursor == UI_GENERATE_ROW_SPOILER)
		state->spoiler = false;
	else
		rando_setting_set(&state->options, &state->heart_beep, (RandoSettingId)state->cursor,
				  rando_setting_default((RandoSettingId)state->cursor));
}

static int clamp_row(int row)
{
	return row < 0 ? 0 : row >= UI_GENERATE_ROW_COUNT ? UI_GENERATE_ROW_COUNT - 1 : row;
}

UiGenerateEvent ui_generate_key(UiGenerate *state, UiButton button)
{
	bool on_action = state->cursor == UI_GENERATE_ROW_ACTION;

	switch (button) {
	case UI_BUTTON_UP: move_to(state, wrap(state->cursor - 1, UI_GENERATE_ROW_COUNT)); break;
	case UI_BUTTON_DOWN: move_to(state, wrap(state->cursor + 1, UI_GENERATE_ROW_COUNT)); break;
	case UI_BUTTON_L2: move_to(state, clamp_row(state->cursor - state->page)); break;
	case UI_BUTTON_R2: move_to(state, clamp_row(state->cursor + state->page)); break;
	case UI_BUTTON_LEFT:
		if (!on_action)
			step_value(state, -1);
		break;
	case UI_BUTTON_RIGHT:
		if (!on_action)
			step_value(state, 1);
		break;
	case UI_BUTTON_A:
		if (on_action)
			return UI_GENERATE_GENERATE;
		step_value(state, 1);
		break;
	case UI_BUTTON_Y:
		if (!on_action)
			reset_row(state);
		break;
	case UI_BUTTON_START: return UI_GENERATE_GENERATE;
	case UI_BUTTON_B:
	case UI_BUTTON_MENU: return UI_GENERATE_QUIT;
	case UI_BUTTON_L1: return UI_GENERATE_TAB_PREV;
	case UI_BUTTON_R1: return UI_GENERATE_TAB_NEXT;
	case UI_BUTTON_X:
	case UI_BUTTON_SELECT:
	case UI_BUTTON_COUNT: break;
	}
	return UI_GENERATE_NONE;
}

void ui_generate_screen(const UiGenerate *state, UiScreen *out)
{
	const UiStrTable *t = state->strings;
	int id;

	ui_screen_init(out);
	out->tabs[0] = ui_str(t, UI_STR_TAB_GENERATE);
	out->tabs[1] = ui_str(t, UI_STR_TAB_HISTORY);
	out->tab_count = 2;
	out->active_tab = 0;
	out->title = ui_str(t, UI_STR_GENERATE_TITLE);
	for (id = 0; id < RANDO_SETTING_COUNT; id++) {
		UiRow row = {UI_ROW_SETTING, NULL, NULL, false};

		row.label = ui_str(t, ui_setting_label((RandoSettingId)id));
		row.value = ui_str(t, ui_setting_value((RandoSettingId)id,
						       rando_setting_get(&state->options, state->heart_beep,
									 (RandoSettingId)id)));
		ui_screen_add_row(out, row);
	}
	{
		UiRow spoiler = {UI_ROW_SETTING, NULL, NULL, false};
		UiRow action = {UI_ROW_ACTION, NULL, NULL, false};

		spoiler.label = ui_str(t, UI_STR_SETTING_SPOILER);
		spoiler.value = ui_str(t, state->spoiler ? UI_STR_SPOILER_ON : UI_STR_SPOILER_OFF);
		action.label = ui_str(t, UI_STR_GENERATE_ACTION);
		ui_screen_add_row(out, spoiler);
		ui_screen_add_row(out, action);
	}
	out->cursor = state->cursor;
	out->scroll = state->scroll;
	out->more_above = state->scroll > 0;
	out->more_below = state->scroll + state->page < UI_GENERATE_ROW_COUNT;
	out->footer = ui_str(t, UI_STR_GENERATE_FOOTER);
}

void ui_generate_settings(const UiGenerate *state, RandoOptions *options, RandoHeartBeep *heart_beep, bool *spoiler)
{
	*options = state->options;
	*heart_beep = state->heart_beep;
	*spoiler = state->spoiler;
}
