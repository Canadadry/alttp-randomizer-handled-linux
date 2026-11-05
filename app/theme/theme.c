#include "theme.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SECTION_KEYS_MAX 40
#define SECTIONS_MAX 64
#define SECTION_NAME_MAX 32
/* One per section with a font key, each section at most once. */
#define FONT_REFS_MAX 8
#define INT_SATURATE 1000000
#define PATH_MAX_BYTES 1024

typedef enum KeyType {
	KEY_INT,
	KEY_COLOUR,
	KEY_IMAGE,
	KEY_TTF,
	KEY_CUT,
	KEY_MARKER,
	KEY_BUTTON,
	KEY_FONT,
	KEY_SIZE,
	KEY_CELL,
	KEY_ANCHOR
} KeyType;

typedef struct KeySpec {
	const char *name;
	KeyType type;
	size_t offset;
	int min, max;
} KeySpec;

typedef enum SectionKind {
	SECTION_SCREEN,
	SECTION_FONT,
	SECTION_TABS,
	SECTION_TABS_BAND,
	SECTION_TAB,
	SECTION_TITLE,
	SECTION_TITLE_RULE,
	SECTION_LIST,
	SECTION_LIST_SELECTED,
	SECTION_LIST_SELECTED_DISABLED,
	SECTION_NOTICE,
	SECTION_MESSAGE,
	SECTION_FOOTER,
	SECTION_FOOTER_BAND,
	SECTION_KIND_COUNT
} SectionKind;

typedef enum SectionArity {
	ARITY_REQUIRED,
	ARITY_OPTIONAL,
	ARITY_NAMED,
	ARITY_NUMBERED
} SectionArity;

/* missing is the name the "missing" message gives a required section. */
typedef struct SectionSpec {
	const char *name;
	SectionArity arity;
	const char *missing;
	const KeySpec *keys;
	int nkeys;
} SectionSpec;

typedef struct Section {
	SectionKind kind;
	char name[SECTION_NAME_MAX];
	void *base;
	int number;
	int line;
	bool seen[SECTION_KEYS_MAX];
	int key_line[SECTION_KEYS_MAX];
} Section;

typedef struct FontRef {
	int *index;
	const Section *section;
	const char *key;
	int line;
	char name[THEME_LINE_MAX + 1];
} FontRef;

typedef struct Parser {
	Theme *out;
	const char *file;
	ThemeError *err;
	int line;
	Section sections[SECTIONS_MAX];
	int nsections;
	Section *current;
	ThemeFont font_overflow;
	char font_overflow_name[THEME_NAME_MAX];
	ThemeTabSlot tab_overflow;
	bool tab_overflow_seen;
	FontRef refs[FONT_REFS_MAX];
	int nrefs;
} Parser;

#define X 0, THEME_SCREEN_W
#define Y 0, THEME_SCREEN_H
#define R -128, 128
#define L 0, THEME_SCREEN_W
#define NO_RANGE 0, 0

#define INT(s, f, ...) { #f, KEY_INT, offsetof(s, f), __VA_ARGS__ }
#define TYPED(s, f, type) { #f, type, offsetof(s, f), NO_RANGE }

static const KeySpec screen_keys[] = {
	TYPED(ThemeScreen, colour, KEY_COLOUR),
	TYPED(ThemeScreen, image, KEY_IMAGE),
	TYPED(ThemeScreen, cut, KEY_CUT),
};

static const KeySpec font_keys[] = {
	TYPED(ThemeFont, file, KEY_TTF),
	TYPED(ThemeFont, size, KEY_SIZE),
	TYPED(ThemeFont, cell, KEY_CELL),
	TYPED(ThemeFont, anchor, KEY_ANCHOR),
};

static const KeySpec tabs_keys[] = {
	TYPED(ThemeTabs, font, KEY_FONT),
	INT(ThemeTabs, y, Y),
	INT(ThemeTabs, right, X),
	TYPED(ThemeTabs, colour, KEY_COLOUR),
	TYPED(ThemeTabs, active_colour, KEY_COLOUR),
	TYPED(ThemeTabs, active_before, KEY_MARKER),
	TYPED(ThemeTabs, active_after, KEY_MARKER),
	INT(ThemeTabs, marker_y, Y),
};

