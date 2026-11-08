#include "ui_ladx_generate.h"
#include "ui_ladx_settings.h"
#include "ui_settings.h"

#include <assert.h>

void ui_ladx_generate_init_defaults(UiLadxGenerateInit *init, const UiStrTable *strings, int page)
{
	init->strings = strings;
	init->page = page;
	ladx_settings_defaults(&init->settings);
	init->settings_unreadable = false;
}

static bool is_heading(int item)
{
	return item >= UI_LADX_ITEM_HEADING;
}

static void layout(UiLadxGenerate *state)
{
	int category, id;

	state->row_count = 0;
	for (category = 0; category < LADX_CATEGORY_COUNT; category++) {
		state->rows[state->row_count++] = UI_LADX_ITEM_HEADING + category;
		for (id = 0; id < LADX_SETTING_COUNT; id++)
			if ((int)ladx_setting_category((LadxSettingId)id) == category && ui_ladx_setting_shown(&state->settings, (LadxSettingId)id))
				state->rows[state->row_count++] = id;
	}
	state->rows[state->row_count++] = UI_LADX_ITEM_SPOILER;
	state->rows[state->row_count++] = UI_LADX_ITEM_ACTION;
	assert(state->row_count <= UI_LADX_ROWS_MAX);
}

static int index_of(const UiLadxGenerate *state, int item)
{
	int i;

	for (i = 0; i < state->row_count && state->rows[i] != item; i++)
		;
	assert(i < state->row_count);
	return i;
}

static bool pair_holds(const UiLadxGenerate *state, const LadxSettingPair *pair)
{
	return state->settings.value[pair->a] == pair->value_a && state->settings.value[pair->b] == pair->value_b;
}

/* The pair holding on the shown values that involves id; NULL when none. */
static const LadxSettingPair *holding_pair(const UiLadxGenerate *state, LadxSettingId id)
{
	int i;

	for (i = 0; i < ladx_setting_pair_count(); i++) {
		const LadxSettingPair *pair = ladx_setting_pair(i);

		if (pair_holds(state, pair) && (pair->a == id || pair->b == id))
			return pair;
	}
	return NULL;
}

static void fill_finding(const UiLadxGenerate *state, const LadxSettingPair *finding, LadxSettingId other,
			 char buf[UI_GENERATE_REASON_MAX])
{
	const char *label = other == LADX_SETTING_COUNT ? NULL : ui_str(state->strings, ui_ladx_setting_label(other));

	ui_str_fill(ui_str(state->strings, ui_ladx_reason(finding->reason)), label, buf, UI_GENERATE_REASON_MAX);
}

static bool action_finding(const UiLadxGenerate *state, LadxSettingPair *finding)
{
	LadxSettings validated = state->settings;

	ladx_settings_validate(&validated);
	return !ladx_settings_supported(&validated, finding);
}

static bool row_disabled(const UiLadxGenerate *state, LadxSettingId id)
{
	return ladx_setting_value_reason(id, state->settings.value[id]) != LADX_REASON_NONE ||
	       holding_pair(state, id) != NULL;
}

static void fill_reason(const UiLadxGenerate *state, int item, char buf[UI_GENERATE_REASON_MAX])
{
	LadxSettingPair finding;
	const LadxSettingPair *pair;
	LadxReason single;

	buf[0] = '\0';
	if (item == UI_LADX_ITEM_SPOILER || is_heading(item))
		return;
	if (item == UI_LADX_ITEM_ACTION) {
		if (action_finding(state, &finding))
			fill_finding(state, &finding, finding.b, buf);
		return;
	}
	single = ladx_setting_value_reason((LadxSettingId)item, state->settings.value[item]);
	if (single != LADX_REASON_NONE) {
		finding.reason = single;
		fill_finding(state, &finding, LADX_SETTING_COUNT, buf);
		return;
	}
	pair = holding_pair(state, (LadxSettingId)item);
	if (pair != NULL)
		fill_finding(state, pair, pair->a == (LadxSettingId)item ? pair->b : pair->a, buf);
}

