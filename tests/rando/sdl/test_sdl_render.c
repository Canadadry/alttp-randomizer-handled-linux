#include "rando_crc32.h"
#include "sdl_render.h"
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
#define DATA_FONT "../../data/DejaVuSansMono.ttf"
#define FEATURES_SRC "sdl/themes/features"
#define FEATURES WORK "/features"
/* Frozen from the deleted grid renderer's fit on classic's font (pinned environment). */
#define CLASSIC_FIT_SIZE 26

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

static void test_classic_opens_at_the_frozen_fit(void)
{
	SdlRenderFailure f;

	load(&theme, CLASSIC);
	if (!sdl_render_open(&render, &theme, CLASSIC, &f))
		test_fail(TEST, "classic: reason %d asset %d: %s", (int)f.reason, f.asset, f.error);
	CHECK(render.point_size[0] == CLASSIC_FIT_SIZE);
	CHECK(sdl_render_advance(&render) == 16);
	CHECK(every_pixel_is(render.images[asset_of(&theme, "screen.image")], 0x14161a));
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

static SDL_Surface *surface(void)
{
	SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);

	if (s == NULL)
		test_fail(TEST, "SDL_CreateRGBSurface: %s", SDL_GetError());
	return s;
}

static Uint32 pixel(SDL_Surface *s, int x, int y)
{
	return ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x] & 0x00FFFFFF;
}

static UiScreen base_screen(void)
{
	UiScreen sc;

	ui_screen_init(&sc);
	return sc;
}

static void with_tabs(UiScreen *sc, int active)
{
	sc->tabs[0] = "Generate";
	sc->tabs[1] = "History";
	sc->tab_count = 2;
	sc->active_tab = active;
}

static void add_hint(UiScreen *sc, UiHintButton button, const char *key, const char *label, bool primary)
{
	UiHint hint;

	hint.button = button;
	hint.key = key;
	hint.label = label;
	hint.primary = primary;
	CHECK(ui_screen_add_hint(sc, hint));
}

static const char *chars(int n, char c)
{
	static char buf[8][128];
	static int next;
	char *b = buf[next++ % 8];

	CHECK(n >= 0 && n < (int)sizeof buf[0]);
	memset(b, c, (size_t)n);
	b[n] = '\0';
	return b;
}

static void add_row(UiScreen *sc, UiRowKind kind, const char *label, const char *value, bool disabled, bool emphasis)
{
	UiRow row;

	row.kind = kind;
	row.label = label;
	row.value = value;
	row.disabled = disabled;
	row.emphasis = emphasis;
	CHECK(ui_screen_add_row(sc, row));
}

static Uint32 rgb(int r, int g, int b)
{
	return (Uint32)((r << 16) | (g << 8) | b);
}

static Uint32 rgb_of(ThemeColour c)
{
	return rgb(c.r, c.g, c.b);
}

static Theme features;
static SDL_Surface *shot;
static TTF_Font *run_font;

#define FRAME_TOP 0x102030u
#define FRAME_BOTTOM 0x304050u

/* The fixture's theme.ini and images with data/'s font, in FEATURES. */
static void open_features(void)
{
	ThemeAsset assets[THEME_ASSETS_MAX];
	SdlRenderFailure f;
	char from[512], to[512];
	int n, i;

	mkdir(WORK, 0755);
	mkdir(FEATURES, 0755);
	copy_file(FEATURES_SRC "/theme.ini", FEATURES "/theme.ini");
	load(&features, FEATURES);
	n = theme_assets(&features, assets);
	for (i = 0; i < n; i++) {
		snprintf(from, sizeof from, "%s/%s", assets[i].kind == THEME_ASSET_FONT ? "../../data" : FEATURES_SRC,
			 assets[i].file);
		snprintf(to, sizeof to, FEATURES "/%s", assets[i].file);
		copy_file(from, to);
	}
	if (!sdl_render_open(&render, &features, FEATURES, &f))
		test_fail(TEST, "features: reason %d asset %d: %s", (int)f.reason, f.asset, f.error);
	run_font = TTF_OpenFont(DATA_FONT, 16);
	if (run_font == NULL)
		test_fail(TEST, "TTF_OpenFont: %s", TTF_GetError());
	shot = surface();
}

static void close_features(void)
{
	TTF_CloseFont(run_font);
	SDL_FreeSurface(shot);
	sdl_render_close(&render);
}

