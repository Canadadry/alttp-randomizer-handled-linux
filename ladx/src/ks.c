#include "ladx_ks.h"

#include <string.h>

void ladx_ks_init(LadxKs *ks, const uint8_t *rom, size_t len)
{
	ks->rom = rom;
	ks->len = len > LADX_ROM_SIZE ? LADX_ROM_SIZE : len;
	ks->finished = 0;
	ks->failed = LADX_KS_OK;
	ks->span_count = ks->failure_count = ks->byte_count = 0;
	ks->run_count = ks->window_count = 0;
	memset(ks->mask, 0, sizeof ks->mask);
}

static int bit(const LadxKs *ks, uint32_t q)
{
	return ks->mask[q >> 3] >> (q & 7) & 1;
}

static int label_ok(const LadxKsLabel *label)
{
	if (label->kind == LADX_KS_BOSS_ZERO || label->kind == LADX_KS_BOSS_ITEM)
		return 1;
	return ladx_rom_table_name(label->table) != NULL && memchr(label->alt, '\0', sizeof label->alt) != NULL;
}

LadxKsStatus ladx_ks_add(LadxKs *ks, uint32_t start, uint32_t stop, const LadxKsLabel *label)
{
	uint32_t q, added = 0;

	if (ks->failed)
		return ks->failed;
	if (ks->finished || start >= stop || stop > ks->len || !label_ok(label))
		return LADX_KS_BAD_INPUT;
	if (ks->span_count == LADX_KS_SPAN_MAX)
		return ks->failed = LADX_KS_OVERFLOW;
	for (q = start; q < stop; q++)
		added += !bit(ks, q);
	if (ks->byte_count + added > LADX_KS_BYTE_MAX)
		return ks->failed = LADX_KS_OVERFLOW;
	for (q = start; q < stop; q++)
		ks->mask[q >> 3] |= (uint8_t)(1u << (q & 7));
	ks->byte_count += added;
	ks->span[ks->span_count].start = start;
	ks->span[ks->span_count].stop = stop;
	ks->span[ks->span_count].label = *label;
	ks->span_count++;
	return LADX_KS_OK;
}

LadxKsStatus ladx_ks_add_failure(LadxKs *ks, const LadxKsLabel *label)
{
	if (ks->failed)
		return ks->failed;
	if (ks->finished || !label_ok(label))
		return LADX_KS_BAD_INPUT;
	if (ks->failure_count == LADX_KS_FAILURE_MAX)
		return ks->failed = LADX_KS_OVERFLOW;
	ks->failure[ks->failure_count++] = *label;
	return LADX_KS_OK;
}

/* Heap sort of n uint32 items by before(a, b), with no allocation. */
typedef int (*Before)(const LadxKs *ks, uint32_t a, uint32_t b);

static void sift(const LadxKs *ks, uint32_t *v, size_t root, size_t n, Before before)
{
	for (;;) {
		size_t child = 2 * root + 1;
		uint32_t t;

		if (child >= n)
			return;
		if (child + 1 < n && before(ks, v[child], v[child + 1]))
			child++;
		if (!before(ks, v[root], v[child]))
			return;
		t = v[root];
		v[root] = v[child];
		v[child] = t;
		root = child;
	}
}

static void heap_sort(const LadxKs *ks, uint32_t *v, size_t n, Before before)
{
	size_t i;
	uint32_t t;

	for (i = n / 2; i-- > 0;)
		sift(ks, v, i, n, before);
	for (i = n; i-- > 1;) {
		t = v[0];
		v[0] = v[i];
		v[i] = t;
		sift(ks, v, 0, i, before);
	}
}

static int span_before(const LadxKs *ks, uint32_t a, uint32_t b)
{
	const LadxKsSpan *x = &ks->span[a], *y = &ks->span[b];

	if (x->start != y->start)
		return x->start < y->start;
	if (x->stop != y->stop)
		return x->stop > y->stop;
	return a < b;
}

static uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int window_before(const LadxKs *ks, uint32_t a, uint32_t b)
{
	uint32_t x = be32(ks->rom + a), y = be32(ks->rom + b);

	return x != y ? x < y : a < b;
}