static void follow(UiLadxGenerate *state)
{
	int index = index_of(state, state->item);

	if (index > 0 && is_heading(state->rows[index - 1]))
		state->scroll = ui_scroll_follow(state->scroll, index - 1, state->row_count, state->page);
	state->scroll = ui_scroll_follow(state->scroll, index, state->row_count, state->page);
}

void ui_ladx_generate_init(UiLadxGenerate *state, const UiLadxGenerateInit *init)
{
	int id;

	assert(init->page >= 1);
	for (id = 0; id < LADX_SETTING_COUNT; id++)
		assert(init->settings.value[id] < ladx_setting_value_count((LadxSettingId)id));
	state->strings = init->strings;
	state->page = init->page;
	state->settings = init->settings;
	state->spoiler = false;
	state->scroll = 0;
	layout(state);
	state->item = state->rows[1];
	follow(state);
	state->message_kind = init->settings_unreadable ? UI_MESSAGE_INFO : UI_MESSAGE_NONE;
	state->message = init->settings_unreadable ? UI_STR_GENERATE_SETTINGS_UNREADABLE : UI_STR_NONE;
	state->refusal[0] = '\0';
	fill_reason(state, state->item, state->reason);
}

static int wrap(int value, int count)
{
	return (value % count + count) % count;
}

static void move(UiLadxGenerate *state, int delta)
{
	int index = index_of(state, state->item);

	do
		index = wrap(index + delta, state->row_count);
	while (is_heading(state->rows[index]));
	state->item = state->rows[index];
}

/* The first setting shown under category; categories always show one. */
static int first_of(const UiLadxGenerate *state, int category)
{
	int index = index_of(state, UI_LADX_ITEM_HEADING + category);

	assert(!is_heading(state->rows[index + 1]) && state->rows[index + 1] < LADX_SETTING_COUNT);
	return state->rows[index + 1];
}

static void jump(UiLadxGenerate *state, int delta)
{
	int item = state->item;

	if (item >= LADX_SETTING_COUNT) {
		if (delta < 0)
			state->item = first_of(state, LADX_CATEGORY_COUNT - 1);
		else
			state->item = UI_LADX_ITEM_ACTION;
		return;
	}
	if (delta > 0)
		state->item = (int)ladx_setting_category((LadxSettingId)item) + 1 < LADX_CATEGORY_COUNT
				      ? first_of(state, (int)ladx_setting_category((LadxSettingId)item) + 1)
				      : UI_LADX_ITEM_SPOILER;
	else
		state->item = first_of(state, ladx_setting_category((LadxSettingId)item) > 0
						      ? (int)ladx_setting_category((LadxSettingId)item) - 1
						      : 0);
}

static void step_value(UiLadxGenerate *state, int delta)
{
	LadxSettingId id = (LadxSettingId)state->item;

	if (state->item == UI_LADX_ITEM_SPOILER) {
		state->spoiler = !state->spoiler;
		return;
	}
	state->settings.value[id] =
		(uint8_t)wrap(state->settings.value[id] + delta, ladx_setting_value_count(id));
}

static void reset_row(UiLadxGenerate *state)
{
	if (state->item == UI_LADX_ITEM_SPOILER)
		state->spoiler = false;
	else
		state->settings.value[state->item] = (uint8_t)ladx_setting_default((LadxSettingId)state->item);
}

static UiGenerateEvent generate(UiLadxGenerate *state)
{
	fill_reason(state, UI_LADX_ITEM_ACTION, state->refusal);
	if (state->refusal[0] == '\0')
		return UI_GENERATE_GENERATE;
	state->message_kind = UI_MESSAGE_ERROR;
	state->message = UI_STR_GENERATE_UNSUPPORTED;
	return UI_GENERATE_NONE;
}

