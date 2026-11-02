#include "rando_options.h"
#include "rando_pool.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_pool"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

/* Baseline attempt 0: the oracle wrapper seeds splitmix32(seed + attempt) with seed 1. */
static void baseline_prng(RandoPrng *prng)
{
	memset(prng, 0, sizeof *prng);
	rando_prng_seed(prng, rando_splitmix32(1));
}

static void world_from_options(RandoWorld *world, const RandoOptions *options)
{
	RandoConfig config;

	rando_config_from_options(options, 7, 7, &config);
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
}

static void baseline_world(RandoWorld *world)
{
	RandoOptions options;

	rando_options_init(&options);
	world_from_options(world, &options);
}

/* tests/rando/cases/baseline/trace.txt draws 3, 4, 6, 1 in getAdvancementItems. */
static void check_baseline_bottles(void)
{
	static RandoWorld world;
	RandoPools pools;
	RandoPrng prng;

	baseline_world(&world);
	baseline_prng(&prng);
	rando_pool_build(&world, &prng, &pools);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][14] == RANDO_ITEM_BottleWithBluePotion);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][15] == RANDO_ITEM_BottleWithBee);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][16] == RANDO_ITEM_BottleWithFairy);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][17] == RANDO_ITEM_BottleWithRedPotion);
}

/* config/item.php's default counts, as VT's dump of baseline lists them. */
static void check_baseline_lengths(void)
{
	static RandoWorld world;
	RandoPools pools;
	RandoPrng prng;

	baseline_world(&world);
	baseline_prng(&prng);
	rando_pool_build(&world, &prng, &pools);
	CHECK(pools.count[RANDO_POOL_DUNGEON] == 63);
	CHECK(pools.count[RANDO_POOL_ADVANCEMENT] == 48);
	CHECK(pools.count[RANDO_POOL_NICE] == 11);
	CHECK(pools.count[RANDO_POOL_ITEMS] == 94);
	CHECK(pools.items[RANDO_POOL_DUNGEON][0] == RANDO_ITEM_BigKeyA2);
	CHECK(pools.items[RANDO_POOL_DUNGEON][62] == RANDO_ITEM_CompassP3);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][47] == RANDO_ITEM_HalfMagic);
	CHECK(pools.items[RANDO_POOL_NICE][0] == RANDO_ITEM_HeartContainer);
	CHECK(pools.items[RANDO_POOL_ITEMS][93] == RANDO_ITEM_FiftyRupees);
}

typedef struct LastDraw {
	int64_t min, max, r;
	int n;
} LastDraw;

static void observe(void *ctx, int64_t min, int64_t max, int64_t r)
{
	LastDraw *d = ctx;

	d->min = min;
	d->max = max;
	d->r = r;
	d->n++;
}

static void functionality_world(RandoWorld *world, const char *functionality)
{
	RandoOptions options;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "item_functionality", functionality));
	world_from_options(world, &options);
}

static const RandoItemId vt_bottles[] = {
	RANDO_ITEM_Bottle,
	RANDO_ITEM_BottleWithRedPotion,
	RANDO_ITEM_BottleWithGreenPotion,
	RANDO_ITEM_BottleWithBluePotion,
	RANDO_ITEM_BottleWithBee,
	RANDO_ITEM_BottleWithGoldBee,
	RANDO_ITEM_BottleWithFairy,
};

/* World::getBottle: get_random_int(filled ? 1 : 0, 7 - (rom.CatchableFairies ? 1 : 2)), one draw. */
static void check_bottle_range(const char *functionality, bool filled, int64_t min, int64_t max)
{
	static RandoWorld world;
	LastDraw draw = {0, 0, 0, 0};
	RandoPrng prng;
	RandoItemId bottle;

	functionality_world(&world, functionality);
	baseline_prng(&prng);
	prng.observe = observe;
	prng.observe_ctx = &draw;
	bottle = rando_get_bottle(&world, &prng, filled);
	if (draw.n != 1 || draw.min != min || draw.max != max)
		test_fail(TEST, "getBottle(%d) under %s: %d draws in %lld..%lld, expected one in %lld..%lld", filled,
			  functionality, draw.n, (long long)draw.min, (long long)draw.max, (long long)min,
			  (long long)max);
	CHECK(bottle == vt_bottles[draw.r]);
}

static void check_bottle_ranges(void)
{
	check_bottle_range("normal", false, 0, 6);
	check_bottle_range("normal", true, 1, 6);
	check_bottle_range("hard", false, 0, 5);
	check_bottle_range("expert", false, 0, 5);
	check_bottle_range("expert", true, 1, 5);
}

static int occurrences(const RandoPools *pools, RandoPoolList list, RandoItemId item)
{
	int i, n = 0;

	for (i = 0; i < pools->count[list]; i++)
		n += pools->items[list][i] == item;
	return n;
}

/* Baseline's factory config plus one item.count override, through world init. */
static void build_with_count(RandoPools *pools, RandoItemId item, int count)
{
	static RandoWorld world;
	RandoOptions options;
	RandoConfig config;
	RandoPrng prng;

	rando_options_init(&options);
	rando_config_from_options(&options, 7, 7, &config);
	config.item_count[item] = (int16_t)count;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	baseline_prng(&prng);
	rando_pool_build(&world, &prng, pools);
}

