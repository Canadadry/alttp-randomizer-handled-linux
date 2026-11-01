#include "rando_record.h"
#include "rando_settings.h"
#include "rando_spoiler.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void assert_date(const RandoRecordDate *d)
{
	assert(d->year >= 1970 && d->year <= 9999);
	assert(d->month >= 1 && d->month <= 12);
	assert(d->day >= 1 && d->day <= 31);
	assert(d->hour >= 0 && d->hour <= 23);
	assert(d->minute >= 0 && d->minute <= 59);
	assert(d->second >= 0 && d->second <= 59);
	(void)d;
}

static bool write_indent(FILE *out, int depth)
{
	int i;

	for (i = 0; i < depth; i++)
		if (fputs("    ", out) == EOF)
			return false;
	return true;
}

bool rando_record_settings_write(FILE *out, const RandoOptions *options, RandoHeartBeep heart_beep, int depth)
{
	bool ok = fputs("{\n", out) != EOF;
	int rank;

	assert(depth >= 0);
	for (rank = 0; rank < RANDO_SETTING_COUNT; rank++) {
		RandoSettingId id = rando_setting_sorted(rank);

		if (!write_indent(out, depth + 1) ||
		    fprintf(out, "\"%s\": \"%s\"%s\n", rando_setting_key(id),
			    rando_setting_value_name(id, rando_setting_get(options, heart_beep, id)),
			    rank + 1 < RANDO_SETTING_COUNT ? "," : "") < 0)
			ok = false;
	}
	if (!write_indent(out, depth) || fputc('}', out) == EOF)
		ok = false;
	return ok && !ferror(out);
}

bool rando_record_write(FILE *out, const RandoRecord *record, const RandoWorld *world,
			const RandoPlaythrough *playthrough)
{
	const RandoRecordDate *d = &record->date;
	bool ok = true;

	assert_date(d);
	assert(!record->has_spoiler || (world != NULL && playthrough != NULL));
	if (fprintf(out,
		    "{\n"
		    "    \"code\": \"%s\",\n"
		    "    \"date\": \"%04d-%02d-%02dT%02d:%02d:%02d\",\n"
		    "    \"format\": %d,\n"
		    "    \"rom\": \"%s\",\n"
		    "    \"seed\": %lu,\n"
		    "    \"settings\": ",
		    record->name.code, d->year, d->month, d->day, d->hour, d->minute, d->second, RANDO_RECORD_FORMAT,
		    record->name.rom, (unsigned long)record->seed) < 0)
		ok = false;
	if (!rando_record_settings_write(out, &record->options, record->heart_beep, 1))
		ok = false;
	if (record->has_spoiler) {
		if (fputs(",\n    \"spoiler\": ", out) == EOF)
			ok = false;
		if (!rando_spoiler_write_json(out, world, &record->options, playthrough))
			ok = false;
		if (fputs("}\n", out) == EOF)
			ok = false;
	} else if (fputs("\n}\n", out) == EOF)
		ok = false;
	return ok && !ferror(out);
}

#define STRING_MAX 64
#define SPOILER_DEPTH_MAX 64

typedef struct Parser {
	const char *text;
	size_t len, pos;
	char *error;
} Parser;

static bool fail(Parser *p, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(p->error, RANDO_RECORD_ERROR_MAX, fmt, ap);
	va_end(ap);
	return false;
}

static void skip_ws(Parser *p)
{
	while (p->pos < p->len && (p->text[p->pos] == ' ' || p->text[p->pos] == '\t' || p->text[p->pos] == '\n' ||
				   p->text[p->pos] == '\r'))
		p->pos++;
}

static char peek(Parser *p)
{
	skip_ws(p);
	return p->pos < p->len ? p->text[p->pos] : '\0';
}

static bool expect(Parser *p, char c, const char *what)
{
	if (peek(p) != c)
		return fail(p, "%s: expected '%c'", what, c);
	p->pos++;
	return true;
}

