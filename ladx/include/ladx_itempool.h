#ifndef LADX_ITEMPOOL_H
#define LADX_ITEMPOOL_H

#include "ladx_items.h"
#include "ladx_outcome.h"
#include "ladx_rng.h"
#include "ladx_settings.h"

#include <stdbool.h>

typedef struct LadxItemPool {
	int size;
	int item[LADX_ITEM_COUNT];
	int count[LADX_ITEM_COUNT];
} LadxItemPool;

void ladx_itempool_clear(LadxItemPool *pool);
int ladx_itempool_size(const LadxItemPool *pool);
bool ladx_itempool_entry(const LadxItemPool *pool, int i, int *item, int *count);
bool ladx_itempool_contains(const LadxItemPool *pool, int item);
int ladx_itempool_get(const LadxItemPool *pool, int item);

bool ladx_itempool_set(LadxItemPool *pool, int item, int count);
bool ladx_itempool_del(LadxItemPool *pool, int item);
bool ladx_itempool_add(LadxItemPool *pool, int item, int count);
bool ladx_itempool_remove(LadxItemPool *pool, int item, int count);

LadxRandomizeStatus ladx_itempool_build(LadxItemPool *pool, const LadxSettings *validated, LadxRng *rnd,
					LadxOutcome *outcome);

#endif
