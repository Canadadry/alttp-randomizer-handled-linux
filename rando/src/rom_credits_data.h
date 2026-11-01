#ifndef RANDO_ROM_CREDITS_DATA_H
#define RANDO_ROM_CREDITS_DATA_H

#include "rando_items.h"
#include "rando_postfill.h"

/* Randomizer::randomizeCredits' alternatives at 219fcafd, in the order RandoCredits' indices follow. */
extern const char *const rando_credits_castle[RANDO_CREDITS_CASTLE];
extern const char *const rando_credits_sanctuary[RANDO_CREDITS_SANCTUARY];
/* The names before "'s homecoming". */
extern const char *const rando_credits_kakariko[RANDO_CREDITS_KAKARIKO];
extern const char *const rando_credits_lumberjacks[RANDO_CREDITS_LUMBERJACKS];
extern const char *const rando_credits_bridge[RANDO_CREDITS_BRIDGE];
extern const char *const rando_credits_woods[RANDO_CREDITS_WOODS];
extern const char *const rando_credits_well[RANDO_CREDITS_WELL];

/* A getItemCreditsText switch at 219fcafd. NULL items and big_key_a2 fall through to the next rule;
 * VT's Egg class case is left out, as Item::all has no Egg. */
typedef struct RandoCreditsOverride {
	const char *big_key_a2;
	const char *key;
	const char *map;
	const char *compass;
	const char *fallback;
	const char *items[RANDO_ITEM_COUNT];
} RandoCreditsOverride;

extern const RandoCreditsOverride rando_credits_zora;
extern const RandoCreditsOverride rando_credits_witch;
extern const RandoCreditsOverride rando_credits_pedestal;
extern const RandoCreditsOverride rando_credits_kakariko2;
extern const RandoCreditsOverride rando_credits_grove;
extern const RandoCreditsOverride rando_credits_house;

#endif
