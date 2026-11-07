#include "ladx_logic.h"
#include "ladx_logic_builder.h"
#include "ladx_sha512.h"
#include "logic_fixture.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_logic"

static int checks;
static LadxLogic logic;
static char text[LF_GRAPH_TEXT_MAX];

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
		    "spot 3 Chest 0x0B0 BOMB\tDigging Game\tKakariko Village\n"
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
		    "spot 0 StartItem 0x2A3 SWORD,SHIELD\tTarin's Gift\tMabe Village\n"
		    "spot 1 HeartPiece 0x0B5 -\tKey Cavern Keyhole\tUkuku Prairie\n"
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


#define GRAPHS "oracle/logic_graphs.txt"

static LfGraph graph;

/* The options' names joined by commas, "-" when none: graph_lines' form. */
static void options_text(int spot, char *out, size_t cap)
{
	int opts[256], n, i;
	size_t len = 0;

	n = ladx_logic_spot_options(&logic, spot, opts, 256);
	if (n > 256)
		test_fail(TEST, "spot %d: %d options", spot, n);
	out[0] = '\0';
	if (n == 0)
		snprintf(out, cap, "-");
	for (i = 0; i < n; i++) {
		len += (size_t)snprintf(out + len, cap - len, "%s%s", i ? "," : "", ladx_item_name(opts[i]));
		if (len >= cap)
			test_fail(TEST, "spot %d: options text over %lu bytes", spot, (unsigned long)cap);
	}
}

static int kind_find(const char *name)
{
	int k;

	for (k = 0; k < LADX_SPOT_KIND_COUNT; k++)
		if (strcmp(ladx_spot_kind_name(k), name) == 0)
			return k;
	return -1;
}

/*
 * The constructor arguments a spot line's name id gives back. Classes whose
 * __init__ passes its own room take none: AnglerKey, BeachSword, BirdKey,
 * FaceKey, FishingMinigame, SlimeKey, HookshotDrop, Witch, BoomerangGuy,
 * StartItem, TunicFairy(index), SeashellMansionBonus(index). KeyLocation
 * takes its key; KeyHole its default reward, its one option when
 * keyholesanity is off; TradeSequenceItem a default item it reads only when
 * tradequest is off, so any item when the line has every option.
 */
static void constructor_args(int kind, const char *name_id, const char *options, int *room, int *arg)
{
	static const int fixed[] = {
		LADX_SPOT_ANGLER_KEY, LADX_SPOT_BEACH_SWORD, LADX_SPOT_BIRD_KEY, LADX_SPOT_FACE_KEY,
		LADX_SPOT_FISHING_MINIGAME, LADX_SPOT_SLIME_KEY, LADX_SPOT_HOOKSHOT_DROP, LADX_SPOT_WITCH,
		LADX_SPOT_BOOMERANG_GUY, LADX_SPOT_START_ITEM, LADX_SPOT_TUNIC_FAIRY,
		LADX_SPOT_SEASHELL_MANSION_BONUS,
	};
	const char *dash = strchr(name_id, '-');
	unsigned i;

	*room = -1;
	*arg = -1;
	if (kind == LADX_SPOT_KEY_LOCATION) {
		*arg = ladx_item_find(name_id);
		return;
	}
	if (dash && (kind == LADX_SPOT_SHOP_ITEM || kind == LADX_SPOT_TUNIC_FAIRY ||
		     kind == LADX_SPOT_SEASHELL_MANSION_BONUS))
		*arg = atoi(dash + 1);
	if (kind == LADX_SPOT_KEY_HOLE)
		*arg = ladx_item_find(options);
	if (kind == LADX_SPOT_TRADE_SEQUENCE_ITEM)
		*arg = strchr(options, ',') ? LADX_ITEM_TRADING_ITEM_YOSHI_DOLL : ladx_item_find(options);
	for (i = 0; i < sizeof fixed / sizeof fixed[0]; i++)
		if (kind == fixed[i])
			return;
	*room = (int)strtol(name_id, NULL, 16);
}

#define GRAPH_SPOT_MAX 512
#define GRAPH_LOC_MAX 1024

/*
 * Each spot line of a baseline rebuilt from its class and name id (a chain
 * of the graph's locations with their dungeons, so discovery keeps the
 * order), configured under the baseline's settings: its name id, its
 * checkMetadata entry (KeyLocation's "Unset Room" included) and its options
 * equal the line's.
 */
static int replay_graph_spots(const LfGraph *g)
{
	static char want[GRAPH_SPOT_MAX][1280];
	static int dungeon[GRAPH_LOC_MAX];
	char kind_name[32], name_id[64], got[2048];
	char buf[1280];
	const char *line = g->text, *end;
	int locs = 0, spots = 0, i, kind, room, arg, loc, off;

	ladx_logic_begin(&logic);
	for (; *line; line = end + 1) {
		end = strchr(line, '\n');
		if ((size_t)(end - line) >= sizeof buf)
			test_fail(TEST, "graph %s: line too long", g->name);
		memcpy(buf, line, (size_t)(end - line));
		buf[end - line] = '\0';
		if (strncmp(buf, "loc ", 4) == 0) {
			char d[8];

			if (locs >= GRAPH_LOC_MAX || sscanf(buf, "loc %*d %7[^\t]", d) != 1)
				test_fail(TEST, "graph %s: malformed %s", g->name, buf);
			dungeon[locs] = strcmp(d, "-") == 0 ? -1 : atoi(d);
			ladx_logic_new_location(&logic, dungeon[locs], NULL);
			if (locs > 0)
				ladx_logic_single_connect(&logic, locs - 1, locs, ladx_req_none());
			locs++;
		} else if (strncmp(buf, "spot ", 5) == 0) {
			char options[1280];

			if (spots >= GRAPH_SPOT_MAX ||
			    sscanf(buf, "spot %d %31s %n%63s %1279[^\t]", &loc, kind_name, &off, name_id, options) != 4)
				test_fail(TEST, "graph %s: malformed %s", g->name, buf);
			kind = kind_find(kind_name);
			if (kind < 0 || loc < 0 || loc >= locs)
				test_fail(TEST, "graph %s: unknown class or location in %s", g->name, buf);
			constructor_args(kind, name_id, options, &room, &arg);
			ladx_logic_add_item_info(&logic, loc, (LadxSpotKind)kind, room, arg);
			memcpy(want[spots++], buf + off, strlen(buf + off) + 1);
		}
	}
	ladx_logic_finish(&logic, 0);
	ladx_logic_configure(&logic, &g->settings);
	expect(!ladx_logic_broken(&logic), "the spots of a baseline build and configure");
	expect(ladx_logic_spot_count(&logic) == spots, "every spot line rebuilt");
	for (i = 0; i < spots; i++) {
		const LadxSpot *sp = ladx_logic_spot(&logic, i);
		char options[1200];

		options_text(i, options, sizeof options);
		snprintf(got, sizeof got, "%s %s\t%s\t%s", sp->name_id, options, ladx_spot_metadata_name(sp),
			 ladx_spot_metadata_area(sp));
		checks++;
		if (strcmp(got, want[i]) != 0)
			test_fail(TEST, "graph %s: spot %d of class %s: got\n%s\nwant\n%s", g->name, i,
				  ladx_spot_kind_name(sp->kind), got, want[i]);
	}
	return spots;
}