static void draw(const UiScreen *sc)
{
	if (!sdl_render_draw(&render, sc, shot))
		test_fail(TEST, "sdl_render_draw refused the screen");
}

static int run_width(const char *text)
{
	int w = 0;

	if (text[0] != '\0' && TTF_SizeText(run_font, text, &w, NULL) != 0)
		test_fail(TEST, "TTF_SizeText: %s", TTF_GetError());
	return w;
}

/* The box of text blitted directly at (x, baseline - ascent) on a bg surface equals shot's. */
static void expect_run(const char *what, const char *text, ThemeColour c, int x, int baseline, Uint32 bg)
{
	SDL_Surface *scratch = surface(), *t;
	SDL_Color colour;
	SDL_Rect at;
	int top = baseline - TTF_FontAscent(run_font), px, py;

	colour.r = c.r;
	colour.g = c.g;
	colour.b = c.b;
	colour.unused = 0;
	SDL_FillRect(scratch, NULL, bg);
	t = TTF_RenderText_Blended(run_font, text, colour);
	if (t == NULL)
		test_fail(TEST, "TTF_RenderText_Blended: %s", TTF_GetError());
	at.x = (Sint16)x;
	at.y = (Sint16)top;
	SDL_BlitSurface(t, NULL, scratch, &at);
	for (py = top; py < top + t->h; py++)
		for (px = x; px < x + t->w && px < 640; px++)
			if (pixel(scratch, px, py) != pixel(shot, px, py))
				test_fail(TEST, "%s: '%s' at (%d, %d): drawn %06x, expected %06x", what, text, px, py,
					  (unsigned)pixel(shot, px, py), (unsigned)pixel(scratch, px, py));
	SDL_FreeSurface(t);
	SDL_FreeSurface(scratch);
}

static void expect_rect(const char *what, int x, int y, int w, int h, Uint32 colour)
{
	int px, py;

	for (py = y; py < y + h; py++)
		for (px = x; px < x + w; px++)
			if (pixel(shot, px, py) != colour)
				test_fail(TEST, "%s: (%d, %d) is %06x, expected %06x", what, px, py, (unsigned)pixel(shot, px, py),
					  (unsigned)colour);
}

static bool colour_in_rows(Uint32 colour, int y0, int y1)
{
	int x, y;

	for (y = y0; y < y1; y++)
		for (x = 0; x < 640; x++)
			if (pixel(shot, x, y) == colour)
				return true;
	return false;
}

static void test_features_frame_and_bands(void)
{
	const ThemeBand *tb = &features.tabs.band, *fb = &features.footer.band;
	UiScreen sc = base_screen();

	draw(&sc);
	expect_rect("frame top", 0, 0, 640, 240, FRAME_TOP);
	expect_rect("frame bottom", 0, 240, 640, 240, FRAME_BOTTOM);

	with_tabs(&sc, 0);
	draw(&sc);
	expect_rect("tab band top line", 620, tb->y, 10, tb->top_line, rgb_of(tb->line_colour));
	expect_rect("tab band", 620, tb->y + tb->top_line, 10, tb->h - tb->top_line - tb->bottom_line,
		    rgb_of(tb->colour));
	expect_rect("tab band bottom line", 620, tb->y + tb->h - tb->bottom_line, 10, tb->bottom_line,
		    rgb_of(tb->line_colour));
	expect_rect("frame left of the tab band", 0, tb->y, tb->x, tb->h, FRAME_TOP);
	expect_rect("no footer band without hints", 0, 240, 640, 240, FRAME_BOTTOM);

	sc = base_screen();
	add_hint(&sc, UI_HINT_Y, "Y", "Default", false);
	draw(&sc);
	expect_rect("no tab band without tabs", 0, 0, 640, 240, FRAME_TOP);
	expect_rect("footer band top line", 620, fb->y, 4, fb->top_line, rgb_of(fb->line_colour));
	expect_rect("footer band", 620, fb->y + fb->top_line, 4, fb->h - fb->top_line - fb->bottom_line,
		    rgb_of(fb->colour));
	expect_rect("footer band bottom line", 620, fb->y + fb->h - fb->bottom_line, 4, fb->bottom_line,
		    rgb_of(fb->line_colour));
	expect_rect("frame right of the footer band", fb->x + fb->w, fb->y, 640 - fb->x - fb->w, fb->h, FRAME_BOTTOM);
}

