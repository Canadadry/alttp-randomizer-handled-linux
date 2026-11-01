#include "sdl_view.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#define POINT_MIN 8
#define POINT_MAX 64

#define BG { 0x14, 0x16, 0x1A }
#define BAR { 0x22, 0x26, 0x2E }

const SdlPaletteEntry sdl_view_palette[UI_LINE_STYLE_COUNT] = {
	[UI_LINE_BLANK] = { BG, { 0xE6, 0xE6, 0xE6 } },
	[UI_LINE_TABS] = { BAR, { 0xFF, 0xFF, 0xFF } },
	[UI_LINE_TITLE] = { BG, { 0x7F, 0xB4, 0xFF } },
	[UI_LINE_ROW] = { BG, { 0xE6, 0xE6, 0xE6 } },
	[UI_LINE_SELECTED] = { { 0x2F, 0x6F, 0xD6 }, { 0xFF, 0xFF, 0xFF } },
	[UI_LINE_DISABLED] = { BG, { 0x6B, 0x70, 0x78 } },
	[UI_LINE_SELECTED_DISABLED] = { { 0x3A, 0x3F, 0x48 }, { 0x8A, 0x90, 0x99 } },
	[UI_LINE_TEXT] = { BG, { 0xB8, 0xBC, 0xC4 } },
	[UI_LINE_INFO] = { BG, { 0x9A, 0xD0, 0xFF } },
	[UI_LINE_ERROR] = { BG, { 0xFF, 0x6B, 0x6B } },
	[UI_LINE_FOOTER] = { BAR, { 0x9A, 0xA0, 0xA8 } },
};

static bool fits(TTF_Font *font, int cell_w, int cell_h)
{
	int advance;

	return TTF_GlyphMetrics(font, 'M', NULL, NULL, NULL, NULL, &advance) == 0 && advance <= cell_w &&
	       TTF_FontHeight(font) <= cell_h;
}

SdlViewStatus sdl_view_open(SdlView *view, const char *font_path, int surface_w, int surface_h, int cols,
			    int lines)
{
	int size;

	assert(view != NULL && font_path != NULL);
	assert(cols >= UI_GRID_COLS_MIN && cols <= UI_GRID_COLS_MAX);
	assert(lines >= UI_GRID_LINES_MIN && lines <= UI_GRID_LINES_MAX);
	memset(view, 0, sizeof(*view));
	view->cols = cols;
	view->lines = lines;
	view->cell_w = surface_w / cols;
	view->cell_h = surface_h / lines;
	for (size = POINT_MAX; size >= POINT_MIN; size--) {
		TTF_Font *font = TTF_OpenFont(font_path, size);

		if (font == NULL)
			return SDLVIEW_FONT_OPEN;
		if (fits(font, view->cell_w, view->cell_h)) {
			view->font = font;
			view->point_size = size;
			return SDLVIEW_OK;
		}
		TTF_CloseFont(font);
	}
	return SDLVIEW_FONT_TOO_SMALL;
}

/* TTF_RenderGlyph_Blended would give a tight bitmap needing per-glyph metrics; a one-character
 * string gives the full line height with the glyph on its baseline, blittable at the cell's corner. */
static SDL_Surface *glyph(SdlView *view, UiLineStyle style, char c)
{
	int i = (unsigned char)c - 0x20;
	char text[2];
	SdlRgb rgb = sdl_view_palette[style].text;
	SDL_Color colour;

	if (i < 0 || i >= SDL_VIEW_GLYPHS || view->failed[style][i])
		return NULL;
	if (view->glyphs[style][i] == NULL) {
		text[0] = c;
		text[1] = '\0';
		colour.r = rgb.r;
		colour.g = rgb.g;
		colour.b = rgb.b;
		colour.unused = 0;
		view->glyphs[style][i] = TTF_RenderText_Blended(view->font, text, colour);
		view->failed[style][i] = view->glyphs[style][i] == NULL;
	}
	return view->glyphs[style][i];
}

static Uint32 map(SDL_Surface *dst, SdlRgb c)
{
	return SDL_MapRGB(dst->format, c.r, c.g, c.b);
}

void sdl_view_draw(SdlView *view, const UiGrid *grid, SDL_Surface *dst)
{
	SDL_Rect r;
	int line, col;

	assert(view != NULL && view->font != NULL && grid != NULL && dst != NULL);
	assert(grid->cols == view->cols && grid->lines == view->lines);
	assert(grid->cols * view->cell_w <= dst->w && grid->lines * view->cell_h <= dst->h);
	SDL_SetClipRect(dst, NULL);
	SDL_FillRect(dst, NULL, map(dst, sdl_view_palette[UI_LINE_BLANK].band));
	for (line = 0; line < grid->lines; line++) {
		UiLineStyle style = grid->styles[line];

		assert(style >= 0 && style < UI_LINE_STYLE_COUNT);
		/* SDL_FillRect obeys the clip rect the previous glyph left. */
		SDL_SetClipRect(dst, NULL);
		r.x = 0;
		r.y = (Sint16)(line * view->cell_h);
		r.w = (Uint16)(grid->cols * view->cell_w);
		r.h = (Uint16)view->cell_h;
		SDL_FillRect(dst, &r, map(dst, sdl_view_palette[style].band));
		for (col = 0; col < grid->cols; col++) {
			SDL_Surface *g;
			SDL_Rect cell, at;

			if (grid->cells[line][col] == ' ' || (g = glyph(view, style, grid->cells[line][col])) == NULL)
				continue;
			cell.x = (Sint16)(col * view->cell_w);
			cell.y = r.y;
			cell.w = (Uint16)view->cell_w;
			cell.h = (Uint16)view->cell_h;
			at = cell;
			if (g->w < view->cell_w)
				at.x = (Sint16)(at.x + (view->cell_w - g->w) / 2);
			SDL_SetClipRect(dst, &cell);
			SDL_BlitSurface(g, NULL, dst, &at);
		}
	}
	SDL_SetClipRect(dst, NULL);
}

void sdl_view_close(SdlView *view)
{
	int s, i;

	assert(view != NULL);
	for (s = 0; s < UI_LINE_STYLE_COUNT; s++)
		for (i = 0; i < SDL_VIEW_GLYPHS; i++)
			if (view->glyphs[s][i] != NULL)
				SDL_FreeSurface(view->glyphs[s][i]);
	if (view->font != NULL)
		TTF_CloseFont(view->font);
	memset(view, 0, sizeof(*view));
}
