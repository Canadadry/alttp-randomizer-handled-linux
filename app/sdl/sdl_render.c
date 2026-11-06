#include "sdl_render.h"

#include <SDL/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define POINT_MIN 8
#define POINT_MAX 64
#define PATH_BYTES 1024
#define TEXT_BYTES 1024
#define FLOW_PIECES (2 * UI_HINTS_MAX)

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

static Uint32 map(SDL_Surface *dst, ThemeColour c)
{
	return SDL_MapRGB(dst->format, c.r, c.g, c.b);
}

static void fill(SDL_Surface *dst, int x, int y, int w, int h, ThemeColour c)
{
	SDL_Rect rect;

	if (w <= 0 || h <= 0)
		return;
	rect.x = (Sint16)x;
	rect.y = (Sint16)y;
	rect.w = (Uint16)w;
	rect.h = (Uint16)h;
	SDL_FillRect(dst, &rect, map(dst, c));
}

static void band(SDL_Surface *dst, const ThemeBand *b, int y)
{
	fill(dst, b->x, y, b->w, b->h, b->colour);
	fill(dst, b->x, y, b->w, b->top_line, b->line_colour);
	fill(dst, b->x, y + b->h - b->bottom_line, b->w, b->bottom_line, b->line_colour);
}

static void blit(SDL_Surface *src, SDL_Surface *dst, int x, int y)
{
	SDL_Rect at;

	at.x = (Sint16)x;
	at.y = (Sint16)y;
	SDL_BlitSurface(src, NULL, dst, &at);
}

/* theme_assets lists every sprite marker and button, so every sprite file was loaded. */
static SDL_Surface *sprite(const SdlRender *r, const char *file)
{
	int i;

	for (i = 0; i < r->asset_count; i++)
		if (r->assets[i].kind == THEME_ASSET_SPRITE && strcmp(r->assets[i].file, file) == 0)
			return r->images[i];
	assert(false);
	return NULL;
}

static const ThemeFont *font_of(const SdlRender *r, int font)
{
	assert(font >= 0 && font < r->theme->font_count);
	return &r->theme->fonts[font];
}

static int font_top(const SdlRender *r, int font, int y)
{
	return font_of(r, font)->anchor == THEME_ANCHOR_BASELINE ? y - TTF_FontAscent(r->fonts[font]) : y;
}

static SDL_Color sdl_colour(ThemeColour c)
{
	SDL_Color out;

	out.r = c.r;
	out.g = c.g;
	out.b = c.b;
	out.unused = 0;
	return out;
}

/* NULL when SDL_ttf fails. *owned says the caller frees it: the glyph of a colour past the cache. */
static SDL_Surface *glyph(SdlRender *r, int font, ThemeColour c, char ch, bool *owned)
{
	int i = (unsigned char)ch - 0x20, slot;
	char text[2];

	text[0] = ch;
	text[1] = '\0';
	*owned = false;
	assert(i >= 0 && i < SDL_RENDER_GLYPHS);
	for (slot = 0; slot < r->colour_count[font]; slot++)
		if (memcmp(&r->colours[font][slot], &c, sizeof c) == 0)
			break;
	if (slot == r->colour_count[font]) {
		if (slot == SDL_RENDER_COLOURS) {
			*owned = true;
			return TTF_RenderText_Blended(r->fonts[font], text, sdl_colour(c));
		}
		r->colours[font][slot] = c;
		r->colour_count[font]++;
	}
	if (r->glyphs[font][slot][i] == NULL && !r->glyph_failed[font][slot][i]) {
		r->glyphs[font][slot][i] = TTF_RenderText_Blended(r->fonts[font], text, sdl_colour(c));
		r->glyph_failed[font][slot][i] = r->glyphs[font][slot][i] == NULL;
	}
	return r->glyphs[font][slot][i];
}

