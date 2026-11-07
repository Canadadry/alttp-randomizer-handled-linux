#include "ladx_logic.h"

#include <string.h>

void ladx_explorer_init(LadxExplorer *e)
{
	ladx_inventory_init(&e->inventory);
	memset(e->visited, 0, sizeof e->visited);
	e->todo_count = 0;
}

void ladx_explorer_add_item(LadxExplorer *e, int item, int count)
{
	ladx_inventory_add(&e->inventory, item, count);
}

bool ladx_explorer_reached(const LadxExplorer *e, int location)
{
	return location >= 0 && location < LADX_LOGIC_LOCATION_CAP && e->visited[location];
}

static void enter(LadxExplorer *e, const LadxLogic *logic, int location, int depth)
{
	const LadxLocation *loc = ladx_logic_location(logic, location);
	int i;

	e->visited[location] = true;
	for (i = 0; i < loc->spot_count; i++)
		ladx_explorer_add_item(e, ladx_logic_spot(logic, loc->spot_first + i)->item, 1);
	e->stack[2 * depth] = loc->edge_first;
	e->stack[2 * depth + 1] = loc->edge_first + loc->edge_count;
}

/* _visit's recursion on an explicit stack of (next edge, end) frames: each
 * frame is a newly visited location, so the depth stays within the cap. */
static void visit_one(LadxExplorer *e, const LadxLogic *logic, int location)
{
	const LadxReqArena *arena = ladx_logic_arena(logic);
	int depth = 1;

	enter(e, logic, location, 0);
	while (depth > 0) {
		int *frame = &e->stack[2 * (depth - 1)];
		const LadxEdge *edge;

		if (frame[0] >= frame[1]) {
			depth--;
			continue;
		}
		edge = ladx_logic_edge(logic, frame[0]++);
		if (e->visited[edge->target])
			continue;
		if (ladx_req_test(arena, edge->req, &e->inventory)) {
			enter(e, logic, edge->target, depth);
			depth++;
		} else if (e->todo_count < LADX_LOGIC_EDGE_CAP) {
			e->todo[e->todo_count].target = edge->target;
			e->todo[e->todo_count].req = edge->req;
			e->todo_count++;
		}
	}
}

static bool process(LadxExplorer *e, const LadxLogic *logic)
{
	int i, n = 0;

	for (i = 0; i < e->todo_count; i++)
		if (!e->visited[e->todo[i].target])
			e->todo[n++] = e->todo[i];
	e->todo_count = n;
	for (i = 0; i < e->todo_count; i++)
		if (!e->visited[e->todo[i].target] &&
		    ladx_req_test(ladx_logic_arena(logic), e->todo[i].req, &e->inventory)) {
			visit_one(e, logic, e->todo[i].target);
			return true;
		}
	return false;
}

bool ladx_explorer_visit(LadxExplorer *e, const LadxLogic *logic, int location)
{
	if (location < 0 || location >= ladx_logic_location_count(logic) || e->visited[location])
		return false;
	visit_one(e, logic, location);
	while (process(e, logic))
		;
	return true;
}

void ladx_explorer_required_next(const LadxExplorer *e, const LadxLogic *logic, LadxItemSet *out)
{
	int i;

	ladx_item_set_clear(out);
	for (i = 0; i < e->todo_count; i++)
		ladx_req_get_items(ladx_logic_arena(logic), e->todo[i].req, &e->inventory, out);
}
