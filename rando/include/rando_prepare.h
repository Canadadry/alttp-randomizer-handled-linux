#ifndef RANDO_PREPARE_H
#define RANDO_PREPARE_H

#include "rando_pool.h"
#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>

/* Randomizer::__construct: the default pre-collected items, only when the world has none. */
void rando_prepare_start_equipment(RandoWorld *world);

/* prepareWorld before its pool getters: completionist's accessibility switch and the goal's Triforce. */
void rando_prepare_goal(RandoWorld *world);

/* prepareWorld after its pool getters: PegasusBoots and RescueZelda into the pre-collected items, and
 * the glitched logics' list edit (the first PegasusBoots out of the advancement list, TwentyRupees
 * appended to the item pool). */
void rando_prepare_pre_collect(RandoWorld *world, RandoPools *pools);

/* Randomizer::setShops. prng may be NULL when none of generic keys, rupee bow and take-anys is on:
 * that path draws nothing. Otherwise 5 draws, plus 5 under take-anys. */
void rando_set_shops(RandoWorld *world, RandoPrng *prng);

/* One draw per Medallions location that holds no item yet; a filled one is skipped without a draw. */
void rando_set_medallions(RandoWorld *world, RandoPrng *prng);

/* One getBottle(true) draw per Fountains location that holds no item yet; a filled one is skipped without a draw. */
void rando_set_fountains(RandoWorld *world, RandoPrng *prng);

/* Unless custom_prize_packs, exactly 7 draws: packs 0 to 6 get VT's vanilla rows in shuffled order.
 * The four special packs are untouched. */
void rando_shuffle_prize_packs(RandoWorld *world, RandoPrng *prng);

/* prepareWorld after shufflePrizePacks: its pre-fills, list moves and exactly three shuffles (advancement,
 * nice, item pool); the dungeon list is not shuffled. Under swordless it calls collectItems. */
void rando_prepare_pools(RandoWorld *world, RandoPrng *prng, RandoPools *pools);

/* Randomizer::placeBosses with enemizer.bossShuffle none, the only path the matrix reaches: it draws nothing. */
void rando_place_bosses(RandoWorld *world);

/* Randomizer::fillPrizes with VT's five resets. False where VT throws Cannot Place Prize: the attempt
 * failed, and the world holds whatever the last pass left. */
bool rando_fill_prizes(RandoWorld *world, RandoPrng *prng);

#endif
