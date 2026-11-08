#include "ladx_store.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_store"
#define VECTORS "oracle/store_vectors.txt"
#define BANK 0x4000u
#define BANKS 0x40u
#define FIX_MAX (1 << 20)
#define TEXT_MAX 65536
#define BYTES_MAX 65536

static uint8_t banks[BANKS * BANK];
static char got[TEXT_MAX];
static size_t got_len;
static LadxStoreDone done;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void append(const char *fmt, unsigned a, unsigned b)
{
	int k = snprintf(got + got_len, TEXT_MAX - got_len, fmt, a, b);

	if (k < 0 || (size_t)k >= TEXT_MAX - got_len)
		test_fail(TEST, "writes text over %d bytes", TEXT_MAX);
	got_len += (size_t)k;
}

/* Records each write as the fixture's `W <bank> <addr> <hex>` line and applies it. */
static int write_rec(void *ctx, uint8_t bank, uint32_t addr, const uint8_t *bytes, size_t n, int entry)
{
	size_t i;

	(void)ctx;
	(void)entry;
	append("W %x %x ", bank, (unsigned)addr);
	for (i = 0; i < n; i++) {
		if (bank >= BANKS || addr + i >= BANK)
			return 0;
		banks[bank * BANK + addr + i] = bytes[i];
		append("%02x", bytes[i], 0);
	}
	append("\n", 0, 0);
	return 1;
}

static int read_bank(void *ctx, uint8_t bank, uint32_t addr, uint8_t *out)
{
	(void)ctx;
	if (bank >= BANKS || addr >= BANK)
		return 0;
	*out = banks[bank * BANK + addr];
	return 1;
}

static LadxStoreInfo var_info(uint32_t count, int data_bank)
{
	LadxStoreInfo info;

	memset(&info, 0, sizeof info);
	info.count = count;
	info.pointers_bank = 1;
	info.pointers_addr = 0x100;
	info.data_bank = data_bank;
	return info;
}

static LadxStoreEntry bytes_entry(uint8_t bank, const char *s)
{
	LadxStoreEntry e;

	memset(&e, 0, sizeof e);
	e.bank = bank;
	e.bytes = (const uint8_t *)s;
	e.len = (uint32_t)strlen(s);
	return e;
}

static LadxStoreStatus store(const LadxStoreInfo *info, const LadxStoreRegions *r, const LadxStoreEntry *e,
			     LadxStoreRegions *left, LadxStoreError *err)
{
	memset(banks, 0, sizeof banks);
	got_len = 0;
	got[0] = '\0';
	return ladx_store_table(info, r, e, write_rec, read_bank, NULL, &done, left, err);
}

static void test_regions_merge_one_byte_gap(void)
{
	LadxStoreInfo info = var_info(3, 2);
	LadxStoreSpan spans[] = {{2, 0x100, 0x103}, {2, 0x104, 0x108}, {2, 0x10B, 0x10D}};
	LadxStoreRegions r;

	check(ladx_store_regions(&info, spans, 3, &r) == LADX_STORE_OK, "regions: ok");
	check(r.count == 2 && r.region[0].start == 0x100 && r.region[0].end == 0x108 &&
		      r.region[1].start == 0x10B && r.region[1].end == 0x10D,
	      "regions: a one-byte gap merges, a two-byte gap does not");
	info.expand = 1;
	check(ladx_store_regions(&info, spans, 3, &r) == LADX_STORE_OK && r.region[1].end == 0x4000 &&
		      r.region[0].end == 0x108,
	      "regions: expand grows only the bank's last-ending region");
	info.expand = 0;
	info.claim_gaps = 1;
	check(ladx_store_regions(&info, spans, 3, &r) == LADX_STORE_OK && r.count == 1 && r.region[0].start == 0x100 &&
		      r.region[0].end == 0x10D,
	      "regions: claim gaps spans first to last");
}

static void test_suffix_newest(void)
{
	LadxStoreInfo info = var_info(4, 2);
	LadxStoreSpan spans[] = {{2, 0x100, 0x103}, {2, 0x110, 0x114}, {2, 0x120, 0x122}, {2, 0x130, 0x133}};
	LadxStoreEntry e[4];
	LadxStoreRegions r, left;
	LadxStoreError err;

	e[0] = bytes_entry(2, "\x01\x31\xff");
	e[1] = bytes_entry(2, "\x01\x31\xff");
	e[2] = bytes_entry(2, "\x02\x31\xff");
	e[3] = bytes_entry(2, "\x31\xff");
	ladx_store_regions(&info, spans, 4, &r);
	check(store(&info, &r, e, &left, &err) == LADX_STORE_OK, "suffix: ok");
	check(strcmp(got, "W 2 100 0131ff\nW 1 100 00\nW 1 101 41\nW 1 102 00\nW 1 103 41\n"
			  "W 2 110 0231ff\nW 1 104 10\nW 1 105 41\nW 1 106 11\nW 1 107 41\n") == 0,
	      "suffix: an equal entry is shared, the newest tail wins");
	check(left.count == 4 && left.region[0].start == 0x103 && left.region[1].start == 0x113,
	      "suffix: the storage left");
}

