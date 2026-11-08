#ifndef LADX_LOGIC_H
#define LADX_LOGIC_H

#include "ladx_items.h"
#include "ladx_requirements.h"
#include "ladx_settings.h"

#include <stdbool.h>

#define LADX_LOGIC_LOCATION_CAP 1024
#define LADX_LOGIC_EDGE_CAP 2048
#define LADX_LOGIC_SPOT_CAP 512
#define LADX_LOGIC_OPTION_CAP 20480
#define LADX_SPOT_NAME_ID_MAX 23

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

const char *ladx_spot_kind_name(int kind);

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

typedef struct LadxSpot {
	int location;
	LadxSpotKind kind;
	char name_id[LADX_SPOT_NAME_ID_MAX + 1];
	const int *base_options;
	int base_option_count;
	int suffix_first, suffix_count;
	int metadata;
	int room;
	int index;
	int default_item;
	int forced_item;
	int item;
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

typedef struct LadxLogic {
	LadxReqArena arena;
	LadxLocation location[LADX_LOGIC_LOCATION_CAP];
	LadxEdge edge[LADX_LOGIC_EDGE_CAP];
	LadxSpot spot[LADX_LOGIC_SPOT_CAP];
	unsigned char option[LADX_LOGIC_OPTION_CAP];
	int location_count, edge_count, spot_count, option_count;
	int start;
	int windfish, egg, nightmare;
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

bool ladx_logic_broken(const LadxLogic *logic);

const LadxReqArena *ladx_logic_arena(const LadxLogic *logic);
int ladx_logic_start(const LadxLogic *logic);
int ladx_logic_windfish(const LadxLogic *logic);
int ladx_logic_egg(const LadxLogic *logic);
int ladx_logic_nightmare(const LadxLogic *logic);
int ladx_logic_location_count(const LadxLogic *logic);
const LadxLocation *ladx_logic_location(const LadxLogic *logic, int i);
const LadxEdge *ladx_logic_edge(const LadxLogic *logic, int i);
int ladx_logic_spot_count(const LadxLogic *logic);
const LadxSpot *ladx_logic_spot(const LadxLogic *logic, int i);
int ladx_logic_spot_options(const LadxLogic *logic, int spot, int *out, int cap);
const char *ladx_spot_metadata_name(const LadxSpot *spot);
const char *ladx_spot_metadata_area(const LadxSpot *spot);
bool ladx_logic_place(LadxLogic *logic, int spot, int item);

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

#define LADX_MINIBOSS_KEY_COUNT 12

typedef enum LadxGoal {
	LADX_GOAL_VANILLA,
	LADX_GOAL_INSTRUMENTS,
	LADX_GOAL_OPEN,
	LADX_GOAL_SEASHELLS,
	LADX_GOAL_MAZE,
	LADX_GOAL_SPECIFIC,
	LADX_GOAL_COUNT
} LadxGoal;

#define LADX_SIGN_MAZE_SIGNS 22

typedef struct LadxSign {
	int x, y;
} LadxSign;

typedef struct LadxWorldSetup {
	int boss_mapping[9];
	LadxMiniboss miniboss_mapping[LADX_MINIBOSS_KEY_COUNT];
	LadxGoal goal;
	char goal_instruments[9];
	int goal_count;
	int sign_count;
	LadxSign signs[LADX_SIGN_MAZE_SIGNS];
	int directions[LADX_SIGN_MAZE_SIGNS - 1];
	int multichest;
	bool inside_to_outside, keep_two_way, one_on_one;
} LadxWorldSetup;

void ladx_world_setup_defaults(LadxWorldSetup *ws);
const char *ladx_miniboss_key(int i);
const char *ladx_miniboss_name(int miniboss);
int ladx_miniboss_find(const char *name);
const char *ladx_goal_name(int goal);
int ladx_goal_find(const char *name);

typedef enum LadxLogicStatus {
	LADX_LOGIC_OK,
	LADX_LOGIC_UNSUPPORTED,
	LADX_LOGIC_BROKEN
} LadxLogicStatus;

LadxLogicStatus ladx_logic_build(LadxLogic *out, const LadxSettings *validated, const LadxWorldSetup *ws);

LadxLogicStatus ladx_logic_build_dungeon(LadxLogic *out, const LadxSettings *validated, const LadxWorldSetup *ws,
					 int n);

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
void ladx_explorer_add_item(LadxExplorer *e, int item, int count);
bool ladx_explorer_visit(LadxExplorer *e, const LadxLogic *logic, int location);
bool ladx_explorer_reached(const LadxExplorer *e, int location);
void ladx_explorer_required_next(const LadxExplorer *e, const LadxLogic *logic, LadxItemSet *out);

#endif
