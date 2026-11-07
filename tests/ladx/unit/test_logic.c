#include "ladx_logic.h"
#include "ladx_logic_builder.h"
#include "logic_fixture.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_logic"

static int checks;
static LadxLogic logic;
static char text[256 * 1024];

static void expect(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void expect_text(const char *want, const char *what)
{
	expect(!ladx_logic_broken(&logic), "the logic builds");
	lf_graph_text(TEST, &logic, text, sizeof text);
	checks++;
	if (strcmp(text, want) != 0)
		test_fail(TEST, "%s: got\n%s--- want\n%s---", what, text, want);
}

#define I(name) ladx_req_item(LADX_ITEM_##name)
#define A (ladx_logic_arena_mut(&logic))

/* location.py singleConnect, lines 41-57. */
static void test_single_connect(void)
{
	int a, b;

	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	b = ladx_logic_new_location(&logic, -1, "B");
	ladx_logic_single_connect(&logic, a, b, I(SWORD));
	ladx_logic_single_connect(&logic, a, b, I(BOMB));
	ladx_logic_finish(&logic, a);
	expect_text("loc 0 -\tA\nloc 1 -\tB\nedge 0 1 or(BOMB,SWORD)\n",
		    "a second edge to the same target is OR(new, old)");

	/* Lines 51-52: None on either side wins the merge. */
	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	b = ladx_logic_new_location(&logic, -1, "B");
	ladx_logic_single_connect(&logic, a, b, I(SWORD));
	ladx_logic_single_connect(&logic, a, b, ladx_req_none());
	ladx_logic_single_connect(&logic, b, a, ladx_req_none());
	ladx_logic_single_connect(&logic, b, a, I(SWORD));
	ladx_logic_finish(&logic, a);
	expect_text("loc 0 -\tA\nloc 1 -\tB\nedge 0 1 -\nedge 1 0 -\n", "None wins a merge, either way");

	/* Lines 44-47: False adds nothing, even over an edge; True is None. */
	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	b = ladx_logic_new_location(&logic, -1, "B");
	ladx_logic_single_connect(&logic, a, b, ladx_req_false());
	ladx_logic_single_connect(&logic, b, a, I(SWORD));
	ladx_logic_single_connect(&logic, b, a, ladx_req_false());
	ladx_logic_finish(&logic, b);
	expect_text("loc 0 -\tB\nloc 1 -\tA\nedge 0 1 SWORD\n", "a False edge is skipped, an edge kept under it");
	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	b = ladx_logic_new_location(&logic, -1, "B");
	ladx_logic_single_connect(&logic, a, b, I(SWORD));
	ladx_logic_single_connect(&logic, a, b, ladx_req_true());
	ladx_logic_finish(&logic, a);
	expect_text("loc 0 -\tA\nloc 1 -\tB\nedge 0 1 -\n", "a True edge merges as None");
}

/* location.py connect, lines 22-37. */
static void test_connect(void)
{
	int a, b, c;
	LadxReq node;
	const LadxEdge *ab, *ba;

	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	b = ladx_logic_new_location(&logic, -1, "B");
	c = ladx_logic_new_location(&logic, -1, "C");
	node = LADX_AND(A, I(SWORD), I(BOMB));
	ladx_logic_connect(&logic, a, b, node);
	ladx_logic_connect_back(&logic, a, c, I(FEATHER), ladx_req_false());
	ladx_logic_connect(&logic, c, b, ladx_req_none());
	ladx_logic_finish(&logic, a);
	expect_text("loc 0 -\tA\nloc 1 -\tB\nloc 2 -\tC\n"
		    "edge 0 1 and(SWORD,BOMB)\nedge 0 2 FEATHER\nedge 1 0 and(SWORD,BOMB)\nedge 1 2 -\nedge 2 1 -\n",
		    "connect adds both ways, back=False only forward");
	ab = ladx_logic_edge(&logic, ladx_logic_location(&logic, 0)->edge_first);
	ba = ladx_logic_edge(&logic, ladx_logic_location(&logic, 1)->edge_first);
	expect(ab->req.tag == LADX_REQ_NODE && ba->req.tag == LADX_REQ_NODE && ab->req.id == ba->req.id,
	       "the back requirement is the forward one, the same node");
	ladx_req_remove(A, ab->req, I(BOMB));
	expect_text("loc 0 -\tA\nloc 1 -\tB\nloc 2 -\tC\n"
		    "edge 0 1 and(SWORD)\nedge 0 2 FEATHER\nedge 1 0 and(SWORD)\nedge 1 2 -\nedge 2 1 -\n",
		    "a remove on the forward node is seen on the back edge");
}

static const int sword_shield[] = {LADX_ITEM_SWORD, LADX_ITEM_SHIELD};
static const int bomb_only[] = {LADX_ITEM_BOMB};

/*
 * main.py __recursiveFindAll, lines 192-200: created A, B, C, D, E, U with
 * A->C, A->B, C->D, D->B, B->E, B->A, U->A; from A, preorder in connection
 * order gives A, C, D, B, E, U never reached; iteminfo_list follows it.
 */
static void test_discovery(void)
{
	int a, b, c, d, e, u, i;
	static const char *const order[] = {"A", "C", "D", "B", "E"};

	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	b = ladx_logic_new_location(&logic, -1, "B");
	c = ladx_logic_new_location(&logic, -1, "C");
	d = ladx_logic_new_location(&logic, -1, "D");
	e = ladx_logic_new_location(&logic, -1, "E");
	u = ladx_logic_new_location(&logic, -1, "U");
	ladx_logic_add_spot(&logic, b, LADX_SPOT_CHEST, "0x0B0", bomb_only, 1);
	ladx_logic_add_spot(&logic, a, LADX_SPOT_CHEST, "0x0A1", bomb_only, 1);
	ladx_logic_add_spot(&logic, u, LADX_SPOT_CHEST, "0x0F0", bomb_only, 1);
	ladx_logic_add_spot(&logic, a, LADX_SPOT_CHEST, "0x0A2", bomb_only, 1);
	ladx_logic_single_connect(&logic, a, c, ladx_req_none());
	ladx_logic_single_connect(&logic, a, b, ladx_req_none());
	ladx_logic_single_connect(&logic, c, d, ladx_req_none());
	ladx_logic_single_connect(&logic, d, b, ladx_req_none());
	ladx_logic_single_connect(&logic, b, e, ladx_req_none());
	ladx_logic_single_connect(&logic, b, a, ladx_req_none());
	ladx_logic_single_connect(&logic, u, a, ladx_req_none());
	expect(ladx_logic_finish(&logic, a), "finish succeeds");
	expect(ladx_logic_start(&logic) == 0, "the start is location 0");
	expect(ladx_logic_location_count(&logic) == 5, "5 locations reached, U left out");
	for (i = 0; i < 5; i++)
		expect(strcmp(ladx_logic_location(&logic, i)->name, order[i]) == 0, "location_list in preorder");
	expect(ladx_logic_spot_count(&logic) == 3, "3 spots reached");
	expect(strcmp(ladx_logic_spot(&logic, 0)->name_id, "0x0A1") == 0 &&
		       strcmp(ladx_logic_spot(&logic, 1)->name_id, "0x0A2") == 0 &&
		       strcmp(ladx_logic_spot(&logic, 2)->name_id, "0x0B0") == 0,
	       "iteminfo_list by location, then add order");
	expect(ladx_logic_spot(&logic, 2)->location == 3, "a spot's location in final numbering");
	expect_text("loc 0 -\tA\nloc 1 -\tC\nloc 2 -\tD\nloc 3 -\tB\nloc 4 -\tE\n"
		    "spot 0 Chest 0x0A1 BOMB\tUnset Room\tNone\n"
		    "spot 0 Chest 0x0A2 BOMB\tUnset Room\tNone\n"
		    "spot 3 Chest 0x0B0 BOMB\tUnset Room\tNone\n"
		    "edge 0 1 -\nedge 0 3 -\nedge 1 2 -\nedge 2 3 -\nedge 3 4 -\nedge 3 0 -\n",
		    "edges renumbered in location_list order");
}

/* ladx_oracle.graph_lines: dungeons, unnamed locations, options, "" fields. */
static void test_renderer(void)
{
	int a, b, c, s;

	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "Start");
	b = ladx_logic_new_location(&logic, 3, NULL);
	c = ladx_logic_new_location(&logic, 0, "");
	ladx_logic_add_spot(&logic, a, LADX_SPOT_START_ITEM, "0x2A3", sword_shield, 2);
	ladx_logic_add_spot(&logic, b, LADX_SPOT_HEART_PIECE, "0x0B5", NULL, 0);
	ladx_logic_add_spot(&logic, c, LADX_SPOT_DUNGEON_CHEST, "0x2FF", sword_shield, 2);
	ladx_logic_connect(&logic, a, b, LADX_OR(A, I(FEATHER), LADX_AND(A, I(SWORD), ladx_req_count(A, LADX_ITEM_RUPEES, 200))));
	ladx_logic_connect_back(&logic, b, c, ladx_req_counts(A, 2, (const int[]){LADX_ITEM_SONG1, LADX_ITEM_SONG2}, 1),
				ladx_req_found(A, LADX_ITEM_KEY3, 2));
	ladx_logic_finish(&logic, a);
	s = 2;
	ladx_logic_spot_append_option(&logic, s, LADX_ITEM_KEY0);
	ladx_logic_spot_append_option(&logic, s, LADX_ITEM_MAP0);
	expect_text("loc 0 -\tStart\nloc 1 3\t-\nloc 2 0\t\"\"\n"
		    "spot 0 StartItem 0x2A3 SWORD,SHIELD\tUnset Room\tNone\n"
		    "spot 1 HeartPiece 0x0B5 -\tUnset Room\tNone\n"
		    "spot 2 DungeonChest 0x2FF SWORD,SHIELD,KEY0,MAP0\tUnset Room\tNone\n"
		    "edge 0 1 or(FEATHER,and(SWORD,count(RUPEES,200)))\n"
		    "edge 1 0 or(FEATHER,and(SWORD,count(RUPEES,200)))\n"
		    "edge 1 2 counts(1,SONG1,SONG2)\n"
		    "edge 2 1 found(KEY3,2)\n",
		    "the canonical text of a hand-built graph");
	ladx_logic_spot_append_option(&logic, 0, LADX_ITEM_BOMB);
	expect(!ladx_logic_broken(&logic), "a spot's first append may follow another spot's");
	ladx_logic_spot_append_option(&logic, s, LADX_ITEM_COMPASS0);
	expect(ladx_logic_broken(&logic), "an option appended out of spot order breaks the logic");
}

