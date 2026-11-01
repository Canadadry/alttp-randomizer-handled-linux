#include "rando_filler.h"

#include <assert.h>
#include <string.h>

typedef char location_ids_fit_uint8[RANDO_LOCATION_COUNT <= 256 ? 1 : -1];

static bool is_empty(const RandoWorld *world, uint8_t location)
{
	return !rando_world_has_item(world, (RandoLocationId)location, RANDO_ITEM_NONE);
}

/* getEmptyLocations on state's order, in place; returns the new count. */
static int keep_empty(const RandoWorld *world, RandoFillState *state)
{
	int i, n = 0;

	for (i = 0; i < state->count; i++)
		if (is_empty(world, state->order[i]))
			state->order[n++] = state->order[i];
	state->count = (uint16_t)n;
	return n;
}

static void shuffle_locations(RandoPrng *prng, RandoFillState *state)
{
	uint8_t ids[RANDO_LOCATION_COUNT];
	int n = state->count;

	memcpy(ids, state->order, (size_t)n);
	state->count = (uint16_t)rando_random_collection(prng, ids, n, n, state->order);
}

static void place(RandoWorld *world, uint8_t location, RandoItemId item)
{
	bool accepted = rando_world_set_item(world, (RandoLocationId)location, item);

	assert(accepted && "fill: an empty location refuses the item (an unfilled Prize or Medallion?)");
	(void)accepted;
}

/* Filler::fastFillItemsInLocations: items are popped from the end of items[0..n). */
static void fast_fill(RandoWorld *world, const uint8_t *items, int n, const uint8_t *locations, int nlocations)
{
	int i;

	for (i = 0; i < nlocations; i++) {
		if (!is_empty(world, locations[i]))
			continue;
		if (n == 0)
			break;
		place(world, locations[i], (RandoItemId)items[--n]);
	}
}

static bool can_skip_access_checks(const RandoWorld *world, RandoItemId item, const RandoItems *assumed)
{
	const RandoConfig *c = &world->config;
	RandoItemClass item_class = rando_item_info(item)->item_class;

	return c->accessibility == RANDO_ACCESSIBILITY_NONE && !rando_dungeon_item_confined(c, item_class)
		&& rando_world_win_condition(world, assumed);
}

/* RandomAssumed::fillItemsInLocations; base may be NULL (VT's empty $base_assumed_items). */
static bool fill_items_in_locations(RandoWorld *world, RandoPrng *prng, const uint8_t *items, int nitems,
				    const RandoFillState *locations, const RandoItems *base)
{
	RandoItems remaining, assumed;
	uint8_t fillable[RANDO_LOCATION_COUNT];
	int i, j, nempty = 0;

	rando_items_clear(&remaining);
	for (i = 0; i < nitems; i++)
		rando_items_add(&remaining, (RandoItemId)items[i]);
	for (j = 0; j < locations->count; j++)
		nempty += is_empty(world, locations->order[j]);
	if (rando_items_count(&remaining) > nempty)
		return false;

	for (i = 0; i < nitems; i++) {
		RandoItemId item = (RandoItemId)items[i];
		RandoItemClass item_class = rando_item_info(item)->item_class;
		int nfillable = 0;
		bool check_access;

		rando_items_remove(&remaining, item);
		if (base)
			rando_items_merge(&assumed, &remaining, base);
		else
			assumed = remaining;
		rando_items_merge(&assumed, &assumed, &world->pre_collected);
		rando_world_collect_until_stable(world, &assumed);

		check_access = !can_skip_access_checks(world, item, &assumed);
		for (j = 0; j < locations->count; j++)
			if (is_empty(world, locations->order[j])
			    && rando_world_can_fill(world, (RandoLocationId)locations->order[j], item, &assumed,
						    check_access))
				fillable[nfillable++] = locations->order[j];
		if (nfillable == 0)
			return false;

		if (item_class == RANDO_ITEM_CLASS_COMPASS || item_class == RANDO_ITEM_CLASS_MAP)
			j = (int)rando_random_int(prng, 0, nfillable - 1);
		else
			j = 0;
		place(world, fillable[j], item);
	}
	return true;
}

