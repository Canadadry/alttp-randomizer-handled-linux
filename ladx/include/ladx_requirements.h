#ifndef LADX_REQUIREMENTS_H
#define LADX_REQUIREMENTS_H

#include "ladx_items.h"

#include <stdbool.h>

/* LADXR's logic/requirements.py at 4325207e: AND, OR, COUNT, COUNTS and
 * FOUND as nodes of a caller-owned arena, with no allocation. */

typedef enum LadxReqTag {
	LADX_REQ_NONE, /* Python's None: no requirement */
	LADX_REQ_TRUE,
	LADX_REQ_FALSE,
	LADX_REQ_ITEM, /* id: a LadxItem */
	LADX_REQ_NODE  /* id: a node of the arena */
} LadxReqTag;

/* Two NODE references are the same node exactly when LADXR's objects are
 * the same object. */
typedef struct LadxReq {
	LadxReqTag tag;
	int id;
} LadxReq;

typedef enum LadxNodeKind {
	LADX_NODE_AND,
	LADX_NODE_OR,
	LADX_NODE_COUNT,
	LADX_NODE_COUNTS,
	LADX_NODE_FOUND
} LadxNodeKind;

#define LADX_REQ_NODE_CAP 4096
#define LADX_REQ_TERM_CAP 16384

typedef struct LadxReqNode {
	LadxNodeKind kind;
	int amount;
	int items_head, items_tail;
	int children_head, children_tail;
} LadxReqNode;

typedef struct LadxReqTerm {
	int value; /* an item, or a child's node index */
	int next;  /* -1 at the end */
} LadxReqTerm;

/* Read only through the functions below. */
typedef struct LadxReqArena {
	LadxReqNode nodes[LADX_REQ_NODE_CAP];
	LadxReqTerm terms[LADX_REQ_TERM_CAP];
	int node_count;
	int term_count;
	bool broken;
} LadxReqArena;

void ladx_req_arena_init(LadxReqArena *a);
/* Set by the first capacity overflow or LADXR assertion; every later
 * constructor, add and remove is then a no-op returning a NONE reference. */
bool ladx_req_arena_broken(const LadxReqArena *a);

LadxReq ladx_req_none(void);
LadxReq ladx_req_true(void);
LadxReq ladx_req_false(void);
LadxReq ladx_req_bool(bool value);
LadxReq ladx_req_item(int item);

/* LADXR's __new__ folding, then a node keeping the ITEM terms, then the
 * NODE terms, each in argument order, duplicates kept; NONE terms are
 * dropped. An OR left with no item and no child breaks the arena. */
LadxReq ladx_req_and(LadxReqArena *a, int n, const LadxReq *terms);
LadxReq ladx_req_or(LadxReqArena *a, int n, const LadxReq *terms);
LadxReq ladx_req_count(LadxReqArena *a, int item, int amount);
LadxReq ladx_req_counts(LadxReqArena *a, int n, const int *items, int amount);
LadxReq ladx_req_found(LadxReqArena *a, int item, int amount);

#define LADX_REQ_TERMS_(...) (sizeof((LadxReq[]){__VA_ARGS__}) / sizeof(LadxReq))
#define LADX_AND(a, ...) ladx_req_and((a), (int)LADX_REQ_TERMS_(__VA_ARGS__), (LadxReq[]){__VA_ARGS__})
#define LADX_OR(a, ...) ladx_req_or((a), (int)LADX_REQ_TERMS_(__VA_ARGS__), (LadxReq[]){__VA_ARGS__})

/* On an AND or OR node: add appends an ITEM or NODE term unless already
 * there (by item, or by node identity); remove deletes its first
 * occurrence, seen through every reference to the node. Any other node or
 * term breaks the arena. */
void ladx_req_add(LadxReqArena *a, LadxReq node, LadxReq term);
void ladx_req_remove(LadxReqArena *a, LadxReq node, LadxReq term);

/* On a NODE reference. Items and children are copied in stored order, at
 * most cap of them; each returns the full count. COUNT and FOUND have one
 * item; COUNTS has its items and no child. */
LadxNodeKind ladx_req_node_kind(const LadxReqArena *a, LadxReq node);
int ladx_req_node_amount(const LadxReqArena *a, LadxReq node);
int ladx_req_node_items(const LadxReqArena *a, LadxReq node, int *out, int cap);
int ladx_req_node_children(const LadxReqArena *a, LadxReq node, LadxReq *out, int cap);

/* Explorer's inventory: an item is present once added, even with a count
 * of 0; RUPEES_<n> adds n x count to RUPEES instead. */
typedef struct LadxInventory {
	int count[LADX_ITEM_COUNT];
	bool present[LADX_ITEM_COUNT];
} LadxInventory;

void ladx_inventory_init(LadxInventory *inv);
/* LADX_ITEM_NONE adds nothing. */
void ladx_inventory_add(LadxInventory *inv, int item, int count);
bool ladx_inventory_has(const LadxInventory *inv, int item);
int ladx_inventory_count(const LadxInventory *inv, int item);

typedef struct LadxItemSet {
	bool has[LADX_ITEM_COUNT];
} LadxItemSet;

void ladx_item_set_clear(LadxItemSet *set);

/* Explorer.testRequirements: NONE and TRUE pass, FALSE fails. */
bool ladx_req_test(const LadxReqArena *a, LadxReq req, const LadxInventory *inv);
/* A node's getItems; a bare ITEM is added even when held, as
 * Explorer.getRequiredItemsForNextLocations does; NONE, TRUE and FALSE add
 * nothing. */
void ladx_req_get_items(const LadxReqArena *a, LadxReq req, const LadxInventory *inv, LadxItemSet *out);

#endif