static LadxExplorer explorer;

static void expect_reached(const char *want, const char *what)
{
	bool member[LADX_LOGIC_LOCATION_CAP];
	char got[1024];
	int i;

	for (i = 0; i < ladx_logic_location_count(&logic); i++)
		member[i] = ladx_explorer_reached(&explorer, i);
	lf_index_ranges(TEST, member, ladx_logic_location_count(&logic), got, sizeof got);
	checks++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: reached %s, want %s", what, got, want);
}

static void expect_required(const char *want, const char *what)
{
	LadxItemSet set;
	char got[1024] = "";
	int i;

	ladx_explorer_required_next(&explorer, &logic, &set);
	for (i = 0; i < LADX_ITEM_COUNT; i++)
		if (set.has[i]) {
			if (got[0])
				strcat(got, ",");
			strcat(got, ladx_item_name(i));
		}
	checks++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: required %s, want %s", what, got, want);
}

/*
 * explorer.py: _visit (lines 31-42) recurses into a passable target at once,
 * picking up its items, so A's later edges see them; a failing edge waits in
 * the todo list, which _process (44-50) retries until no progress.
 */
static void test_explorer(void)
{
	int a, b, c, d, e, f, g, h;

	ladx_logic_begin(&logic);
	a = ladx_logic_new_location(&logic, -1, "A");
	b = ladx_logic_new_location(&logic, -1, "B");
	c = ladx_logic_new_location(&logic, -1, "C");
	d = ladx_logic_new_location(&logic, -1, "D");
	e = ladx_logic_new_location(&logic, -1, "E");
	f = ladx_logic_new_location(&logic, -1, "F");
	g = ladx_logic_new_location(&logic, -1, "G");
	h = ladx_logic_new_location(&logic, -1, "H");
	ladx_logic_add_spot(&logic, b, LADX_SPOT_CHEST, "0x0B0", bomb_only, 1);
	ladx_logic_add_spot(&logic, h, LADX_SPOT_CHEST, "0x0B1", bomb_only, 1);
	ladx_logic_single_connect(&logic, a, g, I(SHOVEL));
	ladx_logic_single_connect(&logic, a, b, I(SWORD));
	ladx_logic_single_connect(&logic, b, d, I(FEATHER));
	ladx_logic_single_connect(&logic, a, c, I(BOMB));
	ladx_logic_single_connect(&logic, a, e, ladx_req_count(A, LADX_ITEM_RUPEES, 100));
	ladx_logic_single_connect(&logic, a, f, LADX_OR(A, I(HOOKSHOT), LADX_AND(A, I(FLIPPERS), I(SWORD))));
	ladx_logic_single_connect(&logic, a, h, ladx_req_none());
	ladx_logic_finish(&logic, a);
	expect(!ladx_logic_broken(&logic), "the explorer's graph builds");
	/* Discovery: A G B D C E F H; spots B's, then H's. */
	expect(ladx_logic_spot(&logic, 0)->item == LADX_ITEM_NONE, "a spot holds no item after the build");
	expect(ladx_logic_place(&logic, 0, LADX_ITEM_BOMB), "BOMB placed on B's spot");
	expect(ladx_logic_place(&logic, 1, LADX_ITEM_SHOVEL), "SHOVEL placed on H's spot");
	expect(!ladx_logic_place(&logic, 1, LADX_ITEM_COUNT), "an unknown item is not placed");

	ladx_explorer_init(&explorer);
	ladx_explorer_add_item(&explorer, LADX_ITEM_SWORD, 1);
	ladx_explorer_add_item(&explorer, LADX_ITEM_RUPEES_50, 2);
	ladx_explorer_add_item(&explorer, LADX_ITEM_NONE, 1);
	expect(ladx_explorer_visit(&explorer, &logic, 0), "visit the start");
	expect_reached("0-2,4-5,7", "BOMB picked up on B opens C; SHOVEL from H opens G on _process");
	/* The set is shown in item order. */
	expect_required("HOOKSHOT,FEATHER,SWORD,FLIPPERS",
			"a bare item, then getItems of the failing OR (its AND's items, held or not)");
	expect(!ladx_explorer_visit(&explorer, &logic, 0), "a second visit of a location is refused");
	expect(!ladx_explorer_visit(&explorer, &logic, 99), "a location out of range is refused");

	ladx_explorer_init(&explorer);
	ladx_explorer_add_item(&explorer, LADX_ITEM_SWORD, 0);
	ladx_explorer_add_item(&explorer, LADX_ITEM_RUPEES_20, 4);
	ladx_explorer_add_item(&explorer, LADX_ITEM_RUPEES_50, 0);
	ladx_explorer_visit(&explorer, &logic, 0);
	expect_reached("0-2,4,7", "a zero-count SWORD passes; 80 rupees do not open E");
	ladx_explorer_add_item(&explorer, LADX_ITEM_RUPEES_20, 1);
	expect_reached("0-2,4,7", "an item added after a visit opens nothing by itself");
	expect(ladx_explorer_visit(&explorer, &logic, 5), "visit E, unreached, by hand");
	expect_reached("0-2,4-5,7", "a second visit adds its location");
}