/* text is printable ASCII; each byte in its own cell, centred and clipped to it. */
static void put_cells(SdlRender *r, SDL_Surface *dst, int font, int x, int top, const char *text, ThemeColour c)
{
	const ThemeCell *cell = &font_of(r, font)->cell;
	int i;

	for (i = 0; text[i] != '\0'; i++) {
		SDL_Rect clip, at;
		SDL_Surface *g;
		bool owned;

		if (text[i] == ' ' || (g = glyph(r, font, c, text[i], &owned)) == NULL)
			continue;
		clip.x = (Sint16)(x + i * cell->w);
		clip.y = (Sint16)top;
		clip.w = (Uint16)cell->w;
		clip.h = (Uint16)cell->h;
		at = clip;
		if (g->w < cell->w)
			at.x = (Sint16)(at.x + (cell->w - g->w) / 2);
		SDL_SetClipRect(dst, &clip);
		SDL_BlitSurface(g, NULL, dst, &at);
		if (owned)
			SDL_FreeSurface(g);
	}
	SDL_SetClipRect(dst, NULL);
}

/* text is printable ASCII. */
static int measure(const SdlRender *r, int font, const char *text)
{
	const ThemeFont *tf = font_of(r, font);
	int w = 0;

	if (tf->cell.present)
		return (int)strlen(text) * tf->cell.w;
	if (text[0] == '\0' || TTF_SizeText(r->fonts[font], text, &w, NULL) != 0)
		return 0;
	return w;
}

/* text is printable ASCII, y the zone's y before the anchor. */
static void put_plain(SdlRender *r, SDL_Surface *dst, int font, int x, int y, const char *text, ThemeColour c)
{
	SDL_Surface *s;

	if (text[0] == '\0')
		return;
	if (font_of(r, font)->cell.present) {
		put_cells(r, dst, font, x, font_top(r, font, y), text, c);
		return;
	}
	s = TTF_RenderText_Blended(r->fonts[font], text, sdl_colour(c));
	if (s == NULL)
		return;
	blit(s, dst, x, font_top(r, font, y));
	SDL_FreeSurface(s);
}

/* Bytes outside 0x20-0x7E become '?', as the grid's pen does; NULL is "". */
static void printable(char out[TEXT_BYTES], const char *text)
{
	size_t i;

	for (i = 0; text != NULL && text[i] != '\0' && i < TEXT_BYTES - 1; i++)
		out[i] = (unsigned char)text[i] >= 0x20 && (unsigned char)text[i] <= 0x7E ? text[i] : '?';
	out[i] = '\0';
}

static const char *cut_mark(const SdlRender *r)
{
	return r->theme->screen.cut.kind == THEME_MARKER_TEXT ? r->theme->screen.cut.text : "";
}

/* Cuts printable s in place so that it, and the cut mark when *cut, end within room px; returns the
   width of both. A room too small for the mark leaves s empty. */
static int fit(const SdlRender *r, int font, char *s, int room, bool *cut)
{
	const char *mark = cut_mark(r);
	int w = measure(r, font, s), mark_w, len;

	*cut = false;
	if (w <= room)
		return w;
	mark_w = measure(r, font, mark);
	if (room - mark_w < 0) {
		s[0] = '\0';
		return 0;
	}
	len = (int)strlen(s);
	if (font_of(r, font)->cell.present) {
		len = (room - mark_w) / font_of(r, font)->cell.w;
	} else {
		do
			s[--len] = '\0';
		while (len > 0 && measure(r, font, s) + mark_w > room);
	}
	s[len] = '\0';
	*cut = true;
	return measure(r, font, s) + mark_w;
}

static void put_fitted(SdlRender *r, SDL_Surface *dst, int font, int x, int y, const char *s, bool cut,
		       ThemeColour c)
{
	put_plain(r, dst, font, x, y, s, c);
	if (cut)
		put_plain(r, dst, font, x + measure(r, font, s), y, cut_mark(r), c);
}

/* Draws text from x, cut so that it and the cut mark end within room px; returns the drawn end. */
static int put_text(SdlRender *r, SDL_Surface *dst, int font, int x, int y, int room, const char *text,
		    ThemeColour c)
{
	char s[TEXT_BYTES];
	bool cut;
	int w;

	printable(s, text);
	w = fit(r, font, s, room, &cut);
	put_fitted(r, dst, font, x, y, s, cut, c);
	return x + w;
}

static int marker_width(const SdlRender *r, int font, const ThemeMarker *m)
{
	if (m->kind == THEME_MARKER_SPRITE)
		return sprite(r, m->file)->w;
	return m->kind == THEME_MARKER_TEXT ? measure(r, font, m->text) : 0;
}

