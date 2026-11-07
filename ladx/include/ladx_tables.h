#ifndef LADX_TABLES_H
#define LADX_TABLES_H

#include "ladx_logic.h"
#include "ladx_settings.h"
#include "ladx_wlog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The patch tables data/ladx/ (tests/ladx/README.md, "Patch tables"),
 * indexed from text the caller owns and keeps alive, with no allocation:
 * the key of a unit from validated settings, the walk of order.txt, an
 * entry's records decoded into atoms, and the bank applier.
 */

#define LADX_TABLES_GROUP_MAX 16
#define LADX_TABLES_UNIT_MAX 256
#define LADX_TABLES_ROW_MAX 512
#define LADX_TABLES_ENTRY_MAX 2048
#define LADX_TABLES_COMPONENT_MAX 12
/* A unit id or a key's rendered values, without its NUL. */
#define LADX_TABLES_UNIT_ID_MAX 47
#define LADX_TABLES_VALUES_MAX 255

typedef enum LadxTablesStatus {
	LADX_TABLES_OK = 0,
	LADX_TABLES_BAD_VERSION,   /* not the file's version line */
	LADX_TABLES_BAD_LINE,	   /* a line the format does not allow there */
	LADX_TABLES_BAD_COMPONENT, /* not a setting key, a 0/1 asm: constant or count */
	LADX_TABLES_BAD_VALUES,	   /* an E line without one value per component */
	LADX_TABLES_BAD_TOKEN,	   /* a B or T record that is not write-log v1 */
	LADX_TABLES_TABLES_DIFFER, /* group files with different tables lines */
	LADX_TABLES_DUP_GROUP,	   /* two files of one group */
	LADX_TABLES_DUP_UNIT,	   /* a unit id twice in its group file */
	LADX_TABLES_NO_GROUP_FILE, /* a table row whose group has no file */
	LADX_TABLES_NO_UNIT,	   /* a table row whose unit is not in its group file */
	LADX_TABLES_OVERFLOW,	   /* a capacity of this header exceeded */
	LADX_TABLES_NO_ENTRY,	   /* no entry for the key in a unit */
	LADX_TABLES_BUFFER,	   /* a record's payload longer than the caller's buffer */
	LADX_TABLES_RANGE,	   /* an offset or reference beyond the source or output ROM */
	LADX_TABLES_STATUS_COUNT
} LadxTablesStatus;

/* A stable name ("no-entry"). */
const char *ladx_tables_status_name(LadxTablesStatus status);

/*
 * file: the name of the file at fault (the caller's), NULL when none;
 * line 1-based, 0 when none. NO_ENTRY: unit is the unit's id and values
 * the key's values as an E line would write them.
 */
typedef struct LadxTablesError {
	LadxTablesStatus status;
	const char *file;
	int line;
	char unit[LADX_TABLES_UNIT_ID_MAX + 1];
	char values[LADX_TABLES_VALUES_MAX + 1];
} LadxTablesError;

/* name: for errors only, outliving the tables. */
typedef struct LadxTablesFile {
	const char *name;
	LadxSpan text;
} LadxTablesFile;

typedef struct LadxTablesGroup {
	LadxSpan name;
	const char *file;
	int first_unit, unit_count;
} LadxTablesGroup;

typedef struct LadxTablesUnit {
	LadxSpan id;
	int group, line;
	LadxSpan component[LADX_TABLES_COMPONENT_MAX];
	int component_count;
	int first_entry, entry_count;
} LadxTablesUnit;

typedef struct LadxTablesEntry {
	LadxSpan values;
	LadxSpan records; /* its record lines with their newlines; empty for nothing */
	int line;	  /* the E line */
} LadxTablesEntry;

typedef struct LadxTablesRow {
	bool c_row;
	int group; /* the group file's index; -1 for a C row's group without one */
	int unit;  /* -1 for a C row */
	LadxSpan group_name, call;
} LadxTablesRow;

/* Read through the functions below; its fields belong to tables.c. */
typedef struct LadxTables {
	const char *order_file;
	LadxSpan tables; /* the tables line's names */
	LadxTablesGroup group[LADX_TABLES_GROUP_MAX];
	LadxTablesUnit unit[LADX_TABLES_UNIT_MAX];
	LadxTablesEntry entry[LADX_TABLES_ENTRY_MAX];
	LadxTablesRow row[LADX_TABLES_ROW_MAX];
	int group_count, unit_count, entry_count, row_count;
} LadxTables;

