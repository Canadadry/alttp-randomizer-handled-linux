#include "ladx_wlog.h"

#include "wlog_token.h"

#include <string.h>

#define OUT_CHUNK 256

/* Buffers the sink's text so a record of any length goes out in pieces. */
typedef struct Out {
	LadxWlog *log;
	char buf[OUT_CHUNK];
	size_t len;
} Out;

static void out_flush(Out *o)
{
	if (o->len)
		o->log->sink(o->log->ctx, o->buf, o->len);
	o->len = 0;
}

static void out_char(Out *o, char c)
{
	if (o->len == OUT_CHUNK)
		out_flush(o);
	o->buf[o->len++] = c;
}

static void out_text(Out *o, const char *s)
{
	while (*s)
		out_char(o, *s++);
}

static void out_init(Out *o, LadxWlog *log)
{
	o->log = log;
	o->len = 0;
}

static int name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
	       c == '-';
}

/* write_log's NAME, copied with its NUL; 0 when malformed or too long. */
static int copy_name(char *dst, const char *at, size_t len)
{
	size_t i;

	if (len == 0 || len > LADX_WLOG_NAME_MAX)
		return 0;
	for (i = 0; i < len; i++) {
		if (!name_char(at[i]))
			return 0;
		dst[i] = at[i];
	}
	dst[len] = '\0';
	return 1;
}

LadxWlogStatus ladx_wlog_begin(LadxWlog *log, LadxSpan tables, LadxWlogSink sink, void *ctx)
{
	size_t i = 0;
	int count = 0, a, b;
	Out o;

	log->failed = LADX_WLOG_OK;
	log->group[0] = '\0';
	log->bank_count = log->entry_count = log->atom_count = 0;
	log->table_count = 0;
	while (i < tables.len) {
		size_t j = i;

		while (j < tables.len && tables.at[j] != ' ')
			j++;
		if (count == LADX_WLOG_TABLES_MAX || !copy_name(log->tables[count], tables.at + i, j - i))
			return LADX_WLOG_BAD_INPUT;
		count++;
		if (j == tables.len)
			break;
		i = j + 1;
		if (i == tables.len)
			return LADX_WLOG_BAD_INPUT;
	}
	if (count == 0)
		return LADX_WLOG_BAD_INPUT;
	for (a = 0; a < count; a++)
		for (b = a + 1; b < count; b++)
			if (strcmp(log->tables[a], log->tables[b]) == 0)
				return LADX_WLOG_BAD_INPUT;
	log->table_count = count;
	log->sink = sink;
	log->ctx = ctx;
	out_init(&o, log);
	out_text(&o, "ladx-rom-writes 1\ntables");
	for (a = 0; a < count; a++) {
		out_char(&o, ' ');
		out_text(&o, log->tables[a]);
	}
	out_char(&o, '\n');
	out_flush(&o);
	return LADX_WLOG_OK;
}

static void out_hex(Out *o, uint32_t v)
{
	static const char digit[] = "0123456789abcdef";
	char tmp[8];
	int n = 0;

	do {
		tmp[n++] = digit[v & 0xf];
		v >>= 4;
	} while (v);
	while (n)
		out_char(o, tmp[--n]);
}

static void out_byte(Out *o, uint32_t b)
{
	static const char digit[] = "0123456789abcdef";

	out_char(o, digit[(b >> 4) & 0xf]);
	out_char(o, digit[b & 0xf]);
}

static uint32_t literal_value(LadxAtom a)
{
	return (uint32_t)(-1 - a);
}

/* A record's atoms: a bank run (each byte with its own offset) or a table entry's atoms. */
typedef struct Atoms {
	const LadxWlogBank *bank;
	const LadxAtom *atoms;
	size_t n;
} Atoms;

static LadxAtom atom_at(const Atoms *r, size_t i)
{
	return r->bank ? r->bank[i].atom : r->atoms[i];
}