static bool parse_string(Parser *p, const char *what, char out[STRING_MAX])
{
	size_t n = 0;

	if (!expect(p, '"', what))
		return false;
	for (;;) {
		char c;

		if (p->pos >= p->len)
			return fail(p, "%s: unterminated string", what);
		c = p->text[p->pos++];
		if (c == '"')
			break;
		if (c == '\\' || (unsigned char)c < 0x20)
			return fail(p, "%s: escapes and control characters are refused", what);
		if (n + 1 >= STRING_MAX)
			return fail(p, "%s: string too long", what);
		out[n++] = c;
	}
	out[n] = '\0';
	return true;
}

static bool is_number_char(char c)
{
	return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E';
}

static bool number_token(Parser *p, const char *what, size_t *start, size_t *n)
{
	skip_ws(p);
	*start = p->pos;
	while (p->pos < p->len && is_number_char(p->text[p->pos]))
		p->pos++;
	*n = p->pos - *start;
	if (*n == 0)
		return fail(p, "%s: expected an integer", what);
	return true;
}

static bool next_member(Parser *p, const char *what, bool *more)
{
	char c = peek(p);

	if (c != ',' && c != '}')
		return fail(p, "%s: expected ',' or '}'", what);
	p->pos++;
	*more = c == ',';
	return true;
}

static bool skip_value(Parser *p, int depth)
{
	char c = peek(p);

	if (depth > SPOILER_DEPTH_MAX)
		return fail(p, "spoiler: nested too deep");
	if (c == '{' || c == '[') {
		char close = c == '{' ? '}' : ']';

		p->pos++;
		if (peek(p) == close) {
			p->pos++;
			return true;
		}
		for (;;) {
			if (c == '{') {
				if (peek(p) != '"' || !skip_value(p, depth + 1))
					return fail(p, "spoiler: malformed at byte %lu", (unsigned long)p->pos);
				if (!expect(p, ':', "spoiler"))
					return false;
			}
			if (!skip_value(p, depth + 1))
				return false;
			c = peek(p);
			if (c == close) {
				p->pos++;
				return true;
			}
			if (c != ',')
				return fail(p, "spoiler: unbalanced or malformed at byte %lu", (unsigned long)p->pos);
			p->pos++;
			c = close == '}' ? '{' : '[';
		}
	}
	if (c == '"') {
		for (p->pos++; p->pos < p->len; p->pos++) {
			if (p->text[p->pos] == '\\')
				p->pos++;
			else if (p->text[p->pos] == '"') {
				p->pos++;
				return true;
			}
		}
		return fail(p, "spoiler: unterminated string");
	}
	if ((c >= '0' && c <= '9') || c == '-') {
		while (p->pos < p->len && is_number_char(p->text[p->pos]))
			p->pos++;
		return true;
	}
	{
		static const char *const literals[] = {"true", "false", "null"};
		size_t i;

		for (i = 0; i < 3; i++) {
			size_t n = strlen(literals[i]);

			if (p->len - p->pos >= n && memcmp(p->text + p->pos, literals[i], n) == 0) {
				p->pos += n;
				return true;
			}
		}
	}
	return fail(p, "spoiler: malformed value at byte %lu", (unsigned long)p->pos);
}

static int two_digits(const char *s)
{
	return (s[0] - '0') * 10 + (s[1] - '0');
}

static bool parse_date(Parser *p, const char *s, RandoRecordDate *d)
{
	static const char pattern[] = "dddd-dd-ddTdd:dd:dd";
	size_t i;

	for (i = 0; pattern[i]; i++)
		if (pattern[i] == 'd' ? !(s[i] >= '0' && s[i] <= '9') : s[i] != pattern[i])
			return fail(p, "date: \"%s\" is not YYYY-MM-DDTHH:MM:SS", s);
	if (s[i] != '\0')
		return fail(p, "date: \"%s\" is not YYYY-MM-DDTHH:MM:SS", s);
	d->year = two_digits(s) * 100 + two_digits(s + 2);
	d->month = two_digits(s + 5);
	d->day = two_digits(s + 8);
	d->hour = two_digits(s + 11);
	d->minute = two_digits(s + 14);
	d->second = two_digits(s + 17);
	if (d->year < 1970 || d->year > 9999)
		return fail(p, "date: year out of range in \"%s\"", s);
	if (d->month < 1 || d->month > 12)
		return fail(p, "date: month out of range in \"%s\"", s);
	if (d->day < 1 || d->day > 31)
		return fail(p, "date: day out of range in \"%s\"", s);
	if (d->hour > 23)
		return fail(p, "date: hour out of range in \"%s\"", s);
	if (d->minute > 59)
		return fail(p, "date: minute out of range in \"%s\"", s);
	if (d->second > 59)
		return fail(p, "date: second out of range in \"%s\"", s);
	return true;
}

