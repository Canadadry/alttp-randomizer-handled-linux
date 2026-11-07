#include "ladx_tables.h"

#include "wlog_token.h"

#include <string.h>

static const char *const status_names[LADX_TABLES_STATUS_COUNT] = {
	[LADX_TABLES_OK] = "ok",
	[LADX_TABLES_BAD_VERSION] = "bad-version",
	[LADX_TABLES_BAD_LINE] = "bad-line",
	[LADX_TABLES_BAD_COMPONENT] = "bad-component",
	[LADX_TABLES_BAD_VALUES] = "bad-values",
	[LADX_TABLES_BAD_TOKEN] = "bad-token",
	[LADX_TABLES_TABLES_DIFFER] = "tables-differ",
	[LADX_TABLES_DUP_GROUP] = "dup-group",
	[LADX_TABLES_DUP_UNIT] = "dup-unit",
	[LADX_TABLES_NO_GROUP_FILE] = "no-group-file",
	[LADX_TABLES_NO_UNIT] = "no-unit",
	[LADX_TABLES_OVERFLOW] = "overflow",
	[LADX_TABLES_NO_ENTRY] = "no-entry",
	[LADX_TABLES_BUFFER] = "buffer",
	[LADX_TABLES_RANGE] = "range",
};

const char *ladx_tables_status_name(LadxTablesStatus status)
{
	return (unsigned)status < LADX_TABLES_STATUS_COUNT ? status_names[status] : "unknown";
}

/* generateRom's 0/1 assembler.const rows (the inventory's asm-consts group). */
typedef enum AsmConst {
	ASM_HARD_MODE,
	ASM_DUNGEON_CHAIN,
	ASM_ITEM_DIALOGS,
	ASM_KEYSANITY_KEYS,
	ASM_KEYSANITY_NIGHTMARE_KEYS,
	ASM_KEYSANITY_BEAKS,
	ASM_KEYSANITY_MAPS,
	ASM_COUNT
} AsmConst;

static const char *const asm_names[ASM_COUNT] = {
	"HARD_MODE",	   "DUNGEON_CHAIN",   "ITEM_DIALOGS",	"KEYSANITY_KEYS",
	"KEYSANITY_NIGHTMARE_KEYS", "KEYSANITY_BEAKS", "KEYSANITY_MAPS",
};

static int value_is(const LadxSettings *s, LadxSettingId id, const char *name)
{
	return strcmp(ladx_setting_value_name(id, s->value[id]), name) == 0;
}

static int asm_value(AsmConst c, const LadxSettings *s)
{
	switch (c) {
	case ASM_HARD_MODE:
		return !value_is(s, LADX_SETTING_HARDMODE, "none");
	case ASM_DUNGEON_CHAIN:
		return value_is(s, LADX_SETTING_OVERWORLD, "dungeonchain");
	case ASM_ITEM_DIALOGS:
		return !value_is(s, LADX_SETTING_TEXTMODE, "notextbox");
	case ASM_KEYSANITY_KEYS:
		return value_is(s, LADX_SETTING_DUNGEON_KEYS, "keysanity");
	case ASM_KEYSANITY_NIGHTMARE_KEYS:
		return value_is(s, LADX_SETTING_NIGHTMARE_KEYS, "keysanity");
	case ASM_KEYSANITY_BEAKS:
		return value_is(s, LADX_SETTING_DUNGEON_BEAKS, "keysanity");
	case ASM_KEYSANITY_MAPS:
		return value_is(s, LADX_SETTING_DUNGEON_MAPS, "keysanity");
	default:
		return 0;
	}
}

/* Text helpers -------------------------------------------------------------------------------- */

static LadxSpan sub(LadxSpan s, size_t from, size_t to)
{
	LadxSpan r;

	r.at = s.at + from;
	r.len = to - from;
	return r;
}

static int span_eq(LadxSpan a, LadxSpan b)
{
	return a.len == b.len && memcmp(a.at, b.at, a.len) == 0;
}

static int span_is(LadxSpan a, const char *s)
{
	return a.len == strlen(s) && memcmp(a.at, s, a.len) == 0;
}

static int starts(LadxSpan a, const char *s)
{
	size_t n = strlen(s);

	return a.len >= n && memcmp(a.at, s, n) == 0;
}