/* A text marker's y goes through the font's anchor; a sprite's is its top. */
static int put_marker(SdlRender *r, SDL_Surface *dst, int font, const ThemeMarker *m, int x, int y, ThemeColour c)
{
	if (m->kind == THEME_MARKER_SPRITE)
		blit(sprite(r, m->file), dst, x, y);
	else if (m->kind == THEME_MARKER_TEXT)
		put_plain(r, dst, font, x, y, m->text, c);
	return x + marker_width(r, font, m);
}

static void draw_tabs(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	const ThemeTabs *t = &r->theme->tabs;
	int i;

	for (i = 0; i < screen->tab_count; i++) {
		bool active = i == screen->active_tab;
		int x = active ? t->slots[i].active_x : t->slots[i].x;
		ThemeColour c = active ? t->active_colour : t->colour;
		int end = put_text(r, dst, t->font, x, t->y, t->right - x, screen->tabs[i], c);

		if (!active)
			continue;
		put_marker(r, dst, t->font, &t->active_before, t->slots[i].marker_x, t->marker_y, c);
		put_marker(r, dst, t->font, &t->active_after, end, t->marker_y, c);
	}
}

static void draw_title(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	const ThemeTitle *t = &r->theme->title;
	const ThemeRule *rule = &t->rule;
	int end, x;

	if (screen->title == NULL || screen->title[0] == '\0')
		return;
	end = put_text(r, dst, t->font, t->x, t->y, t->right - t->x, screen->title,
		       screen->title_tone == UI_TITLE_ALERT ? t->alert_colour : t->colour);
	if (!rule->present)
		return;
	for (x = end + rule->offset; x < rule->end; x += rule->dash + rule->gap)
		fill(dst, x, rule->y, rule->dash < rule->end - x ? rule->dash : rule->end - x, rule->thickness,
		     rule->colour);
}

typedef struct RowColours {
	ThemeColour label, value;
} RowColours;

static RowColours row_colours(const ThemeList *l, const UiRow *row, bool selected)
{
	RowColours c;

	if (row->disabled)
		c.label = selected ? l->selected_disabled_colour : l->disabled_colour;
	else if (selected)
		c.label = l->selected_colour;
	else
		c.label = row->kind == UI_ROW_TEXT ? l->text_colour : l->label_colour;
	c.value = row->disabled || selected || row->kind == UI_ROW_TEXT ? c.label : l->value_colour;
	return c;
}

static bool row_visible(const UiScreen *screen, int index, int page)
{
	return index >= screen->scroll && index < screen->scroll + page && index < screen->row_count;
}

static void draw_selected_band(const SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	const ThemeList *l = &r->theme->list;
	const ThemeBand *b;

	if (screen->body_kind != UI_BODY_LIST || screen->cursor < 0 ||
	    !row_visible(screen, screen->cursor, theme_page(r->theme)))
		return;
	b = screen->rows[screen->cursor].disabled ? &l->selected_disabled : &l->selected;
	if (b->present)
		band(dst, b, l->top + (screen->cursor - screen->scroll) * l->row_height + b->y);
}

/* The value right-aligned to value_right, its arrows around it; the label cut before the left arrow. */
static void draw_setting(SdlRender *r, SDL_Surface *dst, const UiRow *row, int top, int label_x, bool arrows,
			 RowColours c)
{
	const ThemeList *l = &r->theme->list;
	char value[TEXT_BYTES];
	bool cut;
	int w, value_x, arrow_x;

	printable(value, row->value);
	w = fit(r, l->font, value, l->value_max, &cut);
	value_x = l->value_right - w;
	arrow_x = value_x - l->arrow_left_gap - marker_width(r, l->font, &l->arrow_left);
	put_text(r, dst, l->font, label_x, top + l->text_y, arrow_x - l->label_gap - label_x, row->label, c.label);
	put_fitted(r, dst, l->font, value_x, top + l->text_y, value, cut, c.value);
	if (!arrows)
		return;
	put_marker(r, dst, l->font, &l->arrow_left, arrow_x, top + l->arrow_y, c.value);
	put_marker(r, dst, l->font, &l->arrow_right, l->value_right + l->arrow_right_gap, top + l->arrow_y, c.value);
}

