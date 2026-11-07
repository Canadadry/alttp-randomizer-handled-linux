#include "logic_fixture.h"

#include "support.h"

#include <stdlib.h>
#include <string.h>

static void append(const char *test, char *out, size_t cap, const char *s)
{
	size_t len = strlen(out);

	if (len + strlen(s) + 1 > cap)
		test_fail(test, "rendered text longer than %zu bytes", cap);
	memcpy(out + len, s, strlen(s) + 1);
}

#define NODE_WIDTH_MAX 256

void lf_requirement_text(const char *test, const LadxReqArena *a, LadxReq r, char *out, size_t cap)
{
	int items[NODE_WIDTH_MAX];
	LadxReq children[NODE_WIDTH_MAX];
	int ni, nc, i;
	char num[16];
	LadxNodeKind kind;

	switch (r.tag) {
	case LADX_REQ_NONE:
		append(test, out, cap, "-");
		return;
	case LADX_REQ_TRUE:
		append(test, out, cap, "T");
		return;
	case LADX_REQ_FALSE:
		append(test, out, cap, "F");
		return;
	case LADX_REQ_ITEM:
		append(test, out, cap, ladx_item_name(r.id));
		return;
	case LADX_REQ_NODE:
		break;
	}
	ni = ladx_req_node_items(a, r, items, NODE_WIDTH_MAX);
	nc = ladx_req_node_children(a, r, children, NODE_WIDTH_MAX);
	if (ni > NODE_WIDTH_MAX || nc > NODE_WIDTH_MAX)
		test_fail(test, "a requirement node wider than %d", NODE_WIDTH_MAX);
	sprintf(num, "%d", ladx_req_node_amount(a, r));
	kind = ladx_req_node_kind(a, r);
	switch (kind) {
	case LADX_NODE_AND:
	case LADX_NODE_OR:
		append(test, out, cap, kind == LADX_NODE_AND ? "and(" : "or(");
		for (i = 0; i < ni; i++) {
			if (i)
				append(test, out, cap, ",");
			append(test, out, cap, ladx_item_name(items[i]));
		}
		for (i = 0; i < nc; i++) {
			if (i || ni)
				append(test, out, cap, ",");
			lf_requirement_text(test, a, children[i], out, cap);
		}
		append(test, out, cap, ")");
		return;
	case LADX_NODE_COUNT:
	case LADX_NODE_FOUND:
		append(test, out, cap, kind == LADX_NODE_COUNT ? "count(" : "found(");
		append(test, out, cap, ladx_item_name(items[0]));
		append(test, out, cap, ",");
		append(test, out, cap, num);
		append(test, out, cap, ")");
		return;
	case LADX_NODE_COUNTS:
		append(test, out, cap, "counts(");
		append(test, out, cap, num);
		for (i = 0; i < ni; i++) {
			append(test, out, cap, ",");
			append(test, out, cap, ladx_item_name(items[i]));
		}
		append(test, out, cap, ")");
		return;
	}
	test_fail(test, "unknown requirement node kind %d", (int)kind);
}

/* ladx_oracle.field: "" for an empty text. */
static const char *field(const char *s)
{
	return s[0] ? s : "\"\"";
}

static char line[64 * 1024];

/* append's strlen of out made the 425-graph replay quadratic: 4 minutes
 * under qemu-arm. */
static void emit(const char *test, char *out, size_t cap, size_t *len)
{
	size_t n = strlen(line);

	if (*len + n + 1 > cap)
		test_fail(test, "rendered text longer than %zu bytes", cap);
	memcpy(out + *len, line, n + 1);
	*len += n;
}

