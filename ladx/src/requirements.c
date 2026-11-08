#include "ladx_requirements.h"

#include <assert.h>
#include <string.h>

void ladx_req_arena_init(LadxReqArena *a)
{
	a->node_count = 0;
	a->term_count = 0;
	a->broken = false;
}

bool ladx_req_arena_broken(const LadxReqArena *a)
{
	return a->broken;
}

static LadxReq make(LadxReqTag tag, int id)
{
	LadxReq r;

	r.tag = tag;
	r.id = id;
	return r;
}

LadxReq ladx_req_none(void)
{
	return make(LADX_REQ_NONE, 0);
}

LadxReq ladx_req_true(void)
{
	return make(LADX_REQ_TRUE, 0);
}

LadxReq ladx_req_false(void)
{
	return make(LADX_REQ_FALSE, 0);
}

LadxReq ladx_req_bool(bool value)
{
	return value ? ladx_req_true() : ladx_req_false();
}

LadxReq ladx_req_item(int item)
{
	return make(LADX_REQ_ITEM, item);
}

static LadxReq broken(LadxReqArena *a)
{
	a->broken = true;
	return ladx_req_none();
}

static int new_node(LadxReqArena *a, LadxNodeKind kind, int amount)
{
	LadxReqNode *n;

	if (a->broken)
		return -1;
	if (a->node_count == LADX_REQ_NODE_CAP) {
		a->broken = true;
		return -1;
	}
	n = &a->nodes[a->node_count];
	n->kind = kind;
	n->amount = amount;
	n->items_head = n->items_tail = -1;
	n->children_head = n->children_tail = -1;
	return a->node_count++;
}

static void append(LadxReqArena *a, int *head, int *tail, int value)
{
	LadxReqTerm *t;

	if (a->broken)
		return;
	if (a->term_count == LADX_REQ_TERM_CAP) {
		a->broken = true;
		return;
	}
	t = &a->terms[a->term_count];
	t->value = value;
	t->next = -1;
	if (*tail < 0)
		*head = a->term_count;
	else
		a->terms[*tail].next = a->term_count;
	*tail = a->term_count++;
}

static bool contains(const LadxReqArena *a, int head, int value)
{
	int t;

	for (t = head; t >= 0; t = a->terms[t].next)
		if (a->terms[t].value == value)
			return true;
	return false;
}

static void unlink_first(LadxReqArena *a, int *head, int *tail, int value)
{
	int prev = -1, t;

	for (t = *head; t >= 0; prev = t, t = a->terms[t].next) {
		if (a->terms[t].value != value)
			continue;
		if (prev < 0)
			*head = a->terms[t].next;
		else
			a->terms[prev].next = a->terms[t].next;
		if (*tail == t)
			*tail = prev;
		return;
	}
}

static LadxReq list_node(LadxReqArena *a, LadxNodeKind kind, int n, const LadxReq *terms)
{
	int node = new_node(a, kind, 0), i;
	LadxReqNode *nd;

	if (node < 0)
		return ladx_req_none();
	nd = &a->nodes[node];
	for (i = 0; i < n; i++)
		if (terms[i].tag == LADX_REQ_ITEM)
			append(a, &nd->items_head, &nd->items_tail, terms[i].id);
	for (i = 0; i < n; i++)
		if (terms[i].tag == LADX_REQ_NODE)
			append(a, &nd->children_head, &nd->children_tail, terms[i].id);
	if (a->broken)
		return ladx_req_none();
	return make(LADX_REQ_NODE, node);
}

static bool any_tag(int n, const LadxReq *terms, LadxReqTag tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (terms[i].tag == tag)
			return true;
	return false;
}

static bool all_tag(int n, const LadxReq *terms, LadxReqTag tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (terms[i].tag != tag)
			return false;
	return true;
}

LadxReq ladx_req_and(LadxReqArena *a, int n, const LadxReq *terms)
{
	if (a->broken)
		return ladx_req_none();
	if (any_tag(n, terms, LADX_REQ_FALSE))
		return ladx_req_false();
	if (all_tag(n, terms, LADX_REQ_TRUE))
		return ladx_req_true();
	return list_node(a, LADX_NODE_AND, n, terms);
}

