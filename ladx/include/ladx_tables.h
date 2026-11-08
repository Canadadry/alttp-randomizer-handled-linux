#ifndef LADX_TABLES_H
#define LADX_TABLES_H

#include "ladx_logic.h"
#include "ladx_settings.h"
#include "ladx_wlog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LADX_TABLES_GROUP_MAX 16
#define LADX_TABLES_UNIT_MAX 512
#define LADX_TABLES_ROW_MAX 512
#define LADX_TABLES_ENTRY_MAX 2048
#define LADX_TABLES_COMPONENT_MAX 12
#define LADX_TABLES_UNIT_ID_MAX 47
#define LADX_TABLES_VALUES_MAX 255

typedef enum LadxTablesStatus {
	LADX_TABLES_OK = 0,
	LADX_TABLES_BAD_VERSION,
	LADX_TABLES_BAD_LINE,
	LADX_TABLES_BAD_COMPONENT,
	LADX_TABLES_BAD_VALUES,
	LADX_TABLES_BAD_TOKEN,
	LADX_TABLES_TABLES_DIFFER,
	LADX_TABLES_DUP_GROUP,
	LADX_TABLES_DUP_UNIT,
	LADX_TABLES_NO_GROUP_FILE,
	LADX_TABLES_NO_UNIT,
	LADX_TABLES_OVERFLOW,
	LADX_TABLES_NO_ENTRY,
	LADX_TABLES_BUFFER,
	LADX_TABLES_RANGE,
	LADX_TABLES_STATUS_COUNT
} LadxTablesStatus;

const char *ladx_tables_status_name(LadxTablesStatus status);

typedef struct LadxTablesError {
	LadxTablesStatus status;
	const char *file;
	int line;
	char unit[LADX_TABLES_UNIT_ID_MAX + 1];
	char values[LADX_TABLES_VALUES_MAX + 1];
} LadxTablesError;

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
	LadxSpan records;
	int line;
} LadxTablesEntry;

typedef struct LadxTablesRow {
	bool c_row;
	int group;
	int unit;
	LadxSpan group_name, call;
} LadxTablesRow;

typedef struct LadxTables {
	const char *order_file;
	LadxSpan tables;
	LadxTablesGroup group[LADX_TABLES_GROUP_MAX];
	LadxTablesUnit unit[LADX_TABLES_UNIT_MAX];
	LadxTablesEntry entry[LADX_TABLES_ENTRY_MAX];
	LadxTablesRow row[LADX_TABLES_ROW_MAX];
	int group_count, unit_count, entry_count, row_count;
} LadxTables;

LadxTablesStatus ladx_tables_open(LadxTables *t, const LadxTablesFile *order, const LadxTablesFile *groups,
				  int group_count, LadxTablesError *err);

int ladx_tables_group_count(const LadxTables *t);
int ladx_tables_unit_count(const LadxTables *t);
int ladx_tables_entry_count(const LadxTables *t);
LadxSpan ladx_tables_names(const LadxTables *t);
int ladx_tables_find_group(const LadxTables *t, LadxSpan name);
int ladx_tables_group_units(const LadxTables *t, int group, int *first);
LadxSpan ladx_tables_unit_id(const LadxTables *t, int unit);
bool ladx_tables_unit_uses_count(const LadxTables *t, int unit);

typedef struct LadxTablesStep {
	bool c_row;
	LadxSpan group, call;
	int unit;
	const LadxTablesEntry *entry;
	const char *file;
} LadxTablesStep;

bool ladx_tables_select(const LadxTables *t, int unit, const LadxSettings *validated, int spot_count,
			LadxTablesStep *step, LadxTablesError *err);

typedef struct LadxTablesWalk {
	const LadxTables *t;
	const LadxSettings *settings;
	int spot_count;
	int row;
	bool seen[LADX_TABLES_UNIT_MAX];
} LadxTablesWalk;

void ladx_tables_walk_init(LadxTablesWalk *w, const LadxTables *t, const LadxSettings *validated, int spot_count);
bool ladx_tables_walk_next(LadxTablesWalk *w, LadxTablesStep *step, LadxTablesError *err);

typedef struct LadxTablesRecord {
	bool table_record;
	uint32_t offset;
	int table;
	LadxWlogKey key;
	bool pointer;
	uint32_t value;
	LadxAtom *atoms;
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
bool ladx_tables_records_next(LadxTablesRecords *it, LadxTablesRecord *rec, LadxAtom *buf, size_t cap,
			      LadxTablesError *err);

LadxTablesStatus ladx_tables_apply(const LadxTablesRecord *rec, const uint8_t *source, size_t source_len, uint8_t *out,
				   size_t out_len);

int ladx_tables_spot_count(const LadxLogic *logic);

#endif
