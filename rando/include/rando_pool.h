#ifndef RANDO_POOL_H
#define RANDO_POOL_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_POOL_CAPACITY 256

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

void rando_pool_build(const RandoWorld *world, RandoPrng *prng, RandoPools *pools);

void rando_pool_add_items(const RandoWorld *world, RandoPrng *prng, RandoPoolList list, RandoItems *items);

void rando_pool_append(RandoPools *pools, RandoPoolList list, RandoItemId item);
int rando_pool_find(const RandoPools *pools, RandoPoolList list, RandoItemId item);
bool rando_pool_remove_first(RandoPools *pools, RandoPoolList list, RandoItemId item);

RandoItemId rando_get_bottle(const RandoWorld *world, RandoPrng *prng, bool filled);

#endif
