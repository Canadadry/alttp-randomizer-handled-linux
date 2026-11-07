#include "ladx_logic_builder.h"
#include "ladx_tables.h"
#include "support.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_tables"

static LadxTables tables;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static LadxSpan span(const char *s)
{
	LadxSpan sp;

	sp.at = s;
	sp.len = strlen(s);
	return sp;
}

static int span_is(LadxSpan sp, const char *s)
{
	return sp.len == strlen(s) && memcmp(sp.at, s, sp.len) == 0;
}

#define N(a) (sizeof(a) / sizeof(a)[0])
#define TABLES_LINE "tables texts entities\n"

/* Synthetic tables: two table groups, a C group without a file, a fused unit. */
static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "table alpha alpha.1 patches.a.one(rom)\n"
				      "C beta - patches.b.first(rom, rnd)\n"
				      "table alpha alpha.2 patches.a.two(rom)\n"
				      "table alpha alpha.2 patches.a.two(rom) #2\n"
				      "table gamma gamma.1 patches.g.one(rom)\n"
				      "section romTables.py ROMWithTables.save\n"
				      "C beta - self.texts.store(self)\n"
				      "table alpha alpha.3 patches.a.three(rom)\n";

static const char *const alpha_text = "ladx-tables 1\n"
				      "group alpha\n" TABLES_LINE "unit alpha.1\n"
				      "E\n"
				      "B 10 0102\n"
				      "unit alpha.2 music\n"
				      "E \"\"\n"
				      "B 20 =2 @400+3 aa\n"
				      "T entities 5 p1234\n"
				      "E off\n"
				      "nothing\n"
				      "unit alpha.3 nagmessages\n"
				      "E 0\n"
				      "T texts Alt1F5 @30+2\n"
				      "E 1\n"
				      "T texts 1ff 0bcd\n";

static const char *const gamma_text = "ladx-tables 1\n"
				      "group gamma\n" TABLES_LINE "unit gamma.1 overworld count\n"
				      "E normal 225\n"
				      "B 7 =1\n"
				      "E normal 226\n"
				      "B 7 ff\n";

static LadxTablesStatus open_texts(const char *order, const char *const *groups, const char *const *names, int n,
				   LadxTablesError *err)
{
	LadxTablesFile o, g[8];
	int i;

	o.name = "order.txt";
	o.text = span(order);
	for (i = 0; i < n; i++) {
		g[i].name = names[i];
		g[i].text = span(groups[i]);
	}
	return ladx_tables_open(&tables, &o, g, n, err);
}