#define BAND_KEYS(y_range) \
	INT(ThemeBand, x, X), \
	INT(ThemeBand, y, y_range), \
	INT(ThemeBand, w, 1, THEME_SCREEN_W), \
	INT(ThemeBand, h, 1, THEME_SCREEN_H), \
	TYPED(ThemeBand, colour, KEY_COLOUR), \
	INT(ThemeBand, top_line, 0, 16), \
	INT(ThemeBand, bottom_line, 0, 16), \
	TYPED(ThemeBand, line_colour, KEY_COLOUR)

#define ROW_Y 0, 240

static const KeySpec band_keys[] = { BAND_KEYS(Y) };
static const KeySpec row_band_keys[] = { BAND_KEYS(ROW_Y) };

static const KeySpec tab_keys[] = {
	INT(ThemeTabSlot, x, X),
	INT(ThemeTabSlot, active_x, X),
	INT(ThemeTabSlot, marker_x, X),
};

static const KeySpec title_keys[] = {
	TYPED(ThemeTitle, font, KEY_FONT),
	INT(ThemeTitle, x, X),
	INT(ThemeTitle, y, Y),
	INT(ThemeTitle, right, X),
	TYPED(ThemeTitle, colour, KEY_COLOUR),
	TYPED(ThemeTitle, alert_colour, KEY_COLOUR),
};

static const KeySpec rule_keys[] = {
	TYPED(ThemeRule, colour, KEY_COLOUR),
	INT(ThemeRule, y, Y),
	INT(ThemeRule, thickness, 1, 16),
	INT(ThemeRule, dash, 1, 64),
	INT(ThemeRule, gap, 0, 64),
	INT(ThemeRule, offset, L),
	INT(ThemeRule, end, X),
};

static const KeySpec list_keys[] = {
	TYPED(ThemeList, font, KEY_FONT),
	INT(ThemeList, top, Y),
	INT(ThemeList, bottom, Y),
	INT(ThemeList, row_height, 8, 240),
	INT(ThemeList, text_y, R),
	INT(ThemeList, label_x, X),
	INT(ThemeList, label_x_cursor, X),
	INT(ThemeList, right, X),
	INT(ThemeList, value_right, X),
	INT(ThemeList, value_max, L),
	INT(ThemeList, label_gap, L),
	INT(ThemeList, arrow_left_gap, L),
	INT(ThemeList, arrow_right_gap, L),
	TYPED(ThemeList, label_colour, KEY_COLOUR),
	TYPED(ThemeList, value_colour, KEY_COLOUR),
	TYPED(ThemeList, text_colour, KEY_COLOUR),
	TYPED(ThemeList, disabled_colour, KEY_COLOUR),
	TYPED(ThemeList, selected_colour, KEY_COLOUR),
	TYPED(ThemeList, selected_disabled_colour, KEY_COLOUR),
	TYPED(ThemeList, cursor, KEY_MARKER),
	TYPED(ThemeList, cursor_disabled, KEY_MARKER),
	TYPED(ThemeList, arrow_left, KEY_MARKER),
	TYPED(ThemeList, arrow_right, KEY_MARKER),
	TYPED(ThemeList, scroll_up, KEY_MARKER),
	TYPED(ThemeList, scroll_down, KEY_MARKER),
	INT(ThemeList, cursor_x, X),
	INT(ThemeList, scroll_x, X),
	INT(ThemeList, cursor_y, R),
	INT(ThemeList, arrow_y, R),
	INT(ThemeList, scroll_y, R),
};

static const KeySpec notice_keys[] = {
	TYPED(ThemeNotice, font, KEY_FONT),
	INT(ThemeNotice, x, X),
	INT(ThemeNotice, emphasis_x, X),
	INT(ThemeNotice, right, X),
	INT(ThemeNotice, top, Y),
	INT(ThemeNotice, row_height, 8, 240),
	INT(ThemeNotice, text_y, R),
	TYPED(ThemeNotice, colour, KEY_COLOUR),
	TYPED(ThemeNotice, emphasis_colour, KEY_COLOUR),
};

static const KeySpec message_keys[] = {
	TYPED(ThemeMessage, font, KEY_FONT),
	INT(ThemeMessage, x, X),
	INT(ThemeMessage, y, Y),
	INT(ThemeMessage, right, X),
	TYPED(ThemeMessage, info_colour, KEY_COLOUR),
	TYPED(ThemeMessage, error_colour, KEY_COLOUR),
};