/* write_log._tokens, each token preceded by a space; keep tokens for a bank run only. */
static void out_tokens(Out *o, const Atoms *r)
{
	size_t i = 0, j;

	while (i < r->n) {
		LadxAtom a = atom_at(r, i);

		out_char(o, ' ');
		j = i + 1;
		if (a < 0) {
			out_byte(o, literal_value(a));
			while (j < r->n && atom_at(r, j) < 0)
				out_byte(o, literal_value(atom_at(r, j++)));
		} else {
			while (j < r->n && atom_at(r, j) >= 0 && (uint32_t)atom_at(r, j) == (uint32_t)a + (j - i))
				j++;
			if (r->bank && (uint32_t)a == r->bank[i].offset) {
				out_char(o, '=');
			} else {
				out_char(o, '@');
				out_hex(o, (uint32_t)a);
				out_char(o, '+');
			}
			out_hex(o, (uint32_t)(j - i));
		}
		i = j;
	}
}

static void close_segment(LadxWlog *log)
{
	Out o;
	int i = 0;

	if (!log->group[0])
		return;
	out_init(&o, log);
	out_text(&o, "G ");
	out_text(&o, log->group);
	out_char(&o, '\n');
	while (i < log->bank_count) {
		Atoms r;
		int j = i + 1;

		while (j < log->bank_count && log->bank[j].offset == log->bank[j - 1].offset + 1)
			j++;
		r.bank = log->bank + i;
		r.atoms = NULL;
		r.n = (size_t)(j - i);
		out_text(&o, "B ");
		out_hex(&o, log->bank[i].offset);
		out_tokens(&o, &r);
		out_char(&o, '\n');
		i = j;
	}
	for (i = 0; i < log->entry_count; i++) {
		const LadxWlogEntry *e = &log->entry[i];

		out_text(&o, "T ");
		out_text(&o, log->tables[e->table]);
		out_char(&o, ' ');
		if (e->alt_len)
			out_text(&o, e->alt);
		else
			out_hex(&o, e->index);
		if (e->pointer) {
			out_text(&o, " p");
			out_hex(&o, e->value);
		} else {
			Atoms r;

			r.bank = NULL;
			r.atoms = log->atom + e->first;
			r.n = e->count;
			out_tokens(&o, &r);
		}
		out_char(&o, '\n');
	}
	out_flush(&o);
	log->group[0] = '\0';
	log->bank_count = log->entry_count = log->atom_count = 0;
}

/* Opens group's segment, closing another one; BAD_INPUT for a bad name. */
static LadxWlogStatus open_group(LadxWlog *log, LadxSpan group)
{
	char name[LADX_WLOG_NAME_MAX + 1];

	if (!copy_name(name, group.at, group.len))
		return LADX_WLOG_BAD_INPUT;
	if (strcmp(name, log->group) != 0) {
		close_segment(log);
		strcpy(log->group, name);
	}
	return LADX_WLOG_OK;
}

static int atom_ok(LadxAtom a)
{
	return a >= LADX_ATOM_LITERAL(255);
}

