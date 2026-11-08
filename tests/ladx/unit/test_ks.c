#include "ladx_ks.h"
#include "ladx_rom.h"
#include "support.h"
#include "synth_rom.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_ks"
#define TEXT_MAX (1u << 20)

static SynthRom synth;
static LadxRomTables tables;
static LadxKs ks;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

typedef struct Text {
	char at[TEXT_MAX];
	size_t len;
} Text;

static Text want, got;

static void sink(void *ctx, const char *text, size_t len)
{
	Text *t = ctx;

	if (t->len + len >= TEXT_MAX)
		test_fail(TEST, "span list over %u bytes", TEXT_MAX);
	memcpy(t->at + t->len, text, len);
	t->len += len;
	t->at[t->len] = '\0';
}

static void line(Text *t, const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsprintf(buf, fmt, ap);
	va_end(ap);
	sink(t, buf, (size_t)n);
}

/* known_source.LAYOUT's KS keys, written out from it. */
static int known_key(const SynthEntry *e)
{
	switch (e->table) {
	case 0:
		return 0;
	case 1:
		return !e->alt && e->index < 0x316;
	case 9:
		return !e->alt && (e->index == 0x03 || e->index == 0x04 || e->index == 0x0D);
	case 10:
		return !e->alt && (e->index == 0x03 || e->index == 0x04);
	default:
		return 1;
	}
}

static const char *key_text(const SynthEntry *e)
{
	static char text[16];

	if (e->alt)
		return e->alt;
	sprintf(text, "%x", (unsigned)e->index);
	return text;
}

static uint8_t covered[SYNTH_ROM_SIZE];
static uint32_t want_bytes, want_spans, want_failures;

static void want_span(uint32_t start, uint32_t length, const char *fmt, const char *a, const char *b)
{
	uint32_t q;

	line(&want, "%x %x ", (unsigned)start, (unsigned)length);
	line(&want, fmt, a, b);
	line(&want, "\n");
	for (q = start; q < start + length; q++) {
		want_bytes += !covered[q];
		covered[q] = 1;
	}
	want_spans++;
}

static Text failures;

/* The span list known_source.build gives on the builder's record, in its order. */
static void expected_spans(void)
{
	int t, i;
	uint32_t q;

	want.len = failures.len = 0;
	memset(covered, 0, sizeof covered);
	want_bytes = want_spans = want_failures = 0;
	line(&want, "ladx-ks-spans 1\n");
	for (t = 0; t < SYNTH_TABLES; t++) {
		const char *name = synth_table_names[t];
		int first = synth.table_first[t], count = synth.table_count[t], alts = 0;

		for (i = 0; i < count; i++)
			alts += synth.entry[first + i].alt != NULL;
		for (i = 0; i < count; i++) {
			const SynthEntry *e = &synth.entry[first + (i < alts ? count - alts + i : i - alts)];

			want_span(e->pointer_at, e->pointer_len, "%s pointer %s", name, key_text(e));
			if (e->bank_at >= 0 && e->bank_rebuilds) {
				want_span((uint32_t)e->bank_at, 1, "%s bank %s", name, key_text(e));
			} else if (e->bank_at >= 0) {
				line(&failures, "failure %s bank %s\n", name, key_text(e));
				want_failures++;
			}
		}
		for (i = 0; i < count; i++) {
			const SynthEntry *e = &synth.entry[first + i];

			if (e->integer || !known_key(e))
				continue;
			if (e->entry_rebuilds) {
				want_span(e->start, e->stop - e->start, "%s %s", name, key_text(e));
			} else {
				line(&failures, "failure %s %s\n", name, key_text(e));
				want_failures++;
			}
		}
	}
	for (q = SYNTH_BOSS_ZERO; q < SYNTH_BOSS_ZERO + SYNTH_BOSS_ZERO_COUNT; q++) {
		if (synth.rom[q] == 0x00) {
			char text[16];

			sprintf(text, "%x", (unsigned)q);
			want_span(q, 1, "boss zero %s%s", text, "");
		}
	}
	for (i = 0; i < SYNTH_BOSS_ROOMS; i++) {
		char text[16];

		sprintf(text, "%x", (unsigned)synth_boss_rooms[i]);
		want_span(SYNTH_BOSS_ITEM + synth_boss_rooms[i], 1, "boss item %s%s", text, "");
	}
	sink(&want, failures.at, failures.len);
}

