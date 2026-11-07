#ifndef LADX_ITEMPOOL_H
#define LADX_ITEMPOOL_H

#include "ladx_items.h"
#include "ladx_outcome.h"
#include "ladx_rng.h"
#include "ladx_settings.h"

#include <stdbool.h>

/*
 * An item-count map with Python dict semantics: insertion order, a deleted
 * key re-added at the end, zero and negative counts kept. Copy by assignment.
 */
typedef struct LadxItemPool {
	int size;
	int item[LADX_ITEM_COUNT];
	int count[LADX_ITEM_COUNT];
} LadxItemPool;

void ladx_itempool_clear(LadxItemPool *pool);
int ladx_itempool_size(const LadxItemPool *pool);
/* Entry i in insertion order; false, nothing set, out of range. */
bool ladx_itempool_entry(const LadxItemPool *pool, int i, int *item, int *count);
bool ladx_itempool_contains(const LadxItemPool *pool, int item);
/* dict.get(item, 0). */
int ladx_itempool_get(const LadxItemPool *pool, int item);

/* Each returns false, the pool untouched, for an item outside the table. */
/* d[item] = count: an absent key goes at the end. */
bool ladx_itempool_set(LadxItemPool *pool, int item, int count);
/* del d[item]; an absent key is left absent. */
bool ladx_itempool_del(LadxItemPool *pool, int item);
/* ItemPool.add: d[item] = d.get(item, 0) + count, the key kept at zero. */
bool ladx_itempool_add(LadxItemPool *pool, int item, int count);
/* ItemPool.remove: d[item] = d.get(item, 0) - count, deleted at exactly zero. */
bool ladx_itempool_remove(LadxItemPool *pool, int item, int count);

/*
 * itempool.py ItemPool(logic, settings, rnd, False).toDict() on validated
 * settings with overworld=normal and no shopsanity (else UNSUPPORTED):
 * __setup in statement order, then __randomizeRupees. OK with outcome
 * UNFINISHED when built, or with LADXR's RuntimeError as a CRASH outcome.
 */
LadxRandomizeStatus ladx_itempool_build(LadxItemPool *pool, const LadxSettings *validated, LadxRng *rnd,
					LadxOutcome *outcome);

#endif