static void test_features_tabs(void)
{
	const ThemeTabs *t = &features.tabs;
	Uint32 band_colour = rgb_of(t->band.colour);
	UiScreen sc = base_screen();
	int end;

	with_tabs(&sc, 0);
	draw(&sc);
	expect_run("active tab at active_x", "Generate", t->active_colour, t->slots[0].active_x, t->y, band_colour);
	expect_run("inactive tab at x", "History", t->colour, t->slots[1].x, t->y, band_colour);
	expect_rect("sprite marker", t->slots[0].marker_x, t->marker_y, 12, 10, 0xff00ffu);
	expect_rect("right of the sprite marker", t->slots[0].marker_x + 12, t->marker_y, 1, 10, band_colour);
	end = t->slots[0].active_x + run_width("Generate");
	expect_run("text marker after the active text", "]", t->active_colour, end, t->marker_y, band_colour);

	with_tabs(&sc, 1);
	draw(&sc);
	expect_run("inactive first tab at x", "Generate", t->colour, t->slots[0].x, t->y, band_colour);
	expect_run("active second tab at active_x", "History", t->active_colour, t->slots[1].active_x, t->y,
		   band_colour);
	expect_rect("second slot's marker", t->slots[1].marker_x, t->marker_y, 12, 10, 0xff00ffu);
	CHECK(pixel(shot, t->slots[0].marker_x, t->marker_y) == band_colour);
}

static void test_features_title(void)
{
	const ThemeTitle *t = &features.title;
	const ThemeRule *rule = &t->rule;
	static const char long_title[] = "Generation failed because the seed could not be placed anywhere";
	char kept[sizeof long_title];
	UiScreen sc = base_screen();
	int start, x, y, len, room = t->right - t->x, top = t->y - TTF_FontAscent(run_font);

	sc.title = "Settings";
	draw(&sc);
	expect_run("title", "Settings", t->colour, t->x, t->y, FRAME_TOP);
	start = t->x + run_width("Settings") + rule->offset;
	for (y = rule->y - 1; y <= rule->y + rule->thickness; y++)
		for (x = t->x; x < 640; x++) {
			bool dash = y >= rule->y && y < rule->y + rule->thickness && x >= start && x < rule->end &&
				    (x - start) % (rule->dash + rule->gap) < rule->dash;

			if ((pixel(shot, x, y) == rgb_of(rule->colour)) != dash)
				test_fail(TEST, "title rule: (%d, %d) is %06x, dash expected: %d", x, y,
					  (unsigned)pixel(shot, x, y), (int)dash);
		}

	sc.title_tone = UI_TITLE_ALERT;
	draw(&sc);
	expect_run("alert title", "Settings", t->alert_colour, t->x, t->y, FRAME_TOP);

	sc.title = long_title;
	draw(&sc);
	len = (int)strlen(long_title);
	memcpy(kept, long_title, sizeof kept);
	while (len > 0 && run_width(kept) + run_width("~") > room)
		kept[--len] = '\0';
	CHECK(len > 0 && len < (int)strlen(long_title) - 1);
	expect_run("cut title", kept, t->alert_colour, t->x, t->y, FRAME_TOP);
	expect_run("cut mark", "~", t->alert_colour, t->x + run_width(kept), t->y, FRAME_TOP);
	expect_rect("nothing past the title's room", t->right, top, 640 - t->right, TTF_FontHeight(run_font),
		    FRAME_TOP);
}