static void test_graph_spots(void)
{
	FILE *f = lf_graphs_open(TEST, GRAPHS);
	int graphs = 0, spots = 0;

	while (lf_graphs_next(TEST, f, &graph)) {
		graphs++;
		spots += replay_graph_spots(&graph);
	}
	fclose(f);
	expect(graphs == 5 && spots == 1334, "1334 spot lines in the 5 graphs of " GRAPHS);
	printf("%s: %d spot lines of %d graphs of %s rebuilt from class and name id: name id, metadata and "
	       "options after configure equal\n",
	       TEST, spots, graphs, GRAPHS);
}

#define DUNGEON_FIRST 0
#define DUNGEON_LAST 8

/*
 * Each dungeon built alone (ladx_logic_build_dungeon) under each
 * baseline's settings and WorldSetup()'s defaults, which every baseline's
 * ws lines equal (checked with --fixtures), equal to the dungeon in the
 * fixture's whole graph: lf_dungeon_compare's walk from "D<n> Entrance".
 */
static void test_dungeons(void)
{
	FILE *f = lf_graphs_open(TEST, GRAPHS);
	LadxWorldSetup ws;
	int graphs = 0, walked = 0, n;
	size_t len;

	ladx_world_setup_defaults(&ws);
	while (lf_graphs_next(TEST, f, &graph)) {
		graphs++;
		for (n = DUNGEON_FIRST; n <= DUNGEON_LAST; n++) {
			checks++;
			if (ladx_logic_build_dungeon(&logic, &graph.settings, &ws, n) != LADX_LOGIC_OK)
				test_fail(TEST, "dungeon D%d, graph %s: the build fails", n, graph.name);
			len = lf_graph_text(TEST, &logic, text, sizeof text);
			walked += lf_dungeon_compare(TEST, graph.name, n, graph.text, graph.len, text, len);
		}
	}
	fclose(f);
	expect(graphs == 5, "5 graphs in " GRAPHS);
	printf("%s: D%d to D%d built alone equal to the %d graphs of %s (%d dungeon walks, %d locations)\n", TEST,
	       DUNGEON_FIRST, DUNGEON_LAST, graphs, GRAPHS, graphs * (DUNGEON_LAST - DUNGEON_FIRST + 1), walked);
}

/* Settings "k=v,..." on defaults, not validated: configure reads them raw. */
static void settings_from(const char *text, LadxSettings *settings)
{
	char buf[256], *p = buf, *comma;

	ladx_settings_defaults(settings);
	snprintf(buf, sizeof buf, "%s", text);
	while (*p) {
		comma = strchr(p, ',');
		if (comma)
			*comma = '\0';
		if (ladx_settings_set(settings, p) != LADX_SETTINGS_OK)
			test_fail(TEST, "synthetic setting %s refused", p);
		if (!comma)
			break;
		p = comma + 1;
	}
}

#define MISMATCH_DIR "build/logic"

