#ifndef RANDO_POSTFILL_DRAW_H
#define RANDO_POSTFILL_DRAW_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdint.h>

/* Arr::first(fy_shuffle(alternatives)) over count alternatives, 1 to 256: count draws. */
uint8_t rando_pick_first(RandoPrng *prng, int count);

/* World::getLocationsWithItem, in world location order; returns the count written to out. */
int rando_locations_with(const RandoWorld *world, RandoItemId item, uint8_t out[RANDO_LOCATION_COUNT]);
/* Its first location, or RANDO_LOCATION_NONE. */
RandoLocationId rando_first_with(const RandoWorld *world, RandoItemId item);

#endif
