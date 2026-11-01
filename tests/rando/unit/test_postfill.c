#include "rando_postfill.h"
#include "rando_prng.h"
#include "rando_world.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_postfill"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define DRAWS_MAX 512

typedef struct Draws {
	int n;
	int64_t max[DRAWS_MAX];
	int64_t r[DRAWS_MAX];
} Draws;

static void record_draw(void *ctx, int64_t min, int64_t max, int64_t r)
{
	Draws *d = ctx;

	(void)min;
	if (d->n >= DRAWS_MAX)
		test_fail(TEST, "more than %d draws", DRAWS_MAX);
	d->max[d->n] = max;
	d->r[d->n] = r;
	d->n++;
}

static void seeded(RandoPrng *prng, uint32_t seed, Draws *draws)
{
	memset(prng, 0, sizeof *prng);
	rando_prng_seed(prng, rando_splitmix32(seed));
	if (draws) {
		memset(draws, 0, sizeof *draws);
		prng->observe = record_draw;
		prng->observe_ctx = draws;
	}
}

static void pool_world(RandoWorld *world, RandoItemPool pool)
{
	RandoConfig config;

	rando_config_init(&config);
	config.item_pool = pool;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
}

static RandoLocationId nth(int i)
{
	return rando_layout_location(RANDO_VARIANT_OPEN, i);
}

/* Element 0 of VT's fy_shuffle over count indices, from prng's state. */
static int first_of_shuffle(RandoPrng *prng, int count)
{
	uint8_t idx[256];
	int i;

	for (i = 0; i < count; i++)
		idx[i] = (uint8_t)i;
	rando_fy_shuffle(prng, idx, (size_t)count, 1);
	return idx[0];
}

/* Two progressive bows under the default overflow count: the alternate bow goes to the bow drawn
 * second, the popped-first one, after 55 + 77 + 51 + 31 + 2 + 39 draws. */
static void check_texts_alternate(void)
{
	RandoWorld world;
	RandoPrng prng, replay;
	RandoPostFill post;
	Draws draws;
	RandoLocationId a = nth(10), b = nth(20);

	pool_world(&world, RANDO_ITEM_POOL_NORMAL);
	CHECK(rando_world_set_item(&world, a, RANDO_ITEM_ProgressiveBow));
	CHECK(rando_world_set_item(&world, b, RANDO_ITEM_ProgressiveBow));
	CHECK(rando_world_set_item(&world, nth(30), RANDO_ITEM_SilverArrowUpgrade));
	rando_postfill_clear(&post);
	seeded(&prng, 7, &draws);
	rando_set_texts(&world, &prng, &post);
	CHECK(draws.n == 255);
	CHECK(draws.max[214] == 1 && draws.max[215] == 0);
	CHECK(post.texts.bow_hint == RANDO_BOW_HINT_ALTERNATE);
	CHECK(post.texts.bow_locations[0] == (draws.r[214] == 0 ? b : a));
	CHECK(post.texts.bow_locations[1] == (draws.r[214] == 0 ? a : b));
	CHECK(post.texts.alternate_bow == post.texts.bow_locations[0]);
	CHECK(post.texts.silvers_location == RANDO_LOCATION_NONE);
	CHECK(rando_world_item(&world, post.texts.alternate_bow) == RANDO_ITEM_ProgressiveBow);

	seeded(&replay, 7, NULL);
	CHECK(post.texts.uncle == first_of_shuffle(&replay, RANDO_STRINGS_UNCLE));
	CHECK(post.texts.blind == first_of_shuffle(&replay, RANDO_STRINGS_BLIND));
	CHECK(post.texts.tavern_man == first_of_shuffle(&replay, RANDO_STRINGS_TAVERN_MAN));
	CHECK(post.texts.ganon_1 == first_of_shuffle(&replay, RANDO_STRINGS_GANON_1));
	rando_random_int(&replay, 0, 1);
	rando_random_int(&replay, 0, 0);
	CHECK(post.texts.triforce == first_of_shuffle(&replay, RANDO_STRINGS_TRIFORCE));
}

