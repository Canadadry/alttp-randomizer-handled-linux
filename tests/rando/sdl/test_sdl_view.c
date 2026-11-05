#include "sdl_view.h"
#include "support.h"
#include "ui_grid.h"

#include <SDL/SDL_ttf.h>
#include <string.h>

#define TEST "test_sdl_view"
#define FONT "../../data/DejaVuSansMono.ttf"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static SdlView view;

static SDL_Surface *surface(int w, int h)
{
	SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);

	if (s == NULL)
		test_fail(TEST, "SDL_CreateRGBSurface: %s", SDL_GetError());
	return s;
}

static Uint32 pixel(SDL_Surface *s, int x, int y)
{
	return ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x];
}

static Uint32 band(SDL_Surface *s, UiLineStyle style)
{
	SdlRgb c = sdl_view_palette[style].band;

	return SDL_MapRGB(s->format, c.r, c.g, c.b);
}

static void open_default(void)
{
	SdlViewStatus st = sdl_view_open(&view, FONT, 640, 480, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES);

	if (st != SDLVIEW_OK)
		test_fail(TEST, "sdl_view_open: status %d (%s)", (int)st, TTF_GetError());
}

/* Each space cell is exactly its line's band colour; each other cell has a pixel off the band. */
static void check_cells(SDL_Surface *s, const UiGrid *g)
{
	int line, col, x, y;

	for (line = 0; line < g->lines; line++) {
		Uint32 b = band(s, g->styles[line]);

		for (col = 0; col < g->cols; col++) {
			int x0 = col * view.cell_w, y0 = line * view.cell_h, off = 0;

			for (y = y0; y < y0 + view.cell_h; y++)
				for (x = x0; x < x0 + view.cell_w; x++)
					off += pixel(s, x, y) != b;
			if (g->cells[line][col] == ' ' && off != 0)
				test_fail(TEST, "line %d col %d: space cell has %d pixels off its band (%06x, band %06x, style %d)", line, col, off, (unsigned)pixel(s, x0, y0), (unsigned)b, (int)g->styles[line]);
			if (g->cells[line][col] != ' ' && off == 0)
				test_fail(TEST, "line %d col %d: '%c' drew nothing", line, col, g->cells[line][col]);
		}
	}
}

static void fill_line(UiGrid *g, int line, UiLineStyle style, const char *text)
{
	size_t n = strlen(text);

	memset(g->cells[line], ' ', (size_t)g->cols);
	memcpy(g->cells[line], text, n < (size_t)g->cols ? n : (size_t)g->cols);
	g->styles[line] = style;
}

static void test_open_metrics(void)
{
	TTF_Font *f;
	int adv;

	open_default();
	CHECK(view.cell_w == 16 && view.cell_h == 32);
	f = TTF_OpenFont(FONT, view.point_size);
	CHECK(f != NULL);
	CHECK(TTF_GlyphMetrics(f, 'M', NULL, NULL, NULL, NULL, &adv) == 0);
	CHECK(adv <= view.cell_w && TTF_FontHeight(f) <= view.cell_h);
	TTF_CloseFont(f);
	f = TTF_OpenFont(FONT, view.point_size + 1);
	CHECK(f != NULL);
	CHECK(TTF_GlyphMetrics(f, 'M', NULL, NULL, NULL, NULL, &adv) == 0);
	CHECK(adv > view.cell_w || TTF_FontHeight(f) > view.cell_h);
	TTF_CloseFont(f);
	sdl_view_close(&view);
}

/* Every style, every printable character, on one hand-filled grid. */
static void test_every_style(void)
{
	SDL_Surface *s = surface(640, 480);
	UiGrid g;
	char chars[96];
	int i, line;

	for (i = 0; i < 95; i++)
		chars[i] = (char)(0x20 + i);
	chars[95] = '\0';
	open_default();
	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	for (line = 0; line < g.lines; line++) {
		const char *text = chars + (line * 7) % 60;

		fill_line(&g, line, (UiLineStyle)(line % UI_LINE_STYLE_COUNT), line % 3 == 0 ? "  < a  b >  ^ v ~ ?" : text);
	}
	sdl_view_draw(&view, &g, s);
	check_cells(s, &g);
	sdl_view_close(&view);
	SDL_FreeSurface(s);
}