#define BUTTON(key, index) \
	{ "button." key, KEY_BUTTON, offsetof(ThemeFooter, buttons) + (index) * sizeof(ThemeButton), NO_RANGE }

static const KeySpec footer_keys[] = {
	TYPED(ThemeFooter, font, KEY_FONT),
	INT(ThemeFooter, x, X),
	INT(ThemeFooter, y, Y),
	INT(ThemeFooter, key_y, Y),
	INT(ThemeFooter, right, X),
	INT(ThemeFooter, key_gap, L),
	INT(ThemeFooter, hint_gap, L),
	TYPED(ThemeFooter, label_colour, KEY_COLOUR),
	TYPED(ThemeFooter, primary_colour, KEY_COLOUR),
	BUTTON("left_right", THEME_BUTTON_LEFT_RIGHT),
	BUTTON("a", THEME_BUTTON_A),
	BUTTON("b", THEME_BUTTON_B),
	BUTTON("x", THEME_BUTTON_X),
	BUTTON("y", THEME_BUTTON_Y),
	BUTTON("start", THEME_BUTTON_START),
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define KEYS(a) a, COUNT(a)

/* In the format's table order, which is the order missing keys and sections are reported in. */
static const SectionSpec section_specs[SECTION_KIND_COUNT] = {
	[SECTION_SCREEN] = { "screen", ARITY_REQUIRED, "screen", KEYS(screen_keys) },
	[SECTION_FONT] = { "font", ARITY_NAMED, "font.<name>", KEYS(font_keys) },
	[SECTION_TABS] = { "tabs", ARITY_REQUIRED, "tabs", KEYS(tabs_keys) },
	[SECTION_TABS_BAND] = { "tabs.band", ARITY_OPTIONAL, NULL, KEYS(band_keys) },
	[SECTION_TAB] = { "tab", ARITY_NUMBERED, "tab.<n>", KEYS(tab_keys) },
	[SECTION_TITLE] = { "title", ARITY_REQUIRED, "title", KEYS(title_keys) },
	[SECTION_TITLE_RULE] = { "title.rule", ARITY_OPTIONAL, NULL, KEYS(rule_keys) },
	[SECTION_LIST] = { "list", ARITY_REQUIRED, "list", KEYS(list_keys) },
	[SECTION_LIST_SELECTED] = { "list.selected", ARITY_REQUIRED, "list.selected", KEYS(row_band_keys) },
	[SECTION_LIST_SELECTED_DISABLED] = { "list.selected_disabled", ARITY_REQUIRED, "list.selected_disabled",
					     KEYS(row_band_keys) },
	[SECTION_NOTICE] = { "notice", ARITY_REQUIRED, "notice", KEYS(notice_keys) },
	[SECTION_MESSAGE] = { "message", ARITY_REQUIRED, "message", KEYS(message_keys) },
	[SECTION_FOOTER] = { "footer", ARITY_REQUIRED, "footer", KEYS(footer_keys) },
	[SECTION_FOOTER_BAND] = { "footer.band", ARITY_OPTIONAL, NULL, KEYS(band_keys) },
};

static bool fail(Parser *p, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(p->err->message, sizeof p->err->message, fmt, ap);
	va_end(ap);
	return false;
}

static char *trim(char *s)
{
	char *end;

	while (*s == ' ')
		s++;
	end = s + strlen(s);
	while (end > s && end[-1] == ' ')
		end--;
	*end = '\0';
	return s;
}

static bool is_name(const char *s)
{
	size_t n = strlen(s);
	size_t i;

	if (n < 1 || n >= THEME_NAME_MAX)
		return false;
	for (i = 0; i < n; i++)
		if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_'))
			return false;
	return true;
}

/* A tab number: decimal digits without a leading zero ("0" itself is a number). */
static bool is_number(const char *s)
{
	size_t n = strlen(s);
	size_t i;

	if (n < 1 || n > 6 || (s[0] == '0' && n > 1))
		return false;
	for (i = 0; i < n; i++)
		if (s[i] < '0' || s[i] > '9')
			return false;
	return true;
}

static bool is_file(const char *s, const char *ext)
{
	size_t n = strlen(s);
	size_t e = strlen(ext);
	size_t i;

	if (n < 1 || n >= THEME_FILE_MAX || s[0] == '.' || strstr(s, "..") || n <= e || strcmp(s + n - e, ext) != 0)
		return false;
	for (i = 0; i < n; i++) {
		char c = s[i];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
		      c == '.' || c == '-'))
			return false;
	}
	return true;
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* Values beyond INT_SATURATE are clamped to it, so any range check still refuses them. */
static bool parse_int(const char *s, int *out)
{
	bool negative = false;
	long v = 0;

	if (*s == '-') {
		negative = true;
		s++;
	}
	if (!*s)
		return false;
	for (; *s; s++) {
		if (*s < '0' || *s > '9')
			return false;
		if (v < INT_SATURATE)
			v = v * 10 + (*s - '0');
	}
	if (v > INT_SATURATE)
		v = INT_SATURATE;
	*out = (int)(negative ? -v : v);
	return true;
}

