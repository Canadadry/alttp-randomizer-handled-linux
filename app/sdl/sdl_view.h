#ifndef SDL_VIEW_H
#define SDL_VIEW_H

#include "ui_grid.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <stdbool.h>

#define SDL_VIEW_GLYPHS 95 /* printable ASCII, 0x20 to 0x7E */

typedef struct SdlRgb {
	Uint8 r, g, b;
} SdlRgb;

typedef struct SdlPaletteEntry {
	SdlRgb band, text;
} SdlPaletteEntry;

/* One entry per line style; UI_LINE_BLANK's band is also the background around the cells. */
extern const SdlPaletteEntry sdl_view_palette[UI_LINE_STYLE_COUNT];

typedef enum SdlViewStatus {
	SDLVIEW_OK,
	SDLVIEW_FONT_OPEN,
	SDLVIEW_FONT_TOO_SMALL
} SdlViewStatus;

/* Caller-owned; valid between an SDLVIEW_OK sdl_view_open and sdl_view_close. */
typedef struct SdlView {
	TTF_Font *font;
	int point_size;
	int cols, lines;
	int cell_w, cell_h;
	SDL_Surface *glyphs[UI_LINE_STYLE_COUNT][SDL_VIEW_GLYPHS];
	bool failed[UI_LINE_STYLE_COUNT][SDL_VIEW_GLYPHS];
} SdlView;

/* TTF_Init first. Cells are surface_w / cols by surface_h / lines, from the top-left corner; the font is
 * opened at the largest size in 8..64 whose 'M' advance and line height fit a cell. On failure nothing
 * stays open and view needs no close. Asserts cols and lines are within ui_grid_init's bounds. */
SdlViewStatus sdl_view_open(SdlView *view, const char *font_path, int surface_w, int surface_h, int cols,
			    int lines);

/* Draws grid, the size view was opened with, over the whole of dst (at least that size). */
void sdl_view_draw(SdlView *view, const UiGrid *grid, SDL_Surface *dst);

/* Frees the glyph cache and the font. */
void sdl_view_close(SdlView *view);

#endif
