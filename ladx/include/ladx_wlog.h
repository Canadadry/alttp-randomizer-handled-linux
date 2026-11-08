#ifndef LADX_WLOG_H
#define LADX_WLOG_H

#include <stddef.h>
#include <stdint.h>

#define LADX_WLOG_BANK_MAX 262144
#define LADX_WLOG_ENTRY_MAX 4096
#define LADX_WLOG_ATOM_MAX 65536
#define LADX_WLOG_TABLES_MAX 16
#define LADX_WLOG_NAME_MAX 31

typedef int32_t LadxAtom;

#define LADX_ATOM_LITERAL(b) ((LadxAtom)(-1 - (int32_t)(b)))

typedef struct LadxSpan {
	const char *at;
	size_t len;
} LadxSpan;

typedef struct LadxWlogKey {
	uint32_t index;
	LadxSpan alt;
} LadxWlogKey;

typedef void (*LadxWlogSink)(void *ctx, const char *text, size_t len);

typedef enum LadxWlogStatus {
	LADX_WLOG_OK = 0,
	LADX_WLOG_BAD_INPUT,
	LADX_WLOG_OVERFLOW,
	LADX_WLOG_MALFORMED
} LadxWlogStatus;

typedef struct LadxWlogBank {
	uint32_t offset;
	LadxAtom atom;
} LadxWlogBank;

typedef struct LadxWlogEntry {
	uint8_t table;
	uint8_t pointer;
	uint8_t alt_len;
	char alt[LADX_WLOG_NAME_MAX];
	uint32_t index;
	uint32_t value, first, count;
} LadxWlogEntry;

typedef struct LadxWlog {
	LadxWlogSink sink;
	void *ctx;
	LadxWlogStatus failed;
	char tables[LADX_WLOG_TABLES_MAX][LADX_WLOG_NAME_MAX + 1];
	int table_count;
	char group[LADX_WLOG_NAME_MAX + 1];
	LadxWlogBank bank[LADX_WLOG_BANK_MAX];
	int bank_count;
	LadxWlogEntry entry[LADX_WLOG_ENTRY_MAX];
	int entry_count;
	LadxAtom atom[LADX_WLOG_ATOM_MAX];
	int atom_count;
	LadxAtom scratch[LADX_WLOG_ATOM_MAX];
} LadxWlog;

LadxWlogStatus ladx_wlog_begin(LadxWlog *log, LadxSpan tables, LadxWlogSink sink, void *ctx);

LadxWlogStatus ladx_wlog_bank(LadxWlog *log, LadxSpan group, uint32_t offset, const LadxAtom *atoms, size_t n);

LadxWlogStatus ladx_wlog_table(LadxWlog *log, LadxSpan group, int table, LadxWlogKey key, const LadxAtom *atoms,
			       size_t n);

LadxWlogStatus ladx_wlog_pointer(LadxWlog *log, LadxSpan group, int table, LadxWlogKey key, uint32_t value);

LadxWlogStatus ladx_wlog_end(LadxWlog *log);

LadxWlogStatus ladx_wlog_project(LadxWlog *log, LadxSpan text, LadxSpan group, LadxWlogSink sink, void *ctx,
				 int *line);

LadxWlogStatus ladx_wlog_groups(LadxSpan text, LadxSpan *groups, int cap, int *count, int *line);

#endif