static void test_sprite_errors(void)
{
	LadxStoreInfo info = var_info(1, 0x20);
	LadxStoreRegions r, left;
	LadxStoreEntry e = bytes_entry(0x20, "\x01\x02\x03\x04");
	LadxStoreError err;
	const char *type;
	char msg[128];

	info.has_data_addr = 1;
	info.data_addr = 0x2001;
	info.data_size = 4;
	r.count = 1;
	r.region[0].bank = 0x20;
	r.region[0].start = 0x2000;
	r.region[0].end = 0x2004;
	check(store(&info, &r, &e, &left, &err) == LADX_STORE_MISALIGNED, "sprite: misaligned");
	ladx_store_message(&info, &err, &type, msg, sizeof msg);
	check(strcmp(type, "AssertionError") == 0 && msg[0] == '\0', "sprite: misaligned is a bare assert");
	info.data_addr = 0x2004;
	check(store(&info, &r, &e, &left, &err) == LADX_STORE_BYTE_RANGE, "sprite: a negative index");
	ladx_store_message(&info, &err, &type, msg, sizeof msg);
	check(strcmp(type, "ValueError") == 0 && strcmp(msg, "byte must be in range(0, 256)") == 0,
	      "sprite: ValueError's message");
	info.data_addr = 0x2000;
	check(store(&info, &r, &e, &left, &err) == LADX_STORE_OK && strcmp(got, "W 20 2000 01020304\nW 1 100 00\n") == 0,
	      "sprite: the index byte");
}

static void test_no_room_message(void)
{
	LadxStoreInfo info = var_info(2, 2);
	LadxStoreRegions r, left;
	LadxStoreEntry e[2];
	LadxStoreError err;
	const char *type;
	char msg[256], small[16];
	size_t full;

	e[0] = bytes_entry(2, "\x01\xff");
	e[1] = bytes_entry(2, "\x02\x03\xff");
	r.count = 1;
	r.region[0].bank = 2;
	r.region[0].start = 0x100;
	r.region[0].end = 0x104;
	check(store(&info, &r, e, &left, &err) == LADX_STORE_NO_ROOM, "no room: status");
	full = ladx_store_message(&info, &err, &type, msg, sizeof msg);
	check(strcmp(msg, "Not enough room in storage... 1/2 [{'bank': 2, 'start': 258, 'end': 260}] id:1(1) bank:2") ==
		      0,
	      "no room: LADXR's message");
	check(ladx_store_message(&info, &err, &type, small, sizeof small) == full && strlen(small) == sizeof small - 1,
	      "no room: a cut message keeps its full length");
}

/* --fixtures: store_vectors.txt replayed. */

static char fix[FIX_MAX];
static uint8_t entry_bytes[BYTES_MAX];

static unsigned long hex(const char *s, const char *what, const char *name)
{
	char *end;
	unsigned long v = strtoul(s, &end, 16);

	if (end == s || *end)
		test_fail(TEST, "%s: %s: bad %s `%s`", VECTORS, name, what, s);
	return v;
}

static int split(char *line, char **f, int cap)
{
	int n = 0;

	while (n < cap) {
		f[n++] = line;
		line = strchr(line, ' ');
		if (!line)
			break;
		*line++ = '\0';
	}
	return n;
}

static int region_line(char *line, LadxStoreRegion *r, const char *name)
{
	char *f[4];

	if (split(line, f, 4) != 3)
		return 0;
	r->bank = (uint8_t)hex(f[0], "bank", name);
	r->start = (int32_t)hex(f[1], "start", name);
	r->end = (int32_t)hex(f[2], "end", name);
	return 1;
}

static void regions_text(const char *tag, const LadxStoreRegions *r, char *out, size_t cap)
{
	size_t len = 0;
	int i;

	out[0] = '\0';
	for (i = 0; i < r->count && len < cap; i++)
		len += (size_t)snprintf(out + len, cap - len, "%s %x %x %x\n", tag, r->region[i].bank,
					(unsigned)r->region[i].start, (unsigned)r->region[i].end);
}

