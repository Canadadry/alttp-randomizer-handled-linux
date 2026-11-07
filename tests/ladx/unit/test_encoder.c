#include "ladx_encoder.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_encoder"
#define OUT_MAX (1 << 20)
#define ROM_LEN 0x1000u

static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static uint8_t rom[ROM_LEN];
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

#define TABLES "texts entities"
#define HEADER "ladx-rom-writes 1\ntables " TABLES "\n"
#define TEXTS 0
#define ENTITIES 1
#define K LADX_ENC_KNOWN
#define V(i) LADX_ENC_IDENTITY(i)
#define LIT(b) LADX_ATOM_LITERAL(b)
#define N(a) (sizeof(a) / sizeof(a)[0])

/*
 * A ROM of 0x1000 bytes from a fixed PRNG, with planted regions: 0xC00 a
 * copy of 0x200, 0x240 of 0xE00 (KS-match ties) and 0xD40 of 0xD00 (a
 * repeated middle run); runs of distinct values at 0x900, 0x980 and 0xA00,
 * and at 0x9C0 one with a repeat (fragments).
 */
static void build_rom(void)
{
	uint32_t x = 0x1ade5eedu, i;

	for (i = 0; i < ROM_LEN; i++) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		rom[i] = (uint8_t)(x >> 24);
	}
	memcpy(rom + 0xc00, rom + 0x200, 0x10);
	memcpy(rom + 0x240, rom + 0xe00, 6);
	for (i = 0; i < 0x10; i++) {
		rom[0x900 + i] = (uint8_t)(0x10 + i);
		rom[0x980 + i] = (uint8_t)(0x60 + i);
	}
	for (i = 0; i < 0x20; i++)
		rom[0xa00 + i] = (uint8_t)(0x40 + i);
	memcpy(rom + 0xd40, rom + 0xd00, 8);
	for (i = 0; i < 0x10; i++)
		rom[0x9c0 + i] = (uint8_t)(0x30 + i);
	rom[0x9c3] = 0x30;
	rom[0x9c4] = 0x31;
	rom[0x9c5] = 0x33;
}

static LadxWlogKey key(uint32_t index)
{
	LadxWlogKey k;

	k.index = index;
	k.alt.at = "";
	k.alt.len = 0;
	return k;
}

/* ks_spans: start, stop pairs of plain spans, then the entry spans of entities' keys. */
static const LadxKs *make_ks(const uint32_t *spans, size_t n, const uint32_t *entries, size_t m)
{
	LadxKsLabel l;
	size_t i;

	if (n == 0 && m == 0)
		return NULL;
	ladx_ks_init(&ks, rom, ROM_LEN);
	memset(&l, 0, sizeof l);
	l.kind = LADX_KS_POINTER;
	l.table = ENTITIES;
	for (i = 0; i < n; i += 2)
		check(ladx_ks_add(&ks, spans[i], spans[i + 1], &l) == LADX_KS_OK, "a KS span is added");
	l.kind = LADX_KS_ENTRY;
	for (i = 0; i < m; i += 3) {
		l.index = entries[i];
		check(ladx_ks_add(&ks, entries[i + 1], entries[i + 2], &l) == LADX_KS_OK, "a KS entry is added");
	}
	check(ladx_ks_finish(&ks) == LADX_KS_OK, "the KS is finished");
	return &ks;
}

static void begin_with(const LadxKs *k)
{
	out_len = 0;
	out[0] = '\0';
	check(ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) == LADX_WLOG_OK, "the log begins");
	check(ladx_enc_init(&enc, rom, ROM_LEN, k, NULL, &wlog) == LADX_ENC_OK, "the encoder starts");
}

static void begin(void)
{
	begin_with(NULL);
}

static void expect(const char *name, const char *want)
{
	check(ladx_enc_end(&enc) == LADX_ENC_OK, name);
	if (strcmp(out, want) != 0)
		test_fail(TEST, "%s: got\n%s---\nwant\n%s---", name, out, want);
}

static void bank(const char *group, uint32_t offset, const uint8_t *values, const int32_t *origins, size_t n)
{
	LadxEncStatus st = ladx_enc_bank(&enc, span(group), offset, values, origins, n);

	if (st != LADX_ENC_OK)
		test_fail(TEST, "a bank write at %x: status %d", (unsigned)offset, (int)st);
	checks++;
}

static void entry(const char *group, uint32_t k, const uint8_t *values, const int32_t *origins, size_t n)
{
	check(ladx_enc_table(&enc, span(group), ENTITIES, key(k), values, origins, n) == LADX_ENC_OK,
	      "an entry write");
}

/* The source copy of rom[q..q+n), origins included. */
static void src(uint32_t q, size_t n, uint8_t *values, int32_t *origins)
{
	size_t i;

	for (i = 0; i < n; i++) {
		values[i] = rom[q + i];
		origins[i] = (int32_t)(q + i);
	}
}

static void initial_source(uint32_t k, uint32_t q, size_t n)
{
	uint8_t v[64];
	int32_t o[64];

	src(q, n, v, o);
	check(ladx_enc_initial(&enc, ENTITIES, key(k), v, o, n) == LADX_ENC_OK, "an initial entry");
}

static void test_carried_source(void)
{
	uint8_t v[4];
	int32_t o[4];

	begin();
	src(0x100, 1, v, o);
	src(0x300, 3, v + 1, o + 1);
	bank("ca", 0x100, v, o, 4);
	expect("a carried source copy", HEADER "G ca\nB 100 =1 @300+3\n");
}

