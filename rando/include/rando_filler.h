#ifndef RANDO_FILLER_H
#define RANDO_FILLER_H

#include "rando_pool.h"
#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct RandoFillState {
	uint8_t order[RANDO_LOCATION_COUNT];
	uint16_t count;
} RandoFillState;

bool rando_fill_dungeon(RandoWorld *world, RandoPrng *prng, const RandoPools *pools, RandoFillState *state);

bool rando_fill_rest(RandoWorld *world, RandoPrng *prng, RandoPools *pools, RandoFillState *state);

#endif
