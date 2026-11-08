#ifndef LADX_ENCODER_H
#define LADX_ENCODER_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_ks.h"
#include "ladx_rom.h"
#include "ladx_wlog.h"

#define LADX_ENC_KNOWN (-1)
#define LADX_ENC_IDENTITY(i) ((int32_t)(-2 - (int32_t)(i)))

#define LADX_ENC_IDENTITY_MAX 262144
#define LADX_ENC_ENTRY_MAX 8192
#define LADX_ENC_POOL_MAX 524288
#define LADX_ENC_VALUE_MAX LADX_WLOG_ATOM_MAX

typedef enum LadxEncStatus {
	LADX_ENC_OK = 0,
	LADX_ENC_BAD_INPUT,
	LADX_ENC_OVERFLOW
} LadxEncStatus;

typedef struct LadxEncEntry {
	uint8_t table, pointer, alt_len;
	char alt[LADX_WLOG_NAME_MAX + 1];
	uint32_t index, value;
	uint32_t first, count;
	uint32_t segment;
} LadxEncEntry;

typedef struct LadxEncoder {
	const uint8_t *rom;
	size_t rom_len;
	const LadxKs *ks;
	LadxWlog *log;
	uint32_t positions;
	LadxEncStatus failed;
	int open;
	char group[LADX_WLOG_NAME_MAX + 1];
	uint32_t segment;
	LadxAtom state[LADX_ROM_SIZE];
	int32_t origin[LADX_ROM_SIZE];
	uint8_t touched[LADX_ROM_SIZE / 8];
	uint32_t touched_count, touched_low, touched_high;
	LadxAtom written[LADX_ENC_IDENTITY_MAX];
	uint32_t next_identity;
	uint32_t auto_low;
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
	int32_t own[LADX_ENC_VALUE_MAX];
} LadxEncoder;

LadxEncStatus ladx_enc_init(LadxEncoder *enc, const uint8_t *rom, size_t rom_len, const LadxKs *ks,
			    const LadxRomTables *tables, LadxWlog *log);

LadxEncStatus ladx_enc_initial(LadxEncoder *enc, int table, LadxWlogKey key, const uint8_t *values,
			       const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_initial_pointer(LadxEncoder *enc, int table, LadxWlogKey key, uint32_t value);

LadxEncStatus ladx_enc_bank(LadxEncoder *enc, LadxSpan group, uint32_t offset, const uint8_t *values,
			    const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_table(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, const uint8_t *values,
			     const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_pointer(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, uint32_t value);

LadxEncStatus ladx_enc_fixed_bank(LadxEncoder *enc, LadxSpan group, uint32_t offset, const LadxAtom *atoms,
				  const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_fixed_table(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key,
				   const LadxAtom *atoms, const int32_t *origins, size_t n);
LadxEncStatus ladx_enc_fixed_pointer(LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key,
				     uint32_t value);

LadxEncStatus ladx_enc_end(LadxEncoder *enc);

int32_t ladx_enc_bank_origin(const LadxEncoder *enc, uint32_t offset);

int ladx_enc_bank_value(const LadxEncoder *enc, uint32_t offset, uint8_t *value, int32_t *origin);

int ladx_enc_entry_origins(const LadxEncoder *enc, int table, LadxWlogKey key, int32_t *out, size_t cap,
			   size_t *n);

int ladx_enc_entry_value(const LadxEncoder *enc, int table, LadxWlogKey key, uint8_t *values, int32_t *origins,
			 size_t cap, size_t *n);

int ladx_enc_entry_pointer(const LadxEncoder *enc, int table, LadxWlogKey key, uint32_t *value);

int32_t ladx_enc_fresh(LadxEncoder *enc, uint32_t n);

#endif
