#ifndef SDL_RENDER_H
#define SDL_RENDER_H

#include "theme.h"
#include "ui_screen.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <stdbool.h>

#define SDL_RENDER_ERROR_MAX 256
#define SDL_RENDER_COLOURS 16
#define SDL_RENDER_GLYPHS 95

typedef enum SdlRenderReason {
	SDLRENDER_OPEN,
	SDLRENDER_FIT,
	SDLRENDER_SIZE,
	SDLRENDER_PATH
} SdlRenderReason;

/* asset indexes theme_assets' list; w, h are the image's size for SDLRENDER_SIZE; error is SDL_ttf's or
   SDL_image's text for SDLRENDER_OPEN, else "". */
typedef struct SdlRenderFailure {
	int asset;
	SdlRenderReason reason;
	int w, h;
	char error[SDL_RENDER_ERROR_MAX];
} SdlRenderFailure;

/* theme must outlive it. images is indexed like assets, NULL for a font. A cell font's glyphs are
   cached for its first SDL_RENDER_COLOURS colours only. */
typedef struct SdlRender {
	const Theme *theme;
	ThemeAsset assets[THEME_ASSETS_MAX];
	int asset_count;
	TTF_Font *fonts[THEME_FONTS_MAX];
	int point_size[THEME_FONTS_MAX];
	SDL_Surface *images[THEME_ASSETS_MAX];
	ThemeColour colours[THEME_FONTS_MAX][SDL_RENDER_COLOURS];
	int colour_count[THEME_FONTS_MAX];
	SDL_Surface *glyphs[THEME_FONTS_MAX][SDL_RENDER_COLOURS][SDL_RENDER_GLYPHS];
	bool glyph_failed[THEME_FONTS_MAX][SDL_RENDER_COLOURS][SDL_RENDER_GLYPHS];
} SdlRender;

/* TTF and IMG must be initialised. On false, f says why and nothing stays open. */
bool sdl_render_open(SdlRender *r, const Theme *theme, const char *dir, SdlRenderFailure *f);

/* The list font's cell W, or its 'M' advance when it has no cell. */
int sdl_render_advance(const SdlRender *r);

/* dst is 640x480, 32-bit, masks 0x00FF0000 0x0000FF00 0x000000FF. False, dst untouched, when the
   screen has more tabs than the theme has tab slots. */
bool sdl_render_draw(SdlRender *r, const UiScreen *screen, SDL_Surface *dst);

void sdl_render_close(SdlRender *r);

#endif
