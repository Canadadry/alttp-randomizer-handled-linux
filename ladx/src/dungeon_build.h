#ifndef LADX_DUNGEON_BUILD_H
#define LADX_DUNGEON_BUILD_H

#include "logic_internal.h"

#define L (c->logic)
#define A ladx_logic_arena_mut(L)
#define R (c->r)
#define OPTION(key, name) ladx_setting_is(c->options, LADX_SETTING_##key, name)
#define LOGIC_IS(name) OPTION(LOGIC, name)
#define HARD_OR_ABOVE (LOGIC_IS("hard") || LOGIC_IS("glitched") || LOGIC_IS("hell"))
#define GLITCHED_OR_ABOVE (LOGIC_IS("glitched") || LOGIC_IS("hell"))

#define I(name) ladx_req_item(LADX_ITEM_##name)
#define NONE ladx_req_none()
#define YES ladx_req_true()
#define NO ladx_req_false()
#define AND(...) LADX_AND(A, __VA_ARGS__)
#define OR(...) LADX_OR(A, __VA_ARGS__)
#define FOUND(item, n) ladx_req_found(A, LADX_ITEM_##item, (n))
#define COUNT(item, n) ladx_req_count(A, LADX_ITEM_##item, (n))
#define ENEMY(name) ladx_dungeon_enemy(c, LADX_ENEMY_##name)
#define MINIBOSS(i) (R->miniboss_requirements[c->world_setup->miniboss_mapping[(i)]])
#define MAPPING_C1 8
#define MAPPING_C2 9
#define MINIBOSS_NAMED(name) (R->miniboss_requirements[LADX_MINIBOSS_##name])
#define BOSS(i) (R->boss_requirements[c->world_setup->boss_mapping[(i)]])

#define LOC(name) ladx_logic_new_location(L, DUNGEON, (name))
#define LOC_OUTSIDE(name) ladx_logic_new_location(L, -1, (name))
#define ADD(loc, kind, room) ladx_logic_add_to(L, (loc), LADX_SPOT_##kind, (room), -1)
#define ADD_INDEX(loc, kind, index) ladx_logic_add_to(L, (loc), LADX_SPOT_##kind, -1, (index))
#define ADD_KEY(loc, item) ladx_logic_add_to(L, (loc), LADX_SPOT_KEY_LOCATION, -1, LADX_ITEM_##item)

#define CONNECT(from, to, req) ladx_logic_connect(L, (from), (to), (req))
#define CONNECT_BACK(from, to, req, back) ladx_logic_connect_back(L, (from), (to), (req), (back))
#define TARGETS(...) (int)(sizeof((int[]){__VA_ARGS__}) / sizeof(int)), (int[]){__VA_ARGS__}
#define CONNECT_ALL_BACK(from, targets, req, back) connect_all_back(L, (from), targets, (req), (back))
#define CONNECT_ALL(from, targets, req) connect_all(L, (from), targets, (req))

static inline void connect_all_back(LadxLogic *logic, int from, int n, const int *targets, LadxReq req, LadxReq back)
{
	ladx_logic_connect_all(logic, from, n, targets, req, back);
}

static inline void connect_all(LadxLogic *logic, int from, int n, const int *targets, LadxReq req)
{
	ladx_logic_connect_all(logic, from, n, targets, req, req);
}

#endif
