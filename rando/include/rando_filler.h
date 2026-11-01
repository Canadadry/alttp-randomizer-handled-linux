#ifndef RANDO_FILLER_H
#define RANDO_FILLER_H

#include "rando_pool.h"
#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

/* Caller-owned; rando_fill_dungeon sets it and rando_fill_rest reads and rewrites it. */
typedef struct RandoFillState {
	uint8_t order[RANDO_LOCATION_COUNT];
	uint16_t count;
} RandoFillState;

/* RandomAssumed::fill up to its junk fill: the empty locations shuffled into state, then the dungeon
 * list placed with the advancement and nice lists assumed. False where VT throws; the world then
 * holds what was placed before. */
bool rando_fill_dungeon(RandoWorld *world, RandoPrng *prng, const RandoPools *pools, RandoFillState *state);

/* The rest of RandomAssumed::fill: Ganon's Tower junk, advancement, nice, then the item pool. The
 * pools' contents are unspecified on return. False where VT throws, as rando_fill_dungeon. */
bool rando_fill_rest(RandoWorld *world, RandoPrng *prng, RandoPools *pools, RandoFillState *state);

#endif