typedef struct Vector {
	const char *name;
	LadxStoreInfo info;
	LadxStoreSpan spans[LADX_STORE_ENTRY_MAX];
	size_t span_count;
	char want_regions[TEXT_MAX];
	LadxStoreRegions extra;
	int has_extra, has_adjust;
	int32_t adjust;
	LadxStoreEntry entries[LADX_STORE_ENTRY_MAX];
	uint32_t entry_count;
	char want_writes[TEXT_MAX];
	char want_left[TEXT_MAX];
	char want_raise[TEXT_MAX];
} Vector;

static void cat(char *dst, const char *line)
{
	size_t len = strlen(dst), n = strlen(line);

	if (len + n + 2 > TEXT_MAX)
		test_fail(TEST, "%s: a vector's text over %d bytes", VECTORS, TEXT_MAX);
	memcpy(dst + len, line, n);
	dst[len + n] = '\n';
	dst[len + n + 1] = '\0';
}

static void replay_vector(Vector *v)
{
	static char text[TEXT_MAX];
	LadxStoreRegions r, left;
	LadxStoreError err;
	LadxStoreStatus st;
	const char *type;

	if (ladx_store_regions(&v->info, v->spans, v->span_count, &r) != LADX_STORE_OK)
		test_fail(TEST, "%s: regions failed", v->name);
	regions_text("R", &r, text, sizeof text);
	if (strcmp(text, v->want_regions) != 0)
		test_fail(TEST, "%s: regions\n%s\nwant\n%s", v->name, text, v->want_regions);
	if (v->has_extra && ladx_store_add_storage(&r, &v->extra) != LADX_STORE_OK)
		test_fail(TEST, "%s: addStorage failed", v->name);
	if (v->has_adjust)
		v->info.data_addr = v->adjust;
	if (v->entry_count != (uint32_t)v->info.alt_count + v->info.count)
		test_fail(TEST, "%s: %lu entries", v->name, (unsigned long)v->entry_count);
	st = store(&v->info, &r, v->entries, &left, &err);
	if (strcmp(got, v->want_writes) != 0)
		test_fail(TEST, "%s: writes\n%s\nwant\n%s", v->name, got, v->want_writes);
	if (st == LADX_STORE_OK) {
		regions_text("L", &left, text, sizeof text);
		if (v->want_raise[0] || strcmp(text, v->want_left) != 0)
			test_fail(TEST, "%s: left\n%s\nwant\n%s%s", v->name, text, v->want_left, v->want_raise);
	} else {
		static char msg[TEXT_MAX];

		if (st == LADX_STORE_FAILED)
			test_fail(TEST, "%s: FAILED", v->name);
		ladx_store_message(&v->info, &err, &type, msg, sizeof msg);
		snprintf(text, sizeof text, msg[0] ? "raise %s %s\n" : "raise %s%s\n", type, msg);
		if (strcmp(text, v->want_raise) != 0)
			test_fail(TEST, "%s: %s want %s", v->name, text, v->want_raise);
	}
	checks++;
}