static bool parse_settings(Parser *p, RandoOptions *options, RandoHeartBeep *heart_beep)
{
	bool seen[RANDO_SETTING_COUNT] = {false}, more = true;
	int rank;

	rando_options_init(options);
	if (!expect(p, '{', "settings"))
		return false;
	if (peek(p) == '}') {
		p->pos++;
		more = false;
	}
	while (more) {
		char key[STRING_MAX], value[STRING_MAX];
		RandoSettingId id;
		int v;

		if (!parse_string(p, "settings", key))
			return false;
		id = rando_setting_find(key);
		if (id == RANDO_SETTING_COUNT)
			return fail(p, "settings: unknown key \"%s\"", key);
		if (seen[id])
			return fail(p, "settings: duplicate key \"%s\"", key);
		seen[id] = true;
		if (!expect(p, ':', key) || !parse_string(p, key, value))
			return false;
		v = rando_setting_value_find(id, value);
		if (v < 0)
			return fail(p, "settings: %s: unknown value \"%s\"", key, value);
		rando_setting_set(options, heart_beep, id, v);
		if (!next_member(p, "settings", &more))
			return false;
	}
	for (rank = 0; rank < RANDO_SETTING_COUNT; rank++)
		if (!seen[rando_setting_sorted(rank)])
			return fail(p, "settings: missing key \"%s\"", rando_setting_key(rando_setting_sorted(rank)));
	return true;
}

bool rando_record_settings_parse(const char *text, size_t len, RandoOptions *options, RandoHeartBeep *heart_beep,
				 char error[RANDO_RECORD_ERROR_MAX])
{
	Parser parser = {text, len, 0, error};

	if (!parse_settings(&parser, options, heart_beep))
		return false;
	skip_ws(&parser);
	if (parser.pos != parser.len)
		return fail(&parser, "settings: trailing text at byte %lu", (unsigned long)parser.pos);
	return true;
}

enum { KEY_CODE, KEY_DATE, KEY_FORMAT, KEY_ROM, KEY_SEED, KEY_SETTINGS, KEY_SPOILER, KEY_COUNT };

static const char *const top_keys[KEY_COUNT] = {"code", "date", "format", "rom", "seed", "settings", "spoiler"};

static int hex_digit(char c)
{
	return c >= '0' && c <= '9' ? c - '0' : c - 'A' + 10;
}

static bool check_rom(Parser *p, RandoRecord *out, const char *rom)
{
	const char *code = out->name.code;
	uint32_t value = 0;
	int i, suffix = 1;
	RandoName name;

	for (i = 0; i < 6; i++)
		value = value * 16 + (uint32_t)hex_digit(code[i]);
	if (code[6] == '-')
		suffix = atoi(code + 7);
	rando_name_make(&out->options, value, suffix, &name);
	if (strcmp(name.rom, rom) != 0)
		return fail(p, "rom: \"%s\" does not match the code and settings (\"%s\")", rom, name.rom);
	out->name = name;
	return true;
}

static bool parse_member(Parser *p, int key, RandoRecord *out, char rom[STRING_MAX])
{
	char value[STRING_MAX];
	size_t start, n;

	switch (key) {
	case KEY_CODE:
		if (!parse_string(p, "code", value))
			return false;
		if (strlen(value) >= RANDO_NAME_CODE_MAX || !rando_name_code_valid(value))
			return fail(p, "code: invalid \"%s\"", value);
		strcpy(out->name.code, value);
		return true;
	case KEY_DATE:
		return parse_string(p, "date", value) && parse_date(p, value, &out->date);
	case KEY_FORMAT:
		if (!number_token(p, "format", &start, &n))
			return false;
		if (n != 1 || p->text[start] != '0' + RANDO_RECORD_FORMAT)
			return fail(p, "format: unsupported %.*s", (int)(n < 16 ? n : 16), p->text + start);
		return true;
	case KEY_ROM:
		return parse_string(p, "rom", rom);
	case KEY_SEED:
		if (!number_token(p, "seed", &start, &n))
			return false;
		if (!rando_parse_uint32(p->text + start, n, &out->seed))
			return fail(p, "seed: not a decimal uint32 on one line");
		return true;
	case KEY_SETTINGS:
		return parse_settings(p, &out->options, &out->heart_beep);
	default:
		if (peek(p) != '{')
			return fail(p, "spoiler: not an object");
		start = p->pos;
		if (!skip_value(p, 0))
			return false;
		out->has_spoiler = true;
		out->spoiler_offset = start;
		out->spoiler_size = p->pos - start;
		return true;
	}
}

