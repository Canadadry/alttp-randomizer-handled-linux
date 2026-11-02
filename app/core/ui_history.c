#include "ui_history.h"
#include "ui_settings.h"

#include <assert.h>
#include <string.h>

#define DETAIL_HEAD (RANDO_SETTING_COUNT + 4)
#define WRAP_MIN 8

static char *put_digits(char *out, long value, int n)
{
	int i;

	for (i = n - 1; i >= 0; i--) {
		out[i] = (char)('0' + value % 10);
		value /= 10;
	}
	return out + n;
}

static void put_decimal(char *buf, unsigned long value)
{
	char digits[16];
	int n = 0;

	do {
		digits[n++] = (char)('0' + value % 10);
		value /= 10;
	} while (value > 0);
	while (n > 0)
		*buf++ = digits[--n];
	*buf = '\0';
}

static void put_date(char buf[17], const UiHistoryDate *d)
{
	char *out = buf;

	out = put_digits(out, d->year, 4);
	*out++ = '-';
	out = put_digits(out, d->month, 2);
	*out++ = '-';
	out = put_digits(out, d->day, 2);
	*out++ = ' ';
	out = put_digits(out, d->hour, 2);
	*out++ = ':';
	out = put_digits(out, d->minute, 2);
	*out = '\0';
}

static void set_message(UiHistory *state, UiMessageKind kind, UiStrId id)
{
	char number[16];

	put_decimal(number, UI_HISTORY_MAX);
	state->message_kind = kind;
	ui_str_fill(ui_str(state->strings, id), number, state->message, sizeof state->message);
}

static void clear_message(UiHistory *state)
{
	state->message_kind = UI_MESSAGE_NONE;
	state->message[0] = '\0';
}

void ui_history_init(UiHistory *state, const UiHistoryInit *init)
{
	const UiHistoryList *list = init->list;

	assert(init->page >= 1 && init->page <= UI_ROWS_MAX);
	assert(init->width >= UI_HISTORY_WIDTH_MIN && init->width <= UI_HISTORY_WIDTH_MAX);
	assert(list != NULL && list->count >= 0 && list->count <= UI_HISTORY_MAX);
	assert(init->spoiler_fn != NULL && init->delete_fn != NULL);
	state->strings = init->strings;
	state->page = init->page;
	state->width = init->width;
	state->list = *list;
	state->view = UI_HISTORY_VIEW_LIST;
	state->confirm_from = UI_HISTORY_VIEW_LIST;
	state->cursor = 0;
	state->scroll = 0;
	state->detail_scroll = 0;
	state->spoiler.len = 0;
	state->spoiler.cut = false;
	state->line_count = 0;
	state->spoiler_state = UI_HISTORY_SPOILER_NONE;
	state->spoiler_fn = init->spoiler_fn;
	state->delete_fn = init->delete_fn;
	state->ctx = init->ctx;
	clear_message(state);
	if (list->folder_unreadable)
		set_message(state, UI_MESSAGE_INFO, UI_STR_HISTORY_NOTICE_FOLDER);
	else if (list->unreadable)
		set_message(state, UI_MESSAGE_INFO, UI_STR_HISTORY_NOTICE_UNREADABLE);
	else if (list->truncated)
		set_message(state, UI_MESSAGE_INFO, UI_STR_HISTORY_NOTICE_TRUNCATED);
}

static bool add_line(UiHistory *state, size_t offset, size_t len, int indent)
{
	UiHistoryLine *line;

	if (state->line_count >= UI_HISTORY_LINES_MAX)
		return false;
	line = &state->lines[state->line_count++];
	line->offset = (uint32_t)offset;
	line->len = (uint16_t)len;
	line->indent = (uint16_t)indent;
	return true;
}

static int row_indent(const UiHistory *state, int indent)
{
	return indent < state->width - WRAP_MIN ? indent : state->width - WRAP_MIN;
}

static bool wrap_line(UiHistory *state, size_t start, size_t end)
{
	const char *text = state->spoiler.text;
	size_t pos = start;
	int indent = 0, rows_indent;

	while (pos < end && text[pos] == ' ') {
		pos++;
		indent++;
	}
	rows_indent = row_indent(state, indent);
	if ((size_t)rows_indent + (end - pos) <= (size_t)state->width)
		return add_line(state, pos, end - pos, rows_indent);
	for (;;) {
		size_t avail = (size_t)(state->width - rows_indent), cut;

		if (end - pos <= avail)
			return add_line(state, pos, end - pos, rows_indent);
		for (cut = pos + avail; cut > pos && text[cut] != ' '; cut--)
			;
		if (cut == pos)
			cut = pos + avail;
		if (!add_line(state, pos, cut - pos, rows_indent))
			return false;
		pos = cut < end && text[cut] == ' ' ? cut + 1 : cut;
		rows_indent = indent + 2 <= state->width - WRAP_MIN ? indent + 2 : row_indent(state, indent);
	}
}