/*
 * Indexes order.txt and the group files. Any error leaves t unusable and
 * fills err (status, file, line).
 */
LadxTablesStatus ladx_tables_open(LadxTables *t, const LadxTablesFile *order, const LadxTablesFile *groups,
				  int group_count, LadxTablesError *err);

int ladx_tables_group_count(const LadxTables *t);
int ladx_tables_unit_count(const LadxTables *t);
int ladx_tables_entry_count(const LadxTables *t);
/* The tables line's names, space-separated, as ladx_wlog_begin takes them. */
LadxSpan ladx_tables_names(const LadxTables *t);
/* A group file's index by its group's name, -1 when none. */
int ladx_tables_find_group(const LadxTables *t, LadxSpan name);
/* The group's unit count, *first its first unit. */
int ladx_tables_group_units(const LadxTables *t, int group, int *first);
/* The unit's id; whether a component is count. */
LadxSpan ladx_tables_unit_id(const LadxTables *t, int unit);
bool ladx_tables_unit_uses_count(const LadxTables *t, int unit);

/*
 * A step of the walk. TABLE: the unit's first row, with the entry the key
 * selects; C: a C row, with no entry.
 */
typedef struct LadxTablesStep {
	bool c_row;
	LadxSpan group, call;
	int unit; /* -1 for a C row */
	const LadxTablesEntry *entry;
	const char *file; /* the entry's group file */
} LadxTablesStep;

/*
 * The entry of unit for the key of validated, supported settings and
 * spot_count (the count component, ladx_tables_spot_count). False with
 * NO_ENTRY when the unit has none.
 */
bool ladx_tables_select(const LadxTables *t, int unit, const LadxSettings *validated, int spot_count,
			LadxTablesStep *step, LadxTablesError *err);

typedef struct LadxTablesWalk {
	const LadxTables *t;
	const LadxSettings *settings;
	int spot_count;
	int row;
	bool seen[LADX_TABLES_UNIT_MAX];
} LadxTablesWalk;

/* The walk of order.txt for one key, as tables.select; settings outlive the walk. */
void ladx_tables_walk_init(LadxTablesWalk *w, const LadxTables *t, const LadxSettings *validated, int spot_count);
/* The next step: false at the end (err OK) or on an error (NO_ENTRY). */
bool ladx_tables_walk_next(LadxTablesWalk *w, LadxTablesStep *step, LadxTablesError *err);

/*
 * One record of an entry. B: offset, atoms (keep tokens resolved to the
 * record's own offsets). T: table (an index into the tables line), key,
 * and atoms, or pointer true with value.
 */
typedef struct LadxTablesRecord {
	bool table_record;
	uint32_t offset;
	int table;
	LadxWlogKey key;
	bool pointer;
	uint32_t value;
	LadxAtom *atoms; /* the caller's buffer */
	size_t n;
	int line;
} LadxTablesRecord;

typedef struct LadxTablesRecords {
	const LadxTables *t;
	LadxSpan rest;
	const char *file;
	int line;
} LadxTablesRecords;

void ladx_tables_records_init(LadxTablesRecords *it, const LadxTables *t, const LadxTablesStep *step);
/*
 * The next record, its atoms decoded into buf (cap atoms): false at the
 * end (err OK) or with BUFFER when its payload does not fit.
 */
bool ladx_tables_records_next(LadxTablesRecords *it, LadxTablesRecord *rec, LadxAtom *buf, size_t cap,
			      LadxTablesError *err);

/*
 * Writes a B record onto out from source (references read source, never
 * out); a T record writes nothing. RANGE, nothing written, when an offset
 * is beyond out or a reference beyond source.
 */
LadxTablesStatus ladx_tables_apply(const LadxTablesRecord *rec, const uint8_t *source, size_t source_len, uint8_t *out,
				   size_t out_len);

/* generator.py's addFrameCounter argument: the spots whose class is not exactly KeyLocation. */
int ladx_tables_spot_count(const LadxLogic *logic);

#endif
