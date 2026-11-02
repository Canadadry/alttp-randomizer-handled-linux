#ifndef SDL_VIEW_H
#define SDL_VIEW_H

#include "ui_grid.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <stdbool.h>

#define SDL_VIEW_GLYPHS 95

typedef struct SdlRgb {
	Uint8 r, g, b;
} SdlRgb;

typedef struct SdlPaletteEntry {
	SdlRgb band, text;
} SdlPaletteEntry;

extern const SdlPaletteEntry sdl_view_palette[UI_LINE_STYLE_COUNT];

typedef enum SdlViewStatus {
	SDLVIEW_OK,
	SDLVIEW_FONT_OPEN,
	SDLVIEW_FONT_TOO_SMALL
} SdlViewStatus;

typedef struct SdlView {
	TTF_Font *font;
	int point_size;
	int cols, lines;
	int cell_w, cell_h;
	SDL_Surface *glyphs[UI_LINE_STYLE_COUNT][SDL_VIEW_GLYPHS];
	bool failed[UI_LINE_STYLE_COUNT][SDL_VIEW_GLYPHS];
} SdlView;

SdlViewStatus sdl_view_open(SdlView *view, const char *font_path, int surface_w, int surface_h, int cols,
			    int lines);

void sdl_view_draw(SdlView *view, const UiGrid *grid, SDL_Surface *dst);

void sdl_view_close(SdlView *view);

#endif
