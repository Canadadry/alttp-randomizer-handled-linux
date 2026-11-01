#include "rando_postfill.h"
#include "rando_prng.h"
#include "rando_world.h"
#include "support.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_postfill"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define DRAWS_MAX 1024
#define FIXTURE_HINT_TABLES "oracle/hint_tables.txt"

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

/* The location's hint for item: draws and the picked indices, checked against a replay of seed. */
static void check_location_hint(RandoWorld *world, RandoLocationId location, RandoItemId item, int item_draws,
				int location_draws)
{
	RandoPrng prng, replay;
	RandoLocationHint hint;
	Draws draws;

	CHECK(rando_world_set_item(world, location, item));
	seeded(&prng, 5, &draws);
	CHECK(rando_location_hint(world, location, &prng, &hint));
	if (draws.n != item_draws + location_draws)
		test_fail(TEST, "%s at %s: %d draws, want %d + %d", rando_item_info(item)->name,
			  rando_location_info(location)->name, draws.n, item_draws, location_draws);
	CHECK(hint.location == location);
	CHECK(hint.item == (rando_item_info(item)->item_class == RANDO_ITEM_CLASS_ALIAS
				    ? rando_item_info(item)->alias_target
				    : item));
	seeded(&replay, 5, NULL);
	CHECK(hint.item_name == (item_draws ? first_of_shuffle(&replay, item_draws) : 0));
	CHECK(hint.location_name == (location_draws ? first_of_shuffle(&replay, location_draws) : 0));
}

/* Location::getHint: hint.item then hint.location, each drawn only when an array. */
static void check_location_hints(void)
{
	RandoWorld world;
	RandoPrng prng;
	RandoLocationHint hint;
	Draws draws;

	pool_world(&world, RANDO_ITEM_POOL_NORMAL);
	seeded(&prng, 5, &draws);
	CHECK(!rando_location_hint(&world, RANDO_LOC_SAHASRAHLA, &prng, &hint));
	CHECK(draws.n == 0);

	check_location_hint(&world, RANDO_LOC_SAHASRAHLA, RANDO_ITEM_FireRod, 2, 3);
	check_location_hint(&world, RANDO_LOC_CATFISH, RANDO_ITEM_Arrow, 2, 4);
	check_location_hint(&world, RANDO_LOC_PURPLE_CHEST, RANDO_ITEM_FireRod, 2, 0);
	CHECK(rando_hint_item_alternatives(RANDO_ITEM_ProgressiveBowAlternate) == RANDO_HINT_MISSING);
	check_location_hint(&world, RANDO_LOC_SAHASRAHLA, RANDO_ITEM_ProgressiveBowAlternate, 0, 3);
	/* An alias names its target: ShopArrow has no key, Arrow has two. */
	CHECK(rando_hint_item_alternatives(RANDO_ITEM_ShopArrow) == RANDO_HINT_MISSING);
	check_location_hint(&world, RANDO_LOC_MIMIC_CAVE, RANDO_ITEM_ShopArrow, 2, 2);
	check_location_hint(&world, RANDO_LOC_MIMIC_CAVE, RANDO_ITEM_UncleSword,
			    rando_hint_item_alternatives(RANDO_ITEM_ProgressiveSword), 2);
}

/* Under rom.genericKeys a key (class Key, not the ShopKey alias) is named as KeyGK. Every key's text is a
 * plain string, so only the named item shows it. */
static void check_generic_keys_hint(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoPrng prng;
	RandoLocationHint hint;
	Draws draws;
	int generic;

	rando_config_init(&config);
	for (generic = 0; generic < 2; generic++) {
		config.rom_generic_keys = generic != 0;
		rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
		CHECK(rando_world_set_item(&world, RANDO_LOC_SAHASRAHLA, RANDO_ITEM_KeyH2));
		seeded(&prng, 5, &draws);
		CHECK(rando_location_hint(&world, RANDO_LOC_SAHASRAHLA, &prng, &hint));
		CHECK(draws.n == 3);
		CHECK(hint.item == (generic ? RANDO_ITEM_KeyGK : RANDO_ITEM_KeyH2));
		CHECK(rando_world_set_item(&world, RANDO_LOC_SAHASRAHLA, RANDO_ITEM_ShopKey));
		CHECK(rando_location_hint(&world, RANDO_LOC_SAHASRAHLA, &prng, &hint));
		CHECK(hint.item == RANDO_ITEM_KeyGK);
		CHECK(rando_world_set_item(&world, RANDO_LOC_SAHASRAHLA, RANDO_ITEM_BigKeyH2));
		CHECK(rando_location_hint(&world, RANDO_LOC_SAHASRAHLA, &prng, &hint));
		CHECK(hint.item == RANDO_ITEM_BigKeyH2);
	}
}