static bool key_fail_at(Parser *p, const Section *s, int line, const char *key, const char *problem)
{
	return fail(p, "%s:%d: %s.%s: %s", p->file, line, s->name, key, problem);
}

static bool key_fail(Parser *p, const char *key, const char *problem)
{
	return key_fail_at(p, p->current, p->line, key, problem);
}

static bool range_fail_at(Parser *p, const Section *s, int line, const char *key, int min, int max)
{
	return fail(p, "%s:%d: %s.%s: out of range %d..%d", p->file, line, s->name, key, min, max);
}

static bool set_int(Parser *p, const char *key, const char *s, int min, int max, int *out)
{
	if (!parse_int(s, out))
		return key_fail(p, key, "not an integer");
	if (*out < min || *out > max)
		return range_fail_at(p, p->current, p->line, key, min, max);
	return true;
}

static bool set_marker(Parser *p, const char *key, const char *s, bool sprite_allowed, ThemeMarker *m)
{
	const char *problem =
		sprite_allowed ? "not a marker (none, text:<chars>, sprite:<file.png>)" : "not a marker (none, text:<chars>)";

	memset(m, 0, sizeof *m);
	if (strcmp(s, "none") == 0) {
		m->kind = THEME_MARKER_NONE;
		return true;
	}
	if (strncmp(s, "text:", 5) == 0) {
		const char *t = s + 5;
		size_t n = strlen(t);

		if (n < 1 || n >= THEME_MARKER_TEXT_MAX || strchr(t, ' '))
			return key_fail(p, key, problem);
		m->kind = THEME_MARKER_TEXT;
		memcpy(m->text, t, n + 1);
		return true;
	}
	if (sprite_allowed && strncmp(s, "sprite:", 7) == 0 && is_file(s + 7, ".png")) {
		m->kind = THEME_MARKER_SPRITE;
		memcpy(m->file, s + 7, strlen(s + 7) + 1);
		return true;
	}
	return key_fail(p, key, problem);
}

static bool set_button(Parser *p, const char *key, const char *s, ThemeButton *b)
{
	memset(b, 0, sizeof *b);
	if (strcmp(s, "hint_text") == 0) {
		b->kind = THEME_BUTTON_HINT_TEXT;
		return true;
	}
	if (strncmp(s, "sprite:", 7) == 0 && is_file(s + 7, ".png")) {
		b->kind = THEME_BUTTON_SPRITE;
		memcpy(b->file, s + 7, strlen(s + 7) + 1);
		return true;
	}
	return key_fail(p, key, "not hint_text or sprite:<file.png>");
}

static bool set_cell(Parser *p, const char *key, char *s, ThemeCell *cell)
{
	char *w;
	char *h;

	memset(cell, 0, sizeof *cell);
	if (strcmp(s, "none") == 0)
		return true;
	w = s;
	h = strchr(s, ' ');
	if (!h)
		return key_fail(p, key, "not an integer");
	*h = '\0';
	h = trim(h + 1);
	if (!set_int(p, key, w, 4, 64, &cell->w) || !set_int(p, key, h, 4, 128, &cell->h))
		return false;
	cell->present = true;
	return true;
}