size_t lf_graph_text(const char *test, const LadxLogic *logic, char *out, size_t cap)
{
	int i, j, n;
	int options[LADX_ITEM_COUNT * 4];
	char num[64];
	size_t len = 0;

	if (cap == 0)
		test_fail(test, "no room for the graph text");
	out[0] = '\0';
	for (i = 0; i < ladx_logic_location_count(logic); i++) {
		const LadxLocation *loc = ladx_logic_location(logic, i);

		if (loc->dungeon < 0)
			sprintf(line, "loc %d -\t", i);
		else
			sprintf(line, "loc %d %d\t", i, loc->dungeon);
		append(test, line, sizeof line, loc->name ? field(loc->name) : "-");
		append(test, line, sizeof line, "\n");
		emit(test, out, cap, &len);
	}
	for (i = 0; i < ladx_logic_spot_count(logic); i++) {
		const LadxSpot *spot = ladx_logic_spot(logic, i);
		const char *kind = ladx_spot_kind_name(spot->kind);

		if (!kind)
			test_fail(test, "spot %d has no kind name", i);
		n = ladx_logic_spot_options(logic, i, options, (int)(sizeof options / sizeof options[0]));
		if (n > (int)(sizeof options / sizeof options[0]))
			test_fail(test, "spot %d has %d options", i, n);
		sprintf(line, "spot %d %s %s ", spot->location, kind, field(spot->name_id));
		for (j = 0; j < n; j++) {
			if (j)
				append(test, line, sizeof line, ",");
			append(test, line, sizeof line, ladx_item_name(options[j]));
		}
		if (n == 0)
			append(test, line, sizeof line, "-");
		append(test, line, sizeof line, "\t");
		append(test, line, sizeof line, field(ladx_spot_metadata_name(spot)));
		append(test, line, sizeof line, "\t");
		append(test, line, sizeof line, field(ladx_spot_metadata_area(spot)));
		append(test, line, sizeof line, "\n");
		emit(test, out, cap, &len);
	}
	for (i = 0; i < ladx_logic_location_count(logic); i++) {
		const LadxLocation *loc = ladx_logic_location(logic, i);

		for (j = 0; j < loc->edge_count; j++) {
			const LadxEdge *edge = ladx_logic_edge(logic, loc->edge_first + j);

			sprintf(num, "edge %d %d ", i, edge->target);
			strcpy(line, num);
			lf_requirement_text(test, ladx_logic_arena(logic), edge->req, line, sizeof line);
			append(test, line, sizeof line, "\n");
			emit(test, out, cap, &len);
		}
	}
	return len;
}

void lf_index_ranges(const char *test, const bool *member, int count, char *out, size_t cap)
{
	int i = 0, first;
	char part[32];

	out[0] = '\0';
	while (i < count) {
		if (!member[i]) {
			i++;
			continue;
		}
		first = i;
		while (i + 1 < count && member[i + 1])
			i++;
		if (out[0])
			append(test, out, cap, ",");
		if (first == i)
			sprintf(part, "%d", i);
		else
			sprintf(part, "%d-%d", first, i);
		append(test, out, cap, part);
		i++;
	}
	if (!out[0])
		append(test, out, cap, "-");
}

static const char *const ws_fields[13] = {
	"entrance_mapping", "boss_mapping", "miniboss_mapping", "goal", "goal_count", "bingo_goals",
	"sign_maze", "multichest", "map", "dungeon_chain", "inside_to_outside", "keep_two_way",
	"one_on_one",
};

/* A decimal int, sign allowed, nothing else; 0 when malformed. */
static int parse_int(const char *s, int *out)
{
	char *end;
	long v;

	if (!s[0] || (s[0] != '-' && (s[0] < '0' || s[0] > '9')))
		return 0;
	v = strtol(s, &end, 10);
	if (*end || v < -1000000 || v > 1000000)
		return 0;
	*out = (int)v;
	return 1;
}

static int copy_text(char *dst, const char *src)
{
	if (strlen(src) >= LF_WS_TEXT_MAX)
		return 0;
	strcpy(dst, src);
	return 1;
}

static int parse_bool_text(char *dst, const char *value)
{
	return (strcmp(value, "0") == 0 || strcmp(value, "1") == 0) && copy_text(dst, value);
}

static int parse_boss_mapping(const char *value, LadxWorldSetup *ws)
{
	char buf[LF_WS_TEXT_MAX];
	char *p, *comma;
	int i;

	if (!copy_text(buf, value))
		return 0;
	p = buf;
	for (i = 0; i < 9; i++) {
		comma = strchr(p, ',');
		if ((i < 8) != (comma != NULL))
			return 0;
		if (comma)
			*comma = '\0';
		if (!parse_int(p, &ws->boss_mapping[i]) || ws->boss_mapping[i] < 0 || ws->boss_mapping[i] > 8)
			return 0;
		p = comma ? comma + 1 : p;
	}
	return 1;
}

