#ifndef LADX_REQUIREMENTS_H
#define LADX_REQUIREMENTS_H

#include "ladx_items.h"

#include <stdbool.h>

typedef enum LadxReqTag {
	LADX_REQ_NONE,
	LADX_REQ_TRUE,
	LADX_REQ_FALSE,
	LADX_REQ_ITEM,
	LADX_REQ_NODE
} LadxReqTag;

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
	int value;
	int next;
} LadxReqTerm;

typedef struct LadxReqArena {
	LadxReqNode nodes[LADX_REQ_NODE_CAP];
	LadxReqTerm terms[LADX_REQ_TERM_CAP];
	int node_count;
	int term_count;
	bool broken;
} LadxReqArena;

void ladx_req_arena_init(LadxReqArena *a);
bool ladx_req_arena_broken(const LadxReqArena *a);

LadxReq ladx_req_none(void);
LadxReq ladx_req_true(void);
LadxReq ladx_req_false(void);
LadxReq ladx_req_bool(bool value);
LadxReq ladx_req_item(int item);

LadxReq ladx_req_and(LadxReqArena *a, int n, const LadxReq *terms);
LadxReq ladx_req_or(LadxReqArena *a, int n, const LadxReq *terms);
LadxReq ladx_req_count(LadxReqArena *a, int item, int amount);
LadxReq ladx_req_counts(LadxReqArena *a, int n, const int *items, int amount);
LadxReq ladx_req_found(LadxReqArena *a, int item, int amount);

#define LADX_REQ_TERMS_(...) (sizeof((LadxReq[]){__VA_ARGS__}) / sizeof(LadxReq))
#define LADX_AND(a, ...) ladx_req_and((a), (int)LADX_REQ_TERMS_(__VA_ARGS__), (LadxReq[]){__VA_ARGS__})
#define LADX_OR(a, ...) ladx_req_or((a), (int)LADX_REQ_TERMS_(__VA_ARGS__), (LadxReq[]){__VA_ARGS__})

void ladx_req_add(LadxReqArena *a, LadxReq node, LadxReq term);
void ladx_req_remove(LadxReqArena *a, LadxReq node, LadxReq term);

LadxNodeKind ladx_req_node_kind(const LadxReqArena *a, LadxReq node);
int ladx_req_node_amount(const LadxReqArena *a, LadxReq node);
int ladx_req_node_items(const LadxReqArena *a, LadxReq node, int *out, int cap);
int ladx_req_node_children(const LadxReqArena *a, LadxReq node, LadxReq *out, int cap);

typedef struct LadxInventory {
	int count[LADX_ITEM_COUNT];
	bool present[LADX_ITEM_COUNT];
} LadxInventory;

void ladx_inventory_init(LadxInventory *inv);
void ladx_inventory_add(LadxInventory *inv, int item, int count);
bool ladx_inventory_has(const LadxInventory *inv, int item);
int ladx_inventory_count(const LadxInventory *inv, int item);

typedef struct LadxItemSet {
	bool has[LADX_ITEM_COUNT];
} LadxItemSet;

void ladx_item_set_clear(LadxItemSet *set);

bool ladx_req_test(const LadxReqArena *a, LadxReq req, const LadxInventory *inv);
void ladx_req_get_items(const LadxReqArena *a, LadxReq req, const LadxInventory *inv, LadxItemSet *out);

#endif