/* min(config('item.count.<name>', default), 216): config() falls back only on null. */
static void check_count_overrides(void)
{
	RandoPools pools;

	build_with_count(&pools, RANDO_ITEM_Lamp, 0);
	CHECK(occurrences(&pools, RANDO_POOL_ADVANCEMENT, RANDO_ITEM_Lamp) == 0);
	CHECK(pools.count[RANDO_POOL_ADVANCEMENT] == 47);

	build_with_count(&pools, RANDO_ITEM_Lamp, RANDO_CONFIG_NULL);
	CHECK(occurrences(&pools, RANDO_POOL_ADVANCEMENT, RANDO_ITEM_Lamp) == 1);

	build_with_count(&pools, RANDO_ITEM_Bottle, 2);
	CHECK(pools.count[RANDO_POOL_ADVANCEMENT] == 50);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][17] == RANDO_ITEM_BottleWithRedPotion);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][18] == RANDO_ITEM_Bottle);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][19] == RANDO_ITEM_Bottle);
	CHECK(pools.items[RANDO_POOL_ADVANCEMENT][20] == RANDO_ITEM_Bombos);

	build_with_count(&pools, RANDO_ITEM_ThreeHundredRupees, 300);
	CHECK(occurrences(&pools, RANDO_POOL_NICE, RANDO_ITEM_ThreeHundredRupees) == 216);
	CHECK(pools.count[RANDO_POOL_NICE] == 6 + 216);
}

/* item_pool-crowd_control's lists in pool_lists.txt: four entries at 0, TwentyRupees2 at 4. */
static void check_crowd_control(void)
{
	static RandoWorld world;
	RandoOptions options;
	RandoPools pools;
	RandoPrng prng;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "item_pool", "crowd_control"));
	world_from_options(&world, &options);
	baseline_prng(&prng);
	rando_pool_build(&world, &prng, &pools);
	CHECK(pools.count[RANDO_POOL_ADVANCEMENT] == 44);
	CHECK(occurrences(&pools, RANDO_POOL_ADVANCEMENT, RANDO_ITEM_Cape) == 0);
	CHECK(pools.count[RANDO_POOL_ITEMS] == 98);
	CHECK(occurrences(&pools, RANDO_POOL_ITEMS, RANDO_ITEM_TwentyRupees2) == 4);
	CHECK(pools.items[RANDO_POOL_ITEMS][86] == RANDO_ITEM_TwentyRupees);
	CHECK(pools.items[RANDO_POOL_ITEMS][87] == RANDO_ITEM_TwentyRupees2);
}

typedef struct DrawLog {
	int64_t min[8], max[8];
	int n;
} DrawLog;

static void log_draw(void *ctx, int64_t min, int64_t max, int64_t r)
{
	DrawLog *log = ctx;

	(void)r;
	if (log->n < 8) {
		log->min[log->n] = min;
		log->max[log->n] = max;
	}
	log->n++;
}

static void list_as_items(const RandoPools *pools, RandoPoolList list, RandoItems *items)
{
	int i;

	rando_items_clear(items);
	for (i = 0; i < pools->count[list]; i++)
		rando_items_add(items, (RandoItemId)pools->items[list][i]);
}

/* fillPrizes' assumed items: the getter's list added to a collection, from the same draws. */
static void check_add_items(const char *functionality, RandoPoolList list, int ndraws, int64_t max)
{
	static RandoWorld world;
	DrawLog log = {{0}, {0}, 0};
	RandoPools pools;
	RandoItems expected, added;
	RandoPrng built, twin;
	int i;

	functionality_world(&world, functionality);
	baseline_prng(&built);
	baseline_prng(&twin);
	rando_pool_build(&world, &built, &pools);
	list_as_items(&pools, list, &expected);
	twin.observe = log_draw;
	twin.observe_ctx = &log;
	rando_items_clear(&added);
	rando_pool_add_items(&world, &twin, list, &added);
	CHECK(memcmp(&added, &expected, sizeof added) == 0);
	CHECK(log.n == ndraws);
	for (i = 0; i < ndraws && i < 8; i++)
		CHECK(log.min[i] == 0 && log.max[i] == max);
	if (list == RANDO_POOL_ADVANCEMENT)
		CHECK(memcmp(built.s, twin.s, sizeof built.s) == 0);
}

static void check_add_items_lists(void)
{
	check_add_items("normal", RANDO_POOL_ADVANCEMENT, 4, 6);
	check_add_items("hard", RANDO_POOL_ADVANCEMENT, 4, 5);
	check_add_items("normal", RANDO_POOL_DUNGEON, 0, 0);
}

int main(void)
{
	check_baseline_bottles();
	check_baseline_lengths();
	check_bottle_ranges();
	check_count_overrides();
	check_crowd_control();
	check_add_items_lists();
	puts(TEST ": ok");
	return 0;
}