/* C's canonical text of a graph that differs, for a diff against LADXR's. */
static void write_mismatch(const char *name, const char *got, size_t len, char *path, size_t cap)
{
	FILE *f;
	char *p;

	snprintf(path, cap, MISMATCH_DIR "/%s.txt", name);
	for (p = path + strlen(MISMATCH_DIR) + 1; *p; p++)
		if (*p == '/')
			*p = '+';
	f = fopen(path, "w");
	if (!f || fwrite(got, 1, len, f) != len || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", path);
}

/*
 * ladx_logic_build (main.py Logic.__init__: World, the nine dungeons, the
 * entrance loop on the identity mapping, the egg, the removed dungeon
 * items, discovery, configure) under each baseline's settings and
 * WorldSetup()'s defaults: its canonical text equal to the fixture's, line
 * for line.
 */
static void test_baselines(void)
{
	FILE *f = lf_graphs_open(TEST, GRAPHS);
	char path[LF_NAME_MAX + 32];
	LadxWorldSetup ws;
	int graphs = 0, lines = 0, line;
	size_t len, i;

	ladx_world_setup_defaults(&ws);
	while (lf_graphs_next(TEST, f, &graph)) {
		graphs++;
		checks++;
		if (ladx_logic_build(&logic, &graph.settings, &ws) != LADX_LOGIC_OK)
			test_fail(TEST, "graph %s: the build fails", graph.name);
		len = lf_graph_text(TEST, &logic, text, sizeof text);
		if (len != graph.len || memcmp(text, graph.text, len) != 0) {
			for (i = 0, line = 1; i < len && i < graph.len && text[i] == graph.text[i]; i++)
				line += text[i] == '\n';
			write_mismatch(graph.name, text, len, path, sizeof path);
			test_fail(TEST,
				  "graph %s: differs from line %d of its text; C's text in %s, LADXR's from python3 "
				  "tests/ladx/oracle/extract_logic.py --graph '%s' --out tmp/ladx-logic "
				  "tests/ladx/oracle/ladxr",
				  graph.name, line, path, graph.name);
		}
		for (i = 0; i < len; i++)
			lines += text[i] == '\n';
	}
	fclose(f);
	expect(graphs == 5, "5 graphs in " GRAPHS);
	printf("%s: the %d graphs of %s built whole (ladx_logic_build) line-identical (%d lines)\n", TEST, graphs,
	       GRAPHS, lines);
}

/* The world's named locations and the build's refusals. */
static void test_build(void)
{
	LadxSettings settings;
	LadxWorldSetup ws;
	const LadxLocation *loc;

	ladx_world_setup_defaults(&ws);
	settings_from("", &settings);
	ladx_settings_validate(&settings);
	expect(ladx_logic_build(&logic, &settings, &ws) == LADX_LOGIC_OK, "the defaults build");
	loc = ladx_logic_location(&logic, ladx_logic_start(&logic));
	expect(loc && loc->name && strcmp(loc->name, "Start House") == 0, "the start is the Start House");
	loc = ladx_logic_location(&logic, ladx_logic_windfish(&logic));
	expect(loc && loc->name && strcmp(loc->name, "Windfish") == 0, "Logic.windfish");
	loc = ladx_logic_location(&logic, ladx_logic_egg(&logic));
	expect(loc && loc->name && strcmp(loc->name, "Outside the Egg") == 0, "World.egg");
	loc = ladx_logic_location(&logic, ladx_logic_nightmare(&logic));
	expect(loc && loc->name && strcmp(loc->name, "Nightmare") == 0, "World.nightmare");

	settings_from("overworld=random", &settings);
	ladx_settings_validate(&settings);
	expect(ladx_logic_build(&logic, &settings, &ws) == LADX_LOGIC_UNSUPPORTED, "an unsupported value refused");
	expect(ladx_logic_broken(&logic) && ladx_logic_location_count(&logic) == 0 &&
		       ladx_logic_windfish(&logic) == -1,
	       "nothing built when refused");
	settings_from("boomerang=default,accessibility=all", &settings);
	ladx_settings_validate(&settings);
	expect(ladx_logic_build(&logic, &settings, &ws) == LADX_LOGIC_OK,
	       "the pair LADXR's fill refuses still builds its logic");
	settings_from("", &settings);
	ladx_settings_validate(&settings);
	ws.boss_mapping[4] = 9;
	expect(ladx_logic_build(&logic, &settings, &ws) == LADX_LOGIC_BROKEN, "a boss mapping out of range");
}

/*
 * One spot of the class, in a location of the dungeon, configured under the
 * settings: its options' text equals want, where a leading "*" stands for
 * the class's OPTIONS (the spot's options before configure).
 */
static void expect_configure(LadxSpotKind kind, int room, int arg, int dungeon, const char *settings_text,
			     const char *want, const char *what)
{
	char before[4096], after[4096], full[8192];
	LadxSettings settings;

	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, dungeon, NULL);
	ladx_logic_add_item_info(&logic, 0, kind, room, arg);
	ladx_logic_finish(&logic, 0);
	expect(!ladx_logic_broken(&logic), what);
	options_text(0, before, sizeof before);
	settings_from(settings_text, &settings);
	ladx_logic_configure(&logic, &settings);
	expect(!ladx_logic_broken(&logic), what);
	options_text(0, after, sizeof after);
	if (want[0] == '*')
		snprintf(full, sizeof full, "%s%s", before, want + 1);
	else
		snprintf(full, sizeof full, "%s", want);
	checks++;
	if (strcmp(after, full) != 0)
		test_fail(TEST, "%s %s: got\n%s\nwant\n%s", ladx_spot_kind_name(kind), what, after, full);
}

static void expect_configure_breaks(LadxSpotKind kind, int room, int arg, int dungeon, const char *settings_text,
				    const char *what)
{
	LadxSettings settings;

	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, dungeon, NULL);
	ladx_logic_add_item_info(&logic, 0, kind, room, arg);
	ladx_logic_finish(&logic, 0);
	settings_from(settings_text, &settings);
	ladx_logic_configure(&logic, &settings);
	expect(ladx_logic_broken(&logic), what);
}

#define D3_ITEMS ",KEY3,NIGHTMARE_KEY3,STONE_BEAK3,MAP3,COMPASS3"
#define ALL_KEYSANITY                                                                                        \
	",KEY0,NIGHTMARE_KEY0,STONE_BEAK0,MAP0,COMPASS0,KEY1,NIGHTMARE_KEY1,STONE_BEAK1,MAP1,COMPASS1,"      \
	"KEY2,NIGHTMARE_KEY2,STONE_BEAK2,MAP2,COMPASS2,KEY3,NIGHTMARE_KEY3,STONE_BEAK3,MAP3,COMPASS3,"       \
	"KEY4,NIGHTMARE_KEY4,STONE_BEAK4,MAP4,COMPASS4,KEY5,NIGHTMARE_KEY5,STONE_BEAK5,MAP5,COMPASS5,"       \
	"KEY6,NIGHTMARE_KEY6,STONE_BEAK6,MAP6,COMPASS6,KEY7,NIGHTMARE_KEY7,STONE_BEAK7,MAP7,COMPASS7,"       \
	"KEY8,NIGHTMARE_KEY8,STONE_BEAK8,MAP8,COMPASS8,KEY9,NIGHTMARE_KEY9,STONE_BEAK9,MAP9,COMPASS9"
#define KEYSANITY_KEYS ",KEY0,KEY1,KEY2,KEY3,KEY4,KEY5,KEY6,KEY7,KEY8,KEY9"
#define GIFT_OPTIONS                                                                                         \
	"POWER_BRACELET,SHIELD,BOW,HOOKSHOT,MAGIC_ROD,PEGASUS_BOOTS,OCARINA,FEATHER,SHOVEL,MAGIC_POWDER,BOMB,"   \
	"SWORD,FLIPPERS,MAGNIFYING_LENS,MEDICINE,TAIL_KEY,ANGLER_KEY,FACE_KEY,BIRD_KEY,GOLD_LEAF,SLIME_KEY,"     \
	"ROOSTER,HAMMER,RUPEES_50,RUPEES_20,RUPEES_100,RUPEES_200,RUPEES_500,SEASHELL,BOOMERANG,HEART_PIECE,"    \
	"BOWWOW,ARROWS_10,SINGLE_ARROW,MAX_POWDER_UPGRADE,MAX_BOMBS_UPGRADE,MAX_ARROWS_UPGRADE,RED_TUNIC,"      \
	"BLUE_TUNIC,HEART_CONTAINER,BAD_HEART_CONTAINER,TOADSTOOL"

/* Each class's configure, as locations/ writes it (file and lines cited). */
static int location_named(const char *name)
{
	int i;

	for (i = 0; i < ladx_logic_location_count(&logic); i++)
		if (ladx_logic_location(&logic, i)->name && strcmp(ladx_logic_location(&logic, i)->name, name) == 0)
			return i;
	test_fail(TEST, "no location %s", name);
	return -1;
}

