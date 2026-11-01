#include "rando_postfill.h"

#include "postfill_draw.h"

#include <assert.h>
#include <string.h>

void rando_postfill_clear(RandoPostFill *post)
{
	memset(post, 0, sizeof *post);
	/* ALTERNATE, the zero value, would need two bow locations. */
	post->texts.bow_hint = RANDO_BOW_HINT_FAKE;
	post->texts.bow_locations[0] = RANDO_LOCATION_NONE;
	post->texts.bow_locations[1] = RANDO_LOCATION_NONE;
	post->texts.silvers_location = RANDO_LOCATION_NONE;
	post->texts.alternate_bow = RANDO_LOCATION_NONE;
}

uint8_t rando_pick_first(RandoPrng *prng, int count)
{
	uint8_t idx[256];
	int i;

	assert(count > 0 && count <= 256);
	for (i = 0; i < count; i++)
		idx[i] = (uint8_t)i;
	rando_fy_shuffle(prng, idx, (size_t)count, 1);
	return idx[0];
}

int rando_locations_with(const RandoWorld *world, RandoItemId item, uint8_t out[RANDO_LOCATION_COUNT])
{
	int i, n = 0;

	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);

		if (rando_world_has_item(world, location, item))
			out[n++] = (uint8_t)location;
	}
	return n;
}

RandoLocationId rando_first_with(const RandoWorld *world, RandoItemId item)
{
	uint8_t ids[RANDO_LOCATION_COUNT];

	return rando_locations_with(world, item, ids) > 0 ? (RandoLocationId)ids[0] : RANDO_LOCATION_NONE;
}

void rando_set_texts(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post)
{
	RandoTexts *t = &post->texts;
	uint8_t bows[RANDO_LOCATION_COUNT], picked[2];
	RandoLocationId silvers;
	int nbows, npicked;

	t->uncle = rando_pick_first(prng, RANDO_STRINGS_UNCLE);
	t->blind = rando_pick_first(prng, RANDO_STRINGS_BLIND);
	t->tavern_man = rando_pick_first(prng, RANDO_STRINGS_TAVERN_MAN);
	t->ganon_1 = rando_pick_first(prng, RANDO_STRINGS_GANON_1);

	silvers = rando_first_with(world, RANDO_ITEM_SilverArrowUpgrade);
	if (silvers == RANDO_LOCATION_NONE)
		silvers = rando_first_with(world, RANDO_ITEM_BowAndSilverArrows);
	/* Drawn before the overflow-count branch, whatever it decides. */
	nbows = rando_locations_with(world, RANDO_ITEM_ProgressiveBow, bows);
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
		t->fake_silvers = rando_pick_first(prng, RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS);
	}

	t->triforce = rando_pick_first(prng, RANDO_STRINGS_TRIFORCE);
}

void rando_randomize_credits(RandoPrng *prng, RandoPostFill *post)
{
	RandoCredits *c = &post->credits;

	c->castle = rando_pick_first(prng, RANDO_CREDITS_CASTLE);
	c->sanctuary = rando_pick_first(prng, RANDO_CREDITS_SANCTUARY);
	c->kakariko = rando_pick_first(prng, RANDO_CREDITS_KAKARIKO);
	c->lumberjacks = rando_pick_first(prng, RANDO_CREDITS_LUMBERJACKS);
	c->smithy_breadsmiths = rando_random_int(prng, 0, 1) == 1;
	c->bridge = rando_pick_first(prng, RANDO_CREDITS_BRIDGE);
	c->woods = rando_pick_first(prng, RANDO_CREDITS_WOODS);
	c->well = rando_pick_first(prng, RANDO_CREDITS_WELL);
}

static bool has_music(RandoRegionId region)
{
	switch (region) {
	case RANDO_REGION_EASTERN_PALACE:
	case RANDO_REGION_DESERT_PALACE:
	case RANDO_REGION_TOWER_OF_HERA:
	case RANDO_REGION_PALACE_OF_DARKNESS:
	case RANDO_REGION_SWAMP_PALACE:
	case RANDO_REGION_SKULL_WOODS:
	case RANDO_REGION_THIEVES_TOWN:
	case RANDO_REGION_ICE_PALACE:
	case RANDO_REGION_MISERY_MIRE:
	case RANDO_REGION_TURTLE_ROCK:
	case RANDO_REGION_GANONS_TOWER:
		return true;
	default:
		return false;
	}
}

