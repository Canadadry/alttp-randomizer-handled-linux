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

size_t lf_graph_text(const char *test, const LadxLogic *logic, char *out, size_t cap)
{
	int i, j, n;
	int options[LADX_ITEM_COUNT * 4];
	char num[64];

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
		append(test, out, cap, line);
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
		append(test, out, cap, line);
	}
	for (i = 0; i < ladx_logic_location_count(logic); i++) {
		const LadxLocation *loc = ladx_logic_location(logic, i);

		for (j = 0; j < loc->edge_count; j++) {
			const LadxEdge *edge = ladx_logic_edge(logic, loc->edge_first + j);

			sprintf(num, "edge %d %d ", i, edge->target);
			strcpy(line, num);
			lf_requirement_text(test, ladx_logic_arena(logic), edge->req, line, sizeof line);
			append(test, line, sizeof line, "\n");
			append(test, out, cap, line);
		}
	}
	return strlen(out);
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
			test_fail(test, "logic_vectors line %ld: too long or unterminated", vline_no);
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
	return f;
}

static void parse_settings(const char *test, LfConfig *c)
{
	char buf[LF_SETTINGS_MAX + 2];
	char *p, *comma;
	size_t len;

	ladx_settings_defaults(&c->settings);
	strcpy(buf, c->settings_text);
	p = buf;
	while (*p) {
		comma = strchr(p, ',');
		if (comma)
			*comma = '\0';
		len = strlen(p);
		if (len >= 3 && strcmp(p + len - 3, "=\"\"") == 0)
			p[len - 2] = '\0';
		if (ladx_settings_set(&c->settings, p) != LADX_SETTINGS_OK)
			test_fail(test, "config %s: setting %s refused", c->name, p);
		if (!comma)
			break;
		p = comma + 1;
	}
	ladx_settings_validate(&c->settings);
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
	parse_settings(test, c);
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
