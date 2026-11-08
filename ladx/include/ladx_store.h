#ifndef LADX_STORE_H
#define LADX_STORE_H

#include <stddef.h>
#include <stdint.h>

#define LADX_STORE_REGION_MAX 1024
#define LADX_STORE_ALT_MAX 5
#define LADX_STORE_ENTRY_MAX 1024

typedef struct LadxStoreRegion {
	uint8_t bank;
	int32_t start, end;
} LadxStoreRegion;

typedef struct LadxStoreRegions {
	LadxStoreRegion region[LADX_STORE_REGION_MAX];
	int count;
} LadxStoreRegions;

typedef struct LadxStoreInfo {
	uint32_t count;
	uint8_t pointers_bank;
	uint32_t pointers_addr;
	int data_bank;
	int has_data_addr;
	int32_t data_addr;
	uint32_t data_size;
	int alt_count;
	uint8_t alt_bank[LADX_STORE_ALT_MAX];
	uint32_t alt_addr[LADX_STORE_ALT_MAX];
	int claim_gaps;
	int expand;
} LadxStoreInfo;

typedef struct LadxStoreSpan {
	uint8_t bank;
	int32_t start, end;
} LadxStoreSpan;

typedef struct LadxStoreEntry {
	int integer;
	uint32_t value;
	uint8_t bank;
	const uint8_t *bytes;
	uint32_t len;
} LadxStoreEntry;

typedef enum LadxStoreStatus {
	LADX_STORE_OK = 0,
	LADX_STORE_NO_ROOM,
	LADX_STORE_READBACK,
	LADX_STORE_MISALIGNED,
	LADX_STORE_BYTE_RANGE,
	LADX_STORE_FAILED
} LadxStoreStatus;

LadxStoreStatus ladx_store_regions(const LadxStoreInfo *info, const LadxStoreSpan *spans, size_t n,
				   LadxStoreRegions *out);

LadxStoreStatus ladx_store_add_storage(LadxStoreRegions *r, const LadxStoreRegions *extra);

typedef int (*LadxStoreWrite)(void *ctx, uint8_t bank, uint32_t addr, const uint8_t *bytes, size_t n, int entry);
typedef int (*LadxStoreRead)(void *ctx, uint8_t bank, uint32_t addr, uint8_t *out);

typedef struct LadxStoreError {
	LadxStoreStatus status;
	int alt;
	uint32_t entry;
	uint8_t bank;
	LadxStoreRegions storage;
} LadxStoreError;

typedef struct LadxStoreDone {
	uint32_t entry[LADX_STORE_ENTRY_MAX];
	int32_t pointer[LADX_STORE_ENTRY_MAX];
	uint32_t count;
} LadxStoreDone;

LadxStoreStatus ladx_store_table(const LadxStoreInfo *info, const LadxStoreRegions *regions,
				 const LadxStoreEntry *entries, LadxStoreWrite write, LadxStoreRead read, void *ctx,
				 LadxStoreDone *done, LadxStoreRegions *left, LadxStoreError *err);

size_t ladx_store_message(const LadxStoreInfo *info, const LadxStoreError *err, const char **type, char *buf,
			  size_t cap);

#endif
