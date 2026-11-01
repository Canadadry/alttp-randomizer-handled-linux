#include "rando_postfill.h"

#include <assert.h>
#include <string.h>

void rando_postfill_clear(RandoPostFill *post)
{
	memset(post, 0, sizeof *post);
	post->texts.bow_locations[0] = RANDO_LOCATION_NONE;
	post->texts.bow_locations[1] = RANDO_LOCATION_NONE;
	post->texts.silvers_location = RANDO_LOCATION_NONE;
	post->texts.alternate_bow = RANDO_LOCATION_NONE;
}

/* Arr::first(fy_shuffle(alternatives)): count draws. */
static uint8_t pick_text(RandoPrng *prng, int count)
{
	uint8_t idx[256];
	int i;

	assert(count > 0 && count <= 256);
	for (i = 0; i < count; i++)
		idx[i] = (uint8_t)i;
	rando_fy_shuffle(prng, idx, (size_t)count, 1);
	return idx[0];
}

/* World::getLocationsWithItem, in world location order; returns the count written to out. */
static int locations_with(const RandoWorld *world, RandoItemId item, uint8_t out[RANDO_LOCATION_COUNT])
{
	int i, n = 0;

	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);

		if (rando_world_has_item(world, location, item))
			out[n++] = (uint8_t)location;
	}
	return n;
}

static RandoLocationId first_with(const RandoWorld *world, RandoItemId item)
{
	uint8_t ids[RANDO_LOCATION_COUNT];

	return locations_with(world, item, ids) > 0 ? (RandoLocationId)ids[0] : RANDO_LOCATION_NONE;
}

void rando_set_texts(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post)
{
	RandoTexts *t = &post->texts;
	uint8_t bows[RANDO_LOCATION_COUNT], picked[2];
	RandoLocationId silvers;
	int nbows, npicked;

	t->uncle = pick_text(prng, RANDO_STRINGS_UNCLE);
	t->blind = pick_text(prng, RANDO_STRINGS_BLIND);
	t->tavern_man = pick_text(prng, RANDO_STRINGS_TAVERN_MAN);
	t->ganon_1 = pick_text(prng, RANDO_STRINGS_GANON_1);

	silvers = first_with(world, RANDO_ITEM_SilverArrowUpgrade);
	if (silvers == RANDO_LOCATION_NONE)
		silvers = first_with(world, RANDO_ITEM_BowAndSilverArrows);
	/* Drawn before the overflow-count branch, whatever it decides. */
	nbows = locations_with(world, RANDO_ITEM_ProgressiveBow, bows);
	npicked = rando_random_collection(prng, bows, nbows, 2, picked);

	t->bow_locations[0] = RANDO_LOCATION_NONE;
	t->bow_locations[1] = RANDO_LOCATION_NONE;
	t->silvers_location = RANDO_LOCATION_NONE;
	t->alternate_bow = RANDO_LOCATION_NONE;
	t->fake_silvers = 0;
	if (npicked >= 2 && world->config.item_overflow_count_bow >= 2) {
		t->bow_hint = RANDO_BOW_HINT_ALTERNATE;
		t->bow_locations[0] = (RandoLocationId)picked[1];
		t->bow_locations[1] = (RandoLocationId)picked[0];
		t->alternate_bow = t->bow_locations[0];
	} else if (silvers != RANDO_LOCATION_NONE) {
		t->bow_hint = RANDO_BOW_HINT_SILVERS;
		t->silvers_location = silvers;
	} else {
		t->bow_hint = RANDO_BOW_HINT_FAKE;
		t->fake_silvers = pick_text(prng, RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS);
	}

	t->triforce = pick_text(prng, RANDO_STRINGS_TRIFORCE);
}