/* hex of rom[q..q+n) */
static const char *hex(uint32_t q, size_t n)
{
	static char buf[8][200];
	static int next;
	char *b = buf[next++ & 7];
	size_t i;

	for (i = 0; i < n; i++)
		sprintf(b + 2 * i, "%02x", rom[q + i]);
	b[2 * n] = '\0';
	return b;
}

static char want[4096];

static void test_carried_identity(void)
{
	static const uint32_t spans[] = {0x100, 0x104};
	int32_t o[2] = {K, V(0)};
	uint8_t v[2];

	begin_with(make_ks(spans, N(spans), NULL, 0));
	bank("ca", 0x100, rom + 0x100, o + 1, 1);
	bank("cb", 0x500, rom + 0x100, o + 1, 1);
	v[0] = 1;
	v[1] = rom[0x100];
	entry("cb", 5, v, o, 2);
	expect("an identity inheriting a keep, carried by a later group",
	       HEADER "G ca\nB 100 =1\nG cb\nB 500 @100+1\nT entities 5 01 @100+1\n");
}

static void test_inherit_bank(void)
{
	static const uint32_t spans[] = {0x100, 0x110};
	static const int32_t k5[] = {K, K, K, K, K};
	int32_t o[1];
	uint8_t v[5];

	begin_with(make_ks(spans, N(spans), NULL, 0));
	memcpy(v, rom + 0x100, 5);
	v[4] ^= 0xff;
	bank("ca", 0x100, v, k5, 5);
	bank("ca", 0x200, rom + 0x200, k5, 2);
	v[0] = 0x55;
	bank("ca", 0x300, v, k5, 1);
	src(0x105, 1, v + 1, o);
	bank("ca", 0x700, v + 1, o, 1);
	bank("cb", 0x300, v, k5, 1);
	bank("cb", 0x700, v + 1, k5, 1);
	sprintf(want, HEADER "G ca\nB 100 =4 %02x\nB 200 %s\nB 300 55\nB 700 @105+1\nG cb\nB 300 55\nB 700 @105+1\n",
		rom[0x104] ^ 0xff, hex(0x200, 2));
	expect("inherit-on-equal: a KS reference or a literal kept, a source byte outside KS a literal", want);
}

static void test_table_prefix_suffix_middle(void)
{
	static const uint32_t spans[] = {0x400, 0x410};
	int32_t o[16];
	uint8_t v[16];

	begin_with(make_ks(spans, N(spans), NULL, 0));
	initial_source(3, 0x400, 16);
	memcpy(v, rom + 0x400, 3);
	v[3] = rom[0x403] ^ 0xff;
	memcpy(v + 4, rom + 0x406, 5);
	v[9] = rom[0x40b] ^ 0xff;
	memcpy(v + 10, rom + 0x40e, 2);
	for (int i = 0; i < 12; i++)
		o[i] = K;
	entry("ca", 3, v, o, 12);
	sprintf(want, HEADER "G ca\nT entities 3 @400+3 %02x @406+5 %02x @40e+2\n", v[3], v[9]);
	expect("an entry rewritten: prefix, middle run and suffix inherited", want);
}

static void test_table_unknowable_stop(void)
{
	static const uint32_t spans[] = {0x500, 0x502, 0x506, 0x508};
	static const int32_t k8[] = {K, K, K, K, K, K, K, K};

	begin_with(make_ks(spans, N(spans), NULL, 0));
	initial_source(4, 0x500, 8);
	entry("ca", 4, rom + 0x500, k8, 8);
	sprintf(want, HEADER "G ca\nT entities 4 @500+2 %s @506+2\n", hex(0x502, 4));
	expect("prefix and suffix stop at a byte C cannot know", want);
}

static void test_net_effect(void)
{
	static const uint8_t v[] = {1, 2, 3}, nine = 9;
	static const int32_t k3[] = {K, K, K};

	begin();
	bank("ca", 0x100, v, k3, 3);
	bank("ca", 0x101, &nine, k3, 1);
	entry("ca", 2, v, k3, 1);
	entry("ca", 2, v + 1, k3, 2);
	expect("a segment's net effect", HEADER "G ca\nB 100 010903\nT entities 2 0203\n");
}

static void test_integer_pointer(void)
{
	static const int32_t k4[] = {K, K, K, K};

	begin();
	check(ladx_enc_initial_pointer(&enc, ENTITIES, key(0xb), 0x1234) == LADX_ENC_OK, "an initial pointer");
	check(ladx_enc_pointer(&enc, span("ca"), ENTITIES, key(0xa), 0x5678) == LADX_ENC_OK, "a pointer write");
	entry("ca", 0xb, rom + 0x40, k4, 4);
	sprintf(want, HEADER "G ca\nT entities a p5678\nT entities b %s\n", hex(0x40, 4));
	expect("an integer pointer written, and an entry replacing one", want);
}

static void test_keep_rewrite(void)
{
	static const int32_t k1[] = {K};
	int32_t o[1];
	uint8_t v[1];

	begin();
	v[0] = rom[0x300] ^ 0x5a;
	bank("ca", 0x300, v, k1, 1);
	v[0] = rom[0x310] ^ 0x5a;
	bank("ca", 0x310, v, k1, 1);
	src(0x310, 1, v, o);
	bank("ca", 0x310, v, o, 1);
	src(0x300, 1, v, o);
	bank("cb", 0x300, v, o, 1);
	sprintf(want, HEADER "G ca\nB 300 %02x\nB 310 =1\nG cb\nB 300 =1\n", rom[0x300] ^ 0x5a);
	expect("a position written back to its source byte is a keep", want);
}

