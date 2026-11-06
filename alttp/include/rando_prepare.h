#ifndef RANDO_PREPARE_H
#define RANDO_PREPARE_H

#include "rando_pool.h"
#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>

void rando_prepare_start_equipment(RandoWorld *world);

void rando_prepare_goal(RandoWorld *world);

void rando_prepare_pre_collect(RandoWorld *world, RandoPools *pools);

void rando_set_shops(RandoWorld *world, RandoPrng *prng);

void rando_set_medallions(RandoWorld *world, RandoPrng *prng);

void rando_set_fountains(RandoWorld *world, RandoPrng *prng);

void rando_shuffle_prize_packs(RandoWorld *world, RandoPrng *prng);

void rando_prepare_pools(RandoWorld *world, RandoPrng *prng, RandoPools *pools);

void rando_place_bosses(RandoWorld *world);

bool rando_fill_prizes(RandoWorld *world, RandoPrng *prng);

#endif