static void replay_vectors(void)
{
	static Vector v;
	FILE *file = fopen(VECTORS, "rb");
	size_t len, used = 0;
	char *at, *line, *f[10];
	int count = 0, raised = 0, in = 0;

	if (!file)
		test_fail(TEST, "cannot open %s", VECTORS);
	len = fread(fix, 1, FIX_MAX - 1, file);
	fclose(file);
	if (len == FIX_MAX - 1)
		test_fail(TEST, "%s: over %d bytes", VECTORS, FIX_MAX - 2);
	fix[len] = '\0';
	for (at = fix; at && *at; at = line) {
		char *row = at;

		line = strchr(at, '\n');
		if (line)
			*line++ = '\0';
		if (row[0] == '#' || row[0] == '\0')
			continue;
		if (!in && strcmp(row, "ladx-store-vectors 1") == 0 && count == 0)
			continue;
		if (strncmp(row, "V ", 2) == 0) {
			memset(&v, 0, sizeof v);
			v.name = row + 2;
			used = 0;
			in = 1;
			continue;
		}
		if (!in)
			test_fail(TEST, "%s: a line outside a vector: %s", VECTORS, row);
		if (strcmp(row, "end") == 0) {
			replay_vector(&v);
			count++;
			raised += v.want_raise[0] != '\0';
			in = 0;
		} else if (strncmp(row, "info ", 5) == 0) {
			if (split(row + 5, f, 10) != 8)
				test_fail(TEST, "%s: %s: bad info line", VECTORS, v.name);
			v.info.count = (uint32_t)hex(f[0], "count", v.name);
			v.info.pointers_bank = (uint8_t)hex(f[1], "pointers bank", v.name);
			v.info.pointers_addr = (uint32_t)hex(f[2], "pointers addr", v.name);
			v.info.data_bank = strcmp(f[3], "-1") == 0 ? -1 : (int)hex(f[3], "data bank", v.name);
			v.info.has_data_addr = strcmp(f[4], "-") != 0;
			v.info.data_addr = v.info.has_data_addr ? (int32_t)hex(f[4], "data addr", v.name) : 0;
			v.info.data_size = (uint32_t)hex(f[5], "data size", v.name);
			v.info.claim_gaps = f[6][0] == '1';
			v.info.expand = f[7][0] == '1';
		} else if (strncmp(row, "alt ", 4) == 0) {
			if (v.info.alt_count == LADX_STORE_ALT_MAX || split(row + 4, f, 3) != 2)
				test_fail(TEST, "%s: %s: bad alt line", VECTORS, v.name);
			v.info.alt_bank[v.info.alt_count] = (uint8_t)hex(f[0], "alt bank", v.name);
			v.info.alt_addr[v.info.alt_count++] = (uint32_t)hex(f[1], "alt addr", v.name);
		} else if (strncmp(row, "S ", 2) == 0) {
			LadxStoreRegion s;

			if (v.span_count == LADX_STORE_ENTRY_MAX || !region_line(row + 2, &s, v.name))
				test_fail(TEST, "%s: %s: bad S line", VECTORS, v.name);
			v.spans[v.span_count].bank = s.bank;
			v.spans[v.span_count].start = s.start;
			v.spans[v.span_count++].end = s.end;
		} else if (strncmp(row, "R ", 2) == 0) {
			cat(v.want_regions, row);
		} else if (strncmp(row, "add ", 4) == 0) {
			if (v.extra.count == LADX_STORE_REGION_MAX ||
			    !region_line(row + 4, &v.extra.region[v.extra.count++], v.name))
				test_fail(TEST, "%s: %s: bad add line", VECTORS, v.name);
			v.has_extra = 1;
		} else if (strncmp(row, "adjust ", 7) == 0) {
			v.has_adjust = 1;
			v.adjust = (int32_t)hex(row + 7, "adjust", v.name);
		} else if (strncmp(row, "E ", 2) == 0) {
			LadxStoreEntry *e = &v.entries[v.entry_count];
			size_t k, n;

			if (v.entry_count == LADX_STORE_ENTRY_MAX || split(row + 2, f, 3) != 2)
				test_fail(TEST, "%s: %s: bad E line", VECTORS, v.name);
			v.entry_count++;
			if (strcmp(f[0], "int") == 0) {
				e->integer = 1;
				e->value = (uint32_t)hex(f[1], "integer", v.name);
				continue;
			}
			e->bank = (uint8_t)hex(f[0], "bank", v.name);
			n = strlen(f[1]) / 2;
			if (strlen(f[1]) % 2 || used + n > BYTES_MAX)
				test_fail(TEST, "%s: %s: bad E bytes", VECTORS, v.name);
			for (k = 0; k < n; k++) {
				char pair[3] = {f[1][2 * k], f[1][2 * k + 1], '\0'};

				entry_bytes[used + k] = (uint8_t)hex(pair, "byte", v.name);
			}
			e->bytes = entry_bytes + used;
			e->len = (uint32_t)n;
			used += n;
		} else if (strncmp(row, "W ", 2) == 0) {
			cat(v.want_writes, row);
		} else if (strncmp(row, "L ", 2) == 0) {
			cat(v.want_left, row);
		} else if (strncmp(row, "raise ", 6) == 0) {
			cat(v.want_raise, row);
		} else {
			test_fail(TEST, "%s: %s: unknown line %s", VECTORS, v.name, row);
		}
	}
	if (in || count == 0 || raised == 0)
		test_fail(TEST, "%s: %d vectors, %d raising, one unfinished: %d", VECTORS, count, raised, in);
	printf("%s: %d vectors of %s replayed write for write (%d raising)\n", TEST, count, VECTORS, raised);
}

int main(int argc, char **argv)
{
	if (test_fixtures_mode(TEST, argc, argv)) {
		replay_vectors();
		return 0;
	}
	test_regions_merge_one_byte_gap();
	test_suffix_newest();
	test_sprite_errors();
	test_no_room_message();
	printf("%s: %d checks passed\n", TEST, checks);
	return 0;
}