static void draw_list(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	const ThemeList *l = &r->theme->list;
	int page = theme_page(r->theme), label_x = screen->cursor >= 0 ? l->label_x_cursor : l->label_x, i;
	int last = l->top + (page - 1) * l->row_height;
	ThemeColour first_colour = l->label_colour, last_colour = l->label_colour;

	for (i = 0; i < page && screen->scroll + i < screen->row_count; i++) {
		int index = screen->scroll + i, top = l->top + i * l->row_height;
		const UiRow *row = &screen->rows[index];
		bool selected = index == screen->cursor;
		RowColours c = row_colours(l, row, selected);

		if (i == 0)
			first_colour = c.label;
		if (i == page - 1)
			last_colour = c.label;
		if (selected)
			put_marker(r, dst, l->font, row->disabled ? &l->cursor_disabled : &l->cursor, l->cursor_x,
				   top + l->cursor_y, c.label);
		if (row->kind == UI_ROW_SETTING)
			draw_setting(r, dst, row, top, label_x, selected && !row->disabled, c);
		else
			put_text(r, dst, l->font, label_x, top + l->text_y, l->right - label_x, row->label, c.label);
	}
	if (screen->scroll > 0 || screen->more_above)
		put_marker(r, dst, l->font, &l->scroll_up, l->scroll_x, l->top + l->scroll_y, first_colour);
	if (screen->scroll + page < screen->row_count || screen->more_below)
		put_marker(r, dst, l->font, &l->scroll_down, l->scroll_x, last + l->scroll_y, last_colour);
}

static void draw_notice(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	const ThemeNotice *n = &r->theme->notice;
	int page = theme_page(r->theme), i;

	for (i = 0; i < page && screen->scroll + i < screen->row_count; i++) {
		const UiRow *row = &screen->rows[screen->scroll + i];
		int x = row->emphasis ? n->emphasis_x : n->x;

		put_text(r, dst, n->font, x, n->top + i * n->row_height + n->text_y, n->right - x, row->label,
			 row->emphasis ? n->emphasis_colour : n->colour);
	}
}

static void draw_message(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	const ThemeMessage *m = &r->theme->message;
	const ThemeHelp *h = &r->theme->help;

	if (screen->message_kind == UI_MESSAGE_NONE)
		return;
	if (screen->message_kind == UI_MESSAGE_HELP) {
		if (h->present)
			put_text(r, dst, h->font, h->x, h->y, h->right - h->x, screen->message, h->colour);
		return;
	}
	put_text(r, dst, m->font, m->x, m->y, m->right - m->x, screen->message,
		 screen->message_kind == UI_MESSAGE_ERROR ? m->error_colour : m->info_colour);
}

/* A key or a label of the footer's flow: text, or the button's sprite when image is set. */
typedef struct Piece {
	int x, w;
	char text[TEXT_BYTES];
	SDL_Surface *image;
	ThemeColour colour;
} Piece;

static int flow(const SdlRender *r, const UiScreen *screen, Piece pieces[FLOW_PIECES])
{
	const ThemeFooter *f = &r->theme->footer;
	int x = f->x, n = 0, i;

	for (i = 0; i < screen->hint_count; i++) {
		const UiHint *h = &screen->hints[i];
		const ThemeButton *b = &f->buttons[h->button];
		ThemeColour c = h->primary ? f->primary_colour : f->label_colour;
		Piece *key = &pieces[n++], *label = &pieces[n++];

		if (i > 0)
			x += f->hint_gap;
		key->x = x;
		key->colour = c;
		key->image = b->kind == THEME_BUTTON_SPRITE ? sprite(r, b->file) : NULL;
		printable(key->text, key->image != NULL ? NULL : h->key);
		key->w = key->image != NULL ? key->image->w : measure(r, f->font, key->text);
		x += key->w + f->key_gap;
		label->x = x;
		label->colour = c;
		label->image = NULL;
		printable(label->text, h->label);
		label->w = measure(r, f->font, label->text);
		x += label->w;
	}
	return n;
}

static void put_piece(SdlRender *r, SDL_Surface *dst, const Piece *p)
{
	const ThemeFooter *f = &r->theme->footer;

	if (p->image != NULL)
		blit(p->image, dst, p->x, f->key_y);
	else
		put_plain(r, dst, f->font, p->x, f->y, p->text, p->colour);
}