static void test_features_footer(void)
{
	const ThemeFooter *f = &features.footer;
	Uint32 band_colour = rgb_of(f->band.colour);
	UiScreen sc = base_screen();
	int label, key, len;
	char text[128];

	add_hint(&sc, UI_HINT_A, "A", "Back", false);
	add_hint(&sc, UI_HINT_B, "B", "Delete", true);
	draw(&sc);
	expect_rect("sprite key's opaque half", f->x, f->key_y, 10, 18, 0xc00000u);
	expect_rect("sprite key's transparent half", f->x + 10, f->key_y, 10, 18, band_colour);
	label = f->x + 20 + f->key_gap;
	expect_run("label after the sprite key", "Back", f->label_colour, label, f->y, band_colour);
	key = label + run_width("Back") + f->hint_gap;
	expect_rect("second key at the label's end + hint_gap", key, f->key_y, 21, 22, 0x00c000u);
	expect_rect("before the second key", key - 1, f->key_y, 1, 22, band_colour);
	expect_run("primary label", "Delete", f->primary_colour, key + 21 + f->key_gap, f->y, band_colour);

	sc = base_screen();
	add_hint(&sc, UI_HINT_A, "A", "Back", false);
	add_hint(&sc, UI_HINT_START, "Start", chars(60, 's'), false);
	add_hint(&sc, UI_HINT_B, "B", "Keep", false);
	draw(&sc);
	label = f->x + 20 + f->key_gap + run_width("Back") + f->hint_gap + 66 + f->key_gap;
	expect_rect("start key", label - f->key_gap - 66, f->key_y, 66, 22, 0xe83838u);
	memcpy(text, chars(60, 's'), 61);
	for (len = 60; len > 0 && label + run_width(text) + run_width("~") > f->right;)
		text[--len] = '\0';
	expect_run("cut label", text, f->label_colour, label, f->y, band_colour);
	expect_run("cut mark", "~", f->label_colour, label + run_width(text), f->y, band_colour);
	CHECK(!colour_in_rows(0x00c000u, f->band.y, f->band.y + f->band.h));

	for (len = 1; len < 100; len++) {
		int end = f->x + 20 + f->key_gap + run_width(chars(len, 'd'));

		key = end + f->hint_gap;
		if (end <= f->right && key + 21 > f->right && key + run_width("~") <= f->right)
			break;
	}
	CHECK(len < 100);
	sc = base_screen();
	add_hint(&sc, UI_HINT_A, "A", chars(len, 'd'), false);
	add_hint(&sc, UI_HINT_B, "B", "Keep", false);
	draw(&sc);
	expect_run("label before a dropped key", chars(len, 'd'), f->label_colour, f->x + 20 + f->key_gap, f->y,
		   band_colour);
	CHECK(!colour_in_rows(0x00c000u, f->band.y, f->band.y + f->band.h));
	expect_run("mark at the dropped key's x", "~", f->label_colour, key, f->y, band_colour);
}

static int row_top(int k)
{
	return features.list.top + k * features.list.row_height;
}

static int row_base(int k)
{
	return row_top(k) + features.list.text_y;
}

/* The text a run-font cut keeps: the longest prefix whose width plus the mark's fits in room. */
static const char *run_cut(const char *text, int room)
{
	static char kept[128];
	int len = (int)strlen(text);

	CHECK(len < (int)sizeof kept);
	memcpy(kept, text, (size_t)len + 1);
	while (len > 0 && run_width(kept) + run_width("~") > room)
		kept[--len] = '\0';
	return kept;
}