/* A full capacity breaks the logic, which then builds nothing. */
static void test_capacity(void)
{
	int i, j;

	ladx_logic_begin(&logic);
	for (i = 0; i < LADX_LOGIC_LOCATION_CAP; i++)
		ladx_logic_new_location(&logic, -1, NULL);
	expect(!ladx_logic_broken(&logic), "the location capacity is usable whole");
	expect(ladx_logic_new_location(&logic, -1, NULL) == -1, "a location past the capacity is -1");
	expect(ladx_logic_broken(&logic), "a location past the capacity breaks the logic");
	expect(!ladx_logic_finish(&logic, 0), "a broken logic does not finish");
	expect(ladx_logic_location_count(&logic) == 0 && ladx_logic_location(&logic, 0) == NULL,
	       "a broken logic shows nothing");

	ladx_logic_begin(&logic);
	for (i = 0; i < LADX_LOGIC_LOCATION_CAP; i++)
		ladx_logic_new_location(&logic, -1, NULL);
	for (i = 0; i < LADX_LOGIC_LOCATION_CAP; i++) {
		ladx_logic_single_connect(&logic, i, (i + 1) % LADX_LOGIC_LOCATION_CAP, ladx_req_none());
		ladx_logic_single_connect(&logic, i, (i + 2) % LADX_LOGIC_LOCATION_CAP, ladx_req_none());
	}
	expect(!ladx_logic_broken(&logic), "the edge capacity is usable whole");
	ladx_logic_single_connect(&logic, 0, 1, I(SWORD));
	expect(!ladx_logic_broken(&logic), "a merge takes no new edge");
	ladx_logic_single_connect(&logic, 0, 3, ladx_req_none());
	expect(ladx_logic_broken(&logic), "an edge past the capacity breaks the logic");
	expect(ladx_logic_new_location(&logic, -1, NULL) == -1, "a broken logic adds no location");

	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	for (i = 0; i < LADX_LOGIC_SPOT_CAP; i++)
		ladx_logic_add_spot(&logic, 0, LADX_SPOT_CHEST, "0x000", NULL, 0);
	expect(ladx_logic_finish(&logic, 0), "the spot capacity is usable whole");
	for (i = 0; i < LADX_LOGIC_SPOT_CAP; i++)
		for (j = 0; j < LADX_LOGIC_OPTION_CAP / LADX_LOGIC_SPOT_CAP; j++)
			ladx_logic_spot_append_option(&logic, i, LADX_ITEM_BOMB);
	expect(!ladx_logic_broken(&logic) && LADX_LOGIC_OPTION_CAP % LADX_LOGIC_SPOT_CAP == 0,
	       "the option capacity is usable whole");
	ladx_logic_spot_append_option(&logic, LADX_LOGIC_SPOT_CAP - 1, LADX_ITEM_BOMB);
	expect(ladx_logic_broken(&logic), "an option past the capacity breaks the logic");

	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	expect(ladx_logic_add_spot(&logic, 0, LADX_SPOT_CHEST, "0x000", NULL, 0) == 0, "a spot handle");
	expect(ladx_logic_add_spot(&logic, 0, LADX_SPOT_CHEST, "", NULL, 0) == 1, "an empty name id");
	ladx_logic_add_spot(&logic, 0, LADX_SPOT_CHEST, "D6_THREE_WIZROBE_CLEAR_", NULL, 0);
	expect(!ladx_logic_broken(&logic), "a name id of LADX_SPOT_NAME_ID_MAX characters");
	ladx_logic_add_spot(&logic, 0, LADX_SPOT_CHEST, "D6_THREE_WIZROBE_CLEAR_X", NULL, 0);
	expect(ladx_logic_broken(&logic), "a longer name id breaks the logic");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_single_connect(&logic, 0, 1, ladx_req_none());
	expect(ladx_logic_broken(&logic), "an edge to an unknown location breaks the logic");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_finish(&logic, 0);
	ladx_logic_new_location(&logic, -1, NULL);
	expect(ladx_logic_broken(&logic), "building after finish breaks the logic");
}