static bool set_value(Parser *p, const KeySpec *spec, char *value)
{
	void *field = (char *)p->current->base + spec->offset;

	switch (spec->type) {
	case KEY_INT:
		return set_int(p, spec->name, value, spec->min, spec->max, field);
	case KEY_COLOUR: {
		ThemeColour *c = field;
		int d[6];
		int i;

		if (strlen(value) != 7 || value[0] != '#')
			return key_fail(p, spec->name, "not a colour (#rrggbb)");
		for (i = 0; i < 6; i++)
			if ((d[i] = hex_digit(value[i + 1])) < 0)
				return key_fail(p, spec->name, "not a colour (#rrggbb)");
		c->r = (unsigned char)(d[0] * 16 + d[1]);
		c->g = (unsigned char)(d[2] * 16 + d[3]);
		c->b = (unsigned char)(d[4] * 16 + d[5]);
		return true;
	}
	case KEY_IMAGE: {
		ThemeImage *img = field;

		memset(img, 0, sizeof *img);
		if (strcmp(value, "none") == 0)
			return true;
		if (!is_file(value, ".png"))
			return key_fail(p, spec->name, "not a file name (.png)");
		img->present = true;
		memcpy(img->file, value, strlen(value) + 1);
		return true;
	}
	case KEY_TTF:
		if (!is_file(value, ".ttf"))
			return key_fail(p, spec->name, "not a file name (.ttf)");
		memcpy(field, value, strlen(value) + 1);
		return true;
	case KEY_CUT:
		return set_marker(p, spec->name, value, false, field);
	case KEY_MARKER:
		return set_marker(p, spec->name, value, true, field);
	case KEY_BUTTON:
		return set_button(p, spec->name, value, field);
	case KEY_FONT: {
		FontRef *ref;

		assert(p->nrefs < FONT_REFS_MAX);
		ref = &p->refs[p->nrefs++];
		ref->index = field;
		ref->section = p->current;
		ref->key = spec->name;
		ref->line = p->line;
		memcpy(ref->name, value, strlen(value) + 1);
		return true;
	}
	case KEY_SIZE: {
		ThemeSize *size = field;

		memset(size, 0, sizeof *size);
		if (strcmp(value, "fit") == 0) {
			size->fit = true;
			return true;
		}
		return set_int(p, spec->name, value, 8, 64, &size->px);
	}
	case KEY_CELL:
		return set_cell(p, spec->name, value, field);
	case KEY_ANCHOR:
		if (strcmp(value, "top") == 0)
			*(ThemeAnchor *)field = THEME_ANCHOR_TOP;
		else if (strcmp(value, "baseline") == 0)
			*(ThemeAnchor *)field = THEME_ANCHOR_BASELINE;
		else
			return key_fail(p, spec->name, "not top or baseline");
		return true;
	}
	return false;
}

static int section_kind(const char *name)
{
	int kind;

	for (kind = 0; kind < SECTION_KIND_COUNT; kind++) {
		const SectionSpec *spec = &section_specs[kind];
		size_t n = strlen(spec->name);
		const char *rest = name + n;

		if (strncmp(name, spec->name, n) != 0)
			continue;
		if ((spec->arity == ARITY_REQUIRED || spec->arity == ARITY_OPTIONAL) && *rest == '\0')
			return kind;
		if (spec->arity == ARITY_NAMED && *rest == '.' && is_name(rest + 1))
			return kind;
		if (spec->arity == ARITY_NUMBERED && *rest == '.' && is_number(rest + 1))
			return kind;
	}
	return -1;
}

static int count_kind(const Parser *p, SectionKind kind)
{
	int n = 0;
	int i;

	for (i = 0; i < p->nsections; i++)
		n += p->sections[i].kind == kind;
	return n;
}

