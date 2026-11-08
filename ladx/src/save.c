#include "ladx_groups.h"

#include <stdio.h>
#include <string.h>

#define OVERWORLD_SPRITES 7
#define INDOOR_SPRITES 8
#define SPRITE_BASE_LOAD 0x0DFAu
#define LD_HL 0x21
#define OLD_BASE 0x763B /* ASM("ld hl, $763B"), LADXR's own source */

typedef struct Save {
	LadxEncoder *enc;
	LadxPipeline *p;
	LadxSpan group;
	int failed; /* the encoder refused */
} Save;

static int write_bytes(Save *s, uint32_t at, const uint8_t *bytes, const int32_t *origins, size_t n)
{
	if (ladx_enc_bank(s->enc, s->group, at, bytes, origins, n) != LADX_ENC_OK) {
		s->failed = 1;
		return 0;
	}
	return 1;
}

static int write_cb(void *ctx, uint8_t bank, uint32_t addr, const uint8_t *bytes, size_t n, int entry)
{
	Save *s = ctx;
	int32_t known[2] = {LADX_ENC_KNOWN, LADX_ENC_KNOWN};
	uint32_t at = (uint32_t)bank * LADX_ROM_BANK + addr;

	if (n == 0)
		return 1; /* an empty slice assignment writes nothing */
	if (entry >= 0)
		return write_bytes(s, at, bytes, s->p->save.origins + s->p->save.at[entry], n);
	return n <= 2 && write_bytes(s, at, bytes, known, n);
}

static int read_cb(void *ctx, uint8_t bank, uint32_t addr, uint8_t *out)
{
	Save *s = ctx;
	int32_t origin;

	return ladx_enc_bank_value(s->enc, (uint32_t)bank * LADX_ROM_BANK + addr, out, &origin);
}

static void info_of(const LadxRomTables *tables, int t, LadxStoreInfo *info)
{
	const LadxRomTable *table = &tables->table[t];
	uint32_t a;

	memset(info, 0, sizeof *info);
	info->count = table->count;
	info->pointers_bank = table->pointers_bank;
	info->pointers_addr = table->pointers_addr;
	info->data_bank = table->data_bank;
	info->has_data_addr = table->data_size != 0;
	info->data_addr = (int32_t)table->data_addr;
	info->data_size = table->data_size;
	info->alt_count = (int)table->alt_count;
	for (a = 0; a < table->alt_count && a < LADX_STORE_ALT_MAX; a++) {
		const LadxRomEntry *e = &tables->entry[table->first + table->count + a];

		info->alt_bank[a] = (uint8_t)(e->pointer_at / LADX_ROM_BANK);
		info->alt_addr[a] = e->pointer_at % LADX_ROM_BANK;
	}
	info->claim_gaps = table->claim_gaps;
	info->expand = table->expand;
}

/* The parse's spans in its order: alt keys, then index keys; integer pointers add none. */
static size_t spans_of(const LadxRomTables *tables, int t, LadxStoreSpan *spans)
{
	const LadxRomTable *table = &tables->table[t];
	size_t n = 0;
	uint32_t k, order;

	for (order = 0; order < table->count + table->alt_count; order++) {
		const LadxRomEntry *e;

		k = order < table->alt_count ? table->count + order : order - table->alt_count;
		e = &tables->entry[table->first + k];
		if (e->integer)
			continue;
		spans[n].bank = e->bank;
		spans[n].start = (int32_t)(e->start % LADX_ROM_BANK);
		spans[n].end = (int32_t)(e->stop - e->start) + spans[n].start;
		n++;
	}
	return n;
}