static int parse_miniboss_mapping(const char *value, LadxWorldSetup *ws)
{
	char buf[LF_WS_TEXT_MAX];
	char *p, *comma, *eq;
	int i, m;

	if (!copy_text(buf, value))
		return 0;
	p = buf;
	for (i = 0; i < LADX_MINIBOSS_KEY_COUNT; i++) {
		comma = strchr(p, ',');
		if ((i < LADX_MINIBOSS_KEY_COUNT - 1) != (comma != NULL))
			return 0;
		if (comma)
			*comma = '\0';
		eq = strchr(p, '=');
		if (!eq)
			return 0;
		*eq = '\0';
		if (strcmp(p, ladx_miniboss_key(i)) != 0)
			return 0;
		m = ladx_miniboss_find(eq + 1);
		if (m < 0)
			return 0;
		ws->miniboss_mapping[i] = (LadxMiniboss)m;
		p = comma ? comma + 1 : p;
	}
	return 1;
}

static int parse_goal(const char *value, LadxWorldSetup *ws)
{
	size_t i, n;
	int g;

	if (value[0] == '=') {
		n = strlen(value + 1);
		if (n == 0 || n > 8)
			return 0;
		for (i = 1; i <= n; i++)
			if (value[i] < '1' || value[i] > '8')
				return 0;
		ws->goal = LADX_GOAL_SPECIFIC;
		strcpy(ws->goal_instruments, value + 1);
		return 1;
	}
	g = ladx_goal_find(value);
	if (g < 0)
		return 0;
	ws->goal = (LadxGoal)g;
	ws->goal_instruments[0] = '\0';
	return 1;
}

int lf_ws_parse(const char *name, const char *value, LadxWorldSetup *ws, LfWorldSetupRest *rest)
{
	if (strcmp(name, "entrance_mapping") == 0)
		return strcmp(value, "identity") == 0;
	if (strcmp(name, "bingo_goals") == 0 || strcmp(name, "map") == 0 || strcmp(name, "dungeon_chain") == 0)
		return strcmp(value, "-") == 0;
	if (strcmp(name, "boss_mapping") == 0)
		return parse_boss_mapping(value, ws);
	if (strcmp(name, "miniboss_mapping") == 0)
		return parse_miniboss_mapping(value, ws);
	if (strcmp(name, "goal") == 0)
		return parse_goal(value, ws);
	if (strcmp(name, "goal_count") == 0)
		return parse_int(value, &ws->goal_count);
	if (strcmp(name, "multichest") == 0)
		return ladx_item_find(value) != LADX_ITEM_NONE && copy_text(rest->multichest, value);
	if (strcmp(name, "sign_maze") == 0)
		return value[0] && copy_text(rest->sign_maze, value);
	if (strcmp(name, "inside_to_outside") == 0)
		return parse_bool_text(rest->inside_to_outside, value);
	if (strcmp(name, "keep_two_way") == 0)
		return parse_bool_text(rest->keep_two_way, value);
	if (strcmp(name, "one_on_one") == 0)
		return parse_bool_text(rest->one_on_one, value);
	return 0;
}

void lf_ws_render(const LadxWorldSetup *ws, const LfWorldSetupRest *rest, char lines[13][LF_WS_LINE_MAX])
{
	char value[LF_WS_TEXT_MAX];
	char part[64];
	int i, f;

	for (f = 0; f < 13; f++) {
		value[0] = '\0';
		switch (f) {
		case 0:
			strcpy(value, "identity");
			break;
		case 1:
			for (i = 0; i < 9; i++) {
				sprintf(part, i ? ",%d" : "%d", ws->boss_mapping[i]);
				strcat(value, part);
			}
			break;
		case 2:
			for (i = 0; i < LADX_MINIBOSS_KEY_COUNT; i++) {
				sprintf(part, "%s%s=%s", i ? "," : "", ladx_miniboss_key(i),
					ladx_miniboss_name(ws->miniboss_mapping[i]));
				strcat(value, part);
			}
			break;
		case 3:
			if (ws->goal == LADX_GOAL_SPECIFIC)
				sprintf(value, "=%s", ws->goal_instruments);
			else
				strcpy(value, ladx_goal_name(ws->goal));
			break;
		case 4:
			sprintf(value, "%d", ws->goal_count);
			break;
		case 6:
			strcpy(value, rest->sign_maze);
			break;
		case 7:
			strcpy(value, rest->multichest);
			break;
		case 10:
			strcpy(value, rest->inside_to_outside);
			break;
		case 11:
			strcpy(value, rest->keep_two_way);
			break;
		case 12:
			strcpy(value, rest->one_on_one);
			break;
		default:
			strcpy(value, "-");
			break;
		}
		snprintf(lines[f], LF_WS_LINE_MAX, "ws %s %s", ws_fields[f], value);
	}
}

