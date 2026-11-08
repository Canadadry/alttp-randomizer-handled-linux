#include "ladx_store.h"

#include <stdio.h>
#include <string.h>

#define BANK_SIZE 0x4000

/* _addStorage: the first region of the bank it touches or holds absorbs it, else a new one. */
static LadxStoreStatus add(LadxStoreRegions *r, uint8_t bank, int32_t start, int32_t end)
{
	int n;

	for (n = 0; n < r->count; n++) {
		LadxStoreRegion *d = &r->region[n];

		if (d->bank != bank)
			continue;
		if (d->start == end) {
			d->start = start;
			return LADX_STORE_OK;
		}
		if (d->end == start) {
			d->end = end;
			return LADX_STORE_OK;
		}
		if (d->start <= start && d->end >= end)
			return LADX_STORE_OK;
	}
	if (r->count == LADX_STORE_REGION_MAX)
		return LADX_STORE_FAILED;
	r->region[r->count].bank = bank;
	r->region[r->count].start = start;
	r->region[r->count].end = end;
	r->count++;
	return LADX_STORE_OK;
}

/* __mergeStorage: one merge (n absorbs m when m.start - 1 <= n.end <= m.end); 0 when none. */
static int merge_one(LadxStoreRegions *r)
{
	int n, m;

	for (n = 0; n < r->count; n++)
		for (m = 0; m < r->count; m++) {
			LadxStoreRegion *a = &r->region[n], *b = &r->region[m];

			if (m == n || a->bank != b->bank)
				continue;
			if (b->start - 1 <= a->end && a->end <= b->end) {
				if (b->start < a->start)
					a->start = b->start;
				a->end = b->end;
				memmove(&r->region[m], &r->region[m + 1], (size_t)(r->count - m - 1) * sizeof r->region[0]);
				r->count--;
				return 1;
			}
		}
	return 0;
}

/* Python's list.sort(key=start): stable. */
static void sort_by_start(LadxStoreRegions *r)
{
	int i, j;

	for (i = 1; i < r->count; i++) {
		LadxStoreRegion v = r->region[i];

		for (j = i; j > 0 && r->region[j - 1].start > v.start; j--)
			r->region[j] = r->region[j - 1];
		r->region[j] = v;
	}
}

static void settle(LadxStoreRegions *r)
{
	while (merge_one(r))
		;
	sort_by_start(r);
}

LadxStoreStatus ladx_store_regions(const LadxStoreInfo *info, const LadxStoreSpan *spans, size_t n,
				   LadxStoreRegions *out)
{
	size_t i;
	int a, b;

	out->count = 0;
	for (i = 0; i < n; i++)
		if (add(out, spans[i].bank, spans[i].start, spans[i].end) != LADX_STORE_OK)
			return LADX_STORE_FAILED;
	settle(out);
	if (info->claim_gaps && out->count > 0) {
		out->region[0].end = out->region[out->count - 1].end;
		out->count = 1;
	}
	if (info->expand)
		for (a = 0; a < out->count; a++) {
			int expand = 1;

			for (b = 0; b < out->count; b++)
				if (out->region[a].bank == out->region[b].bank && out->region[a].end < out->region[b].end)
					expand = 0;
			if (expand)
				out->region[a].end = BANK_SIZE;
		}
	return LADX_STORE_OK;
}

LadxStoreStatus ladx_store_add_storage(LadxStoreRegions *r, const LadxStoreRegions *extra)
{
	int i;

	for (i = 0; i < extra->count; i++)
		if (add(r, extra->region[i].bank, extra->region[i].start, extra->region[i].end) != LADX_STORE_OK)
			return LADX_STORE_FAILED;
	settle(r);
	return LADX_STORE_OK;
}

/* First fit: the first region of bank with len bytes left, its start moved past them; -1 for none. */
static int32_t place(LadxStoreRegions *s, uint8_t bank, uint32_t len)
{
	int i;

	for (i = 0; i < s->count; i++) {
		LadxStoreRegion *st = &s->region[i];

		if (st->end - st->start >= (int32_t)len && st->bank == bank) {
			st->start += (int32_t)len;
			return st->start - (int32_t)len;
		}
	}
	return -1;
}

/*
 * The done dict's lookup: the newest stored entry of bank that registered
 * e's bytes (a non-empty tail of it, or for sprite data, or an empty e, the
 * whole of it); its pointer, -1 for none.
 */
static int32_t done_lookup(const LadxStoreInfo *info, const LadxStoreEntry *entries, const LadxStoreDone *done,
			   const LadxStoreEntry *e)
{
	uint32_t k;

	for (k = done->count; k-- > 0;) {
		const LadxStoreEntry *d = &entries[done->entry[k]];

		if (d->bank != e->bank || d->len < e->len)
			continue;
		if ((info->data_size || e->len == 0) && d->len != e->len)
			continue;
		if (memcmp(d->bytes + d->len - e->len, e->bytes, e->len) == 0)
			return done->pointer[k] + (int32_t)(d->len - e->len);
	}
	return -1;
}

static LadxStoreStatus no_room(LadxStoreError *err, const LadxStoreRegions *s, int alt, uint32_t entry,
			       uint8_t bank)
{
	err->status = LADX_STORE_NO_ROOM;
	err->alt = alt;
	err->entry = entry;
	err->bank = bank;
	err->storage = *s;
	return err->status;
}

static LadxStoreStatus fail(LadxStoreError *err, LadxStoreStatus status, uint32_t entry, uint8_t bank)
{
	err->status = status;
	err->entry = entry;
	err->bank = bank;
	return status;
}

