#ifndef LADX_LOGIC_H
#define LADX_LOGIC_H

#include "ladx_items.h"
#include "ladx_requirements.h"
#include "ladx_settings.h"

#include <stdbool.h>

/* LADXR's logic graph at 4325207e (logic/location.py, Logic's discovery,
 * explorer.py), tier 1 only, in a caller-owned struct with no allocation. */

#define LADX_LOGIC_LOCATION_CAP 1024
#define LADX_LOGIC_EDGE_CAP 2048
#define LADX_LOGIC_SPOT_CAP 512
#define LADX_LOGIC_OPTION_CAP 20480
#define LADX_SPOT_NAME_ID_MAX 23

/* LADXR's ItemInfo subclasses of locations/, by class name. */
typedef enum LadxSpotKind {
	LADX_SPOT_ANGLER_KEY,
	LADX_SPOT_BEACH_SWORD,
	LADX_SPOT_BIRD_KEY,
	LADX_SPOT_BOOMERANG_GUY,
	LADX_SPOT_CHEST,
	LADX_SPOT_DROPPED_KEY,
	LADX_SPOT_DUNGEON_CHEST,
	LADX_SPOT_FACE_KEY,
	LADX_SPOT_FISHING_MINIGAME,
	LADX_SPOT_GOLD_LEAF,
	LADX_SPOT_HEART_CONTAINER,
	LADX_SPOT_HEART_PIECE,
	LADX_SPOT_HOOKSHOT_DROP,
	LADX_SPOT_INSTRUMENT,
	LADX_SPOT_KEY_HOLE,
	LADX_SPOT_KEY_LOCATION,
	LADX_SPOT_MAD_BATTER,
	LADX_SPOT_OWL_STATUE,
	LADX_SPOT_SEASHELL,
	LADX_SPOT_SEASHELL_MANSION,
	LADX_SPOT_SEASHELL_MANSION_BONUS,
	LADX_SPOT_SHOP_ITEM,
	LADX_SPOT_SLIME_KEY,
	LADX_SPOT_SONG,
	LADX_SPOT_START_ITEM,
	LADX_SPOT_TOADSTOOL,
	LADX_SPOT_TRADE_SEQUENCE_ITEM,
	LADX_SPOT_TUNIC_FAIRY,
	LADX_SPOT_WITCH,
	LADX_SPOT_KIND_COUNT
} LadxSpotKind;

/* LADXR's class name ("DungeonChest"); NULL out of range. */
const char *ladx_spot_kind_name(int kind);

/* Python's None is dungeon -1 and name NULL. */
typedef struct LadxLocation {
	int dungeon;
	const char *name;
	int edge_first, edge_count;
	int spot_first, spot_count;
} LadxLocation;

typedef struct LadxEdge {
	int target;
	LadxReq req;
} LadxEdge;

/* Options: base_options (static, never copied), then the spot's suffix in
 * the logic's option pool; read them through ladx_logic_spot_options. */
typedef struct LadxSpot {
	int location;
	LadxSpotKind kind;
	char name_id[LADX_SPOT_NAME_ID_MAX + 1];
	const int *base_options;
	int base_option_count;
	int suffix_first, suffix_count;
	int metadata;
	int room;	  /* the constructor's room; -1 for None (KeyLocation) */
	int index;	  /* ShopItem's, TunicFairy's, SeashellMansionBonus's; else -1 */
	int default_item; /* KeyHole's reward, TradeSequenceItem's item, KeyLocation's key; else none */
	int forced_item;  /* forced_item: LADX_ITEM_NONE (None) unless the logic forces one */
	int item;	  /* LADX_ITEM_NONE until placed */
} LadxSpot;

typedef struct LadxBuildLocation {
	int dungeon;
	const char *name;
	int edge_head, edge_tail;
	int spot_head, spot_tail;
	int order;
} LadxBuildLocation;

