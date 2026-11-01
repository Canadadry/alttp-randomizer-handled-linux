#include "ui_grid.h"

#include <assert.h>
#include <string.h>

static void grid_clear(UiGrid *grid)
{
	int line;

	for (line = 0; line < grid->lines; line++) {
		memset(grid->cells[line], ' ', (size_t)grid->cols);
		grid->styles[line] = UI_LINE_BLANK;
	}
}

static bool grid_size_valid(int cols, int lines)
{
	return cols >= UI_GRID_COLS_MIN && cols <= UI_GRID_COLS_MAX && lines >= UI_GRID_LINES_MIN &&
	       lines <= UI_GRID_LINES_MAX;
}

bool ui_grid_init(UiGrid *grid, int cols, int lines)
{
	if (!grid_size_valid(cols, lines))
		return false;
	grid->cols = cols;
	grid->lines = lines;
	grid_clear(grid);
	return true;
}

int ui_grid_page(const UiGrid *grid)
{
	assert(grid_size_valid(grid->cols, grid->lines));
	return grid->lines - 4;
}

static char style_tag(UiLineStyle style)
{
	switch (style) {
	case UI_LINE_TABS:
		return '=';
	case UI_LINE_TITLE:
		return '#';
	case UI_LINE_SELECTED:
		return '>';
	case UI_LINE_DISABLED:
		return 'x';
	case UI_LINE_SELECTED_DISABLED:
		return 'X';
	case UI_LINE_INFO:
		return 'i';
	case UI_LINE_ERROR:
		return 'E';
	case UI_LINE_FOOTER:
		return '_';
	case UI_LINE_BLANK:
	case UI_LINE_ROW:
	case UI_LINE_TEXT:
		break;
	}
	return ' ';
}

static char *text_border(char *out, int cols)
{
	*out++ = ' ';
	*out++ = '+';
	memset(out, '-', (size_t)cols);
	out += cols;
	*out++ = '+';
	*out++ = '\n';
	return out;
}

size_t ui_grid_text(const UiGrid *grid, char *buf, size_t size)
{
	size_t len;
	char *out = buf;
	int line;

	assert(grid_size_valid(grid->cols, grid->lines));
	len = (size_t)(grid->lines + 2) * (size_t)(grid->cols + 4);
	if (size < len + 1)
		return 0;
	out = text_border(out, grid->cols);
	for (line = 0; line < grid->lines; line++) {
		*out++ = style_tag(grid->styles[line]);
		*out++ = '|';
		memcpy(out, grid->cells[line], (size_t)grid->cols);
		out += grid->cols;
		*out++ = '|';
		*out++ = '\n';
	}
	out = text_border(out, grid->cols);
	*out = '\0';
	return len;
}

/* Writes into one line's columns [col, end); text past end is cut and the last column becomes CUT_MARK. */
typedef struct Pen {
	char *cells;
	int col, end;
} Pen;

#define CUT_MARK '~'

static Pen pen_at(UiGrid *grid, int line, int col, int end)
{
	Pen pen;

	pen.cells = grid->cells[line];
	pen.col = col;
	pen.end = end;
	return pen;
}

static void pen_char(Pen *pen, char c)
{
	if (pen->col < pen->end)
		pen->cells[pen->col] = c;
	pen->col++;
}

static void pen_text(Pen *pen, const char *text)
{
	const unsigned char *p;

	if (!text)
		return;
	for (p = (const unsigned char *)text; *p != '\0'; p++)
		pen_char(pen, *p >= 0x20 && *p <= 0x7E ? (char)*p : '?');
}

static void pen_done(const Pen *pen)
{
	if (pen->col > pen->end && pen->end > 0)
		pen->cells[pen->end - 1] = CUT_MARK;
}

static int text_len(const char *text)
{
	return text ? (int)strlen(text) : 0;
}

