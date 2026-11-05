#include "theme.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SECTION_KEYS_MAX 40
#define SECTIONS_MAX 32
#define SECTION_NAME_MAX 32
#define INT_SATURATE 1000000

typedef enum KeyType {
	KEY_COLOUR,
	KEY_IMAGE,
	KEY_TTF,
	KEY_CUT,
	KEY_SIZE,
	KEY_CELL,
	KEY_ANCHOR
} KeyType;

typedef struct KeySpec {
	const char *name;
	KeyType type;
	size_t offset;
} KeySpec;

typedef enum SectionKind {
	SECTION_SCREEN,
	SECTION_FONT,
	SECTION_KIND_COUNT
} SectionKind;

typedef struct SectionSpec {
	const char *name;
	bool named;
	const KeySpec *keys;
	int nkeys;
} SectionSpec;

typedef struct Section {
	SectionKind kind;
	char name[SECTION_NAME_MAX];
	void *base;
	int line;
	bool seen[SECTION_KEYS_MAX];
} Section;

typedef struct Parser {
	Theme *out;
	const char *file;
	ThemeError *err;
	int line;
	Section sections[SECTIONS_MAX];
	int nsections;
	Section *current;
	ThemeFont overflow;
	char overflow_name[THEME_NAME_MAX];
} Parser;

static const KeySpec screen_keys[] = {
	{ "colour", KEY_COLOUR, offsetof(ThemeScreen, colour) },
	{ "image", KEY_IMAGE, offsetof(ThemeScreen, image) },
	{ "cut", KEY_CUT, offsetof(ThemeScreen, cut) },
};

static const KeySpec font_keys[] = {
	{ "file", KEY_TTF, offsetof(ThemeFont, file) },
	{ "size", KEY_SIZE, offsetof(ThemeFont, size) },
	{ "cell", KEY_CELL, offsetof(ThemeFont, cell) },
	{ "anchor", KEY_ANCHOR, offsetof(ThemeFont, anchor) },
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const SectionSpec section_specs[SECTION_KIND_COUNT] = {
	{ "screen", false, screen_keys, COUNT(screen_keys) },
	{ "font", true, font_keys, COUNT(font_keys) },
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

static bool key_fail(Parser *p, const char *key, const char *problem)
{
	return fail(p, "%s:%d: %s.%s: %s", p->file, p->line, p->current->name, key, problem);
}

static bool set_int(Parser *p, const char *key, const char *s, int min, int max, int *out)
{
	if (!parse_int(s, out))
		return key_fail(p, key, "not an integer");
	if (*out < min || *out > max)
		return fail(p, "%s:%d: %s.%s: out of range %d..%d", p->file, p->line, p->current->name, key, min, max);
	return true;
}

static bool set_marker(Parser *p, const char *key, const char *s, ThemeMarker *m)
{
	const char *problem = "not a marker (none, text:<chars>)";

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
	return key_fail(p, key, problem);
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
		return set_marker(p, spec->name, value, field);
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

static bool open_section(Parser *p, const char *name)
{
	Section *s;
	int kind;
	int i;

	for (kind = 0; kind < SECTION_KIND_COUNT; kind++) {
		const SectionSpec *spec = &section_specs[kind];
		size_t n = strlen(spec->name);

		if (!spec->named && strcmp(name, spec->name) == 0)
			break;
		if (spec->named && strncmp(name, spec->name, n) == 0 && name[n] == '.' && is_name(name + n + 1))
			break;
	}
	if (kind == SECTION_KIND_COUNT)
		return fail(p, "%s:%d: [%s]: unknown section", p->file, p->line, name);
	for (i = 0; i < p->nsections; i++)
		if (strcmp(p->sections[i].name, name) == 0)
			return fail(p, "%s:%d: [%s]: duplicate section", p->file, p->line, name);
	if (p->nsections == SECTIONS_MAX)
		return fail(p, "%s: [%s]: more than %d fonts", p->file, name, THEME_FONTS_MAX);
	s = &p->sections[p->nsections++];
	memset(s, 0, sizeof *s);
	s->kind = (SectionKind)kind;
	s->line = p->line;
	memcpy(s->name, name, strlen(name) + 1);
	if (kind == SECTION_SCREEN) {
		s->base = &p->out->screen;
	} else if (p->out->font_count < THEME_FONTS_MAX) {
		ThemeFont *font = &p->out->fonts[p->out->font_count++];

		memcpy(font->name, name + 5, strlen(name + 5) + 1);
		s->base = font;
	} else {
		if (!p->overflow_name[0])
			memcpy(p->overflow_name, name + 5, strlen(name + 5) + 1);
		s->base = &p->overflow;
	}
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

static bool check_complete(Parser *p)
{
	int kind;
	int i;
	int k;
	bool has[SECTION_KIND_COUNT] = { false };

	for (kind = 0; kind < SECTION_KIND_COUNT; kind++) {
		const SectionSpec *spec = &section_specs[kind];

		for (i = 0; i < p->nsections; i++) {
			Section *s = &p->sections[i];

			if (s->kind != (SectionKind)kind)
				continue;
			has[kind] = true;
			for (k = 0; k < spec->nkeys; k++)
				if (!s->seen[k])
					return fail(p, "%s:%d: %s.%s: missing", p->file, s->line, s->name, spec->keys[k].name);
		}
	}
	if (!has[SECTION_SCREEN])
		return fail(p, "%s: [screen]: missing", p->file);
	if (!has[SECTION_FONT])
		return fail(p, "%s: [font.<name>]: missing", p->file);
	if (p->overflow_name[0])
		return fail(p, "%s: [font.%s]: more than %d fonts", p->file, p->overflow_name, THEME_FONTS_MAX);
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
	return check_complete(p);
}
