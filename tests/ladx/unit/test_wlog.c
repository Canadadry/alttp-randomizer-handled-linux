#include "ladx_wlog.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_wlog"
#define OUT_MAX (2 * LADX_WLOG_BANK_MAX + 4096)

/* Every expected text below is write_log.render's text of the same net maps. */

static LadxWlog wlog;
static char out[OUT_MAX];
static size_t out_len;
static int checks;

static void sink(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	if (out_len + len >= OUT_MAX)
		test_fail(TEST, "log text over %d bytes", OUT_MAX);
	memcpy(out + out_len, text, len);
	out_len += len;
	out[out_len] = '\0';
}

static LadxSpan span(const char *s)
{
	LadxSpan sp;

	sp.at = s;
	sp.len = strlen(s);
	return sp;
}

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void begin(void)
{
	out_len = 0;
	out[0] = '\0';
	check(ladx_wlog_begin(&wlog, span("texts entities"), sink, NULL) == LADX_WLOG_OK, "begin");
}

static void expect_text(const char *name, const char *want)
{
	check(ladx_wlog_end(&wlog) == LADX_WLOG_OK, name);
	if (strcmp(out, want) != 0)
		test_fail(TEST, "%s: got\n%s---\nwant\n%s---", name, out, want);
}

#define HEADER "ladx-rom-writes 1\ntables texts entities\n"

static void test_empty(void)
{
	begin();
	expect_text("empty log", HEADER);
}

static void bank(const char *group, uint32_t offset, const LadxAtom *atoms, size_t n)
{
	check(ladx_wlog_bank(&wlog, span(group), offset, atoms, n) == LADX_WLOG_OK, "bank record");
}

#define LIT(b) LADX_ATOM_LITERAL(b)
#define N(a) (sizeof(a) / sizeof(a)[0])

static void test_bank(void)
{
	static const LadxAtom touch1[] = {LIT(1), LIT(2)}, touch2[] = {LIT(3)};
	static const LadxAtom own[] = {0x200, 0x201, 0x202};
	static const LadxAtom keep[] = {LIT(0xaa), 0x301, 0x302, LIT(0xbb)};
	static const LadxAtom cont1[] = {0x1000, 0x1001}, cont2[] = {0x1002, 0x1003};
	static const LadxAtom last1[] = {LIT(1), LIT(2), LIT(3)}, last2[] = {LIT(0xff)};
	static const LadxAtom back1[] = {LIT(5)}, back2[] = {0x600};
	static const LadxAtom broken[] = {0x10, 0x11, 0x30};
	static const LadxAtom zero[] = {LIT(0)};

	begin();
	bank("fixed", 0x7ffff, zero, N(zero));
	bank("fixed", 0x102, touch2, N(touch2));
	bank("fixed", 0x100, touch1, N(touch1));
	bank("fixed", 0x200, own, N(own));
	bank("fixed", 0x300, keep, N(keep));
	bank("fixed", 0x402, cont2, N(cont2));
	bank("fixed", 0x400, cont1, N(cont1));
	bank("fixed", 0x500, last1, N(last1));
	bank("fixed", 0x501, last2, N(last2));
	bank("fixed", 0x600, back1, N(back1));
	bank("fixed", 0x600, back2, N(back2));
	bank("fixed", 0x700, broken, N(broken));
	expect_text("bank records", HEADER "G fixed\n"
					   "B 100 010203\n"
					   "B 200 =3\n"
					   "B 300 aa =2 bb\n"
					   "B 400 @1000+4\n"
					   "B 500 01ff03\n"
					   "B 600 =1\n"
					   "B 700 @10+2 @30+1\n"
					   "B 7ffff 00\n");
}

static LadxWlogKey index_key(uint32_t index)
{
	LadxWlogKey k;

	k.index = index;
	k.alt.at = NULL;
	k.alt.len = 0;
	return k;
}

static LadxWlogKey alt_key(const char *alt)
{
	LadxWlogKey k;

	k.index = 0;
	k.alt = span(alt);
	return k;
}

static void entry(int table, LadxWlogKey key, const LadxAtom *atoms, size_t n)
{
	check(ladx_wlog_table(&wlog, span("options"), table, key, atoms, n) == LADX_WLOG_OK, "table record");
}

static void pointer(int table, LadxWlogKey key, uint32_t value)
{
	check(ladx_wlog_pointer(&wlog, span("options"), table, key, value) == LADX_WLOG_OK, "pointer record");
}