/* Splits off the next space-separated field of *rest; false when none is left. */
static int field(LadxSpan *rest, LadxSpan *out)
{
	size_t i = 0;

	if (rest->at == NULL)
		return 0;
	while (i < rest->len && rest->at[i] != ' ')
		i++;
	*out = sub(*rest, 0, i);
	if (i == rest->len)
		rest->at = NULL, rest->len = 0;
	else
		*rest = sub(*rest, i + 1, rest->len);
	return 1;
}

static int name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
	       c == '-';
}

static int is_name(LadxSpan s)
{
	size_t i;

	if (s.len == 0)
		return 0;
	for (i = 0; i < s.len; i++)
		if (!name_char(s.at[i]))
			return 0;
	return 1;
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static int is_hex(LadxSpan s)
{
	size_t i;

	if (s.len == 0)
		return 0;
	for (i = 0; i < s.len; i++)
		if (hex_digit(s.at[i]) < 0)
			return 0;
	return 1;
}

/* Canonical lowercase hex (no leading zero) of at most 32 bits. */
static int parse_hex(LadxSpan s, uint32_t *v)
{
	size_t i;

	if (!is_hex(s) || s.len > 8 || (s.len > 1 && s.at[0] == '0'))
		return 0;
	*v = 0;
	for (i = 0; i < s.len; i++)
		*v = *v << 4 | (uint32_t)hex_digit(s.at[i]);
	return 1;
}

/* A file's lines: each ends with '\n', and holds no NUL. */
typedef struct Lines {
	LadxSpan rest;
	int number;
} Lines;

static int next_line(Lines *l, LadxSpan *line)
{
	size_t i = 0;

	if (l->rest.len == 0)
		return 0;
	while (i < l->rest.len && l->rest.at[i] != '\n')
		i++;
	*line = sub(l->rest, 0, i);
	l->rest = i < l->rest.len ? sub(l->rest, i + 1, l->rest.len) : sub(l->rest, i, i);
	l->number++;
	return 1;
}

/* Opening ------------------------------------------------------------------------------------- */

static LadxTablesStatus fail(LadxTablesError *err, LadxTablesStatus status, const char *file, int line)
{
	err->status = status;
	err->file = file;
	err->line = line;
	return status;
}

static int file_ok(LadxSpan text)
{
	return text.len > 0 && text.at[text.len - 1] == '\n' && memchr(text.at, '\0', text.len) == NULL;
}

static int component_ok(LadxSpan c)
{
	char key[32];
	int i;

	if (span_is(c, "count"))
		return 1;
	if (starts(c, "asm:")) {
		for (i = 0; i < ASM_COUNT; i++)
			if (span_is(sub(c, 4, c.len), asm_names[i]))
				return 1;
		return 0;
	}
	if (c.len >= sizeof key)
		return 0;
	memcpy(key, c.at, c.len);
	key[c.len] = '\0';
	return ladx_setting_find(key) != LADX_SETTING_COUNT;
}

/* write_log v1 tokens of a record; pointer allowed alone in a T record. */
static int tokens_ok(LadxSpan rest, int bank)
{
	LadxSpan tok;
	LadxWlogToken t;
	int count = 0, pointer = 0;

	while (field(&rest, &tok)) {
		count++;
		if (!ladx_wlog_token(tok, &t) || (t.kind == LADX_WLOG_TOKEN_KEEP && !bank) ||
		    (t.kind == LADX_WLOG_TOKEN_POINTER && bank))
			return 0;
		pointer |= t.kind == LADX_WLOG_TOKEN_POINTER;
	}
	return pointer ? count == 1 : !bank || count > 0;
}

static int table_index(const LadxTables *t, LadxSpan name)
{
	LadxSpan rest = t->tables, tok;
	int i = 0;

	while (field(&rest, &tok)) {
		if (span_eq(tok, name))
			return i;
		i++;
	}
	return -1;
}

static int record_ok(const LadxTables *t, LadxSpan line)
{
	LadxSpan rest = line, kind, a, b;
	uint32_t v;

	if (!field(&rest, &kind) || !field(&rest, &a))
		return 0;
	if (span_is(kind, "B"))
		return parse_hex(a, &v) && tokens_ok(rest, 1);
	if (!field(&rest, &b) || table_index(t, a) < 0)
		return 0;
	if (!(is_hex(b) ? parse_hex(b, &v) : is_name(b) && b.len <= LADX_WLOG_NAME_MAX))
		return 0;
	return tokens_ok(rest, 0);
}

static LadxTablesStatus open_group(LadxTables *t, const LadxTablesFile *f, int gi, LadxTablesError *err)
{
	LadxTablesGroup *g = &t->group[gi];
	LadxTablesEntry *e = NULL;
	Lines l;
	LadxSpan line, rest, tok, tables;
	int body_lines = 0, nothing = 0, i;

	l.rest = f->text;
	l.number = 0;
	g->file = f->name;
	g->first_unit = t->unit_count;
	g->unit_count = 0;
	if (!file_ok(f->text))
		return fail(err, LADX_TABLES_BAD_LINE, f->name, 0);
	if (!next_line(&l, &line) || !span_is(line, "ladx-tables 1"))
		return fail(err, LADX_TABLES_BAD_VERSION, f->name, 1);
	if (!next_line(&l, &line) || !starts(line, "group ") || !is_name(sub(line, 6, line.len)))
		return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
	g->name = sub(line, 6, line.len);
	for (i = 0; i < gi; i++)
		if (span_eq(t->group[i].name, g->name))
			return fail(err, LADX_TABLES_DUP_GROUP, f->name, l.number);
	if (!next_line(&l, &line) || !starts(line, "tables ") || line.len == 7)
		return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
	tables = sub(line, 7, line.len);
	if (gi == 0)
		t->tables = tables;
	else if (!span_eq(tables, t->tables))
		return fail(err, LADX_TABLES_TABLES_DIFFER, f->name, l.number);
	rest = tables;
	while (field(&rest, &tok))
		if (!is_name(tok) || tok.len > LADX_WLOG_NAME_MAX)
			return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
	while (next_line(&l, &line)) {
		LadxTablesUnit *u = t->unit_count > g->first_unit ? &t->unit[t->unit_count - 1] : NULL;

		if (starts(line, "unit ") || starts(line, "E ") || span_is(line, "E")) {
			if (e && !body_lines)
				return fail(err, LADX_TABLES_BAD_LINE, f->name, e->line);
			e = NULL;
		}
		if (starts(line, "unit ")) {
			LadxSpan id;

			if (u && u->entry_count == 0)
				return fail(err, LADX_TABLES_BAD_LINE, f->name, u->line);
			rest = sub(line, 5, line.len);
			field(&rest, &id);
			if (id.len <= g->name.len + 1 || id.len > LADX_TABLES_UNIT_ID_MAX ||
			    memcmp(id.at, g->name.at, g->name.len) != 0 || id.at[g->name.len] != '.')
				return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
			for (i = g->first_unit; i < t->unit_count; i++)
				if (span_eq(t->unit[i].id, id))
					return fail(err, LADX_TABLES_DUP_UNIT, f->name, l.number);
			if (t->unit_count == LADX_TABLES_UNIT_MAX)
				return fail(err, LADX_TABLES_OVERFLOW, f->name, l.number);
			u = &t->unit[t->unit_count++];
			g->unit_count++;
			u->id = id;
			u->group = gi;
			u->line = l.number;
			u->component_count = 0;
			u->first_entry = t->entry_count;
			u->entry_count = 0;
			while (field(&rest, &tok)) {
				if (!component_ok(tok))
					return fail(err, LADX_TABLES_BAD_COMPONENT, f->name, l.number);
				for (i = 0; i < u->component_count; i++)
					if (span_eq(u->component[i], tok))
						return fail(err, LADX_TABLES_BAD_COMPONENT, f->name, l.number);
				if (u->component_count == LADX_TABLES_COMPONENT_MAX)
					return fail(err, LADX_TABLES_OVERFLOW, f->name, l.number);
				u->component[u->component_count++] = tok;
			}
		} else if (starts(line, "E ") || span_is(line, "E")) {
			int values = 0;

			if (!u)
				return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
			rest = line.len > 1 ? sub(line, 2, line.len) : sub(line, 1, 1);
			if (line.len == 1)
				rest.at = NULL;
			while (field(&rest, &tok)) {
				if (!(span_is(tok, "\"\"") || is_name(tok)))
					return fail(err, LADX_TABLES_BAD_VALUES, f->name, l.number);
				values++;
			}
			if (values != u->component_count)
				return fail(err, LADX_TABLES_BAD_VALUES, f->name, l.number);
			if (t->entry_count == LADX_TABLES_ENTRY_MAX)
				return fail(err, LADX_TABLES_OVERFLOW, f->name, l.number);
			e = &t->entry[t->entry_count++];
			u->entry_count++;
			e->values = line.len > 1 ? sub(line, 2, line.len) : sub(line, 1, 1);
			e->records = sub(line, line.len, line.len);
			e->line = l.number;
			body_lines = nothing = 0;
		} else if (e && !nothing && span_is(line, "nothing") && !body_lines) {
			nothing = 1;
			body_lines = 1;
		} else if (e && !nothing && (starts(line, "B ") || starts(line, "T "))) {
			if (!record_ok(t, line))
				return fail(err, LADX_TABLES_BAD_TOKEN, f->name, l.number);
			if (!body_lines)
				e->records.at = line.at;
			e->records.len = (size_t)(line.at + line.len + 1 - e->records.at);
			body_lines++;
		} else {
			return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
		}
	}
	if (e && !body_lines)
		return fail(err, LADX_TABLES_BAD_LINE, f->name, e->line);
	if (g->unit_count == 0 || t->unit[t->unit_count - 1].entry_count == 0)
		return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
	return LADX_TABLES_OK;
}

static LadxTablesStatus open_order(LadxTables *t, const LadxTablesFile *f, LadxTablesError *err)
{
	Lines l;
	LadxSpan line, rest, side, group, unit, call;
	int sections = 0, i;

	l.rest = f->text;
	l.number = 0;
	if (!file_ok(f->text))
		return fail(err, LADX_TABLES_BAD_LINE, f->name, 0);
	if (!next_line(&l, &line) || !span_is(line, "ladx-tables-order 1"))
		return fail(err, LADX_TABLES_BAD_VERSION, f->name, 1);
	while (next_line(&l, &line)) {
		LadxTablesRow *r;

		rest = line;
		field(&rest, &side);
		if (span_is(side, "section")) {
			LadxSpan a, b;

			if (!field(&rest, &a) || !field(&rest, &b) || rest.at || !a.len || !b.len)
				return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
			sections++;
			continue;
		}
		if (!sections || !(span_is(side, "table") || span_is(side, "C")) || !field(&rest, &group) ||
		    !field(&rest, &unit) || !rest.at || !rest.len || !is_name(group))
			return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
		call = rest;
		if (call.at[0] == ' ' || call.at[call.len - 1] == ' ')
			return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
		if (t->row_count == LADX_TABLES_ROW_MAX)
			return fail(err, LADX_TABLES_OVERFLOW, f->name, l.number);
		r = &t->row[t->row_count++];
		r->c_row = span_is(side, "C");
		r->group_name = group;
		r->call = call;
		r->group = -1;
		r->unit = -1;
		for (i = 0; i < t->group_count; i++)
			if (span_eq(t->group[i].name, group))
				r->group = i;
		if (r->c_row) {
			if (!span_is(unit, "-"))
				return fail(err, LADX_TABLES_BAD_LINE, f->name, l.number);
			continue;
		}
		if (r->group < 0)
			return fail(err, LADX_TABLES_NO_GROUP_FILE, f->name, l.number);
		for (i = t->group[r->group].first_unit;
		     i < t->group[r->group].first_unit + t->group[r->group].unit_count; i++)
			if (span_eq(t->unit[i].id, unit))
				r->unit = i;
		if (r->unit < 0)
			return fail(err, LADX_TABLES_NO_UNIT, f->name, l.number);
	}
	return LADX_TABLES_OK;
}

LadxTablesStatus ladx_tables_open(LadxTables *t, const LadxTablesFile *order, const LadxTablesFile *groups,
				  int group_count, LadxTablesError *err)
{
	LadxTablesStatus st;
	int i;

	memset(err, 0, sizeof *err);
	t->group_count = t->unit_count = t->entry_count = t->row_count = 0;
	t->order_file = order->name;
	t->tables.at = NULL;
	t->tables.len = 0;
	if (group_count < 0 || group_count > LADX_TABLES_GROUP_MAX)
		return fail(err, LADX_TABLES_OVERFLOW, NULL, 0);
	for (i = 0; i < group_count; i++) {
		if ((st = open_group(t, &groups[i], i, err)) != LADX_TABLES_OK) {
			t->group_count = 0;
			return st;
		}
		t->group_count++;
	}
	if ((st = open_order(t, order, err)) != LADX_TABLES_OK) {
		t->group_count = 0;
		return st;
	}
	return LADX_TABLES_OK;
}

int ladx_tables_group_count(const LadxTables *t)
{
	return t->group_count;
}

int ladx_tables_unit_count(const LadxTables *t)
{
	return t->unit_count;
}

int ladx_tables_entry_count(const LadxTables *t)
{
	return t->entry_count;
}

LadxSpan ladx_tables_names(const LadxTables *t)
{
	return t->tables;
}

LadxSpan ladx_tables_unit_id(const LadxTables *t, int unit)
{
	return t->unit[unit].id;
}

bool ladx_tables_unit_uses_count(const LadxTables *t, int unit)
{
	int i;

	for (i = 0; i < t->unit[unit].component_count; i++)
		if (span_is(t->unit[unit].component[i], "count"))
			return true;
	return false;
}

/* Key and walk -------------------------------------------------------------------------------- */

/* Appends text to buf (cap with its NUL); false when it does not fit. */
static int append(char *buf, size_t cap, size_t *len, const char *text, size_t n)
{
	if (*len + n >= cap)
		return 0;
	memcpy(buf + *len, text, n);
	*len += n;
	buf[*len] = '\0';
	return 1;
}

static int append_decimal(char *buf, size_t cap, size_t *len, int v)
{
	char tmp[16];
	int n = 0;
	unsigned u = v < 0 ? 0u - (unsigned)v : (unsigned)v;

	do {
		tmp[sizeof tmp - 1 - n++] = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	if (v < 0)
		tmp[sizeof tmp - 1 - n++] = '-';
	return append(buf, cap, len, tmp + sizeof tmp - n, (size_t)n);
}

/* The unit's key as an E line writes its values. */
static int key_values(const LadxTablesUnit *u, const LadxSettings *s, int spot_count, char *buf, size_t cap)
{
	size_t len = 0;
	int i, k;

	buf[0] = '\0';
	for (i = 0; i < u->component_count; i++) {
		LadxSpan c = u->component[i];

		if (i && !append(buf, cap, &len, " ", 1))
			return 0;
		if (span_is(c, "count")) {
			if (!append_decimal(buf, cap, &len, spot_count))
				return 0;
		} else if (starts(c, "asm:")) {
			for (k = 0; k < ASM_COUNT; k++)
				if (span_is(sub(c, 4, c.len), asm_names[k]))
					break;
			if (!append(buf, cap, &len, asm_value((AsmConst)k, s) ? "1" : "0", 1))
				return 0;
		} else {
			char key[32];
			LadxSettingId id;
			const char *name;

			memcpy(key, c.at, c.len);
			key[c.len] = '\0';
			id = ladx_setting_find(key);
			name = ladx_setting_value_name(id, s->value[id]);
			if (!*name)
				name = "\"\"";
			if (!append(buf, cap, &len, name, strlen(name)))
				return 0;
		}
	}
	return 1;
}

bool ladx_tables_select(const LadxTables *t, int unit, const LadxSettings *validated, int spot_count,
			LadxTablesStep *step, LadxTablesError *err)
{
	const LadxTablesUnit *u = &t->unit[unit];
	char values[LADX_TABLES_VALUES_MAX + 1];
	int i;

	memset(err, 0, sizeof *err);
	if (key_values(u, validated, spot_count, values, sizeof values)) {
		for (i = u->first_entry; i < u->first_entry + u->entry_count; i++) {
			if (span_is(t->entry[i].values, values)) {
				step->c_row = false;
				step->group = t->group[u->group].name;
				step->unit = unit;
				step->entry = &t->entry[i];
				step->file = t->group[u->group].file;
				step->call.at = NULL;
				step->call.len = 0;
				return true;
			}
		}
	}
	fail(err, LADX_TABLES_NO_ENTRY, t->group[u->group].file, u->line);
	memcpy(err->unit, u->id.at, u->id.len);
	err->unit[u->id.len] = '\0';
	memcpy(err->values, values, sizeof values);
	return false;
}

void ladx_tables_walk_init(LadxTablesWalk *w, const LadxTables *t, const LadxSettings *validated, int spot_count)
{
	w->t = t;
	w->settings = validated;
	w->spot_count = spot_count;
	w->row = 0;
	memset(w->seen, 0, sizeof w->seen);
}

bool ladx_tables_walk_next(LadxTablesWalk *w, LadxTablesStep *step, LadxTablesError *err)
{
	memset(err, 0, sizeof *err);
	while (w->row < w->t->row_count) {
		const LadxTablesRow *r = &w->t->row[w->row++];

		if (r->c_row) {
			step->c_row = true;
			step->group = r->group_name;
			step->call = r->call;
			step->unit = -1;
			step->entry = NULL;
			step->file = NULL;
			return true;
		}
		if (w->seen[r->unit])
			continue;
		w->seen[r->unit] = true;
		if (!ladx_tables_select(w->t, r->unit, w->settings, w->spot_count, step, err))
			return false;
		step->call = r->call;
		return true;
	}
	return false;
}

/* Records and applier ------------------------------------------------------------------------- */

void ladx_tables_records_init(LadxTablesRecords *it, const LadxTables *t, const LadxTablesStep *step)
{
	it->t = t;
	it->file = step->file;
	if (step->entry) {
		it->rest = step->entry->records;
		it->line = step->entry->line;
	} else {
		it->rest.at = NULL;
		it->rest.len = 0;
		it->line = 0;
	}
}

static uint32_t hex_value(LadxSpan s)
{
	uint32_t v = 0;

	parse_hex(s, &v);
	return v;
}

bool ladx_tables_records_next(LadxTablesRecords *it, LadxTablesRecord *rec, LadxAtom *buf, size_t cap,
			      LadxTablesError *err)
{
	Lines l;
	LadxSpan line, rest, kind, a, tok;
	size_t n = 0;

	memset(err, 0, sizeof *err);
	l.rest = it->rest;
	l.number = it->line;
	if (!next_line(&l, &line))
		return false;
	it->rest = l.rest;
	it->line = l.number;
	rest = line;
	field(&rest, &kind);
	field(&rest, &a);
	memset(rec, 0, sizeof *rec);
	rec->line = l.number;
	rec->atoms = buf;
	rec->table_record = span_is(kind, "T");
	if (rec->table_record) {
		LadxSpan key;

		rec->table = table_index(it->t, a);
		field(&rest, &key);
		if (is_hex(key))
			rec->key.index = hex_value(key);
		else
			rec->key.alt = key;
	} else {
		rec->offset = hex_value(a);
	}
	while (field(&rest, &tok)) {
		LadxWlogToken t;
		uint32_t q, i;

		ladx_wlog_token(tok, &t);
		if (t.kind == LADX_WLOG_TOKEN_POINTER) {
			rec->pointer = true;
			rec->value = t.q;
			continue;
		}
		if (t.n > cap - n) {
			fail(err, LADX_TABLES_BUFFER, it->file, l.number);
			return false;
		}
		q = t.kind == LADX_WLOG_TOKEN_KEEP ? rec->offset + (uint32_t)n : t.q;
		for (i = 0; i < t.n; i++)
			buf[n++] = t.kind == LADX_WLOG_TOKEN_LITERAL ? LADX_ATOM_LITERAL(ladx_wlog_token_byte(&t, i))
								     : (LadxAtom)(q + i);
	}
	rec->n = n;
	return true;
}

LadxTablesStatus ladx_tables_apply(const LadxTablesRecord *rec, const uint8_t *source, size_t source_len, uint8_t *out,
				   size_t out_len)
{
	size_t i;

	if (rec->table_record)
		return LADX_TABLES_OK;
	if ((uint64_t)rec->offset + rec->n > out_len)
		return LADX_TABLES_RANGE;
	for (i = 0; i < rec->n; i++)
		if (rec->atoms[i] >= 0 && (size_t)rec->atoms[i] >= source_len)
			return LADX_TABLES_RANGE;
	for (i = 0; i < rec->n; i++) {
		LadxAtom a = rec->atoms[i];

		out[rec->offset + i] = a >= 0 ? source[a] : (uint8_t)(-1 - a);
	}
	return LADX_TABLES_OK;
}

int ladx_tables_spot_count(const LadxLogic *logic)
{
	int i, n = 0;

	for (i = 0; i < ladx_logic_spot_count(logic); i++)
		if (ladx_logic_spot(logic, i)->kind != LADX_SPOT_KEY_LOCATION)
			n++;
	return n;
}