static void test_features_list(void)
{
	const ThemeList *l = &features.list;
	static const char long_label[] = "A label far too long to fit before the value of its setting row";
	char kept[128];
	UiScreen sc = base_screen();
	int value_x, arrow_x, room;

	add_row(&sc, UI_ROW_SETTING, "Mode", "Open", false, false);
	add_row(&sc, UI_ROW_SETTING, "Goal", "Ganon", false, false);
	add_row(&sc, UI_ROW_ACTION, "Generate", NULL, false, false);
	add_row(&sc, UI_ROW_TEXT, "Some text", NULL, false, false);
	add_row(&sc, UI_ROW_SETTING, "Glitches", "None", true, false);
	sc.cursor = 0;
	draw(&sc);
	expect_rect("selected band top line", l->selected.x, row_top(0), 8, l->selected.top_line,
		    rgb_of(l->selected.line_colour));
	expect_rect("selected band", l->selected.x, row_top(0) + l->selected.top_line, 8,
		    l->selected.h - l->selected.top_line - l->selected.bottom_line, rgb_of(l->selected.colour));
	expect_rect("selected band bottom line", l->selected.x, row_top(0) + l->selected.h - l->selected.bottom_line, 8,
		    l->selected.bottom_line, rgb_of(l->selected.line_colour));
	expect_rect("left of the selected band", 0, row_top(0), l->selected.x, l->selected.h, FRAME_TOP);
	expect_rect("right of the selected band", l->selected.x + l->selected.w, row_top(0),
		    640 - l->selected.x - l->selected.w, l->selected.h, FRAME_TOP);
	expect_rect("no band on an unselected row", l->selected.x, row_top(1), 8, l->row_height, FRAME_TOP);
	expect_rect("cursor sprite", l->cursor_x, row_top(0) + l->cursor_y, 16, 14, 0xf8d038u);
	value_x = l->value_right - run_width("Open");
	arrow_x = value_x - l->arrow_left_gap - 8;
	expect_rect("left arrow before the drawn value", arrow_x, row_top(0) + l->arrow_y, 8, 12, 0x00ff00u);
	expect_rect("right of the left arrow", arrow_x + 8, row_top(0) + l->arrow_y, 1, 12, rgb_of(l->selected.colour));
	expect_rect("right arrow after value_right", l->value_right + l->arrow_right_gap, row_top(0) + l->arrow_y, 9, 12,
		    0x00ffaau);
	expect_run("unselected label at label_x_cursor", "Goal", l->label_colour, l->label_x_cursor, row_base(1),
		   FRAME_TOP);
	expect_run("value right-aligned", "Ganon", l->value_colour, l->value_right - run_width("Ganon"), row_base(1),
		   FRAME_TOP);
	CHECK(!colour_in_rows(0x00ff00u, row_top(1), row_top(2)));
	expect_run("action label", "Generate", l->label_colour, l->label_x_cursor, row_base(2), FRAME_TOP);
	expect_run("text row", "Some text", l->text_colour, l->label_x_cursor, row_base(3), FRAME_TOP);
	expect_run("disabled label", "Glitches", l->disabled_colour, l->label_x_cursor, row_base(4), FRAME_TOP);
	expect_run("disabled value", "None", l->disabled_colour, l->value_right - run_width("None"), row_base(4),
		   FRAME_TOP);

	sc.cursor = 4;
	draw(&sc);
	expect_rect("selected disabled band top line", l->selected_disabled.x, row_top(4), 8,
		    l->selected_disabled.top_line, rgb_of(l->selected_disabled.line_colour));
	expect_rect("selected disabled band bottom line", l->selected_disabled.x,
		    row_top(4) + l->selected_disabled.h - l->selected_disabled.bottom_line, 8,
		    l->selected_disabled.bottom_line, rgb_of(l->selected_disabled.line_colour));
	expect_rect("disabled cursor sprite", l->cursor_x, row_top(4) + l->cursor_y, 15, 13, 0xbcc4eeu);
	CHECK(!colour_in_rows(0xf8d038u, row_top(0), row_top(5)));
	CHECK(!colour_in_rows(0x00ff00u, row_top(0), row_top(5)));
	CHECK(!colour_in_rows(0x00ffaau, row_top(0), row_top(5)));

	sc.cursor = -1;
	draw(&sc);
	expect_run("label at label_x without a cursor", "Mode", l->label_colour, l->label_x, row_base(0), FRAME_TOP);
	CHECK(!colour_in_rows(rgb_of(l->selected.colour), row_top(0), row_top(5)));

	sc = base_screen();
	add_row(&sc, UI_ROW_SETTING, long_label, "Open", false, false);
	add_row(&sc, UI_ROW_SETTING, "Seed", chars(80, 'v'), false, false);
	add_row(&sc, UI_ROW_ACTION, long_label, NULL, false, false);
	add_row(&sc, UI_ROW_ACTION, "Back", NULL, false, false);
	sc.cursor = 3;
	draw(&sc);
	room = l->value_right - run_width("Open") - l->arrow_left_gap - 8 - l->label_gap - l->label_x_cursor;
	memcpy(kept, run_cut(long_label, room), sizeof kept);
	CHECK(kept[0] != '\0' && strlen(kept) < strlen(long_label) - 1);
	expect_run("label cut before the arrow", kept, l->label_colour, l->label_x_cursor, row_base(0), FRAME_TOP);
	expect_run("label's cut mark", "~", l->label_colour, l->label_x_cursor + run_width(kept), row_base(0), FRAME_TOP);
	memcpy(kept, run_cut(chars(80, 'v'), l->value_max), sizeof kept);
	value_x = l->value_right - run_width(kept) - run_width("~");
	CHECK(l->value_right - value_x <= l->value_max);
	expect_run("value cut to value_max", kept, l->value_colour, value_x, row_base(1), FRAME_TOP);
	expect_run("value's cut mark", "~", l->value_colour, value_x + run_width(kept), row_base(1), FRAME_TOP);
	memcpy(kept, run_cut(long_label, l->right - l->label_x_cursor), sizeof kept);
	expect_run("action cut at list.right", kept, l->label_colour, l->label_x_cursor, row_base(2), FRAME_TOP);
	expect_run("action's cut mark", "~", l->label_colour, l->label_x_cursor + run_width(kept), row_base(2),
		   FRAME_TOP);
}

