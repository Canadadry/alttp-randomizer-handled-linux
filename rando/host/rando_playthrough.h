#ifndef RANDO_PLAYTHROUGH_H
#define RANDO_PLAYTHROUGH_H

#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_PLAYTHROUGH_ENTRY_MAX (RANDO_LOCATION_COUNT + RANDO_SHOP_COUNT * RANDO_SHOP_SLOTS)

/* location is RANDO_LOCATION_NONE for a shop slot. item is the raw item, never an alias target. region
 * is the location's in the world's variant, or the shop's. */
typedef struct RandoPlaythroughEntry {
	uint16_t round;
	RandoRegionId region;
	RandoLocationId location;
	RandoShopId shop;
	uint8_t slot;
	RandoItemId item;
} RandoPlaythroughEntry;

/* VT's getPlayThrough($world, true). Dropped rounds have no entry, so round numbers may skip. */
typedef struct RandoPlaythrough {
	int nequipped;
	RandoItemId equipped[RANDO_PRE_COLLECTED_MAX];
	int nentries;
	RandoPlaythroughEntry entries[RANDO_PLAYTHROUGH_ENTRY_MAX];
	int longest_item_chain;
	int regions_visited;
} RandoPlaythrough;

/* VT's $location->getRegion()->getName(): a shop slot's is its shop's region. */
const char *rando_playthrough_entry_region(const RandoPlaythroughEntry *entry);

/* Leaves world unchanged. */
void rando_playthrough(const RandoWorld *world, RandoPlaythrough *out);

#endif