static void wrap_spoiler(UiHistory *state)
{
	const char *text = state->spoiler.text;
	size_t len = state->spoiler.len, start = 0, end;

	state->line_count = 0;
	state->spoiler_state = state->spoiler.cut ? UI_HISTORY_SPOILER_CUT : UI_HISTORY_SPOILER_OK;
	while (start < len) {
		for (end = start; end < len && text[end] != '\n'; end++)
			;
		if (!wrap_line(state, start, end)) {
			state->spoiler_state = UI_HISTORY_SPOILER_CUT;
			return;
		}
		start = end + 1;
	}
}

static void open_detail(UiHistory *state)
{
	const UiHistoryEntry *entry = &state->list.entries[state->cursor];

	state->view = UI_HISTORY_VIEW_DETAIL;
	state->detail_scroll = 0;
	state->line_count = 0;
	state->spoiler_state = UI_HISTORY_SPOILER_NONE;
	if (!entry->has_spoiler)
		return;
	state->spoiler.len = 0;
	state->spoiler.cut = false;
	if (!state->spoiler_fn(state->ctx, entry, &state->spoiler)) {
		state->spoiler_state = UI_HISTORY_SPOILER_UNREADABLE;
		return;
	}
	assert(state->spoiler.len <= UI_HISTORY_SPOILER_MAX);
	wrap_spoiler(state);
}

static int detail_rows(const UiHistory *state)
{
	switch (state->spoiler_state) {
	case UI_HISTORY_SPOILER_OK: return DETAIL_HEAD + 1 + state->line_count;
	case UI_HISTORY_SPOILER_CUT: return DETAIL_HEAD + 2 + state->line_count;
	case UI_HISTORY_SPOILER_NONE:
	case UI_HISTORY_SPOILER_UNREADABLE: break;
	}
	return DETAIL_HEAD + 1;
}

static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

static void scroll_detail(UiHistory *state, int delta)
{
	int last = detail_rows(state) - state->page;

	state->detail_scroll = clamp(state->detail_scroll + delta, 0, last > 0 ? last : 0);
}

static void move_to(UiHistory *state, int cursor)
{
	state->cursor = cursor;
	state->scroll = ui_scroll_follow(state->scroll, cursor, state->list.count, state->page);
}

static void confirm(UiHistory *state)
{
	state->confirm_from = state->view;
	state->view = UI_HISTORY_VIEW_CONFIRM;
}

static void delete_entry(UiHistory *state)
{
	UiHistoryDeleteOutcome outcome;
	int i;

	outcome.ok = false;
	outcome.list_stale = false;
	outcome.status = UI_STR_NONE;
	state->delete_fn(state->ctx, &state->list.entries[state->cursor], &outcome);
	if (!outcome.ok) {
		state->view = state->confirm_from;
		set_message(state, UI_MESSAGE_ERROR, outcome.status);
		return;
	}
	for (i = state->cursor; i + 1 < state->list.count; i++)
		state->list.entries[i] = state->list.entries[i + 1];
	state->list.count--;
	state->view = UI_HISTORY_VIEW_LIST;
	if (state->list.count == 0) {
		state->cursor = 0;
		state->scroll = 0;
	} else {
		move_to(state, clamp(state->cursor, 0, state->list.count - 1));
	}
	set_message(state, UI_MESSAGE_INFO, outcome.list_stale ? UI_STR_HISTORY_DELETED_REFRESH : UI_STR_HISTORY_DELETED);
}

static UiHistoryEvent tab_event(UiButton button)
{
	return button == UI_BUTTON_L1 ? UI_HISTORY_TAB_PREV : UI_HISTORY_TAB_NEXT;
}

static UiHistoryEvent list_key(UiHistory *state, UiButton button)
{
	int count = state->list.count;

	switch (button) {
	case UI_BUTTON_UP:
		if (count > 0)
			move_to(state, (state->cursor + count - 1) % count);
		break;
	case UI_BUTTON_DOWN:
		if (count > 0)
			move_to(state, (state->cursor + 1) % count);
		break;
	case UI_BUTTON_L2:
		if (count > 0)
			move_to(state, clamp(state->cursor - state->page, 0, count - 1));
		break;
	case UI_BUTTON_R2:
		if (count > 0)
			move_to(state, clamp(state->cursor + state->page, 0, count - 1));
		break;
	case UI_BUTTON_A:
		if (count > 0)
			open_detail(state);
		break;
	case UI_BUTTON_X:
		if (count > 0)
			confirm(state);
		break;
	case UI_BUTTON_B:
	case UI_BUTTON_MENU: return UI_HISTORY_QUIT;
	case UI_BUTTON_L1:
	case UI_BUTTON_R1: return tab_event(button);
	case UI_BUTTON_LEFT:
	case UI_BUTTON_RIGHT:
	case UI_BUTTON_Y:
	case UI_BUTTON_SELECT:
	case UI_BUTTON_START:
	case UI_BUTTON_COUNT: break;
	}
	return UI_HISTORY_NONE;
}