/* From column 1 to the last column before the scroll column. */
static void put_field(UiGrid *grid, int line, const char *text)
{
	Pen pen = pen_at(grid, line, 1, grid->cols - 1);

	pen_text(&pen, text);
	pen_done(&pen);
}

static void put_tabs(UiGrid *grid, const UiScreen *screen)
{
	Pen pen = pen_at(grid, 0, 0, grid->cols);
	int i;

	if (screen->tab_count == 0)
		return;
	for (i = 0; i < screen->tab_count; i++) {
		bool active = i == screen->active_tab;

		if (i > 0)
			pen_char(&pen, ' ');
		pen_char(&pen, active ? '[' : ' ');
		pen_text(&pen, screen->tabs[i]);
		pen_char(&pen, active ? ']' : ' ');
	}
	pen_done(&pen);
	grid->styles[0] = UI_LINE_TABS;
}

/* The value right-aligned as "< value >" (selected) or "  value  ", the label in what is left. */
static void put_setting(UiGrid *grid, int line, const UiRow *row, bool arrows)
{
	int field = grid->cols - 2;
	int value_len = text_len(row->value);
	int shown = value_len < field - 4 ? value_len : field - 4;
	int start = grid->cols - 1 - (shown + 4);
	Pen pen;

	if (value_len + 4 <= field - 2) {
		pen = pen_at(grid, line, 1, field - (value_len + 4));
		pen_text(&pen, row->label);
		pen_done(&pen);
	}
	grid->cells[line][start] = arrows ? '<' : ' ';
	pen = pen_at(grid, line, start + 2, start + 2 + shown);
	pen_text(&pen, row->value);
	pen_done(&pen);
	grid->cells[line][start + shown + 3] = arrows ? '>' : ' ';
}

static UiLineStyle row_style(const UiRow *row, bool selected)
{
	if (row->disabled)
		return selected ? UI_LINE_SELECTED_DISABLED : UI_LINE_DISABLED;
	if (selected)
		return UI_LINE_SELECTED;
	return row->kind == UI_ROW_TEXT ? UI_LINE_TEXT : UI_LINE_ROW;
}

static void put_list(UiGrid *grid, const UiScreen *screen)
{
	int page = ui_grid_page(grid);
	int first = 2, last = first + page - 1;
	int i;

	for (i = 0; i < page && screen->scroll + i < screen->row_count; i++) {
		int index = screen->scroll + i;
		const UiRow *row = &screen->rows[index];
		bool selected = index == screen->cursor;

		if (row->kind == UI_ROW_SETTING)
			put_setting(grid, first + i, row, selected && !row->disabled);
		else
			put_field(grid, first + i, row->label);
		grid->styles[first + i] = row_style(row, selected);
	}
	if (screen->scroll > 0 || screen->more_above)
		grid->cells[first][grid->cols - 1] = '^';
	if (screen->scroll + page < screen->row_count || screen->more_below)
		grid->cells[last][grid->cols - 1] = 'v';
}

static void put_line(UiGrid *grid, int line, const char *text, UiLineStyle style)
{
	put_field(grid, line, text);
	grid->styles[line] = style;
}

bool ui_grid_render(UiGrid *grid, const UiScreen *screen)
{
	assert(grid_size_valid(grid->cols, grid->lines));
	grid_clear(grid);
	if (!ui_screen_valid(screen))
		return false;
	put_tabs(grid, screen);
	if (text_len(screen->title) > 0)
		put_line(grid, 1, screen->title, UI_LINE_TITLE);
	put_list(grid, screen);
	if (screen->message_kind != UI_MESSAGE_NONE)
		put_line(grid, grid->lines - 2, screen->message,
			 screen->message_kind == UI_MESSAGE_ERROR ? UI_LINE_ERROR : UI_LINE_INFO);
	if (text_len(screen->footer) > 0)
		put_line(grid, grid->lines - 1, screen->footer, UI_LINE_FOOTER);
	return true;
}