LadxReq ladx_req_or(LadxReqArena *a, int n, const LadxReq *terms)
{
	if (a->broken)
		return ladx_req_none();
	if (any_tag(n, terms, LADX_REQ_TRUE))
		return ladx_req_true();
	if (all_tag(n, terms, LADX_REQ_FALSE))
		return ladx_req_false();
	if (!any_tag(n, terms, LADX_REQ_ITEM) && !any_tag(n, terms, LADX_REQ_NODE))
		return broken(a);
	return list_node(a, LADX_NODE_OR, n, terms);
}

static LadxReq item_node(LadxReqArena *a, LadxNodeKind kind, int n, const int *items, int amount)
{
	int node = new_node(a, kind, amount), i;
	LadxReqNode *nd;

	if (node < 0)
		return ladx_req_none();
	nd = &a->nodes[node];
	for (i = 0; i < n; i++)
		append(a, &nd->items_head, &nd->items_tail, items[i]);
	if (a->broken)
		return ladx_req_none();
	return make(LADX_REQ_NODE, node);
}

LadxReq ladx_req_count(LadxReqArena *a, int item, int amount)
{
	return item_node(a, LADX_NODE_COUNT, 1, &item, amount);
}

LadxReq ladx_req_counts(LadxReqArena *a, int n, const int *items, int amount)
{
	return item_node(a, LADX_NODE_COUNTS, n, items, amount);
}

LadxReq ladx_req_found(LadxReqArena *a, int item, int amount)
{
	return item_node(a, LADX_NODE_FOUND, 1, &item, amount);
}

static LadxReqNode *list_of(LadxReqArena *a, LadxReq node)
{
	LadxReqNode *nd;

	if (a->broken || node.tag != LADX_REQ_NODE)
		return NULL;
	nd = &a->nodes[node.id];
	if (nd->kind != LADX_NODE_AND && nd->kind != LADX_NODE_OR)
		return NULL;
	return nd;
}

void ladx_req_add(LadxReqArena *a, LadxReq node, LadxReq term)
{
	LadxReqNode *nd = list_of(a, node);

	if (!nd) {
		a->broken = true;
		return;
	}
	if (term.tag == LADX_REQ_ITEM) {
		if (!contains(a, nd->items_head, term.id))
			append(a, &nd->items_head, &nd->items_tail, term.id);
	} else if (term.tag == LADX_REQ_NODE) {
		if (!contains(a, nd->children_head, term.id))
			append(a, &nd->children_head, &nd->children_tail, term.id);
	} else {
		a->broken = true;
	}
}

void ladx_req_remove(LadxReqArena *a, LadxReq node, LadxReq term)
{
	LadxReqNode *nd = list_of(a, node);

	if (!nd) {
		a->broken = true;
		return;
	}
	if (term.tag == LADX_REQ_ITEM)
		unlink_first(a, &nd->items_head, &nd->items_tail, term.id);
	else if (term.tag == LADX_REQ_NODE)
		unlink_first(a, &nd->children_head, &nd->children_tail, term.id);
	else
		a->broken = true;
}

LadxNodeKind ladx_req_node_kind(const LadxReqArena *a, LadxReq node)
{
	assert(node.tag == LADX_REQ_NODE && node.id < a->node_count);
	return a->nodes[node.id].kind;
}

int ladx_req_node_amount(const LadxReqArena *a, LadxReq node)
{
	assert(node.tag == LADX_REQ_NODE && node.id < a->node_count);
	return a->nodes[node.id].amount;
}

int ladx_req_node_items(const LadxReqArena *a, LadxReq node, int *out, int cap)
{
	int n = 0, t;

	assert(node.tag == LADX_REQ_NODE && node.id < a->node_count);
	for (t = a->nodes[node.id].items_head; t >= 0; t = a->terms[t].next, n++)
		if (n < cap)
			out[n] = a->terms[t].value;
	return n;
}