/* World::getGanonsTowerJunkFillRange. */
static void junk_fill_range(const RandoConfig *c, int *lower, int *upper)
{
	assert(c->crystals_tower >= 0 && "junk fill: crystals.tower must be resolved before the fill");
	if (c->logic == RANDO_LOGIC_NO_LOGIC
	    || (c->mode_state != RANDO_STATE_INVERTED
		&& (c->logic == RANDO_LOGIC_OVERWORLD_GLITCHES || c->logic == RANDO_LOGIC_HYBRID_MAJOR_GLITCHES
		    || c->logic == RANDO_LOGIC_MAJOR_GLITCHES))) {
		*lower = 0;
		*upper = 0;
	} else if (c->goal == RANDO_GOAL_TRIFORCE_HUNT || c->goal == RANDO_GOAL_PEDESTAL) {
		*lower = 15 * c->crystals_tower / 7;
		*upper = 25 * c->crystals_tower / 7;
	} else {
		*lower = 0;
		*upper = 15 * c->crystals_tower / 7;
	}
}

bool rando_fill_dungeon(RandoWorld *world, RandoPrng *prng, const RandoPools *pools, RandoFillState *state)
{
	RandoItems base;
	int i, n = 0;

	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		uint8_t location = (uint8_t)rando_layout_location(world->variant, i);

		if (is_empty(world, location))
			state->order[n++] = location;
	}
	state->count = (uint16_t)n;
	shuffle_locations(prng, state);

	rando_items_clear(&base);
	for (i = 0; i < pools->count[RANDO_POOL_ADVANCEMENT]; i++)
		rando_items_add(&base, (RandoItemId)pools->items[RANDO_POOL_ADVANCEMENT][i]);
	for (i = 0; i < pools->count[RANDO_POOL_NICE]; i++)
		rando_items_add(&base, (RandoItemId)pools->items[RANDO_POOL_NICE][i]);
	return fill_items_in_locations(world, prng, pools->items[RANDO_POOL_DUNGEON], pools->count[RANDO_POOL_DUNGEON],
				       state, &base);
}

static void shuffle_list(RandoPrng *prng, RandoPools *pools, RandoPoolList list)
{
	rando_fy_shuffle(prng, pools->items[list], pools->count[list], sizeof pools->items[list][0]);
}

static void junk_fill(RandoWorld *world, RandoPrng *prng, RandoPools *pools)
{
	uint8_t empty[RANDO_LOCATION_COUNT], chosen[RANDO_LOCATION_COUNT];
	uint8_t *extra = pools->items[RANDO_POOL_ITEMS];
	int lower, upper, number, nempty = 0, nchosen, ntrash, i;

	junk_fill_range(&world->config, &lower, &upper);
	number = (int)rando_random_int(prng, lower, upper);
	for (i = 0; i < rando_layout_region_location_count(world->variant, RANDO_REGION_GANONS_TOWER); i++) {
		uint8_t location = (uint8_t)rando_layout_region_location(world->variant, RANDO_REGION_GANONS_TOWER, i);

		if (is_empty(world, location))
			empty[nempty++] = location;
	}
	nchosen = rando_random_collection(prng, empty, nempty, number, chosen);

	shuffle_list(prng, pools, RANDO_POOL_ITEMS);
	/* array_splice($extra, 0, n): the trash is the front of the shuffled pool, popped from its end. */
	ntrash = nchosen < pools->count[RANDO_POOL_ITEMS] ? nchosen : pools->count[RANDO_POOL_ITEMS];
	fast_fill(world, extra, ntrash, chosen, nchosen);
	memmove(extra, extra + ntrash, (size_t)(pools->count[RANDO_POOL_ITEMS] - ntrash));
	pools->count[RANDO_POOL_ITEMS] = (uint16_t)(pools->count[RANDO_POOL_ITEMS] - ntrash);
}

bool rando_fill_rest(RandoWorld *world, RandoPrng *prng, RandoPools *pools, RandoFillState *state)
{
	int i, n;

	junk_fill(world, prng, pools);

	n = keep_empty(world, state);
	for (i = 0; i < n / 2; i++) {
		uint8_t t = state->order[i];

		state->order[i] = state->order[n - 1 - i];
		state->order[n - 1 - i] = t;
	}
	shuffle_list(prng, pools, RANDO_POOL_ADVANCEMENT);
	if (!fill_items_in_locations(world, prng, pools->items[RANDO_POOL_ADVANCEMENT],
				     pools->count[RANDO_POOL_ADVANCEMENT], state, NULL))
		return false;

	keep_empty(world, state);
	shuffle_locations(prng, state);
	shuffle_list(prng, pools, RANDO_POOL_NICE);
	fast_fill(world, pools->items[RANDO_POOL_NICE], pools->count[RANDO_POOL_NICE], state->order, state->count);

	shuffle_list(prng, pools, RANDO_POOL_ITEMS);
	fast_fill(world, pools->items[RANDO_POOL_ITEMS], pools->count[RANDO_POOL_ITEMS], state->order, state->count);
	return true;
}