/* Overflow count 1 (the hard pool) and no silvers: the fake hint, 36 more draws before the triforce's. */
static void check_texts_fake(void)
{
	RandoWorld world;
	RandoPrng prng, replay;
	RandoPostFill post;
	Draws draws;
	int i;

	pool_world(&world, RANDO_ITEM_POOL_HARD);
	CHECK(rando_world_set_item(&world, nth(10), RANDO_ITEM_ProgressiveBow));
	CHECK(rando_world_set_item(&world, nth(20), RANDO_ITEM_ProgressiveBow));
	rando_postfill_clear(&post);
	seeded(&prng, 3, &draws);
	rando_set_texts(&world, &prng, &post);
	CHECK(draws.n == 291);
	CHECK(post.texts.bow_hint == RANDO_BOW_HINT_FAKE);
	CHECK(post.texts.alternate_bow == RANDO_LOCATION_NONE);

	seeded(&replay, 3, NULL);
	for (i = 0; i < 216; i++)
		rando_prng_next(&replay);
	CHECK(post.texts.fake_silvers == first_of_shuffle(&replay, RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS));
	CHECK(post.texts.triforce == first_of_shuffle(&replay, RANDO_STRINGS_TRIFORCE));
}

/* Silvers win over the fake hint when the bow branch fails: the first location holding them, SilverArrowUpgrade
 * before BowAndSilverArrows; one progressive bow makes one collection draw. */
static void check_texts_silvers(void)
{
	RandoWorld world;
	RandoPrng prng;
	RandoPostFill post;
	Draws draws;

	pool_world(&world, RANDO_ITEM_POOL_NORMAL);
	CHECK(rando_world_set_item(&world, nth(10), RANDO_ITEM_ProgressiveBow));
	CHECK(rando_world_set_item(&world, nth(5), RANDO_ITEM_BowAndSilverArrows));
	CHECK(rando_world_set_item(&world, nth(40), RANDO_ITEM_SilverArrowUpgrade));
	CHECK(rando_world_set_item(&world, nth(50), RANDO_ITEM_SilverArrowUpgrade));
	rando_postfill_clear(&post);
	seeded(&prng, 1, &draws);
	rando_set_texts(&world, &prng, &post);
	CHECK(draws.n == 254);
	CHECK(post.texts.bow_hint == RANDO_BOW_HINT_SILVERS);
	CHECK(post.texts.silvers_location == nth(40));
	CHECK(post.texts.alternate_bow == RANDO_LOCATION_NONE);

	pool_world(&world, RANDO_ITEM_POOL_EXPERT);
	CHECK(rando_world_set_item(&world, nth(5), RANDO_ITEM_BowAndSilverArrows));
	CHECK(rando_world_set_item(&world, nth(45), RANDO_ITEM_BowAndSilverArrows));
	seeded(&prng, 1, &draws);
	rando_set_texts(&world, &prng, &post);
	CHECK(draws.n == 253);
	CHECK(post.texts.bow_hint == RANDO_BOW_HINT_SILVERS);
	CHECK(post.texts.silvers_location == nth(5));
}

/* Seven Arr::first(fy_shuffle(...)) picks and the smithy coin, between lumberjacks and bridge: 78 draws. */
static void check_credits(void)
{
	RandoPrng prng, replay;
	RandoPostFill post;
	Draws draws;
	uint32_t seed;

	for (seed = 1; seed <= 8; seed++) {
		rando_postfill_clear(&post);
		seeded(&prng, seed, &draws);
		rando_randomize_credits(&prng, &post);
		CHECK(draws.n == 78);
		CHECK(draws.max[62] == 0 && draws.max[63] == 1);

		seeded(&replay, seed, NULL);
		CHECK(post.credits.castle == first_of_shuffle(&replay, RANDO_CREDITS_CASTLE));
		CHECK(post.credits.sanctuary == first_of_shuffle(&replay, RANDO_CREDITS_SANCTUARY));
		CHECK(post.credits.kakariko == first_of_shuffle(&replay, RANDO_CREDITS_KAKARIKO));
		CHECK(post.credits.lumberjacks == first_of_shuffle(&replay, RANDO_CREDITS_LUMBERJACKS));
		CHECK(post.credits.smithy_breadsmiths == (rando_random_int(&replay, 0, 1) == 1));
		CHECK(post.credits.bridge == first_of_shuffle(&replay, RANDO_CREDITS_BRIDGE));
		CHECK(post.credits.woods == first_of_shuffle(&replay, RANDO_CREDITS_WOODS));
		CHECK(post.credits.well == first_of_shuffle(&replay, RANDO_CREDITS_WELL));
	}
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	check_texts_alternate();
	check_texts_fake();
	check_texts_silvers();
	check_credits();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