bool rando_record_parse(const char *text, size_t len, RandoRecord *out, char error[RANDO_RECORD_ERROR_MAX])
{
	Parser parser = {text, len, 0, error};
	Parser *p = &parser;
	bool seen[KEY_COUNT] = {false}, more = true;
	char rom[STRING_MAX] = "";
	int key;

	if (len == 0)
		return fail(p, "empty text");
	if (len > RANDO_RECORD_TEXT_MAX)
		return fail(p, "text longer than %d bytes", RANDO_RECORD_TEXT_MAX);
	memset(out, 0, sizeof *out);
	if (!expect(p, '{', "record"))
		return false;
	if (peek(p) == '}') {
		p->pos++;
		more = false;
	}
	while (more) {
		char name[STRING_MAX];

		if (!parse_string(p, "record key", name))
			return false;
		for (key = 0; key < KEY_COUNT && strcmp(top_keys[key], name) != 0; key++)
			;
		if (key == KEY_COUNT)
			return fail(p, "unknown key \"%s\"", name);
		if (seen[key])
			return fail(p, "duplicate key \"%s\"", name);
		seen[key] = true;
		if (!expect(p, ':', name) || !parse_member(p, key, out, rom) || !next_member(p, name, &more))
			return false;
	}
	skip_ws(p);
	if (p->pos != p->len)
		return fail(p, "trailing text after the record at byte %lu", (unsigned long)p->pos);
	for (key = 0; key < KEY_SPOILER; key++)
		if (!seen[key])
			return fail(p, "missing key \"%s\"", top_keys[key]);
	return check_rom(p, out, rom);
}

typedef struct Outline {
	char *out;
	size_t cap, len, line_start;
	bool cut;
} Outline;

static void put(Outline *o, const char *s, size_t n)
{
	if (o->cut)
		return;
	if (o->cap - o->len < n + 1) {
		o->cut = true;
		o->len = o->line_start;
		return;
	}
	memcpy(o->out + o->len, s, n);
	o->len += n;
}

static void line_begin(Outline *o, int indent)
{
	int i;

	o->line_start = o->len;
	for (i = 0; i < indent; i++)
		put(o, "  ", 2);
}

static void line_end(Outline *o)
{
	assert((o->cut || o->len < o->cap) && "put leaves room for this newline");
	if (!o->cut)
		o->out[o->len++] = '\n';
}

static bool raw_string(Parser *p, size_t *start, size_t *n)
{
	size_t end, i;

	if (peek(p) != '"')
		return fail(p, "spoiler: expected a string at byte %lu", (unsigned long)p->pos);
	*start = p->pos + 1;
	if (!skip_value(p, 0))
		return false;
	end = p->pos - 1;
	for (i = end; i > *start && p->text[i - 1] >= '0' && p->text[i - 1] <= '9'; i--)
		;
	if (i < end && i - 1 > *start && p->text[i - 1] == ':')
		end = i - 1;
	*n = end - *start;
	return true;
}

typedef struct Member {
	size_t key, key_len, value, index;
	bool numeric;
} Member;

static bool all_digits(const char *s, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (s[i] < '0' || s[i] > '9')
			return false;
	return n > 0;
}

static bool member_before(const char *text, const Member *a, const Member *b)
{
	const char *x = text + a->key, *y = text + b->key;
	size_t nx = a->key_len, ny = b->key_len;
	int c;

	while (nx > 1 && *x == '0')
		x++, nx--;
	while (ny > 1 && *y == '0')
		y++, ny--;
	if (nx != ny)
		return nx < ny;
	c = memcmp(x, y, nx);
	return c != 0 ? c < 0 : a->index < b->index;
}

