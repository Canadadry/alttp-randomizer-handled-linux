#ifndef LADX_STORE_H
#define LADX_STORE_H

#include <stddef.h>
#include <stdint.h>

/*
 * LADXR's PointerTable storage and store (pointerTable.py at the pin)
 * without a ROM or an encoder (tests/ladx/README.md, "Store"): regions
 * from parsed spans, addStorage, and store's packing through caller
 * callbacks. Offsets are in-bank (0..0x4000).
 */

#define LADX_STORE_REGION_MAX 1024
#define LADX_STORE_ALT_MAX 5
#define LADX_STORE_ENTRY_MAX 1024 /* alt and index keys of one table: LADXR's largest has 0x325 */

typedef struct LadxStoreRegion {
	uint8_t bank;
	int32_t start, end;
} LadxStoreRegion;

typedef struct LadxStoreRegions {
	LadxStoreRegion region[LADX_STORE_REGION_MAX];
	int count;
} LadxStoreRegions;

typedef struct LadxStoreInfo {
	uint32_t count; /* index keys */
	uint8_t pointers_bank;
	uint32_t pointers_addr;
	int data_bank;		/* -1: each entry's bank from a banks table */
	int has_data_addr;	/* the sprite data tables */
	int32_t data_addr;	/* in-bank, moved by adjustDataStart */
	uint32_t data_size;	/* 0: variable size (suffix de-duplication) */
	int alt_count;		/* in dict order */
	uint8_t alt_bank[LADX_STORE_ALT_MAX];
	uint32_t alt_addr[LADX_STORE_ALT_MAX];
	int claim_gaps;		/* claim_storage_gaps */
	int expand;		/* expand_to_end_of_bank */
} LadxStoreInfo;

/* A parsed entry's span, in parse order (alt keys first); integer pointers have none. */
typedef struct LadxStoreSpan {
	uint8_t bank;
	int32_t start, end;
} LadxStoreSpan;

/* An entry's value at store time: alt keys first, then index keys. */
typedef struct LadxStoreEntry {
	int integer;	      /* 1: value is an integer pointer */
	uint32_t value;
	uint8_t bank;	      /* its parse-time bank */
	const uint8_t *bytes; /* len bytes, caller-owned */
	uint32_t len;
} LadxStoreEntry;

typedef enum LadxStoreStatus {
	LADX_STORE_OK = 0,
	LADX_STORE_NO_ROOM,    /* AssertionError "Not enough room in storage..." */
	LADX_STORE_READBACK,   /* AssertionError, empty message: a de-duplicated pointer's bytes differ */
	LADX_STORE_MISALIGNED, /* AssertionError, empty message: a sprite index off data_size */
	LADX_STORE_BYTE_RANGE, /* ValueError "byte must be in range(0, 256)": a sprite index byte */
	LADX_STORE_FAILED      /* a callback returned 0, or over LADX_STORE_REGION_MAX regions or LADX_STORE_ENTRY_MAX entries */
} LadxStoreStatus;

/*
 * The parse's regions: _addStorage of each span in order, merged, sorted
 * stably by start, then claim_storage_gaps and expand_to_end_of_bank.
 * FAILED over LADX_STORE_REGION_MAX.
 */
LadxStoreStatus ladx_store_regions(const LadxStoreInfo *info, const LadxStoreSpan *spans, size_t n,
				   LadxStoreRegions *out);

/* addStorage: extra's regions added to r in order, merged, sorted stably by start. */
LadxStoreStatus ladx_store_add_storage(LadxStoreRegions *r, const LadxStoreRegions *extra);

/* A write of n bytes at bank:addr; entry: the entry's position in entries, -1 for pointer bytes. 0 fails. */
typedef int (*LadxStoreWrite)(void *ctx, uint8_t bank, uint32_t addr, const uint8_t *bytes, size_t n, int entry);
/* The byte now at bank:addr (addr < 0x4000) into *out; 0 fails. */
typedef int (*LadxStoreRead)(void *ctx, uint8_t bank, uint32_t addr, uint8_t *out);

typedef struct LadxStoreError {
	LadxStoreStatus status;
	int alt;	   /* NO_ROOM: an alt entry's */
	uint32_t entry;	   /* the entry's position in entries */
	uint8_t bank;
	LadxStoreRegions storage; /* NO_ROOM: storage as the assert saw it */
} LadxStoreError;

/* The entries stored so far in one store pass, the done dict's registrations: caller-owned scratch. */
typedef struct LadxStoreDone {
	uint32_t entry[LADX_STORE_ENTRY_MAX];
	int32_t pointer[LADX_STORE_ENTRY_MAX];
	uint32_t count;
} LadxStoreDone;

/*
 * store(rom): entries holds info->alt_count alt entries then info->count
 * index entries; regions is copied, left gets the storage left (store's
 * return value). Writes in LADXR's order: alt data then its two pointer
 * bytes, then per index entry its data (unless de-duplicated) and pointer.
 */
LadxStoreStatus ladx_store_table(const LadxStoreInfo *info, const LadxStoreRegions *regions,
				 const LadxStoreEntry *entries, LadxStoreWrite write, LadxStoreRead read, void *ctx,
				 LadxStoreDone *done, LadxStoreRegions *left, LadxStoreError *err);

/*
 * LADXR's exception message for err (store's assert text, Python's repr
 * of the storage list for NO_ROOM) into buf, NUL-terminated and cut at
 * cap; returns the full length. type receives the exception's name.
 */
size_t ladx_store_message(const LadxStoreInfo *info, const LadxStoreError *err, const char **type, char *buf,
			  size_t cap);

#endif