LadxKsStatus ladx_ks_finish(LadxKs *ks)
{
	uint32_t i, q, reach = 0;

	if (ks->failed)
		return ks->failed;
	if (ks->finished)
		return LADX_KS_BAD_INPUT;
	for (i = 0; i < ks->span_count; i++)
		ks->by_start[i] = i;
	heap_sort(ks, ks->by_start, ks->span_count, span_before);
	for (i = 0; i < ks->span_count; i++) {
		if (ks->span[ks->by_start[i]].stop > reach)
			reach = ks->span[ks->by_start[i]].stop;
		ks->reach[i] = reach;
	}
	ks->run_count = 0;
	for (q = 0; q < ks->len; q++) {
		if (!bit(ks, q))
			continue;
		if (ks->run_count && ks->run[ks->run_count - 1].stop == q) {
			ks->run[ks->run_count - 1].stop = q + 1;
		} else {
			ks->run[ks->run_count].start = q;
			ks->run[ks->run_count].stop = q + 1;
			ks->run_count++;
		}
	}
	ks->window_count = 0;
	for (i = 0; i < ks->run_count; i++)
		for (q = ks->run[i].start; q + 4 <= ks->run[i].stop; q++)
			ks->window[ks->window_count++] = q;
	heap_sort(ks, ks->window, ks->window_count, window_before);
	ks->finished = 1;
	return LADX_KS_OK;
}

uint32_t ladx_ks_span_count(const LadxKs *ks)
{
	return ks->span_count;
}

uint32_t ladx_ks_byte_count(const LadxKs *ks)
{
	return ks->byte_count;
}

uint32_t ladx_ks_failure_count(const LadxKs *ks)
{
	return ks->failure_count;
}

static int usable(const LadxKs *ks)
{
	return ks->finished && !ks->failed;
}

int ladx_ks_contains(const LadxKs *ks, int64_t q)
{
	return usable(ks) && q >= 0 && (uint64_t)q < ks->len && bit(ks, (uint32_t)q);
}

static int same_key(const LadxKsLabel *label, LadxWlogKey key)
{
	if (key.alt.len == 0)
		return label->alt[0] == '\0' && label->index == key.index;
	return key.alt.len <= LADX_KS_ALT_MAX && memcmp(label->alt, key.alt.at, key.alt.len) == 0 &&
	       label->alt[key.alt.len] == '\0';
}

int ladx_ks_entry(const LadxKs *ks, int table, LadxWlogKey key, uint32_t *start, uint32_t *stop)
{
	uint32_t i;

	if (!usable(ks))
		return 0;
	for (i = ks->span_count; i-- > 0;) {
		const LadxKsSpan *s = &ks->span[i];

		if (s->label.kind == LADX_KS_ENTRY && s->label.table == table && same_key(&s->label, key)) {
			*start = s->start;
			*stop = s->stop;
			return 1;
		}
	}
	return 0;
}

int ladx_ks_span_at(const LadxKs *ks, int64_t q, uint32_t *start, uint32_t *stop)
{
	uint32_t low = 0, high = ks->span_count, k;
	const LadxKsSpan *s;

	if (!ladx_ks_contains(ks, q))
		return 0;
	/* k: the spans by_start[0..k) start at or before q */
	while (low < high) {
		uint32_t mid = low + (high - low) / 2;

		if (ks->span[ks->by_start[mid]].start <= (uint64_t)q)
			low = mid + 1;
		else
			high = mid;
	}
	k = low;
	low = 0;
	high = k;
	/* the first of them reaching past q is the lowest start, the longest on a tie */
	while (low < high) {
		uint32_t mid = low + (high - low) / 2;

		if (ks->reach[mid] > (uint64_t)q)
			high = mid;
		else
			low = mid + 1;
	}
	if (low == k)
		return 0;
	s = &ks->span[ks->by_start[low]];
	*start = s->start;
	*stop = s->stop;
	return 1;
}

uint32_t ladx_ks_run_count(const LadxKs *ks)
{
	return usable(ks) ? ks->run_count : 0;
}