static void test_tables(void)
{
	static const LadxAtom alt2[] = {LIT(1)}, alt10[] = {0x20, 0x21, 0x22, LIT(9)};
	static const LadxAtom e20[] = {LIT(0x12), LIT(0x34)}, e7[] = {LIT(0xab)};
	static const LadxAtom t1ff[] = {0x400, 0x401, LIT(0xc0), 0x402};
	static const LadxAtom old[] = {LIT(0xee), LIT(0xee), LIT(0xee)};

	begin();
	entry(1, alt_key("Alt2"), alt2, N(alt2));
	entry(1, index_key(0x20), old, N(old));
	pointer(1, index_key(0x7), 0x99);
	entry(1, alt_key("Alt10"), alt10, N(alt10));
	entry(0, index_key(0x1ff), old, N(old));
	entry(1, index_key(0x20), e20, N(e20));
	entry(0, index_key(0x3), NULL, 0);
	pointer(1, index_key(0x5), 0x1234);
	entry(1, index_key(0x7), e7, N(e7));
	entry(0, index_key(0x1ff), t1ff, N(t1ff));
	expect_text("table records", HEADER "G options\n"
					    "T texts 3\n"
					    "T texts 1ff @400+2 c0 @402+1\n"
					    "T entities 5 p1234\n"
					    "T entities 7 ab\n"
					    "T entities 20 1234\n"
					    "T entities Alt10 @20+3 09\n"
					    "T entities Alt2 01\n");
}

static void test_groups(void)
{
	static const LadxAtom a[] = {LIT(1), LIT(2)}, b[] = {LIT(3)}, c[] = {LIT(4)}, d[] = {0x20};

	begin();
	bank("fixed", 0x10, a, N(a));
	bank("options", 0x10, b, N(b));
	entry(0, index_key(1), c, N(c));
	bank("fixed", 0x20, d, N(d));
	expect_text("group changes", HEADER "G fixed\n"
					    "B 10 0102\n"
					    "G options\n"
					    "B 10 03\n"
					    "T texts 1 04\n"
					    "G fixed\n"
					    "B 20 =1\n");
}

static void test_bad_input(void)
{
	static const LadxAtom ok[] = {LIT(1)}, bad_atom[] = {LADX_ATOM_LITERAL(256)};
	LadxSpan bad_tables[] = {{"", 0}, {"texts ", 6}, {"texts texts", 11}, {"te xts!", 7}};
	size_t i;

	for (i = 0; i < N(bad_tables); i++)
		check(ladx_wlog_begin(&wlog, bad_tables[i], sink, NULL) == LADX_WLOG_BAD_INPUT, "bad tables line");
	begin();
	check(ladx_wlog_bank(&wlog, span("fixed"), 0x10, ok, 0) == LADX_WLOG_BAD_INPUT, "empty bank record");
	check(ladx_wlog_bank(&wlog, span("fixed"), 0x10, bad_atom, 1) == LADX_WLOG_BAD_INPUT, "literal over 0xff");
	check(ladx_wlog_bank(&wlog, span("fixed"), 0xffffffffu, bad_atom, 2) == LADX_WLOG_BAD_INPUT,
	      "offset past 2^32");
	check(ladx_wlog_bank(&wlog, span("a b"), 0x10, ok, 1) == LADX_WLOG_BAD_INPUT, "bad group name");
	check(ladx_wlog_bank(&wlog, span(""), 0x10, ok, 1) == LADX_WLOG_BAD_INPUT, "empty group name");
	check(ladx_wlog_table(&wlog, span("fixed"), 2, index_key(0), ok, 1) == LADX_WLOG_BAD_INPUT, "table out of range");
	check(ladx_wlog_table(&wlog, span("fixed"), -1, index_key(0), ok, 1) == LADX_WLOG_BAD_INPUT, "negative table");
	check(ladx_wlog_pointer(&wlog, span("fixed"), 0, alt_key("Alt 1"), 1) == LADX_WLOG_BAD_INPUT, "bad alt key");
	expect_text("refused records", HEADER);
}

static LadxAtom big[(LADX_WLOG_BANK_MAX > LADX_WLOG_ATOM_MAX ? LADX_WLOG_BANK_MAX : LADX_WLOG_ATOM_MAX) + 1];