static void test_passthrough(void)
{
	static const LadxAtom atoms[] = {LIT(1), 0x343, 0x344}, twice[] = {LIT(2), LIT(3)};
	static const int32_t named[] = {V(20), V(21), V(22)}, same[] = {V(5), V(5)};
	int32_t o[4] = {V(21), V(22), V(23), V(5)};
	uint8_t v[4] = {0, 0, 7, 3};

	begin();
	check(ladx_enc_fixed_bank(&enc, span("tg"), 0x130, twice, same, 2) == LADX_ENC_OK, "a table group's atoms");
	check(ladx_enc_fixed_bank(&enc, span("tg"), 0x120, atoms, named, 3) == LADX_ENC_OK, "a table group's atoms");
	v[0] = rom[0x343];
	v[1] = rom[0x344];
	bank("ca", 0x140, v, o, 4);
	expect("a table group's atoms taken as given, its identities named by offset and carried",
	       HEADER "G tg\nB 120 01 @343+2\nB 130 0203\nG ca\nB 140 @343+2 0702\n");
}

static void test_queries(void)
{
	static const LadxAtom atoms[] = {LIT(1), 0x343};
	int32_t o[3], got[4];
	uint8_t v[3] = {9, 9, 9};
	size_t n = 0;

	begin();
	check(ladx_enc_bank_origin(&enc, 0x120) == 0x120, "an unwritten byte's origin is its own offset");
	check(ladx_enc_fixed_bank(&enc, span("tg"), 0x120, atoms, NULL, 2) == LADX_ENC_OK, "a table group's atoms");
	check(ladx_enc_fixed_table(&enc, span("tg"), ENTITIES, key(1), atoms, NULL, 2) == LADX_ENC_OK,
	      "a table group's entry");
	initial_source(7, 0x10, 2);
	check(ladx_enc_entry_origins(&enc, ENTITIES, key(7), got, 4, &n) && n == 2 && got[0] == 0x10 &&
		      got[1] == 0x11,
	      "an initial entry's origins are its source offsets");
	check(!ladx_enc_entry_origins(&enc, ENTITIES, key(8), got, 4, &n), "an unknown entry has no origins");
	o[0] = ladx_enc_bank_origin(&enc, 0x121);
	o[1] = ladx_enc_bank_origin(&enc, 0x120);
	src(0x30, 1, v + 2, o + 2);
	bank("ca", 0x140, v, o, 3);
	check(o[0] < K && o[1] < K && o[0] != o[1], "a table group's bytes get fresh identities");
	check(ladx_enc_bank_origin(&enc, 0x142) == 0x30, "a written byte's origin is the one it carried");
	check(ladx_enc_entry_origins(&enc, ENTITIES, key(1), got, 4, &n) && n == 2 && got[0] < K && got[1] < K &&
		      got[0] != o[0] && got[0] != o[1],
	      "a table group's entry bytes get fresh identities");
	check(ladx_enc_fresh(&enc, 2) < got[1] && ladx_enc_fresh(&enc, 1) < got[1], "fresh identities are new ones");
	expect("carried copies of a table group's fresh identities",
	       HEADER "G tg\nB 120 01 @343+1\nT entities 1 01 @343+1\nG ca\nB 140 @343+1 01 @30+1\n");
}

static void k8(int32_t *o)
{
	int i;

	for (i = 0; i < 8; i++)
		o[i] = K;
}

static void test_ks_match_lowest_offset(void)
{
	static const uint32_t spans[] = {0xc00, 0xc10, 0x200, 0x210};
	int32_t o[8];

	k8(o);
	begin_with(make_ks(spans, N(spans), NULL, 0));
	bank("ca", 0x800, rom + 0x200, o, 8);
	expect("KS-match: of two equal matches the lowest offset", HEADER "G ca\nB 800 @200+8\n");
}

static void test_ks_match_longest(void)
{
	static const uint32_t spans[] = {0x240, 0x248, 0xe00, 0xe0c};
	int32_t o[12];
	int i;

	for (i = 0; i < 12; i++)
		o[i] = K;
	begin_with(make_ks(spans, N(spans), NULL, 0));
	bank("ca", 0x800, rom + 0xe00, o, 12);
	expect("KS-match: a longer match at a higher offset wins", HEADER "G ca\nB 800 @e00+c\n");
}

static void test_ks_match_run_boundary(void)
{
	static const uint32_t spans[] = {0x600, 0x608, 0x609, 0x612};
	int32_t o[8];

	k8(o);
	begin_with(make_ks(spans, N(spans), NULL, 0));
	bank("ca", 0x700, rom + 0x604, o, 8);
	bank("ca", 0x720, rom + 0x60a, o, 8);
	sprintf(want, HEADER "G ca\nB 700 @604+4 %s\nB 720 @60a+8\n", hex(0x608, 4));
	expect("KS-match: a match never crosses a KS run's end", want);
}

static void test_fragment_table(void)
{
	static const uint32_t entries[] = {7, 0x900, 0x910};
	static const uint8_t v[] = {0x15, 0x16, 0x00, 0x1a};
	int32_t o[8];

	k8(o);
	begin_with(make_ks(NULL, 0, entries, N(entries)));
	initial_source(7, 0x900, 16);
	entry("ca", 7, v, o, 4);
	expect("the fragment rule on an entry: pieces of its KS entry, lowest offset",
	       HEADER "G ca\nT entities 7 @905+2 00 @90a+1\n");
}