typedef struct LadxBuildEdge {
	int target;
	LadxReq req;
	int next;
} LadxBuildEdge;

/* Read only, through the functions below; the build_* fields belong to
 * the builder (ladx_logic_builder.h). */
typedef struct LadxLogic {
	LadxReqArena arena;
	LadxLocation location[LADX_LOGIC_LOCATION_CAP];
	LadxEdge edge[LADX_LOGIC_EDGE_CAP];
	LadxSpot spot[LADX_LOGIC_SPOT_CAP];
	unsigned char option[LADX_LOGIC_OPTION_CAP];
	int location_count, edge_count, spot_count, option_count;
	int start;
	int windfish, egg, nightmare; /* creation ids until finished, then location indexes; -1 for none */
	bool finished;
	bool configured;
	bool broken;
	LadxBuildLocation build_location[LADX_LOGIC_LOCATION_CAP];
	LadxBuildEdge build_edge[LADX_LOGIC_EDGE_CAP];
	LadxSpot build_spot[LADX_LOGIC_SPOT_CAP];
	int build_spot_next[LADX_LOGIC_SPOT_CAP];
	int build_location_count, build_edge_count, build_spot_count;
	int stack[2 * LADX_LOGIC_LOCATION_CAP];
} LadxLogic;

/* Set by the first capacity overflow or LADXR assertion, the arena's
 * included; the graph is then unusable. */
bool ladx_logic_broken(const LadxLogic *logic);

/* Once finished: locations in LADXR's location_list order, spots in
 * iteminfo_list order. Out of range gives NULL, or -1 for start. */
const LadxReqArena *ladx_logic_arena(const LadxLogic *logic);
int ladx_logic_start(const LadxLogic *logic);
/* World's windfish, egg and nightmare (ladx_logic_build only); -1 when
 * not reached or not built. */
int ladx_logic_windfish(const LadxLogic *logic);
int ladx_logic_egg(const LadxLogic *logic);
int ladx_logic_nightmare(const LadxLogic *logic);
int ladx_logic_location_count(const LadxLogic *logic);
const LadxLocation *ladx_logic_location(const LadxLogic *logic, int i);
/* The location's connections in order: edge_first + 0 .. edge_count - 1. */
const LadxEdge *ladx_logic_edge(const LadxLogic *logic, int i);
int ladx_logic_spot_count(const LadxLogic *logic);
const LadxSpot *ladx_logic_spot(const LadxLogic *logic, int i);
/* getOptions() in order, at most cap copied; returns the full count. */
int ladx_logic_spot_options(const LadxLogic *logic, int spot, int *out, int cap);
/* checkMetadata.py's entry of the spot: its name and area. */
const char *ladx_spot_metadata_name(const LadxSpot *spot);
const char *ladx_spot_metadata_area(const LadxSpot *spot);
/* The fill's placement; false (nothing set) for a bad spot or item. */
bool ladx_logic_place(LadxLogic *logic, int spot, int item);

/* worldSetup.py WorldSetup: the fields logic reads. */
typedef enum LadxMiniboss {
	LADX_MINIBOSS_ROLLING_BONES,
	LADX_MINIBOSS_HINOX,
	LADX_MINIBOSS_DODONGO,
	LADX_MINIBOSS_CUE_BALL,
	LADX_MINIBOSS_GHOMA,
	LADX_MINIBOSS_SMASHER,
	LADX_MINIBOSS_GRIM_CREEPER,
	LADX_MINIBOSS_BLAINO,
	LADX_MINIBOSS_AVALAUNCH,
	LADX_MINIBOSS_GIANT_BUZZ_BLOB,
	LADX_MINIBOSS_MOBLIN_KING,
	LADX_MINIBOSS_ARMOS_KNIGHT,
	LADX_MINIBOSS_COUNT
} LadxMiniboss;

/* miniboss_mapping's keys: 0 to 7, c1, c2, moblin_cave, armos_temple. */
#define LADX_MINIBOSS_KEY_COUNT 12