static void test_features_scroll_marks(void)
{
	const ThemeList *l = &features.list;
	int page = theme_page(&features), i, last;
	UiScreen sc = base_screen();

	CHECK(page == 10);
	last = row_top(page - 1);
	add_row(&sc, UI_ROW_ACTION, "Generate", NULL, false, false);
	draw(&sc);
	CHECK(!colour_in_rows(0xff8000u, 0, 480) && !colour_in_rows(0xff0080u, 0, 480));
	sc.more_below = true;
	draw(&sc);
	expect_rect("down mark on an empty last line", l->scroll_x, last + l->scroll_y, 11, 6, 0xff0080u);
	CHECK(!colour_in_rows(0xff8000u, 0, 480));
	sc.more_below = false;
	sc.more_above = true;
	draw(&sc);
	expect_rect("up mark from more_above", l->scroll_x, l->top + l->scroll_y, 10, 6, 0xff8000u);

	sc = base_screen();
	for (i = 0; i < 15; i++)
		add_row(&sc, UI_ROW_ACTION, chars(i + 1, 'a'), NULL, false, false);
	sc.scroll = 3;
	sc.cursor = 3;
	draw(&sc);
	expect_rect("up mark when scrolled", l->scroll_x, l->top + l->scroll_y, 10, 6, 0xff8000u);
	expect_rect("down mark with rows below", l->scroll_x, last + l->scroll_y, 11, 6, 0xff0080u);
	expect_rect("cursor on the scrolled page's first line", l->cursor_x, row_top(0) + l->cursor_y, 16, 14,
		    0xf8d038u);
	expect_run("second visible row", chars(5, 'a'), l->label_colour, l->label_x_cursor, row_base(1), FRAME_TOP);
}

static void test_features_notice(void)
{
	const ThemeNotice *n = &features.notice;
	const ThemeList *l = &features.list;
	UiScreen sc = base_screen();
	int i;

	sc.body_kind = UI_BODY_NOTICE;
	add_row(&sc, UI_ROW_TEXT, "Seed ready", NULL, false, false);
	add_row(&sc, UI_ROW_TEXT, "seed.sfc", NULL, false, true);
	draw(&sc);
	expect_run("notice row", "Seed ready", n->colour, n->x, n->top + n->text_y, FRAME_TOP);
	expect_run("emphasised notice row", "seed.sfc", n->emphasis_colour, n->emphasis_x,
		   n->top + n->row_height + n->text_y, FRAME_TOP);

	sc = base_screen();
	sc.body_kind = UI_BODY_NOTICE;
	for (i = 0; i < 12; i++)
		add_row(&sc, UI_ROW_TEXT, chars(20, 'W'), NULL, false, false);
	sc.cursor = 0;
	sc.more_above = true;
	sc.more_below = true;
	draw(&sc);
	CHECK(!colour_in_rows(rgb_of(l->selected.colour), 0, 480));
	CHECK(!colour_in_rows(0xf8d038u, 0, 480));
	CHECK(!colour_in_rows(0xff8000u, 0, 480) && !colour_in_rows(0xff0080u, 0, 480));
	expect_rect("no row past the page", 0, n->top + theme_page(&features) * n->row_height, 640,
		    480 - n->top - theme_page(&features) * n->row_height, FRAME_BOTTOM);
}

static void test_features_too_many_tabs(void)
{
	UiScreen sc = base_screen();

	with_tabs(&sc, 2);
	sc.tabs[2] = "Third";
	sc.tab_count = 3;
	sc.title = "Settings";
	SDL_FillRect(shot, NULL, 0x123456u);
	CHECK(!sdl_render_draw(&render, &sc, shot));
	expect_rect("untouched", 0, 0, 640, 480, 0x123456u);
}

static void test_features(void)
{
	open_features();
	test_features_frame_and_bands();
	test_features_tabs();
	test_features_title();
	test_features_footer();
	test_features_list();
	test_features_scroll_marks();
	test_features_notice();
	test_features_too_many_tabs();
	close_features();
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
	test_classic_opens_at_the_frozen_fit();
	test_cell_less_advance_is_m_advance();
	test_failures();
	test_open_error_text_is_kept();
	test_features();
	IMG_Quit();
	TTF_Quit();
	SDL_Quit();
	printf("%s: passed\n", TEST);
	return 0;
}