LadxKsRun ladx_ks_run(const LadxKs *ks, uint32_t i)
{
	LadxKsRun none = { 0, 0 };

	return i < ladx_ks_run_count(ks) ? ks->run[i] : none;
}

/* The run holding q; q is in the map. */
static const LadxKsRun *run_of(const LadxKs *ks, uint32_t q)
{
	uint32_t low = 0, high = ks->run_count;

	while (high - low > 1) {
		uint32_t mid = low + (high - low) / 2;

		if (ks->run[mid].start <= q)
			low = mid;
		else
			high = mid;
	}
	return &ks->run[low];
}

int64_t ladx_ks_find(const LadxKs *ks, const uint8_t *data, size_t n)
{
	uint32_t key, low = 0, high, i;

	if (!usable(ks) || n < 4)
		return -1;
	key = be32(data);
	high = ks->window_count;
	while (low < high) {
		uint32_t mid = low + (high - low) / 2;

		if (be32(ks->rom + ks->window[mid]) < key)
			low = mid + 1;
		else
			high = mid;
	}
	for (i = low; i < ks->window_count && be32(ks->rom + ks->window[i]) == key; i++) {
		uint32_t q = ks->window[i];

		if (q + n <= run_of(ks, q)->stop && memcmp(ks->rom + q, data, n) == 0)
			return q;
	}
	return -1;
}

/* known_source.LAYOUT's KS keys of each table. */
static int known_key(int table, const LadxRomEntry *e)
{
	switch (table) {
	case 0: /* texts */
		return 0;
	case 1: /* entities */
		return !e->alt && e->index < 0x316;
	case 9: /* background_tiles */
		return !e->alt && (e->index == 0x03 || e->index == 0x04 || e->index == 0x0D);
	case 10: /* background_attributes */
		return !e->alt && (e->index == 0x03 || e->index == 0x04);
	default:
		return 1;
	}
}

/* data[i], -1 past n (Python's IndexError). */
static int get(const uint8_t *data, size_t n, size_t i)
{
	return i < n ? data[i] : -1;
}

/* The rebuilt bytes, compared with the entry's as they come. */
typedef struct Rebuilt {
	const uint8_t *data;
	size_t n, m;
	int same;
} Rebuilt;

static void emit(Rebuilt *r, uint8_t b)
{
	if (r->m >= r->n || r->data[r->m] != b)
		r->same = 0;
	r->m++;
}

/*
 * known_source.fields then rebuild, ported literally: 1 when the fields
 * parse and give data's n bytes back.
 */
static int rebuilds(LadxRomKind kind, const uint8_t *data, size_t n)
{
	Rebuilt out;
	size_t i = 0, k;
	int v;

	out.data = data;
	out.n = n;
	out.m = 0;
	out.same = 1;

	switch (kind) {
	case LADX_ROM_KIND_SPRITE:
		for (k = 0; k < n; k++)
			emit(&out, data[k]);
		break;
	case LADX_ROM_KIND_DATA:
		while ((v = get(data, n, i)) != 0xFF) {
			int t = get(data, n, i + 1);

			if (v < 0 || t < 0)
				return 0;
			emit(&out, (uint8_t)((v & 0x0F) | (v >> 4) << 4));
			emit(&out, (uint8_t)t);
			i += 2;
		}
		if (i != n - 1)
			return 0;
		emit(&out, 0xFF);
		break;
	case LADX_ROM_KIND_ROOMS:
		if (n < 2)
			return 0;
		emit(&out, data[0]);
		emit(&out, data[1]);
		i = 2;
		while ((v = get(data, n, i)) != 0xFE) {
			int nibble = v >> 4;

			if (v < 0)
				return 0;
			if (nibble == 0x8 || nibble == 0xC) {
				int b = get(data, n, i + 1), c = get(data, n, i + 2);

				if (b < 0 || c < 0)
					return 0;
				emit(&out, (uint8_t)((nibble == 0xC ? 0xC0 : 0x80) | (v & 0x0F)));
				emit(&out, (uint8_t)((b & 0x0F) | (b >> 4) << 4));
				emit(&out, (uint8_t)c);
				i += 3;
			} else if (nibble == 0xE) {
				if (i + 5 > n)
					return 0;
				emit(&out, (uint8_t)(0xE0 | (v & 0x0F)));
				for (k = 1; k < 5; k++)
					emit(&out, data[i + k]);
				i += 5;
			} else {
				int b = get(data, n, i + 1);

				if (b < 0)
					return 0;
				emit(&out, (uint8_t)((v & 0x0F) | (v >> 4) << 4));
				emit(&out, (uint8_t)b);
				i += 2;
			}
		}
		if (i != n - 1)
			return 0;
		emit(&out, 0xFE);
		break;
	case LADX_ROM_KIND_BACKGROUND:
		while ((v = get(data, n, i)) != 0x00) {
			int lo = get(data, n, i + 1), flags = get(data, n, i + 2);
			size_t count;

			if (v < 0 || lo < 0 || flags < 0)
				return 0;
			count = (flags & 0x40) ? 1 : (size_t)(flags & 0x3F) + 1;
			if (i + 3 + count > n)
				return 0;
			emit(&out, (uint8_t)v);
			emit(&out, (uint8_t)lo);
			emit(&out, (uint8_t)((flags >> 7) << 7 | (flags >> 6 & 1) << 6 | (flags & 0x3F)));
			for (k = 0; k < count; k++)
				emit(&out, data[i + 3 + k]);
			i += 3 + count;
		}
		if (i != n - 1)
			return 0;
		emit(&out, 0x00);
		break;
	default:
		return 0;
	}
	return out.same && out.m == n;
}