static void test_fragment_bank(void)
{
	static const uint32_t spans[] = {0xa08, 0xa20}, entries[] = {2, 0xa00, 0xa10};
	static const uint8_t v[] = {0x42, 0x43, 0x00, 0x45, 0x52, 0x00, 0x58, 0x59};
	int32_t o[8];

	k8(o);
	begin_with(make_ks(spans, N(spans), entries, N(entries)));
	bank("ca", 0xa0c, v, o, 8);
	expect("the fragment rule on bank bytes: pieces cut where span_at's lowest-start span changes",
	       HEADER "G ca\nB a0c @a02+2 00 @a05+1 @a12+1 00 @a18+2\n");
}

static void test_table_middle_lowest_index(void)
{
	static const uint32_t spans[] = {0xd00, 0xd08, 0xd40, 0xd48};
	int32_t o[8];
	uint8_t v[6];

	k8(o);
	begin_with(make_ks(spans, N(spans), NULL, 0));
	{
		uint8_t old[8];
		int32_t q[8];

		src(0xd40, 4, old, q);
		src(0xd00, 4, old + 4, q + 4);
		check(ladx_enc_initial(&enc, ENTITIES, key(3), old, q, 8) == LADX_ENC_OK, "an initial entry");
	}
	v[0] = rom[0xd40] ^ 0xff;
	memcpy(v + 1, rom + 0xd00, 4);
	v[5] = rom[0xd03] ^ 0xff;
	entry("ca", 3, v, o, 6);
	sprintf(want, HEADER "G ca\nT entities 3 %02x @d40+4 %02x\n", v[0], v[5]);
	expect("a middle run equal to two runs of the old value inherits the lowest one", want);
}

static void test_fragment_repeat(void)
{
	static const uint32_t entries[] = {6, 0x9c0, 0x9d0};
	static const uint8_t v[] = {0x31, 0x33, 0x00, 0x30, 0x31};
	int32_t o[8];

	k8(o);
	begin_with(make_ks(NULL, 0, entries, N(entries)));
	initial_source(6, 0x9c0, 16);
	entry("ca", 6, v, o, 5);
	expect("the fragment rule: the longest prefix found, at its lowest offset",
	       HEADER "G ca\nT entities 6 @9c4+2 00 @9c0+2\n");
}

static void test_rename_after_ks_match(void)
{
	static const uint32_t spans[] = {0xb00, 0xb10};
	int32_t o[8];
	uint8_t v[3];
	int i;

	for (i = 0; i < 8; i++)
		o[i] = V(10 + i);
	begin_with(make_ks(spans, N(spans), NULL, 0));
	bank("ca", 0x780, rom + 0xb00, o, 8);
	bank("cb", 0x7a0, rom + 0xb02, o + 2, 4);
	memcpy(v, rom + 0xb00, 2);
	v[2] = 0;
	o[2] = K;
	entry("cb", 9, v, o, 3);
	expect("identities renamed by KS-match carry the copy into a later segment",
	       HEADER "G ca\nB 780 @b00+8\nG cb\nB 7a0 @b02+4\nT entities 9 @b00+2 00\n");
}

static void test_rename_after_fragment(void)
{
	static const uint32_t entries[] = {8, 0x980, 0x990};
	static const uint8_t v[] = {0x66, 0x67, 0x00};
	static const int32_t o[] = {V(30), V(31), V(32)};

	begin_with(make_ks(NULL, 0, entries, N(entries)));
	initial_source(8, 0x980, 16);
	entry("ca", 8, v, o, 3);
	bank("cb", 0x7c0, v, o, 2);
	expect("identities renamed by the fragment rule carry the copy into a later segment",
	       HEADER "G ca\nT entities 8 @986+2 00\nG cb\nB 7c0 @986+2\n");
}

static void test_refusals(void)
{
	static const LadxAtom atoms[] = {LIT(1)};
	static const int32_t k1[] = {K};
	static const uint8_t v[] = {1};

	begin();
	check(ladx_enc_bank(&enc, span("ca"), ROM_LEN, v, k1, 1) == LADX_ENC_BAD_INPUT, "a write past the ROM");
	check(ladx_enc_bank(&enc, span("c a"), 0, v, k1, 1) == LADX_ENC_BAD_INPUT, "a bad group name");
	check(ladx_enc_table(&enc, span("ca"), 2, key(0), v, k1, 1) == LADX_ENC_BAD_INPUT, "a table out of range");
	check(ladx_enc_fixed_bank(&enc, span("tg"), 0, atoms, NULL, 1) == LADX_ENC_OK, "a table group's write");
	check(ladx_enc_bank(&enc, span("tg"), 0, v, k1, 1) == LADX_ENC_BAD_INPUT, "one segment of both sides");
	expect("refused writes change nothing", HEADER "G tg\nB 0 01\n");
}

/* The fixture ------------------------------------------------------------------------------------ */

#define VECTORS "oracle/encoder_vectors.txt"
#define FIX_MAX (1 << 20)
#define FIELD_MAX 16
#define VEC_LINES 512
#define SEG_EVENTS 256

static char fix[FIX_MAX];
static uint8_t fix_rom[ROM_LEN];
static size_t fix_rom_len;
static char fix_tables[512];
static uint8_t values[LADX_ENC_VALUE_MAX];
static int32_t origins[LADX_ENC_VALUE_MAX];
static LadxAtom atoms[LADX_ENC_VALUE_MAX];

typedef struct Cursor {
	char *at;
	long line;
} Cursor;

/* The next line, NUL-terminated in place; NULL at the end. */
static char *next_line(Cursor *c)
{
	char *line = c->at, *nl;

	if (!*line)
		return NULL;
	nl = strchr(line, '\n');
	if (!nl)
		test_fail(TEST, "%s line %ld: unterminated", VECTORS, c->line + 1);
	*nl = '\0';
	c->at = nl + 1;
	c->line++;
	return line;
}