/* The requirement text of from's connection to to. */
static void edge_text(const char *from, const char *to, char *out, size_t cap)
{
	const LadxLocation *loc = ladx_logic_location(&logic, location_named(from));
	int target = location_named(to), e;

	for (e = loc->edge_first; e < loc->edge_first + loc->edge_count; e++)
		if (ladx_logic_edge(&logic, e)->target == target) {
			out[0] = '\0';
			lf_requirement_text(TEST, ladx_logic_arena(&logic), ladx_logic_edge(&logic, e)->req, out, cap);
			return;
		}
	test_fail(TEST, "no edge %s -> %s", from, to);
}

static LadxLogicStatus build_dungeon(const char *settings_text, const LadxWorldSetup *ws, int n)
{
	LadxSettings settings;

	settings_from(settings_text, &settings);
	ladx_settings_validate(&settings);
	return ladx_logic_build_dungeon(&logic, &settings, ws, n);
}

/* The name ids of D3's spots with a forced item, each KEY3. */
static void forced_text(char *out, size_t cap)
{
	int i;

	out[0] = '\0';
	for (i = 0; i < ladx_logic_spot_count(&logic); i++) {
		const LadxSpot *sp = ladx_logic_spot(&logic, i);

		if (sp->forced_item == LADX_ITEM_NONE)
			continue;
		snprintf(out + strlen(out), cap - strlen(out), "%s%s=%s", out[0] ? "," : "", sp->name_id,
			 ladx_item_name(sp->forced_item));
	}
}

/* The requirement text of from's only edge. */
static void only_edge_text(const char *from, char *out, size_t cap)
{
	const LadxLocation *loc = ladx_logic_location(&logic, location_named(from));

	if (loc->edge_count != 1)
		test_fail(TEST, "%s has %d edges, want 1", from, loc->edge_count);
	out[0] = '\0';
	lf_requirement_text(TEST, ladx_logic_arena(&logic), ladx_logic_edge(&logic, loc->edge_first)->req, out, cap);
}

/* "<location name or ->:<class> <name id>" of the spots outside any
 * dungeon, in iteminfo_list order, and the names of the locations outside. */
static void outside_text(char *out, size_t cap)
{
	int i;

	out[0] = '\0';
	for (i = 0; i < ladx_logic_location_count(&logic); i++) {
		const LadxLocation *loc = ladx_logic_location(&logic, i);

		if (loc->dungeon == -1)
			snprintf(out + strlen(out), cap - strlen(out), "%s%s", out[0] ? "," : "",
				 loc->name ? loc->name : "-");
	}
	for (i = 0; i < ladx_logic_spot_count(&logic); i++) {
		const LadxSpot *sp = ladx_logic_spot(&logic, i);

		if (ladx_logic_location(&logic, sp->location)->dungeon == -1)
			snprintf(out + strlen(out), cap - strlen(out), ",%s %s", ladx_spot_kind_name(sp->kind),
				 sp->name_id);
	}
}

/*
 * ladx_logic_build_dungeon beyond the baselines: the world setup read
 * (dungeon1.py lines 60-63: miniboss_mapping[0], boss_mapping[0];
 * dungeonColor.py lines 54-55 and 63: miniboss_mapping["c2"], ["c1"],
 * boss_mapping[8]), the forced keys (dungeon3.py lines 131-135, dungeon7.py
 * lines 159-160: only when dungeon_keys is ""), the locations outside the
 * dungeon the walk skips (dungeon6.py lines 43 and 82-84: the raft island
 * and its Chest(0x06C); dungeon8.py lines 77 and 93-96: the Tal Tal
 * connector and its HeartPiece(0x000)), and the refusals.
 */
static void test_dungeon_build(void)
{
	LadxWorldSetup ws;
	char got[256];

	ladx_world_setup_defaults(&ws);
	ws.miniboss_mapping[0] = LADX_MINIBOSS_DODONGO;
	ws.boss_mapping[0] = 5;
	expect(build_dungeon("", &ws, 1) == LADX_LOGIC_OK, "D1 builds under a shuffled world setup");
	edge_text("D1 Miniboss", "D1 After Miniboss", got, sizeof got);
	expect(strcmp(got, "BOMB") == 0, "D1's miniboss edge reads miniboss_mapping[0] (DODONGO: BOMB)");
	edge_text("D1 Boss Room", "D1 After Miniboss", got, sizeof got);
	expect(strcmp(got, "BOMB") == 0, "D1's boss back edge reads boss_mapping[0] (5: BOMB)");

	ladx_world_setup_defaults(&ws);
	expect(build_dungeon("", &ws, 3) == LADX_LOGIC_OK, "D3 builds");
	forced_text(got, sizeof got);
	expect(strcmp(got, "0x158=KEY3,0x155=KEY3") == 0,
	       "D3 forces KEY3 on its south and west key drops, in iteminfo_list order");
	expect(build_dungeon("dungeon_keys=keysanity", &ws, 3) == LADX_LOGIC_OK, "D3 builds under keysanity");
	forced_text(got, sizeof got);
	expect(strcmp(got, "") == 0, "D3 forces nothing under keysanity");

	expect(build_dungeon("", &ws, 7) == LADX_LOGIC_OK, "D7 builds");
	forced_text(got, sizeof got);
	expect(strcmp(got, "0x210=KEY7") == 0, "D7 forces KEY7 on its entrance key drop");
	expect(build_dungeon("dungeon_keys=keysanity", &ws, 7) == LADX_LOGIC_OK, "D7 builds under keysanity");
	forced_text(got, sizeof got);
	expect(strcmp(got, "") == 0, "D7 forces nothing under keysanity");

	ws.miniboss_mapping[8] = LADX_MINIBOSS_DODONGO;
	ws.miniboss_mapping[9] = LADX_MINIBOSS_CUE_BALL;
	ws.boss_mapping[8] = 5;
	expect(build_dungeon("", &ws, 0) == LADX_LOGIC_OK, "D0 builds under a shuffled world setup");
	edge_text("D0 Center Miniboss", "D0 Hidden Button Room", got, sizeof got);
	expect(strcmp(got, "BOMB") == 0, "D0's centre miniboss reads miniboss_mapping[\"c1\"] (DODONGO: BOMB)");
	only_edge_text("D0 NE Miniboss", got, sizeof got);
	expect(strcmp(got, "SWORD") == 0, "D0's NE miniboss reads miniboss_mapping[\"c2\"] (CUE_BALL: SWORD)");
	edge_text("D0 Boss Room", "D0 Fairy Room", got, sizeof got);
	expect(strcmp(got, "BOMB") == 0, "D0's boss reads boss_mapping[8] (5: BOMB)");
	ladx_world_setup_defaults(&ws);

	expect(build_dungeon("", &ws, 6) == LADX_LOGIC_OK, "D6 builds");
	outside_text(got, sizeof got);
	expect(strcmp(got, "D6 Raft Ride Island,-,Chest 0x06C") == 0,
	       "D6's raft island and its chest outside the dungeon");
	expect(build_dungeon("", &ws, 8) == LADX_LOGIC_OK, "D8 builds");
	outside_text(got, sizeof got);
	expect(strcmp(got, "D8 Tal Tal Connector,-,HeartPiece 0x000") == 0,
	       "D8's Tal Tal connector and its heart piece 0x000 outside the dungeon");

	expect(build_dungeon("boomerang=default,accessibility=all", &ws, 1) == LADX_LOGIC_OK,
	       "the pair LADXR's fill refuses still builds its logic");
	expect(build_dungeon("overworld=random", &ws, 1) == LADX_LOGIC_UNSUPPORTED, "an unsupported value refused");
	expect(ladx_logic_broken(&logic) && ladx_logic_location_count(&logic) == 0, "nothing built when refused");
	expect(build_dungeon("", &ws, 9) == LADX_LOGIC_BROKEN && build_dungeon("", &ws, -1) == LADX_LOGIC_BROKEN,
	       "a dungeon out of range");
	ws.boss_mapping[2] = 9;
	expect(build_dungeon("", &ws, 1) == LADX_LOGIC_BROKEN, "a boss mapping out of range");
}