static char vline[LF_EXPLORE_MAX + 64];
static long vline_no;
static const char *vfile = "";

/* The next non-comment line, newline stripped; 0 at the end. */
static int next_line(const char *test, FILE *f)
{
	size_t len;

	do {
		if (!fgets(vline, sizeof vline, f))
			return 0;
		vline_no++;
		len = strlen(vline);
		if (len == 0 || vline[len - 1] != '\n')
			test_fail(test, "%s line %ld: too long or unterminated", vfile, vline_no);
		vline[len - 1] = '\0';
	} while (vline[0] == '#');
	return 1;
}

FILE *lf_vectors_open(const char *test, const char *path)
{
	FILE *f = fopen(path, "r");

	if (!f)
		test_fail(test, "cannot open %s", path);
	vline_no = 0;
	vfile = path;
	return f;
}

static void parse_settings(const char *test, const char *name, const char *text, LadxSettings *settings)
{
	char buf[LF_SETTINGS_MAX + 2];
	char *p, *comma;
	size_t len;

	ladx_settings_defaults(settings);
	if (strlen(text) >= LF_SETTINGS_MAX)
		test_fail(test, "config %s: settings too long", name);
	strcpy(buf, text);
	p = buf;
	while (*p) {
		comma = strchr(p, ',');
		if (comma)
			*comma = '\0';
		len = strlen(p);
		if (len >= 3 && strcmp(p + len - 3, "=\"\"") == 0)
			p[len - 2] = '\0';
		if (ladx_settings_set(settings, p) != LADX_SETTINGS_OK)
			test_fail(test, "config %s: setting %s refused", name, p);
		if (!comma)
			break;
		p = comma + 1;
	}
	ladx_settings_validate(settings);
}

int lf_vectors_next(const char *test, FILE *f, LfConfig *c)
{
	char *space;
	int i;

	if (!next_line(test, f))
		return 0;
	if (strncmp(vline, "config ", 7) != 0)
		test_fail(test, "logic_vectors line %ld: want a config line", vline_no);
	space = strchr(vline + 7, ' ');
	if (!space || (size_t)(space - (vline + 7)) >= LF_NAME_MAX || strlen(space + 1) >= LF_SETTINGS_MAX)
		test_fail(test, "logic_vectors line %ld: malformed config line", vline_no);
	*space = '\0';
	strcpy(c->name, vline + 7);
	strcpy(c->settings_text, space + 1);
	parse_settings(test, c->name, c->settings_text, &c->settings);
	ladx_world_setup_defaults(&c->ws);
	memset(&c->ws_rest, 0, sizeof c->ws_rest);
	for (i = 0; i < 13; i++) {
		char *value;

		if (!next_line(test, f) || strncmp(vline, "ws ", 3) != 0)
			test_fail(test, "config %s: want 13 ws lines", c->name);
		if (strlen(vline) >= LF_WS_LINE_MAX)
			test_fail(test, "config %s: ws line too long", c->name);
		strcpy(c->ws_lines[i], vline);
		value = strchr(vline + 3, ' ');
		if (!value)
			test_fail(test, "config %s: malformed ws line %s", c->name, c->ws_lines[i]);
		*value++ = '\0';
		if (strcmp(vline + 3, ws_fields[i]) != 0)
			test_fail(test, "config %s: ws field %s out of order, want %s", c->name, vline + 3, ws_fields[i]);
		if (!lf_ws_parse(vline + 3, value, &c->ws, &c->ws_rest))
			test_fail(test, "config %s: ws value refused: %s", c->name, c->ws_lines[i]);
	}
	if (!next_line(test, f) || strncmp(vline, "graph ", 6) != 0 || strlen(vline + 6) != 128)
		test_fail(test, "config %s: want a graph line with a SHA-512", c->name);
	strcpy(c->graph_sha512, vline + 6);
	for (i = 0; i < LF_EXPLORE_COUNT; i++) {
		if (!next_line(test, f) || strncmp(vline, "explore ", 8) != 0)
			test_fail(test, "config %s: want %d explore lines", c->name, LF_EXPLORE_COUNT);
		if (strlen(vline + 8) >= LF_EXPLORE_MAX)
			test_fail(test, "config %s: explore line too long", c->name);
		strcpy(c->explore[i], vline + 8);
	}
	return 1;
}