static int split(char *line, char sep, char **fields, int cap)
{
	int n = 0;

	for (;;) {
		char *end = strchr(line, sep);

		if (n == cap)
			test_fail(TEST, "%s: a line of over %d fields", VECTORS, cap);
		fields[n++] = line;
		if (!end)
			return n;
		*end = '\0';
		line = end + 1;
	}
}

static uint32_t hex_num(const char *s, const char *what)
{
	char *end;
	unsigned long v;

	if (!*s)
		test_fail(TEST, "%s: empty %s", VECTORS, what);
	v = strtoul(s, &end, 16);
	if (*end)
		test_fail(TEST, "%s: bad %s %s", VECTORS, what, s);
	return (uint32_t)v;
}

static int is_hex(const char *s)
{
	return *s && strspn(s, "0123456789abcdef") == strlen(s);
}

static size_t hex_bytes(const char *s, uint8_t *out, size_t cap)
{
	size_t n = strlen(s) / 2, i;

	if (strlen(s) % 2 || n > cap || (*s && !is_hex(s)))
		test_fail(TEST, "%s: bad hex bytes", VECTORS);
	for (i = 0; i < n; i++) {
		unsigned v;

		sscanf(s + 2 * i, "%2x", &v);
		out[i] = (uint8_t)v;
	}
	return n;
}

static int table_index(const char *name)
{
	char names[sizeof fix_tables], *fields[LADX_WLOG_TABLES_MAX + 1];
	int n, i;

	strcpy(names, fix_tables);
	n = split(names, ' ', fields, LADX_WLOG_TABLES_MAX + 1);
	for (i = 0; i < n; i++)
		if (strcmp(fields[i], name) == 0)
			return i;
	test_fail(TEST, "%s: unknown table %s", VECTORS, name);
	return -1;
}

static LadxWlogKey key_of(const char *text)
{
	LadxWlogKey k;

	k.index = 0;
	k.alt.at = "";
	k.alt.len = 0;
	if (is_hex(text))
		k.index = hex_num(text, "key");
	else {
		k.alt.at = text;
		k.alt.len = strlen(text);
	}
	return k;
}

/* write_log._origins: `-`, or comma-separated `k<n>`, `v<i>+<n>`, `<q>+<n>`. */
static size_t parse_origins(char *text, int32_t *out)
{
	char *runs[4096];
	size_t n = 0;
	int count, r;

	if (strcmp(text, "-") == 0)
		return 0;
	count = split(text, ',', runs, 4096);
	for (r = 0; r < count; r++) {
		char *run = runs[r], *plus = strchr(run, '+');
		uint32_t len, first, i;

		if (run[0] == 'k') {
			len = hex_num(run + 1, "origin run");
			first = 0;
		} else {
			if (!plus)
				test_fail(TEST, "%s: bad origin run %s", VECTORS, run);
			*plus = '\0';
			len = hex_num(plus + 1, "origin run");
			first = hex_num(run[0] == 'v' ? run + 1 : run, "origin");
		}
		if (n + len > LADX_ENC_VALUE_MAX)
			test_fail(TEST, "%s: origins too long", VECTORS);
		for (i = 0; i < len; i++)
			out[n++] = run[0] == 'k' ? K : run[0] == 'v' ? V(first + i) : (int32_t)(first + i);
	}
	return n;
}

/* A payload: 1 for an integer pointer in *pointer, else values and origins of *n bytes. */
static int payload(const char *value, char *origin_text, uint32_t *pointer, size_t *n)
{
	if (value[0] == 'p') {
		*pointer = hex_num(value + 1, "pointer");
		return 1;
	}
	*n = strcmp(value, "-") == 0 ? 0 : hex_bytes(value, values, LADX_ENC_VALUE_MAX);
	if (parse_origins(origin_text, origins) != *n)
		test_fail(TEST, "%s: origins out of step with the value", VECTORS);
	return 0;
}

/* A record's tokens decoded into atoms; 1 for an integer pointer in *pointer. */
static int decode_tokens(char **tokens, int count, uint32_t offset, int bank_record, uint32_t *pointer, size_t *n)
{
	int t;

	*n = 0;
	for (t = 0; t < count; t++) {
		const char *tok = tokens[t];
		uint32_t len, q, i;

		if (tok[0] == 'p') {
			*pointer = hex_num(tok + 1, "pointer");
			return 1;
		}
		if (tok[0] == '=' || tok[0] == '@') {
			const char *plus = strchr(tok, '+');
			char head[16];

			if (tok[0] == '=') {
				len = hex_num(tok + 1, "keep");
				q = offset + (uint32_t)*n;
				if (!bank_record)
					test_fail(TEST, "%s: a keep in a T record", VECTORS);
			} else {
				if (!plus || (size_t)(plus - tok) > sizeof head)
					test_fail(TEST, "%s: bad copy %s", VECTORS, tok);
				memcpy(head, tok + 1, (size_t)(plus - tok - 1));
				head[plus - tok - 1] = '\0';
				q = hex_num(head, "copy");
				len = hex_num(plus + 1, "copy length");
			}
			for (i = 0; i < len; i++)
				atoms[(*n)++] = (LadxAtom)(q + i);
		} else {
			uint8_t b[LADX_ENC_VALUE_MAX];
			size_t m = hex_bytes(tok, b, LADX_ENC_VALUE_MAX);

			for (i = 0; i < m; i++)
				atoms[(*n)++] = LIT(b[i]);
		}
	}
	return 0;
}