static void test_long_record(void)
{
	char want[2 * 1000 + 64];
	size_t i, n = 0;

	for (i = 0; i < 1000; i++)
		big[i] = LIT(0x5a);
	begin();
	bank("fixed", 0, big, 1000);
	n += (size_t)sprintf(want, "%sG fixed\nB 0 ", HEADER);
	for (i = 0; i < 1000; i++)
		n += (size_t)sprintf(want + n, "5a");
	sprintf(want + n, "\n");
	expect_text("long record", want);
}

static void expect_overflow(const char *what)
{
	static const LadxAtom one[] = {LIT(1)};

	check(ladx_wlog_bank(&wlog, span("fixed"), 0, one, 1) == LADX_WLOG_OVERFLOW, what);
	check(ladx_wlog_table(&wlog, span("fixed"), 0, index_key(0), one, 1) == LADX_WLOG_OVERFLOW, what);
	check(ladx_wlog_pointer(&wlog, span("fixed"), 0, index_key(0), 1) == LADX_WLOG_OVERFLOW, what);
	check(ladx_wlog_end(&wlog) == LADX_WLOG_OVERFLOW, what);
}

static void test_overflow(void)
{
	uint32_t i;

	for (i = 0; i < LADX_WLOG_ATOM_MAX + 1; i++)
		big[i] = (LadxAtom)(0x10000 + i);

	/* Bank bytes: rewriting held offsets never overflows, one more offset does. */
	begin();
	bank("fixed", 0, big, LADX_WLOG_BANK_MAX);
	bank("fixed", 0, big, LADX_WLOG_BANK_MAX);
	check(ladx_wlog_bank(&wlog, span("fixed"), LADX_WLOG_BANK_MAX, big, 1) == LADX_WLOG_OVERFLOW, "bank overflow");
	expect_overflow("after a bank overflow");

	begin();
	for (i = 0; i < LADX_WLOG_ENTRY_MAX; i++)
		pointer(0, index_key(i), i);
	pointer(0, index_key(0), 7);
	check(ladx_wlog_pointer(&wlog, span("options"), 1, index_key(0), 1) == LADX_WLOG_OVERFLOW, "entry overflow");
	expect_overflow("after an entry overflow");

	/* Atoms: a replaced entry's atoms are reclaimed, live atoms over the pool are not. */
	begin();
	entry(0, index_key(1), big, 100);
	entry(1, index_key(1), big, LADX_WLOG_ATOM_MAX - 200);
	entry(0, index_key(1), big, 150);
	entry(0, index_key(1), big, 200);
	entry(1, index_key(1), big + 1, LADX_WLOG_ATOM_MAX - 200);
	check(ladx_wlog_end(&wlog) == LADX_WLOG_OK, "atoms reclaimed");
	if (strcmp(out, HEADER "G options\nT texts 1 @10000+c8\nT entities 1 @10001+ff38\n") != 0)
		test_fail(TEST, "atoms reclaimed: got\n%s", out);
	begin();
	entry(0, index_key(1), big, LADX_WLOG_ATOM_MAX - 1);
	check(ladx_wlog_table(&wlog, span("options"), 0, index_key(2), big, 2) == LADX_WLOG_OVERFLOW, "atom overflow");
	expect_overflow("after an atom overflow");
}

static const char sample_log[] = HEADER "G a\n"
					"B 10 =2 @400+3 aa\n"
					"B 20 0102\n"
					"T texts 5 @30+2 0a\n"
					"T entities Alt06 p1234\n"
					"G b\n"
					"B 11 ff\n"
					"B 100 @500+4\n"
					"T texts 5 bb\n"
					"G a\n"
					"B 15 @403+2\n"
					"B 30 =1\n"
					"T texts 5 cc\n"
					"T entities 2 p0\n"
					"I 4 5+1\n"
					"I 5 0+2\n";

static LadxWlogStatus project(const char *text, const char *group, int *line)
{
	out_len = 0;
	out[0] = '\0';
	return ladx_wlog_project(&wlog, span(text), span(group), sink, NULL, line);
}

static void expect_projection(const char *text, const char *group, const char *want)
{
	int line = -1;
	LadxWlogStatus st = project(text, group, &line);

	if (st != LADX_WLOG_OK)
		test_fail(TEST, "projection of %s: status %d at line %d", group, (int)st, line);
	if (strcmp(out, want) != 0)
		test_fail(TEST, "projection of %s: got\n%s---\nwant\n%s---", group, out, want);
	checks++;
}

