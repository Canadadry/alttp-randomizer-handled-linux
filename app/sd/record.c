#include "rando_record.h"
#include "rando_case.h"
#include "rando_spoiler.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* Bytewise sorted: the record's settings keys. */
static const char *const settings_keys[] = {
	"accessibility", "crystals_ganon", "crystals_tower", "dungeon_items",  "glitches",
	"goal",		 "heart_beep",	   "hints",	     "item_functionality", "item_placement",
	"item_pool",	 "state",	   "weapons",
};

#define SETTINGS_COUNT (sizeof settings_keys / sizeof settings_keys[0])

static const char *settings_value(const RandoRecord *record, const char *key)
{
	RandoOptionKey option;

	if (strcmp(key, "hints") == 0)
		return rando_hints_name(record->options.hints);
	if (strcmp(key, "heart_beep") == 0)
		return rando_heart_beep_name(record->heart_beep);
	option = rando_option_key_find(key);
	assert(option < RANDO_OPTION_KEY_COUNT);
	return rando_option_value_name(option, rando_options_get(&record->options, option));
}

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

bool rando_record_write(FILE *out, const RandoRecord *record, const RandoWorld *world,
			const RandoPlaythrough *playthrough)
{
	const RandoRecordDate *d = &record->date;
	size_t i;
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
		    "    \"settings\": {\n",
		    record->name.code, d->year, d->month, d->day, d->hour, d->minute, d->second, RANDO_RECORD_FORMAT,
		    record->name.rom, (unsigned long)record->seed) < 0)
		ok = false;
	for (i = 0; i < SETTINGS_COUNT; i++)
		if (fprintf(out, "        \"%s\": \"%s\"%s\n", settings_keys[i], settings_value(record, settings_keys[i]),
			    i + 1 < SETTINGS_COUNT ? "," : "") < 0)
			ok = false;
	if (record->has_spoiler) {
		if (fputs("    },\n    \"spoiler\": ", out) == EOF)
			ok = false;
		if (!rando_spoiler_write_json(out, world, &record->options, playthrough))
			ok = false;
		if (fputs("}\n", out) == EOF)
			ok = false;
	} else if (fputs("    }\n}\n", out) == EOF)
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

/* '\0' at the end of the text. */
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

/* A string without escapes or control characters, copied NUL-terminated into out[STRING_MAX]. */
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

/* The number token at the cursor, as [*start, *start + *n). */
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

/* "," continues (true, *more set), "}" ends (true, *more cleared). */
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

static bool parse_settings(Parser *p, RandoRecord *out)
{
	bool seen[SETTINGS_COUNT] = {false}, more = true;
	size_t i;

	rando_options_init(&out->options);
	if (!expect(p, '{', "settings"))
		return false;
	if (peek(p) == '}') {
		p->pos++;
		more = false;
	}
	while (more) {
		char key[STRING_MAX], value[STRING_MAX];
		bool ok;

		if (!parse_string(p, "settings", key))
			return false;
		for (i = 0; i < SETTINGS_COUNT && strcmp(settings_keys[i], key) != 0; i++)
			;
		if (i == SETTINGS_COUNT)
			return fail(p, "settings: unknown key \"%s\"", key);
		if (seen[i])
			return fail(p, "settings: duplicate key \"%s\"", key);
		seen[i] = true;
		if (!expect(p, ':', key) || !parse_string(p, key, value))
			return false;
		if (strcmp(key, "hints") == 0)
			ok = rando_hints_find(value, &out->options.hints);
		else if (strcmp(key, "heart_beep") == 0)
			ok = rando_heart_beep_find(value, &out->heart_beep);
		else
			ok = rando_options_set(&out->options, key, value);
		if (!ok)
			return fail(p, "settings: %s: unknown value \"%s\"", key, value);
		if (!next_member(p, "settings", &more))
			return false;
	}
	for (i = 0; i < SETTINGS_COUNT; i++)
		if (!seen[i])
			return fail(p, "settings: missing key \"%s\"", settings_keys[i]);
	return true;
}

enum { KEY_CODE, KEY_DATE, KEY_FORMAT, KEY_ROM, KEY_SEED, KEY_SETTINGS, KEY_SPOILER, KEY_COUNT };

static const char *const top_keys[KEY_COUNT] = {"code", "date", "format", "rom", "seed", "settings", "spoiler"};

static int hex_digit(char c)
{
	return c >= '0' && c <= '9' ? c - '0' : c - 'A' + 10;
}

/* Accepts only the rom the settings and a valid code make. */
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
	char value[STRING_MAX], seed_error[RANDO_CASE_ERROR_MAX];
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
		if (!rando_case_parse_uint32(p->text + start, n, "seed", &out->seed, seed_error))
			return fail(p, "%s", seed_error);
		return true;
	case KEY_SETTINGS:
		return parse_settings(p, out);
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
