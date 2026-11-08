#include "ladx_encoder.h"

#include <string.h>

#define MIN_MATCH 4
#define UNSET INT32_MIN /* no atom yet */
#define SIDE_C 1
#define SIDE_TABLE 2

static LadxEncStatus fail(LadxEncoder *enc, LadxEncStatus st)
{
	if (st == LADX_ENC_OVERFLOW)
		enc->failed = st;
	return st;
}

static LadxEncStatus wlog_status(LadxEncoder *enc, LadxWlogStatus st)
{
	if (st == LADX_WLOG_OK)
		return LADX_ENC_OK;
	return enc->failed = LADX_ENC_OVERFLOW;
}

/* Atoms and origins ---------------------------------------------------------------------------- */

static int knowable(const LadxEncoder *enc, LadxAtom a)
{
	return a < 0 || (enc->ks && ladx_ks_contains(enc->ks, a));
}

/* Only for a knowable atom: a reference is then inside the ROM. */
static int atom_value(const LadxEncoder *enc, LadxAtom a)
{
	return a >= 0 ? enc->rom[a] : -1 - a;
}

static int origin_ok(int32_t q)
{
	return q >= LADX_ENC_KNOWN || -2 - (int64_t)q < LADX_ENC_IDENTITY_MAX;
}

/* The atom an origin gives by itself; UNSET for KNOWN and an identity not met yet. */
static LadxAtom carried(const LadxEncoder *enc, int32_t q)
{
	if (q >= 0)
		return q;
	if (q == LADX_ENC_KNOWN)
		return UNSET;
	return enc->written[-2 - q];
}

static void see(LadxEncoder *enc, int32_t q)
{
	if (q < LADX_ENC_KNOWN && (uint32_t)(-2 - q) >= enc->next_identity && (uint32_t)(-2 - q) < enc->auto_low)
		enc->next_identity = (uint32_t)(-2 - q) + 1;
}

static void name(LadxEncoder *enc, int32_t q, LadxAtom a)
{
	if (q < LADX_ENC_KNOWN && enc->written[-2 - q] == UNSET)
		enc->written[-2 - q] = a;
}

static void rename_to(LadxEncoder *enc, int32_t q, LadxAtom a)
{
	if (q < LADX_ENC_KNOWN && a >= 0)
		enc->written[-2 - q] = a;
}

/* origins with every KNOWN byte given a write identity of its own, into out; 0 on overflow. */
static int own_identities(LadxEncoder *enc, const int32_t *origins, int32_t *out, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		out[i] = origins[i];
		if (origins[i] != LADX_ENC_KNOWN)
			continue;
		if (enc->auto_low <= enc->next_identity)
			return 0;
		out[i] = LADX_ENC_IDENTITY(--enc->auto_low);
	}
	return 1;
}

static LadxEncStatus check_origins(const int32_t *origins, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (!origin_ok(origins[i]))
			return LADX_ENC_BAD_INPUT;
	return LADX_ENC_OK;
}

int32_t ladx_enc_fresh(LadxEncoder *enc, uint32_t n)
{
	int32_t first;

	if (enc->failed || (uint64_t)enc->next_identity + n > enc->auto_low) {
		enc->failed = LADX_ENC_OVERFLOW;
		return LADX_ENC_KNOWN;
	}
	first = LADX_ENC_IDENTITY(enc->next_identity);
	enc->next_identity += n;
	return first;
}

/* Entries -------------------------------------------------------------------------------------- */

static uint32_t key_hash(int table, LadxWlogKey key)
{
	uint32_t h = 2166136261u ^ (uint32_t)table;
	size_t i;

	h = (h ^ key.index) * 16777619u;
	for (i = 0; i < key.alt.len; i++)
		h = (h ^ (uint8_t)key.alt.at[i]) * 16777619u;
	return h;
}

static int entry_is(const LadxEncEntry *e, int table, LadxWlogKey key)
{
	return e->table == table && e->alt_len == key.alt.len &&
	       (key.alt.len ? memcmp(e->alt, key.alt.at, key.alt.len) == 0 : e->index == key.index);
}

