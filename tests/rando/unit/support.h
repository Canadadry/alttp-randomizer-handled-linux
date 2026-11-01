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

/* Returns 1 for `--fixtures`, 0 with no argument; exits 2 on anything else. */
int test_fixtures_mode(const char *test, int argc, char **argv);

void test_fail(const char *test, const char *fmt, ...);

/* Exits 1 when the file cannot be opened. */
void fixture_open(Fixture *fx, const char *path);

/* Returns 0 at the end of the file, else 1 with field[0..nfields) pointing into fx (valid until the next call).
 * Skips empty lines and `#` comments; exits 1 on a line longer than FIXTURE_LINE_MAX. */
int fixture_next(Fixture *fx);

/* Prints the file, line number and the record as read, then exits 1. */
void fixture_fail(const Fixture *fx, const char *why);

void fixture_close(Fixture *fx);

/* Parses a fixture view (`E` or `A`, then `:<raw name>=<count>|...`) into out; `A` starts from
 * all, which may be NULL when the record cannot use it. Fails the record on a malformed view. */
void fixture_parse_view(const Fixture *fx, const char *view, uint16_t out[RANDO_ITEM_COUNT],
			const uint16_t all[RANDO_ITEM_COUNT]);

/* Both views, `A` meaning rando_items_all. */
void fixture_parse_items(const Fixture *fx, const char *counts, const char *values, RandoItems *out);

/* Sets the field of a VT config key from its JSON value; null leaves the field as it is. Fails the
 * record on an unknown key or value. rom.freeItemMenu, read by VT only when writing the ROM, is
 * checked to be an integer and ignored. */
void fixture_apply_config(const Fixture *fx, RandoConfig *config, const char *key, const char *json);

typedef struct PredicateColumn {
	const char *name;
	int (*eval)(const RandoItems *items, const RandoConfig *config);
} PredicateColumn;

/* Replays the predicate vectors: every value of a column named in columns[0..ncolumns) is checked
 * against eval on its multiset and config, the other columns are skipped. Every entry of columns
 * must appear in the fixture's columns record. Returns the number of pred records replayed and
 * stores the number of values checked in *values. */
int fixture_replay_predicates(const char *test, const PredicateColumn *columns, int ncolumns, long *values);

/* Whether a row of that test file (relative to the fixture's VT tree, or tests/ for RegionTest.php,
 * WorldTest.php, RetroBowTest.php and RandomizerTest.php) and method is replayed. */
typedef int (*RowFilter)(const char *file, const char *method);

/* Replays every row record of the fixture at path the filter accepts: a fresh world of the row's
 * variant, built from a config holding only the row's logic override (so world init derives that
 * logic's glitch flags, as VT resolves them lazily), then all its config overrides and preset items
 * and bosses (a boss name outside Boss::all sets RANDO_BOSS_RULELESS), then its query on its two-view item multiset; vanilla_dungeon_items applies every override to the
 * config the world is built from instead; prepared_item maps the overrides to generator options
 * (accessibility "full" as items) and runs rando_generate_until through prepareWorld, seed 1. On the
 * first answer that differs from VT's, prints the row's origin, query, expected and actual and exits 1. Returns the number of rows replayed and
 * stores the number of row records in *total. */
int fixture_replay_rows(const char *test, const char *path, RowFilter filter, int *total);

#endif