/* worldSetup.py WorldSetup.__init__, lines 29-50. */
static void test_world_setup(void)
{
	LadxWorldSetup ws;
	LfWorldSetupRest rest;
	char lines[13][LF_WS_LINE_MAX];
	int i;

	ladx_world_setup_defaults(&ws);
	strcpy(rest.multichest, "RUPEES_20");
	strcpy(rest.sign_maze, "-");
	strcpy(rest.inside_to_outside, "1");
	strcpy(rest.keep_two_way, "1");
	strcpy(rest.one_on_one, "1");
	lf_ws_render(&ws, &rest, lines);
	expect(strcmp(lines[1], "ws boss_mapping 0,1,2,3,4,5,6,7,8") == 0, "bosses in place");
	expect(strcmp(lines[2], "ws miniboss_mapping 0=ROLLING_BONES,1=HINOX,2=DODONGO,3=CUE_BALL,4=GHOMA,"
				"5=SMASHER,6=GRIM_CREEPER,7=BLAINO,c1=AVALAUNCH,c2=GIANT_BUZZ_BLOB,"
				"moblin_cave=MOBLIN_KING,armos_temple=ARMOS_KNIGHT") == 0,
	       "the vanilla minibosses, in key order");
	expect(strcmp(lines[3], "ws goal vanilla") == 0 && strcmp(lines[4], "ws goal_count 8") == 0,
	       "goal vanilla, 8 instruments");
	expect(ladx_goal_find("seashells") == LADX_GOAL_SEASHELLS && ladx_goal_find("specific") == -1 &&
		       ladx_goal_name(LADX_GOAL_SPECIFIC) == NULL,
	       "goal names, SPECIFIC unnamed");
	expect(lf_ws_parse("goal", "=2357", &ws, &rest) && ws.goal == LADX_GOAL_SPECIFIC &&
		       strcmp(ws.goal_instruments, "2357") == 0,
	       "a specific goal keeps its digits");
	expect(!lf_ws_parse("goal", "=239", &ws, &rest) && !lf_ws_parse("goal", "bingo", &ws, &rest),
	       "an instrument 9 or an unknown goal is refused");
	expect(!lf_ws_parse("entrance_mapping", "a=b", &ws, &rest) && !lf_ws_parse("map", "x", &ws, &rest) &&
		       !lf_ws_parse("boss_mapping", "0,1,2,3,4,5,6,7", &ws, &rest) &&
		       !lf_ws_parse("miniboss_mapping", "c1=HINOX", &ws, &rest) &&
		       !lf_ws_parse("one_on_one", "2", &ws, &rest) && !lf_ws_parse("seed", "1", &ws, &rest),
	       "what tier 1 never gives is refused");
	for (i = 0; i < LADX_MINIBOSS_COUNT; i++)
		expect(ladx_miniboss_find(ladx_miniboss_name(i)) == i, "miniboss names round-trip");
}

