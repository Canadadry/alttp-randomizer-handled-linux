#include "rando_text.h"
#include "text_table.h"

#include <assert.h>
#include <string.h>

#define WRAP 19
#define BOX_ROWS 3

typedef struct CharBytes {
	uint32_t code_point;
	uint8_t count;
	uint8_t bytes[3];
} CharBytes;

/* Dialog::$characters at 219fcafd, sorted by code point. */
static const CharBytes characters[] = {
	{0x0020, 1, {0xFF}},
	{0x0021, 1, {0xC7}},
	{0x0025, 2, {0xFD, 0x10}},
	{0x0027, 1, {0x9D}},
	{0x002C, 1, {0xC8}},
	{0x002D, 1, {0xC9}},
	{0x002E, 1, {0xCD}},
	{0x003A, 1, {0xEA}},
	{0x003D, 2, {0xFD, 0x12}},
	{0x003E, 2, {0x9B, 0x9C}},
	{0x003F, 1, {0xC6}},
	{0x0040, 2, {0xFE, 0x6A}},
	{0x005E, 2, {0xFD, 0x11}},
	{0x007E, 1, {0xCE}},
	{0x00BC, 2, {0xE5, 0xE7}},
	{0x00BD, 2, {0xE6, 0xE7}},
	{0x00BE, 2, {0xE8, 0xE9}},
	{0x168B, 3, {0xFE, 0x6C, 0x00}},
	{0x168C, 3, {0xFE, 0x6C, 0x01}},
	{0x168D, 3, {0xFE, 0x6C, 0x02}},
	{0x168E, 3, {0xFE, 0x6C, 0x03}},
	{0x2019, 1, {0x9D}},
	{0x2026, 1, {0x9F}},
	{0x2190, 2, {0xFD, 0x16}},
	{0x2191, 2, {0xFD, 0x13}},
	{0x2192, 2, {0xFD, 0x15}},
	{0x2193, 2, {0xFD, 0x14}},
	{0x2265, 1, {0x99}},
	{0x2665, 2, {0xEA, 0xEB}},
	{0x3041, 1, {0x4B}},
	{0x3042, 1, {0x00}},
	{0x3043, 1, {0x4C}},
	{0x3044, 1, {0x01}},
	{0x3045, 1, {0x4D}},
	{0x3046, 1, {0x02}},
	{0x3047, 1, {0x4E}},
	{0x3048, 1, {0x03}},
	{0x3049, 1, {0x4F}},
	{0x304A, 1, {0x04}},
	{0x304B, 1, {0x08}},
	{0x304C, 1, {0x15}},
	{0x304D, 1, {0x09}},
	{0x304E, 1, {0x16}},
	{0x304F, 1, {0x0A}},
	{0x3050, 1, {0x17}},
	{0x3051, 1, {0x0B}},
	{0x3052, 1, {0x1D}},
	{0x3053, 1, {0x0C}},
	{0x3054, 1, {0x1E}},
	{0x3055, 1, {0x10}},
	{0x3056, 1, {0x1F}},
	{0x3057, 1, {0x11}},
	{0x3058, 1, {0x25}},
	{0x3059, 1, {0x12}},
	{0x305A, 1, {0x26}},
	{0x305B, 1, {0x13}},
	{0x305C, 1, {0x27}},
	{0x305D, 1, {0x14}},
	{0x305E, 1, {0x2D}},
	{0x305F, 1, {0x18}},
	{0x3060, 1, {0x2E}},
	{0x3061, 1, {0x19}},
	{0x3062, 1, {0x2F}},
	{0x3063, 1, {0x4A}},
	{0x3064, 1, {0x1A}},
	{0x3065, 1, {0x35}},
	{0x3066, 1, {0x1B}},
	{0x3067, 1, {0x36}},
	{0x3068, 1, {0x1C}},
	{0x3069, 1, {0x37}},
	{0x306A, 1, {0x20}},
	{0x306B, 1, {0x21}},
	{0x306C, 1, {0x22}},
	{0x306D, 1, {0x23}},
	{0x306E, 1, {0x24}},
	{0x306F, 1, {0x28}},
	{0x3070, 1, {0x3D}},
	{0x3071, 1, {0x42}},
	{0x3072, 1, {0x29}},
	{0x3073, 1, {0x3E}},
	{0x3074, 1, {0x43}},
	{0x3075, 1, {0x2A}},
	{0x3076, 1, {0x3F}},
	{0x3077, 1, {0x44}},
	{0x3078, 1, {0x2B}},
	{0x3079, 1, {0x40}},
	{0x307A, 1, {0x45}},
	{0x307B, 1, {0x2C}},
	{0x307C, 1, {0x41}},
	{0x307D, 1, {0x46}},
	{0x307E, 1, {0x30}},
	{0x307F, 1, {0x31}},
	{0x3080, 1, {0x32}},
	{0x3081, 1, {0x33}},
	{0x3082, 1, {0x34}},
	{0x3083, 1, {0x47}},
	{0x3084, 1, {0x05}},
	{0x3085, 1, {0x48}},
	{0x3086, 1, {0x06}},
	{0x3087, 1, {0x49}},
	{0x3088, 1, {0x07}},
	{0x3089, 1, {0x38}},
	{0x308A, 1, {0x39}},
	{0x308B, 1, {0x3A}},
	{0x308C, 1, {0x3B}},
	{0x308D, 1, {0x3C}},
	{0x308F, 1, {0x0D}},
	{0x3092, 1, {0x0E}},
	{0x3093, 1, {0x0F}},
	{0x30A1, 1, {0x9B}},
	{0x30A2, 1, {0x50}},
	{0x30A3, 1, {0x9C}},
	{0x30A4, 1, {0x51}},
	{0x30A5, 1, {0x9D}},
	{0x30A6, 1, {0x52}},
	{0x30A7, 1, {0x9E}},
	{0x30A8, 1, {0x53}},
	{0x30A9, 1, {0x9F}},
	{0x30AA, 1, {0x54}},
	{0x30AB, 1, {0x58}},
	{0x30AC, 1, {0x65}},
	{0x30AD, 1, {0x59}},
	{0x30AE, 1, {0x66}},
	{0x30AF, 1, {0x5A}},
	{0x30B0, 1, {0x67}},
	{0x30B1, 1, {0x5B}},
	{0x30B2, 1, {0x6D}},
	{0x30B3, 1, {0x5C}},
	{0x30B4, 1, {0x6E}},
	{0x30B5, 1, {0x60}},
	{0x30B6, 1, {0x6F}},
	{0x30B7, 1, {0x61}},
	{0x30B8, 1, {0x75}},
	{0x30B9, 1, {0x62}},
	{0x30BA, 1, {0x76}},
	{0x30BB, 1, {0x63}},
	{0x30BC, 1, {0x77}},
	{0x30BD, 1, {0x64}},
	{0x30BE, 1, {0x7D}},
	{0x30BF, 1, {0x68}},
	{0x30C0, 1, {0x7E}},
	{0x30C1, 1, {0x69}},
	{0x30C3, 1, {0x9A}},
	{0x30C4, 1, {0x6A}},
	{0x30C5, 1, {0x85}},
	{0x30C6, 1, {0x6B}},
	{0x30C7, 1, {0x86}},
	{0x30C8, 1, {0x6C}},
	{0x30C9, 1, {0x87}},
	{0x30CA, 1, {0x70}},
	{0x30CB, 1, {0x71}},
	{0x30CC, 1, {0x72}},
	{0x30CD, 1, {0x73}},
	{0x30CE, 1, {0x74}},
	{0x30CF, 1, {0x78}},
	{0x30D0, 1, {0x8D}},
	{0x30D1, 1, {0x92}},
	{0x30D2, 1, {0x79}},
	{0x30D3, 1, {0x8E}},
	{0x30D4, 1, {0x93}},
	{0x30D5, 1, {0x7A}},
	{0x30D6, 1, {0x8F}},
	{0x30D7, 1, {0x94}},
	{0x30D8, 1, {0x7B}},
	{0x30D9, 1, {0x90}},
	{0x30DA, 1, {0x95}},
	{0x30DB, 1, {0x7C}},
	{0x30DC, 1, {0x91}},
	{0x30DD, 1, {0x96}},
	{0x30DE, 1, {0x80}},
	{0x30DF, 1, {0x81}},
	{0x30E0, 1, {0x82}},
	{0x30E1, 1, {0x83}},
	{0x30E2, 1, {0x84}},
	{0x30E3, 1, {0x97}},
	{0x30E4, 1, {0x55}},
	{0x30E5, 1, {0x98}},
	{0x30E6, 1, {0x56}},
	{0x30E7, 1, {0x99}},
	{0x30E8, 1, {0x57}},
	{0x30E9, 1, {0x88}},
	{0x30EA, 1, {0x89}},
	{0x30EB, 1, {0x8A}},
	{0x30EC, 1, {0x8B}},
	{0x30ED, 1, {0x8C}},
	{0x30EF, 1, {0x5D}},
	{0x30F2, 1, {0x5E}},
	{0x30F3, 1, {0x5F}},
	{0xFF5E, 1, {0xCE}},
};

