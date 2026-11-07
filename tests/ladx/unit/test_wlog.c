#include "ladx_wlog.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_wlog"
#define OUT_MAX 4096

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

/* A refused record changes nothing and opens no segment. */
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

static LadxAtom big[LADX_WLOG_ATOM_MAX + 1];

/* A record longer than any internal buffer goes out whole. */
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

	/* Entries. */
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
	printf("%s: write_log.render's texts of synthetic segments, refused records and overflows (%d checks)\n", TEST,
	       checks);
	return 0;
}