#define VECTORS "oracle/logic_vectors.txt"

static LfConfig config;

/* Each k=v of the config line holds after validate; "" is the empty value. */
static void expect_settings_kept(const LfConfig *c)
{
	char buf[LF_SETTINGS_MAX];
	char *p = buf, *comma, *eq, *value;
	LadxSettingId id;

	strcpy(buf, c->settings_text);
	for (;;) {
		comma = strchr(p, ',');
		if (comma)
			*comma = '\0';
		eq = strchr(p, '=');
		if (!eq)
			test_fail(TEST, "config %s: malformed setting %s", c->name, p);
		*eq = '\0';
		value = strcmp(eq + 1, "\"\"") == 0 ? "" : eq + 1;
		id = ladx_setting_find(p);
		checks++;
		if (id == LADX_SETTING_COUNT || strcmp(ladx_setting_value_name(id, c->settings.value[id]), value) != 0)
			test_fail(TEST, "config %s: %s=%s not kept by validate", c->name, p, value);
		if (!comma)
			break;
		p = comma + 1;
	}
}

/* logic_vectors.txt: every configuration's settings parsed, validated and
 * supported, its ws lines read into LadxWorldSetup and rendered back. */
static void replay_vectors(void)
{
	FILE *f = lf_vectors_open(TEST, VECTORS);
	char lines[13][LF_WS_LINE_MAX];
	LadxSettingPair finding;
	int n = 0, pairs = 0, i;

	while (lf_vectors_next(TEST, f, &config)) {
		n++;
		expect_settings_kept(&config);
		for (i = 0; i < LADX_SETTING_COUNT; i++) {
			checks++;
			if (ladx_setting_value_reason((LadxSettingId)i, config.settings.value[i]) != LADX_REASON_NONE)
				test_fail(TEST, "config %s: %s=%s not supported", config.name,
					  ladx_setting_key((LadxSettingId)i),
					  ladx_setting_value_name((LadxSettingId)i, config.settings.value[i]));
		}
		/* The one pair is LADXR's fill refusing every seed: its logic builds. */
		if (!ladx_settings_supported(&config.settings, &finding)) {
			checks++;
			if (finding.b == LADX_SETTING_COUNT || finding.reason != LADX_REASON_LADXR_REFUSES)
				test_fail(TEST, "config %s: refused as %s", config.name, ladx_reason_id(finding.reason));
			pairs++;
		}
		lf_ws_render(&config.ws, &config.ws_rest, lines);
		for (i = 0; i < 13; i++) {
			checks++;
			if (strcmp(lines[i], config.ws_lines[i]) != 0)
				test_fail(TEST, "config %s: ws read back as %s, fixture %s", config.name, lines[i],
					  config.ws_lines[i]);
		}
	}
	fclose(f);
	expect(n == 425, "425 configurations in " VECTORS);
	printf("%s: %d configurations of %s read: settings kept by validate, every value supported alone "
	       "(%d refused only as a pair LADXR's fill refuses), %d ws lines read back unchanged\n",
	       TEST, n, VECTORS, pairs, 13 * n);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	test_single_connect();
	test_connect();
	test_discovery();
	test_renderer();
	test_explorer();
	test_capacity();
	test_world_setup();
	printf("%s: synthetic graphs, renderer and explorer ok (%d checks)\n", TEST, checks);
	if (fixtures)
		replay_vectors();
	return 0;
}
