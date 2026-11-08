#include "rando_ladx_record.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

bool rando_ladx_record_write(FILE *out, const RandoLadxRecord *record)
{
	const RandoRecordDate *d = &record->date;
	bool ok;
	int k;

	assert(rando_record_date_valid(d));
	assert(strlen(record->seed) == RANDO_LADX_SEED_HEX);
	ok = fprintf(out,
		     "{\n"
		     "    \"code\": \"%s\",\n"
		     "    \"date\": \"%04d-%02d-%02dT%02d:%02d:%02d\",\n"
		     "    \"format\": %d,\n"
		     "    \"rom\": \"%s\",\n"
		     "    \"seed\": \"%s\",\n"
		     "    \"settings\": {\n",
		     record->name.code, d->year, d->month, d->day, d->hour, d->minute, d->second,
		     RANDO_LADX_RECORD_FORMAT, record->name.rom, record->seed) >= 0;
	for (k = 0; k < LADX_SETTING_COUNT; k++)
		if (fprintf(out, "        \"%s\": \"%s\"%s\n", ladx_setting_key((LadxSettingId)k),
			    ladx_setting_value_name((LadxSettingId)k, record->settings.value[k]),
			    k + 1 < LADX_SETTING_COUNT ? "," : "") < 0)
			ok = false;
	if (fprintf(out, "    },\n    \"spoiler\": %s\n}\n", record->has_spoiler ? "true" : "false") < 0)
		ok = false;
	return ok && !ferror(out);
}

#define STRING_MAX 64

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