static LadxKsLabel entry_label(LadxKsKind kind, int table, const LadxRomEntry *e)
{
	LadxKsLabel label;

	memset(&label, 0, sizeof label);
	label.kind = kind;
	label.table = table;
	label.index = e->index;
	if (e->alt) {
		strncpy(label.alt, e->alt, LADX_KS_ALT_MAX);
		label.alt[LADX_KS_ALT_MAX] = '\0';
	}
	return label;
}

static LadxKsLabel boss_label(LadxKsKind kind, uint32_t index)
{
	LadxKsLabel label;

	memset(&label, 0, sizeof label);
	label.kind = kind;
	label.table = -1;
	label.index = index;
	return label;
}

/* The pointer bytes _pointers read for e, kept when they rebuild from e. */
static void add_pointer(LadxKs *ks, const uint8_t *rom, int t, const LadxRomTable *table, const LadxRomEntry *e)
{
	uint8_t rebuilt[2];
	size_t length;
	LadxKsLabel label = entry_label(LADX_KS_POINTER, t, e);

	if (e->integer) {
		rebuilt[0] = (uint8_t)(e->pointer & 0xFF);
		rebuilt[1] = (uint8_t)(e->pointer >> 8);
		length = 2;
	} else if (table->data_size) {
		uint32_t slot = (e->start % LADX_ROM_BANK - table->data_addr) / table->data_size;

		rebuilt[0] = (uint8_t)slot;
		length = 1;
	} else {
		uint32_t pointer = (e->start % LADX_ROM_BANK) | 0x4000u;

		rebuilt[0] = (uint8_t)(pointer & 0xFF);
		rebuilt[1] = (uint8_t)(pointer >> 8);
		length = 2;
	}
	if (length == e->pointer_len && memcmp(rom + e->pointer_at, rebuilt, length) == 0)
		ladx_ks_add(ks, e->pointer_at, e->pointer_at + (uint32_t)length, &label);
	else
		ladx_ks_add_failure(ks, &label);
	if (e->bank_at >= 0 && !e->alt) {
		label = entry_label(LADX_KS_BANK, t, e);
		if (rom[e->bank_at] == e->bank)
			ladx_ks_add(ks, (uint32_t)e->bank_at, (uint32_t)e->bank_at + 1, &label);
		else
			ladx_ks_add_failure(ks, &label);
	}
}

#define BOSS_ZERO (0x14u * LADX_ROM_BANK + 0x04E0u)
#define BOSS_ZERO_COUNT 64u
#define BOSS_ITEM (0x3Eu * LADX_ROM_BANK + 0x3300u)

static const uint32_t boss_rooms[] = { 0x106, 0x12B, 0x15A, 0x166, 0x185, 0x1BC, 0x223, 0x234, 0x300 };