static void compare_text(const char *what)
{
	size_t i = 0, line_no = 1;

	if (want.len == got.len && memcmp(want.at, got.at, want.len) == 0)
		return;
	while (i < want.len && i < got.len && want.at[i] == got.at[i]) {
		if (want.at[i] == '\n')
			line_no++;
		i++;
	}
	test_fail(TEST, "%s: differs from line %lu: got \"%.60s\", expected \"%.60s\"", what,
		  (unsigned long)line_no, got.at + i, want.at + i);
}

static LadxWlogKey key_of(const SynthEntry *e)
{
	LadxWlogKey key;

	key.index = e->index;
	key.alt.at = e->alt;
	key.alt.len = e->alt ? strlen(e->alt) : 0;
	return key;
}

static void test_build(void)
{
	int i, entries = 0, planted_entities = 0, planted_banks = 0;

	synth_rom_build(&synth, 0x5EED0001u);
	check(ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &tables) == LADX_ROM_PARSE_OK,
	      "the synthetic ROM parses");
	check(ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &tables) == LADX_KS_OK, "KS builds");
	expected_spans();
	got.len = 0;
	ladx_ks_render_spans(&ks, sink, &got);
	compare_text("the span list");
	checks++;
	if (ladx_ks_span_count(&ks) != want_spans || ladx_ks_byte_count(&ks) != want_bytes ||
	    ladx_ks_failure_count(&ks) != want_failures)
		test_fail(TEST, "%u spans, %u bytes, %u failures; expected %u, %u, %u",
			  (unsigned)ladx_ks_span_count(&ks), (unsigned)ladx_ks_byte_count(&ks),
			  (unsigned)ladx_ks_failure_count(&ks), (unsigned)want_spans, (unsigned)want_bytes,
			  (unsigned)want_failures);
	for (i = 0; i < synth.entry_count; i++) {
		const SynthEntry *e = &synth.entry[i];
		uint32_t start, stop;
		int found = ladx_ks_entry(&ks, e->table, key_of(e), &start, &stop);
		int kept = !e->integer && known_key(e) && e->entry_rebuilds;

		planted_entities += e->table == 1 && !e->entry_rebuilds && known_key(e);
		planted_banks += !e->bank_rebuilds;
		checks++;
		if (found != kept || (kept && (start != e->start || stop != e->stop)))
			test_fail(TEST, "entries[(%s, %s)]: %s, expected %s", synth_table_names[e->table],
				  key_text(e), found ? "found" : "none", kept ? "its span" : "none");
		entries += kept;
	}
	check(planted_entities > 0 && planted_banks > 0, "planted entity and bank failures were left out");
	for (i = 0; i < synth.entry_count; i++) {
		const SynthEntry *e = &synth.entry[i];
		uint32_t start, stop;

		if (e->integer || !known_key(e) || !e->entry_rebuilds || e->table == 7 || e->table == 8)
			continue;
		checks++;
		if (!ladx_ks_contains(&ks, e->start) || !ladx_ks_span_at(&ks, e->stop - 1, &start, &stop) ||
		    start != e->start || stop != e->stop)
			test_fail(TEST, "span_at in %s %s is not its entry span", synth_table_names[e->table],
				  key_text(e));
	}
	printf("test_ks: the span list of a synthetic ROM (%u spans, %u bytes, %u not rebuilt, %d entries)\n",
	       (unsigned)want_spans, (unsigned)want_bytes, (unsigned)want_failures, entries);
}

static LadxKsLabel label(uint32_t index)
{
	LadxKsLabel l;

	memset(&l, 0, sizeof l);
	l.kind = LADX_KS_BOSS_ZERO;
	l.table = -1;
	l.index = index;
	return l;
}

static uint8_t small[0x60];

static void add(uint32_t start, uint32_t stop)
{
	LadxKsLabel l = label(start);

	check(ladx_ks_add(&ks, start, stop, &l) == LADX_KS_OK, "a span is added");
}

static void expect_span_at(int64_t q, int found, uint32_t start, uint32_t stop)
{
	uint32_t s = 0, e = 0;
	int got_found = ladx_ks_span_at(&ks, q, &s, &e);

	checks++;
	if (got_found != found || (found && (s != start || e != stop)))
		test_fail(TEST, "span_at(%ld): %d %x..%x, expected %d %x..%x", (long)q, got_found, (unsigned)s,
			  (unsigned)e, found, (unsigned)start, (unsigned)stop);
}