typedef struct Command {
	const char *name;
	uint8_t count;
	uint8_t bytes[13];
} Command;

/* {NOTEXT} is not here: it returns at once, dropping what came before. */
static const Command commands[] = {
	{"{SPEED0}", 2, {0xFC, 0x00}},
	{"{SPEED2}", 2, {0xFC, 0x02}},
	{"{SPEED6}", 2, {0xFC, 0x06}},
	{"{PAUSE1}", 3, {0xFE, 0x78, 0x01}},
	{"{PAUSE3}", 3, {0xFE, 0x78, 0x03}},
	{"{PAUSE5}", 3, {0xFE, 0x78, 0x05}},
	{"{PAUSE7}", 3, {0xFE, 0x78, 0x07}},
	{"{PAUSE9}", 3, {0xFE, 0x78, 0x09}},
	{"{INPUT}", 1, {0xFA}},
	{"{CHOICE}", 2, {0xFE, 0x68}},
	{"{ITEMSELECT}", 2, {0xFE, 0x69}},
	{"{CHOICE2}", 2, {0xFE, 0x71}},
	{"{CHOICE3}", 2, {0xFE, 0x72}},
	{"{C:GREEN}", 3, {0xFE, 0x77, 0x07}},
	{"{C:YELLOW}", 3, {0xFE, 0x77, 0x02}},
	{"{HARP}", 3, {0xFE, 0x79, 0x2D}},
	{"{MENU}", 3, {0xFE, 0x6D, 0x00}},
	{"{BOTTOM}", 3, {0xFE, 0x6D, 0x01}},
	{"{NOBORDER}", 3, {0xFE, 0x6B, 0x02}},
	{"{CHANGEPIC}", 4, {0xFE, 0x67, 0xFE, 0x67}},
	{"{CHANGEMUSIC}", 2, {0xFE, 0x67}},
	{"{INTRO}", 13, {0xFE, 0x6E, 0x00, 0xFE, 0x77, 0x07, 0xFC, 0x03, 0xFE, 0x6B, 0x02, 0xFE, 0x67}},
	{"{IBOX}", 9, {0xFE, 0x6B, 0x02, 0xFE, 0x77, 0x07, 0xFC, 0x03, 0xF7}},
};