void lf_explore_parse(const char *test, const char *config, const char *line, LfExplore *out)
{
	char buf[LF_EXPLORE_MAX];
	char *arrow, *tab, *p, *comma, *eq, *end;
	long count;

	if (strlen(line) >= sizeof buf)
		test_fail(test, "config %s: explore line too long", config);
	strcpy(buf, line);
	arrow = strstr(buf, " -> ");
	tab = arrow ? strchr(arrow, '\t') : NULL;
	if (!tab || strchr(tab + 1, '\t') || !tab[1] || tab == arrow + 4)
		test_fail(test, "config %s: malformed explore line %s", config, line);
	*arrow = '\0';
	*tab = '\0';
	strcpy(out->reached, arrow + 4);
	strcpy(out->required, tab + 1);
	out->items = 0;
	if (strcmp(buf, "-") == 0)
		return;
	for (p = buf;; p = comma + 1) {
		comma = strchr(p, ',');
		if (comma)
			*comma = '\0';
		eq = strchr(p, '=');
		if (!eq || out->items == LADX_ITEM_COUNT)
			test_fail(test, "config %s: malformed explore inventory entry %s", config, p);
		*eq = '\0';
		count = strtol(eq + 1, &end, 10);
		if (end == eq + 1 || *end || count < 1 || count > 1000000)
			test_fail(test, "config %s: malformed explore count %s=%s", config, p, eq + 1);
		out->item[out->items] = ladx_item_find(p);
		if (out->item[out->items] == LADX_ITEM_NONE)
			test_fail(test, "config %s: unknown explore item %s", config, p);
		out->count[out->items++] = (int)count;
		if (!comma)
			break;
	}
}

int lf_index_ranges_parse(const char *text, bool *member, int count)
{
	const char *p = text;
	char *end;
	long first, last;
	int i;

	for (i = 0; i < count; i++)
		member[i] = false;
	if (strcmp(text, "-") == 0)
		return 1;
	for (;;) {
		first = strtol(p, &end, 10);
		if (end == p || first < 0)
			return 0;
		last = first;
		if (*end == '-') {
			p = end + 1;
			last = strtol(p, &end, 10);
			if (end == p || last < first)
				return 0;
		}
		if (last >= count)
			return 0;
		for (i = (int)first; i <= (int)last; i++)
			member[i] = true;
		if (!*end)
			return 1;
		if (*end != ',')
			return 0;
		p = end + 1;
	}
}

FILE *lf_graphs_open(const char *test, const char *path)
{
	FILE *f = lf_vectors_open(test, path);

	if (!next_line(test, f) || strncmp(vline, "graph ", 6) != 0)
		test_fail(test, "%s: want a graph line first", path);
	return f;
}

