#include "rando_postfill.h"

#include "postfill_draw.h"
#include "rando_pool.h"
#include "text_strings.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int rando_hint_item_alternatives(RandoItemId item)
{
	assert(item < RANDO_ITEM_COUNT);
	return rando_hint_items[item].names == NULL ? RANDO_HINT_MISSING : rando_hint_items[item].count;
}

int rando_hint_location_alternatives(RandoLocationId location)
{
	assert(location < RANDO_LOCATION_COUNT);
	return rando_hint_locations[location].names == NULL ? RANDO_HINT_MISSING : rando_hint_locations[location].count;
}

/* An alternative index, or 0 with no draw for a plain string or a missing key. */
static uint8_t pick_name(RandoPrng *prng, int alternatives)
{
	return alternatives > 0 ? rando_pick_first(prng, alternatives) : 0;
}

bool rando_location_hint(const RandoWorld *world, RandoLocationId location, RandoPrng *prng, RandoLocationHint *out)
{
	RandoItemId item = rando_world_item(world, location);
	const RandoItemInfo *info;

	if (item == RANDO_ITEM_NONE)
		return false;
	info = rando_item_info(item);
	if (world->config.rom_generic_keys && info->item_class == RANDO_ITEM_CLASS_KEY)
		item = RANDO_ITEM_KeyGK;
	else if (info->item_class == RANDO_ITEM_CLASS_ALIAS)
		item = info->alias_target;
	out->location = location;
	out->item = item;
	out->item_name = pick_name(prng, rando_hint_item_alternatives(item));
	out->location_name = pick_name(prng, rando_hint_location_alternatives(location));
	return true;
}

RandoCollectionHint rando_collection_hint(const RandoWorld *world, const RandoLocationId *locations, int n,
					  RandoPrng *prng, RandoLocationHint *out)
{
	RandoLocationId prime = RANDO_LOCATION_NONE;
	int i, items = 0;

	for (i = 0; i < n; i++)
		if (rando_world_item(world, locations[i]) != RANDO_ITEM_NONE && items++ == 0)
			prime = locations[i];
	if (items == 0)
		return RANDO_COLLECTION_HINT_NONE;
	if (items == 1) {
		rando_location_hint(world, prime, prng, out);
		return RANDO_COLLECTION_HINT_ONE;
	}
	out->location = prime;
	out->item = RANDO_ITEM_NONE;
	out->item_name = 0;
	out->location_name = 0;
	return RANDO_COLLECTION_HINT_MANY;
}

/* HintService's $locations; RANDO_LOCATION_NONE marks a pair pick. */
static const RandoLocationId hint_picks[] = {
	RANDO_LOC_SAHASRAHLA,
	RANDO_LOC_MIMIC_CAVE,
	RANDO_LOC_CATFISH,
	RANDO_LOC_GRAVEYARD_LEDGE,
	RANDO_LOC_PURPLE_CHEST,
	RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST,
	RANDO_LOC_SWAMP_PALACE_BIG_CHEST,
	RANDO_LOCATION_NONE,
	RANDO_LOCATION_NONE,
	RANDO_LOCATION_NONE,
};

#define HINT_PICKS ((int)(sizeof hint_picks / sizeof hint_picks[0]))
#define HINT_PICK_ROUNDS 5
#define HINT_ITEMS_MAX 4

static bool hintable_location(RandoLocationId location)
{
	RandoLocationClass c = rando_location_info(location)->location_class;

	return !rando_location_is_a(c, RANDO_LOCATION_CLASS_MEDALLION)
		&& !rando_location_is_a(c, RANDO_LOCATION_CLASS_FOUNTAIN)
		&& !rando_location_is_a(c, RANDO_LOCATION_CLASS_PRIZE)
		&& !rando_location_is_a(c, RANDO_LOCATION_CLASS_TRADE);
}

static bool dungeon_item(const RandoWorld *world, RandoItemClass c)
{
	return c == RANDO_ITEM_CLASS_KEY || c == RANDO_ITEM_CLASS_MAP || c == RANDO_ITEM_CLASS_COMPASS
		|| (!world->config.region_wild_big_keys && c == RANDO_ITEM_CLASS_BIG_KEY);
}

static bool hintable_item(const RandoWorld *world, RandoItemId item)
{
	RandoItemClass c = rando_item_info(item)->item_class;

	return c != RANDO_ITEM_CLASS_SHIELD && !dungeon_item(world, c) && c != RANDO_ITEM_CLASS_BOTTLE
		&& c != RANDO_ITEM_CLASS_SWORD && item != RANDO_ITEM_TenBombs && item != RANDO_ITEM_HalfMagic
		&& item != RANDO_ITEM_BugCatchingNet && item != RANDO_ITEM_Powder && item != RANDO_ITEM_Mushroom;
}

static void tile_location(RandoHints *hints, uint8_t tile, const RandoLocationHint *hint)
{
	hints->tiles[tile].kind = RANDO_TILE_HINT_LOCATION;
	hints->tiles[tile].location = *hint;
}