static UiGenerateEvent act(UiLadxGenerate *state, UiButton button)
{
	bool on_action = state->item == UI_LADX_ITEM_ACTION;

	switch (button) {
	case UI_BUTTON_UP: move(state, -1); break;
	case UI_BUTTON_DOWN: move(state, 1); break;
	case UI_BUTTON_L2: jump(state, -1); break;
	case UI_BUTTON_R2: jump(state, 1); break;
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

UiGenerateEvent ui_ladx_generate_key(UiLadxGenerate *state, UiButton button)
{
	UiGenerateEvent event;

	state->message_kind = UI_MESSAGE_NONE;
	state->message = UI_STR_NONE;
	event = act(state, button);
	layout(state);
	follow(state);
	fill_reason(state, state->item, state->reason);
	return event;
}

static UiRow row_of(const UiLadxGenerate *state, int item)
{
	const UiStrTable *t = state->strings;
	UiRow row = {UI_ROW_SETTING, NULL, NULL, false, false};

	if (is_heading(item)) {
		row.kind = UI_ROW_TEXT;
		row.emphasis = true;
		row.label = ui_str(t, ui_ladx_category_label((LadxSettingCategory)(item - UI_LADX_ITEM_HEADING)));
	} else if (item == UI_LADX_ITEM_SPOILER) {
		row.label = ui_str(t, UI_STR_SETTING_SPOILER);
		row.value = ui_str(t, state->spoiler ? UI_STR_SPOILER_ON : UI_STR_SPOILER_OFF);
	} else if (item == UI_LADX_ITEM_ACTION) {
		LadxSettingPair finding;

		row.kind = UI_ROW_ACTION;
		row.label = ui_str(t, UI_STR_GENERATE_ACTION);
		row.disabled = action_finding(state, &finding);
	} else {
		LadxSettingId id = (LadxSettingId)item;

		row.label = ui_str(t, ui_ladx_setting_label(id));
		row.value = ui_str(t, ui_ladx_setting_value(id, state->settings.value[id]));
		row.disabled = row_disabled(state, id);
	}
	return row;
}

void ui_ladx_generate_screen(const UiLadxGenerate *state, UiScreen *out)
{
	const UiStrTable *t = state->strings;
	int i;

	ui_screen_init(out);
	out->tabs[0] = ui_str(t, UI_STR_TAB_GENERATE);
	out->tabs[1] = ui_str(t, UI_STR_TAB_HISTORY);
	out->tab_count = 2;
	out->active_tab = 0;
	out->title = ui_str(t, UI_STR_GAME_LADX);
	for (i = 0; i < state->row_count; i++)
		ui_screen_add_row(out, row_of(state, state->rows[i]));
	out->cursor = index_of(state, state->item);
	out->scroll = state->scroll;
	out->more_above = state->scroll > 0;
	out->more_below = state->scroll + state->page < state->row_count;
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_LEFT_RIGHT, UI_STR_HINT_CHANGE, false));
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_Y, UI_STR_HINT_DEFAULT, false));
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_START, UI_STR_HINT_GENERATE, true));
	if (state->message_kind != UI_MESSAGE_NONE) {
		out->message_kind = state->message_kind;
		out->message = state->message == UI_STR_GENERATE_UNSUPPORTED ? state->refusal : ui_str(t, state->message);
	} else if (state->reason[0] != '\0') {
		out->message_kind = UI_MESSAGE_INFO;
		out->message = state->reason;
	} else if (state->item == UI_LADX_ITEM_SPOILER) {
		out->message_kind = UI_MESSAGE_HELP;
		out->message = ui_str(t, ui_spoiler_help(state->spoiler));
	} else if (state->item < LADX_SETTING_COUNT) {
		out->message_kind = UI_MESSAGE_HELP;
		out->message = ui_str(t, ui_ladx_setting_help((LadxSettingId)state->item));
	}
}

void ui_ladx_generate_settings(const UiLadxGenerate *state, LadxSettings *settings, bool *spoiler)
{
	*settings = state->settings;
	*spoiler = state->spoiler;
}
