#ifndef RANDO_ROM_CREDITS_DATA_H
#define RANDO_ROM_CREDITS_DATA_H

#include "rando_items.h"
#include "rando_postfill.h"

extern const char *const rando_credits_castle[RANDO_CREDITS_CASTLE];
extern const char *const rando_credits_sanctuary[RANDO_CREDITS_SANCTUARY];
extern const char *const rando_credits_kakariko[RANDO_CREDITS_KAKARIKO];
extern const char *const rando_credits_lumberjacks[RANDO_CREDITS_LUMBERJACKS];
extern const char *const rando_credits_bridge[RANDO_CREDITS_BRIDGE];
extern const char *const rando_credits_woods[RANDO_CREDITS_WOODS];
extern const char *const rando_credits_well[RANDO_CREDITS_WELL];

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