static const uint8_t notext[] = {0xFB, 0xFE, 0x6E, 0x00, 0xFE, 0x6B, 0x04};

typedef struct Span {
	const char *at;
	size_t len;
} Span;

static size_t utf8_len(unsigned char c)
{
	if (c < 0x80)
		return 1;
	if ((c & 0xE0) == 0xC0)
		return 2;
	if ((c & 0xF0) == 0xE0)
		return 3;
	return 4;
}

static bool utf8_valid(const char *s)
{
	const unsigned char *p = (const unsigned char *)s;

	while (*p != '\0') {
		size_t n, k;
		uint32_t cp;

		if (*p < 0x80) {
			p++;
			continue;
		}
		if (*p >= 0xC2 && *p <= 0xDF) {
			n = 2;
			cp = *p & 0x1F;
		} else if ((*p & 0xF0) == 0xE0) {
			n = 3;
			cp = *p & 0x0F;
		} else if (*p >= 0xF0 && *p <= 0xF4) {
			n = 4;
			cp = *p & 0x07;
		} else {
			return false;
		}
		for (k = 1; k < n; k++) {
			if ((p[k] & 0xC0) != 0x80)
				return false;
			cp = cp << 6 | (p[k] & 0x3F);
		}
		if ((n == 3 && cp < 0x800) || (n == 4 && (cp < 0x10000 || cp > 0x10FFFF)) ||
		    (cp >= 0xD800 && cp <= 0xDFFF))
			return false;
		p += n;
	}
	return true;
}

static uint32_t utf8_decode(const char *s, size_t *len)
{
	const unsigned char *p = (const unsigned char *)s;
	size_t n = utf8_len(*p), k;
	uint32_t cp = n == 1 ? *p : n == 2 ? (*p & 0x1Fu) : n == 3 ? (*p & 0x0Fu) : (*p & 0x07u);

	for (k = 1; k < n; k++)
		cp = cp << 6 | (p[k] & 0x3Fu);
	*len = n;
	return cp;
}

static size_t span_chars(Span s)
{
	size_t i = 0, n = 0;

	while (i < s.len) {
		i += utf8_len((unsigned char)s.at[i]);
		n++;
	}
	return n;
}