/* The entry's slot: its index there, or -1 - the empty slot it would take. */
static int32_t find_slot(const LadxEncoder *enc, int table, LadxWlogKey key)
{
	uint32_t mask = 2 * LADX_ENC_ENTRY_MAX - 1, s = key_hash(table, key) & mask;

	while (enc->slot[s] >= 0) {
		if (entry_is(&enc->entry[enc->slot[s]], table, key))
			return (int32_t)s;
		s = (s + 1) & mask;
	}
	return -1 - (int32_t)s;
}

static LadxEncEntry *find_entry(const LadxEncoder *enc, int table, LadxWlogKey key)
{
	int32_t s = find_slot(enc, table, key);

	return s >= 0 ? (LadxEncEntry *)&enc->entry[enc->slot[s]] : NULL;
}

static int key_ok(const LadxEncoder *enc, int table, LadxWlogKey key)
{
	size_t i;

	if (table < 0 || table >= enc->log->table_count || key.alt.len > LADX_WLOG_NAME_MAX)
		return 0;
	for (i = 0; i < key.alt.len; i++)
		if (key.alt.at[i] == ' ' || key.alt.at[i] == '\0' || key.alt.at[i] == '\n')
			return 0;
	return 1;
}

static LadxEncEntry *get_entry(LadxEncoder *enc, int table, LadxWlogKey key)
{
	int32_t s = find_slot(enc, table, key);
	LadxEncEntry *e;

	if (s >= 0)
		return &enc->entry[enc->slot[s]];
	if (enc->entry_count == LADX_ENC_ENTRY_MAX)
		return NULL;
	e = &enc->entry[enc->entry_count];
	memset(e, 0, sizeof *e);
	e->table = (uint8_t)table;
	e->alt_len = (uint8_t)key.alt.len;
	memcpy(e->alt, key.alt.at, key.alt.len);
	e->index = key.alt.len ? 0 : key.index;
	enc->slot[-1 - s] = (int32_t)enc->entry_count++;
	return e;
}

static LadxWlogKey entry_key(const LadxEncEntry *e)
{
	LadxWlogKey key;

	key.index = e->index;
	key.alt.at = e->alt;
	key.alt.len = e->alt_len;
	return key;
}

static int pool_room(const LadxEncoder *enc, size_t n)
{
	return (uint64_t)enc->pool_count + n <= LADX_ENC_POOL_MAX;
}

static void set_value(LadxEncoder *enc, LadxEncEntry *e, const LadxAtom *atoms, const int32_t *origins, size_t n)
{
	e->pointer = 0;
	e->first = enc->pool_count;
	e->count = (uint32_t)n;
	memcpy(enc->pool_atom + e->first, atoms, n * sizeof atoms[0]);
	memcpy(enc->pool_origin + e->first, origins, n * sizeof origins[0]);
	enc->pool_count += (uint32_t)n;
}

LadxEncStatus ladx_enc_initial(LadxEncoder *enc, int table, LadxWlogKey key, const uint8_t *values,
			       const int32_t *origins, size_t n)
{
	LadxEncEntry *e;
	size_t i;

	if (enc->failed)
		return enc->failed;
	if (!key_ok(enc, table, key) || n > LADX_ENC_VALUE_MAX || check_origins(origins, n))
		return LADX_ENC_BAD_INPUT;
	if (!pool_room(enc, n) || !(e = get_entry(enc, table, key)))
		return fail(enc, LADX_ENC_OVERFLOW);
	for (i = 0; i < n; i++)
		enc->atoms[i] = origins[i] >= 0 ? origins[i] : LADX_ATOM_LITERAL(values[i]);
	set_value(enc, e, enc->atoms, origins, n);
	for (i = 0; i < n; i++)
		see(enc, origins[i]);
	return LADX_ENC_OK;
}

LadxEncStatus ladx_enc_initial_pointer(LadxEncoder *enc, int table, LadxWlogKey key, uint32_t value)
{
	LadxEncEntry *e;

	if (enc->failed)
		return enc->failed;
	if (!key_ok(enc, table, key))
		return LADX_ENC_BAD_INPUT;
	if (!(e = get_entry(enc, table, key)))
		return fail(enc, LADX_ENC_OVERFLOW);
	e->pointer = 1;
	e->value = value;
	e->count = 0;
	return LADX_ENC_OK;
}

