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
	init->settings_unreadable = false;
	init->unsupported_count = rando_setting_unsupported_count();
	init->unsupported = init->unsupported_count > 0 ? rando_setting_unsupported(0) : NULL;
}

static bool pair_holds(const UiGenerate *state, const RandoSettingPair *pair)
{
	return rando_setting_get(&state->options, state->heart_beep, pair->a) == pair->value_a &&
	       rando_setting_get(&state->options, state->heart_beep, pair->b) == pair->value_b;
}

static const RandoSettingPair *holding_pair(const UiGenerate *state, int row)
{
	int i;

	for (i = 0; i < state->unsupported_count; i++) {
		const RandoSettingPair *pair = &state->unsupported[i];

		if (pair_holds(state, pair) &&
		    (row == UI_GENERATE_ROW_ACTION || (int)pair->a == row || (int)pair->b == row))
			return pair;
	}
	return NULL;
}

static void fill_reason(const UiGenerate *state, int row, char buf[UI_GENERATE_REASON_MAX])
{
	const RandoSettingPair *pair = row == UI_GENERATE_ROW_SPOILER ? NULL : holding_pair(state, row);
	RandoSettingId other;

	buf[0] = '\0';
	if (pair == NULL)
		return;
	other = row == UI_GENERATE_ROW_ACTION || (int)pair->a == row ? pair->b : pair->a;
	ui_str_fill(ui_str(state->strings, UI_STR_GENERATE_UNSUPPORTED), ui_str(state->strings, ui_setting_label(other)),
		    buf, UI_GENERATE_REASON_MAX);
}

static UiGenerateEvent generate(UiGenerate *state)
{
	fill_reason(state, UI_GENERATE_ROW_ACTION, state->refusal);
	if (state->refusal[0] == '\0')
		return UI_GENERATE_GENERATE;
	state->message_kind = UI_MESSAGE_ERROR;
	state->message = UI_STR_GENERATE_UNSUPPORTED;
	return UI_GENERATE_NONE;
}

void ui_generate_init(UiGenerate *state, const UiGenerateInit *init)
{
	int i;

	assert(init->page >= 1);
	assert(init->unsupported_count >= 0 && (init->unsupported_count == 0 || init->unsupported != NULL));
	for (i = 0; i < init->unsupported_count; i++) {
		const RandoSettingPair *pair = &init->unsupported[i];

		assert(pair->a < RANDO_SETTING_COUNT && pair->b < RANDO_SETTING_COUNT && pair->a != pair->b);
		assert(pair->value_a >= 0 && pair->value_a < rando_setting_value_count(pair->a));
		assert(pair->value_b >= 0 && pair->value_b < rando_setting_value_count(pair->b));
		(void)pair;
	}
	state->strings = init->strings;
	state->page = init->page;
	state->options = init->options;
	state->heart_beep = init->heart_beep;
	state->spoiler = false;
	state->cursor = 0;
	state->scroll = 0;
	state->unsupported = init->unsupported;
	state->unsupported_count = init->unsupported_count;
	state->message_kind = init->settings_unreadable ? UI_MESSAGE_INFO : UI_MESSAGE_NONE;
	state->message = init->settings_unreadable ? UI_STR_GENERATE_SETTINGS_UNREADABLE : UI_STR_NONE;
	state->refusal[0] = '\0';
	fill_reason(state, state->cursor, state->reason);
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

static UiGenerateEvent act(UiGenerate *state, UiButton button);

UiGenerateEvent ui_generate_key(UiGenerate *state, UiButton button)
{
	UiGenerateEvent event;

	state->message_kind = UI_MESSAGE_NONE;
	state->message = UI_STR_NONE;
	event = act(state, button);
	fill_reason(state, state->cursor, state->reason);
	return event;
}

static UiGenerateEvent act(UiGenerate *state, UiButton button)
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
			return generate(state);
		step_value(state, 1);
		break;
	case UI_BUTTON_Y:
		if (!on_action)
			reset_row(state);
		break;
	case UI_BUTTON_START: return generate(state);
	case UI_BUTTON_B: return UI_GENERATE_BACK;
	case UI_BUTTON_MENU: return UI_GENERATE_QUIT;
	case UI_BUTTON_L1: return UI_GENERATE_TAB_PREV;
	case UI_BUTTON_R1: return UI_GENERATE_TAB_NEXT;
	case UI_BUTTON_X:
	case UI_BUTTON_SELECT:
	case UI_BUTTON_COUNT: break;
	}
	return UI_GENERATE_NONE;
}

static UiStrId selected_help(const UiGenerate *state)
{
	RandoSettingId id = (RandoSettingId)state->cursor;

	assert(state->cursor != UI_GENERATE_ROW_ACTION);
	if (state->cursor == UI_GENERATE_ROW_SPOILER)
		return ui_spoiler_help(state->spoiler);
	return ui_setting_help(id, rando_setting_get(&state->options, state->heart_beep, id));
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
		UiRow row = {UI_ROW_SETTING, NULL, NULL, false, false};

		row.label = ui_str(t, ui_setting_label((RandoSettingId)id));
		row.disabled = holding_pair(state, id) != NULL;
		row.value = ui_str(t, ui_setting_value((RandoSettingId)id,
						       rando_setting_get(&state->options, state->heart_beep,
									 (RandoSettingId)id)));
		ui_screen_add_row(out, row);
	}
	{
		UiRow spoiler = {UI_ROW_SETTING, NULL, NULL, false, false};
		UiRow action = {UI_ROW_ACTION, NULL, NULL, false, false};

		spoiler.label = ui_str(t, UI_STR_SETTING_SPOILER);
		spoiler.value = ui_str(t, state->spoiler ? UI_STR_SPOILER_ON : UI_STR_SPOILER_OFF);
		action.label = ui_str(t, UI_STR_GENERATE_ACTION);
		action.disabled = holding_pair(state, UI_GENERATE_ROW_ACTION) != NULL;
		ui_screen_add_row(out, spoiler);
		ui_screen_add_row(out, action);
	}
	out->cursor = state->cursor;
	out->scroll = state->scroll;
	out->more_above = state->scroll > 0;
	out->more_below = state->scroll + state->page < UI_GENERATE_ROW_COUNT;
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_LEFT_RIGHT, UI_STR_HINT_CHANGE, false));
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_Y, UI_STR_HINT_DEFAULT, false));
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_START, UI_STR_HINT_GENERATE, true));
	if (state->message_kind != UI_MESSAGE_NONE) {
		out->message_kind = state->message_kind;
		out->message = state->message == UI_STR_GENERATE_UNSUPPORTED ? state->refusal : ui_str(t, state->message);
	} else if (state->reason[0] != '\0') {
		out->message_kind = UI_MESSAGE_INFO;
		out->message = state->reason;
	} else if (state->cursor != UI_GENERATE_ROW_ACTION) {
		out->message_kind = UI_MESSAGE_HELP;
		out->message = ui_str(t, selected_help(state));
	}
}

void ui_generate_settings(const UiGenerate *state, RandoOptions *options, RandoHeartBeep *heart_beep, bool *spoiler)
{
	*options = state->options;
	*heart_beep = state->heart_beep;
	*spoiler = state->spoiler;
}