static void *section_base(Parser *p, Section *s)
{
	Theme *t = p->out;

	switch (s->kind) {
	case SECTION_SCREEN:
		return &t->screen;
	case SECTION_FONT:
		if (t->font_count < THEME_FONTS_MAX) {
			ThemeFont *font = &t->fonts[t->font_count++];

			memcpy(font->name, s->name + 5, strlen(s->name + 5) + 1);
			return font;
		}
		if (!p->font_overflow_name[0])
			memcpy(p->font_overflow_name, s->name + 5, strlen(s->name + 5) + 1);
		return &p->font_overflow;
	case SECTION_TABS:
		return &t->tabs;
	case SECTION_TABS_BAND:
		t->tabs.band.present = true;
		return &t->tabs.band;
	case SECTION_TAB:
		parse_int(s->name + 4, &s->number);
		if (s->number >= 1 && s->number <= THEME_TABS_MAX)
			return &t->tabs.slots[s->number - 1];
		p->tab_overflow_seen = true;
		return &p->tab_overflow;
	case SECTION_TITLE:
		return &t->title;
	case SECTION_TITLE_RULE:
		t->title.rule.present = true;
		return &t->title.rule;
	case SECTION_LIST:
		return &t->list;
	case SECTION_LIST_SELECTED:
		t->list.selected.present = true;
		return &t->list.selected;
	case SECTION_LIST_SELECTED_DISABLED:
		t->list.selected_disabled.present = true;
		return &t->list.selected_disabled;
	case SECTION_NOTICE:
		return &t->notice;
	case SECTION_MESSAGE:
		return &t->message;
	case SECTION_FOOTER:
		return &t->footer;
	case SECTION_FOOTER_BAND:
		t->footer.band.present = true;
		return &t->footer.band;
	case SECTION_KIND_COUNT:
		break;
	}
	return NULL;
}

static bool tab_numbering_fail(Parser *p)
{
	return fail(p, "%s: [tab.<n>]: tab slots must be numbered 1 to N, N at most %d", p->file, THEME_TABS_MAX);
}

static bool fonts_fail(Parser *p)
{
	return fail(p, "%s: [font.%s]: more than %d fonts", p->file, p->font_overflow_name, THEME_FONTS_MAX);
}

static bool open_section(Parser *p, const char *name)
{
	Section *s;
	int kind = section_kind(name);
	int i;

	if (kind < 0)
		return fail(p, "%s:%d: [%s]: unknown section", p->file, p->line, name);
	for (i = 0; i < p->nsections; i++)
		if (strcmp(p->sections[i].name, name) == 0)
			return fail(p, "%s:%d: [%s]: duplicate section", p->file, p->line, name);
	if (p->nsections == SECTIONS_MAX) {
		if (p->font_overflow_name[0])
			return fonts_fail(p);
		return tab_numbering_fail(p);
	}
	s = &p->sections[p->nsections++];
	memset(s, 0, sizeof *s);
	s->kind = (SectionKind)kind;
	s->line = p->line;
	memcpy(s->name, name, strlen(name) + 1);
	s->base = section_base(p, s);
	p->current = s;
	return true;
}

static bool set_key(Parser *p, const char *key, char *value)
{
	const SectionSpec *spec;
	int i;

	if (!p->current)
		return fail(p, "%s:%d: key outside a section", p->file, p->line);
	spec = &section_specs[p->current->kind];
	for (i = 0; i < spec->nkeys; i++)
		if (strcmp(spec->keys[i].name, key) == 0)
			break;
	if (i == spec->nkeys)
		return key_fail(p, key, "unknown key");
	if (p->current->seen[i])
		return key_fail(p, key, "duplicate key");
	p->current->seen[i] = true;
	p->current->key_line[i] = p->line;
	return set_value(p, &spec->keys[i], value);
}

static bool parse_line(Parser *p, char *line)
{
	char *s = trim(line);
	char *eq;
	size_t n = strlen(s);

	if (n == 0 || s[0] == ';' || s[0] == '#')
		return true;
	if (s[0] == '[') {
		if (s[n - 1] != ']' || n < 3)
			return fail(p, "%s:%d: not \"key = value\" or \"[section]\"", p->file, p->line);
		s[n - 1] = '\0';
		s = trim(s + 1);
		if (!*s)
			return fail(p, "%s:%d: not \"key = value\" or \"[section]\"", p->file, p->line);
		return open_section(p, s);
	}
	eq = strchr(s, '=');
	if (!eq)
		return fail(p, "%s:%d: not \"key = value\" or \"[section]\"", p->file, p->line);
	*eq = '\0';
	{
		char *key = trim(s);
		char *value = trim(eq + 1);

		if (!*key || !*value)
			return fail(p, "%s:%d: not \"key = value\" or \"[section]\"", p->file, p->line);
		return set_key(p, key, value);
	}
}

