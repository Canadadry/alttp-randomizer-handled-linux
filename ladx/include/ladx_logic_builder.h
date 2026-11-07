#ifndef LADX_LOGIC_BUILDER_H
#define LADX_LOGIC_BUILDER_H

#include "ladx_logic.h"

/*
 * How the library builds a LadxLogic, public so that tests build synthetic
 * graphs: LADXR's Location (location.py) and Logic's discovery. Handles are
 * creation ids, valid until ladx_logic_finish. After the first capacity
 * overflow or LADXR assertion the logic is broken and every call is a
 * no-op (a handle of -1).
 */

/* An empty logic with a fresh arena (ladx_logic_arena_mut). */
void ladx_logic_begin(LadxLogic *logic);
LadxReqArena *ladx_logic_arena_mut(LadxLogic *logic);

/* Location(name, dungeon): name NULL or a string outliving the logic. */
int ladx_logic_new_location(LadxLogic *logic, int dungeon, const char *name);

/* Location.singleConnect: FALSE adds nothing, TRUE is NONE; a second edge
 * to the same target becomes OR(req, old), NONE when either is NONE. */
void ladx_logic_single_connect(LadxLogic *logic, int from, int to, LadxReq req);
/* Location.connect(to, req, back=back): from->to, then to->from. */
void ladx_logic_connect_back(LadxLogic *logic, int from, int to, LadxReq req, LadxReq back);
/* Location.connect(to, req): back is req, the same node. */
void ladx_logic_connect(LadxLogic *logic, int from, int to, LadxReq req);

/* Location.add of an ItemInfo: name_id copied (at most
 * LADX_SPOT_NAME_ID_MAX characters), its metadata looked up by it; options
 * outlive the logic. Returns the spot's handle. */
int ladx_logic_add_spot(LadxLogic *logic, int location, LadxSpotKind kind, const char *name_id,
			const int *options, int option_count);

/* Logic's __recursiveFindAll from start: drops the locations it does not
 * reach and numbers the rest in its order. False when broken. */
bool ladx_logic_finish(LadxLogic *logic, int start);

/* ItemInfo.configure's append, on a finished spot: each spot's appends
 * must be consecutive (configure runs spot by spot), else broken. */
void ladx_logic_spot_append_option(LadxLogic *logic, int spot, int item);

#endif