/* Every entry's current value from enc into p->save: alt keys, then index keys. 0 on a failure. */
static int load_entries(LadxEncoder *enc, LadxPipeline *p, int t)
{
	const LadxRomTables *tables = p->rom->tables;
	const LadxRomTable *table = &tables->table[t];
	LadxPipelineSave *sv = &p->save;
	uint32_t used = 0, order, total = table->count + table->alt_count;

	if (total > LADX_STORE_ENTRY_MAX)
		return 0;
	for (order = 0; order < total; order++) {
		uint32_t k = order < table->alt_count ? table->count + order : order - table->alt_count;
		const LadxRomEntry *e = &tables->entry[table->first + k];
		LadxStoreEntry *out = &sv->entries[order];
		LadxWlogKey key;
		uint32_t value;
		size_t n;
		int kind;

		key.index = e->alt ? 0 : e->index;
		key.alt.at = e->alt;
		key.alt.len = e->alt ? strlen(e->alt) : 0;
		memset(out, 0, sizeof *out);
		out->bank = e->bank;
		sv->at[order] = used;
		kind = ladx_enc_entry_pointer(enc, t, key, &value);
		if (kind < 0)
			return 0;
		if (kind == 1) {
			out->integer = 1;
			out->value = value;
			continue;
		}
		if (!ladx_enc_entry_value(enc, t, key, sv->bytes + used, sv->origins + used, LADX_SAVE_BYTES_MAX - used, &n))
			return 0;
		out->bytes = sv->bytes + used;
		out->len = (uint32_t)n;
		used += (uint32_t)n;
	}
	return 1;
}

static int raise_error(LadxPipeline *p, const char *type, const char *message)
{
	p->raised.kind = LADX_OUTCOME_CRASH;
	p->raised.type = type;
	snprintf(p->raised.text, sizeof p->raised.text, "%s", message);
	p->raised.message = p->raised.text;
	return 0;
}

/* ROM.patch(0x00, 0x0DFA, ASM("ld hl, $763B"), ASM("ld hl, $<base>")), its old-byte check included. */
static int patch_base(Save *s, uint32_t base)
{
	const uint8_t old[3] = {LD_HL, OLD_BASE & 0xFF, OLD_BASE >> 8};
	uint8_t now[3], next[3];
	int32_t origins[3] = {LADX_ENC_KNOWN, LADX_ENC_KNOWN, LADX_ENC_KNOWN}, origin;
	char message[LADX_OUTCOME_TEXT_MAX];
	int i;

	next[0] = LD_HL;
	next[1] = (uint8_t)(base & 0xFF);
	next[2] = (uint8_t)(base >> 8);
	for (i = 0; i < 3; i++)
		if (!ladx_enc_bank_value(s->enc, SPRITE_BASE_LOAD + (uint32_t)i, &now[i], &origin))
			return 0;
	if (memcmp(now, old, 3) != 0) {
		if (memcmp(now, next, 3) == 0)
			return 1;
		snprintf(message, sizeof message, "Patch mismatch:\n%02x%02x%02x !=\n%02x%02x%02x at 0x%04x", now[0],
			 now[1], now[2], old[0], old[1], old[2], SPRITE_BASE_LOAD);
		return raise_error(s->p, "AssertionError", message);
	}
	return write_bytes(s, SPRITE_BASE_LOAD, next, origins, 3);
}

int ladx_group_save(LadxEncoder *enc, LadxPipeline *p)
{
	LadxStoreRegions regions, left, leftover;
	LadxStoreInfo info;
	LadxStoreError err;
	LadxStoreStatus st;
	Save s;
	char message[LADX_OUTCOME_TEXT_MAX];
	const char *type;
	int t;

	if (!p->rom || !p->rom->tables)
		return 0;
	s.enc = enc;
	s.p = p;
	s.group.at = "save";
	s.group.len = strlen(s.group.at);
	s.failed = 0;
	leftover.count = 0;
	for (t = 0; t < LADX_ROM_TABLES; t++) {
		info_of(p->rom->tables, t, &info);
		if (ladx_store_regions(&info, p->save.spans, spans_of(p->rom->tables, t, p->save.spans), &regions) !=
		    LADX_STORE_OK)
			return 0;
		if (t == INDOOR_SPRITES) {
			if (leftover.count == 0 || ladx_store_add_storage(&regions, &leftover) != LADX_STORE_OK ||
			    !patch_base(&s, (uint32_t)leftover.region[0].start | 0x4000u))
				return 0;
			info.data_addr = leftover.region[0].start;
		}
		if (!load_entries(enc, p, t))
			return 0;
		st = ladx_store_table(&info, &regions, p->save.entries, write_cb, read_cb, &s, &p->save.done, &left, &err);
		if (st == LADX_STORE_FAILED || s.failed)
			return 0;
		if (st != LADX_STORE_OK) {
			ladx_store_message(&info, &err, &type, message, sizeof message);
			return raise_error(p, type, message);
		}
		if (t == OVERWORLD_SPRITES)
			leftover = left;
	}
	return 1;
}