typedef enum LadxGoal {
	LADX_GOAL_VANILLA,
	LADX_GOAL_INSTRUMENTS,
	LADX_GOAL_OPEN,
	LADX_GOAL_SEASHELLS,
	LADX_GOAL_MAZE,
	LADX_GOAL_SPECIFIC, /* LADXR's "=<digits>" */
	LADX_GOAL_COUNT
} LadxGoal;

typedef struct LadxWorldSetup {
	int boss_mapping[9];
	LadxMiniboss miniboss_mapping[LADX_MINIBOSS_KEY_COUNT];
	LadxGoal goal;
	char goal_instruments[9]; /* SPECIFIC: digits 1 to 8, NUL-terminated */
	int goal_count;
} LadxWorldSetup;

/* WorldSetup(): bosses in place, the vanilla minibosses, goal vanilla, 8. */
void ladx_world_setup_defaults(LadxWorldSetup *ws);
/* NULL out of range; key i of miniboss_mapping ("c1"). */
const char *ladx_miniboss_key(int i);
const char *ladx_miniboss_name(int miniboss);
/* -1 when unknown. */
int ladx_miniboss_find(const char *name);
/* LADXR's goal text ("seashells"); NULL for SPECIFIC and out of range. */
const char *ladx_goal_name(int goal);
/* -1 when unknown; never SPECIFIC. */
int ladx_goal_find(const char *name);

typedef enum LadxLogicStatus {
	LADX_LOGIC_OK,
	LADX_LOGIC_UNSUPPORTED, /* a value unsupported alone, or a pair other than LADXR_REFUSES; nothing built */
	LADX_LOGIC_BROKEN	/* a capacity overflow or LADXR assertion; the logic is unusable */
} LadxLogicStatus;

/*
 * main.py Logic(options, world_setup=ws) for overworld=normal and the
 * identity entrance mapping: World, the nine dungeons, the entrance loop,
 * the egg requirement, the removed dungeon items, discovery and configure.
 * Draws no random number.
 */
LadxLogicStatus ladx_logic_build(LadxLogic *out, const LadxSettings *validated, const LadxWorldSetup *ws);

/*
 * Dungeon n alone (0 the colour dungeon, 1 to 8; any other n BROKEN):
 * RequirementsSettings, then DungeonN with its entrance as the start,
 * discovery and configure. Not LADXR's whole graph:
 * for the per-dungeon claims and for narrowing a divergence.
 */
LadxLogicStatus ladx_logic_build_dungeon(LadxLogic *out, const LadxSettings *validated, const LadxWorldSetup *ws,
					 int n);

/* explorer.py Explorer over a finished graph. */
typedef struct LadxExplorerTodo {
	int target;
	LadxReq req;
} LadxExplorerTodo;

typedef struct LadxExplorer {
	LadxInventory inventory;
	bool visited[LADX_LOGIC_LOCATION_CAP];
	LadxExplorerTodo todo[LADX_LOGIC_EDGE_CAP];
	int todo_count;
	int stack[2 * LADX_LOGIC_LOCATION_CAP];
} LadxExplorer;

void ladx_explorer_init(LadxExplorer *e);
/* Explorer.addItem: RUPEES_<n> folding, LADX_ITEM_NONE ignored. */
void ladx_explorer_add_item(LadxExplorer *e, int item, int count);
/* Explorer.visit: _visit, then _process until no progress. False, nothing
 * done, for a location already visited (LADXR's assertion) or out of range. */
bool ladx_explorer_visit(LadxExplorer *e, const LadxLogic *logic, int location);
bool ladx_explorer_reached(const LadxExplorer *e, int location);
/* getRequiredItemsForNextLocations, into a cleared set. */
void ladx_explorer_required_next(const LadxExplorer *e, const LadxLogic *logic, LadxItemSet *out);

#endif