static UiHistoryEvent detail_key(UiHistory *state, UiButton button)
{
	switch (button) {
	case UI_BUTTON_UP: scroll_detail(state, -1); break;
	case UI_BUTTON_DOWN: scroll_detail(state, 1); break;
	case UI_BUTTON_L2: scroll_detail(state, -state->page); break;
	case UI_BUTTON_R2: scroll_detail(state, state->page); break;
	case UI_BUTTON_B: state->view = UI_HISTORY_VIEW_LIST; break;
	case UI_BUTTON_X: confirm(state); break;
	case UI_BUTTON_MENU: return UI_HISTORY_QUIT;
	case UI_BUTTON_L1:
	case UI_BUTTON_R1: return tab_event(button);
	case UI_BUTTON_LEFT:
	case UI_BUTTON_RIGHT:
	case UI_BUTTON_A:
	case UI_BUTTON_Y:
	case UI_BUTTON_SELECT:
	case UI_BUTTON_START:
	case UI_BUTTON_COUNT: break;
	}
	return UI_HISTORY_NONE;
}

static UiHistoryEvent confirm_key(UiHistory *state, UiButton button)
{
	switch (button) {
	case UI_BUTTON_A: delete_entry(state); break;
	case UI_BUTTON_B: state->view = state->confirm_from; break;
	case UI_BUTTON_MENU: return UI_HISTORY_QUIT;
	case UI_BUTTON_UP:
	case UI_BUTTON_DOWN:
	case UI_BUTTON_LEFT:
	case UI_BUTTON_RIGHT:
	case UI_BUTTON_X:
	case UI_BUTTON_Y:
	case UI_BUTTON_L1:
	case UI_BUTTON_R1:
	case UI_BUTTON_L2:
	case UI_BUTTON_R2:
	case UI_BUTTON_SELECT:
	case UI_BUTTON_START:
	case UI_BUTTON_COUNT: break;
	}
	return UI_HISTORY_NONE;
}

UiHistoryEvent ui_history_key(UiHistory *state, UiButton button)
{
	clear_message(state);
	switch (state->view) {
	case UI_HISTORY_VIEW_DETAIL: return detail_key(state, button);
	case UI_HISTORY_VIEW_CONFIRM: return confirm_key(state, button);
	case UI_HISTORY_VIEW_LIST: break;
	}
	return list_key(state, button);
}

static void add_text(UiScreen *out, const char *text)
{
	UiRow row = {UI_ROW_TEXT, NULL, NULL, false};

	row.label = text;
	ui_screen_add_row(out, row);
}

static void fill_date_line(UiHistory *state, const UiHistoryEntry *entry)
{
	char date[17];

	put_date(date, &entry->date);
	ui_str_fill(ui_str(state->strings, UI_STR_HISTORY_GENERATED_ON), date, state->date_line,
		    sizeof state->date_line);
}

static void show_message(const UiHistory *state, UiScreen *out)
{
	if (state->message_kind == UI_MESSAGE_NONE)
		return;
	out->message_kind = state->message_kind;
	out->message = state->message;
}

static void list_screen(UiHistory *state, UiScreen *out)
{
	const UiStrTable *t = state->strings;
	int i;

	out->title = ui_str(t, UI_STR_HISTORY_TITLE);
	show_message(state, out);
	if (state->list.count == 0) {
		add_text(out, ui_str(t, UI_STR_HISTORY_EMPTY));
		add_text(out, ui_str(t, UI_STR_HISTORY_EMPTY_HINT));
		return;
	}
	for (i = state->scroll; i < state->list.count && i < state->scroll + state->page; i++) {
		UiRow row = {UI_ROW_ACTION, NULL, NULL, false};

		row.label = state->list.entries[i].name.rom;
		ui_screen_add_row(out, row);
	}
	out->cursor = state->cursor - state->scroll;
	out->more_above = state->scroll > 0;
	out->more_below = state->scroll + state->page < state->list.count;
	out->footer = ui_str(t, UI_STR_HISTORY_FOOTER);
	if (out->message_kind == UI_MESSAGE_NONE) {
		fill_date_line(state, &state->list.entries[state->cursor]);
		out->message_kind = UI_MESSAGE_INFO;
		out->message = state->date_line;
	}
}