/* Each expected text is write_log.py project's on the same log. */
static void test_project(void)
{
	static const char a[] = HEADER "G a\nB 10 =2 @400+5\nB 20 0102\nB 30 =1\nT texts 5 cc\nT entities 2 p0\n"
				       "T entities Alt06 p1234\n";
	static const char b[] = HEADER "G b\nB 11 ff\nB 100 @500+4\nT texts 5 bb\n";

	expect_projection(sample_log, "a", a);
	expect_projection(sample_log, "b", b);
	expect_projection(sample_log, "c", HEADER);
	expect_projection(a, "a", a);
	expect_projection(HEADER "G a\nT texts 0\n", "a", HEADER "G a\nT texts 0\n");
}

static void test_groups_of_log(void)
{
	LadxSpan names[4];
	int count = -1, line = -1;

	check(ladx_wlog_groups(span(sample_log), names, 4, &count, &line) == LADX_WLOG_OK, "groups of the sample log");
	check(count == 2 && names[0].len == 1 && names[0].at[0] == 'a' && names[1].len == 1 && names[1].at[0] == 'b',
	      "groups in first-write order, each once");
	check(ladx_wlog_groups(span(sample_log), names, 1, &count, &line) == LADX_WLOG_OVERFLOW,
	      "groups over the caller's array");
	check(ladx_wlog_groups(span(HEADER), names, 4, &count, &line) == LADX_WLOG_OK && count == 0,
	      "a log of no segment");
}

static char long_log[2 * LADX_WLOG_BANK_MAX + 256];

/* A record longer than any internal buffer, and save's 121,397 net bytes, project whole. */
static void test_project_long(void)
{
	size_t n = (size_t)sprintf(long_log, "%sG save\nB 4000 ", HEADER), i;

	for (i = 0; i < 1000; i++)
		n += (size_t)sprintf(long_log + n, "%02x", (unsigned)(i * 7 % 256));
	n += (size_t)sprintf(long_log + n, " =%x\nB %x 5a\nT texts 1 ", 121397u - 1001u, 0x4000u + 121397u);
	for (i = 0; i < 3000; i++)
		n += (size_t)sprintf(long_log + n, "%02x", (unsigned)(i % 251));
	sprintf(long_log + n, "\n");
	expect_projection(long_log, "save", long_log);
}

static int malformed_count;