static LadxEncStatus initial_tables(LadxEncoder *enc, const LadxRomTables *tables)
{
	int t;
	uint32_t i, k;

	for (t = 0; t < LADX_ROM_TABLES; t++) {
		const LadxRomTable *table = &tables->table[t];

		for (i = table->first; i < table->first + table->count + table->alt_count; i++) {
			const LadxRomEntry *r = &tables->entry[i];
			LadxWlogKey key;
			LadxEncEntry *e;

			key.index = r->alt ? 0 : r->index;
			key.alt.at = r->alt ? r->alt : "";
			key.alt.len = r->alt ? strlen(r->alt) : 0;
			if (!key_ok(enc, t, key))
				return LADX_ENC_BAD_INPUT;
			if (r->integer) {
				LadxEncStatus st = ladx_enc_initial_pointer(enc, t, key, r->pointer);

				if (st)
					return st;
				continue;
			}
			if (r->stop - r->start > LADX_ENC_VALUE_MAX || !pool_room(enc, r->stop - r->start) ||
			    !(e = get_entry(enc, t, key)))
				return fail(enc, LADX_ENC_OVERFLOW);
			for (k = r->start; k < r->stop; k++)
				enc->atoms[k - r->start] = (LadxAtom)k;
			set_value(enc, e, enc->atoms, enc->atoms, r->stop - r->start);
		}
	}
	return LADX_ENC_OK;
}

LadxEncStatus ladx_enc_init(LadxEncoder *enc, const uint8_t *rom, size_t rom_len, const LadxKs *ks,
			    const LadxRomTables *tables, LadxWlog *log)
{
	uint32_t p;

	if (!log)
		return LADX_ENC_BAD_INPUT;
	enc->rom = rom;
	enc->rom_len = rom ? rom_len : 0;
	enc->ks = rom ? ks : NULL;
	enc->log = log;
	enc->positions = rom ? (rom_len < LADX_ROM_SIZE ? (uint32_t)rom_len : LADX_ROM_SIZE) : LADX_ROM_SIZE;
	enc->failed = LADX_ENC_OK;
	enc->open = 0;
	enc->group[0] = '\0';
	enc->segment = 0;
	for (p = 0; p < enc->positions; p++)
		enc->state[p] = enc->origin[p] = (int32_t)p;
	memset(enc->touched, 0, (enc->positions + 7) / 8);
	enc->touched_count = 0;
	for (p = 0; p < LADX_ENC_IDENTITY_MAX; p++)
		enc->written[p] = UNSET;
	enc->next_identity = 0;
	enc->auto_low = LADX_ENC_IDENTITY_MAX;
	enc->entry_count = 0;
	for (p = 0; p < 2 * LADX_ENC_ENTRY_MAX; p++)
		enc->slot[p] = -1;
	enc->seg_entry_count = 0;
	enc->pool_count = 0;
	return tables ? initial_tables(enc, tables) : LADX_ENC_OK;
}

/* Queries -------------------------------------------------------------------------------------- */

int32_t ladx_enc_bank_origin(const LadxEncoder *enc, uint32_t offset)
{
	return offset < enc->positions ? enc->origin[offset] : (int32_t)offset;
}

int ladx_enc_bank_value(const LadxEncoder *enc, uint32_t offset, uint8_t *value, int32_t *origin)
{
	LadxAtom a;

	if (offset >= enc->positions)
		return 0;
	a = enc->state[offset];
	if (a >= 0 && (!enc->rom || (size_t)a >= enc->rom_len))
		return 0;
	*value = (uint8_t)atom_value(enc, a);
	*origin = enc->origin[offset];
	return 1;
}

int ladx_enc_entry_origins(const LadxEncoder *enc, int table, LadxWlogKey key, int32_t *out, size_t cap,
			   size_t *n)
{
	const LadxEncEntry *e = find_entry(enc, table, key);

	if (!e || e->pointer || e->count > cap)
		return 0;
	memcpy(out, enc->pool_origin + e->first, e->count * sizeof out[0]);
	*n = e->count;
	return 1;
}

