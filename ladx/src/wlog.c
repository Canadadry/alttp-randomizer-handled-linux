#include "ladx_wlog.h"

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