static void test_project_malformed(void)
{
	static const struct {
		const char *name, *text;
		int line;
	} bad[] = {
		{"no final newline", HEADER "G a\nB 10 aa", 0},
		{"version", "ladx-rom-writes 2\ntables texts\n", 1},
		{"no tables line", "ladx-rom-writes 1\nG a\nB 1 aa\n", 2},
		{"repeated table", "ladx-rom-writes 1\ntables texts texts\n", 2},
		{"bad table name", "ladx-rom-writes 1\ntables te/xts\n", 2},
		{"record before G", HEADER "B 10 aa\n", 3},
		{"empty segment", HEADER "G a\nG b\nB 1 aa\n", 3},
		{"empty last segment", HEADER "G a\nB 1 aa\nG b\n", 5},
		{"segment continues", HEADER "G a\nB 1 aa\nG a\nB 3 aa\n", 5},
		{"bad G", HEADER "G a b\nB 1 aa\n", 3},
		{"bank out of order", HEADER "G a\nB 10 aa\nB 5 aa\n", 5},
		{"bank touching", HEADER "G a\nB 10 aa\nB 11 aa\n", 5},
		{"bank after table", HEADER "G a\nT texts 1 aa\nB 10 aa\n", 5},
		{"offset not canonical", HEADER "G a\nB 010 aa\n", 4},
		{"offset uppercase", HEADER "G a\nB 1A aa\n", 4},
		{"empty bank record", HEADER "G a\nB 10\n", 4},
		{"unknown table", HEADER "G a\nT rooms 1 aa\n", 4},
		{"key not canonical", HEADER "G a\nT texts 0a aa\n", 4},
		{"bad alt key", HEADER "G a\nT texts Alt/1 aa\n", 4},
		{"table out of order", HEADER "G a\nT entities 1 aa\nT texts 1 aa\n", 5},
		{"alt before index", HEADER "G a\nT texts Alt06 aa\nT texts 1 aa\n", 5},
		{"table repeated", HEADER "G a\nT texts 1 aa\nT texts 1 bb\n", 5},
		{"keep zero", HEADER "G a\nB 10 =0\n", 4},
		{"keep not canonical", HEADER "G a\nB 10 =04\n", 4},
		{"copy zero", HEADER "G a\nB 10 @400+0\n", 4},
		{"copy not canonical", HEADER "G a\nB 10 @0400+3\n", 4},
		{"copy on own offset", HEADER "G a\nB 10 aa @11+2\n", 4},
		{"literals continue", HEADER "G a\nB 10 aa bb\n", 4},
		{"copies continue", HEADER "G a\nB 10 @400+2 @402+1\n", 4},
		{"keep continued by copy", HEADER "G a\nB 10 =2 @12+1\n", 4},
		{"odd literal", HEADER "G a\nB 10 abc\n", 4},
		{"uppercase literal", HEADER "G a\nB 10 AA\n", 4},
		{"pointer in bank", HEADER "G a\nB 10 p1\n", 4},
		{"pointer with bytes", HEADER "G a\nT texts 1 p1 aa\n", 4},
		{"pointer not canonical", HEADER "G a\nT texts 1 p01\n", 4},
		{"keep in table", HEADER "G a\nT texts 1 =2\n", 4},
		{"empty token", HEADER "G a\nB 10 aa  @400+1\n", 4},
		{"trailing space", HEADER "G a\nB 10 aa \n", 4},
		{"unknown line", HEADER "G a\nB 10 aa\nX 1\n", 5},
		{"empty line", HEADER "G a\nB 10 aa\n\n", 5},
		{"I names a G line", HEADER "G a\nB 10 aabb\nI 3 0+1\n", 5},
		{"I names a later line", HEADER "G a\nB 10 aabb\nI 5 0+1\n", 5},
		{"I names a pointer", HEADER "G a\nT texts 1 p5\nI 4 0+1\n", 5},
		{"I range not literal", HEADER "G a\nB 10 aa @400+2\nI 4 0+2\n", 5},
		{"I range past the record", HEADER "G a\nB 10 aabb\nI 4 1+2\n", 5},
		{"I ranges touching", HEADER "G a\nB 10 aabbcc\nI 4 0+1 1+1\n", 5},
		{"I range empty", HEADER "G a\nB 10 aabbcc\nI 4 0+0\n", 5},
		{"I range not canonical", HEADER "G a\nB 10 aabbcc\nI 4 00+1\n", 5},
		{"I line not canonical", HEADER "G a\nB 10 aabbcc\nI 04 0+1\n", 5},
		{"I without range", HEADER "G a\nB 10 aabbcc\nI 4\n", 5},
		{"I lines out of order", HEADER "G a\nB 10 aabbcc\nB 20 dd\nI 5 0+1\nI 4 0+1\n", 7},
		{"I lines repeated", HEADER "G a\nB 10 aabbcc\nI 4 0+1\nI 4 2+1\n", 6},
		{"record after I", HEADER "G a\nB 10 aabbcc\nI 4 0+1\nB 20 aa\n", 6},
	};
	size_t i;

	for (i = 0; i < N(bad); i++) {
		LadxSpan names[4];
		int line = -1, count;
		LadxWlogStatus st = project(bad[i].text, "a", &line);

		if (st != LADX_WLOG_MALFORMED || line != bad[i].line)
			test_fail(TEST, "malformed log %s: status %d line %d, want malformed at line %d", bad[i].name,
				  (int)st, line, bad[i].line);
		if (out_len != 0)
			test_fail(TEST, "malformed log %s: text written", bad[i].name);
		line = -1;
		if (ladx_wlog_groups(span(bad[i].text), names, 4, &count, &line) != LADX_WLOG_MALFORMED ||
		    line != bad[i].line)
			test_fail(TEST, "malformed log %s: groups not refused at line %d", bad[i].name, bad[i].line);
		checks += 2;
		malformed_count++;
	}
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	test_empty();
	test_bank();
	test_tables();
	test_groups();
	test_bad_input();
	test_long_record();
	test_overflow();
	test_project();
	test_groups_of_log();
	test_project_long();
	test_project_malformed();
	printf("%s: write_log.render's texts of synthetic segments, refused records and overflows; write_log.py's "
	       "projections and groups of hand-written logs, %d malformed logs refused (%d checks)\n",
	       TEST, malformed_count, checks);
	return 0;
}
