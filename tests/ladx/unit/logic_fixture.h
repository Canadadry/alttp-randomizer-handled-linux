#ifndef LADX_LOGIC_FIXTURE_H
#define LADX_LOGIC_FIXTURE_H

#include "ladx_logic.h"
#include "ladx_settings.h"

#include <stddef.h>
#include <stdio.h>

void lf_requirement_text(const char *test, const LadxReqArena *a, LadxReq r, char *out, size_t cap);

size_t lf_graph_text(const char *test, const LadxLogic *logic, char *out, size_t cap);

void lf_index_ranges(const char *test, const bool *member, int count, char *out, size_t cap);

#define LF_NAME_MAX 128
#define LF_SETTINGS_MAX 512
#define LF_WS_TEXT_MAX 512
#define LF_WS_LINE_MAX (LF_WS_TEXT_MAX + 32)

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
	LadxSettings settings;
	LadxWorldSetup ws;
	LfWorldSetupRest ws_rest;
	char ws_lines[13][LF_WS_LINE_MAX];
	char graph_sha512[129];
	char explore[LF_EXPLORE_COUNT][LF_EXPLORE_MAX];
} LfConfig;

typedef struct LfExplore {
	int item[LADX_ITEM_COUNT];
	int count[LADX_ITEM_COUNT];
	int items;
	char reached[LF_EXPLORE_MAX];
	char required[LF_EXPLORE_MAX];
} LfExplore;

void lf_explore_parse(const char *test, const char *config, const char *line, LfExplore *out);
int lf_index_ranges_parse(const char *text, bool *member, int count);

FILE *lf_vectors_open(const char *test, const char *path);
int lf_vectors_next(const char *test, FILE *f, LfConfig *config);

int lf_ws_parse(const char *field, const char *value, LadxWorldSetup *ws, LfWorldSetupRest *rest);
void lf_ws_render(const LadxWorldSetup *ws, const LfWorldSetupRest *rest, char lines[13][LF_WS_LINE_MAX]);

#define LF_GRAPH_TEXT_MAX (512 * 1024)

typedef struct LfGraph {
	char name[LF_NAME_MAX];
	LadxSettings settings;
	char text[LF_GRAPH_TEXT_MAX];
	size_t len;
} LfGraph;

FILE *lf_graphs_open(const char *test, const char *path);
int lf_graphs_next(const char *test, FILE *f, LfGraph *g);

int lf_dungeon_compare(const char *test, const char *graph, int n, const char *want, size_t want_len,
		       const char *got, size_t got_len);

#endif