/* PHP's default trim set. */
static bool php_space(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\0' || c == '\x0B';
}

static Span rtrim(Span s)
{
	while (s.len > 0 && php_space(s.at[s.len - 1]))
		s.len--;
	return s;
}

static Span trim(Span s)
{
	s = rtrim(s);
	while (s.len > 0 && php_space(s.at[0])) {
		s.at++;
		s.len--;
	}
	return s;
}

/* mb_wordwrap($line, 19, "\n"): a single-byte line takes PHP's wordwrap (no cut, no trim), a
 * multibyte one is rtrimmed and re-packed by character count. */
typedef struct Wrap {
	Span line;
	bool multibyte;
	bool done;
	size_t cur, laststart, lastspace, seg;
	/* mb_wordwrap's $actual is this span plus one trailing space, unless actual_empty. */
	bool repack;
	size_t word;
	Span actual;
	bool actual_empty;
} Wrap;

static void wrap_start(Wrap *w, Span line)
{
	size_t i;

	memset(w, 0, sizeof *w);
	w->line = line;
	for (i = 0; i < line.len; i++)
		if ((unsigned char)line.at[i] >= 0x80)
			w->multibyte = true;
	if (w->multibyte) {
		w->line = rtrim(line);
		w->repack = span_chars(w->line) > WRAP;
		w->actual_empty = true;
	}
}

static bool wordwrap_next(Wrap *w, Span *out)
{
	const char *p = w->line.at;
	size_t n = w->line.len;

	while (w->cur < n) {
		size_t cur = w->cur++;
		long brk = -1;

		if (p[cur] == ' ') {
			if (cur - w->laststart >= WRAP) {
				brk = (long)cur;
				w->laststart = cur + 1;
			}
			w->lastspace = cur;
		} else if (cur - w->laststart >= WRAP && w->laststart != w->lastspace) {
			brk = (long)w->lastspace;
			w->laststart = w->lastspace + 1;
		}
		/* A break at or before seg is one already made. */
		if (brk >= 0 && (size_t)brk >= w->seg) {
			out->at = p + w->seg;
			out->len = (size_t)brk - w->seg;
			w->seg = (size_t)brk + 1;
			return true;
		}
	}
	if (w->done)
		return false;
	w->done = true;
	out->at = p + w->seg;
	out->len = n - w->seg;
	return true;
}

static bool repack_next(Wrap *w, Span *out)
{
	const char *p = w->line.at;
	size_t n = w->line.len;

	while (w->word <= n && !w->done) {
		Span word;
		size_t end = w->word, fit;

		while (end < n && p[end] != ' ')
			end++;
		word.at = p + w->word;
		word.len = end - w->word;
		w->word = end + 1;
		fit = (w->actual_empty ? 0 : span_chars(w->actual) + 1) + span_chars(word);
		if (fit <= WRAP) {
			if (w->actual_empty)
				w->actual = word;
			else
				w->actual.len = (size_t)(word.at + word.len - w->actual.at);
			w->actual_empty = false;
			continue;
		}
		if (!w->actual_empty) {
			*out = rtrim(w->actual);
			w->actual = word;
			return true;
		}
		w->actual = word;
		w->actual_empty = false;
	}
	if (w->done)
		return false;
	w->done = true;
	*out = trim(w->actual);
	return true;
}

static bool wrap_next(Wrap *w, Span *out)
{
	if (!w->multibyte)
		return wordwrap_next(w, out);
	if (w->repack)
		return repack_next(w, out);
	if (w->done)
		return false;
	w->done = true;
	*out = w->line;
	return true;
}

typedef struct Lines {
	const char *rest;
	bool more;
	Wrap wrap;
} Lines;

static void lines_start(Lines *ls, const char *s)
{
	ls->rest = s;
	ls->more = false;
}

static bool lines_next(Lines *ls, Span *out)
{
	for (;;) {
		Span src;
		const char *nl;

		if (ls->more && wrap_next(&ls->wrap, out))
			return true;
		if (ls->rest == NULL)
			return false;
		nl = strchr(ls->rest, '\n');
		src.at = ls->rest;
		src.len = nl != NULL ? (size_t)(nl - ls->rest) : strlen(ls->rest);
		ls->rest = nl != NULL ? nl + 1 : NULL;
		wrap_start(&ls->wrap, src);
		ls->more = true;
	}
}

typedef struct Out {
	uint8_t *bytes;
	size_t count;
} Out;