static void test_configure(void)
{
	/* itemInfo.py configure, lines 28-48: the range(10) loop, then the
	 * dungeon's own items for each setting not keysanity. */
	expect_configure(LADX_SPOT_DUNGEON_CHEST, 0x108, -1, 3, "", "*" D3_ITEMS, "in D3, default dungeon items");
	expect_configure(LADX_SPOT_DUNGEON_CHEST, 0x108, -1, 3, "dungeon_keys=keysanity",
			 "*" KEYSANITY_KEYS ",NIGHTMARE_KEY3,STONE_BEAK3,MAP3,COMPASS3", "in D3, keysanity keys");
	expect_configure(LADX_SPOT_DUNGEON_CHEST, 0x108, -1, 3, "dungeon_keys=removed", "*" D3_ITEMS,
			 "in D3, removed keys still appended (only keysanity is read)");
	expect_configure(LADX_SPOT_DUNGEON_CHEST, 0x108, -1, 3,
			 "dungeon_keys=keysanity,nightmare_keys=keysanity,dungeon_beaks=keysanity,dungeon_maps=keysanity",
			 "*" ALL_KEYSANITY, "in D3, every family keysanity, interleaved per n");
	expect_configure(LADX_SPOT_DUNGEON_CHEST, 0x2FF, -1, 0, "",
			 "*,KEY0,NIGHTMARE_KEY0,STONE_BEAK0,MAP0,COMPASS0", "in the colour dungeon");
	expect_configure(LADX_SPOT_CHEST, 0x0B0, -1, -1, "", "*", "outside a dungeon, nothing appended");
	expect_configure(LADX_SPOT_CHEST, 0x0B0, -1, -1, "dungeon_keys=keysanity", "*" KEYSANITY_KEYS,
			 "outside a dungeon, keysanity keys appended");

	/* heartPiece.py and fishingMinigame.py configure: heartpiece. */
	expect_configure(LADX_SPOT_HEART_PIECE, 0x044, -1, -1, "heartpiece=1", "*", "heartpiece on");
	expect_configure(LADX_SPOT_HEART_PIECE, 0x044, -1, -1, "heartpiece=0", "HEART_PIECE", "heartpiece off");
	expect_configure(LADX_SPOT_FISHING_MINIGAME, -1, -1, -1, "heartpiece=0", "HEART_PIECE", "heartpiece off");
	/* seashell.py Seashell and SeashellMansionBonus configure: seashells. */
	expect_configure(LADX_SPOT_SEASHELL, 0x0A3, -1, -1, "seashells=1,dungeon_keys=keysanity", "*" KEYSANITY_KEYS,
			 "seashells on");
	expect_configure(LADX_SPOT_SEASHELL, 0x0A3, -1, -1, "seashells=0,dungeon_keys=keysanity", "SEASHELL",
			 "seashells off, no append");
	expect_configure(LADX_SPOT_SEASHELL_MANSION_BONUS, -1, 1, -1, "seashells=0", "SEASHELL", "seashells off");
	/* heartContainer.py configure: heartcontainers, then hpmode. */
	expect_configure(LADX_SPOT_HEART_CONTAINER, 0x106, -1, 1, "heartcontainers=1",
			 "*,KEY1,NIGHTMARE_KEY1,STONE_BEAK1,MAP1,COMPASS1", "heartcontainers on");
	expect_configure(LADX_SPOT_HEART_CONTAINER, 0x106, -1, 1, "heartcontainers=0,hpmode=5hit",
			 "*,KEY1,NIGHTMARE_KEY1,STONE_BEAK1,MAP1,COMPASS1", "off, hpmode 5hit");
	expect_configure(LADX_SPOT_HEART_CONTAINER, 0x106, -1, 1, "heartcontainers=0,hpmode=inverted",
			 "BAD_HEART_CONTAINER", "off, hpmode inverted");
	expect_configure(LADX_SPOT_HEART_CONTAINER, 0x106, -1, 1, "heartcontainers=0,hpmode=low", "HEART_PIECE",
			 "off, hpmode low");
	expect_configure(LADX_SPOT_HEART_CONTAINER, 0x106, -1, 1, "heartcontainers=0,hpmode=1", "HEART_CONTAINER",
			 "off, hpmode 1");
	expect_configure(LADX_SPOT_HEART_CONTAINER, 0x106, -1, 1, "heartcontainers=0", "HEART_CONTAINER",
			 "off, hpmode default");
	/* witch.py and toadstool.py configure: witch. */
	expect_configure(LADX_SPOT_WITCH, -1, -1, -1, "witch=1", "*", "witch on");
	expect_configure(LADX_SPOT_WITCH, -1, -1, -1, "witch=0", "MAGIC_POWDER", "witch off");
	expect_configure(LADX_SPOT_TOADSTOOL, -1, -1, -1, "witch=1", "*", "witch on");
	expect_configure(LADX_SPOT_TOADSTOOL, 0x011, -1, -1, "witch=0", "TOADSTOOL", "witch off");
	/* instrument.py configure: instruments, or the seashells goal. */
	expect_configure(LADX_SPOT_INSTRUMENT, 0x1B5, -1, 5, "instruments=0", "INSTRUMENT5", "instruments off");
	expect_configure(LADX_SPOT_INSTRUMENT, 0x1B5, -1, 5, "instruments=1",
			 "*,KEY5,NIGHTMARE_KEY5,STONE_BEAK5,MAP5,COMPASS5", "instruments on");
	expect_configure(LADX_SPOT_INSTRUMENT, 0x1B5, -1, 5, "instruments=0,goal=seashells",
			 "*,KEY5,NIGHTMARE_KEY5,STONE_BEAK5,MAP5,COMPASS5", "off, goal seashells");
	expect_configure_breaks(LADX_SPOT_INSTRUMENT, 0x1B5, -1, 0, "instruments=0",
				"an Instrument in D0 breaks the logic (no INSTRUMENT0)");
	/* keyhole.py configure: keyholesanity, else the default reward. */
	expect_configure(LADX_SPOT_KEY_HOLE, 0x0D3, LADX_ITEM_TAIL_CAVE_OPENED, -1, "keyholesanity=0",
			 "TAIL_CAVE_OPENED", "keyholesanity off");
	expect_configure(LADX_SPOT_KEY_HOLE, 0x0D3, LADX_ITEM_TAIL_CAVE_OPENED, -1, "keyholesanity=1", "*",
			 "keyholesanity on");
	/* tradeSequence.py configure: tradequest, else the default item. */
	expect_configure(LADX_SPOT_TRADE_SEQUENCE_ITEM, 0x2A0, LADX_ITEM_TRADING_ITEM_YOSHI_DOLL, -1, "tradequest=0",
			 "TRADING_ITEM_YOSHI_DOLL", "tradequest off");
	expect_configure(LADX_SPOT_TRADE_SEQUENCE_ITEM, 0x2A0, LADX_ITEM_TRADING_ITEM_YOSHI_DOLL, -1, "tradequest=1",
			 "*", "tradequest on");
	/* boomerangGuy.py configure: gift replaces the list then appends; the
	 * other values skip ItemInfo.configure. */
	expect_configure(LADX_SPOT_BOOMERANG_GUY, -1, -1, -1, "boomerang=gift,dungeon_keys=keysanity",
			 GIFT_OPTIONS KEYSANITY_KEYS, "boomerang gift");
	expect_configure(LADX_SPOT_BOOMERANG_GUY, -1, -1, -1, "boomerang=trade,dungeon_keys=keysanity",
			 "BOOMERANG,HOOKSHOT,MAGIC_ROD,PEGASUS_BOOTS,FEATHER,SHOVEL", "boomerang trade, no append");
	/* startItem.py configure: bowwow; never ItemInfo.configure. */
	expect_configure(LADX_SPOT_START_ITEM, -1, -1, -1, "bowwow=normal,dungeon_keys=keysanity",
			 "SWORD,SHIELD,POWER_BRACELET,OCARINA,FEATHER,BOOMERANG,MAGIC_ROD,TAIL_KEY,SHOVEL,HOOKSHOT,"
			 "PEGASUS_BOOTS,MAGIC_POWDER,BOMB",
			 "bowwow normal, no append");
	expect_configure(LADX_SPOT_START_ITEM, -1, -1, -1, "bowwow=swordless", "SWORD", "bowwow swordless");
	/* shop.py configure: ItemInfo.configure; its shopsanity branch is not ported. */
	expect_configure(LADX_SPOT_SHOP_ITEM, -1, 1, -1, "dungeon_maps=keysanity",
			 "*,MAP0,COMPASS0,MAP1,COMPASS1,MAP2,COMPASS2,MAP3,COMPASS3,MAP4,COMPASS4,MAP5,COMPASS5,"
			 "MAP6,COMPASS6,MAP7,COMPASS7,MAP8,COMPASS8,MAP9,COMPASS9",
			 "keysanity maps");
	expect_configure_breaks(LADX_SPOT_SHOP_ITEM, -1, 0, -1, "shopsanity=important", "shopsanity breaks the logic");
	/* owlStatue.py configure: ItemInfo.configure, then a RuntimeError when
	 * the statue's side is not enabled. */
	expect_configure(LADX_SPOT_OWL_STATUE, 0x103, -1, 1, "owlstatues=dungeon",
			 "*,KEY1,NIGHTMARE_KEY1,STONE_BEAK1,MAP1,COMPASS1", "a dungeon owl, owlstatues dungeon");
	expect_configure(LADX_SPOT_OWL_STATUE, 0x035, -1, -1, "owlstatues=overworld", "*",
			 "an overworld owl, owlstatues overworld");
	expect_configure(LADX_SPOT_OWL_STATUE, 0x035, -1, -1, "owlstatues=both", "*", "an overworld owl, both");
	expect_configure_breaks(LADX_SPOT_OWL_STATUE, 0x035, -1, -1, "owlstatues=dungeon",
				"an overworld owl under owlstatues dungeon breaks the logic");
	expect_configure_breaks(LADX_SPOT_OWL_STATUE, 0x103, -1, 1, "owlstatues=",
				"an owl with owlstatues off breaks the logic");
	/* keyLocation.py configure: pass, under any settings. */
	expect_configure(LADX_SPOT_KEY_LOCATION, -1, LADX_ITEM_D3_BOMBWALL, 3, "dungeon_keys=keysanity", "D3_BOMBWALL",
			 "never changed");
}

