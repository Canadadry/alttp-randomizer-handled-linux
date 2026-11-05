#include "ui_screen.h"

#include <assert.h>
#include <stddef.h>

void ui_screen_init(UiScreen *screen)
{
	int i;

	for (i = 0; i < UI_TABS_MAX; i++)
		screen->tabs[i] = NULL;
	screen->tab_count = 0;
	screen->active_tab = 0;
	screen->title = NULL;
	screen->row_count = 0;
	screen->cursor = -1;
	screen->scroll = 0;
	screen->more_above = false;
	screen->more_below = false;
	screen->message_kind = UI_MESSAGE_NONE;
	screen->message = NULL;
	screen->hint_count = 0;
}

bool ui_screen_add_row(UiScreen *screen, UiRow row)
{
	if (screen->row_count >= UI_ROWS_MAX)
		return false;
	screen->rows[screen->row_count++] = row;
	return true;
}

bool ui_screen_add_hint(UiScreen *screen, UiHint hint)
{
	if (screen->hint_count >= UI_HINTS_MAX)
		return false;
	screen->hints[screen->hint_count++] = hint;
	return true;
}

static bool hints_valid(const UiScreen *screen)
{
	int i, primaries = 0;

	if (screen->hint_count < 0 || screen->hint_count > UI_HINTS_MAX)
		return false;
	for (i = 0; i < screen->hint_count; i++) {
		const UiHint *hint = &screen->hints[i];

		if ((int)hint->button < 0 || hint->button >= UI_HINT_BUTTON_COUNT)
			return false;
		if (hint->primary)
			primaries++;
	}
	return primaries <= 1;
}

bool ui_screen_valid(const UiScreen *screen)
{
	int last_scroll;

	if (!hints_valid(screen))
		return false;

	if (screen->tab_count < 0 || screen->tab_count > UI_TABS_MAX)
		return false;
	if (screen->row_count < 0 || screen->row_count > UI_ROWS_MAX)
		return false;
	if (screen->tab_count > 0 && (screen->active_tab < 0 || screen->active_tab >= screen->tab_count))
		return false;
	if (screen->cursor < -1 || screen->cursor >= screen->row_count)
		return false;
	last_scroll = screen->row_count > 0 ? screen->row_count - 1 : 0;
	return screen->scroll >= 0 && screen->scroll <= last_scroll;
}

int ui_scroll_follow(int scroll, int cursor, int rows, int page)
{
	int last;

	assert(page >= 1 && rows >= 0);
	assert(cursor >= -1 && cursor < rows);
	if (cursor >= 0 && cursor < scroll)
		scroll = cursor;
	if (cursor >= 0 && cursor >= scroll + page)
		scroll = cursor - page + 1;
	last = rows > page ? rows - page : 0;
	if (scroll > last)
		scroll = last;
	return scroll < 0 ? 0 : scroll;
}