/* count keeps growing past the cut, so a command overflow can be asserted. */
static void put(Out *o, const uint8_t *bytes, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++, o->count++)
		if (o->count < RANDO_TEXT_ENTRY_MAX)
			o->bytes[o->count] = bytes[i];
}

static void put1(Out *o, uint8_t byte)
{
	put(o, &byte, 1);
}

static void put_char(Out *o, uint32_t cp)
{
	size_t lo = 0, hi = sizeof characters / sizeof characters[0];

	if (cp >= '0' && cp <= '9') {
		put1(o, (uint8_t)(cp - '0' + 0xA0));
		return;
	}
	if (cp >= 'A' && cp <= 'Z') {
		put1(o, (uint8_t)(cp - 'A' + 0xAA));
		return;
	}
	if (cp >= 'a' && cp <= 'z') {
		put1(o, (uint8_t)(cp + 0x6F));
		return;
	}
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;

		if (characters[mid].code_point == cp) {
			put(o, characters[mid].bytes, characters[mid].count);
			return;
		}
		if (characters[mid].code_point < cp)
			lo = mid + 1;
		else
			hi = mid;
	}
	put1(o, 0xFF);
}

static bool span_is(Span s, const char *name)
{
	return s.len == strlen(name) && memcmp(s.at, name, s.len) == 0;
}

size_t rando_text_encode(const char *utf8, bool pause, uint8_t out[RANDO_TEXT_ENTRY_MAX])
{
	Out o;
	Lines ls;
	Span line;
	size_t nlines = 0, line_count, i = 0;
	bool last_command = false, pad_out = false;

	assert(utf8 != NULL && utf8_valid(utf8) && "rando_text_encode: not valid UTF-8");
	o.bytes = out;
	o.count = 0;
	put1(&o, 0xFB);

	lines_start(&ls, utf8);
	while (lines_next(&ls, &line)) {
		nlines++;
		last_command = line.len > 0 && line.at[0] == '{';
	}
	line_count = last_command ? nlines - 1 : nlines;

	lines_start(&ls, utf8);
	while (lines_next(&ls, &line)) {
		size_t at = 0, nchars = 0, k;

		if (line.len > 0 && line.at[0] == '{') {
			Span name = trim(line);

			if (span_is(name, "{NOTEXT}")) {
				memcpy(out, notext, sizeof notext);
				return sizeof notext;
			}
			for (k = 0; k < sizeof commands / sizeof commands[0]; k++)
				if (span_is(name, commands[k].name)) {
					put(&o, commands[k].bytes, commands[k].count);
					break;
				}
			if (span_is(name, "{INTRO}"))
				pad_out = true;
			/* VT's line_count may go negative; 0 compares the same against i. */
			if (line_count > 0)
				line_count--;
			assert(o.count <= RANDO_TEXT_ENTRY_MAX && "rando_text_encode: command overflowed byte length");
			continue;
		}

		if (i == 1)
			put1(&o, 0xF8);
		else if (i == 2)
			put1(&o, 0xF9);
		else if (i > 2)
			put1(&o, 0xF6);

		while (at < line.len && nchars < WRAP) {
			size_t len;

			put_char(&o, utf8_decode(line.at + at, &len));
			at += len;
			nchars++;
		}
		if (pad_out && i < BOX_ROWS)
			for (; nchars < WRAP; nchars++)
				put1(&o, 0xFF);
		i++;

		if (pause && i % BOX_ROWS == 0 && line_count > i)
			put1(&o, 0xFA);
	}
	return o.count < RANDO_TEXT_ENTRY_MAX ? o.count : RANDO_TEXT_ENTRY_MAX;
}

const char *rando_text_name(RandoTextId id)
{
	assert(id < RANDO_TEXT_COUNT);
	return rando_text_names[id];
}

RandoTextId rando_text_find(const char *name)
{
	int id;

	for (id = 0; id < RANDO_TEXT_COUNT; id++)
		if (strcmp(rando_text_names[id], name) == 0)
			return (RandoTextId)id;
	return RANDO_TEXT_NONE;
}

size_t rando_text_default(RandoTextId id, uint8_t out[RANDO_TEXT_ENTRY_MAX])
{
	const TextDefault *d;

	assert(id < RANDO_TEXT_COUNT);
	d = &rando_text_defaults[id];
	if (d->text != NULL)
		return rando_text_encode(d->text, d->pause, out);
	assert(d->raw_count > 0 && d->raw_count <= RANDO_TEXT_ENTRY_MAX);
	memcpy(out, d->raw, d->raw_count);
	return d->raw_count;
}