static void test_rendered_screen(void)
{
	SDL_Surface *s = surface(640, 480);
	UiScreen sc;
	UiRow row;
	UiGrid g;
	int line, sel = -1, y;

	ui_screen_init(&sc);
	sc.tabs[0] = "Generate";
	sc.tabs[1] = "History";
	sc.tab_count = 2;
	sc.title = "Seed Settings";
	memset(&row, 0, sizeof(row));
	row.kind = UI_ROW_SETTING;
	row.label = "Glitches";
	row.value = "None";
	CHECK(ui_screen_add_row(&sc, row));
	row.label = "Goal";
	row.value = "Defeat Ganon";
	CHECK(ui_screen_add_row(&sc, row));
	row.disabled = true;
	CHECK(ui_screen_add_row(&sc, row));
	sc.cursor = 1;
	sc.message_kind = UI_MESSAGE_ERROR;
	sc.message = "Not supported";
	{
		UiHint hint = {UI_HINT_A, "A", "Select", false};

		CHECK(ui_screen_add_hint(&sc, hint));
	}
	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&g, &sc));
	open_default();
	sdl_view_draw(&view, &g, s);
	check_cells(s, &g);
	for (line = 0; line < g.lines; line++)
		if (g.styles[line] == UI_LINE_SELECTED)
			sel = line;
	CHECK(sel >= 0);
	for (y = sel * view.cell_h; y < (sel + 1) * view.cell_h; y++)
		CHECK(pixel(s, 0, y) == band(s, UI_LINE_SELECTED) && pixel(s, 639, y) == band(s, UI_LINE_SELECTED));
	CHECK(band(s, UI_LINE_SELECTED) != band(s, UI_LINE_BLANK));
	sdl_view_close(&view);
	SDL_FreeSurface(s);
}

/* Wide and low glyphs between blank lines: nothing leaks out of its cell. */
static void test_no_overflow(void)
{
	SDL_Surface *s = surface(640, 480);
	UiGrid g;
	int line;

	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	for (line = 4; line < 11; line += 3)
		fill_line(&g, line, UI_LINE_ROW, "W_W_W_W_W_W_W_W_W_W_W_W_W_W_W_W_W_W_W_W_");
	fill_line(&g, 7, UI_LINE_SELECTED, "W W W W W W W W W W W W W W W W W W W W ");
	open_default();
	sdl_view_draw(&view, &g, s);
	check_cells(s, &g);
	sdl_view_close(&view);
	SDL_FreeSurface(s);
}

static void test_twice_identical(void)
{
	SDL_Surface *a = surface(640, 480), *b = surface(640, 480);
	UiGrid g;

	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	fill_line(&g, 0, UI_LINE_TABS, " [Generate]  History");
	fill_line(&g, 5, UI_LINE_SELECTED, " Goal          < Defeat Ganon >");
	fill_line(&g, 14, UI_LINE_FOOTER, " A Select");
	open_default();
	sdl_view_draw(&view, &g, a);
	sdl_view_draw(&view, &g, b);
	CHECK(memcmp(a->pixels, b->pixels, (size_t)a->pitch * (size_t)a->h) == 0);
	sdl_view_close(&view);
	SDL_FreeSurface(a);
	SDL_FreeSurface(b);
}

/* A surface that leaves a remainder: the border outside the cells is the background. */
static void test_border(void)
{
	SDL_Surface *s = surface(650, 490);
	UiGrid g;
	Uint32 bg;
	int x, y;

	CHECK(sdl_view_open(&view, FONT, 650, 490, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES) == SDLVIEW_OK);
	CHECK(view.cell_w == 16 && view.cell_h == 32);
	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	fill_line(&g, 14, UI_LINE_FOOTER, "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
	sdl_view_draw(&view, &g, s);
	bg = band(s, UI_LINE_BLANK);
	for (y = 0; y < 490; y++)
		for (x = 0; x < 650; x++)
			if (x >= 640 || y >= 480)
				CHECK(pixel(s, x, y) == bg);
	check_cells(s, &g);
	sdl_view_close(&view);
	SDL_FreeSurface(s);
}

static void test_open_failures(void)
{
	CHECK(sdl_view_open(&view, FONT, 160, 120, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES) ==
	      SDLVIEW_FONT_TOO_SMALL);
	CHECK(sdl_view_open(&view, "../../data/no-such-font.ttf", 640, 480, UI_GRID_DEFAULT_COLS,
			    UI_GRID_DEFAULT_LINES) == SDLVIEW_FONT_OPEN);
}

static void test_reopen(void)
{
	int i;

	for (i = 0; i < 20; i++) {
		open_default();
		sdl_view_close(&view);
	}
}

int main(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		test_fail(TEST, "SDL_Init: %s", SDL_GetError());
	if (TTF_Init() != 0)
		test_fail(TEST, "TTF_Init: %s", TTF_GetError());
	test_open_metrics();
	test_every_style();
	test_rendered_screen();
	test_no_overflow();
	test_twice_identical();
	test_border();
	test_open_failures();
	test_reopen();
	TTF_Quit();
	SDL_Quit();
	printf("%s: passed\n", TEST);
	return 0;
}