static void detail_row(UiHistory *state, int index, UiScreen *out)
{
	const UiStrTable *t = state->strings;
	const UiHistoryEntry *entry = &state->list.entries[state->cursor];
	UiRow row = {UI_ROW_SETTING, NULL, NULL, false};
	char *buf = state->rows[out->row_count];

	if (index == 0) {
		fill_date_line(state, entry);
		add_text(out, state->date_line);
	} else if (index == 1) {
		char number[16];

		put_decimal(number, (unsigned long)entry->seed);
		ui_str_fill(ui_str(t, UI_STR_HISTORY_SEED), number, state->seed_line, sizeof state->seed_line);
		add_text(out, state->seed_line);
	} else if (index < 2 + RANDO_SETTING_COUNT) {
		RandoSettingId id = (RandoSettingId)(index - 2);

		row.label = ui_str(t, ui_setting_label(id));
		row.value = ui_str(t, ui_setting_value(id, rando_setting_get(&entry->options, entry->heart_beep, id)));
		ui_screen_add_row(out, row);
	} else if (index == 2 + RANDO_SETTING_COUNT) {
		row.label = ui_str(t, UI_STR_SETTING_SPOILER);
		row.value = ui_str(t, entry->has_spoiler ? UI_STR_SPOILER_ON : UI_STR_SPOILER_OFF);
		ui_screen_add_row(out, row);
	} else if (index == 3 + RANDO_SETTING_COUNT) {
		add_text(out, "");
	} else if (state->spoiler_state == UI_HISTORY_SPOILER_NONE) {
		add_text(out, ui_str(t, UI_STR_HISTORY_NO_SPOILER));
	} else if (state->spoiler_state == UI_HISTORY_SPOILER_UNREADABLE) {
		add_text(out, ui_str(t, UI_STR_HISTORY_SPOILER_UNREADABLE));
	} else if (index == DETAIL_HEAD) {
		add_text(out, ui_str(t, UI_STR_HISTORY_SPOILER_TITLE));
	} else if (index - DETAIL_HEAD - 1 < state->line_count) {
		const UiHistoryLine *line = &state->lines[index - DETAIL_HEAD - 1];

		memset(buf, ' ', line->indent);
		memcpy(buf + line->indent, state->spoiler.text + line->offset, line->len);
		buf[line->indent + line->len] = '\0';
		add_text(out, buf);
	} else {
		add_text(out, ui_str(t, UI_STR_HISTORY_SPOILER_CUT));
	}
}

static void detail_screen(UiHistory *state, UiScreen *out)
{
	int rows = detail_rows(state), i;

	out->title = state->list.entries[state->cursor].name.rom;
	for (i = state->detail_scroll; i < rows && i < state->detail_scroll + state->page; i++)
		detail_row(state, i, out);
	out->more_above = state->detail_scroll > 0;
	out->more_below = state->detail_scroll + state->page < rows;
	out->footer = ui_str(state->strings, UI_STR_HISTORY_DETAIL_FOOTER);
	show_message(state, out);
}

static void confirm_screen(UiHistory *state, UiScreen *out)
{
	const UiStrTable *t = state->strings;
	const UiHistoryEntry *entry = &state->list.entries[state->cursor];

	ui_str_fill(ui_str(t, UI_STR_HISTORY_DELETE_CONFIRM), entry->name.code, state->title, sizeof state->title);
	out->title = state->title;
	add_text(out, entry->name.rom);
	add_text(out, ui_str(t, UI_STR_HISTORY_DELETE_WHAT));
	add_text(out, ui_str(t, UI_STR_HISTORY_DELETE_FOREVER));
	out->footer = ui_str(t, UI_STR_HISTORY_CONFIRM_FOOTER);
	show_message(state, out);
}

void ui_history_screen(UiHistory *state, UiScreen *out)
{
	const UiStrTable *t = state->strings;

	ui_screen_init(out);
	out->tabs[0] = ui_str(t, UI_STR_TAB_GENERATE);
	out->tabs[1] = ui_str(t, UI_STR_TAB_HISTORY);
	out->tab_count = 2;
	out->active_tab = 1;
	switch (state->view) {
	case UI_HISTORY_VIEW_DETAIL: detail_screen(state, out); return;
	case UI_HISTORY_VIEW_CONFIRM: confirm_screen(state, out); return;
	case UI_HISTORY_VIEW_LIST: break;
	}
	list_screen(state, out);
}
