#ifndef LADX_LOGIC_FIXTURE_H
#define LADX_LOGIC_FIXTURE_H

#include "ladx_logic.h"
#include "ladx_settings.h"

#include <stddef.h>
#include <stdio.h>

/* test_logic's support: the canonical text of ladx_oracle.graph_lines and
 * requirement_text, and the reader of oracle/logic_vectors.txt. Fails
 * through test_fail under the given test name. */

/* requirement_text, appended to out (NUL-terminated, cap bytes in all). */
void lf_requirement_text(const char *test, const LadxReqArena *a, LadxReq r, char *out, size_t cap);

/* graph_lines, each ending in LF, into out; returns the length. */
size_t lf_graph_text(const char *test, const LadxLogic *logic, char *out, size_t cap);

/* index_ranges: "0-2,5", "-" when empty. */
void lf_index_ranges(const char *test, const bool *member, int count, char *out, size_t cap);

#define LF_NAME_MAX 128
#define LF_SETTINGS_MAX 512
#define LF_WS_TEXT_MAX 512
#define LF_WS_LINE_MAX (LF_WS_TEXT_MAX + 32)

/* The ws fields the logic does not read, kept as text. */
typedef struct LfWorldSetupRest {
	char multichest[LF_WS_TEXT_MAX];
	char sign_maze[LF_WS_TEXT_MAX];
	char inside_to_outside[LF_WS_TEXT_MAX];
	char keep_two_way[LF_WS_TEXT_MAX];
	char one_on_one[LF_WS_TEXT_MAX];
} LfWorldSetupRest;

#define LF_EXPLORE_COUNT 6
#define LF_EXPLORE_MAX 4096

typedef struct LfConfig {
	char name[LF_NAME_MAX];
	char settings_text[LF_SETTINGS_MAX];
	LadxSettings settings; /* parsed on defaults, then validated */
	LadxWorldSetup ws;
	LfWorldSetupRest ws_rest;
	char ws_lines[13][LF_WS_LINE_MAX];
	char graph_sha512[129];
	char explore[LF_EXPLORE_COUNT][LF_EXPLORE_MAX];
} LfConfig;

/* Opens oracle/logic_vectors.txt-style text, skipping its # header. */
FILE *lf_vectors_open(const char *test, const char *path);
/* The next configuration, 0 at the end; a malformed one fails. */
int lf_vectors_next(const char *test, FILE *f, LfConfig *config);

/* A ws line's value parsed into ws or rest; 0 for an unknown field, a value
 * out of format, or a required constant (identity, -) not met. */
int lf_ws_parse(const char *field, const char *value, LadxWorldSetup *ws, LfWorldSetupRest *rest);
/* The 13 ws lines, in WorldSetup's order, from ws and rest. */
void lf_ws_render(const LadxWorldSetup *ws, const LfWorldSetupRest *rest, char lines[13][LF_WS_LINE_MAX]);

#define LF_GRAPH_TEXT_MAX (512 * 1024)

typedef struct LfGraph {
	char name[LF_NAME_MAX];
	LadxSettings settings; /* the name's settings on defaults, then validated */
	char text[LF_GRAPH_TEXT_MAX]; /* its loc, spot and edge lines, each ending in LF */
	size_t len;
} LfGraph;

/* Opens oracle/logic_graphs.txt-style text: # header, then graphs. */
FILE *lf_graphs_open(const char *test, const char *path);
/* The next graph, 0 at the end. */
int lf_graphs_next(const char *test, FILE *f, LfGraph *g);

/*
 * Dungeon n of two canonical texts: each walked in preorder from the
 * location named "D<n> Entrance" (dungeon n), following in order only the
 * edges whose target has dungeon n, locations paired by walk position.
 * Each pair's name, dungeon, spot lines (class, name id, options,
 * metadata) and in-dungeon edges (target walk position, requirement text)
 * must be equal; edges leaving the dungeon are skipped. A difference
 * fails with the dungeon, the graph's name and the first differing
 * location. Returns the walk's length.
 */
int lf_dungeon_compare(const char *test, const char *graph, int n, const char *want, size_t want_len,
		       const char *got, size_t got_len);

#endif