/* The first index whose offset is >= offset. */
static int bank_find(const LadxWlog *log, uint64_t offset)
{
	int lo = 0, hi = log->bank_count;

	while (lo < hi) {
		int mid = lo + (hi - lo) / 2;

		if (log->bank[mid].offset < offset)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

LadxWlogStatus ladx_wlog_bank(LadxWlog *log, LadxSpan group, uint32_t offset, const LadxAtom *atoms, size_t n)
{
	LadxWlogStatus st;
	int lo, hi, i;
	size_t k;

	if (log->failed)
		return log->failed;
	if (n == 0 || (uint64_t)offset + n > (uint64_t)UINT32_MAX + 1)
		return LADX_WLOG_BAD_INPUT;
	for (k = 0; k < n; k++)
		if (!atom_ok(atoms[k]))
			return LADX_WLOG_BAD_INPUT;
	if ((st = open_group(log, group)) != LADX_WLOG_OK)
		return st;
	lo = bank_find(log, offset);
	hi = bank_find(log, (uint64_t)offset + n);
	if ((size_t)(log->bank_count - (hi - lo)) + n > LADX_WLOG_BANK_MAX)
		return log->failed = LADX_WLOG_OVERFLOW;
	memmove(log->bank + lo + n, log->bank + hi, (size_t)(log->bank_count - hi) * sizeof log->bank[0]);
	log->bank_count += (int)n - (hi - lo);
	for (i = 0; i < (int)n; i++) {
		log->bank[lo + i].offset = offset + (uint32_t)i;
		log->bank[lo + i].atom = atoms[i];
	}
	return LADX_WLOG_OK;
}

/* write_log's render order: table, index keys ascending, then alt keys in byte order. */
static int entry_cmp(const LadxWlogEntry *e, int table, LadxWlogKey key)
{
	size_t n;
	int c;

	if (e->table != table)
		return e->table < table ? -1 : 1;
	if (!e->alt_len != !key.alt.len)
		return e->alt_len ? 1 : -1;
	if (!e->alt_len)
		return e->index < key.index ? -1 : e->index > key.index;
	n = e->alt_len < key.alt.len ? e->alt_len : key.alt.len;
	c = memcmp(e->alt, key.alt.at, n);
	if (c)
		return c;
	return e->alt_len < key.alt.len ? -1 : e->alt_len > key.alt.len;
}

/* Moves the live atoms to the pool's start, in entry order. */
static void compact_atoms(LadxWlog *log)
{
	uint32_t next = 0;
	int i, k, order[LADX_WLOG_ENTRY_MAX], n = 0;

	for (i = 0; i < log->entry_count; i++)
		if (!log->entry[i].pointer && log->entry[i].count)
			order[n++] = i;
	/* Live runs are moved down in pool order, so no run is overwritten before it moves. */
	for (i = 1; i < n; i++) {
		int v = order[i];

		for (k = i; k > 0 && log->entry[order[k - 1]].first > log->entry[v].first; k--)
			order[k] = order[k - 1];
		order[k] = v;
	}
	for (i = 0; i < n; i++) {
		LadxWlogEntry *e = &log->entry[order[i]];

		memmove(log->atom + next, log->atom + e->first, e->count * sizeof log->atom[0]);
		e->first = next;
		next += e->count;
	}
	log->atom_count = (int)next;
}

static LadxWlogStatus put_entry(LadxWlog *log, LadxSpan group, int table, LadxWlogKey key, const LadxAtom *atoms,
				size_t n, int pointer, uint32_t value)
{
	char alt[LADX_WLOG_NAME_MAX + 1];
	LadxWlogStatus st;
	LadxWlogEntry *e;
	int lo = 0, hi, found = 0;
	size_t k;

	if (log->failed)
		return log->failed;
	if (table < 0 || table >= log->table_count || (key.alt.len && !copy_name(alt, key.alt.at, key.alt.len)))
		return LADX_WLOG_BAD_INPUT;
	for (k = 0; k < n; k++)
		if (!atom_ok(atoms[k]))
			return LADX_WLOG_BAD_INPUT;
	if ((st = open_group(log, group)) != LADX_WLOG_OK)
		return st;
	hi = log->entry_count;
	while (lo < hi) {
		int mid = lo + (hi - lo) / 2;

		if (entry_cmp(&log->entry[mid], table, key) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	found = lo < log->entry_count && entry_cmp(&log->entry[lo], table, key) == 0;
	if (!pointer && (size_t)log->atom_count + n > LADX_WLOG_ATOM_MAX) {
		if (found)
			log->entry[lo].count = 0;
		compact_atoms(log);
		if ((size_t)log->atom_count + n > LADX_WLOG_ATOM_MAX)
			return log->failed = LADX_WLOG_OVERFLOW;
	}
	if (!found) {
		if (log->entry_count == LADX_WLOG_ENTRY_MAX)
			return log->failed = LADX_WLOG_OVERFLOW;
		memmove(log->entry + lo + 1, log->entry + lo, (size_t)(log->entry_count - lo) * sizeof log->entry[0]);
		log->entry_count++;
	}
	e = &log->entry[lo];
	e->table = (uint8_t)table;
	e->alt_len = (uint8_t)key.alt.len;
	if (key.alt.len)
		memcpy(e->alt, alt, key.alt.len + 1);
	e->index = key.alt.len ? 0 : key.index;
	e->pointer = (uint8_t)pointer;
	e->value = value;
	e->first = (uint32_t)log->atom_count;
	e->count = (uint32_t)n;
	if (n) {
		memcpy(log->atom + log->atom_count, atoms, n * sizeof atoms[0]);
		log->atom_count += (int)n;
	}
	return LADX_WLOG_OK;
}

LadxWlogStatus ladx_wlog_table(LadxWlog *log, LadxSpan group, int table, LadxWlogKey key, const LadxAtom *atoms,
			       size_t n)
{
	return put_entry(log, group, table, key, atoms, n, 0, 0);
}

LadxWlogStatus ladx_wlog_pointer(LadxWlog *log, LadxSpan group, int table, LadxWlogKey key, uint32_t value)
{
	return put_entry(log, group, table, key, NULL, 0, 1, value);
}

LadxWlogStatus ladx_wlog_end(LadxWlog *log)
{
	if (!log->failed)
		close_segment(log);
	return log->failed;
}

/* Parsing ------------------------------------------------------------------------------------- */

#define EMIT_CHUNK 256

/* Python's str.split(" ") of a line: an empty text is one empty field. */
typedef struct Fields {
	LadxSpan rest;
	int more;
} Fields;

static void fields_init(Fields *f, LadxSpan line)
{
	f->rest = line;
	f->more = 1;
}

static int next_field(Fields *f, LadxSpan *out)
{
	size_t i = 0;

	if (!f->more)
		return 0;
	while (i < f->rest.len && f->rest.at[i] != ' ')
		i++;
	out->at = f->rest.at;
	out->len = i;
	if (i == f->rest.len) {
		f->more = 0;
	} else {
		f->rest.at += i + 1;
		f->rest.len -= i + 1;
	}
	return 1;
}

static int span_eq(LadxSpan a, LadxSpan b)
{
	return a.len == b.len && memcmp(a.at, b.at, a.len) == 0;
}

static int span_is(LadxSpan a, const char *s)
{
	return a.len == strlen(s) && memcmp(a.at, s, a.len) == 0;
}

static int is_name(LadxSpan s)
{
	size_t i;

	if (s.len == 0 || s.len > LADX_WLOG_NAME_MAX)
		return 0;
	for (i = 0; i < s.len; i++)
		if (!name_char(s.at[i]))
			return 0;
	return 1;
}

static int is_hex_text(LadxSpan s)
{
	size_t i;

	if (s.len == 0)
		return 0;
	for (i = 0; i < s.len; i++)
		if (!((s.at[i] >= '0' && s.at[i] <= '9') || (s.at[i] >= 'a' && s.at[i] <= 'f')))
			return 0;
	return 1;
}

/* The text's lines, each without its '\n'. */
typedef struct Lines {
	LadxSpan text;
	size_t pos;
	int number;
} Lines;

static int lines_next(Lines *l, LadxSpan *line)
{
	size_t i = l->pos;

	if (l->pos >= l->text.len)
		return 0;
	while (l->text.at[i] != '\n')
		i++;
	line->at = l->text.at + l->pos;
	line->len = i - l->pos;
	l->pos = i + 1;
	l->number++;
	return 1;
}

/* A T record's key, in write_log's order: table, index keys, then alt keys by bytes. */
typedef struct Key {
	int table;
	int alt;
	uint32_t index;
	LadxSpan name;
} Key;

static int key_cmp(const Key *a, const Key *b)
{
	size_t n;
	int c;

	if (a->table != b->table)
		return a->table < b->table ? -1 : 1;
	if (a->alt != b->alt)
		return a->alt ? 1 : -1;
	if (!a->alt)
		return a->index < b->index ? -1 : a->index > b->index;
	n = a->name.len < b->name.len ? a->name.len : b->name.len;
	c = memcmp(a->name.at, b->name.at, n);
	if (c)
		return c;
	return a->name.len < b->name.len ? -1 : a->name.len > b->name.len;
}

/* Where a record's atoms go while projecting: none, the log in bank chunks, or the scratch. */
typedef struct Emit {
	LadxWlog *log;
	LadxSpan group;
	int bank;
	uint32_t offset;
	LadxAtom chunk[EMIT_CHUNK];
	size_t n;
	LadxWlogStatus st;
} Emit;

static void emit(Emit *e, LadxAtom a)
{
	if (!e || e->st != LADX_WLOG_OK)
		return;
	if (e->bank) {
		if (e->n == EMIT_CHUNK) {
			e->st = ladx_wlog_bank(e->log, e->group, e->offset, e->chunk, e->n);
			e->offset += (uint32_t)e->n;
			e->n = 0;
		}
		e->chunk[e->n++] = a;
	} else if (e->n == LADX_WLOG_ATOM_MAX) {
		e->st = LADX_WLOG_OVERFLOW;
	} else {
		e->log->scratch[e->n++] = a;
	}
}

/*
 * A record's tokens, every rule of write_log._parse_tokens: bank (offset
 * given) or table. A table record of a single pointer sets *pointer.
 * Atoms go to e when not NULL.
 */
static int payload(Fields *f, int bank, uint32_t offset, uint32_t *total, int *pointer, uint32_t *value, Emit *e)
{
	enum { NONE, LIT, REF } prev = NONE;
	uint64_t at = 0, last = 0;
	LadxSpan tok;
	int count = 0;
	uint32_t i;

	*pointer = 0;
	while (next_field(f, &tok)) {
		LadxWlogToken t;
		uint64_t q;

		count++;
		if (!ladx_wlog_token(tok, &t))
			return 0;
		if (t.kind == LADX_WLOG_TOKEN_POINTER) {
			if (bank || count != 1 || f->more)
				return 0;
			*pointer = 1;
			*value = t.q;
			return 1;
		}
		if (t.kind == LADX_WLOG_TOKEN_LITERAL) {
			if (prev == LIT)
				return 0;
			prev = LIT;
			for (i = 0; i < t.n; i++)
				emit(e, LADX_ATOM_LITERAL(ladx_wlog_token_byte(&t, i)));
		} else {
			if (t.kind == LADX_WLOG_TOKEN_KEEP && !bank)
				return 0;
			q = t.kind == LADX_WLOG_TOKEN_KEEP ? offset + at : t.q;
			if ((t.kind == LADX_WLOG_TOKEN_COPY && bank && q == offset + at) || (prev == REF && last + 1 == q) ||
			    q + t.n > 0x80000000u)
				return 0;
			prev = REF;
			last = q + t.n - 1;
			for (i = 0; i < t.n; i++)
				emit(e, (LadxAtom)(q + i));
		}
		at += t.n;
		if ((bank && offset + at > 0x80000000u) || at > 0x7fffffffu)
			return 0;
	}
	if (bank && at == 0)
		return 0;
	*total = (uint32_t)at;
	return 1;
}

typedef struct Parse {
	LadxSpan tables[LADX_WLOG_TABLES_MAX];
	int table_count;
	LadxSpan tables_names; /* the tables line after "tables " */
	/* projection: when log is set, group's records go into it */
	LadxWlog *log;
	LadxSpan group;
	/* the groups found, when groups is set */
	LadxSpan *groups;
	int cap, count;
	int overflow;
} Parse;

/* An I line's target: a record line before it, its tokens literal over each range. */
static int mark_ok(Fields *f, Lines *cursor, int number, int *last_mark)
{
	LadxSpan tok, line = {NULL, 0}, kind, ignore;
	uint32_t target, start, n;
	int64_t end = -1;
	Fields rf;
	size_t plus;

	if (!next_field(f, &tok) || !ladx_wlog_hex(tok, &target) || !f->more)
		return 0;
	if ((int64_t)target <= *last_mark || (int64_t)target >= number)
		return 0;
	*last_mark = (int)target;
	while (cursor->number < (int)target)
		lines_next(cursor, &line);
	fields_init(&rf, line);
	next_field(&rf, &kind);
	if (span_is(kind, "B"))
		next_field(&rf, &ignore);
	else if (span_is(kind, "T"))
		next_field(&rf, &ignore), next_field(&rf, &ignore);
	else
		return 0;
	while (next_field(f, &tok)) {
		Fields scan = rf;
		LadxSpan rt;
		uint64_t at = 0;
		int literal = 0;

		for (plus = 0; plus < tok.len && tok.at[plus] != '+'; plus++)
			;
		if (plus == tok.len)
			return 0;
		ignore.at = tok.at;
		ignore.len = plus;
		rt.at = tok.at + plus + 1;
		rt.len = tok.len - plus - 1;
		if (!ladx_wlog_hex(ignore, &start) || !ladx_wlog_hex(rt, &n) || n == 0 || (int64_t)start <= end)
			return 0;
		while (next_field(&scan, &rt)) {
			LadxWlogToken t;

			ladx_wlog_token(rt, &t);
			if (t.kind == LADX_WLOG_TOKEN_POINTER)
				return 0;
			if (t.kind == LADX_WLOG_TOKEN_LITERAL && at <= start && (uint64_t)start + n <= at + t.n)
				literal = 1;
			at += t.n;
		}
		if (!literal)
			return 0;
		end = (int64_t)start + n;
	}
	return 1;
}

static LadxWlogStatus parse_fail(int *line, int number)
{
	*line = number;
	return LADX_WLOG_MALFORMED;
}

static void found_group(Parse *p, LadxSpan name)
{
	int i;

	if (!p->groups)
		return;
	for (i = 0; i < p->count; i++)
		if (span_eq(p->groups[i], name))
			return;
	if (p->count == p->cap)
		p->overflow = 1;
	else
		p->groups[p->count++] = name;
}

/* The header lines into p's tables. */
static LadxWlogStatus parse_header(Parse *p, Lines *l, int *line)
{
	LadxSpan text, tok;
	Fields f;
	int a;

	if (!lines_next(l, &text) || !span_is(text, "ladx-rom-writes 1"))
		return parse_fail(line, 1);
	if (!lines_next(l, &text))
		return parse_fail(line, 2);
	fields_init(&f, text);
	if (!next_field(&f, &tok) || !span_is(tok, "tables") || !f.more)
		return parse_fail(line, 2);
	p->tables_names = f.rest;
	p->table_count = 0;
	while (next_field(&f, &tok)) {
		if (p->table_count == LADX_WLOG_TABLES_MAX || !is_name(tok))
			return parse_fail(line, 2);
		for (a = 0; a < p->table_count; a++)
			if (span_eq(p->tables[a], tok))
				return parse_fail(line, 2);
		p->tables[p->table_count++] = tok;
	}
	return LADX_WLOG_OK;
}

static int table_of(const Parse *p, LadxSpan name)
{
	int i;

	for (i = 0; i < p->table_count; i++)
		if (span_eq(p->tables[i], name))
			return i;
	return -1;
}

static LadxWlogStatus parse_log(Parse *p, LadxSpan text, int *line)
{
	Lines l, cursor;
	LadxSpan row;
	LadxWlogStatus st;
	int open = 0, records = 0, group_line = 0, has_bank = 0, has_key = 0, last_mark = 0, feeding = 0;
	uint32_t bank_end = 0;
	LadxSpan group = {NULL, 0};
	Key last_key = {0, 0, 0, {NULL, 0}};
	Emit e;

	*line = 0;
	if (text.len && text.at[text.len - 1] != '\n')
		return LADX_WLOG_MALFORMED;
	l.text = text;
	l.pos = 0;
	l.number = 0;
	if ((st = parse_header(p, &l, line)) != LADX_WLOG_OK)
		return st;
	cursor = l;
	cursor.pos = 0;
	cursor.number = 0;
	while (lines_next(&l, &row)) {
		LadxSpan kind, a, b;
		Fields f;
		uint32_t total, value;
		int pointer;
		Emit *out;

		fields_init(&f, row);
		next_field(&f, &kind);
		if (span_is(kind, "I")) {
			if (!mark_ok(&f, &cursor, l.number, &last_mark))
				return parse_fail(line, l.number);
			continue;
		}
		if (last_mark)
			return parse_fail(line, l.number);
		if (span_is(kind, "G") && next_field(&f, &a) && !f.more && is_name(a)) {
			if (open && records == 0)
				return parse_fail(line, group_line);
			if (open && span_eq(a, group))
				return parse_fail(line, l.number);
			open = 1;
			records = has_bank = has_key = 0;
			group = a;
			group_line = l.number;
			feeding = p->log && span_eq(a, p->group);
			found_group(p, a);
			continue;
		}
		fields_init(&f, row);
		next_field(&f, &kind);
		if (!open)
			return parse_fail(line, l.number);
		out = feeding ? &e : NULL;
		if (out) {
			out->log = p->log;
			out->group = p->group;
			out->n = 0;
			out->st = LADX_WLOG_OK;
		}
		if (span_is(kind, "B") && next_field(&f, &a)) {
			uint32_t offset;

			if (!ladx_wlog_hex(a, &offset) || has_key || (has_bank && offset <= bank_end))
				return parse_fail(line, l.number);
			if (out) {
				out->bank = 1;
				out->offset = offset;
			}
			if (!payload(&f, 1, offset, &total, &pointer, &value, out))
				return parse_fail(line, l.number);
			if (out && out->st == LADX_WLOG_OK && out->n)
				out->st = ladx_wlog_bank(p->log, p->group, out->offset, out->chunk, out->n);
			bank_end = offset + total;
			has_bank = 1;
		} else if (span_is(kind, "T") && next_field(&f, &a) && next_field(&f, &b)) {
			Key key;
			LadxWlogKey wkey;

			key.table = table_of(p, a);
			key.alt = !is_hex_text(b);
			key.index = 0;
			key.name = b;
			if (key.table < 0 || (key.alt ? !is_name(b) : !ladx_wlog_hex(b, &key.index)) ||
			    (has_key && key_cmp(&last_key, &key) >= 0))
				return parse_fail(line, l.number);
			if (out)
				out->bank = 0;
			if (!payload(&f, 0, 0, &total, &pointer, &value, out))
				return parse_fail(line, l.number);
			last_key = key;
			has_key = 1;
			wkey.index = key.index;
			wkey.alt.at = key.alt ? b.at : NULL;
			wkey.alt.len = key.alt ? b.len : 0;
			if (out && out->st == LADX_WLOG_OK)
				out->st = pointer ? ladx_wlog_pointer(p->log, p->group, key.table, wkey, value)
						  : ladx_wlog_table(p->log, p->group, key.table, wkey, p->log->scratch, out->n);
		} else {
			return parse_fail(line, l.number);
		}
		records++;
		if (out && out->st != LADX_WLOG_OK)
			return out->st;
	}
	if (open && records == 0)
		return parse_fail(line, group_line);
	return LADX_WLOG_OK;
}

LadxWlogStatus ladx_wlog_groups(LadxSpan text, LadxSpan *groups, int cap, int *count, int *line)
{
	Parse p;
	LadxWlogStatus st;

	memset(&p, 0, sizeof p);
	p.groups = groups;
	p.cap = cap;
	st = parse_log(&p, text, line);
	*count = p.count;
	if (st == LADX_WLOG_OK && p.overflow)
		return LADX_WLOG_OVERFLOW;
	return st;
}

LadxWlogStatus ladx_wlog_project(LadxWlog *log, LadxSpan text, LadxSpan group, LadxWlogSink sink, void *ctx,
				 int *line)
{
	Parse p;
	LadxWlogStatus st;

	memset(&p, 0, sizeof p);
	if ((st = parse_log(&p, text, line)) != LADX_WLOG_OK)
		return st;
	if ((st = ladx_wlog_begin(log, p.tables_names, sink, ctx)) != LADX_WLOG_OK)
		return st;
	p.log = log;
	p.group = group;
	if ((st = parse_log(&p, text, line)) != LADX_WLOG_OK)
		return st;
	return ladx_wlog_end(log);
}