static bool check_keys(Parser *p)
{
	int kind;
	int i;
	int k;

	for (kind = 0; kind < SECTION_KIND_COUNT; kind++) {
		const SectionSpec *spec = &section_specs[kind];

		for (i = 0; i < p->nsections; i++) {
			const Section *s = &p->sections[i];

			if (s->kind != (SectionKind)kind)
				continue;
			for (k = 0; k < spec->nkeys; k++)
				if (!s->seen[k])
					return fail(p, "%s:%d: %s.%s: missing", p->file, s->line, s->name, spec->keys[k].name);
		}
	}
	return true;
}

static bool check_sections(Parser *p)
{
	int kind;

	for (kind = 0; kind < SECTION_KIND_COUNT; kind++) {
		const SectionSpec *spec = &section_specs[kind];

		if (spec->arity != ARITY_OPTIONAL && count_kind(p, (SectionKind)kind) == 0)
			return fail(p, "%s: [%s]: missing", p->file, spec->missing);
	}
	return true;
}

static bool check_tabs(Parser *p)
{
	int n = count_kind(p, SECTION_TAB);
	int i;

	if (p->tab_overflow_seen || n > THEME_TABS_MAX)
		return tab_numbering_fail(p);
	for (i = 0; i < p->nsections; i++)
		if (p->sections[i].kind == SECTION_TAB && p->sections[i].number > n)
			return tab_numbering_fail(p);
	p->out->tabs.slot_count = n;
	return true;
}

static bool check_font_refs(Parser *p)
{
	int r;
	int f;

	for (r = 0; r < p->nrefs; r++) {
		FontRef *ref = &p->refs[r];

		for (f = 0; f < p->out->font_count; f++)
			if (strcmp(p->out->fonts[f].name, ref->name) == 0)
				break;
		if (f == p->out->font_count)
			return fail(p, "%s:%d: %s.%s: unknown font %s", p->file, ref->line, ref->section->name, ref->key,
				    ref->name);
		*ref->index = f;
	}
	return true;
}

static int key_index(SectionKind kind, const char *key)
{
	const SectionSpec *spec = &section_specs[kind];
	int k;

	for (k = 0; k < spec->nkeys; k++)
		if (strcmp(spec->keys[k].name, key) == 0)
			return k;
	return -1;
}

static int key_line(const Section *s, const char *key)
{
	int k = key_index(s->kind, key);

	assert(k >= 0);
	return s->key_line[k];
}

static bool check_fit(Parser *p)
{
	int i;

	for (i = 0; i < p->nsections; i++) {
		const Section *s = &p->sections[i];
		const ThemeFont *font = s->base;

		if (s->kind == SECTION_FONT && font->size.fit && !font->cell.present)
			return key_fail_at(p, s, key_line(s, "size"), "size", "fit needs a cell");
	}
	return true;
}

static const Section *find_kind(const Parser *p, SectionKind kind)
{
	int i;

	for (i = 0; i < p->nsections; i++)
		if (p->sections[i].kind == kind)
			return &p->sections[i];
	return NULL;
}

/* row_height is 0 for an absolute band. */
static bool check_band(Parser *p, const Section *s, int row_height)
{
	const ThemeBand *b = s->base;

	if (row_height > 0 && b->h > row_height)
		return range_fail_at(p, s, key_line(s, "h"), "h", 1, row_height);
	if (row_height > 0 && b->y + b->h > row_height)
		return range_fail_at(p, s, key_line(s, "y"), "y", 0, row_height - b->h);
	if (b->x + b->w > THEME_SCREEN_W)
		return key_fail_at(p, s, key_line(s, "w"), "w", "outside the screen");
	if (row_height == 0 && b->y + b->h > THEME_SCREEN_H)
		return key_fail_at(p, s, key_line(s, "h"), "h", "outside the screen");
	return true;
}

static bool check_bands(Parser *p)
{
	static const SectionKind kinds[] = { SECTION_TABS_BAND, SECTION_LIST_SELECTED, SECTION_LIST_SELECTED_DISABLED,
					     SECTION_FOOTER_BAND };
	size_t i;

	for (i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
		const Section *s = find_kind(p, kinds[i]);
		bool row = kinds[i] == SECTION_LIST_SELECTED || kinds[i] == SECTION_LIST_SELECTED_DISABLED;

		if (s && !check_band(p, s, row ? p->out->list.row_height : 0))
			return false;
	}
	return true;
}