static bool is_prize(RandoLocationId location)
{
	switch (rando_location_info(location)->location_class) {
	case RANDO_LOCATION_CLASS_PRIZE:
	case RANDO_LOCATION_CLASS_PRIZE_CRYSTAL:
	case RANDO_LOCATION_CLASS_PRIZE_EVENT:
	case RANDO_LOCATION_CLASS_PRIZE_PENDANT:
		return true;
	default:
		return false;
	}
}

void rando_prize_music(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post)
{
	RandoPrizeMusic *m = &post->prize_music;
	int r, i;

	m->count = 0;
	if (!world->config.rom_map_on_pickup)
		return;
	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		if (!has_music((RandoRegionId)r))
			continue;
		for (i = 0; i < rando_layout_region_location_count(world->variant, (RandoRegionId)r); i++) {
			RandoLocationId location = rando_layout_region_location(world->variant, (RandoRegionId)r, i);
			uint8_t music[2] = {0x11, 0x16};

			if (!is_prize(location))
				continue;
			assert(m->count < RANDO_PRIZE_MUSIC_MAX);
			rando_fy_shuffle(prng, music, 2, 1);
			m->locations[m->count] = location;
			m->music[m->count] = music[0];
			m->count++;
		}
	}
}

void rando_digging_game(RandoPrng *prng, RandoPostFill *post)
{
	post->digging_game = (uint8_t)rando_random_int(prng, 1, 30);
}

void rando_rng_block(RandoPrng *prng, RandoPostFill *post)
{
	int i;

	for (i = 0; i < RANDO_RNG_BLOCK_SIZE; i++)
		post->rng_block[i] = (uint8_t)(rando_random_int(prng, 0, 0x100) & 0xFF);
}

typedef struct DropCount {
	RandoDrop drop;
	uint8_t count;
} DropCount;

/* config/item.php's drop table, BeeGood and Bee at 0 left out. */
static const DropCount drop_pool[] = {
	{RANDO_DROP_HEART, 13},		 {RANDO_DROP_RUPEE_GREEN, 9},	     {RANDO_DROP_RUPEE_BLUE, 7},
	{RANDO_DROP_RUPEE_RED, 6},	 {RANDO_DROP_BOMB_REFILL_1, 7},	     {RANDO_DROP_BOMB_REFILL_4, 1},
	{RANDO_DROP_BOMB_REFILL_8, 2},	 {RANDO_DROP_MAGIC_REFILL_SMALL, 6}, {RANDO_DROP_MAGIC_REFILL_FULL, 3},
	{RANDO_DROP_ARROW_REFILL_5, 5},	 {RANDO_DROP_ARROW_REFILL_10, 3},    {RANDO_DROP_FAIRY, 1},
};

#define DROP_POOL_SIZE 63

void rando_prize_packs_to_rom(RandoWorld *world, RandoPrng *prng)
{
	uint8_t pool[DROP_POOL_SIZE];
	size_t k;
	int i, pack, slot, n = 0;

	for (k = 0; k < sizeof drop_pool / sizeof drop_pool[0]; k++)
		for (i = 0; i < drop_pool[k].count; i++) {
			assert(n < DROP_POOL_SIZE);
			pool[n++] = (uint8_t)drop_pool[k].drop;
		}
	assert(n == DROP_POOL_SIZE);
	rando_fy_shuffle(prng, pool, (size_t)n, 1);
	i = 0;
	for (pack = 0; pack < RANDO_PRIZE_PACK_COUNT; pack++)
		for (slot = 0; slot < rando_prize_pack_slots((RandoPrizePack)pack); slot++)
			if (rando_world_drop(world, (RandoPrizePack)pack, slot) == RANDO_DROP_NONE) {
				assert(i < n && "more empty drop slots than drops: VT reads past its pool");
				rando_world_set_drop(world, (RandoPrizePack)pack, slot, (RandoDrop)pool[i++]);
			}
}