/* LocationCollection::getHint: one item draws as Location::getHint; two or more draw nothing, since
 * VT's lookups by getName() (":<world id>" suffix) all miss. */
static void check_collection_hints(void)
{
	static const RandoLocationId pair[] = {RANDO_LOC_SAHASRAHLA, RANDO_LOC_CATFISH};
	RandoWorld world;
	RandoPrng prng, replay;
	RandoLocationHint hint, want;
	Draws draws;

	pool_world(&world, RANDO_ITEM_POOL_NORMAL);
	seeded(&prng, 9, &draws);
	CHECK(rando_collection_hint(&world, pair, 0, &prng, &hint) == RANDO_COLLECTION_HINT_NONE);
	CHECK(rando_collection_hint(&world, pair, 2, &prng, &hint) == RANDO_COLLECTION_HINT_NONE);
	CHECK(draws.n == 0);

	CHECK(rando_world_set_item(&world, RANDO_LOC_CATFISH, RANDO_ITEM_FireRod));
	CHECK(rando_collection_hint(&world, pair, 2, &prng, &hint) == RANDO_COLLECTION_HINT_ONE);
	CHECK(draws.n == 2 + 4);
	seeded(&replay, 9, NULL);
	CHECK(rando_location_hint(&world, RANDO_LOC_CATFISH, &replay, &want));
	CHECK(hint.location == want.location && hint.item == want.item && hint.item_name == want.item_name
	      && hint.location_name == want.location_name);

	CHECK(rando_world_set_item(&world, RANDO_LOC_SAHASRAHLA, RANDO_ITEM_Arrow));
	seeded(&prng, 9, &draws);
	CHECK(rando_collection_hint(&world, pair, 2, &prng, &hint) == RANDO_COLLECTION_HINT_MANY);
	CHECK(draws.n == 0);
	CHECK(hint.location == RANDO_LOC_SAHASRAHLA);
}

/* 15 tiles, 10 picks, the five pick shuffles over 10 to 6, the count draw: drawn whatever the world. */
#define HINT_FIXED_DRAWS (15 + 10 + 40 + 1)

static void hints_world(RandoWorld *world, bool on, bool wild_big_keys)
{
	RandoConfig config;

	rando_config_init(&config);
	config.spoil_hints = on;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
	world->config.region_wild_big_keys = wild_big_keys;
}

static int tiles_of(const RandoPostFill *post, RandoTileHintKind kind, RandoLocationId location)
{
	int i, n = 0;

	for (i = 0; i < RANDO_TILE_COUNT; i++)
		if (post->hints.tiles[i].kind == kind
		    && (kind != RANDO_TILE_HINT_LOCATION || post->hints.tiles[i].location.location == location))
			n++;
	return n;
}

static int apply_hints(const RandoWorld *world, uint32_t seed, const uint8_t *advancement, int n, RandoPostFill *post)
{
	RandoPrng prng;
	Draws draws;

	seeded(&prng, seed, &draws);
	rando_apply_hints(world, &prng, advancement, n, post);
	return draws.n;
}

/* Hints off: no draw, no tile. */
static void check_hints_off(void)
{
	static const uint8_t advancement[] = {RANDO_ITEM_FireRod};
	RandoWorld world;
	RandoPostFill post;

	hints_world(&world, false, false);
	CHECK(rando_world_set_item(&world, RANDO_LOC_LINKS_HOUSE, RANDO_ITEM_FireRod));
	rando_postfill_clear(&post);
	CHECK(apply_hints(&world, 1, advancement, 1, &post) == 0);
	CHECK(!post.hints.on);
	CHECK(tiles_of(&post, RANDO_TILE_HINT_NONE, RANDO_LOCATION_NONE) == RANDO_TILE_COUNT);
}

/* An empty world: only the hintables shuffle (keys, maps, compasses, bottles, swords, shields and the five
 * named items left out; big keys too unless wild), random() over no location drawing nothing, then 15 jokes. */
