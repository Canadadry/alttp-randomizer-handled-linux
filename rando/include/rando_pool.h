#ifndef RANDO_POOL_H
#define RANDO_POOL_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_POOL_CAPACITY 256

/* prepareWorld's getter order: getDungeonPool, getAdvancementItems, getNiceItems, getItemPool. */
typedef enum RandoPoolList {
	RANDO_POOL_DUNGEON,
	RANDO_POOL_ADVANCEMENT,
	RANDO_POOL_NICE,
	RANDO_POOL_ITEMS,
	RANDO_POOL_LIST_COUNT,
} RandoPoolList;

typedef struct RandoPools {
	uint8_t items[RANDO_POOL_LIST_COUNT][RANDO_POOL_CAPACITY];
	uint16_t count[RANDO_POOL_LIST_COUNT];
} RandoPools;

/* The four lists from the initialized world's derived config, in VT's item order. Asserts a list
 * stays under RANDO_POOL_CAPACITY and that a random bottle only occurs in the advancement list. */
void rando_pool_build(const RandoWorld *world, RandoPrng *prng, RandoPools *pools);

/* rando_pool_build's walk of one list, each item added to items (both views) on top of what it
 * holds; the same draws as the list's build. */
void rando_pool_add_items(const RandoWorld *world, RandoPrng *prng, RandoPoolList list, RandoItems *items);

/* Asserts the list stays under RANDO_POOL_CAPACITY. */
void rando_pool_append(RandoPools *pools, RandoPoolList list, RandoItemId item);
/* The index of the item's first occurrence in the list, -1 when absent. */
int rando_pool_find(const RandoPools *pools, RandoPoolList list, RandoItemId item);
/* Removes the item's first occurrence, shifting the rest down; false when absent. */
bool rando_pool_remove_first(RandoPools *pools, RandoPoolList list, RandoItemId item);

/* VT's World::getBottle: one draw. */
RandoItemId rando_get_bottle(const RandoWorld *world, RandoPrng *prng, bool filled);

#endif