typedef struct Vector {
	char *name;
	char *sides[16];
	int side_count;
	int ks_none;
	char *events[VEC_LINES];
	int event_count;
	char *log[VEC_LINES];
	int log_count;
} Vector;

static const char *side_of(const Vector *v, const char *group)
{
	int i;
	size_t len = strlen(group);

	for (i = 0; i < v->side_count; i++)
		if (strncmp(v->sides[i], group, len) == 0 && v->sides[i][len] == '=')
			return v->sides[i] + len + 1;
	test_fail(TEST, "%s: %s: group %s has no side", VECTORS, v->name, group);
	return NULL;
}

static void run_c_event(const Vector *v, char **f, int n)
{
	LadxSpan group = span(f[1]);
	uint32_t pointer;
	size_t len;
	LadxEncStatus st;

	if (f[0][0] == 'B') {
		payload(f[3], f[4], &pointer, &len);
		st = ladx_enc_bank(&enc, group, hex_num(f[2], "offset"), values, origins, len);
	} else if (payload(f[4], f[5], &pointer, &len)) {
		st = ladx_enc_pointer(&enc, group, table_index(f[2]), key_of(f[3]), pointer);
	} else {
		st = ladx_enc_table(&enc, group, table_index(f[2]), key_of(f[3]), values, origins, len);
	}
	(void)n;
	if (st != LADX_ENC_OK)
		test_fail(TEST, "%s: %s: event refused (status %d)", VECTORS, v->name, (int)st);
}

/* The expected log's segment s: its first record line, and the line after its last. */
static void log_segment(const Vector *v, int s, int *first, int *stop)
{
	int i, seen = -1;

	for (i = 2; i < v->log_count; i++)
		if (v->log[i][0] == 'G' && ++seen == s)
			break;
	if (seen != s)
		test_fail(TEST, "%s: %s: no segment %d in the expected log", VECTORS, v->name, s);
	*first = i + 1;
	for (*stop = *first; *stop < v->log_count && v->log[*stop][0] != 'G'; (*stop)++)
		;
}

/* A table group's segment: its net origins from the events, its atoms from the expected log. */
static void run_table_segment(const Vector *v, char **events, int count, int s)
{
	static int32_t bank_origin[ROM_LEN];
	static uint8_t bank_set[ROM_LEN];
	static char line[4096];
	static char entry_keys[SEG_EVENTS][2][LADX_WLOG_NAME_MAX + 1];
	int32_t *entry_origins[SEG_EVENTS];
	static int32_t entry_pool[SEG_EVENTS][1024];
	size_t entry_len[SEG_EVENTS];
	int entries = 0, i, first, stop, r;
	LadxSpan group;
	char *f[FIELD_MAX];

	memset(bank_set, 0, sizeof bank_set);
	for (i = 0; i < count; i++) {
		uint32_t pointer, offset, k;
		size_t len;
		int e;

		strcpy(line, events[i]);
		split(line, '\t', f, FIELD_MAX);
		if (f[0][0] == 'B') {
			payload(f[3], f[4], &pointer, &len);
			offset = hex_num(f[2], "offset");
			for (k = 0; k < len; k++) {
				bank_origin[offset + k] = origins[k];
				bank_set[offset + k] = 1;
			}
			continue;
		}
		for (e = 0; e < entries; e++)
			if (strcmp(entry_keys[e][0], f[2]) == 0 && strcmp(entry_keys[e][1], f[3]) == 0)
				break;
		if (e == entries) {
			if (strlen(f[2]) > LADX_WLOG_NAME_MAX || strlen(f[3]) > LADX_WLOG_NAME_MAX)
				test_fail(TEST, "%s: %s: a long table or key", VECTORS, v->name);
			strcpy(entry_keys[e][0], f[2]);
			strcpy(entry_keys[e][1], f[3]);
			entries++;
		}
		entry_len[e] = 0;
		entry_origins[e] = entry_pool[e];
		if (!payload(f[4], f[5], &pointer, &len)) {
			if (len > 1024)
				test_fail(TEST, "%s: an entry over 1024 bytes", VECTORS);
			memcpy(entry_pool[e], origins, len * sizeof origins[0]);
			entry_len[e] = len;
		}
	}
	strcpy(line, events[0]);
	split(line, '\t', f, FIELD_MAX);
	group.at = events[0] + (f[1] - line);
	group.len = strlen(f[1]);
	log_segment(v, s, &first, &stop);
	/* banks from the log in its order; entries in the events' first-write order */
	for (r = first; r < stop; r++) {
		char rec[4096], *tok[1024];
		int n;
		uint32_t pointer, offset, k;
		size_t len;

		strcpy(rec, v->log[r]);
		n = split(rec, ' ', tok, 1024);
		if (tok[0][0] != 'B')
			continue;
		offset = hex_num(tok[1], "offset");
		decode_tokens(tok + 2, n - 2, offset, 1, &pointer, &len);
		for (k = 0; k < len; k++) {
			if (!bank_set[offset + k])
				test_fail(TEST, "%s: %s: B record byte %x not written", VECTORS, v->name, offset + k);
			origins[k] = bank_origin[offset + k];
		}
		if (ladx_enc_fixed_bank(&enc, group, offset, atoms, origins, len) != LADX_ENC_OK)
			test_fail(TEST, "%s: %s: a table group's bank record refused", VECTORS, v->name);
	}
	for (i = 0; i < entries; i++) {
		for (r = first; r < stop; r++) {
			char rec[4096], *tok[1024];
			int n;
			uint32_t pointer;
			size_t len;
			LadxEncStatus st;

			strcpy(rec, v->log[r]);
			n = split(rec, ' ', tok, 1024);
			if (tok[0][0] != 'T' || n < 3 || strcmp(tok[1], entry_keys[i][0]) != 0 ||
			    strcmp(tok[2], entry_keys[i][1]) != 0)
				continue;
			if (decode_tokens(tok + 3, n - 3, 0, 0, &pointer, &len)) {
				st = ladx_enc_fixed_pointer(&enc, group, table_index(tok[1]), key_of(tok[2]), pointer);
			} else {
				if (len != entry_len[i])
					test_fail(TEST, "%s: %s: T record out of step with its events", VECTORS, v->name);
				st = ladx_enc_fixed_table(&enc, group, table_index(tok[1]), key_of(tok[2]), atoms,
							  entry_origins[i], len);
			}
			if (st != LADX_ENC_OK)
				test_fail(TEST, "%s: %s: a table group's entry refused", VECTORS, v->name);
			break;
		}
		if (r == stop)
			test_fail(TEST, "%s: %s: an entry missing from the expected log", VECTORS, v->name);
	}
}

