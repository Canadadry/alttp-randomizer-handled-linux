#ifndef RANDO_POSTFILL_DRAW_H
#define RANDO_POSTFILL_DRAW_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdint.h>

uint8_t rando_pick_first(RandoPrng *prng, int count);

int rando_locations_with(const RandoWorld *world, RandoItemId item, uint8_t out[RANDO_LOCATION_COUNT]);
RandoLocationId rando_first_with(const RandoWorld *world, RandoItemId item);

#endif