/* The settings are the name's part before its last '/' ("logic=hard"). */
int lf_graphs_next(const char *test, FILE *f, LfGraph *g)
{
	char settings[LF_SETTINGS_MAX];
	const char *slash;
	size_t len;

	if (strncmp(vline, "graph ", 6) != 0)
		return 0;
	slash = strrchr(vline + 6, '/');
	if (strlen(vline + 6) >= LF_NAME_MAX || !slash || (size_t)(slash - (vline + 6)) >= LF_SETTINGS_MAX)
		test_fail(test, "%s line %ld: malformed graph line", vfile, vline_no);
	strcpy(g->name, vline + 6);
	memcpy(settings, vline + 6, (size_t)(slash - (vline + 6)));
	settings[slash - (vline + 6)] = '\0';
	parse_settings(test, g->name, settings, &g->settings);
	g->len = 0;
	vline[0] = '\0';
	while (next_line(test, f)) {
		if (strncmp(vline, "graph ", 6) == 0)
			return 1;
		len = strlen(vline);
		if (g->len + len + 2 > sizeof g->text)
			test_fail(test, "graph %s: text over %lu bytes", g->name, (unsigned long)sizeof g->text);
		memcpy(g->text + g->len, vline, len);
		g->len += len;
		g->text[g->len++] = '\n';
		g->text[g->len] = '\0';
	}
	vline[0] = '\0';
	return 1;
}

#define WALK_LOC_MAX 1024
#define WALK_SPOT_MAX 512
#define WALK_EDGE_MAX 2048

/* A canonical text cut into its lines' fields, edges and spots chained per
 * location in text order. */
typedef struct WalkGraph {
	char text[LF_GRAPH_TEXT_MAX + 1];
	int locs, spots, edges;
	int dungeon[WALK_LOC_MAX];
	const char *name[WALK_LOC_MAX];
	int spot_head[WALK_LOC_MAX], spot_tail[WALK_LOC_MAX];
	int edge_head[WALK_LOC_MAX], edge_tail[WALK_LOC_MAX];
	const char *spot_text[WALK_SPOT_MAX];
	int spot_next[WALK_SPOT_MAX];
	int edge_to[WALK_EDGE_MAX];
	const char *edge_req[WALK_EDGE_MAX];
	int edge_next[WALK_EDGE_MAX];
	int pos[WALK_LOC_MAX];
	int walk[WALK_LOC_MAX];
	int walk_len;
} WalkGraph;

static WalkGraph walk_want, walk_got;

static void chain(int *head, int *tail, int *next, int i)
{
	next[i] = -1;
	if (*tail < 0)
		*head = i;
	else
		next[*tail] = i;
	*tail = i;
}

static void walk_parse(const char *test, const char *what, WalkGraph *g, const char *text, size_t len)
{
	char *line, *end;
	int i, a, b, off;

	if (len > LF_GRAPH_TEXT_MAX)
		test_fail(test, "%s: graph text over %d bytes", what, LF_GRAPH_TEXT_MAX);
	memcpy(g->text, text, len);
	g->text[len] = '\0';
	g->locs = g->spots = g->edges = 0;
	for (i = 0; i < WALK_LOC_MAX; i++) {
		g->spot_head[i] = g->spot_tail[i] = -1;
		g->edge_head[i] = g->edge_tail[i] = -1;
		g->pos[i] = -1;
	}
	for (line = g->text; *line; line = end + 1) {
		end = strchr(line, '\n');
		if (!end)
			test_fail(test, "%s: unterminated line", what);
		*end = '\0';
		if (sscanf(line, "loc %d %n", &a, &off) == 1) {
			char *tab = strchr(line + off, '\t');

			if (a != g->locs || a >= WALK_LOC_MAX || !tab)
				test_fail(test, "%s: malformed %s", what, line);
			*tab = '\0';
			g->dungeon[a] = strcmp(line + off, "-") == 0 ? -1 : atoi(line + off);
			g->name[a] = tab + 1;
			g->locs++;
		} else if (sscanf(line, "spot %d %n", &a, &off) == 1) {
			if (a < 0 || a >= g->locs || g->spots >= WALK_SPOT_MAX)
				test_fail(test, "%s: malformed %s", what, line);
			g->spot_text[g->spots] = line + off;
			chain(&g->spot_head[a], &g->spot_tail[a], g->spot_next, g->spots++);
		} else if (sscanf(line, "edge %d %d %n", &a, &b, &off) == 2) {
			if (a < 0 || a >= g->locs || b < 0 || b >= g->locs || g->edges >= WALK_EDGE_MAX)
				test_fail(test, "%s: malformed %s", what, line);
			g->edge_to[g->edges] = b;
			g->edge_req[g->edges] = line + off;
			chain(&g->edge_head[a], &g->edge_tail[a], g->edge_next, g->edges++);
		} else {
			test_fail(test, "%s: malformed %s", what, line);
		}
	}
}