static void replay_vector(const Vector *v, const LadxKs *k)
{
	static char want_log[1 << 16], line[4096];
	char *f[FIELD_MAX], *seg[SEG_EVENTS];
	size_t len = 0;
	int i, n, segs = 0, seg_count = 0;
	char seg_group[64] = "";

	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(fix_tables), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, fix_rom, fix_rom_len, k, NULL, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "%s: %s: the encoder does not start", VECTORS, v->name);
	for (i = 0; i <= v->event_count; i++) {
		char group[64] = "";

		if (i < v->event_count) {
			strcpy(line, v->events[i]);
			n = split(line, '\t', f, FIELD_MAX);
			if (f[0][0] == 'I') {
				uint32_t pointer;
				size_t m;
				LadxEncStatus st;

				if (n != 5)
					test_fail(TEST, "%s: %s: bad initial entry", VECTORS, v->name);
				st = payload(f[3], f[4], &pointer, &m)
					     ? ladx_enc_initial_pointer(&enc, table_index(f[1]), key_of(f[2]), pointer)
					     : ladx_enc_initial(&enc, table_index(f[1]), key_of(f[2]), values, origins, m);
				if (st != LADX_ENC_OK)
					test_fail(TEST, "%s: %s: initial entry refused", VECTORS, v->name);
				continue;
			}
			snprintf(group, sizeof group, "%s", f[1]);
		}
		if (seg_count && strcmp(group, seg_group) != 0) {
			if (strcmp(side_of(v, seg_group), "table") == 0) {
				run_table_segment(v, seg, seg_count, segs);
			} else {
				int e;

				for (e = 0; e < seg_count; e++) {
					strcpy(line, seg[e]);
					n = split(line, '\t', f, FIELD_MAX);
					run_c_event(v, f, n);
				}
			}
			segs++;
			seg_count = 0;
		}
		if (i < v->event_count) {
			if (seg_count == SEG_EVENTS)
				test_fail(TEST, "%s: a segment of over %d events", VECTORS, SEG_EVENTS);
			strcpy(seg_group, group);
			seg[seg_count++] = v->events[i];
		}
	}
	if (ladx_enc_end(&enc) != LADX_ENC_OK)
		test_fail(TEST, "%s: %s: the log does not end", VECTORS, v->name);
	for (i = 0; i < v->log_count; i++) {
		size_t l = strlen(v->log[i]);

		if (len + l + 2 > sizeof want_log)
			test_fail(TEST, "%s: %s: expected log too long", VECTORS, v->name);
		memcpy(want_log + len, v->log[i], l);
		len += l;
		want_log[len++] = '\n';
	}
	want_log[len] = '\0';
	if (strcmp(out, want_log) != 0)
		test_fail(TEST, "%s: vector %s: got\n%s---\nwant\n%s---", VECTORS, v->name, out, want_log);
}

/* known_source's span-list label: `<table> pointer|bank <key>`, `<table> <key>`, `boss zero|item <n>`. */
static void add_ks_span(char *text, const char *name)
{
	char *f[8];
	int n = split(text, ' ', f, 8);
	uint32_t start, length;
	LadxKsLabel l;
	const char *k;

	memset(&l, 0, sizeof l);
	if (n < 4)
		test_fail(TEST, "%s: %s: bad K line", VECTORS, name);
	start = hex_num(f[1], "span start");
	length = hex_num(f[2], "span length");
	if (strcmp(f[3], "boss") == 0) {
		l.kind = strcmp(f[4], "zero") == 0 ? LADX_KS_BOSS_ZERO : LADX_KS_BOSS_ITEM;
		l.table = -1;
		l.index = hex_num(f[5], "boss");
	} else {
		l.table = table_index(f[3]);
		if (n == 6 && (strcmp(f[4], "pointer") == 0 || strcmp(f[4], "bank") == 0)) {
			l.kind = f[4][0] == 'p' ? LADX_KS_POINTER : LADX_KS_BANK;
			k = f[5];
		} else if (n == 5) {
			l.kind = LADX_KS_ENTRY;
			k = f[4];
		} else {
			test_fail(TEST, "%s: %s: bad K label", VECTORS, name);
			return;
		}
		if (is_hex(k))
			l.index = hex_num(k, "key");
		else if (strlen(k) <= LADX_KS_ALT_MAX)
			strcpy(l.alt, k);
		else
			test_fail(TEST, "%s: %s: bad K key", VECTORS, name);
	}
	if (ladx_ks_add(&ks, start, start + length, &l) != LADX_KS_OK)
		test_fail(TEST, "%s: %s: K span refused", VECTORS, name);
}

