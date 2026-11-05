#ifndef SDL_RENDER_H
#define SDL_RENDER_H

#include "theme.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <stdbool.h>

#define SDL_RENDER_ERROR_MAX 256

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

/* theme must outlive it. images is indexed like assets, NULL for a font. */
typedef struct SdlRender {
	const Theme *theme;
	ThemeAsset assets[THEME_ASSETS_MAX];
	int asset_count;
	TTF_Font *fonts[THEME_FONTS_MAX];
	int point_size[THEME_FONTS_MAX];
	SDL_Surface *images[THEME_ASSETS_MAX];
} SdlRender;

/* TTF and IMG must be initialised. On false, f says why and nothing stays open. */
bool sdl_render_open(SdlRender *r, const Theme *theme, const char *dir, SdlRenderFailure *f);

/* The list font's cell W, or its 'M' advance when it has no cell. */
int sdl_render_advance(const SdlRender *r);

void sdl_render_close(SdlRender *r);

#endif