/* A cell font's flow: nothing is drawn from the mark's cell on, a gap included, as the grid's pen. */
static void cut_cells(SdlRender *r, SDL_Surface *dst, Piece *pieces, int n)
{
	const ThemeFooter *f = &r->theme->footer;
	int w = font_of(r, f->font)->cell.w;
	int room = f->right - f->x - measure(r, f->font, cut_mark(r));
	int cut, i;
	ThemeColour c;

	if (room < 0)
		return;
	cut = f->x + room / w * w;
	c = pieces[0].colour;
	for (i = 0; i < n; i++) {
		Piece *p = &pieces[i];

		if (p->x > cut)
			break;
		c = p->colour;
		if (p->image != NULL) {
			if (p->x + p->w <= cut)
				put_piece(r, dst, p);
			continue;
		}
		if (p->x + p->w > cut)
			p->text[(cut - p->x) / w] = '\0';
		put_piece(r, dst, p);
	}
	put_plain(r, dst, f->font, cut, f->y, cut_mark(r), c);
}

/* A run font's flow: the overflowing piece is cut as a run, the later ones dropped. */
static void cut_run(SdlRender *r, SDL_Surface *dst, const Piece *pieces, int n)
{
	const ThemeFooter *f = &r->theme->footer;
	const char *mark = cut_mark(r);
	int i;

	for (i = 0; i < n; i++) {
		const Piece *p = &pieces[i];

		if (p->x + p->w <= f->right) {
			put_piece(r, dst, p);
			continue;
		}
		if (p->image == NULL)
			put_text(r, dst, f->font, p->x, f->y, f->right - p->x, p->text, p->colour);
		else if (p->x + measure(r, f->font, mark) <= f->right)
			put_plain(r, dst, f->font, p->x, f->y, mark, p->colour);
		return;
	}
}

static void draw_footer(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	static Piece pieces[FLOW_PIECES];
	const ThemeFooter *f = &r->theme->footer;
	int n = flow(r, screen, pieces), i;

	if (n == 0)
		return;
	if (pieces[n - 1].x + pieces[n - 1].w <= f->right) {
		for (i = 0; i < n; i++)
			put_piece(r, dst, &pieces[i]);
	} else if (font_of(r, f->font)->cell.present) {
		cut_cells(r, dst, pieces, n);
	} else {
		cut_run(r, dst, pieces, n);
	}
}

bool sdl_render_draw(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)
{
	const Theme *t;

	assert(r != NULL && r->theme != NULL && screen != NULL && dst != NULL);
	assert(ui_screen_valid(screen));
	assert(dst->w == THEME_SCREEN_W && dst->h == THEME_SCREEN_H && dst->format->BytesPerPixel == 4 &&
	       dst->format->Rmask == 0x00FF0000 && dst->format->Gmask == 0x0000FF00 &&
	       dst->format->Bmask == 0x000000FF);
	t = r->theme;
	if (screen->tab_count > t->tabs.slot_count)
		return false;
	SDL_SetClipRect(dst, NULL);
	SDL_FillRect(dst, NULL, map(dst, t->screen.colour));
	if (t->screen.image.present) {
		assert(r->assets[0].kind == THEME_ASSET_SCREEN);
		blit(r->images[0], dst, 0, 0);
	}
	if (screen->tab_count > 0 && t->tabs.band.present)
		band(dst, &t->tabs.band, t->tabs.band.y);
	if (screen->hint_count > 0 && t->footer.band.present)
		band(dst, &t->footer.band, t->footer.band.y);
	draw_selected_band(r, screen, dst);
	draw_tabs(r, screen, dst);
	draw_title(r, screen, dst);
	if (screen->body_kind == UI_BODY_LIST)
		draw_list(r, screen, dst);
	else
		draw_notice(r, screen, dst);
	draw_message(r, screen, dst);
	draw_footer(r, screen, dst);
	return true;
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
	for (i = 0; i < THEME_FONTS_MAX; i++) {
		int c, g;

		for (c = 0; c < SDL_RENDER_COLOURS; c++)
			for (g = 0; g < SDL_RENDER_GLYPHS; g++)
				if (r->glyphs[i][c][g] != NULL)
					SDL_FreeSurface(r->glyphs[i][c][g]);
	}
	memset(r, 0, sizeof *r);
}
