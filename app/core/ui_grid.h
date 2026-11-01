#ifndef UI_GRID_H
#define UI_GRID_H

#include "ui_screen.h"

#include <stdbool.h>
#include <stddef.h>

#define UI_GRID_COLS_MIN 20
#define UI_GRID_COLS_MAX 96
#define UI_GRID_LINES_MIN 5
#define UI_GRID_LINES_MAX 40

/* 640x480 surface (docs/onion.md), 16x32 px cells. */
#define UI_GRID_DEFAULT_COLS 40
#define UI_GRID_DEFAULT_LINES 15

typedef enum UiLineStyle {
	UI_LINE_BLANK,
	UI_LINE_TABS,
	UI_LINE_TITLE,
	UI_LINE_ROW,
	UI_LINE_SELECTED,
	UI_LINE_DISABLED,
	UI_LINE_SELECTED_DISABLED,
	UI_LINE_TEXT,
	UI_LINE_INFO,
	UI_LINE_ERROR,
	UI_LINE_FOOTER,
	UI_LINE_STYLE_COUNT
} UiLineStyle;

typedef struct UiGrid {
	int cols, lines;
	char cells[UI_GRID_LINES_MAX][UI_GRID_COLS_MAX];
	UiLineStyle styles[UI_GRID_LINES_MAX];
} UiGrid;

/* All blank; false, leaving grid untouched, outside the bounds above. */
bool ui_grid_init(UiGrid *grid, int cols, int lines);

/* The number of list lines: lines - 4. */
int ui_grid_page(const UiGrid *grid);

/* Lays screen out on grid (already initialised, its size kept); false, leaving grid all blank, when
 * ui_screen_valid refuses screen. Texts are read during the call only. */
bool ui_grid_render(UiGrid *grid, const UiScreen *screen);

/* The text form of the largest grid, plus the terminating NUL. */
#define UI_GRID_TEXT_MAX ((UI_GRID_LINES_MAX + 2) * (UI_GRID_COLS_MAX + 4) + 1)

/* Writes the text form (a frame, a style tag per line) NUL-terminated into buf; returns its length, or 0
 * when size cannot hold it and the NUL (buf contents then unspecified). */
size_t ui_grid_text(const UiGrid *grid, char *buf, size_t size);

#endif
