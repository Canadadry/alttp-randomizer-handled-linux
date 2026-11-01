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

#define DRAWS_MAX 512
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

/* Fixture names no C table holds, each with its reason. */
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
	printf(TEST ": hand-written cases passed\n");
	if (fixtures)
		check_hint_tables_fixture();
	return 0;
}
