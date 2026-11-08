#ifndef LADX_LOGIC_BUILDER_H
#define LADX_LOGIC_BUILDER_H

#include "ladx_logic.h"
#include "ladx_settings.h"

void ladx_logic_begin(LadxLogic *logic);
LadxReqArena *ladx_logic_arena_mut(LadxLogic *logic);

int ladx_logic_new_location(LadxLogic *logic, int dungeon, const char *name);

void ladx_logic_single_connect(LadxLogic *logic, int from, int to, LadxReq req);
void ladx_logic_connect_back(LadxLogic *logic, int from, int to, LadxReq req, LadxReq back);
void ladx_logic_connect(LadxLogic *logic, int from, int to, LadxReq req);

int ladx_logic_add_spot(LadxLogic *logic, int location, LadxSpotKind kind, const char *name_id,
			const int *options, int option_count);

int ladx_logic_add_item_info(LadxLogic *logic, int location, LadxSpotKind kind, int room, int arg);

void ladx_logic_configure(LadxLogic *logic, const LadxSettings *settings);

bool ladx_logic_finish(LadxLogic *logic, int start);

void ladx_logic_spot_append_option(LadxLogic *logic, int spot, int item);

#endif
