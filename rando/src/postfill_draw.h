#ifndef RANDO_POSTFILL_DRAW_H
#define RANDO_POSTFILL_DRAW_H

#include "rando_prng.h"

#include <stdint.h>

/* Arr::first(fy_shuffle(alternatives)) over count alternatives, 1 to 256: count draws. */
uint8_t rando_pick_first(RandoPrng *prng, int count);

#endif