static bool check_list(Parser *p)
{
	const Section *s = find_kind(p, SECTION_LIST);
	const ThemeList *l = &p->out->list;

	if (l->bottom - l->top < l->row_height)
		return range_fail_at(p, s, key_line(s, "bottom"), "bottom", l->top + l->row_height, THEME_SCREEN_H);
	if (l->label_x >= l->right)
		return range_fail_at(p, s, key_line(s, "label_x"), "label_x", 0, l->right - 1);
	if (l->label_x_cursor >= l->right)
		return range_fail_at(p, s, key_line(s, "label_x_cursor"), "label_x_cursor", 0, l->right - 1);
	if (l->value_right > l->right)
		return range_fail_at(p, s, key_line(s, "value_right"), "value_right", 0, l->right);
	return true;
}

bool theme_parse(Theme *out, const char *text, const char *file, ThemeError *err)
{
	Parser parser;
	Parser *p = &parser;
	const char *at = text;

	memset(p, 0, sizeof *p);
	memset(out, 0, sizeof *out);
	p->out = out;
	p->file = file;
	p->err = err;
	err->message[0] = '\0';
	for (p->line = 1; *at; p->line++) {
		const char *end = strchr(at, '\n');
		size_t len = end ? (size_t)(end - at) : strlen(at);
		char line[THEME_LINE_MAX + 1];
		size_t i;

		if (len > THEME_LINE_MAX)
			return fail(p, "%s:%d: line longer than %d bytes", file, p->line, THEME_LINE_MAX);
		for (i = 0; i < len; i++)
			if ((unsigned char)at[i] < 0x20 || (unsigned char)at[i] > 0x7e)
				return fail(p, "%s:%d: not printable ASCII", file, p->line);
		memcpy(line, at, len);
		line[len] = '\0';
		if (!parse_line(p, line))
			return false;
		at = end ? end + 1 : at + len;
	}
	if (!check_keys(p) || !check_sections(p))
		return false;
	if (p->font_overflow_name[0])
		return fonts_fail(p);
	return check_tabs(p) && check_font_refs(p) && check_fit(p) && check_bands(p) && check_list(p);
}

static bool load_fail(ThemeError *err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(err->message, sizeof err->message, fmt, ap);
	va_end(ap);
	return false;
}

static int line_of(const char *text, size_t at)
{
	int line = 1;
	size_t i;

	for (i = 0; i < at; i++)
		line += text[i] == '\n';
	return line;
}

bool theme_load(Theme *out, const char *dir, ThemeError *err)
{
	static char buf[THEME_FILE_BYTES_MAX + 1];
	char path[PATH_MAX_BYTES];
	FILE *f;
	size_t n;
	const char *nul;

	err->message[0] = '\0';
	if ((size_t)snprintf(path, sizeof path, "%s/theme.ini", dir) >= sizeof path) {
		return load_fail(err, "%s/theme.ini: cannot open: path too long", dir);
	}
	errno = 0;
	f = fopen(path, "rb");
	if (!f) {
		return load_fail(err, "%s: cannot open: %s", path, strerror(errno));
	}
	n = fread(buf, 1, sizeof buf, f);
	if (ferror(f)) {
		fclose(f);
		return load_fail(err, "%s: cannot open: %s", path, strerror(errno));
	}
	fclose(f);
	if (n > THEME_FILE_BYTES_MAX) {
		return load_fail(err, "%s: larger than %d bytes", path, THEME_FILE_BYTES_MAX);
	}
	buf[n] = '\0';
	nul = memchr(buf, '\0', n);
	if (nul) {
		return load_fail(err, "%s:%d: not printable ASCII", path, line_of(buf, (size_t)(nul - buf)));
	}
	return theme_parse(out, buf, path, err);
}

int theme_page(const Theme *theme)
{
	assert(theme->list.row_height > 0);
	return (theme->list.bottom - theme->list.top) / theme->list.row_height;
}

int theme_columns(const Theme *theme, int advance)
{
	assert(advance >= 1);
	return (theme->list.right - theme->list.label_x) / advance;
}