static void test_span_at(void)
{
	ladx_ks_init(&ks, small, sizeof small);
	add(0x10, 0x18);
	add(0x08, 0x10);
	add(0x08, 0x20);
	add(0x08, 0x20);
	add(0x1C, 0x28);
	expect_span_at(0x12, 0, 0, 0);
	check(!ladx_ks_contains(&ks, 0x12), "nothing is found before finish");
	check(ladx_ks_finish(&ks) == LADX_KS_OK, "the KS finishes");
	expect_span_at(0x09, 1, 0x08, 0x20);
	expect_span_at(0x12, 1, 0x08, 0x20);
	expect_span_at(0x1F, 1, 0x08, 0x20);
	expect_span_at(0x20, 1, 0x1C, 0x28);
	expect_span_at(0x27, 1, 0x1C, 0x28);
	expect_span_at(0x28, 0, 0, 0);
	expect_span_at(0x07, 0, 0, 0);
	expect_span_at(-1, 0, 0, 0);
	expect_span_at(sizeof small, 0, 0, 0);
	check(ladx_ks_byte_count(&ks) == 0x20, "the map holds the union's 0x20 bytes");
	check(ladx_ks_contains(&ks, 0x08) && ladx_ks_contains(&ks, 0x27) && !ladx_ks_contains(&ks, 0x28) &&
		      !ladx_ks_contains(&ks, -1) && !ladx_ks_contains(&ks, sizeof small),
	      "membership is the union of the spans");
	ladx_ks_init(&ks, small, sizeof small);
	add(0x30, 0x34);
	add(0x10, 0x20);
	add(0x12, 0x14);
	check(ladx_ks_finish(&ks) == LADX_KS_OK, "the KS finishes");
	expect_span_at(0x13, 1, 0x10, 0x20);
	expect_span_at(0x31, 1, 0x30, 0x34);
	expect_span_at(0x25, 0, 0, 0);
}

static void expect_run(uint32_t i, uint32_t start, uint32_t stop)
{
	LadxKsRun run = ladx_ks_run(&ks, i);

	checks++;
	if (run.start != start || run.stop != stop)
		test_fail(TEST, "run %u: %x..%x, expected %x..%x", (unsigned)i, (unsigned)run.start,
			  (unsigned)run.stop, (unsigned)start, (unsigned)stop);
}

static void expect_find(const uint8_t *data, size_t n, int64_t want_at, const char *what)
{
	int64_t at = ladx_ks_find(&ks, data, n);

	checks++;
	if (at != want_at)
		test_fail(TEST, "find %s: %ld, expected %ld", what, (long)at, (long)want_at);
}

static void test_runs_and_find(void)
{
	static const uint8_t abcd[4] = { 0xA1, 0xB2, 0xC3, 0xD4 };
	static const uint8_t abcde[5] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5 };
	static const uint8_t cross[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
	static const uint8_t lone[4] = { 0x77, 0x78, 0x79, 0x7A };
	size_t i;

	for (i = 0; i < sizeof small; i++)
		small[i] = (uint8_t)(0x40 + i);
	/* abcd at 0x02 (outside KS), 0x0A, 0x31 and 0x48; abcde only at 0x48 */
	memcpy(small + 0x02, abcd, 4);
	memcpy(small + 0x0A, abcd, 4);
	memcpy(small + 0x31, abcd, 4);
	memcpy(small + 0x48, abcde, 5);
	/* cross at 0x1E..0x24 crosses the offset 0x21 between two runs */
	memcpy(small + 0x1E, cross, 6);
	memcpy(small + 0x5A, lone, 4);
	ladx_ks_init(&ks, small, sizeof small);
	add(0x08, 0x10);
	add(0x10, 0x14);
	add(0x18, 0x21);
	add(0x22, 0x28);
	add(0x30, 0x38);
	add(0x45, 0x50);
	check(ladx_ks_finish(&ks) == LADX_KS_OK, "the KS finishes");
	check(ladx_ks_run_count(&ks) == 5, "five maximal runs");
	expect_run(0, 0x08, 0x14);
	expect_run(1, 0x18, 0x21);
	expect_run(2, 0x22, 0x28);
	expect_run(3, 0x30, 0x38);
	expect_run(4, 0x45, 0x50);
	expect_find(abcd, 4, 0x0A, "4 bytes at three runs, the lowest offset inside KS");
	expect_find(abcde, 5, 0x48, "5 bytes only at the highest run");
	expect_find(cross, 6, -1, "bytes across two runs");
	expect_find(cross, 3, -1, "under 4 bytes");
	expect_find(cross + 2, 4, -1, "bytes across a gap of one offset");
	expect_find(lone, 4, -1, "bytes outside KS");
	expect_find(small + 0x22, 6, 0x22, "a whole run");
	expect_find(small + 0x22, 7, -1, "a run and one byte past it");
	expect_find(small + 0x45, 11, 0x45, "the last run whole");
}