static void check_hints_empty_world(void)
{
	static const uint8_t advancement[] = {
		RANDO_ITEM_FireRod, RANDO_ITEM_Hammer, RANDO_ITEM_KeyH2, RANDO_ITEM_Bottle,
		RANDO_ITEM_ProgressiveSword, RANDO_ITEM_Mushroom, RANDO_ITEM_ProgressiveShield, RANDO_ITEM_MapH2,
		RANDO_ITEM_CompassH2, RANDO_ITEM_BigKeyH2, RANDO_ITEM_Hookshot, RANDO_ITEM_Powder,
	};
	RandoWorld world;
	RandoPostFill post;

	hints_world(&world, true, false);
	rando_postfill_clear(&post);
	CHECK(apply_hints(&world, 1, advancement, 12, &post) == HINT_FIXED_DRAWS + 3 + 15 * RANDO_STRINGS_HINT);
	CHECK(post.hints.on);
	CHECK(tiles_of(&post, RANDO_TILE_HINT_JOKE, RANDO_LOCATION_NONE) == RANDO_TILE_COUNT);
	hints_world(&world, true, true);
	CHECK(apply_hints(&world, 1, advancement, 12, &post) == HINT_FIXED_DRAWS + 4 + 15 * RANDO_STRINGS_HINT);
}

/* The one item both hinted and drawn by randomCollection: merged once, so one tile names it. */
static void check_hints_merge(void)
{
	static const uint8_t advancement[] = {RANDO_ITEM_FireRod};
	RandoWorld world;
	RandoPostFill post;
	int i;

	hints_world(&world, true, false);
	CHECK(rando_world_set_item(&world, RANDO_LOC_LINKS_HOUSE, RANDO_ITEM_FireRod));
	rando_postfill_clear(&post);
	/* Shuffle 1, random() 1, randomCollection 1, the hint 2 + 0. */
	CHECK(apply_hints(&world, 1, advancement, 1, &post) == HINT_FIXED_DRAWS + 5 + 14 * RANDO_STRINGS_HINT);
	CHECK(tiles_of(&post, RANDO_TILE_HINT_LOCATION, RANDO_LOC_LINKS_HOUSE) == 1);
	CHECK(tiles_of(&post, RANDO_TILE_HINT_JOKE, RANDO_LOCATION_NONE) == 14);
	for (i = 0; i < RANDO_TILE_COUNT; i++)
		if (post.hints.tiles[i].kind == RANDO_TILE_HINT_LOCATION)
			CHECK(post.hints.tiles[i].location.item == RANDO_ITEM_FireRod);
}

/* Boots take a tile first, then the location is drawn again for a second tile. */
static void check_hints_boots(void)
{
	RandoWorld world;
	RandoPostFill post;

	hints_world(&world, true, false);
	CHECK(rando_world_set_item(&world, RANDO_LOC_LINKS_HOUSE, RANDO_ITEM_PegasusBoots));
	rando_postfill_clear(&post);
	CHECK(apply_hints(&world, 1, NULL, 0, &post) == 2 + HINT_FIXED_DRAWS + 1 + 2 + 13 * RANDO_STRINGS_HINT);
	CHECK(tiles_of(&post, RANDO_TILE_HINT_LOCATION, RANDO_LOC_LINKS_HOUSE) == 2);
}

/* A single pick hints when drawn among the five, and keeps its tile otherwise; a pair pick never hints. */
static void check_hints_picks(void)
{
	RandoWorld world;
	RandoPostFill post;
	int picked = 0, missed = 0, draws;
	uint32_t seed;

	hints_world(&world, true, false);
	CHECK(rando_world_set_item(&world, RANDO_LOC_SAHASRAHLA, RANDO_ITEM_FireRod));
	for (seed = 1; seed <= 16; seed++) {
		rando_postfill_clear(&post);
		draws = apply_hints(&world, seed, NULL, 0, &post);
		if (draws == HINT_FIXED_DRAWS + 5 + 1 + 5 + 13 * RANDO_STRINGS_HINT) {
			CHECK(tiles_of(&post, RANDO_TILE_HINT_LOCATION, RANDO_LOC_SAHASRAHLA) == 2);
			picked++;
		} else {
			CHECK(draws == HINT_FIXED_DRAWS + 1 + 5 + 14 * RANDO_STRINGS_HINT);
			CHECK(tiles_of(&post, RANDO_TILE_HINT_LOCATION, RANDO_LOC_SAHASRAHLA) == 1);
			missed++;
		}
	}
	CHECK(picked > 0 && missed > 0);

	hints_world(&world, true, false);
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST, RANDO_ITEM_FireRod));
	for (seed = 1; seed <= 16; seed++) {
		rando_postfill_clear(&post);
		CHECK(apply_hints(&world, seed, NULL, 0, &post) == HINT_FIXED_DRAWS + 1 + 4 + 14 * RANDO_STRINGS_HINT);
	}
}

