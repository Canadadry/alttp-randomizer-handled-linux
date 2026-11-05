#include "sdl_render.h"

#include <SDL/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define POINT_MIN 8
#define POINT_MAX 64
#define PATH_BYTES 1024

static bool fail(SdlRender *r, SdlRenderFailure *f, int asset, SdlRenderReason reason, const char *error)
{
	f->asset = asset;
	f->reason = reason;
	snprintf(f->error, sizeof f->error, "%s", error);
	sdl_render_close(r);
	return false;
}

static bool fits(TTF_Font *font, int cell_w, int cell_h)
{
	int advance;

	return TTF_GlyphMetrics(font, 'M', NULL, NULL, NULL, NULL, &advance) == 0 && advance <= cell_w &&
	       TTF_FontHeight(font) <= cell_h;
}

static bool open_font(SdlRender *r, SdlRenderFailure *f, int asset, const char *path)
{
	const ThemeFont *tf = &r->theme->fonts[r->assets[asset].font];
	TTF_Font **out = &r->fonts[r->assets[asset].font];
	int size;

	if (!tf->size.fit) {
		*out = TTF_OpenFont(path, tf->size.px);
		if (*out == NULL)
			return fail(r, f, asset, SDLRENDER_OPEN, TTF_GetError());
		r->point_size[r->assets[asset].font] = tf->size.px;
		return true;
	}
	assert(tf->cell.present);
	for (size = POINT_MAX; size >= POINT_MIN; size--) {
		TTF_Font *font = TTF_OpenFont(path, size);

		if (font == NULL)
			return fail(r, f, asset, SDLRENDER_OPEN, TTF_GetError());
		if (fits(font, tf->cell.w, tf->cell.h)) {
			*out = font;
			r->point_size[r->assets[asset].font] = size;
			return true;
		}
		TTF_CloseFont(font);
	}
	return fail(r, f, asset, SDLRENDER_FIT, "");
}

/* The main's offscreen format; a plain copy, so an alpha channel in the file is dropped. */
static SDL_Surface *screen_copy(SDL_Surface *img)
{
	SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, THEME_SCREEN_W, THEME_SCREEN_H, 32, 0x00FF0000,
					      0x0000FF00, 0x000000FF, 0);

	if (s == NULL)
		return NULL;
	SDL_SetAlpha(img, 0, SDL_ALPHA_OPAQUE);
	if (SDL_BlitSurface(img, NULL, s, NULL) != 0) {
		SDL_FreeSurface(s);
		return NULL;
	}
	return s;
}

static bool open_image(SdlRender *r, SdlRenderFailure *f, int asset, const char *path)
{
	SDL_Surface *img = IMG_Load(path);

	if (img == NULL)
		return fail(r, f, asset, SDLRENDER_OPEN, IMG_GetError());
	if (r->assets[asset].kind == THEME_ASSET_SCREEN) {
		SDL_Surface *copy;

		if (img->w != THEME_SCREEN_W || img->h != THEME_SCREEN_H) {
			f->w = img->w;
			f->h = img->h;
			SDL_FreeSurface(img);
			return fail(r, f, asset, SDLRENDER_SIZE, "");
		}
		copy = screen_copy(img);
		SDL_FreeSurface(img);
		if (copy == NULL)
			return fail(r, f, asset, SDLRENDER_OPEN, SDL_GetError());
		img = copy;
	}
	r->images[asset] = img;
	return true;
}

bool sdl_render_open(SdlRender *r, const Theme *theme, const char *dir, SdlRenderFailure *f)
{
	char path[PATH_BYTES];
	int i;

	assert(r != NULL && theme != NULL && dir != NULL && f != NULL);
	memset(r, 0, sizeof *r);
	memset(f, 0, sizeof *f);
	r->theme = theme;
	r->asset_count = theme_assets(theme, r->assets);
	for (i = 0; i < r->asset_count; i++) {
		if (!theme_path(path, sizeof path, dir, r->assets[i].file))
			return fail(r, f, i, SDLRENDER_PATH, "");
		if (r->assets[i].kind == THEME_ASSET_FONT ? !open_font(r, f, i, path) : !open_image(r, f, i, path))
			return false;
	}
	return true;
}

int sdl_render_advance(const SdlRender *r)
{
	const ThemeFont *tf;
	int advance;

	assert(r != NULL && r->theme != NULL);
	tf = &r->theme->fonts[r->theme->list.font];
	if (tf->cell.present)
		return tf->cell.w;
	if (TTF_GlyphMetrics(r->fonts[r->theme->list.font], 'M', NULL, NULL, NULL, NULL, &advance) != 0 || advance < 1)
		return 1;
	return advance;
}

void sdl_render_close(SdlRender *r)
{
	int i;

	assert(r != NULL);
	for (i = 0; i < THEME_FONTS_MAX; i++)
		if (r->fonts[i] != NULL)
			TTF_CloseFont(r->fonts[i]);
	for (i = 0; i < THEME_ASSETS_MAX; i++)
		if (r->images[i] != NULL)
			SDL_FreeSurface(r->images[i]);
	memset(r, 0, sizeof *r);
}