int ladx_enc_entry_value(const LadxEncoder *enc, int table, LadxWlogKey key, uint8_t *values, int32_t *origins,
			 size_t cap, size_t *n)
{
	const LadxEncEntry *e = find_entry(enc, table, key);
	uint32_t i;

	if (!e || e->pointer || e->count > cap)
		return 0;
	for (i = 0; i < e->count; i++) {
		LadxAtom a = enc->pool_atom[e->first + i];

		if (a >= 0 && (!enc->rom || (size_t)a >= enc->rom_len))
			return 0;
		values[i] = (uint8_t)atom_value(enc, a);
	}
	memcpy(origins, enc->pool_origin + e->first, e->count * sizeof origins[0]);
	*n = e->count;
	return 1;
}

int ladx_enc_entry_pointer(const LadxEncoder *enc, int table, LadxWlogKey key, uint32_t *value)
{
	const LadxEncEntry *e = find_entry(enc, table, key);

	if (!e)
		return -1;
	if (!e->pointer)
		return 0;
	*value = e->value;
	return 1;
}

/* Segment state -------------------------------------------------------------------------------- */

static void touch(LadxEncoder *enc, uint32_t p)
{
	if (enc->touched[p >> 3] & (1u << (p & 7)))
		return;
	enc->touched[p >> 3] |= (uint8_t)(1u << (p & 7));
	if (enc->touched_count == 0 || p < enc->touched_low)
		enc->touched_low = p;
	if (enc->touched_count == 0 || p > enc->touched_high)
		enc->touched_high = p;
	enc->touched_count++;
}

/* The segment's bank positions into enc->sorted, ascending, their marks cleared. */
static uint32_t take_touched(LadxEncoder *enc)
{
	uint32_t n = 0, p;

	if (enc->touched_count == 0)
		return 0;
	for (p = enc->touched_low; p <= enc->touched_high; p++)
		if (enc->touched[p >> 3] & (1u << (p & 7))) {
			enc->touched[p >> 3] &= (uint8_t)~(1u << (p & 7));
			enc->sorted[n++] = p;
		}
	enc->touched_count = 0;
	return n;
}

static void touch_entry(LadxEncoder *enc, LadxEncEntry *e)
{
	if (e->segment == enc->segment)
		return;
	e->segment = enc->segment;
	enc->seg_entry[enc->seg_entry_count++] = (uint32_t)(e - enc->entry);
}

/* KS-match: _KnownRuns.match and .atoms ----------------------------------------------------------- */

/* The longest prefix of data[0..room) of at least MIN_MATCH bytes inside one KS run: its length, 0 for none. */
static size_t ks_match(const LadxKs *ks, const uint8_t *data, size_t room, int64_t *at)
{
	size_t low = MIN_MATCH, high = room + 1;

	if (room < MIN_MATCH || ladx_ks_find(ks, data, MIN_MATCH) < 0)
		return 0;
	while (high - low > 1) {
		size_t mid = (low + high) / 2;

		if (ladx_ks_find(ks, data, mid) >= 0)
			low = mid;
		else
			high = mid;
	}
	*at = ladx_ks_find(ks, data, low);
	return low;
}

/* Every run of literals matching KS made copies, greedily left to right, longest first. */
static void ks_atoms(LadxEncoder *enc, LadxAtom *a, size_t n)
{
	size_t i = 0;

	while (i < n) {
		size_t stop = i, len, j = 0;

		if (a[i] >= 0) {
			i++;
			continue;
		}
		while (stop < n && a[stop] < 0)
			stop++;
		len = stop - i;
		for (j = 0; j < len; j++)
			enc->bytes[j] = (uint8_t)(-1 - a[i + j]);
		j = 0;
		while (j < len) {
			int64_t at = 0;
			size_t found = ks_match(enc->ks, enc->bytes + j, len - j, &at), k;

			if (!found) {
				j++;
				continue;
			}
			for (k = 0; k < found; k++)
				a[i + j + k] = (LadxAtom)(at + (int64_t)k);
			j += found;
		}
		i = stop;
	}
}

/* The fragment rule: _fragment_atoms ------------------------------------------------------------- */