/* A count line `<word> <hex n>`, then its n lines. */
static int counted(Cursor *c, const char *word, char **lines, int cap, const char *name)
{
	char *line = next_line(c), *f[2];
	uint32_t n, i;

	if (!line || split(line, ' ', f, 2) != 2 || strcmp(f[0], word) != 0)
		test_fail(TEST, "%s line %ld: %s: want `%s <n>`", VECTORS, c->line, name, word);
	n = hex_num(f[1], word);
	if ((int)n > cap)
		test_fail(TEST, "%s: %s: over %d %s lines", VECTORS, name, cap, word);
	for (i = 0; i < n; i++)
		if (!(lines[i] = next_line(c)))
			test_fail(TEST, "%s: %s: truncated", VECTORS, name);
	return (int)n;
}

static void replay_vectors(void)
{
	FILE *file = fopen(VECTORS, "rb");
	Cursor c;
	char *line, *f[4];
	size_t len;
	int hand = 0, random = 0, none = 0;

	if (!file)
		test_fail(TEST, "cannot open %s", VECTORS);
	len = fread(fix, 1, FIX_MAX - 1, file);
	fclose(file);
	if (len == FIX_MAX - 1)
		test_fail(TEST, "%s: over %d bytes", VECTORS, FIX_MAX - 2);
	fix[len] = '\0';
	c.at = fix;
	c.line = 0;
	while ((line = next_line(&c)) && line[0] == '#')
		;
	if (!line || strcmp(line, "ladx-encoder-vectors 1") != 0)
		test_fail(TEST, "%s: no version line", VECTORS);
	line = next_line(&c);
	if (!line || strncmp(line, "tables ", 7) != 0 || strlen(line + 7) >= sizeof fix_tables)
		test_fail(TEST, "%s: no tables line", VECTORS);
	strcpy(fix_tables, line + 7);
	line = next_line(&c);
	if (!line || split(line, ' ', f, 2) != 2 || strcmp(f[0], "rom") != 0)
		test_fail(TEST, "%s: no rom line", VECTORS);
	fix_rom_len = hex_num(f[1], "rom size");
	if (fix_rom_len > ROM_LEN || fix_rom_len % 64)
		test_fail(TEST, "%s: rom size %lx", VECTORS, (unsigned long)fix_rom_len);
	for (len = 0; len < fix_rom_len; len += 64) {
		line = next_line(&c);
		if (!line || strncmp(line, "R ", 2) != 0 || hex_bytes(line + 2, fix_rom + len, 64) != 64)
			test_fail(TEST, "%s line %ld: bad R line", VECTORS, c.line);
	}
	while ((line = next_line(&c))) {
		static Vector v;
		char *kl[64];
		const LadxKs *k = NULL;
		int i, n;

		if (strncmp(line, "V ", 2) != 0)
			test_fail(TEST, "%s line %ld: want a V line", VECTORS, c.line);
		v.name = line + 2;
		line = next_line(&c);
		if (!line || strncmp(line, "sides ", 6) != 0)
			test_fail(TEST, "%s: %s: no sides line", VECTORS, v.name);
		v.side_count = split(line + 6, ' ', v.sides, 16);
		line = next_line(&c);
		v.ks_none = line && strcmp(line, "ks none") == 0;
		if (!v.ks_none) {
			if (!line || strncmp(line, "ks ", 3) != 0)
				test_fail(TEST, "%s: %s: no ks line", VECTORS, v.name);
			n = (int)hex_num(line + 3, "ks count");
			if (n > 64)
				test_fail(TEST, "%s: %s: over 64 K lines", VECTORS, v.name);
			ladx_ks_init(&ks, fix_rom, fix_rom_len);
			for (i = 0; i < n; i++) {
				if (!(kl[i] = next_line(&c)) || strncmp(kl[i], "K ", 2) != 0)
					test_fail(TEST, "%s: %s: want a K line", VECTORS, v.name);
				add_ks_span(kl[i], v.name);
			}
			if (ladx_ks_finish(&ks) != LADX_KS_OK)
				test_fail(TEST, "%s: %s: KS refused", VECTORS, v.name);
			k = &ks;
		}
		v.event_count = counted(&c, "events", v.events, VEC_LINES, v.name);
		v.log_count = counted(&c, "log", v.log, VEC_LINES, v.name);
		replay_vector(&v, k);
		if (strncmp(v.name, "random-", 7) == 0)
			random++;
		else
			hand++;
		none += v.ks_none;
	}
	if (hand == 0 || random == 0)
		test_fail(TEST, "%s: %d hand-written and %d random vectors", VECTORS, hand, random);
	printf("%s: %d vectors of %s replayed byte for byte (%d hand-written, %d random, %d with ks none)\n", TEST,
	       hand + random, VECTORS, hand, random, none);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	build_rom();
	test_carried_source();
	test_carried_identity();
	test_inherit_bank();
	test_table_prefix_suffix_middle();
	test_table_unknowable_stop();
	test_net_effect();
	test_integer_pointer();
	test_keep_rewrite();
	test_passthrough();
	test_queries();
	test_refusals();
	test_ks_match_lowest_offset();
	test_ks_match_longest();
	test_ks_match_run_boundary();
	test_fragment_table();
	test_fragment_bank();
	test_table_middle_lowest_index();
	test_fragment_repeat();
	test_rename_after_ks_match();
	test_rename_after_fragment();
	printf("%s: the C-group encoding rule, KS-match and the fragment rule on hand-written streams (%d checks)\n", TEST, checks);
	if (fixtures)
		replay_vectors();
	return 0;
}
