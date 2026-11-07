#ifndef RANDO_PLAYTHROUGH_H
#define RANDO_PLAYTHROUGH_H

#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_PLAYTHROUGH_ENTRY_MAX (RANDO_LOCATION_COUNT + RANDO_SHOP_COUNT * RANDO_SHOP_SLOTS)

typedef struct RandoPlaythroughEntry {
	uint16_t round;
	RandoRegionId region;
	RandoLocationId location;
	RandoShopId shop;
	uint8_t slot;
	RandoItemId item;
} RandoPlaythroughEntry;

typedef struct RandoPlaythrough {
	int nequipped;
	RandoItemId equipped[RANDO_PRE_COLLECTED_MAX];
	int nentries;
	RandoPlaythroughEntry entries[RANDO_PLAYTHROUGH_ENTRY_MAX];
	int longest_item_chain;
	int regions_visited;
} RandoPlaythrough;

const char *rando_playthrough_entry_region(const RandoPlaythroughEntry *entry);

void rando_playthrough(const RandoWorld *world, RandoPlaythrough *out);

#endif