static int literal_at(const LadxEncoder *enc, const LadxAtom *a, size_t n, uint32_t q)
{
	size_t k;

	for (k = 0; k < n; k++)
		if ((int)enc->rom[q + k] != -1 - a[k])
			return 0;
	return 1;
}

/* a[start..stop) walked against the source bytes low..high. */
static void fragment_piece(const LadxEncoder *enc, LadxAtom *a, size_t start, size_t stop, uint32_t low,
			   uint32_t high)
{
	size_t j = start;

	while (j < stop) {
		size_t n = 0, k;
		uint32_t pos = low, best = low;

		/* the lowest occurrence of a longer prefix is never below a shorter one's */
		while (j + n < stop && a[j + n] < 0) {
			while (pos + n + 1 <= high && !literal_at(enc, a + j, n + 1, pos))
				pos++;
			if (pos + n + 1 > high)
				break;
			best = pos;
			n++;
		}
		if (!n) {
			j++;
			continue;
		}
		for (k = 0; k < n; k++)
			a[j + k] = (LadxAtom)(best + k);
		j += n;
	}
}

static void fragment_bank(const LadxEncoder *enc, LadxAtom *a, size_t n, uint32_t offset)
{
	size_t i = 0;

	while (i < n) {
		uint32_t low, high, l2, h2;
		size_t stop = i + 1;

		if (!ladx_ks_span_at(enc->ks, (int64_t)offset + i, &low, &high)) {
			i++;
			continue;
		}
		while (stop < n && ladx_ks_span_at(enc->ks, (int64_t)offset + stop, &l2, &h2) && l2 == low && h2 == high)
			stop++;
		fragment_piece(enc, a, i, stop, low, high);
		i = stop;
	}
}

/* Segment ends --------------------------------------------------------------------------------- */

static LadxEncStatus render(LadxEncoder *enc, uint32_t count)
{
	LadxSpan group;
	uint32_t i = 0, k;

	group.at = enc->group;
	group.len = strlen(enc->group);
	while (i < count) {
		uint32_t j = i + 1;

		while (j < count && enc->sorted[j] == enc->sorted[j - 1] + 1)
			j++;
		if (wlog_status(enc, ladx_wlog_bank(enc->log, group, enc->sorted[i], enc->state + enc->sorted[i], j - i)))
			return enc->failed;
		i = j;
	}
	for (k = 0; k < enc->seg_entry_count; k++) {
		const LadxEncEntry *e = &enc->entry[enc->seg_entry[k]];
		LadxWlogStatus st = e->pointer ? ladx_wlog_pointer(enc->log, group, e->table, entry_key(e), e->value)
					       : ladx_wlog_table(enc->log, group, e->table, entry_key(e),
								 enc->pool_atom + e->first, e->count);

		if (wlog_status(enc, st))
			return enc->failed;
	}
	return LADX_ENC_OK;
}

typedef void (*BankRule)(LadxEncoder *enc, LadxAtom *a, size_t n, uint32_t offset);
typedef void (*EntryRule)(LadxEncoder *enc, LadxAtom *a, size_t n, const LadxEncEntry *e);

static void ks_bank(LadxEncoder *enc, LadxAtom *a, size_t n, uint32_t offset)
{
	(void)offset;
	ks_atoms(enc, a, n);
}

static void ks_entry(LadxEncoder *enc, LadxAtom *a, size_t n, const LadxEncEntry *e)
{
	(void)e;
	ks_atoms(enc, a, n);
}

static void frag_bank(LadxEncoder *enc, LadxAtom *a, size_t n, uint32_t offset)
{
	fragment_bank(enc, a, n, offset);
}

static void frag_entry(LadxEncoder *enc, LadxAtom *a, size_t n, const LadxEncEntry *e)
{
	uint32_t low, high;

	if (ladx_ks_entry(enc->ks, e->table, entry_key(e), &low, &high))
		fragment_piece(enc, a, 0, n, low, high);
}

