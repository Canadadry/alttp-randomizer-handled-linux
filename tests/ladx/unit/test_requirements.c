#include "ladx_items.h"
#include "ladx_requirements.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_requirements"

static int checks;

static void expect(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

/* locations/items.py's order first (125 names), then logic's own names. */
static void test_items(void)
{
	int i;

	expect(LADX_ITEM_COUNT == 162, "162 item names");
	expect(LADX_ITEM_POWER_BRACELET == 0, "items.py's first name first");
	expect(strcmp(ladx_item_name(LADX_ITEM_EAGLE_TOWER_OPENED), "EAGLE_TOWER_OPENED") == 0,
	       "items.py's last name");
	expect(LADX_ITEM_EAGLE_TOWER_OPENED == 124, "items.py's 125 names first");
	expect(ladx_item_find("RUPEES") == LADX_ITEM_RUPEES, "RUPEES found");
	expect(ladx_item_find("D3_BOMBWALL") == LADX_ITEM_D3_BOMBWALL, "a logic-only flag found");
	expect(ladx_item_find("SWORD_USED") == LADX_ITEM_NONE, "no _USED name");
	expect(ladx_item_find("sword") == LADX_ITEM_NONE, "names are case-sensitive");
	expect(ladx_item_find("") == LADX_ITEM_NONE, "empty name unknown");
	expect(ladx_item_name(LADX_ITEM_NONE) == NULL, "no name for none");
	expect(ladx_item_name(LADX_ITEM_COUNT) == NULL, "no name past the table");
	for (i = 0; i < LADX_ITEM_COUNT; i++)
		expect(ladx_item_find(ladx_item_name(i)) == i, "every name round-trips");
}

static LadxReqArena arena;

#define I(name) ladx_req_item(LADX_ITEM_##name)
#define T ladx_req_true()
#define F ladx_req_false()
#define N ladx_req_none()

static void render(const LadxReqArena *a, LadxReq r, char *out, size_t cap);

static void append(char *out, size_t cap, const char *s)
{
	size_t len = strlen(out);

	if (len + strlen(s) + 1 > cap)
		test_fail(TEST, "rendered text too long");
	strcpy(out + len, s);
}

/* ladx_oracle.requirement_text: items, then children, in stored order. */
static void render(const LadxReqArena *a, LadxReq r, char *out, size_t cap)
{
	int items[64];
	LadxReq children[64];
	int ni, nc, i;
	char num[16];

	switch (r.tag) {
	case LADX_REQ_NONE:
		append(out, cap, "-");
		return;
	case LADX_REQ_TRUE:
		append(out, cap, "T");
		return;
	case LADX_REQ_FALSE:
		append(out, cap, "F");
		return;
	case LADX_REQ_ITEM:
		append(out, cap, ladx_item_name(r.id));
		return;
	default:
		break;
	}
	ni = ladx_req_node_items(a, r, items, 64);
	nc = ladx_req_node_children(a, r, children, 64);
	if (ni > 64 || nc > 64)
		test_fail(TEST, "node too wide to render");
	sprintf(num, "%d", ladx_req_node_amount(a, r));
	switch (ladx_req_node_kind(a, r)) {
	case LADX_NODE_AND:
	case LADX_NODE_OR:
		append(out, cap, ladx_req_node_kind(a, r) == LADX_NODE_AND ? "and(" : "or(");
		for (i = 0; i < ni; i++) {
			if (i)
				append(out, cap, ",");
			append(out, cap, ladx_item_name(items[i]));
		}
		for (i = 0; i < nc; i++) {
			if (i || ni)
				append(out, cap, ",");
			render(a, children[i], out, cap);
		}
		append(out, cap, ")");
		return;
	case LADX_NODE_COUNT:
	case LADX_NODE_FOUND:
		append(out, cap, ladx_req_node_kind(a, r) == LADX_NODE_COUNT ? "count(" : "found(");
		append(out, cap, ladx_item_name(items[0]));
		append(out, cap, ",");
		append(out, cap, num);
		append(out, cap, ")");
		return;
	case LADX_NODE_COUNTS:
		append(out, cap, "counts(");
		append(out, cap, num);
		for (i = 0; i < ni; i++) {
			append(out, cap, ",");
			append(out, cap, ladx_item_name(items[i]));
		}
		append(out, cap, ")");
		return;
	default:
		test_fail(TEST, "unknown node kind");
	}
}

static void expect_text(LadxReq r, const char *want, const char *what)
{
	char got[1024] = "";

	render(&arena, r, got, sizeof got);
	checks++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: got %s, want %s", what, got, want);
}

/* requirements.py OR.__new__/__init__ and AND.__new__/__init__ (lines 8-20, 63-74). */
static void test_folding(void)
{
	ladx_req_arena_init(&arena);
	expect_text(LADX_OR(&arena, I(SWORD), T, I(BOMB)), "T", "OR with a True is True");
	expect_text(LADX_OR(&arena, F, F), "F", "OR of only False is False");
	expect_text(ladx_req_or(&arena, 0, NULL), "F", "OR() is False");
	expect_text(LADX_OR(&arena, F, I(SWORD), N, F, I(BOMB)), "or(SWORD,BOMB)",
		    "OR drops False and None terms");
	expect_text(LADX_AND(&arena, I(SWORD), F, T), "F", "AND with a False is False");
	expect_text(LADX_AND(&arena, T, T), "T", "AND of only True is True");
	expect_text(ladx_req_and(&arena, 0, NULL), "T", "AND() is True");
	expect_text(LADX_AND(&arena, T, I(SWORD), T), "and(SWORD)", "AND drops True terms");
	expect_text(LADX_AND(&arena, N), "and()", "AND(None) is an empty AND node");
	expect_text(LADX_AND(&arena, N, T), "and()", "AND(None, True) is an empty AND node");
	expect(!ladx_req_arena_broken(&arena), "no folding breaks the arena");
	expect_text(LADX_OR(&arena, N, F), "-", "OR(None, False) asserts in LADXR");
	expect(ladx_req_arena_broken(&arena), "an empty OR node breaks the arena");
}

/* __init__ keeps items (str args) then children, each in argument order, duplicates kept. */
static void test_stored_order(void)
{
	LadxReq inner, c, outer;
	int items[] = {LADX_ITEM_SEASHELL, LADX_ITEM_RUPEES};

	ladx_req_arena_init(&arena);
	inner = LADX_OR(&arena, I(BOW), I(HOOKSHOT));
	c = ladx_req_count(&arena, LADX_ITEM_SWORD, 2);
	outer = LADX_AND(&arena, inner, I(SWORD), c, I(BOMB), inner, I(SWORD), N);
	expect_text(outer, "and(SWORD,BOMB,SWORD,or(BOW,HOOKSHOT),count(SWORD,2),or(BOW,HOOKSHOT))",
		    "items first, then children, duplicates kept");
	expect_text(ladx_req_counts(&arena, 2, items, 5), "counts(5,SEASHELL,RUPEES)", "COUNTS keeps its item order");
	expect_text(ladx_req_found(&arena, LADX_ITEM_KEY1, 3), "found(KEY1,3)", "FOUND keeps its item and amount");
	expect(ladx_req_node_kind(&arena, c) == LADX_NODE_COUNT, "COUNT is its own kind");
	expect(!ladx_req_arena_broken(&arena), "the arena is sound");
}

/* add (lines 32-38, 85-91): appended unless present, by name or by identity;
 * remove (26-30, 79-83): first occurrence only. */
static void test_add_remove(void)
{
	LadxReq a, b, same_text, parent;

	ladx_req_arena_init(&arena);
	a = LADX_OR(&arena, I(SWORD), I(BOMB), I(SWORD));
	b = LADX_OR(&arena, I(BOW));
	same_text = LADX_OR(&arena, I(BOW));
	parent = LADX_AND(&arena, I(FEATHER), b);
	ladx_req_add(&arena, a, I(BOW));
	ladx_req_add(&arena, a, I(BOMB));
	expect_text(a, "or(SWORD,BOMB,SWORD,BOW)", "add appends a new item, skips a present one");
	ladx_req_add(&arena, parent, b);
	ladx_req_add(&arena, parent, same_text);
	expect_text(parent, "and(FEATHER,or(BOW),or(BOW))", "add compares children by identity");
	ladx_req_remove(&arena, a, I(SWORD));
	expect_text(a, "or(BOMB,SWORD,BOW)", "remove deletes the first occurrence");
	ladx_req_remove(&arena, a, I(HOOKSHOT));
	expect_text(a, "or(BOMB,SWORD,BOW)", "removing an absent item changes nothing");
	ladx_req_remove(&arena, parent, same_text);
	expect_text(parent, "and(FEATHER,or(BOW))", "remove matches a child by identity");
	ladx_req_remove(&arena, a, I(BOW));
	ladx_req_add(&arena, a, I(HOOKSHOT));
	expect_text(a, "or(BOMB,SWORD,HOOKSHOT)", "add after removing the last term");
	expect(!ladx_req_arena_broken(&arena), "the arena is sound");
	ladx_req_add(&arena, ladx_req_count(&arena, LADX_ITEM_SWORD, 2), I(BOMB));
	expect(ladx_req_arena_broken(&arena), "add on a COUNT breaks the arena");
	ladx_req_arena_init(&arena);
	ladx_req_add(&arena, LADX_OR(&arena, I(SWORD)), N);
	expect(ladx_req_arena_broken(&arena), "adding None breaks the arena");
}

/* RequirementsSettings removes items from nodes already shared by several edges. */
static void test_shared_identity(void)
{
	LadxReq bush, edge1, edge2;

	ladx_req_arena_init(&arena);
	bush = LADX_OR(&arena, I(SWORD), I(MAGIC_POWDER), I(MAGIC_ROD));
	edge1 = bush;
	edge2 = LADX_AND(&arena, I(FEATHER), bush);
	ladx_req_remove(&arena, bush, I(SWORD));
	expect_text(edge1, "or(MAGIC_POWDER,MAGIC_ROD)", "a removal is seen through the first edge");
	expect_text(edge2, "and(FEATHER,or(MAGIC_POWDER,MAGIC_ROD))", "and through a node holding it");
}

/* Explorer.addItem: present once added (dict key), RUPEES_<n> folded into RUPEES. */
static void test_inventory(void)
{
	LadxInventory inv;
	LadxReq c;

	ladx_inventory_init(&inv);
	ladx_req_arena_init(&arena);
	expect(!ladx_inventory_has(&inv, LADX_ITEM_SWORD), "an empty inventory holds nothing");
	ladx_inventory_add(&inv, LADX_ITEM_SWORD, 0);
	expect(ladx_inventory_has(&inv, LADX_ITEM_SWORD), "present after a zero-count add");
	expect(ladx_inventory_count(&inv, LADX_ITEM_SWORD) == 0, "with a count of 0");
	expect(ladx_req_test(&arena, I(SWORD), &inv), "a bare item tests its presence");
	c = ladx_req_count(&arena, LADX_ITEM_SWORD, 1);
	expect(!ladx_req_test(&arena, c, &inv), "COUNT tests the count, not the presence");
	ladx_inventory_add(&inv, LADX_ITEM_SWORD, 2);
	expect(ladx_inventory_count(&inv, LADX_ITEM_SWORD) == 2, "counts add up");
	ladx_inventory_add(&inv, LADX_ITEM_RUPEES_50, 3);
	ladx_inventory_add(&inv, LADX_ITEM_RUPEES_200, 2);
	expect(ladx_inventory_count(&inv, LADX_ITEM_RUPEES) == 550, "RUPEES_<n> adds n x count to RUPEES");
	expect(ladx_inventory_has(&inv, LADX_ITEM_RUPEES), "RUPEES present");
	expect(!ladx_inventory_has(&inv, LADX_ITEM_RUPEES_50), "RUPEES_50 itself never present");
	ladx_inventory_add(&inv, LADX_ITEM_NONE, 1);
	expect(ladx_inventory_count(&inv, LADX_ITEM_RUPEES) == 550, "adding None changes nothing");
}

/* test (lines 40-46, 93-99, 123, 142-146, 165) and the reference kinds of testRequirements. */
static void test_tests(void)
{
	LadxInventory inv;
	int counted[] = {LADX_ITEM_SEASHELL, LADX_ITEM_GOLD_LEAF};
	LadxReq counts, found, or_node, and_node, empty_and;

	ladx_inventory_init(&inv);
	ladx_req_arena_init(&arena);
	counts = ladx_req_counts(&arena, 2, counted, 5);
	found = ladx_req_found(&arena, LADX_ITEM_RUPEES, 300);
	or_node = LADX_OR(&arena, I(BOW), LADX_AND(&arena, I(SWORD), I(SHIELD)));
	and_node = LADX_AND(&arena, I(FEATHER), or_node);
	empty_and = LADX_AND(&arena, N);
	expect(ladx_req_test(&arena, N, &inv) && ladx_req_test(&arena, T, &inv), "None and True pass");
	expect(!ladx_req_test(&arena, F, &inv), "False fails");
	expect(ladx_req_test(&arena, empty_and, &inv), "the empty AND passes");
	ladx_inventory_add(&inv, LADX_ITEM_SEASHELL, 3);
	ladx_inventory_add(&inv, LADX_ITEM_GOLD_LEAF, 1);
	expect(!ladx_req_test(&arena, counts, &inv), "COUNTS: 4 of 5 fails");
	ladx_inventory_add(&inv, LADX_ITEM_GOLD_LEAF, 1);
	expect(ladx_req_test(&arena, counts, &inv), "COUNTS: the sum reaches 5");
	ladx_inventory_add(&inv, LADX_ITEM_RUPEES_200, 1);
	ladx_inventory_add(&inv, LADX_ITEM_RUPEES_50, 1);
	expect(!ladx_req_test(&arena, found, &inv), "FOUND: 250 of 300 fails");
	ladx_inventory_add(&inv, LADX_ITEM_RUPEES_50, 1);
	expect(ladx_req_test(&arena, found, &inv), "FOUND: 300 reaches 300");
	ladx_inventory_add(&inv, LADX_ITEM_SWORD, 1);
	expect(!ladx_req_test(&arena, or_node, &inv), "OR: no term holds");
	ladx_inventory_add(&inv, LADX_ITEM_SHIELD, 1);
	expect(ladx_req_test(&arena, or_node, &inv), "OR: a child holds");
	expect(!ladx_req_test(&arena, and_node, &inv), "AND: an item missing");
	ladx_inventory_add(&inv, LADX_ITEM_FEATHER, 1);
	expect(ladx_req_test(&arena, and_node, &inv), "AND: every term holds");
}

static void expect_set(const LadxItemSet *set, const char *want, const char *what)
{
	char got[1024] = "";
	int i;

	for (i = 0; i < LADX_ITEM_COUNT; i++) {
		if (!set->has[i])
			continue;
		if (got[0])
			append(got, sizeof got, ",");
		append(got, sizeof got, ladx_item_name(i));
	}
	checks++;
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: got %s, want %s", what, got, want);
}

/* getItems (lines 48-54, 101-107, 125-128, 148-153, 167-170): nothing for a satisfied node. */
static void test_get_items(void)
{
	LadxInventory inv;
	LadxItemSet set;
	int counted[] = {LADX_ITEM_SEASHELL, LADX_ITEM_GOLD_LEAF};
	LadxReq sat, req;

	ladx_inventory_init(&inv);
	ladx_req_arena_init(&arena);
	ladx_inventory_add(&inv, LADX_ITEM_BOW, 1);
	sat = LADX_OR(&arena, I(BOW), I(HOOKSHOT));
	req = LADX_AND(&arena, I(FEATHER), I(BOW), sat, ladx_req_count(&arena, LADX_ITEM_SWORD, 2),
		       ladx_req_counts(&arena, 2, counted, 5), ladx_req_found(&arena, LADX_ITEM_KEY3, 1),
		       LADX_OR(&arena, I(MAGIC_ROD), ladx_req_count(&arena, LADX_ITEM_SHIELD, 1)));
	ladx_item_set_clear(&set);
	ladx_req_get_items(&arena, req, &inv, &set);
	expect_set(&set, "SHIELD,BOW,MAGIC_ROD,FEATHER,SWORD,GOLD_LEAF,SEASHELL,KEY3",
		   "an unmet node's items, every unmet child's, none of the met OR's");
	ladx_item_set_clear(&set);
	ladx_req_get_items(&arena, sat, &inv, &set);
	expect_set(&set, "", "a satisfied node adds nothing");
	ladx_req_get_items(&arena, I(BOW), &inv, &set);
	expect_set(&set, "BOW", "a bare item is added even when held");
	ladx_item_set_clear(&set);
	ladx_req_get_items(&arena, N, &inv, &set);
	ladx_req_get_items(&arena, T, &inv, &set);
	ladx_req_get_items(&arena, F, &inv, &set);
	expect_set(&set, "", "None, True and False add nothing");
}

static void test_capacity(void)
{
	static int many[LADX_REQ_TERM_CAP];
	int i;

	ladx_req_arena_init(&arena);
	for (i = 0; i < LADX_REQ_NODE_CAP; i++)
		ladx_req_count(&arena, LADX_ITEM_SWORD, 1);
	expect(!ladx_req_arena_broken(&arena), "the node capacity is usable whole");
	expect(ladx_req_count(&arena, LADX_ITEM_SWORD, 1).tag == LADX_REQ_NONE, "a node past the capacity is none");
	expect(ladx_req_arena_broken(&arena), "a node past the capacity breaks the arena");
	expect(LADX_AND(&arena, I(SWORD), I(BOMB)).tag == LADX_REQ_NONE, "a broken arena builds nothing");
	ladx_req_arena_init(&arena);
	for (i = 0; i < LADX_REQ_TERM_CAP; i++)
		many[i] = LADX_ITEM_SEASHELL;
	ladx_req_counts(&arena, LADX_REQ_TERM_CAP, many, 1);
	expect(!ladx_req_arena_broken(&arena), "the term capacity is usable whole");
	expect(ladx_req_count(&arena, LADX_ITEM_SWORD, 1).tag == LADX_REQ_NONE, "a term past the capacity is none");
	expect(ladx_req_arena_broken(&arena), "a term past the capacity breaks the arena");
}

int main(void)
{
	test_items();
	test_folding();
	test_stored_order();
	test_add_remove();
	test_shared_identity();
	test_inventory();
	test_tests();
	test_get_items();
	test_capacity();
	printf("%s: %d item names; folding, stored order, add and remove on shared nodes, inventory, test and getItems ok (%d checks)\n",
	       TEST, LADX_ITEM_COUNT, checks);
	return 0;
}