static void test_entries(void)
{
	LadxKsLabel l;
	LadxWlogKey key;
	uint32_t start = 0, stop = 0;

	ladx_ks_init(&ks, small, sizeof small);
	memset(&l, 0, sizeof l);
	l.kind = LADX_KS_ENTRY;
	l.table = 2;
	strcpy(l.alt, "Alt06");
	check(ladx_ks_add(&ks, 0x10, 0x14, &l) == LADX_KS_OK, "an alt entry span is added");
	check(ladx_ks_add(&ks, 0x20, 0x26, &l) == LADX_KS_OK, "the same key's second span is added");
	l.alt[0] = '\0';
	l.index = 6;
	check(ladx_ks_add(&ks, 0x30, 0x31, &l) == LADX_KS_OK, "index key 6's span is added");
	l.kind = LADX_KS_POINTER;
	l.index = 7;
	check(ladx_ks_add(&ks, 0x40, 0x42, &l) == LADX_KS_OK, "a pointer span is added");
	check(ladx_ks_finish(&ks) == LADX_KS_OK, "the KS finishes");
	key.index = 0;
	key.alt.at = "Alt06";
	key.alt.len = 5;
	check(ladx_ks_entry(&ks, 2, key, &start, &stop) && start == 0x20 && stop == 0x26,
	      "a key added twice gives its later span, as a dict");
	key.alt.len = 4;
	check(!ladx_ks_entry(&ks, 2, key, &start, &stop), "a prefix of an alt key is no key");
	key.alt.len = 0;
	key.index = 6;
	check(ladx_ks_entry(&ks, 2, key, &start, &stop) && start == 0x30 && stop == 0x31, "an index key's span");
	check(!ladx_ks_entry(&ks, 3, key, &start, &stop), "no span of the same key in another table");
	key.index = 7;
	check(!ladx_ks_entry(&ks, 2, key, &start, &stop), "a pointer span is no entry");
}

static void test_statuses(void)
{
	LadxKsLabel l = label(0);
	uint32_t i;

	ladx_ks_init(&ks, small, sizeof small);
	check(ladx_ks_add(&ks, 4, 4, &l) == LADX_KS_BAD_INPUT, "an empty span is refused");
	check(ladx_ks_add(&ks, 0x50, 0x61, &l) == LADX_KS_BAD_INPUT, "a span past the source is refused");
	check(ladx_ks_add(&ks, 0, 1, &l) == LADX_KS_OK, "a span after a refusal is added");
	check(ladx_ks_finish(&ks) == LADX_KS_OK && ladx_ks_span_count(&ks) == 1, "one span kept");
	check(ladx_ks_add(&ks, 2, 3, &l) == LADX_KS_BAD_INPUT, "no add after finish");
	ladx_ks_init(&ks, small, sizeof small);
	for (i = 0; i < LADX_KS_SPAN_MAX; i++)
		ladx_ks_add(&ks, 0, 1, &l);
	check(ladx_ks_add(&ks, 0, 1, &l) == LADX_KS_OVERFLOW, "a span over capacity overflows");
	check(ladx_ks_finish(&ks) == LADX_KS_OVERFLOW, "finish reports the overflow");
	expect_span_at(0, 0, 0, 0);
}

int main(void)
{
	test_build();
	test_span_at();
	test_runs_and_find();
	test_entries();
	test_statuses();
	printf("test_ks: build, span list, entries, span_at, runs, find and statuses (%d checks)\n", checks);
	return 0;
}