static char peek(Parser *p)
{
	while (p->pos < p->len && (p->text[p->pos] == ' ' || p->text[p->pos] == '\t' || p->text[p->pos] == '\n' ||
				   p->text[p->pos] == '\r'))
		p->pos++;
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

static bool literal(Parser *p, const char *word)
{
	size_t n = strlen(word);

	peek(p);
	if (p->len - p->pos < n || memcmp(p->text + p->pos, word, n) != 0)
		return false;
	p->pos += n;
	return true;
}

/* Ends a member: true with *more when a ',' or the closing '}' follows. */
static bool next_member(Parser *p, const char *what, bool *more)
{
	char c = peek(p);

	if (c != ',' && c != '}')
		return fail(p, "%s: expected ',' or '}'", what);
	p->pos++;
	*more = c == ',';
	return true;
}

static int two(const char *s)
{
	return (s[0] - '0') * 10 + (s[1] - '0');
}

static bool parse_date(Parser *p, RandoRecordDate *d)
{
	static const char pattern[] = "dddd-dd-ddTdd:dd:dd";
	char s[STRING_MAX];
	size_t i;

	if (!parse_string(p, "date", s))
		return false;
	for (i = 0; pattern[i]; i++)
		if (pattern[i] == 'd' ? !(s[i] >= '0' && s[i] <= '9') : s[i] != pattern[i])
			return fail(p, "date: \"%s\" is not YYYY-MM-DDTHH:MM:SS", s);
	if (s[i] != '\0')
		return fail(p, "date: \"%s\" is not YYYY-MM-DDTHH:MM:SS", s);
	d->year = two(s) * 100 + two(s + 2);
	d->month = two(s + 5);
	d->day = two(s + 8);
	d->hour = two(s + 11);
	d->minute = two(s + 14);
	d->second = two(s + 17);
	if (!rando_record_date_valid(d))
		return fail(p, "date: out of range in \"%s\"", s);
	return true;
}

static bool parse_seed(Parser *p, char seed[RANDO_LADX_SEED_HEX + 1])
{
	char s[STRING_MAX];
	size_t i;

	if (!parse_string(p, "seed", s))
		return false;
	for (i = 0; s[i]; i++)
		if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
			break;
	if (s[i] != '\0' || i != RANDO_LADX_SEED_HEX)
		return fail(p, "seed: not %d lower-case hex digits", RANDO_LADX_SEED_HEX);
	memcpy(seed, s, RANDO_LADX_SEED_HEX + 1);
	return true;
}

static bool parse_settings(Parser *p, LadxSettings *settings)
{
	bool seen[LADX_SETTING_COUNT] = {false}, more = true;
	int k;

	ladx_settings_defaults(settings);
	if (!expect(p, '{', "settings"))
		return false;
	if (peek(p) == '}') {
		p->pos++;
		more = false;
	}
	while (more) {
		char key[STRING_MAX], value[STRING_MAX];
		LadxSettingId id;
		int v;

		if (!parse_string(p, "settings", key))
			return false;
		id = ladx_setting_find(key);
		if (id == LADX_SETTING_COUNT)
			return fail(p, "settings: unknown key \"%s\"", key);
		if (seen[id])
			return fail(p, "settings: duplicate key \"%s\"", key);
		seen[id] = true;
		if (!expect(p, ':', key) || !parse_string(p, key, value))
			return false;
		v = ladx_setting_value_find(id, value);
		if (v < 0)
			return fail(p, "settings: %s: unknown value \"%s\"", key, value);
		settings->value[id] = (uint8_t)v;
		if (!next_member(p, "settings", &more))
			return false;
	}
	for (k = 0; k < LADX_SETTING_COUNT; k++)
		if (!seen[k])
			return fail(p, "settings: missing key \"%s\"", ladx_setting_key((LadxSettingId)k));
	return true;
}

enum { KEY_CODE, KEY_DATE, KEY_FORMAT, KEY_ROM, KEY_SEED, KEY_SETTINGS, KEY_SPOILER, KEY_COUNT };

static const char *const top_keys[KEY_COUNT] = {"code", "date", "format", "rom", "seed", "settings", "spoiler"};

static bool parse_member(Parser *p, int key, RandoLadxRecord *out, char rom[STRING_MAX])
{
	char value[STRING_MAX];

	switch (key) {
	case KEY_CODE:
		if (!parse_string(p, "code", value))
			return false;
		if (strlen(value) >= RANDO_NAME_CODE_MAX || !rando_name_code_valid(value))
			return fail(p, "code: invalid \"%s\"", value);
		strcpy(out->name.code, value);
		return true;
	case KEY_DATE:
		return parse_date(p, &out->date);
	case KEY_FORMAT:
		if (!literal(p, "1") || (p->pos < p->len && p->text[p->pos] >= '0' && p->text[p->pos] <= '9'))
			return fail(p, "format: unsupported, expected %d", RANDO_LADX_RECORD_FORMAT);
		return true;
	case KEY_ROM:
		return parse_string(p, "rom", rom);
	case KEY_SEED:
		return parse_seed(p, out->seed);
	case KEY_SETTINGS:
		return parse_settings(p, &out->settings);
	default:
		if (literal(p, "true"))
			out->has_spoiler = true;
		else if (literal(p, "false"))
			out->has_spoiler = false;
		else
			return fail(p, "spoiler: expected true or false");
		return true;
	}
}

static bool check_rom(Parser *p, RandoLadxRecord *out, const char *rom)
{
	const char *code = out->name.code;
	int suffix = code[6] == '-' ? atoi(code + 7) : 1, i;
	uint32_t value = 0;
	RandoName name;

	for (i = 0; i < 6; i++)
		value = value * 16 + (uint32_t)(code[i] <= '9' ? code[i] - '0' : code[i] - 'A' + 10);
	rando_ladx_name_make(&out->settings, value, suffix, &name);
	if (strcmp(name.rom, rom) != 0)
		return fail(p, "rom: \"%s\" does not match the code and settings (\"%s\")", rom, name.rom);
	out->name = name;
	return true;
}

bool rando_ladx_record_parse(const char *text, size_t len, RandoLadxRecord *out,
			     char error[RANDO_RECORD_ERROR_MAX])
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
	if (peek(p) != '\0')
		return fail(p, "trailing text after the record at byte %lu", (unsigned long)p->pos);
	for (key = 0; key < KEY_COUNT; key++)
		if (!seen[key])
			return fail(p, "missing key \"%s\"", top_keys[key]);
	return check_rom(p, out, rom);
}
