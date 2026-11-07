#ifndef LADX_WLOG_H
#define LADX_WLOG_H

#include <stddef.h>
#include <stdint.h>

/*
 * The write log rom_writes.txt, version 1 (tests/ladx/README.md), built in
 * a caller-owned struct with no allocation: each segment's net effect is
 * held until the segment closes, then rendered canonically, as
 * write_log.render renders the same net maps. No I lines. Also parsed
 * from text, every rule of write_log.parse, and projected onto one group.
 */

/* Net bank bytes, table entries and table atoms of one open segment. */
#define LADX_WLOG_BANK_MAX 262144
#define LADX_WLOG_ENTRY_MAX 4096
#define LADX_WLOG_ATOM_MAX 65536
#define LADX_WLOG_TABLES_MAX 16
/* A group, table or alt key name, without its NUL. */
#define LADX_WLOG_NAME_MAX 31

/* A byte's encoding: q >= 0 the source byte at offset q, -1 - b the literal b. */
typedef int32_t LadxAtom;

#define LADX_ATOM_LITERAL(b) ((LadxAtom)(-1 - (int32_t)(b)))

/* Text the caller owns; not NUL-terminated. */
typedef struct LadxSpan {
	const char *at;
	size_t len;
} LadxSpan;

/* A table entry's key: alt.len 0 for the index, else the alt key's name. */
typedef struct LadxWlogKey {
	uint32_t index;
	LadxSpan alt;
} LadxWlogKey;

/* Receives the log's text in order, in pieces of any length; lines end with '\n'. */
typedef void (*LadxWlogSink)(void *ctx, const char *text, size_t len);

typedef enum LadxWlogStatus {
	LADX_WLOG_OK = 0,
	LADX_WLOG_BAD_INPUT, /* refused, the log unchanged: a bad name, atom, table or empty bank record */
	LADX_WLOG_OVERFLOW,  /* a capacity exceeded: every later call returns it */
	LADX_WLOG_MALFORMED  /* parsing: not a version-1 log */
} LadxWlogStatus;

typedef struct LadxWlogBank {
	uint32_t offset;
	LadxAtom atom;
} LadxWlogBank;

typedef struct LadxWlogEntry {
	uint8_t table;
	uint8_t pointer; /* value is the pointer, else first and count index the atoms */
	uint8_t alt_len;
	char alt[LADX_WLOG_NAME_MAX];
	uint32_t index;
	uint32_t value, first, count;
} LadxWlogEntry;

/* Read nothing of it: its fields belong to ladx_wlog.c. */
typedef struct LadxWlog {
	LadxWlogSink sink;
	void *ctx;
	LadxWlogStatus failed;
	char tables[LADX_WLOG_TABLES_MAX][LADX_WLOG_NAME_MAX + 1];
	int table_count;
	char group[LADX_WLOG_NAME_MAX + 1]; /* "" when no segment is open */
	LadxWlogBank bank[LADX_WLOG_BANK_MAX];
	int bank_count; /* sorted by offset, one per offset */
	LadxWlogEntry entry[LADX_WLOG_ENTRY_MAX];
	int entry_count; /* sorted in render order */
	LadxAtom atom[LADX_WLOG_ATOM_MAX];
	int atom_count;
	LadxAtom scratch[LADX_WLOG_ATOM_MAX];
} LadxWlog;

/*
 * Starts the log: writes the version line and `tables <tables>`, tables
 * the space-separated table names in ROMWithTables' order. BAD_INPUT for
 * no name, a repeated or malformed one, or too many.
 */
LadxWlogStatus ladx_wlog_begin(LadxWlog *log, LadxSpan tables, LadxWlogSink sink, void *ctx);

/*
 * One record of group: n >= 1 bank bytes from offset, each the last write
 * at its offset. A group other than the open segment's closes it (it is
 * rendered) and opens the group's.
 */
LadxWlogStatus ladx_wlog_bank(LadxWlog *log, LadxSpan group, uint32_t offset, const LadxAtom *atoms, size_t n);

/* One table entry's whole new value (n may be 0), table an index into the tables line. */
LadxWlogStatus ladx_wlog_table(LadxWlog *log, LadxSpan group, int table, LadxWlogKey key, const LadxAtom *atoms,
			       size_t n);

/* An integer-pointer entry. */
LadxWlogStatus ladx_wlog_pointer(LadxWlog *log, LadxSpan group, int table, LadxWlogKey key, uint32_t value);

/* Closes the open segment; the log is then complete. */
LadxWlogStatus ladx_wlog_end(LadxWlog *log);

/*
 * Parsing. *line gets the 1-based line at fault on MALFORMED, 0 for the
 * whole text (no final newline), and 0 on success. Names are at most
 * LADX_WLOG_NAME_MAX characters and the tables line names at least one
 * table, at most LADX_WLOG_TABLES_MAX; I lines are checked, then ignored.
 */

/*
 * write_log.py project: group's segments replayed in order into log,
 * rendered to sink as one segment with the header lines (the header alone
 * when group writes nothing). The whole text is checked first: on
 * MALFORMED nothing reaches sink. OVERFLOW when the projection exceeds
 * the log's capacities (a table entry over LADX_WLOG_ATOM_MAX atoms).
 */
LadxWlogStatus ladx_wlog_project(LadxWlog *log, LadxSpan text, LadxSpan group, LadxWlogSink sink, void *ctx,
				 int *line);

/*
 * write_log.py groups: the log's groups, each once, in first-write order,
 * as spans into text; OVERFLOW when there are more than cap.
 */
LadxWlogStatus ladx_wlog_groups(LadxSpan text, LadxSpan *groups, int cap, int *count, int *line);

#endif
