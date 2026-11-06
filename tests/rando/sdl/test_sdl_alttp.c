#include "rando_settings.h"
#include "sdl_render.h"
#include "support.h"
#include "theme.h"
#include "ui_settings.h"
#include "ui_strings.h"

#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_sdl_alttp"
#define ALTTP "../../themes/alttp"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static Theme theme;
static SdlRender render;

static void test_every_font_is_monospace_at_its_size(void)
{
	int i, c;

	for (i = 0; i < theme.font_count; i++) {
		int px = theme.fonts[i].size.px;

		CHECK(!theme.fonts[i].size.fit && !theme.fonts[i].cell.present);
		for (c = 0x20; c <= 0x7e; c++) {
			int advance = -1;

			if (TTF_GlyphMetrics(render.fonts[i], (Uint16)c, NULL, NULL, NULL, NULL, &advance) != 0)
				test_fail(TEST, "font.%s: no metrics for 0x%02x: %s", theme.fonts[i].name, c, TTF_GetError());
			if (advance != px)
				test_fail(TEST, "font.%s: 0x%02x advances %d px, not its size %d", theme.fonts[i].name, c,
					  advance, px);
		}
	}
	CHECK(sdl_render_advance(&render) == theme.fonts[theme.list.font].size.px);
}

static int width(const char *text)
{
	int w = 0;

	if (text[0] != '\0' && TTF_SizeText(render.fonts[theme.list.font], text, &w, NULL) != 0)
		test_fail(TEST, "TTF_SizeText \"%s\": %s", text, TTF_GetError());
	return w;
}

static int marker_width(const ThemeMarker *m)
{
	int i;

	if (m->kind == THEME_MARKER_TEXT)
		return width(m->text);
	if (m->kind == THEME_MARKER_NONE)
		return 0;
	for (i = 0; i < render.asset_count; i++)
		if (render.images[i] != NULL && strcmp(render.assets[i].file, m->file) == 0)
			return render.images[i]->w;
	test_fail(TEST, "no image for %s", m->file);
	return 0;
}

/* Mirrors the renderer's setting row: the value right-aligned to value_right, the left arrow before it. */
static void test_no_setting_row_is_cut(void)
{
	const UiStrTable *en = ui_str_english();
	const ThemeList *l = &theme.list;
	int arrow = marker_width(&l->arrow_left);
	int id, v;

	for (id = 0; id < RANDO_SETTING_COUNT; id++) {
		const char *label = ui_str(en, ui_setting_label((RandoSettingId)id));
		int label_w = width(label);

		for (v = 0; v < rando_setting_value_count((RandoSettingId)id); v++) {
			const char *value = ui_str(en, ui_setting_value((RandoSettingId)id, v));
			int value_w = width(value);
			int arrow_x = l->value_right - value_w - l->arrow_left_gap - arrow;

			if (value_w > l->value_max)
				test_fail(TEST, "\"%s\" is %d px, over list.value_max %d", value, value_w, l->value_max);
			if (l->label_x_cursor + label_w + l->label_gap > arrow_x ||
			    l->label_x + label_w + l->label_gap > arrow_x)
				test_fail(TEST, "\"%s\" with \"%s\": the label ends past %d", label, value,
					  arrow_x - l->label_gap);
		}
	}
}

int main(void)
{
	SdlRenderFailure f;
	ThemeError err;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		test_fail(TEST, "SDL_Init: %s", SDL_GetError());
	if (TTF_Init() != 0)
		test_fail(TEST, "TTF_Init: %s", TTF_GetError());
	if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
		test_fail(TEST, "IMG_Init: %s", IMG_GetError());
	if (!theme_load(&theme, ALTTP, &err))
		test_fail(TEST, "%s", err.message);
	if (!sdl_render_open(&render, &theme, ALTTP, &f))
		test_fail(TEST, "alttp: reason %d asset %d: %s", (int)f.reason, f.asset, f.error);
	test_every_font_is_monospace_at_its_size();
	test_no_setting_row_is_cut();
	sdl_render_close(&render);
	IMG_Quit();
	TTF_Quit();
	SDL_Quit();
	printf("%s: passed\n", TEST);
	return 0;
}
