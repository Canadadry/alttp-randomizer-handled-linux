#include "ladx_logic.h"
#include "ladx_logic_builder.h"

#include "check_metadata.h"

#include <string.h>

typedef char option_fits_a_byte[LADX_ITEM_COUNT <= 256 ? 1 : -1];

static const char *const kind_names[LADX_SPOT_KIND_COUNT] = {
	"AnglerKey", "BeachSword", "BirdKey", "BoomerangGuy", "Chest", "DroppedKey",
	"DungeonChest", "FaceKey", "FishingMinigame", "GoldLeaf", "HeartContainer", "HeartPiece",
	"HookshotDrop", "Instrument", "KeyHole", "KeyLocation", "MadBatter", "OwlStatue",
	"Seashell", "SeashellMansion", "SeashellMansionBonus", "ShopItem", "SlimeKey", "Song",
	"StartItem", "Toadstool", "TradeSequenceItem", "TunicFairy", "Witch",
};

const char *ladx_spot_kind_name(int kind)
{
	return kind >= 0 && kind < LADX_SPOT_KIND_COUNT ? kind_names[kind] : NULL;
}

bool ladx_logic_broken(const LadxLogic *logic)
{
	return logic->broken || ladx_req_arena_broken(&logic->arena);
}

const LadxReqArena *ladx_logic_arena(const LadxLogic *logic)
{
	return &logic->arena;
}

LadxReqArena *ladx_logic_arena_mut(LadxLogic *logic)
{
	return &logic->arena;
}

static bool usable(const LadxLogic *logic)
{
	return logic->finished && !ladx_logic_broken(logic);
}

int ladx_logic_start(const LadxLogic *logic)
{
	return usable(logic) ? logic->start : -1;
}

int ladx_logic_location_count(const LadxLogic *logic)
{
	return usable(logic) ? logic->location_count : 0;
}

const LadxLocation *ladx_logic_location(const LadxLogic *logic, int i)
{
	return usable(logic) && i >= 0 && i < logic->location_count ? &logic->location[i] : NULL;
}

const LadxEdge *ladx_logic_edge(const LadxLogic *logic, int i)
{
	return usable(logic) && i >= 0 && i < logic->edge_count ? &logic->edge[i] : NULL;
}

int ladx_logic_spot_count(const LadxLogic *logic)
{
	return usable(logic) ? logic->spot_count : 0;
}

const LadxSpot *ladx_logic_spot(const LadxLogic *logic, int i)
{
	return usable(logic) && i >= 0 && i < logic->spot_count ? &logic->spot[i] : NULL;
}

int ladx_logic_spot_options(const LadxLogic *logic, int spot, int *out, int cap)
{
	const LadxSpot *s = ladx_logic_spot(logic, spot);
	int i, n = 0;

	if (!s)
		return 0;
	for (i = 0; i < s->base_option_count; i++, n++)
		if (n < cap)
			out[n] = s->base_options[i];
	for (i = 0; i < s->suffix_count; i++, n++)
		if (n < cap)
			out[n] = logic->option[s->suffix_first + i];
	return n;
}

const char *ladx_spot_metadata_name(const LadxSpot *spot)
{
	return ladx_check_metadata_name(spot->metadata);
}

const char *ladx_spot_metadata_area(const LadxSpot *spot)
{
	return ladx_check_metadata_area(spot->metadata);
}

bool ladx_logic_place(LadxLogic *logic, int spot, int item)
{
	if (!usable(logic) || spot < 0 || spot >= logic->spot_count || item < LADX_ITEM_NONE ||
	    item >= LADX_ITEM_COUNT)
		return false;
	logic->spot[spot].item = item;
	return true;
}

void ladx_logic_begin(LadxLogic *logic)
{
	ladx_req_arena_init(&logic->arena);
	logic->location_count = 0;
	logic->edge_count = 0;
	logic->spot_count = 0;
	logic->option_count = 0;
	logic->start = -1;
	logic->finished = false;
	logic->configured = false;
	logic->broken = false;
	logic->build_location_count = 0;
	logic->build_edge_count = 0;
	logic->build_spot_count = 0;
}

static bool building(const LadxLogic *logic)
{
	return !logic->finished && !ladx_logic_broken(logic);
}

static bool is_location(const LadxLogic *logic, int loc)
{
	return loc >= 0 && loc < logic->build_location_count;
}

static int fail(LadxLogic *logic)
{
	logic->broken = true;
	return -1;
}

int ladx_logic_new_location(LadxLogic *logic, int dungeon, const char *name)
{
	LadxBuildLocation *loc;

	if (!building(logic))
		return fail(logic);
	if (logic->build_location_count >= LADX_LOGIC_LOCATION_CAP || dungeon < -1)
		return fail(logic);
	loc = &logic->build_location[logic->build_location_count];
	loc->dungeon = dungeon;
	loc->name = name;
	loc->edge_head = loc->edge_tail = -1;
	loc->spot_head = loc->spot_tail = -1;
	loc->order = -1;
	return logic->build_location_count++;
}