LadxKsStatus ladx_ks_build(LadxKs *ks, const uint8_t *rom, size_t len, const LadxRomTables *tables)
{
	uint32_t i, q;
	int t;

	ladx_ks_init(ks, rom, len);
	if (len != LADX_ROM_SIZE)
		return ks->failed = LADX_KS_BAD_INPUT;
	for (t = 0; t < LADX_ROM_TABLES; t++) {
		const LadxRomTable *table = &tables->table[t];
		const LadxRomEntry *entry = &tables->entry[table->first];

		for (i = 0; i < table->alt_count; i++)
			add_pointer(ks, rom, t, table, &entry[table->count + i]);
		for (i = 0; i < table->count; i++)
			add_pointer(ks, rom, t, table, &entry[i]);
		for (i = 0; i < table->count + table->alt_count; i++) {
			const LadxRomEntry *e = &entry[i];
			LadxKsLabel label;

			if (e->integer || !known_key(t, e))
				continue;
			label = entry_label(LADX_KS_ENTRY, t, e);
			if (rebuilds(table->kind, rom + e->start, e->stop - e->start))
				ladx_ks_add(ks, e->start, e->stop, &label);
			else
				ladx_ks_add_failure(ks, &label);
		}
	}
	for (q = BOSS_ZERO; q < BOSS_ZERO + BOSS_ZERO_COUNT; q++) {
		if (rom[q] == 0x00) {
			LadxKsLabel label = boss_label(LADX_KS_BOSS_ZERO, q);

			ladx_ks_add(ks, q, q + 1, &label);
		}
	}
	for (i = 0; i < sizeof boss_rooms / sizeof boss_rooms[0]; i++) {
		LadxKsLabel label = boss_label(LADX_KS_BOSS_ITEM, boss_rooms[i]);

		ladx_ks_add(ks, BOSS_ITEM + boss_rooms[i], BOSS_ITEM + boss_rooms[i] + 1, &label);
	}
	return ladx_ks_finish(ks);
}

static size_t put_hex(char *out, uint32_t v)
{
	static const char digits[] = "0123456789abcdef";
	char tmp[8];
	size_t n = 0, i;

	do {
		tmp[n++] = digits[v & 0x0F];
		v >>= 4;
	} while (v);
	for (i = 0; i < n; i++)
		out[i] = tmp[n - 1 - i];
	return n;
}

static size_t put_text(char *out, const char *s)
{
	size_t n = strlen(s);

	memcpy(out, s, n);
	return n;
}

static size_t put_label(char *out, const LadxKsLabel *label)
{
	size_t n = 0;

	if (label->kind == LADX_KS_BOSS_ZERO || label->kind == LADX_KS_BOSS_ITEM) {
		n += put_text(out, label->kind == LADX_KS_BOSS_ZERO ? "boss zero " : "boss item ");
		return n + put_hex(out + n, label->index);
	}
	n += put_text(out, ladx_rom_table_name(label->table));
	if (label->kind == LADX_KS_POINTER)
		n += put_text(out + n, " pointer");
	else if (label->kind == LADX_KS_BANK)
		n += put_text(out + n, " bank");
	out[n++] = ' ';
	if (label->alt[0])
		return n + put_text(out + n, label->alt);
	return n + put_hex(out + n, label->index);
}

void ladx_ks_render_spans(const LadxKs *ks, LadxKsSink sink, void *ctx)
{
	char line[96];
	uint32_t i;
	size_t n;

	sink(ctx, "ladx-ks-spans 1\n", 16);
	for (i = 0; i < ks->span_count; i++) {
		const LadxKsSpan *s = &ks->span[i];

		n = put_hex(line, s->start);
		line[n++] = ' ';
		n += put_hex(line + n, s->stop - s->start);
		line[n++] = ' ';
		n += put_label(line + n, &s->label);
		line[n++] = '\n';
		sink(ctx, line, n);
	}
	for (i = 0; i < ks->failure_count; i++) {
		n = put_text(line, "failure ");
		n += put_label(line + n, &ks->failure[i]);
		line[n++] = '\n';
		sink(ctx, line, n);
	}
}