/* One end-of-segment rule on the net literals; the identities a changed byte names follow it. */
static void apply_rule(LadxEncoder *enc, uint32_t count, BankRule bank, EntryRule entry)
{
	uint32_t i = 0, k;

	while (i < count) {
		uint32_t j = i + 1, start = enc->sorted[i];

		while (j < count && enc->sorted[j] == enc->sorted[j - 1] + 1)
			j++;
		memcpy(enc->atoms, enc->state + start, (j - i) * sizeof enc->atoms[0]);
		bank(enc, enc->atoms, j - i, start);
		for (k = 0; k < j - i; k++)
			if (enc->atoms[k] != enc->state[start + k]) {
				enc->state[start + k] = enc->atoms[k];
				rename_to(enc, enc->origin[start + k], enc->atoms[k]);
			}
		i = j;
	}
	for (k = 0; k < enc->seg_entry_count; k++) {
		LadxEncEntry *e = &enc->entry[enc->seg_entry[k]];
		LadxAtom *atoms = enc->pool_atom + e->first;
		uint32_t m;

		if (e->pointer)
			continue;
		memcpy(enc->atoms, atoms, e->count * sizeof atoms[0]);
		entry(enc, enc->atoms, e->count, e);
		if (memcmp(enc->atoms, atoms, e->count * sizeof atoms[0]) == 0)
			continue;
		memcpy(atoms, enc->atoms, e->count * sizeof atoms[0]);
		for (m = 0; m < e->count; m++)
			rename_to(enc, enc->pool_origin[e->first + m], atoms[m]);
	}
}

static LadxEncStatus close_c(LadxEncoder *enc)
{
	uint32_t count = take_touched(enc);

	if (enc->ks) {
		apply_rule(enc, count, ks_bank, ks_entry);
		apply_rule(enc, count, frag_bank, frag_entry);
	}
	return render(enc, count);
}

static LadxEncStatus close_table(LadxEncoder *enc)
{
	uint32_t count = take_touched(enc), i, k;

	for (i = 0; i < count; i++) {
		uint32_t p = enc->sorted[i];

		name(enc, enc->origin[p], enc->state[p]);
	}
	for (k = 0; k < enc->seg_entry_count; k++) {
		LadxEncEntry *e = &enc->entry[enc->seg_entry[k]];

		if (e->pointer)
			continue;
		for (i = 0; i < e->count; i++)
			name(enc, enc->pool_origin[e->first + i], enc->pool_atom[e->first + i]);
	}
	return render(enc, count);
}

static LadxEncStatus close_segment(LadxEncoder *enc)
{
	LadxEncStatus st = LADX_ENC_OK;

	if (enc->open == SIDE_C)
		st = close_c(enc);
	else if (enc->open == SIDE_TABLE)
		st = close_table(enc);
	enc->open = 0;
	enc->group[0] = '\0';
	enc->seg_entry_count = 0;
	return st;
}

static LadxEncStatus open_segment(LadxEncoder *enc, LadxSpan group, int side)
{
	LadxEncStatus st;

	if (group.len == 0 || group.len > LADX_WLOG_NAME_MAX || memchr(group.at, ' ', group.len) ||
	    memchr(group.at, '\n', group.len) || memchr(group.at, '\0', group.len))
		return LADX_ENC_BAD_INPUT;
	if (enc->open && strlen(enc->group) == group.len && memcmp(enc->group, group.at, group.len) == 0)
		return enc->open == side ? LADX_ENC_OK : LADX_ENC_BAD_INPUT;
	if ((st = close_segment(enc)) != LADX_ENC_OK)
		return st;
	memcpy(enc->group, group.at, group.len);
	enc->group[group.len] = '\0';
	enc->open = side;
	enc->segment++;
	return LADX_ENC_OK;
}

LadxEncStatus ladx_enc_end(LadxEncoder *enc)
{
	LadxEncStatus st;

	if (enc->failed)
		return enc->failed;
	if ((st = close_segment(enc)) != LADX_ENC_OK)
		return st;
	return wlog_status(enc, ladx_wlog_end(enc->log));
}

/* C groups' writes ------------------------------------------------------------------------------ */

static int bank_ok(const LadxEncoder *enc, uint32_t offset, size_t n)
{
	return n > 0 && (uint64_t)offset + n <= enc->positions && n <= LADX_WLOG_BANK_MAX;
}