/* Logic.__recursiveFindAll's preorder, restricted to dungeon n. */
static void walk_from(WalkGraph *g, int loc, int n)
{
	int e;

	g->pos[loc] = g->walk_len;
	g->walk[g->walk_len++] = loc;
	for (e = g->edge_head[loc]; e >= 0; e = g->edge_next[e])
		if (g->dungeon[g->edge_to[e]] == n && g->pos[g->edge_to[e]] < 0)
			walk_from(g, g->edge_to[e], n);
}

static void walk_dungeon(const char *test, const char *what, WalkGraph *g, int n)
{
	char entrance[32];
	int i;

	snprintf(entrance, sizeof entrance, "D%d Entrance", n);
	g->walk_len = 0;
	for (i = 0; i < g->locs; i++)
		if (g->dungeon[i] == n && strcmp(g->name[i], entrance) == 0) {
			walk_from(g, i, n);
			return;
		}
	test_fail(test, "%s: no location %s", what, entrance);
}

/* The next in-dungeon edge from e on; -1 at the end. */
static int next_inside(const WalkGraph *g, int e, int n)
{
	while (e >= 0 && g->dungeon[g->edge_to[e]] != n)
		e = g->edge_next[e];
	return e;
}

int lf_dungeon_compare(const char *test, const char *graph, int n, const char *want, size_t want_len,
		       const char *got, size_t got_len)
{
	WalkGraph *w = &walk_want, *g = &walk_got;
	int p, k;

	walk_parse(test, graph, w, want, want_len);
	walk_parse(test, "the C graph", g, got, got_len);
	walk_dungeon(test, graph, w, n);
	walk_dungeon(test, "the C graph", g, n);
	for (p = 0; p < w->walk_len || p < g->walk_len; p++) {
		int lw, lg, sw, sg, ew, eg;

		if (p >= w->walk_len || p >= g->walk_len)
			test_fail(test, "dungeon D%d, graph %s: walk position %d: %s only in %s", n, graph, p,
				  p < w->walk_len ? w->name[w->walk[p]] : g->name[g->walk[p]],
				  p < w->walk_len ? "the fixture" : "C");
		lw = w->walk[p];
		lg = g->walk[p];
		if (strcmp(w->name[lw], g->name[lg]) != 0)
			test_fail(test, "dungeon D%d, graph %s: walk position %d: name %s, fixture %s", n, graph, p,
				  g->name[lg], w->name[lw]);
		for (sw = w->spot_head[lw], sg = g->spot_head[lg], k = 0; sw >= 0 || sg >= 0;
		     sw = sw >= 0 ? w->spot_next[sw] : -1, sg = sg >= 0 ? g->spot_next[sg] : -1, k++)
			if (sw < 0 || sg < 0 || strcmp(w->spot_text[sw], g->spot_text[sg]) != 0)
				test_fail(test, "dungeon D%d, graph %s: walk position %d (%s): spot %d: %s, fixture %s", n,
					  graph, p, w->name[lw], k, sg >= 0 ? g->spot_text[sg] : "none",
					  sw >= 0 ? w->spot_text[sw] : "none");
		ew = next_inside(w, w->edge_head[lw], n);
		eg = next_inside(g, g->edge_head[lg], n);
		for (k = 0; ew >= 0 || eg >= 0; k++) {
			if (ew < 0 || eg < 0 || w->pos[w->edge_to[ew]] != g->pos[g->edge_to[eg]] ||
			    strcmp(w->edge_req[ew], g->edge_req[eg]) != 0)
				test_fail(test,
					  "dungeon D%d, graph %s: walk position %d (%s): in-dungeon edge %d: to %d %s, "
					  "fixture to %d %s",
					  n, graph, p, w->name[lw], k, eg >= 0 ? g->pos[g->edge_to[eg]] : -1,
					  eg >= 0 ? g->edge_req[eg] : "none", ew >= 0 ? w->pos[w->edge_to[ew]] : -1,
					  ew >= 0 ? w->edge_req[ew] : "none");
			ew = next_inside(w, w->edge_next[ew], n);
			eg = next_inside(g, g->edge_next[eg], n);
		}
	}
	return w->walk_len;
}