static void open_synthetic(void)
{
	static const char *const groups[] = {alpha_text, gamma_text};
	static const char *const names[] = {"alpha.txt", "gamma.txt"};
	LadxTablesError err;

	if (open_texts(order_text, groups, names, 2, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s %s:%d", ladx_tables_status_name(err.status),
			  err.file ? err.file : "-", err.line);
}

static void defaults(LadxSettings *s)
{
	ladx_settings_defaults(s);
	ladx_settings_validate(s);
}

/* The walk's steps as "<group> <unit or -> <first record line, nothing, or a C row's call>|...". */
static void walk_text(const LadxSettings *s, int count, char *buf, size_t cap)
{
	LadxTablesWalk w;
	LadxTablesStep step;
	LadxTablesError err;
	size_t len = 0;

	buf[0] = '\0';
	ladx_tables_walk_init(&w, &tables, s, count);
	while (ladx_tables_walk_next(&w, &step, &err)) {
		const char *rec = step.call.at;
		size_t rec_len = step.call.len;
		LadxSpan unit = step.c_row ? span("-") : ladx_tables_unit_id(&tables, step.unit);

		if (!step.c_row) {
			rec = step.entry->records.len ? step.entry->records.at : "nothing";
			rec_len = step.entry->records.len ? (size_t)(strchr(rec, '\n') - rec) : strlen(rec);
		}
		len += (size_t)snprintf(buf + len, cap - len, "%.*s %.*s %.*s|", (int)step.group.len, step.group.at,
					(int)unit.len, unit.at, (int)rec_len, rec);
		if (len >= cap)
			test_fail(TEST, "walk text over %zu bytes", cap);
	}
	if (err.status != LADX_TABLES_OK)
		snprintf(buf + len, cap - len, "ERROR %s %s:%d %s: %s", ladx_tables_status_name(err.status),
			 err.file ? err.file : "-", err.line, err.unit, err.values);
}

static void expect_walk(const char *what, const LadxSettings *s, int count, const char *want)
{
	char got[1024];

	walk_text(s, count, got, sizeof got);
	checks++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: walk\n  got  %s\n  want %s", what, got, want);
}

static void test_walk(void)
{
	LadxSettings s;

	open_synthetic();
	check(ladx_tables_group_count(&tables) == 2 && ladx_tables_unit_count(&tables) == 4 &&
		      ladx_tables_entry_count(&tables) == 7,
	      "synthetic counts");
	check(span_is(ladx_tables_names(&tables), "texts entities"), "tables names");
	{
		int first = -1;

		check(ladx_tables_find_group(&tables, span("gamma")) == 1 &&
			      ladx_tables_find_group(&tables, span("beta")) == -1,
		      "a group's index by name, -1 for a group without a file");
		check(ladx_tables_group_units(&tables, 0, &first) == 3 && first == 0 &&
			      ladx_tables_group_units(&tables, 1, &first) == 1 && first == 3 &&
			      span_is(ladx_tables_unit_id(&tables, first), "gamma.1"),
		      "a group's units: the first one and their count");
	}
	defaults(&s);
	expect_walk("defaults", &s, 225,
		    "alpha alpha.1 B 10 0102|beta - patches.b.first(rom, rnd)|alpha alpha.2 B 20 =2 @400+3 aa|gamma gamma.1 B 7 =1|"
		    "beta - self.texts.store(self)|alpha alpha.3 T texts Alt1F5 @30+2|");
}

static void set(LadxSettings *s, const char *assignment)
{
	if (ladx_settings_set(s, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
	ladx_settings_validate(s);
}

static void test_values(void)
{
	LadxSettings s;

	open_synthetic();
	defaults(&s);
	set(&s, "music=off");
	set(&s, "nagmessages=1");
	expect_walk("music off, nagmessages", &s, 226,
		    "alpha alpha.1 B 10 0102|beta - patches.b.first(rom, rnd)|alpha alpha.2 nothing|gamma gamma.1 B 7 ff|"
		    "beta - self.texts.store(self)|alpha alpha.3 T texts 1ff 0bcd|");
	defaults(&s);
	expect_walk("a count with no entry", &s, 999,
		    "alpha alpha.1 B 10 0102|beta - patches.b.first(rom, rnd)|alpha alpha.2 B 20 =2 @400+3 aa|"
		    "ERROR no-entry gamma.txt:4 gamma.1: normal 999");
	set(&s, "music=random");
	expect_walk("a value with no entry", &s, 225,
		    "alpha alpha.1 B 10 0102|beta - patches.b.first(rom, rnd)|ERROR no-entry alpha.txt:7 alpha.2: random");
	defaults(&s);
	set(&s, "overworld=dungeondive");
	expect_walk("a setting outside tier 1 at another value", &s, 225,
		    "alpha alpha.1 B 10 0102|beta - patches.b.first(rom, rnd)|alpha alpha.2 B 20 =2 @400+3 aa|"
		    "ERROR no-entry gamma.txt:4 gamma.1: dungeondive 225");
}

/* One unit per 0/1 assembler constant, each flipped by one setting. */
static void test_asm_constants(void)
{
	static const char *const order = "ladx-tables-order 1\n"
					 "section generator.py generateRom\n"
					 "table k k.1 a()\n"
					 "table k k.2 b()\n"
					 "table k k.3 c()\n"
					 "table k k.4 d()\n"
					 "table k k.5 e()\n"
					 "table k k.6 f()\n"
					 "table k k.7 g()\n";
	static const char *const group = "ladx-tables 1\n"
					 "group k\n" TABLES_LINE
					 "unit k.1 asm:HARD_MODE\nE 0\nB 1 00\nE 1\nB 1 01\n"
					 "unit k.2 asm:DUNGEON_CHAIN\nE 0\nB 2 00\nE 1\nB 2 01\n"
					 "unit k.3 asm:ITEM_DIALOGS\nE 0\nB 3 00\nE 1\nB 3 01\n"
					 "unit k.4 asm:KEYSANITY_KEYS\nE 0\nB 4 00\nE 1\nB 4 01\n"
					 "unit k.5 asm:KEYSANITY_NIGHTMARE_KEYS\nE 0\nB 5 00\nE 1\nB 5 01\n"
					 "unit k.6 asm:KEYSANITY_BEAKS\nE 0\nB 6 00\nE 1\nB 6 01\n"
					 "unit k.7 asm:KEYSANITY_MAPS\nE 0\nB 7 00\nE 1\nB 7 01\n";
	static const char *const flips[][2] = {
		{"hardmode=oracle", "k k.1 B 1 01|k k.2 B 2 00|k k.3 B 3 01|k k.4 B 4 00|k k.5 B 5 00|k k.6 B 6 00|k k.7 B 7 00|"},
		{"overworld=dungeonchain", "k k.1 B 1 00|k k.2 B 2 01|k k.3 B 3 01|k k.4 B 4 00|k k.5 B 5 00|k k.6 B 6 00|k k.7 B 7 00|"},
		{"textmode=notextbox", "k k.1 B 1 00|k k.2 B 2 00|k k.3 B 3 00|k k.4 B 4 00|k k.5 B 5 00|k k.6 B 6 00|k k.7 B 7 00|"},
		{"dungeon_keys=keysanity", "k k.1 B 1 00|k k.2 B 2 00|k k.3 B 3 01|k k.4 B 4 01|k k.5 B 5 00|k k.6 B 6 00|k k.7 B 7 00|"},
		{"nightmare_keys=keysanity", "k k.1 B 1 00|k k.2 B 2 00|k k.3 B 3 01|k k.4 B 4 00|k k.5 B 5 01|k k.6 B 6 00|k k.7 B 7 00|"},
		{"dungeon_beaks=keysanity", "k k.1 B 1 00|k k.2 B 2 00|k k.3 B 3 01|k k.4 B 4 00|k k.5 B 5 00|k k.6 B 6 01|k k.7 B 7 00|"},
		{"dungeon_maps=keysanity", "k k.1 B 1 00|k k.2 B 2 00|k k.3 B 3 01|k k.4 B 4 00|k k.5 B 5 00|k k.6 B 6 00|k k.7 B 7 01|"},
	};
	static const char *const names[] = {"k.txt"};
	LadxTablesError err;
	LadxSettings s;
	size_t i;

	check(open_texts(order, &group, names, 1, &err) == LADX_TABLES_OK, "asm tables open");
	defaults(&s);
	expect_walk("asm defaults", &s, 0,
		    "k k.1 B 1 00|k k.2 B 2 00|k k.3 B 3 01|k k.4 B 4 00|k k.5 B 5 00|k k.6 B 6 00|k k.7 B 7 00|");
	for (i = 0; i < N(flips); i++) {
		defaults(&s);
		set(&s, flips[i][0]);
		expect_walk(flips[i][0], &s, 0, flips[i][1]);
	}
}

static char mutated[3][4096];

/* The synthetic file which (0 order, 1 alpha, 2 gamma) with its first `from` replaced by `to`. */
static void expect_error(int which, const char *from, const char *to, LadxTablesStatus status, const char *file,
			 int line)
{
	static const char *const names[] = {"alpha.txt", "gamma.txt"};
	const char *texts[3];
	const char *at;
	LadxTablesError err;
	LadxTablesStatus got;
	int i;

	texts[0] = order_text;
	texts[1] = alpha_text;
	texts[2] = gamma_text;
	for (i = 0; i < 3; i++)
		strcpy(mutated[i], texts[i]);
	at = strstr(texts[which], from);
	if (!at)
		test_fail(TEST, "no %s in file %d", from, which);
	sprintf(mutated[which] + (at - texts[which]), "%s%s", to, at + strlen(from));
	texts[0] = mutated[0];
	texts[1] = mutated[1];
	texts[2] = mutated[2];
	got = open_texts(texts[0], texts + 1, names, 2, &err);
	checks++;
	if (got != status || err.status != status || strcmp(err.file ? err.file : "-", file) != 0 || err.line != line)
		test_fail(TEST, "%s -> %s: got %s %s:%d, want %s %s:%d", from, to, ladx_tables_status_name(got),
			  err.file ? err.file : "-", err.line, ladx_tables_status_name(status), file, line);
}

static void test_errors(void)
{
	expect_error(0, "ladx-tables-order 1", "ladx-tables-order 2", LADX_TABLES_BAD_VERSION, "order.txt", 1);
	expect_error(1, "ladx-tables 1", "ladx-tables-order 1", LADX_TABLES_BAD_VERSION, "alpha.txt", 1);
	expect_error(1, "unit alpha.3 nagmessages", "unit alpha.3 nagmessage", LADX_TABLES_BAD_COMPONENT, "alpha.txt",
		     13);
	expect_error(2, "overworld count", "overworld asm:INV_SIZE", LADX_TABLES_BAD_COMPONENT, "gamma.txt", 4);
	expect_error(2, "overworld count", "overworld overworld", LADX_TABLES_BAD_COMPONENT, "gamma.txt", 4);
	expect_error(0, "alpha.3", "alpha.4", LADX_TABLES_NO_UNIT, "order.txt", 10);
	expect_error(0, "table gamma", "table delta", LADX_TABLES_NO_GROUP_FILE, "order.txt", 7);
	expect_error(1, "unit alpha.3", "unit alpha.1", LADX_TABLES_DUP_UNIT, "alpha.txt", 13);
	expect_error(2, "group gamma", "group alpha", LADX_TABLES_DUP_GROUP, "gamma.txt", 2);
	expect_error(2, "E normal 226", "E normal", LADX_TABLES_BAD_VALUES, "gamma.txt", 7);
	expect_error(2, "E normal 226", "E normal 226 1", LADX_TABLES_BAD_VALUES, "gamma.txt", 7);
	expect_error(1, "E off", "E o=ff", LADX_TABLES_BAD_VALUES, "alpha.txt", 11);
	expect_error(1, "@400+3", "@400+0", LADX_TABLES_BAD_TOKEN, "alpha.txt", 9);
	expect_error(1, "@400+3", "@0400+3", LADX_TABLES_BAD_TOKEN, "alpha.txt", 9);
	expect_error(1, " aa\n", " a\n", LADX_TABLES_BAD_TOKEN, "alpha.txt", 9);
	expect_error(1, " aa\n", "  aa\n", LADX_TABLES_BAD_TOKEN, "alpha.txt", 9);
	expect_error(1, "p1234", "p1234 00", LADX_TABLES_BAD_TOKEN, "alpha.txt", 10);
	expect_error(1, "T entities", "T rooms", LADX_TABLES_BAD_TOKEN, "alpha.txt", 10);
	expect_error(2, "B 7 =1", "B 7", LADX_TABLES_BAD_TOKEN, "gamma.txt", 6);
	expect_error(1, "T texts 1ff", "T texts 01ff", LADX_TABLES_BAD_TOKEN, "alpha.txt", 17);
	expect_error(2, "tables texts entities", "tables texts", LADX_TABLES_TABLES_DIFFER, "gamma.txt", 3);
	expect_error(1, "nothing", "B 0 00\nnothing", LADX_TABLES_BAD_LINE, "alpha.txt", 13);
	expect_error(1, "E off\nnothing\n", "E off\n", LADX_TABLES_BAD_LINE, "alpha.txt", 11);
	expect_error(1, "E \"\"\n", "", LADX_TABLES_BAD_LINE, "alpha.txt", 8);
	expect_error(1, "E 0\n", "", LADX_TABLES_BAD_LINE, "alpha.txt", 14);
	expect_error(2, "B 7 ff\n", "B 7 ff", LADX_TABLES_BAD_LINE, "gamma.txt", 0);
	expect_error(0, "C beta - patches.b.first(rom, rnd)", "C beta beta.1 x()", LADX_TABLES_BAD_LINE, "order.txt", 4);
	expect_error(0, "section generator.py generateRom\n", "", LADX_TABLES_BAD_LINE, "order.txt", 2);
	expect_error(0, "table alpha alpha.1 patches.a.one(rom)", "table alpha alpha.1", LADX_TABLES_BAD_LINE,
		     "order.txt", 3);
}

static char big_group[LADX_TABLES_UNIT_MAX * 32 + 128];

static void test_capacity(void)
{
	static const char *const order = "ladx-tables-order 1\nsection a b\n";
	static const char *const names[] = {"big.txt"};
	const char *texts[1];
	LadxTablesError err;
	size_t len;
	int i;

	len = (size_t)sprintf(big_group, "ladx-tables 1\ngroup big\n" TABLES_LINE);
	for (i = 1; i <= LADX_TABLES_UNIT_MAX; i++)
		len += (size_t)sprintf(big_group + len, "unit big.%d\nE\nnothing\n", i);
	texts[0] = big_group;
	check(open_texts(order, texts, names, 1, &err) == LADX_TABLES_OK, "units at capacity open");
	check(ladx_tables_unit_count(&tables) == LADX_TABLES_UNIT_MAX, "units at capacity");
	sprintf(big_group + len, "unit big.%d\nE\nnothing\n", i);
	check(open_texts(order, texts, names, 1, &err) == LADX_TABLES_OVERFLOW && err.line == 3 * LADX_TABLES_UNIT_MAX + 4 &&
		      strcmp(err.file, "big.txt") == 0,
	      "one unit over capacity");
}

/* The first entry the walk of s selects for unit, with its records iterator. */
static void records_of(const LadxSettings *s, const char *unit, LadxTablesRecords *it)
{
	LadxTablesWalk w;
	LadxTablesStep step;
	LadxTablesError err;

	ladx_tables_walk_init(&w, &tables, s, 225);
	while (ladx_tables_walk_next(&w, &step, &err))
		if (!step.c_row && span_is(ladx_tables_unit_id(&tables, step.unit), unit)) {
			ladx_tables_records_init(it, &tables, &step);
			return;
		}
	test_fail(TEST, "no step of %s", unit);
}

static void next_record(LadxTablesRecords *it, LadxTablesRecord *rec, LadxAtom *buf, size_t cap)
{
	LadxTablesError err;

	if (!ladx_tables_records_next(it, rec, buf, cap, &err))
		test_fail(TEST, "no record: %s", ladx_tables_status_name(err.status));
}

static void expect_atoms(const char *what, const LadxTablesRecord *rec, const LadxAtom *want, size_t n)
{
	checks++;
	if (rec->n != n || memcmp(rec->atoms, want, n * sizeof want[0]) != 0)
		test_fail(TEST, "%s: atoms differ", what);
}

static void expect_end(LadxTablesRecords *it)
{
	LadxTablesRecord rec;
	LadxTablesError err;
	LadxAtom buf[8];

	check(!ladx_tables_records_next(it, &rec, buf, N(buf), &err) && err.status == LADX_TABLES_OK, "records end");
}

#define LIT(b) LADX_ATOM_LITERAL(b)

static void test_records(void)
{
	static const LadxAtom bank[] = {0x20, 0x21, 0x400, 0x401, 0x402, LIT(0xaa)};
	static const LadxAtom alt[] = {0x30, 0x31}, lits[] = {LIT(0x0b), LIT(0xcd)};
	LadxTablesRecords it;
	LadxTablesRecord rec;
	LadxTablesError err;
	LadxSettings s;
	LadxAtom buf[16];

	open_synthetic();
	defaults(&s);
	records_of(&s, "alpha.2", &it);
	next_record(&it, &rec, buf, N(buf));
	check(!rec.table_record && rec.offset == 0x20 && rec.line == 9, "B record");
	expect_atoms("B 20 =2 @400+3 aa", &rec, bank, N(bank));
	next_record(&it, &rec, buf, N(buf));
	check(rec.table_record && rec.table == 1 && rec.key.alt.len == 0 && rec.key.index == 5 && rec.pointer &&
		      rec.value == 0x1234 && rec.n == 0 && rec.line == 10,
	      "T entities 5 p1234");
	expect_end(&it);

	records_of(&s, "alpha.3", &it);
	next_record(&it, &rec, buf, N(buf));
	check(rec.table_record && rec.table == 0 && span_is(rec.key.alt, "Alt1F5") && !rec.pointer, "alt key");
	expect_atoms("T texts Alt1F5 @30+2", &rec, alt, N(alt));
	expect_end(&it);

	set(&s, "nagmessages=1");
	records_of(&s, "alpha.3", &it);
	next_record(&it, &rec, buf, N(buf));
	check(rec.table_record && rec.key.alt.len == 0 && rec.key.index == 0x1ff, "index key");
	expect_atoms("T texts 1ff 0bcd", &rec, lits, N(lits));

	set(&s, "music=off");
	records_of(&s, "alpha.2", &it);
	expect_end(&it);

	defaults(&s);
	records_of(&s, "alpha.2", &it);
	check(!ladx_tables_records_next(&it, &rec, buf, 5, &err) && err.status == LADX_TABLES_BUFFER &&
		      err.line == 9 && strcmp(err.file, "alpha.txt") == 0,
	      "a payload over the buffer");
	records_of(&s, "alpha.3", &it);
	check(!ladx_tables_records_next(&it, &rec, buf, 1, &err) && err.status == LADX_TABLES_BUFFER, "a copy over the buffer");
}

static void test_apply(void)
{
	static uint8_t source[0x500], out[0x500], before[0x500];
	LadxTablesRecords it;
	LadxTablesRecord rec, trec;
	LadxSettings s;
	LadxAtom buf[16];
	size_t i;

	for (i = 0; i < sizeof source; i++) {
		source[i] = (uint8_t)(i * 7);
		out[i] = (uint8_t)(0xee ^ i);
	}
	/* Copied bytes come from the source even where the output already changed them. */
	out[0x400] = 0x11;
	memcpy(before, out, sizeof out);
	open_synthetic();
	defaults(&s);
	records_of(&s, "alpha.2", &it);
	next_record(&it, &rec, buf, N(buf));
	next_record(&it, &trec, buf + 8, 8);
	check(ladx_tables_apply(&rec, source, sizeof source, out, 0x25) == LADX_TABLES_RANGE, "write beyond the output");
	check(ladx_tables_apply(&rec, source, 0x402, out, sizeof out) == LADX_TABLES_RANGE, "copy beyond the source");
	check(memcmp(out, before, sizeof out) == 0, "a refused record writes nothing");
	check(ladx_tables_apply(&trec, source, sizeof source, out, sizeof out) == LADX_TABLES_OK &&
		      memcmp(out, before, sizeof out) == 0,
	      "a T record writes nothing");
	check(ladx_tables_apply(&rec, source, sizeof source, out, 0x26) == LADX_TABLES_OK, "apply");
	check(out[0x20] == source[0x20] && out[0x21] == source[0x21] && out[0x22] == source[0x400] &&
		      out[0x23] == source[0x401] && out[0x24] == source[0x402] && out[0x25] == 0xaa,
	      "applied bytes");
	check(memcmp(out, before, 0x20) == 0 && memcmp(out + 0x26, before + 0x26, sizeof out - 0x26) == 0,
	      "nothing else written");
}

static LadxLogic logic;

static void test_spot_count(void)
{
	int s, d, far;

	ladx_logic_begin(&logic);
	s = ladx_logic_new_location(&logic, -1, "S");
	d = ladx_logic_new_location(&logic, 1, "D");
	far = ladx_logic_new_location(&logic, 1, "F");
	ladx_logic_connect(&logic, s, d, ladx_req_none());
	ladx_logic_add_spot(&logic, s, LADX_SPOT_CHEST, "0x003", NULL, 0);
	ladx_logic_add_spot(&logic, d, LADX_SPOT_KEY_LOCATION, "KEY1", NULL, 0);
	ladx_logic_add_spot(&logic, d, LADX_SPOT_DROPPED_KEY, "0x010", NULL, 0);
	ladx_logic_add_spot(&logic, d, LADX_SPOT_KEY_LOCATION, "KEY2", NULL, 0);
	ladx_logic_add_spot(&logic, far, LADX_SPOT_CHEST, "0x011", NULL, 0);
	check(ladx_logic_finish(&logic, s), "the spot-count graph");
	check(ladx_logic_spot_count(&logic) == 4, "four reached spots");
	check(ladx_tables_spot_count(&logic) == 2, "two spots not KeyLocation");
}

#define DATA "../../data/ladx"
#define DATA_MAX (2 * 1024 * 1024)

static char data[DATA_MAX];
static size_t data_len;
static char file_names[LADX_TABLES_GROUP_MAX][64];

/* The file's text, appended to data; its span. */
static LadxSpan read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	LadxSpan sp;
	size_t n;

	if (!f)
		test_fail(TEST, "cannot open %s", path);
	n = fread(data + data_len, 1, DATA_MAX - data_len, f);
	if (n == DATA_MAX - data_len || ferror(f))
		test_fail(TEST, "%s: unreadable or over %d bytes in all", path, DATA_MAX);
	fclose(f);
	sp.at = data + data_len;
	sp.len = n;
	data_len += n;
	return sp;
}

static int name_cmp(const void *a, const void *b)
{
	return strcmp(a, b);
}

static void open_real(void)
{
	LadxTablesFile order, groups[LADX_TABLES_GROUP_MAX];
	LadxTablesError err;
	DIR *d = opendir(DATA);
	struct dirent *e;
	char path[256];
	int n = 0, i;

	if (!d)
		test_fail(TEST, "cannot open %s", DATA);
	while ((e = readdir(d)) != NULL) {
		size_t len = strlen(e->d_name);

		if (len < 5 || strcmp(e->d_name + len - 4, ".txt") != 0 || strcmp(e->d_name, "order.txt") == 0)
			continue;
		if (n == LADX_TABLES_GROUP_MAX || len >= sizeof file_names[0])
			test_fail(TEST, "%s: too many group files or a long name", DATA);
		strcpy(file_names[n++], e->d_name);
	}
	closedir(d);
	qsort(file_names, (size_t)n, sizeof file_names[0], name_cmp);
	data_len = 0;
	order.name = "order.txt";
	order.text = read_file(DATA "/order.txt");
	for (i = 0; i < n; i++) {
		snprintf(path, sizeof path, "%s/%s", DATA, file_names[i]);
		groups[i].name = file_names[i];
		groups[i].text = read_file(path);
	}
	if (ladx_tables_open(&tables, &order, groups, n, &err) != LADX_TABLES_OK)
		test_fail(TEST, "%s: %s %s:%d", DATA, ladx_tables_status_name(err.status), err.file ? err.file : "-",
			  err.line);
}

static int units_selected;

/* Every unit without a count component selects an entry for the folder's validated settings. */
static int check_case(const char *dir, const char *name)
{
	char path[1024], line[1024];
	LadxSettings s;
	LadxTablesStep step;
	LadxTablesError err;
	FILE *f;
	int u;

	snprintf(path, sizeof path, "%s/%s/config.txt", dir, name);
	f = fopen(path, "r");
	if (!f)
		test_fail(TEST, "cannot open %s", path);
	ladx_settings_defaults(&s);
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\n")] = '\0';
		if (ladx_settings_set(&s, line) != LADX_SETTINGS_OK)
			test_fail(TEST, "%s: cannot set %s", path, line);
	}
	fclose(f);
	ladx_settings_validate(&s);
	if (!ladx_settings_supported(&s, NULL))
		test_fail(TEST, "%s: unsupported settings", path);
	for (u = 0; u < ladx_tables_unit_count(&tables); u++) {
		if (ladx_tables_unit_uses_count(&tables, u))
			continue;
		if (!ladx_tables_select(&tables, u, &s, 0, &step, &err))
			test_fail(TEST, "%s: %s %s:%d unit %s: no entry for %s", name, ladx_tables_status_name(err.status),
				  err.file, err.line, err.unit, err.values);
		units_selected++;
	}
	return 1;
}

static int check_cases(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (!d)
		test_fail(TEST, "cannot open %s", dir);
	while ((e = readdir(d)) != NULL)
		if (e->d_name[0] != '.')
			n += check_case(dir, e->d_name);
	closedir(d);
	return n;
}

static void test_real_tables(void)
{
	int cases, combos, u, with_count = 0;

	open_real();
	check(ladx_tables_group_count(&tables) == 6, "data/ladx: 6 groups, bank3e's constants included");
	check(ladx_tables_unit_count(&tables) == 93, "data/ladx: 93 units");
	check(ladx_tables_entry_count(&tables) == 709, "data/ladx: 709 entries");
	for (u = 0; u < ladx_tables_unit_count(&tables); u++)
		with_count += ladx_tables_unit_uses_count(&tables, u);
	check(with_count == 1, "data/ladx: one unit keyed by count");
	cases = check_cases("cases");
	combos = check_cases("combos");
	check(cases > 0 && combos > 0, "case and combo folders");
	printf("%s: data/ladx opened (6 groups, 93 units, 709 entries); %d cases and %d combos select %d entries\n",
	       TEST, cases, combos, units_selected);
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	test_walk();
	test_values();
	test_asm_constants();
	test_errors();
	test_capacity();
	test_records();
	test_apply();
	test_spot_count();
	test_real_tables();
	printf("%s: synthetic walks, keys, open errors, records, applier and spot count (%d checks)\n", TEST, checks);
	return 0;
}