LadxEncStatus ladx_enc_bank(LadxEncoder *enc, LadxSpan group, uint32_t offset, const uint8_t *values,
			    const int32_t *origins, size_t n)
{
	LadxEncStatus st;
	size_t i;

	if (enc->failed)
		return enc->failed;
	if (!bank_ok(enc, offset, n) || check_origins(origins, n))
		return LADX_ENC_BAD_INPUT;
	if ((st = open_segment(enc, group, SIDE_C)) != LADX_ENC_OK)
		return st;
	for (i = 0; i < n; i++) {
		uint32_t p = offset + (uint32_t)i;
		LadxAtom a = carried(enc, origins[i]);

		see(enc, origins[i]);
		if (a == UNSET) {
			LadxAtom replaced = enc->state[p];

			a = knowable(enc, replaced) && atom_value(enc, replaced) == values[i] ? replaced
											     : LADX_ATOM_LITERAL(values[i]);
		}
		name(enc, origins[i], a);
		enc->state[p] = a;
		enc->origin[p] = origins[i];
		touch(enc, p);
	}
	return LADX_ENC_OK;
}

static int same(const LadxEncoder *enc, const uint8_t *nv, size_t i, const LadxAtom *old, size_t j)
{
	return knowable(enc, old[j]) && nv[i] == atom_value(enc, old[j]);
}

/* atoms[start..stop): the longest knowable run of old, at least MIN_MATCH, at its lowest index, left to right. */
static void inherit_middle(const LadxEncoder *enc, const uint8_t *nv, const LadxAtom *old, size_t m, LadxAtom *atoms,
			   size_t start, size_t stop)
{
	size_t i = start;

	while (i < stop) {
		size_t best = 0, at = 0, j;

		for (j = 0; j < m; j++) {
			size_t k = 0;

			while (i + k < stop && j + k < m && atoms[i + k] == UNSET && same(enc, nv, i + k, old, j + k))
				k++;
			if (k > best) {
				best = k;
				at = j;
			}
		}
		if (best >= MIN_MATCH) {
			memcpy(atoms + i, old + at, best * sizeof atoms[0]);
			i += best;
		} else {
			i++;
		}
	}
}

/* write_log's inherited: carried origins, then the common prefix and suffix, then the middle. */
static void inherited(const LadxEncoder *enc, const uint8_t *nv, const int32_t *origins, size_t n, const LadxAtom *old,
		      size_t m, LadxAtom *atoms)
{
	size_t prefix = 0, suffix = 0, low = n < m ? n : m, i;

	while (prefix < low && same(enc, nv, prefix, old, prefix))
		prefix++;
	while (suffix < low - prefix && same(enc, nv, n - 1 - suffix, old, m - 1 - suffix))
		suffix++;
	for (i = 0; i < n; i++) {
		LadxAtom a = carried(enc, origins[i]);

		if (a == UNSET && i < prefix)
			a = old[i];
		else if (a == UNSET && i >= n - suffix)
			a = old[m - (n - i)];
		atoms[i] = a;
	}
	inherit_middle(enc, nv, old, m, atoms, prefix, n - suffix);
	for (i = 0; i < n; i++)
		if (atoms[i] == UNSET)
			atoms[i] = LADX_ATOM_LITERAL(nv[i]);
}

LadxEncStatus ladx_enc_table(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, const uint8_t *values,
			     const int32_t *origins, size_t n)
{
	LadxEncEntry *e;
	LadxEncStatus st;
	const LadxAtom *old = NULL;
	size_t m = 0, i;

	if (enc->failed)
		return enc->failed;
	if (!key_ok(enc, table, key) || n > LADX_ENC_VALUE_MAX || check_origins(origins, n))
		return LADX_ENC_BAD_INPUT;
	if ((st = open_segment(enc, group, SIDE_C)) != LADX_ENC_OK)
		return st;
	if (!pool_room(enc, n) || !(e = get_entry(enc, table, key)) || enc->seg_entry_count == LADX_ENC_ENTRY_MAX ||
	    !own_identities(enc, origins, enc->own, n))
		return fail(enc, LADX_ENC_OVERFLOW);
	origins = enc->own;
	if (!e->pointer) {
		old = enc->pool_atom + e->first;
		m = e->count;
	}
	for (i = 0; i < n; i++)
		see(enc, origins[i]);
	inherited(enc, values, origins, n, old, m, enc->atoms);
	for (i = 0; i < n; i++)
		name(enc, origins[i], enc->atoms[i]);
	set_value(enc, e, enc->atoms, origins, n);
	touch_entry(enc, e);
	return LADX_ENC_OK;
}