void ladx_logic_single_connect(LadxLogic *logic, int from, int to, LadxReq req)
{
	LadxBuildLocation *loc;
	LadxBuildEdge *edge;
	int e;

	if (!building(logic) || !is_location(logic, from) || !is_location(logic, to)) {
		fail(logic);
		return;
	}
	if (req.tag == LADX_REQ_FALSE)
		return;
	if (req.tag == LADX_REQ_TRUE)
		req = ladx_req_none();
	loc = &logic->build_location[from];
	for (e = loc->edge_head; e >= 0; e = logic->build_edge[e].next) {
		edge = &logic->build_edge[e];
		if (edge->target != to)
			continue;
		if (req.tag == LADX_REQ_NONE || edge->req.tag == LADX_REQ_NONE)
			edge->req = ladx_req_none();
		else
			edge->req = LADX_OR(&logic->arena, req, edge->req);
		return;
	}
	if (logic->build_edge_count >= LADX_LOGIC_EDGE_CAP) {
		fail(logic);
		return;
	}
	e = logic->build_edge_count++;
	edge = &logic->build_edge[e];
	edge->target = to;
	edge->req = req;
	edge->next = -1;
	if (loc->edge_tail >= 0)
		logic->build_edge[loc->edge_tail].next = e;
	else
		loc->edge_head = e;
	loc->edge_tail = e;
}

void ladx_logic_connect_back(LadxLogic *logic, int from, int to, LadxReq req, LadxReq back)
{
	ladx_logic_single_connect(logic, from, to, req);
	ladx_logic_single_connect(logic, to, from, back);
}

void ladx_logic_connect(LadxLogic *logic, int from, int to, LadxReq req)
{
	ladx_logic_connect_back(logic, from, to, req, req);
}

int ladx_logic_add_spot(LadxLogic *logic, int location, LadxSpotKind kind, const char *name_id,
			const int *options, int option_count)
{
	LadxBuildLocation *loc;
	LadxSpot *spot;
	int s;

	if (!building(logic) || !is_location(logic, location) || (int)kind < 0 ||
	    kind >= LADX_SPOT_KIND_COUNT || strlen(name_id) > LADX_SPOT_NAME_ID_MAX || option_count < 0 ||
	    (option_count > 0 && !options) || logic->build_spot_count >= LADX_LOGIC_SPOT_CAP)
		return fail(logic);
	s = logic->build_spot_count++;
	spot = &logic->build_spot[s];
	spot->location = location;
	spot->kind = kind;
	strcpy(spot->name_id, name_id);
	spot->base_options = options;
	spot->base_option_count = option_count;
	spot->suffix_first = 0;
	spot->suffix_count = 0;
	spot->metadata = ladx_check_metadata_find(name_id);
	spot->room = -1;
	spot->index = -1;
	spot->default_item = LADX_ITEM_NONE;
	spot->item = LADX_ITEM_NONE;
	logic->build_spot_next[s] = -1;
	loc = &logic->build_location[location];
	if (loc->spot_tail >= 0)
		logic->build_spot_next[loc->spot_tail] = s;
	else
		loc->spot_head = s;
	loc->spot_tail = s;
	return s;
}

/* Appends the location to location_list, its spots to iteminfo_list and its
 * edges, targets still creation ids. */
static void discover(LadxLogic *logic, int b)
{
	LadxBuildLocation *bl = &logic->build_location[b];
	LadxLocation *loc = &logic->location[logic->location_count];
	int e, s;

	bl->order = logic->location_count++;
	loc->dungeon = bl->dungeon;
	loc->name = bl->name;
	loc->edge_first = logic->edge_count;
	loc->spot_first = logic->spot_count;
	for (e = bl->edge_head; e >= 0; e = logic->build_edge[e].next) {
		logic->edge[logic->edge_count].target = logic->build_edge[e].target;
		logic->edge[logic->edge_count].req = logic->build_edge[e].req;
		logic->edge_count++;
	}
	for (s = bl->spot_head; s >= 0; s = logic->build_spot_next[s]) {
		logic->spot[logic->spot_count] = logic->build_spot[s];
		logic->spot[logic->spot_count].location = bl->order;
		logic->spot_count++;
	}
	loc->edge_count = logic->edge_count - loc->edge_first;
	loc->spot_count = logic->spot_count - loc->spot_first;
}

/* Logic.__recursiveFindAll's preorder, on an explicit stack of (location,
 * next edge) frames: the device's stack is never at stake. */
bool ladx_logic_finish(LadxLogic *logic, int start)
{
	int depth = 0, i;

	if (!building(logic) || !is_location(logic, start)) {
		fail(logic);
		return false;
	}
	discover(logic, start);
	logic->stack[0] = start;
	logic->stack[1] = logic->build_location[start].edge_head;
	depth = 1;
	while (depth > 0) {
		int *frame = &logic->stack[2 * (depth - 1)];
		int e = frame[1], target;

		if (e < 0) {
			depth--;
			continue;
		}
		frame[1] = logic->build_edge[e].next;
		target = logic->build_edge[e].target;
		if (logic->build_location[target].order >= 0)
			continue;
		discover(logic, target);
		logic->stack[2 * depth] = target;
		logic->stack[2 * depth + 1] = logic->build_location[target].edge_head;
		depth++;
	}
	for (i = 0; i < logic->edge_count; i++)
		logic->edge[i].target = logic->build_location[logic->edge[i].target].order;
	logic->start = 0;
	logic->finished = true;
	return true;
}

void ladx_logic_spot_append_option(LadxLogic *logic, int spot, int item)
{
	LadxSpot *s;

	if (!usable(logic) || spot < 0 || spot >= logic->spot_count || item < 0 || item >= LADX_ITEM_COUNT ||
	    logic->option_count >= LADX_LOGIC_OPTION_CAP) {
		logic->broken = true;
		return;
	}
	s = &logic->spot[spot];
	if (s->suffix_count == 0)
		s->suffix_first = logic->option_count;
	else if (s->suffix_first + s->suffix_count != logic->option_count) {
		logic->broken = true;
		return;
	}
	logic->option[logic->option_count++] = (unsigned char)item;
	s->suffix_count++;
}
