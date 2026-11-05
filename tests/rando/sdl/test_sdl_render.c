#include "rando_crc32.h"
#include "sdl_render.h"
#include "sdl_view.h"
#include "support.h"
#include "theme.h"
#include "ui_grid.h"

#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define TEST "test_sdl_render"
#define CLASSIC "../../themes/classic"
#define WORK "build/sdl/render_themes"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static Theme theme;
static SdlRender render;
static char classic_ini[THEME_FILE_BYTES_MAX + 1];

static long read_file(const char *path, unsigned char *buf, long cap)
{
	FILE *f = fopen(path, "rb");
	long n;

	if (f == NULL)
		test_fail(TEST, "cannot open %s", path);
	n = (long)fread(buf, 1, (size_t)cap, f);
	fclose(f);
	return n;
}

static void write_file(const char *path, const void *bytes, size_t n)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL || fwrite(bytes, 1, n, f) != n)
		test_fail(TEST, "cannot write %s", path);
	fclose(f);
}

static void copy_file(const char *from, const char *to)
{
	static unsigned char buf[1 << 20];
	long n = read_file(from, buf, (long)sizeof buf);

	write_file(to, buf, (size_t)n);
}

static void put32(unsigned char *p, unsigned long v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

static size_t chunk(unsigned char *out, const char *type, const unsigned char *data, size_t n)
{
	put32(out, (unsigned long)n);
	memcpy(out + 4, type, 4);
	memcpy(out + 8, data, n);
	put32(out + 8 + n, rando_crc32(out + 4, n + 4));
	return n + 12;
}

/* A w×h 8-bit RGB PNG, one colour, its pixels in one stored deflate block. */
static void write_png(const char *path, int w, int h)
{
	static unsigned char raw[16384], z[16384 + 16], png[16384 + 128];
	static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	unsigned char ihdr[13] = { 0 };
	size_t n = 0, len = (size_t)h * (size_t)(1 + 3 * w), i, at;
	unsigned long a = 1, b = 0;

	CHECK(len <= sizeof raw);
	for (i = 0; i < len; i++)
		raw[i] = (unsigned char)(i % (size_t)(1 + 3 * w) == 0 ? 0 : 0x55);
	for (i = 0; i < len; i++) {
		a = (a + raw[i]) % 65521;
		b = (b + a) % 65521;
	}
	z[0] = 0x78;
	z[1] = 0x01;
	z[2] = 1;
	z[3] = (unsigned char)len;
	z[4] = (unsigned char)(len >> 8);
	z[5] = (unsigned char)~len;
	z[6] = (unsigned char)(~len >> 8);
	memcpy(z + 7, raw, len);
	put32(z + 7 + len, (b << 16) | a);
	put32(ihdr, (unsigned long)w);
	put32(ihdr + 4, (unsigned long)h);
	ihdr[8] = 8;
	ihdr[9] = 2;
	memcpy(png, sig, sizeof sig);
	at = sizeof sig;
	at += chunk(png + at, "IHDR", ihdr, sizeof ihdr);
	at += chunk(png + at, "IDAT", z, len + 11);
	at += chunk(png + at, "IEND", NULL, 0);
	n = at;
	write_file(path, png, n);
}

static void load(Theme *t, const char *dir)
{
	ThemeError err;

	if (!theme_load(t, dir, &err))
		test_fail(TEST, "%s", err.message);
}

/* A copy of classic in WORK/<name>, its theme.ini with the line starting with `from` replaced by `to`. */
static const char *edited(const char *name, const char *from, const char *to)
{
	static char dir[256];
	char path[512], ini[THEME_FILE_BYTES_MAX * 2];
	const char *at = strstr(classic_ini, from);
	const char *end;

	if (at == NULL)
		test_fail(TEST, "classic has no line %s", from);
	end = strchr(at + strlen(from), '\n');
	snprintf(dir, sizeof dir, WORK "/%s", name);
	mkdir(WORK, 0755);
	mkdir(dir, 0755);
	snprintf(ini, sizeof ini, "%.*s%s%s", (int)(at - classic_ini), classic_ini, to, end);
	snprintf(path, sizeof path, "%s/theme.ini", dir);
	write_file(path, ini, strlen(ini));
	snprintf(path, sizeof path, "%s/DejaVuSansMono.ttf", dir);
	copy_file(CLASSIC "/DejaVuSansMono.ttf", path);
	snprintf(path, sizeof path, "%s/background.png", dir);
	copy_file(CLASSIC "/background.png", path);
	return dir;
}

static int asset_of(const Theme *t, const char *key)
{
	ThemeAsset assets[THEME_ASSETS_MAX];
	int n = theme_assets(t, assets), i;

	for (i = 0; i < n; i++)
		if (strcmp(assets[i].key, key) == 0)
			return i;
	test_fail(TEST, "no asset %s", key);
	return -1;
}

static void open_fails(const char *dir, SdlRenderReason reason, const char *key)
{
	static Theme t;
	SdlRenderFailure f;

	load(&t, dir);
	memset(&f, 0, sizeof f);
	if (sdl_render_open(&render, &t, dir, &f))
		test_fail(TEST, "%s: opened, expected reason %d on %s", dir, (int)reason, key);
	if (f.reason != reason || f.asset != asset_of(&t, key))
		test_fail(TEST, "%s: reason %d asset %d, expected %d on %s (%d)", dir, (int)f.reason, f.asset, (int)reason,
			  key, asset_of(&t, key));
}

static bool every_pixel_is(SDL_Surface *s, Uint32 rgb)
{
	int x, y;

	if (s == NULL || s->w != 640 || s->h != 480 || s->format->BytesPerPixel != 4 || s->format->Rmask != 0x00FF0000 ||
	    s->format->Gmask != 0x0000FF00 || s->format->Bmask != 0x000000FF)
		return false;
	for (y = 0; y < s->h; y++)
		for (x = 0; x < s->w; x++)
			if ((((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x] & 0x00FFFFFF) != rgb)
				return false;
	return true;
}

static void test_classic_opens_as_sdl_view(void)
{
	SdlRenderFailure f;
	SdlView view;

	load(&theme, CLASSIC);
	if (!sdl_render_open(&render, &theme, CLASSIC, &f))
		test_fail(TEST, "classic: reason %d asset %d: %s", (int)f.reason, f.asset, f.error);
	if (sdl_view_open(&view, CLASSIC "/DejaVuSansMono.ttf", 640, 480, UI_GRID_DEFAULT_COLS,
			  UI_GRID_DEFAULT_LINES) != SDLVIEW_OK)
		test_fail(TEST, "sdl_view_open: %s", TTF_GetError());
	CHECK(render.point_size[0] == view.point_size);
	CHECK(sdl_render_advance(&render) == 16);
	CHECK(every_pixel_is(render.images[asset_of(&theme, "screen.image")], 0x14161a));
	sdl_view_close(&view);
	sdl_render_close(&render);
}

static void test_cell_less_advance_is_m_advance(void)
{
	static Theme t;
	const char *dir = edited("no_cell", "size = fit\ncell = 16 32", "size = 16\ncell = none");
	SdlRenderFailure f;
	TTF_Font *font;
	int advance;

	load(&t, dir);
	if (!sdl_render_open(&render, &t, dir, &f))
		test_fail(TEST, "no_cell: reason %d asset %d: %s", (int)f.reason, f.asset, f.error);
	font = TTF_OpenFont(CLASSIC "/DejaVuSansMono.ttf", 16);
	CHECK(font != NULL && TTF_GlyphMetrics(font, 'M', NULL, NULL, NULL, NULL, &advance) == 0);
	CHECK(render.point_size[0] == 16);
	CHECK(sdl_render_advance(&render) == advance);
	TTF_CloseFont(font);
	sdl_render_close(&render);
}

static void test_failures(void)
{
	static Theme t;
	char path[512], dir[1200];
	SdlRenderFailure f;
	const char *small;

	open_fails(edited("missing_image", "image = background.png", "image = missing.png"), SDLRENDER_OPEN,
		   "screen.image");
	open_fails(edited("missing_font", "file = DejaVuSansMono.ttf", "file = missing.ttf"), SDLRENDER_OPEN,
		   "font.mono.file");
	open_fails(edited("no_fit", "cell = 16 32", "cell = 4 4"), SDLRENDER_FIT, "font.mono.file");

	small = edited("small_image", "image = background.png", "image = background.png");
	snprintf(path, sizeof path, "%s/background.png", small);
	write_png(path, 3, 2);
	open_fails(small, SDLRENDER_SIZE, "screen.image");
	load(&t, small);
	CHECK(!sdl_render_open(&render, &t, small, &f) && f.w == 3 && f.h == 2);

	load(&t, CLASSIC);
	memset(dir, 'd', sizeof dir - 1);
	dir[sizeof dir - 1] = '\0';
	CHECK(!sdl_render_open(&render, &t, dir, &f));
	CHECK(f.reason == SDLRENDER_PATH && f.asset == asset_of(&t, "screen.image"));
}

static void test_open_error_text_is_kept(void)
{
	static Theme t;
	const char *dir = edited("missing_font_text", "file = DejaVuSansMono.ttf", "file = missing.ttf");
	SdlRenderFailure f;

	load(&t, dir);
	CHECK(!sdl_render_open(&render, &t, dir, &f));
	CHECK(f.error[0] != '\0');
}

int main(void)
{
	long n;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		test_fail(TEST, "SDL_Init: %s", SDL_GetError());
	if (TTF_Init() != 0)
		test_fail(TEST, "TTF_Init: %s", TTF_GetError());
	if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
		test_fail(TEST, "IMG_Init: %s", IMG_GetError());
	n = read_file(CLASSIC "/theme.ini", (unsigned char *)classic_ini, (long)sizeof classic_ini - 1);
	classic_ini[n] = '\0';
	test_classic_opens_as_sdl_view();
	test_cell_less_advance_is_m_advance();
	test_failures();
	test_open_error_text_is_kept();
	IMG_Quit();
	TTF_Quit();
	SDL_Quit();
	printf("%s: passed\n", TEST);
	return 0;
}