static bool member_head(Parser *p, size_t index, Member *m)
{
	if (!raw_string(p, &m->key, &m->key_len) || !expect(p, ':', "spoiler"))
		return false;
	m->numeric = all_digits(p->text + m->key, m->key_len);
	m->value = p->pos;
	m->index = index;
	return true;
}

static bool member_tail(Parser *p, char close, bool *more)
{
	char c = peek(p);

	if (c != ',' && c != close)
		return fail(p, "spoiler: malformed at byte %lu", (unsigned long)p->pos);
	p->pos++;
	*more = c == ',';
	return true;
}

static bool outline_value(Parser *p, Outline *o, int depth, int indent, const char *label, size_t label_len);

static bool outline_object(Parser *p, Outline *o, int depth, int indent, bool top)
{
	size_t start, index;
	bool more, have_last = false;
	Member m, best, last;

	if (!expect(p, '{', "spoiler"))
		return false;
	start = p->pos;
	for (;;) {
		bool found = false;

		p->pos = start;
		more = peek(p) != '}';
		for (index = 0; more; index++) {
			if (!member_head(p, index, &m) || !skip_value(p, depth + 1) || !member_tail(p, '}', &more))
				return false;
			if (m.numeric && (!have_last || member_before(p->text, &last, &m)) &&
			    (!found || member_before(p->text, &m, &best))) {
				best = m;
				found = true;
			}
		}
		if (!found)
			break;
		p->pos = best.value;
		if (!outline_value(p, o, depth + 1, indent, p->text + best.key, best.key_len))
			return false;
		last = best;
		have_last = true;
	}
	p->pos = start;
	more = peek(p) != '}';
	for (index = 0; more; index++) {
		bool skip;

		if (!member_head(p, index, &m))
			return false;
		skip = m.numeric || (top && m.key_len == 4 && memcmp(p->text + m.key, "meta", 4) == 0);
		if (!(skip ? skip_value(p, depth + 1)
			   : outline_value(p, o, depth + 1, indent, p->text + m.key, m.key_len)) ||
		    !member_tail(p, '}', &more))
			return false;
	}
	if (index == 0)
		p->pos++;
	return true;
}

static bool outline_value(Parser *p, Outline *o, int depth, int indent, const char *label, size_t label_len)
{
	char c = peek(p);
	size_t start, n;

	if (depth > RANDO_RECORD_OUTLINE_DEPTH)
		return fail(p, "spoiler: nested deeper than %d", RANDO_RECORD_OUTLINE_DEPTH);
	line_begin(o, indent);
	put(o, label, label_len);
	if (c == '{') {
		line_end(o);
		return outline_object(p, o, depth, indent + 1, false);
	}
	if (c == '[') {
		bool more;
		int index = 0;

		line_end(o);
		p->pos++;
		more = peek(p) != ']';
		if (!more)
			p->pos++;
		while (more) {
			char number[16];

			sprintf(number, "%d", ++index);
			if (!outline_value(p, o, depth + 1, indent + 1, number, strlen(number)) ||
			    !member_tail(p, ']', &more))
				return false;
		}
		return true;
	}
	if (c == '"') {
		if (!raw_string(p, &start, &n))
			return false;
	} else {
		start = p->pos;
		if (!skip_value(p, depth))
			return false;
		n = p->pos - start;
	}
	put(o, ": ", 2);
	put(o, p->text + start, n);
	line_end(o);
	return true;
}

bool rando_record_spoiler_outline(const char *text, size_t offset, size_t size, char *out, size_t cap,
				  size_t *len, bool *cut)
{
	char error[RANDO_RECORD_ERROR_MAX];
	Parser parser = {text + offset, size, 0, error};
	Outline o = {out, cap, 0, 0, false};

	assert(text != NULL && out != NULL && len != NULL && cut != NULL);
	if (peek(&parser) != '{' || !outline_object(&parser, &o, 1, 0, true))
		return false;
	skip_ws(&parser);
	if (parser.pos != parser.len)
		return false;
	*len = o.len;
	*cut = o.cut;
	return true;
}
