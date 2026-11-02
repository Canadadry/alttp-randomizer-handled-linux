#ifndef RANDO_TEST_SUPPORT_H
#define RANDO_TEST_SUPPORT_H

#include "rando_items.h"
#include "rando_world.h"

#include <stdint.h>
#include <stdio.h>

#define FIXTURE_WORLD_DUMP "oracle/world_dump.txt"
#define FIXTURE_PREDICATE_VECTORS "oracle/predicate_vectors.txt"
#define FIXTURE_NOGLITCHES_ROWS "oracle/noglitches_rows.txt"
#define FIXTURE_OVERWORLD_GLITCHES_ROWS "oracle/overworld_glitches_rows.txt"
#define FIXTURE_MAJOR_GLITCHES_ROWS "oracle/major_glitches_rows.txt"
#define FIXTURE_INVERTED_ROWS "oracle/inverted_rows.txt"
#define FIXTURE_INVERTED_OVERWORLD_GLITCHES_ROWS "oracle/inverted_overworld_glitches_rows.txt"
#define FIXTURE_INVERTED_MAJOR_GLITCHES_ROWS "oracle/inverted_major_glitches_rows.txt"
#define FIXTURE_POOL_LISTS "oracle/pool_lists.txt"
#define FIXTURE_PREPARED_POOLS "oracle/prepared_pools.txt"

#define FIXTURE_LINE_MAX 8192
#define FIXTURE_FIELDS_MAX 32

typedef struct Fixture {
	FILE *file;
	const char *path;
	long line_no;
	char line[FIXTURE_LINE_MAX];
	char record[FIXTURE_LINE_MAX];
	char *field[FIXTURE_FIELDS_MAX];
	int nfields;
} Fixture;

int test_fixtures_mode(const char *test, int argc, char **argv);

void test_fail(const char *test, const char *fmt, ...);

void test_snapshot(const char *test, const char *path, const char *actual);

void fixture_open(Fixture *fx, const char *path);

int fixture_next(Fixture *fx);

void fixture_fail(const Fixture *fx, const char *why);

void fixture_close(Fixture *fx);

void fixture_parse_view(const Fixture *fx, const char *view, uint16_t out[RANDO_ITEM_COUNT],
			const uint16_t all[RANDO_ITEM_COUNT]);

void fixture_parse_items(const Fixture *fx, const char *counts, const char *values, RandoItems *out);

void fixture_apply_config(const Fixture *fx, RandoConfig *config, const char *key, const char *json);

typedef struct PredicateColumn {
	const char *name;
	int (*eval)(const RandoItems *items, const RandoConfig *config);
} PredicateColumn;

int fixture_replay_predicates(const char *test, const PredicateColumn *columns, int ncolumns, long *values);

typedef int (*RowFilter)(const char *file, const char *method);

int fixture_replay_rows(const char *test, const char *path, RowFilter filter, int *total);

#endif