/* setTexts' swapped bow is no ProgressiveBow to getLocationsWithItem: random() finds no location. */
static void check_hints_alternate_bow(void)
{
	static const uint8_t advancement[] = {RANDO_ITEM_ProgressiveBow};
	RandoWorld world;
	RandoPostFill post;

	hints_world(&world, true, false);
	CHECK(rando_world_set_item(&world, RANDO_LOC_LINKS_HOUSE, RANDO_ITEM_ProgressiveBow));
	rando_postfill_clear(&post);
	CHECK(apply_hints(&world, 1, advancement, 1, &post) == HINT_FIXED_DRAWS + 5 + 14 * RANDO_STRINGS_HINT);
	rando_postfill_clear(&post);
	post.texts.alternate_bow = RANDO_LOC_LINKS_HOUSE;
	CHECK(apply_hints(&world, 1, advancement, 1, &post) == HINT_FIXED_DRAWS + 4 + 14 * RANDO_STRINGS_HINT);
	CHECK(tiles_of(&post, RANDO_TILE_HINT_LOCATION, RANDO_LOC_LINKS_HOUSE) == 1);
}

/* Fixture names no C table holds, each with its reason. */
static const RandoLocationId music_prizes[] = {
	RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_LOC_DESERT_PALACE_PRIZE, RANDO_LOC_TOWER_OF_HERA_PRIZE,
	RANDO_LOC_PALACE_OF_DARKNESS_PRIZE, RANDO_LOC_SWAMP_PALACE_PRIZE, RANDO_LOC_SKULL_WOODS_PRIZE,
	RANDO_LOC_THIEVES_TOWN_PRIZE, RANDO_LOC_ICE_PALACE_PRIZE, RANDO_LOC_MISERY_MIRE_PRIZE,
	RANDO_LOC_TURTLE_ROCK_PRIZE, RANDO_LOC_AGAHNIM_2,
};

/* Without rom.mapOnPickup the music follows the prize: no draw. With it, each prize location of a region
 * with music addresses draws Arr::first(fy_shuffle([0x11, 0x16])), Agahnim 2 included, Agahnim not. */
static void check_prize_music(RandoVariant variant)
{
	RandoConfig config;
	RandoWorld world;
	RandoPrng prng, replay;
	RandoPostFill post;
	Draws draws;
	size_t i;

	rando_config_init(&config);
	rando_world_init(&world, variant, &config);
	rando_postfill_clear(&post);
	seeded(&prng, 5, &draws);
	rando_prize_music(&world, &prng, &post);
	CHECK(draws.n == 0);
	CHECK(post.prize_music.count == 0);

	config.rom_map_on_pickup = true;
	rando_world_init(&world, variant, &config);
	CHECK(rando_world_set_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_Crystal6));
	seeded(&prng, 5, &draws);
	rando_prize_music(&world, &prng, &post);
	CHECK(draws.n == 22);
	CHECK(post.prize_music.count == sizeof music_prizes / sizeof music_prizes[0]);
	seeded(&replay, 5, NULL);
	for (i = 0; i < sizeof music_prizes / sizeof music_prizes[0]; i++) {
		uint8_t music[2] = {0x11, 0x16};

		CHECK(draws.max[2 * i] == 1 && draws.max[2 * i + 1] == 0);
		CHECK(post.prize_music.locations[i] == music_prizes[i]);
		rando_fy_shuffle(&replay, music, 2, 1);
		CHECK(post.prize_music.music[i] == music[0]);
	}
}

static const char *const unknown_items[] = {
	/* Boss names: getHint looks up an item's raw name, never a boss's. */
	"Armos Knights", "Lanmolas", "Moldorm", "Helmasaur King", "Arrghus", "Mothula", "Blind",
	"Kholdstare", "Vitreous", "Trinexx", "Agahnim", "Agahnim 2", "Ganon",
};
/* No VT code reads it at 219fcafd. */
static const char *const unknown_strings[] = {"fortune.txt"};

static bool listed(const char *name, const char *const *names, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (strcmp(name, names[i]) == 0)
			return true;
	return false;
}

