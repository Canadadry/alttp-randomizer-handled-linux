#ifndef LADX_ENCODER_H
#define LADX_ENCODER_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_ks.h"
#include "ladx_rom.h"
#include "ladx_wlog.h"

/*
 * The write-log encoder (tests/ladx/README.md, "Encoding rule"), as
 * write_log.py's encoder runs: writes with their per-byte origins in,
 * rom_writes.txt out through a caller-begun LadxWlog, without I lines.
 * A segment is a maximal run of one group's calls. C groups' writes are
 * encoded event by event (carried origin, inherit-on-equal, literal), then
 * at the segment's end KS-match and the fragment rule on its net literals;
 * table groups' atoms are taken as given. Caller-owned, no allocation.
 *
 * Origins, per byte: q >= 0 an unchanged copy of source byte q;
 * LADX_ENC_KNOWN no carried origin; LADX_ENC_IDENTITY(i) the computed byte
 * of write identity i, whose first write names its encoding.
 */
#define LADX_ENC_KNOWN (-1)
#define LADX_ENC_IDENTITY(i) ((int32_t)(-2 - (int32_t)(i)))

#define LADX_ENC_IDENTITY_MAX 262144
#define LADX_ENC_ENTRY_MAX 8192
#define LADX_ENC_POOL_MAX 524288 /* entry atoms, every value an entry ever held */
#define LADX_ENC_VALUE_MAX LADX_WLOG_ATOM_MAX /* one entry's bytes */

typedef enum LadxEncStatus {
	LADX_ENC_OK = 0,
	LADX_ENC_BAD_INPUT, /* refused, nothing changed: a bad name, table, offset or origin, a mixed segment */
	LADX_ENC_OVERFLOW   /* a capacity exceeded, or the log failed: every later call returns it */
} LadxEncStatus;

typedef struct LadxEncEntry {
	uint8_t table, pointer, alt_len;
	char alt[LADX_WLOG_NAME_MAX + 1];
	uint32_t index, value; /* value: the integer pointer */
	uint32_t first, count; /* its atoms and origins in the pool */
	uint32_t segment;      /* the last segment that wrote it */
} LadxEncEntry;

/* Read nothing of it: its fields belong to encoder.c. */
typedef struct LadxEncoder {
	const uint8_t *rom;
	size_t rom_len;
	const LadxKs *ks;
	LadxWlog *log;
	uint32_t positions;
	LadxEncStatus failed;
	int open; /* 0: no segment, 1: a C group's, 2: a table group's */
	char group[LADX_WLOG_NAME_MAX + 1];
	uint32_t segment;
	LadxAtom state[LADX_ROM_SIZE];
	int32_t origin[LADX_ROM_SIZE];
	uint8_t touched[LADX_ROM_SIZE / 8];
	uint32_t touched_count, touched_low, touched_high;
	LadxAtom written[LADX_ENC_IDENTITY_MAX];
	uint32_t next_identity;
	LadxEncEntry entry[LADX_ENC_ENTRY_MAX];
	uint32_t entry_count;
	int32_t slot[2 * LADX_ENC_ENTRY_MAX];
	uint32_t seg_entry[LADX_ENC_ENTRY_MAX];
	uint32_t seg_entry_count;
	LadxAtom pool_atom[LADX_ENC_POOL_MAX];
	int32_t pool_origin[LADX_ENC_POOL_MAX];
	uint32_t pool_count;
	uint32_t sorted[LADX_WLOG_BANK_MAX];
	LadxAtom atoms[LADX_WLOG_BANK_MAX];
	uint8_t bytes[LADX_WLOG_BANK_MAX];
	int32_t bytes_origin[LADX_ENC_VALUE_MAX];
} LadxEncoder;

/*
 * rom (NULL for none) and ks (NULL for none, else built over rom) and log
 * (begun with ROMWithTables' tables) outlive enc. With tables, every
 * parsed entry is an initial entry (its source span's offsets, or its
 * integer pointer). Without rom, bank offsets range over LADX_ROM_SIZE.
 */
LadxEncStatus ladx_enc_init(LadxEncoder *enc, const uint8_t *rom, size_t rom_len, const LadxKs *ks,
			    const LadxRomTables *tables, LadxWlog *log);

/* An entry's value before any write: each byte its origin when >= 0, else a literal. */
LadxEncStatus ladx_enc_initial(LadxEncoder *enc, int table, LadxWlogKey key, const uint8_t *values,
			       const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_initial_pointer(LadxEncoder *enc, int table, LadxWlogKey key, uint32_t value);

/* A C group's writes: n bytes from offset, an entry's whole new value, an integer pointer. */
LadxEncStatus ladx_enc_bank(LadxEncoder *enc, LadxSpan group, uint32_t offset, const uint8_t *values,
			    const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_table(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, const uint8_t *values,
			     const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_pointer(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, uint32_t value);

/*
 * A table group's writes, already encoded (data/ladx/). origins NULL: each
 * byte gets a fresh write identity, so a later carried copy can name it.
 * At the segment's end the net bytes' identities are named, bank bytes by
 * offset, then entries in first-write order.
 */
LadxEncStatus ladx_enc_fixed_bank(LadxEncoder *enc, LadxSpan group, uint32_t offset, const LadxAtom *atoms,
				  const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_fixed_table(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key,
				   const LadxAtom *atoms, const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_fixed_pointer(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key,
				     uint32_t value);

/* Closes the last segment and ends the log. */
LadxEncStatus ladx_enc_end(LadxEncoder *enc);

/* The carried origin of the byte now at offset: offset itself while unwritten. */
int32_t ladx_enc_bank_origin(const LadxEncoder *enc, uint32_t offset);

/*
 * The origins of the entry's current bytes copied into out: 1 with *n set,
 * 0 when the entry is unknown, an integer pointer, or over cap bytes.
 */
int ladx_enc_entry_origins(const LadxEncoder *enc, int table, LadxWlogKey key, int32_t *out, size_t cap,
			   size_t *n);

/* n new write identities, the first one's origin returned; LADX_ENC_KNOWN on overflow. */
int32_t ladx_enc_fresh(LadxEncoder *enc, uint32_t n);

#endif