int ladx_req_node_children(const LadxReqArena *a, LadxReq node, LadxReq *out, int cap)
{
	int n = 0, t;

	assert(node.tag == LADX_REQ_NODE && node.id < a->node_count);
	for (t = a->nodes[node.id].children_head; t >= 0; t = a->terms[t].next, n++)
		if (n < cap)
			out[n] = make(LADX_REQ_NODE, a->terms[t].value);
	return n;
}

void ladx_inventory_init(LadxInventory *inv)
{
	memset(inv, 0, sizeof *inv);
}

static int rupees(int item)
{
	const char *name = ladx_item_name(item);
	int n = 0;

	if (!name || strncmp(name, "RUPEES_", 7) != 0)
		return 0;
	for (name += 7; *name; name++)
		n = n * 10 + (*name - '0');
	return n;
}

void ladx_inventory_add(LadxInventory *inv, int item, int count)
{
	int r;

	if (item == LADX_ITEM_NONE)
		return;
	assert(item >= 0 && item < LADX_ITEM_COUNT);
	r = rupees(item);
	if (r) {
		item = LADX_ITEM_RUPEES;
		count *= r;
	}
	inv->count[item] += count;
	inv->present[item] = true;
}

bool ladx_inventory_has(const LadxInventory *inv, int item)
{
	return inv->present[item];
}

int ladx_inventory_count(const LadxInventory *inv, int item)
{
	return inv->count[item];
}

void ladx_item_set_clear(LadxItemSet *set)
{
	memset(set, 0, sizeof *set);
}

static bool test_node(const LadxReqArena *a, int node, const LadxInventory *inv)
{
	const LadxReqNode *nd = &a->nodes[node];
	int t, sum = 0;

	switch (nd->kind) {
	case LADX_NODE_AND:
		for (t = nd->items_head; t >= 0; t = a->terms[t].next)
			if (!inv->present[a->terms[t].value])
				return false;
		for (t = nd->children_head; t >= 0; t = a->terms[t].next)
			if (!test_node(a, a->terms[t].value, inv))
				return false;
		return true;
	case LADX_NODE_OR:
		for (t = nd->items_head; t >= 0; t = a->terms[t].next)
			if (inv->present[a->terms[t].value])
				return true;
		for (t = nd->children_head; t >= 0; t = a->terms[t].next)
			if (test_node(a, a->terms[t].value, inv))
				return true;
		return false;
	case LADX_NODE_COUNTS:
		for (t = nd->items_head; t >= 0; t = a->terms[t].next)
			sum += inv->count[a->terms[t].value];
		return sum >= nd->amount;
	case LADX_NODE_COUNT:
	case LADX_NODE_FOUND:
		return inv->count[a->terms[nd->items_head].value] >= nd->amount;
	}
	return false;
}

bool ladx_req_test(const LadxReqArena *a, LadxReq req, const LadxInventory *inv)
{
	switch (req.tag) {
	case LADX_REQ_NONE:
	case LADX_REQ_TRUE:
		return true;
	case LADX_REQ_FALSE:
		return false;
	case LADX_REQ_ITEM:
		return inv->present[req.id];
	case LADX_REQ_NODE:
		return test_node(a, req.id, inv);
	}
	return false;
}

static void node_items(const LadxReqArena *a, int node, const LadxInventory *inv, LadxItemSet *out)
{
	const LadxReqNode *nd = &a->nodes[node];
	int t;

	if (test_node(a, node, inv))
		return;
	for (t = nd->items_head; t >= 0; t = a->terms[t].next)
		out->has[a->terms[t].value] = true;
	for (t = nd->children_head; t >= 0; t = a->terms[t].next)
		node_items(a, a->terms[t].value, inv, out);
}

void ladx_req_get_items(const LadxReqArena *a, LadxReq req, const LadxInventory *inv, LadxItemSet *out)
{
	if (req.tag == LADX_REQ_ITEM)
		out->has[req.id] = true;
	else if (req.tag == LADX_REQ_NODE)
		node_items(a, req.id, inv, out);
}