/* locations/'s constructors: nameId, metadata, data, and refused arguments. */
static void test_item_info(void)
{
	const LadxSpot *sp;
	LadxSettings settings;

	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, 3, NULL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_KEY_LOCATION, -1, LADX_ITEM_D3_BOMBWALL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_SHOP_ITEM, 0x2CB, 1);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_TOADSTOOL, -1, -1);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_TUNIC_FAIRY, -1, 1);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_OWL_STATUE, 0x103, -1);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_TRADE_SEQUENCE_ITEM, 0x297,
				 LADX_ITEM_TRADING_ITEM_MAGNIFYING_GLASS);
	ladx_logic_finish(&logic, 0);
	expect(!ladx_logic_broken(&logic), "constructors of six classes");
	/* keyLocation.py lines 6-8 and 20-22: the key is the name id, read as
	 * "None" by ItemInfo.__init__'s metadata lookup. */
	sp = ladx_logic_spot(&logic, 0);
	expect(strcmp(sp->name_id, "D3_BOMBWALL") == 0 && strcmp(ladx_spot_metadata_name(sp), "Unset Room") == 0 &&
		       strcmp(ladx_spot_metadata_area(sp), "None") == 0 && sp->room == -1 &&
		       sp->default_item == LADX_ITEM_D3_BOMBWALL,
	       "KeyLocation: the key's name id, the None metadata");
	sp = ladx_logic_spot(&logic, 1);
	expect(strcmp(sp->name_id, "0x2CB-1") == 0 && strcmp(ladx_spot_metadata_name(sp), "Mabe Extra Shop Item 2") == 0 &&
		       sp->room == 0x2CB && sp->index == 1,
	       "ShopItem(1, room=0x2CB): its index in the name id");
	sp = ladx_logic_spot(&logic, 2);
	expect(strcmp(sp->name_id, "0x050") == 0 && sp->room == 0x050, "Toadstool(): room 0x050 by default");
	sp = ladx_logic_spot(&logic, 3);
	expect(strcmp(sp->name_id, "0x301-1") == 0 && strcmp(ladx_spot_metadata_name(sp), "Tunic Fairy Item 2") == 0,
	       "TunicFairy(1): index set before ItemInfo.__init__");
	sp = ladx_logic_spot(&logic, 4);
	expect(strcmp(sp->name_id, "0x103-Owl") == 0 && strcmp(ladx_spot_metadata_name(sp), "Spiked Beetle Owl") == 0,
	       "OwlStatue: 0x%03X-Owl");
	sp = ladx_logic_spot(&logic, 5);
	expect(strcmp(sp->name_id, "0x297-Trade") == 0 && strcmp(ladx_spot_metadata_name(sp), "Mermaid Statue") == 0 &&
		       sp->default_item == LADX_ITEM_TRADING_ITEM_MAGNIFYING_GLASS && sp->item == LADX_ITEM_NONE,
	       "TradeSequenceItem: 0x%03X-Trade, its default item kept");
	ladx_settings_defaults(&settings);
	ladx_logic_configure(&logic, &settings);
	ladx_logic_configure(&logic, &settings);
	expect(ladx_logic_broken(&logic), "a second configure breaks the logic");

	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_WITCH, 0x2A2, -1);
	expect(ladx_logic_broken(&logic), "Witch() takes no room");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_CHEST, -1, -1);
	expect(ladx_logic_broken(&logic), "Chest(room) needs its room");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_CHEST, 0x004, LADX_ITEM_SWORD);
	expect(ladx_logic_broken(&logic), "Chest takes no second argument");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_SHOP_ITEM, -1, 2);
	expect(ladx_logic_broken(&logic), "ShopItem(2) has no default price");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_SHOP_ITEM, 0x0A1, 0);
	expect(ladx_logic_broken(&logic), "a shop outside underworld 2 breaks the logic");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_add_item_info(&logic, 0, LADX_SPOT_KEY_HOLE, 0x0D3, LADX_ITEM_NONE);
	expect(ladx_logic_broken(&logic), "KeyHole needs its default reward");
	ladx_logic_begin(&logic);
	ladx_logic_new_location(&logic, -1, NULL);
	ladx_logic_configure(&logic, &settings);
	expect(ladx_logic_broken(&logic), "configure before finish breaks the logic");
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