static int pointer16(LadxStoreWrite write, void *ctx, uint8_t bank, uint32_t addr, uint32_t pointer)
{
	uint8_t lo = (uint8_t)(pointer & 0xFF), hi = (uint8_t)(((pointer >> 8) & 0xFF) | 0x40);

	return write(ctx, bank, addr, &lo, 1, -1) && write(ctx, bank, addr + 1, &hi, 1, -1);
}

/* Python's floor division and modulo. */
static int32_t floor_div(int32_t a, int32_t b)
{
	int32_t q = a / b;

	return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

LadxStoreStatus ladx_store_table(const LadxStoreInfo *info, const LadxStoreRegions *regions,
				 const LadxStoreEntry *entries, LadxStoreWrite write, LadxStoreRead read, void *ctx,
				 LadxStoreDone *done, LadxStoreRegions *left, LadxStoreError *err)
{
	uint32_t n, i, base = (uint32_t)info->alt_count;
	int a;

	memset(err, 0, sizeof *err);
	*left = *regions;
	done->count = 0;
	if (info->count + base > LADX_STORE_ENTRY_MAX)
		return fail(err, LADX_STORE_FAILED, 0, 0);
	for (a = 0; a < info->alt_count; a++) {
		const LadxStoreEntry *e = &entries[a];
		uint8_t bank = (uint8_t)info->data_bank;
		int32_t pointer = place(left, bank, e->len);

		if (pointer < 0)
			return no_room(err, left, 1, (uint32_t)a, bank);
		if (!write(ctx, bank, (uint32_t)pointer, e->bytes, e->len, a) ||
		    !pointer16(write, ctx, info->alt_bank[a], info->alt_addr[a], (uint32_t)pointer))
			return fail(err, LADX_STORE_FAILED, (uint32_t)a, bank);
	}
	for (n = 0; n < info->count; n++) {
		const LadxStoreEntry *e = &entries[base + n];
		int32_t pointer;

		if (e->integer) {
			pointer = (int32_t)e->value;
		} else {
			pointer = done_lookup(info, entries, done, e);
			if (pointer >= 0) {
				for (i = 0; i < e->len; i++) {
					uint8_t b;

					if ((uint32_t)pointer + i >= BANK_SIZE)
						return fail(err, LADX_STORE_READBACK, base + n, e->bank);
					if (!read(ctx, e->bank, (uint32_t)pointer + i, &b))
						return fail(err, LADX_STORE_FAILED, base + n, e->bank);
					if (b != e->bytes[i])
						return fail(err, LADX_STORE_READBACK, base + n, e->bank);
				}
			} else {
				pointer = place(left, e->bank, e->len);
				if (pointer < 0)
					return no_room(err, left, 0, base + n, e->bank);
				if (!write(ctx, e->bank, (uint32_t)pointer, e->bytes, e->len, (int)(base + n)))
					return fail(err, LADX_STORE_FAILED, base + n, e->bank);
				done->entry[done->count] = base + n;
				done->pointer[done->count] = pointer;
				done->count++;
			}
		}
		if (info->has_data_addr) {
			int32_t offset = pointer - info->data_addr;
			uint8_t byte;

			if (info->data_size) {
				if (offset - floor_div(offset, (int32_t)info->data_size) * (int32_t)info->data_size != 0)
					return fail(err, LADX_STORE_MISALIGNED, base + n, e->bank);
				offset = floor_div(offset, (int32_t)info->data_size);
			}
			if (offset < 0 || offset > 255)
				return fail(err, LADX_STORE_BYTE_RANGE, base + n, e->bank);
			byte = (uint8_t)offset;
			if (!write(ctx, info->pointers_bank, info->pointers_addr + n, &byte, 1, -1))
				return fail(err, LADX_STORE_FAILED, base + n, e->bank);
		} else if (!pointer16(write, ctx, info->pointers_bank, info->pointers_addr + n * 2, (uint32_t)pointer)) {
			return fail(err, LADX_STORE_FAILED, base + n, e->bank);
		}
	}
	return LADX_STORE_OK;
}

/* Appends to buf within cap, counting the full length. */
typedef struct Text {
	char *buf;
	size_t cap, len;
} Text;

static void put_text(Text *t, const char *s)
{
	for (; *s; s++, t->len++)
		if (t->len + 1 < t->cap)
			t->buf[t->len] = *s;
}

static void put(Text *t, const char *fmt, long a, long b, long c)
{
	char piece[96];

	snprintf(piece, sizeof piece, fmt, a, b, c);
	put_text(t, piece);
}

size_t ladx_store_message(const LadxStoreInfo *info, const LadxStoreError *err, const char **type, char *buf,
			  size_t cap)
{
	Text t;
	int i;

	t.buf = buf;
	t.cap = cap;
	t.len = 0;
	*type = err->status == LADX_STORE_BYTE_RANGE ? "ValueError" : "AssertionError";
	if (err->status == LADX_STORE_BYTE_RANGE)
		put_text(&t, "byte must be in range(0, 256)");
	if (err->status == LADX_STORE_NO_ROOM) {
		uint32_t n = err->entry - (uint32_t)info->alt_count;

		put_text(&t, "Not enough room in storage... ");
		if (!err->alt)
			put(&t, "%ld/%ld ", (long)n, (long)info->count, 0L);
		put_text(&t, "[");
		for (i = 0; i < err->storage.count; i++)
			put(&t, i ? ", {'bank': %ld, 'start': %ld, 'end': %ld}" : "{'bank': %ld, 'start': %ld, 'end': %ld}",
			    (long)err->storage.region[i].bank, (long)err->storage.region[i].start,
			    (long)err->storage.region[i].end);
		put_text(&t, "]");
		if (!err->alt) {
			put(&t, " id:%lx(%ld) bank:%ld", (long)n, (long)n, (long)err->bank);
		}
	}
	if (cap)
		buf[t.len < cap ? t.len : cap - 1] = '\0';
	return t.len;
}