/* Pops a tile for location's hint, as VT does only when the hint is not null. */
static void pop_tile_hint(const RandoWorld *world, RandoLocationId location, RandoPrng *prng, RandoHints *hints,
			  uint8_t *tiles, int *ntiles)
{
	RandoLocationHint hint;

	if (location == RANDO_LOCATION_NONE || !rando_location_hint(world, location, prng, &hint))
		return;
	assert(*ntiles > 0);
	tile_location(hints, tiles[--*ntiles], &hint);
}

/* The item's location for a hint, drawn by random() among those VT keeps; RANDO_LOCATION_NONE, no draw,
 * when none is left. */
static RandoLocationId hint_item_location(const RandoWorld *world, RandoItemId item, RandoLocationId alternate_bow,
					  RandoPrng *prng)
{
	uint8_t ids[RANDO_LOCATION_COUNT];
	int i, n, kept = 0;

	n = rando_locations_with(world, item, ids);
	/* setTexts' alternate bow ([0x65]) is not == to Item::get('ProgressiveBow'). */
	for (i = 0; i < n; i++)
		if (hintable_location((RandoLocationId)ids[i])
		    && !(item == RANDO_ITEM_ProgressiveBow && ids[i] == alternate_bow))
			ids[kept++] = ids[i];
	if (kept == 0)
		return RANDO_LOCATION_NONE;
	return (RandoLocationId)ids[rando_random_int(prng, 0, kept - 1)];
}

void rando_apply_hints(const RandoWorld *world, RandoPrng *prng, const uint8_t *advancement, int n,
		       RandoPostFill *post)
{
	RandoHints *hints = &post->hints;
	uint8_t tiles[RANDO_TILE_COUNT], picks_order[HINT_PICKS], picks[HINT_PICKS];
	uint8_t hintables[RANDO_POOL_CAPACITY], ids[RANDO_LOCATION_COUNT], chosen[RANDO_LOCATION_COUNT + HINT_ITEMS_MAX];
	RandoLocationId hinted[HINT_ITEMS_MAX];
	int i, j, ntiles = RANDO_TILE_COUNT, npicks = HINT_PICKS, nhintables = 0, nhinted = 0, nids = 0, nchosen;
	int free_tiles;

	assert(n >= 0 && n <= (int)sizeof hintables);
	memset(hints, 0, sizeof *hints);
	hints->on = world->config.spoil_hints;
	if (!hints->on)
		return;

	for (i = 0; i < RANDO_TILE_COUNT; i++)
		tiles[i] = (uint8_t)i;
	rando_fy_shuffle(prng, tiles, RANDO_TILE_COUNT, 1);
	for (i = 0; i < HINT_PICKS; i++)
		picks_order[i] = (uint8_t)i;
	rando_fy_shuffle(prng, picks_order, HINT_PICKS, 1);

	if (world->config.region_wild_big_keys)
		pop_tile_hint(world, rando_first_with(world, RANDO_ITEM_BigKeyA2), prng, hints, tiles, &ntiles);
	pop_tile_hint(world, rando_first_with(world, RANDO_ITEM_PegasusBoots), prng, hints, tiles, &ntiles);

	for (i = 0; i < HINT_PICKS; i++)
		picks[i] = (uint8_t)i;
	for (i = 0; i < HINT_PICK_ROUNDS; i++) {
		rando_fy_shuffle(prng, picks, (size_t)npicks, 1);
		/* A pair pick matches nothing: VT filters getName(), which has a ":<world id>" suffix. */
		pop_tile_hint(world, hint_picks[picks_order[picks[--npicks]]], prng, hints, tiles, &ntiles);
	}

	for (i = 0; i < n; i++)
		if (hintable_item(world, (RandoItemId)advancement[i]))
			hintables[nhintables++] = advancement[i];
	rando_fy_shuffle(prng, hintables, (size_t)nhintables, 1);
	for (i = 0; i < nhintables && i < HINT_ITEMS_MAX && i < ntiles; i++) {
		RandoLocationId location =
			hint_item_location(world, (RandoItemId)hintables[i], post->texts.alternate_bow, prng);

		if (location != RANDO_LOCATION_NONE)
			hinted[nhinted++] = location;
	}

	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);
		RandoItemId item = rando_world_item(world, location);

		if (item != RANDO_ITEM_NONE && hintable_location(location)
		    && !dungeon_item(world, rando_item_info(item)->item_class))
			ids[nids++] = (uint8_t)location;
	}
	free_tiles = ntiles - nhinted;
	nchosen = rando_random_collection(prng, ids, nids,
					  (int)rando_random_int(prng, free_tiles / 2 - 1, free_tiles - 1), chosen);
	/* LocationCollection keys by name: a hinted location already drawn keeps its first position. */
	for (i = 0; i < nhinted; i++) {
		for (j = 0; j < nchosen && chosen[j] != hinted[i]; j++)
			;
		if (j == nchosen)
			chosen[nchosen++] = (uint8_t)hinted[i];
	}

	for (i = 0; i < ntiles; i++) {
		RandoLocationHint hint;

		if (nchosen > 0 && rando_location_hint(world, (RandoLocationId)chosen[--nchosen], prng, &hint)) {
			tile_location(hints, tiles[i], &hint);
		} else {
			hints->tiles[tiles[i]].kind = RANDO_TILE_HINT_JOKE;
			hints->tiles[tiles[i]].joke = rando_pick_first(prng, RANDO_STRINGS_HINT);
		}
	}
}