static int strings_count(const char *file)
{
	static const struct {
		const char *file;
		int count;
	} strings[] = {
		{"uncle.txt", RANDO_STRINGS_UNCLE},
		{"tavern_man.txt", RANDO_STRINGS_TAVERN_MAN},
		{"blind.txt", RANDO_STRINGS_BLIND},
		{"ganon_1.txt", RANDO_STRINGS_GANON_1},
		{"ganon_phase_3_no_silvers.txt", RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS},
		{"triforce.txt", RANDO_STRINGS_TRIFORCE},
		{"hint.txt", RANDO_STRINGS_HINT},
	};
	size_t i;

	for (i = 0; i < sizeof strings / sizeof strings[0]; i++)
		if (strcmp(file, strings[i].file) == 0)
			return strings[i].count;
	return -1;
}

/* hint_tables.txt: `<kind> <name> <n>`, the name holding spaces. Each line matches the C tables, and
 * every name the C tables hold has its line. */
static void check_hint_tables_fixture(void)
{
	static bool item_seen[RANDO_ITEM_COUNT], location_seen[RANDO_LOCATION_COUNT];
	static char kind[FIXTURE_LINE_MAX];
	Fixture fx;
	int records = 0, i;

	fixture_open(&fx, FIXTURE_HINT_TABLES);
	while (fixture_next(&fx)) {
		char *name, *last, *end;
		long n;

		memcpy(kind, fx.record, sizeof kind);
		name = strchr(kind, ' ');
		last = strrchr(kind, ' ');

		if (name == NULL || last == name)
			fixture_fail(&fx, "expected <kind> <name> <n>");
		*name++ = '\0';
		*last = '\0';
		n = strtol(last + 1, &end, 10);
		if (*end != '\0' || n < 0)
			fixture_fail(&fx, "bad count");
		if (strcmp(kind, "item") == 0) {
			RandoItemId item = rando_item_find(name);

			if (item == RANDO_ITEM_NONE) {
				if (!listed(name, unknown_items, sizeof unknown_items / sizeof unknown_items[0]))
					fixture_fail(&fx, "an item the C world does not know");
			} else if (rando_hint_item_alternatives(item) != n) {
				fixture_fail(&fx, "the C item table disagrees");
			} else {
				item_seen[item] = true;
			}
		} else if (strcmp(kind, "location") == 0) {
			RandoLocationId location = rando_location_find(name);

			if (location == RANDO_LOCATION_NONE)
				fixture_fail(&fx, "a location the C world does not know");
			if (rando_hint_location_alternatives(location) != n)
				fixture_fail(&fx, "the C location table disagrees");
			location_seen[location] = true;
		} else if (strcmp(kind, "strings") == 0) {
			if (strings_count(name) < 0) {
				if (!listed(name, unknown_strings, sizeof unknown_strings / sizeof unknown_strings[0]))
					fixture_fail(&fx, "a strings file the C tables do not know");
			} else if (strings_count(name) != n) {
				fixture_fail(&fx, "the C string count disagrees");
			}
		} else {
			fixture_fail(&fx, "unknown kind");
		}
		records++;
	}
	fixture_close(&fx);
	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (rando_hint_item_alternatives((RandoItemId)i) != RANDO_HINT_MISSING && !item_seen[i])
			test_fail(TEST, "hint.item.%s is in the C table, not in " FIXTURE_HINT_TABLES,
				  rando_item_info((RandoItemId)i)->name);
	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		if (rando_hint_location_alternatives((RandoLocationId)i) != RANDO_HINT_MISSING && !location_seen[i])
			test_fail(TEST, "hint.location.%s is in the C table, not in " FIXTURE_HINT_TABLES,
				  rando_location_info((RandoLocationId)i)->name);
	printf(TEST ": %d hint_tables records matched\n", records);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	check_texts_alternate();
	check_texts_fake();
	check_texts_silvers();
	check_credits();
	check_location_hints();
	check_generic_keys_hint();
	check_collection_hints();
	check_hints_off();
	check_hints_empty_world();
	check_hints_merge();
	check_hints_boots();
	check_hints_picks();
	check_hints_alternate_bow();
	check_prize_music(RANDO_VARIANT_OPEN);
	check_prize_music(RANDO_VARIANT_INVERTED);
	printf(TEST ": hand-written cases passed\n");
	if (fixtures)
		check_hint_tables_fixture();
	return 0;
}