static LadxEncStatus put_pointer(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, uint32_t value,
				 int side)
{
	LadxEncEntry *e;
	LadxEncStatus st;

	if (enc->failed)
		return enc->failed;
	if (!key_ok(enc, table, key))
		return LADX_ENC_BAD_INPUT;
	if ((st = open_segment(enc, group, side)) != LADX_ENC_OK)
		return st;
	if (!(e = get_entry(enc, table, key)) || enc->seg_entry_count == LADX_ENC_ENTRY_MAX)
		return fail(enc, LADX_ENC_OVERFLOW);
	e->pointer = 1;
	e->value = value;
	e->count = 0;
	touch_entry(enc, e);
	return LADX_ENC_OK;
}

LadxEncStatus ladx_enc_pointer(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, uint32_t value)
{
	return put_pointer(enc, group, table, key, value, SIDE_C);
}

/* Table groups' writes -------------------------------------------------------------------------- */

static int atoms_ok(const LadxAtom *atoms, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (atoms[i] < LADX_ATOM_LITERAL(255))
			return 0;
	return 1;
}

LadxEncStatus ladx_enc_fixed_bank(LadxEncoder *enc, LadxSpan group, uint32_t offset, const LadxAtom *atoms,
				  const int32_t *origins, size_t n)
{
	LadxEncStatus st;
	size_t i;

	if (enc->failed)
		return enc->failed;
	if (!bank_ok(enc, offset, n) || !atoms_ok(atoms, n) || (origins && check_origins(origins, n)))
		return LADX_ENC_BAD_INPUT;
	if (!origins && (uint64_t)enc->next_identity + n > LADX_ENC_IDENTITY_MAX)
		return fail(enc, LADX_ENC_OVERFLOW);
	if ((st = open_segment(enc, group, SIDE_TABLE)) != LADX_ENC_OK)
		return st;
	for (i = 0; i < n; i++) {
		uint32_t p = offset + (uint32_t)i;

		if (origins)
			see(enc, origins[i]);
		enc->state[p] = atoms[i];
		enc->origin[p] = origins ? origins[i] : ladx_enc_fresh(enc, 1);
		touch(enc, p);
	}
	return LADX_ENC_OK;
}

LadxEncStatus ladx_enc_fixed_table(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key,
				   const LadxAtom *atoms, const int32_t *origins, size_t n)
{
	LadxEncEntry *e;
	LadxEncStatus st;
	size_t i;

	if (enc->failed)
		return enc->failed;
	if (!key_ok(enc, table, key) || n > LADX_ENC_VALUE_MAX || !atoms_ok(atoms, n) ||
	    (origins && check_origins(origins, n)))
		return LADX_ENC_BAD_INPUT;
	if ((st = open_segment(enc, group, SIDE_TABLE)) != LADX_ENC_OK)
		return st;
	if (!pool_room(enc, n) || !(e = get_entry(enc, table, key)) || enc->seg_entry_count == LADX_ENC_ENTRY_MAX ||
	    (!origins && (uint64_t)enc->next_identity + n > LADX_ENC_IDENTITY_MAX))
		return fail(enc, LADX_ENC_OVERFLOW);
	for (i = 0; i < n; i++) {
		if (origins)
			see(enc, origins[i]);
		enc->bytes_origin[i] = origins ? origins[i] : ladx_enc_fresh(enc, 1);
	}
	set_value(enc, e, atoms, enc->bytes_origin, n);
	touch_entry(enc, e);
	return LADX_ENC_OK;
}

LadxEncStatus ladx_enc_fixed_pointer(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key,
				     uint32_t value)
{
	return put_pointer(enc, group, table, key, value, SIDE_TABLE);
}
