#include "sdl_view.h"
#include "support.h"
#include "theme.h"
#include "ui_grid.h"

#include <SDL/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_sdl_classic"
#define CLASSIC "../../themes/classic"
#define DATA "../../data/"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define COLS UI_GRID_DEFAULT_COLS
#define LINES UI_GRID_DEFAULT_LINES

/* Frozen from sdl_view_palette and sdl_view_open's fit on classic's font (pinned environment). */
#define CLASSIC_FIT_SIZE 26
#define BG { 0x14, 0x16, 0x1A }
#define BAR { 0x22, 0x26, 0x2E }

typedef struct FrozenRgb {
	unsigned char r, g, b;
} FrozenRgb;

static const struct {
	FrozenRgb band, text;
} frozen_palette[UI_LINE_STYLE_COUNT] = {
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

static Theme theme;
static int cw, ch;

static bool same_oracle(ThemeColour c, SdlRgb rgb)
{
	return c.r == rgb.r && c.g == rgb.g && c.b == rgb.b;
}

static bool same_frozen(ThemeColour c, FrozenRgb rgb)
{
	return c.r == rgb.r && c.g == rgb.g && c.b == rgb.b;
}

static bool same_text(ThemeColour c, UiLineStyle style)
{
	return same_oracle(c, sdl_view_palette[style].text) && same_frozen(c, frozen_palette[style].text);
}

static bool same_band(ThemeColour c, UiLineStyle style)
{
	return same_oracle(c, sdl_view_palette[style].band) && same_frozen(c, frozen_palette[style].band);
}

static bool full_line_band(const ThemeBand *b, int line, bool row_relative)
{
	return b->present && b->x == 0 && b->y == (row_relative ? 0 : line * ch) && b->w == COLS * cw && b->h == ch &&
	       b->top_line == 0 && b->bottom_line == 0;
}

static void test_frozen_palette_is_oracle(void)
{
	int i;

	for (i = 0; i < UI_LINE_STYLE_COUNT; i++) {
		const SdlPaletteEntry *o = &sdl_view_palette[i];
		const FrozenRgb *band = &frozen_palette[i].band, *text = &frozen_palette[i].text;

		CHECK(o->band.r == band->r && o->band.g == band->g && o->band.b == band->b);
		CHECK(o->text.r == text->r && o->text.g == text->g && o->text.b == text->b);
	}
}

static void test_colours(void)
{
	CHECK(same_band(theme.screen.colour, UI_LINE_BLANK));
	CHECK(same_text(theme.tabs.colour, UI_LINE_TABS));
	CHECK(same_text(theme.tabs.active_colour, UI_LINE_TABS));
	CHECK(same_band(theme.tabs.band.colour, UI_LINE_TABS));
	CHECK(same_text(theme.title.colour, UI_LINE_TITLE));
	CHECK(same_text(theme.title.alert_colour, UI_LINE_TITLE));
	CHECK(same_text(theme.list.label_colour, UI_LINE_ROW));
	CHECK(same_text(theme.list.value_colour, UI_LINE_ROW));
	CHECK(same_text(theme.list.text_colour, UI_LINE_TEXT));
	CHECK(same_text(theme.list.disabled_colour, UI_LINE_DISABLED));
	CHECK(same_text(theme.list.selected_colour, UI_LINE_SELECTED));
	CHECK(same_text(theme.list.selected_disabled_colour, UI_LINE_SELECTED_DISABLED));
	CHECK(same_band(theme.list.selected.colour, UI_LINE_SELECTED));
	CHECK(same_band(theme.list.selected_disabled.colour, UI_LINE_SELECTED_DISABLED));
	CHECK(same_text(theme.notice.colour, UI_LINE_TEXT));
	CHECK(same_text(theme.notice.emphasis_colour, UI_LINE_TEXT));
	CHECK(same_text(theme.message.info_colour, UI_LINE_INFO));
	CHECK(same_text(theme.message.error_colour, UI_LINE_ERROR));
	CHECK(same_text(theme.footer.label_colour, UI_LINE_FOOTER));
	CHECK(same_text(theme.footer.primary_colour, UI_LINE_FOOTER));
	CHECK(same_band(theme.footer.band.colour, UI_LINE_FOOTER));
	CHECK(!theme.title.rule.present);
	CHECK(theme.screen.image.present && strcmp(theme.screen.image.file, "background.png") == 0);
}

/* classic draws no band for these styles: the grid's band for them is the background. */
static void test_bands_left_out_are_background(void)
{
	static const UiLineStyle styles[] = { UI_LINE_TITLE, UI_LINE_ROW, UI_LINE_DISABLED,
					      UI_LINE_TEXT, UI_LINE_INFO, UI_LINE_ERROR };
	size_t i;

	for (i = 0; i < sizeof styles / sizeof styles[0]; i++)
	{
		CHECK(memcmp(&sdl_view_palette[styles[i]].band, &sdl_view_palette[UI_LINE_BLANK].band, sizeof(SdlRgb)) == 0);
		CHECK(memcmp(&frozen_palette[styles[i]].band, &frozen_palette[UI_LINE_BLANK].band, sizeof(FrozenRgb)) == 0);
	}
}

static void test_geometry(void)
{
	UiGrid grid;

	CHECK(ui_grid_init(&grid, COLS, LINES));
	CHECK(theme.font_count == 1);
	CHECK(theme.fonts[0].cell.present && theme.fonts[0].cell.w == cw && theme.fonts[0].cell.h == ch);
	CHECK(theme.fonts[0].anchor == THEME_ANCHOR_TOP);
	CHECK(theme.screen.cut.kind == THEME_MARKER_TEXT && strcmp(theme.screen.cut.text, "~") == 0);

	CHECK(theme.tabs.y == 0 && theme.tabs.marker_y == 0 && theme.tabs.right == COLS * cw);
	CHECK(full_line_band(&theme.tabs.band, 0, false));
	CHECK(theme.title.y == 1 * ch);
	CHECK(theme.list.top == 2 * ch && theme.list.bottom == (LINES - 2) * ch);
	CHECK(theme.list.row_height == ch && theme.list.text_y == 0);
	CHECK((theme.list.bottom - theme.list.top) / theme.list.row_height == ui_grid_page(&grid));
	CHECK(full_line_band(&theme.list.selected, 0, true));
	CHECK(full_line_band(&theme.list.selected_disabled, 0, true));
	CHECK(theme.notice.top == 2 * ch && theme.notice.row_height == ch && theme.notice.text_y == 0);
	CHECK(theme.message.y == (LINES - 2) * ch);
	CHECK(theme.footer.y == (LINES - 1) * ch && theme.footer.key_y == (LINES - 1) * ch);
	CHECK(full_line_band(&theme.footer.band, LINES - 1, false));

	CHECK(theme.title.x == cw && theme.title.right == (COLS - 1) * cw);
	CHECK(theme.list.label_x == cw && theme.list.label_x_cursor == cw && theme.list.right == (COLS - 1) * cw);
	CHECK(theme.notice.x == cw && theme.notice.emphasis_x == cw && theme.notice.right == (COLS - 1) * cw);
	CHECK(theme.message.x == cw && theme.message.right == (COLS - 1) * cw);
	CHECK(theme.footer.x == cw && theme.footer.right == (COLS - 1) * cw);
	CHECK(theme.footer.key_gap == cw && theme.footer.hint_gap == 2 * cw);
	CHECK(theme.list.scroll_x == (COLS - 1) * cw && theme.list.scroll_y == 0);
	CHECK(theme.list.value_right == (COLS - 3) * cw && theme.list.value_max == (COLS - 6) * cw);
	CHECK(theme.list.arrow_left_gap == cw && theme.list.arrow_right_gap == cw && theme.list.label_gap == cw);
}

static int fit_size(const char *path, int w, int h)
{
	int size;

	for (size = 64; size >= 8; size--) {
		TTF_Font *font = TTF_OpenFont(path, size);
		int advance;
		bool fits;

		if (font == NULL)
			test_fail(TEST, "TTF_OpenFont %s %d: %s", path, size, TTF_GetError());
		fits = TTF_GlyphMetrics(font, 'M', NULL, NULL, NULL, NULL, &advance) == 0 && advance <= w &&
		       TTF_FontHeight(font) <= h;
		TTF_CloseFont(font);
		if (fits)
			return size;
	}
	return 0;
}

static void test_fit_is_sdl_view_open(void)
{
	char path[256];
	SdlView view;
	SdlViewStatus st;
	int size;

	snprintf(path, sizeof path, CLASSIC "/%s", theme.fonts[0].file);
	CHECK(theme.fonts[0].size.fit);
	size = fit_size(path, theme.fonts[0].cell.w, theme.fonts[0].cell.h);
	st = sdl_view_open(&view, path, 640, 480, COLS, LINES);
	if (st != SDLVIEW_OK)
		test_fail(TEST, "sdl_view_open %s: status %d (%s)", path, (int)st, TTF_GetError());
	CHECK(view.cell_w == theme.fonts[0].cell.w && view.cell_h == theme.fonts[0].cell.h);
	CHECK(size > 0 && view.point_size == size);
	CHECK(size == CLASSIC_FIT_SIZE);
	sdl_view_close(&view);
}

static long read_all(const char *path, unsigned char *buf, long cap)
{
	FILE *f = fopen(path, "rb");
	long n;

	if (f == NULL)
		test_fail(TEST, "cannot open %s", path);
	n = (long)fread(buf, 1, (size_t)cap, f);
	if (!feof(f) && fgetc(f) != EOF)
		test_fail(TEST, "%s: larger than %ld bytes", path, cap);
	fclose(f);
	return n;
}

static void same_bytes(const char *a, const char *b)
{
	static unsigned char x[1 << 20], y[1 << 20];
	long nx = read_all(a, x, (long)sizeof x), ny = read_all(b, y, (long)sizeof y);

	if (nx != ny || memcmp(x, y, (size_t)nx) != 0)
		test_fail(TEST, "%s and %s differ", a, b);
}

/* The goldens' font fingerprint is of data/'s copy. */
static void test_font_copies_are_identical(void)
{
	char path[256];

	snprintf(path, sizeof path, CLASSIC "/%s", theme.fonts[0].file);
	same_bytes(path, DATA "DejaVuSansMono.ttf");
	same_bytes(CLASSIC "/DejaVuSansMono-LICENSE.txt", DATA "DejaVuSansMono-LICENSE.txt");
}

int main(void)
{
	ThemeError err;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		test_fail(TEST, "SDL_Init: %s", SDL_GetError());
	if (TTF_Init() != 0)
		test_fail(TEST, "TTF_Init: %s", TTF_GetError());
	if (!theme_load(&theme, CLASSIC, &err))
		test_fail(TEST, "%s", err.message);
	cw = THEME_SCREEN_W / COLS;
	ch = THEME_SCREEN_H / LINES;
	test_frozen_palette_is_oracle();
	test_colours();
	test_bands_left_out_are_background();
	test_geometry();
	test_font_copies_are_identical();
	test_fit_is_sdl_view_open();
	TTF_Quit();
	SDL_Quit();
	printf("%s: passed\n", TEST);
	return 0;
}