/* The fields the logic reads equal WorldSetup()'s. */
static bool ws_logic_defaults(const LadxWorldSetup *ws)
{
	LadxWorldSetup d;
	int i;

	ladx_world_setup_defaults(&d);
	for (i = 0; i < 9; i++)
		if (ws->boss_mapping[i] != d.boss_mapping[i])
			return false;
	for (i = 0; i < LADX_MINIBOSS_KEY_COUNT; i++)
		if (ws->miniboss_mapping[i] != d.miniboss_mapping[i])
			return false;
	return ws->goal == d.goal && ws->goal_count == d.goal_count;
}

/* The largest counts any configuration reached, against the capacities. */
typedef struct Maxima {
	int locations, edges, spots, options, suffix, spot_options, nodes, terms, todo;
} Maxima;

static Maxima maxima;

static void max_into(int *m, int v)
{
	if (v > *m)
		*m = v;
}

static void measure_graph(void)
{
	const LadxReqArena *a = ladx_logic_arena(&logic);
	int i, edges = 0, suffix = 0;

	max_into(&maxima.locations, ladx_logic_location_count(&logic));
	for (i = 0; i < ladx_logic_location_count(&logic); i++)
		edges += ladx_logic_location(&logic, i)->edge_count;
	max_into(&maxima.edges, edges);
	max_into(&maxima.spots, ladx_logic_spot_count(&logic));
	for (i = 0; i < ladx_logic_spot_count(&logic); i++) {
		max_into(&maxima.spot_options, ladx_logic_spot_options(&logic, i, NULL, 0));
		max_into(&maxima.suffix, ladx_logic_spot(&logic, i)->suffix_count);
		suffix += ladx_logic_spot(&logic, i)->suffix_count;
	}
	max_into(&maxima.options, suffix);
	max_into(&maxima.nodes, a->node_count);
	max_into(&maxima.terms, a->term_count);
}

static const char *sha512_hex(const char *bytes, size_t len, char hex[129])
{
	uint8_t digest[LADX_SHA512_SIZE];
	int i;

	ladx_sha512((const uint8_t *)bytes, len, digest);
	for (i = 0; i < LADX_SHA512_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
	return hex;
}

/* The graph of a configuration: built, rendered, its SHA-512 the fixture's. */
static void replay_graph(const LfConfig *c)
{
	char path[LF_NAME_MAX + 32], hex[129];
	size_t len;

	checks++;
	if (ladx_logic_build(&logic, &c->settings, &c->ws) != LADX_LOGIC_OK)
		test_fail(TEST, "config %s: the build fails", c->name);
	len = lf_graph_text(TEST, &logic, text, sizeof text);
	if (strcmp(sha512_hex(text, len, hex), c->graph_sha512) != 0) {
		write_mismatch(c->name, text, len, path, sizeof path);
		test_fail(TEST,
			  "config %s: graph SHA-512 %.16s..., fixture %.16s...; C's text in %s, LADXR's from python3 "
			  "tests/ladx/oracle/extract_logic.py --graph '%s' --out tmp/ladx-logic tests/ladx/oracle/ladxr",
			  c->name, hex, c->graph_sha512, path, c->name);
	}
	measure_graph();
}

static LfExplore explore;

/* Python's sorted() on the item names. */
static int name_cmp(const void *x, const void *y)
{
	return strcmp(*(const char *const *)x, *(const char *const *)y);
}

/* "BOW,SWORD", "-" when empty; the k-th name in names[k]. */
static int split_names(char *list, const char **names, int cap)
{
	int n = 0;
	char *p = list, *comma;

	if (strcmp(list, "-") == 0)
		return 0;
	for (;;) {
		comma = strchr(p, ',');
		if (comma)
			*comma = '\0';
		if (n < cap)
			names[n] = p;
		n++;
		if (!comma)
			return n;
		p = comma + 1;
	}
}

/* explore line k: a new Explorer, the inventory added in order, then
 * visit(start); the reached ranges and the sorted required items. */
static void replay_explore(const LfConfig *c, int k)
{
	static bool want[LADX_LOGIC_LOCATION_CAP], got[LADX_LOGIC_LOCATION_CAP];
	static char want_req[LF_EXPLORE_MAX];
	const char *want_names[LADX_ITEM_COUNT + 1], *got_names[LADX_ITEM_COUNT];
	int count = ladx_logic_location_count(&logic), nw, ng, i;
	LadxItemSet set;

	lf_explore_parse(TEST, c->name, c->explore[k], &explore);
	ladx_explorer_init(&explorer);
	for (i = 0; i < explore.items; i++)
		ladx_explorer_add_item(&explorer, explore.item[i], explore.count[i]);
	checks++;
	if (!ladx_explorer_visit(&explorer, &logic, ladx_logic_start(&logic)))
		test_fail(TEST, "config %s: explore %d: the start is not visited", c->name, k);
	max_into(&maxima.todo, explorer.todo_count);
	if (!lf_index_ranges_parse(explore.reached, want, count))
		test_fail(TEST, "config %s: explore %d: malformed reached ranges %s", c->name, k, explore.reached);
	for (i = 0; i < count; i++) {
		got[i] = ladx_explorer_reached(&explorer, i);
		checks++;
		if (got[i] != want[i])
			test_fail(TEST, "config %s: explore %d: location %d (%s) %s, LADXR's %s", c->name, k, i,
				  ladx_logic_location(&logic, i)->name ? ladx_logic_location(&logic, i)->name : "None",
				  got[i] ? "reached" : "not reached", want[i] ? "reached" : "not reached");
	}
	ladx_explorer_required_next(&explorer, &logic, &set);
	for (i = 0, ng = 0; i < LADX_ITEM_COUNT; i++)
		if (set.has[i])
			got_names[ng++] = ladx_item_name(i);
	qsort(got_names, (size_t)ng, sizeof got_names[0], name_cmp);
	strcpy(want_req, explore.required);
	nw = split_names(want_req, want_names, LADX_ITEM_COUNT + 1);
	for (i = 0; i < nw || i < ng; i++) {
		checks++;
		if (i >= nw || i >= ng || strcmp(want_names[i], got_names[i]) != 0)
			test_fail(TEST, "config %s: explore %d: required item %d is %s, LADXR's %s", c->name, k, i,
				  i < ng ? got_names[i] : "(end)", i < nw ? want_names[i] : "(end)");
	}
}

/* logic_vectors.txt: every configuration's settings parsed, validated and
 * supported, its ws lines read into LadxWorldSetup and rendered back, its
 * graph's SHA-512 and its explore vectors LADXR's. */
static void replay_vectors(void)
{
	FILE *f = lf_vectors_open(TEST, VECTORS);
	char lines[13][LF_WS_LINE_MAX];
	LadxSettingPair finding;
	int n = 0, pairs = 0, baselines = 0, i;

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
		/* test_dungeons builds the baselines under WorldSetup()'s defaults. */
		if (strstr(config.name, "/baseline") && !ws_logic_defaults(&config.ws))
			test_fail(TEST, "config %s: a baseline's world setup is not WorldSetup()'s", config.name);
		baselines += strstr(config.name, "/baseline") != NULL;
		lf_ws_render(&config.ws, &config.ws_rest, lines);
		for (i = 0; i < 13; i++) {
			checks++;
			if (strcmp(lines[i], config.ws_lines[i]) != 0)
				test_fail(TEST, "config %s: ws read back as %s, fixture %s", config.name, lines[i],
					  config.ws_lines[i]);
		}
		replay_graph(&config);
		for (i = 0; i < LF_EXPLORE_COUNT; i++)
			replay_explore(&config, i);
	}
	fclose(f);
	expect(n == 425, "425 configurations in " VECTORS);
	expect(baselines == 5, "5 baselines in " VECTORS);
	printf("%s: %d configurations of %s read: settings kept by validate, every value supported alone "
	       "(%d refused only as a pair LADXR's fill refuses), %d ws lines read back unchanged, the %d "
	       "baselines' world setup WorldSetup()'s\n",
	       TEST, n, VECTORS, pairs, 13 * n, baselines);
	printf("%s: %d configurations, graphs equal, %d explore vectors equal\n", TEST, n, n * LF_EXPLORE_COUNT);
	printf("%s: maxima over %d configurations: %d/%d locations, %d/%d edges, %d/%d spots, %d/%d options "
	       "appended (at most %d on one spot, %d options on one spot), %d/%d requirement nodes, %d/%d terms, "
	       "%d/%d explorer todo\n",
	       TEST, n, maxima.locations, LADX_LOGIC_LOCATION_CAP, maxima.edges, LADX_LOGIC_EDGE_CAP, maxima.spots,
	       LADX_LOGIC_SPOT_CAP, maxima.options, LADX_LOGIC_OPTION_CAP, maxima.suffix, maxima.spot_options,
	       maxima.nodes, LADX_REQ_NODE_CAP, maxima.terms, LADX_REQ_TERM_CAP, maxima.todo, LADX_LOGIC_EDGE_CAP);
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
	test_item_info();
	test_configure();
	test_dungeon_build();
	test_build();
	printf("%s: synthetic graphs, renderer, explorer, constructors and configure ok (%d checks)\n", TEST, checks);
	test_graph_spots();
	test_dungeons();
	test_baselines();
	if (fixtures)
		replay_vectors();
	return 0;
}
