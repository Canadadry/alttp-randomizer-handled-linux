#include "rando.h"
#include "rando_options.h"
#include "rando_pool.h"
#include "rando_prepare.h"
#include "support.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_prepare"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void open_world(RandoWorld *world, RandoItemPool pool)
{
	RandoConfig config;

	rando_config_init(&config);
	config.item_pool = pool;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
}

static void check_same_inventory(const RandoWorld *a, const RandoWorld *b, RandoShopId shop)
{
	int s;

	for (s = 0; s < RANDO_SHOP_SLOTS; s++) {
		CHECK(rando_world_shop_item(a, shop, s) == rando_world_shop_item(b, shop, s));
		if (rando_world_shop_item(a, shop, s) != RANDO_ITEM_NONE)
			CHECK(rando_world_shop_price(a, shop, s) == rando_world_shop_price(b, shop, s));
	}
}

/* setShops activates every shop that is not a Shop\TakeAny and, in baseline, changes no inventory. */
static void check_shops_baseline(void)
{
	RandoWorld world, fresh;
	int i, active = 0;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	fresh = world;
	rando_set_shops(&world, NULL);
	for (i = 0; i < RANDO_SHOP_COUNT; i++) {
		RandoShopId shop = (RandoShopId)i;

		CHECK(rando_world_shop_active(&world, shop) == (rando_shop_info(shop)->kind != RANDO_SHOP_KIND_TAKE_ANY));
		active += rando_world_shop_active(&world, shop);
		check_same_inventory(&world, &fresh, shop);
	}
	CHECK(active == 10);
}

/* shops.HardMode clears Capacity Upgrade, which stays active: VT lists it item-less (item_pool-hard golden). */
static void check_shops_hard(void)
{
	RandoWorld world, fresh;
	int s;

	open_world(&world, RANDO_ITEM_POOL_HARD);
	fresh = world;
	rando_set_shops(&world, NULL);
	CHECK(rando_world_shop_active(&world, RANDO_SHOP_CAPACITY_UPGRADE));
	for (s = 0; s < RANDO_SHOP_SLOTS; s++)
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, s) == RANDO_ITEM_NONE);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_FOREST_SHOP);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_POTION_SHOP);
}

static void check_slot(const RandoWorld *world, RandoShopId shop, int slot, RandoItemId item, int price)
{
	CHECK(rando_world_shop_item(world, shop, slot) == item);
	CHECK(rando_world_shop_price(world, shop, slot) == price);
}

/* Shield overflow count 1, below 2: Dark World Forest Shop slot 0 at 500 (item_pool-expert golden). */
static void check_shops_expert(void)
{
	RandoWorld world, fresh;
	int s;

	open_world(&world, RANDO_ITEM_POOL_EXPERT);
	fresh = world;
	rando_set_shops(&world, NULL);
	for (s = 0; s < RANDO_SHOP_SLOTS; s++)
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, s) == RANDO_ITEM_NONE);
	check_slot(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, RANDO_ITEM_TwentyRupees2, 500);
	check_slot(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 1, RANDO_ITEM_Bee, 10);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_POTION_SHOP);
	check_same_inventory(&world, &fresh, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP);
}

static const RandoShopId below_one_shops[] = {
	RANDO_SHOP_DARK_WORLD_POTION_SHOP,
	RANDO_SHOP_DARK_WORLD_LUMBERJACK_HUT_SHOP,
	RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP,
	RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP,
};

/* Shield overflow count 0: also slot 1 of four shops at 50; no HardMode (item_pool-crowd_control golden). */
static void check_shops_crowd_control(RandoItemId replacement)
{
	RandoWorld world, fresh;
	RandoConfig config;
	size_t i;

	rando_config_init(&config);
	config.item_pool = RANDO_ITEM_POOL_CROWD_CONTROL;
	config.item_overflow_replacement_shield = replacement;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	fresh = world;
	rando_set_shops(&world, NULL);
	check_same_inventory(&world, &fresh, RANDO_SHOP_CAPACITY_UPGRADE);
	check_slot(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, replacement, 500);
	for (i = 0; i < sizeof below_one_shops / sizeof below_one_shops[0]; i++) {
		check_slot(&world, below_one_shops[i], 0, RANDO_ITEM_RedPotion, 150);
		check_slot(&world, below_one_shops[i], 1, replacement, 50);
	}
}

static void seeded(RandoPrng *prng, uint32_t seed, int skip)
{
	memset(prng, 0, sizeof *prng);
	rando_prng_seed(prng, rando_splitmix32(seed));
	while (skip-- > 0)
		rando_random_int(prng, 0, 0);
}

static void count_draw(void *ctx, int64_t min, int64_t max, int64_t r)
{
	(void)min;
	(void)max;
	(void)r;
	++*(int *)ctx;
}

static void retro_world(RandoWorld *world, RandoItemPool pool, RandoWeapons weapons)
{
	RandoConfig config;

	rando_config_init(&config);
	config.item_pool = pool;
	config.mode_weapons = weapons;
	rando_world_init(world, RANDO_VARIANT_RETRO, &config);
}

static int set_shops_draws(RandoWorld *world, uint32_t seed)
{
	RandoPrng prng;
	int draws = 0;

	seeded(&prng, seed, 0);
	prng.observe = count_draw;
	prng.observe_ctx = &draws;
	rando_set_shops(world, &prng);
	return draws;
}

static int holds(const RandoWorld *world, RandoShopId shop, int slot, RandoItemId item, int price)
{
	return rando_world_shop_item(world, shop, slot) == item && rando_world_shop_price(world, shop, slot) == price;
}

/* Randomizer::setShops' random path: 4 + 1 take-any draws, then 5 shops for the arrow, key and bombs. */
static void check_shops_retro(void)
{
	uint32_t seed;

	for (seed = 0; seed < 8; seed++) {
		RandoWorld world;
		int i, take_anys = 0, potions = 0, swords = 0, sellers = 0;

		retro_world(&world, RANDO_ITEM_POOL_NORMAL, RANDO_WEAPONS_RANDOMIZED);
		CHECK(set_shops_draws(&world, seed) == 10);
		for (i = 0; i < RANDO_SHOP_COUNT; i++) {
			RandoShopId shop = (RandoShopId)i;
			RandoShopKind kind = rando_shop_info(shop)->kind;

			if (kind == RANDO_SHOP_KIND_TAKE_ANY && rando_world_shop_active(&world, shop)) {
				take_anys++;
				if (holds(&world, shop, 0, RANDO_ITEM_BluePotion, 0)
				    && holds(&world, shop, 1, RANDO_ITEM_BossHeartContainer, 0))
					potions++;
				else if (holds(&world, shop, 0, RANDO_ITEM_ProgressiveSword, 0)
					 && rando_world_shop_item(&world, shop, 1) == RANDO_ITEM_NONE)
					swords++;
			}
			if (rando_world_shop_item(&world, shop, 1) == RANDO_ITEM_ShopKey) {
				CHECK(kind == RANDO_SHOP_KIND_SHOP);
				CHECK(holds(&world, shop, 0, RANDO_ITEM_ShopArrow, 80));
				CHECK(holds(&world, shop, 1, RANDO_ITEM_ShopKey, 100));
				CHECK(holds(&world, shop, 2, RANDO_ITEM_TenBombs, 50));
				sellers++;
			}
		}
		CHECK(take_anys == 5 && potions == 4 && swords == 1);
		CHECK(sellers == 5);
		CHECK(holds(&world, RANDO_SHOP_CAPACITY_UPGRADE, 0, RANDO_ITEM_BombUpgrade5, 100));
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, 1) == RANDO_ITEM_NONE);
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, 2) == RANDO_ITEM_NONE);
	}
}

static RandoShopId old_man(const RandoWorld *world)
{
	int i;

	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind == RANDO_SHOP_KIND_TAKE_ANY
		    && rando_world_shop_active(world, (RandoShopId)i)
		    && rando_world_shop_item(world, (RandoShopId)i, 1) == RANDO_ITEM_NONE)
			return (RandoShopId)i;
	return RANDO_SHOP_COUNT;
}

/* swordless and vanilla weapons: the old man gives ThreeHundredRupees, no sword. */
static void check_shops_retro_old_man(RandoWeapons weapons)
{
	RandoWorld world;
	RandoShopId shop;

	retro_world(&world, RANDO_ITEM_POOL_NORMAL, weapons);
	CHECK(set_shops_draws(&world, 3) == 10);
	shop = old_man(&world);
	CHECK(shop != RANDO_SHOP_COUNT);
	CHECK(holds(&world, shop, 0, RANDO_ITEM_ThreeHundredRupees, 0));
}

/* Seed 0 does not pick the Forest Shop: only its TenArrows slot becomes ShopArrow at 80. */
static void check_shops_retro_forest(void)
{
	RandoWorld world;

	retro_world(&world, RANDO_ITEM_POOL_NORMAL, RANDO_WEAPONS_RANDOMIZED);
	set_shops_draws(&world, 0);
	CHECK(rando_world_shop_active(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP));
	CHECK(holds(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 0, RANDO_ITEM_RedShield, 500));
	CHECK(holds(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 1, RANDO_ITEM_Bee, 10));
	CHECK(holds(&world, RANDO_SHOP_DARK_WORLD_FOREST_SHOP, 2, RANDO_ITEM_ShopArrow, 80));
}

/* shops.HardMode: the Capacity Upgrade stays empty under the rupee bow. */
static void check_shops_retro_hard(void)
{
	RandoWorld world;
	int s;

	retro_world(&world, RANDO_ITEM_POOL_HARD, RANDO_WEAPONS_RANDOMIZED);
	CHECK(set_shops_draws(&world, 1) == 10);
	CHECK(rando_world_shop_active(&world, RANDO_SHOP_CAPACITY_UPGRADE));
	for (s = 0; s < RANDO_SHOP_SLOTS; s++)
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_CAPACITY_UPGRADE, s) == RANDO_ITEM_NONE);
}

static void flagged_world(RandoWorld *world, RandoVariant variant, bool generic_keys, bool rupee_bow, bool take_anys)
{
	RandoConfig config;

	rando_config_init(&config);
	config.rom_generic_keys = generic_keys;
	config.rom_rupee_bow = rupee_bow;
	config.region_take_anys = take_anys;
	rando_world_init(world, variant, &config);
}

/* VT skips the Dark World Lake Hylia Shop in a World\Inverted only. */
static void check_shops_retro_inverted(void)
{
	int open_hits = 0;
	uint32_t seed;

	for (seed = 0; seed < 16; seed++) {
		RandoWorld world;

		flagged_world(&world, RANDO_VARIANT_INVERTED, true, true, true);
		CHECK(set_shops_draws(&world, seed) == 10);
		CHECK(rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 0) != RANDO_ITEM_ShopArrow);
		flagged_world(&world, RANDO_VARIANT_OPEN, true, true, true);
		set_shops_draws(&world, seed);
		open_hits += rando_world_shop_item(&world, RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP, 0) == RANDO_ITEM_ShopArrow;
	}
	CHECK(open_hits > 0);
}

/* The rupee bow alone: no take-any draw, the 5 sellers get the arrow and bombs but no key. */
static void check_shops_rupee_bow_alone(void)
{
	RandoWorld world;
	int i, sellers = 0;

	flagged_world(&world, RANDO_VARIANT_OPEN, false, true, false);
	CHECK(set_shops_draws(&world, 2) == 5);
	for (i = 0; i < RANDO_SHOP_COUNT; i++) {
		CHECK(rando_world_shop_item(&world, (RandoShopId)i, 1) != RANDO_ITEM_ShopKey);
		if (rando_shop_info((RandoShopId)i)->kind == RANDO_SHOP_KIND_TAKE_ANY)
			CHECK(!rando_world_shop_active(&world, (RandoShopId)i));
		sellers += rando_world_shop_item(&world, (RandoShopId)i, 0) == RANDO_ITEM_ShopArrow
			&& holds(&world, (RandoShopId)i, 2, RANDO_ITEM_TenBombs, 50);
	}
	CHECK(sellers == 5);
	CHECK(holds(&world, RANDO_SHOP_CAPACITY_UPGRADE, 0, RANDO_ITEM_BombUpgrade5, 100));

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	CHECK(set_shops_draws(&world, 2) == 0);
}

/* cases/crystals_ganon-random seed 1, attempt 0: draws 6 and 7 are 0 then 2; Turtle Rock comes first. */
static void check_medallions_golden(void)
{
	RandoWorld world;
	RandoPrng prng;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	seeded(&prng, 1, 5);
	rando_set_medallions(&world, &prng);
	CHECK(rando_world_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION) == RANDO_ITEM_Ether);
	CHECK(rando_world_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION) == RANDO_ITEM_Quake);
}

/* VT's $medallions, indexed by get_random_int(0, 2). */
static const RandoItemId by_index[] = {RANDO_ITEM_Ether, RANDO_ITEM_Bombos, RANDO_ITEM_Quake};

static void check_medallion_index(void)
{
	int seen[3] = {0, 0, 0};
	uint32_t seed;

	for (seed = 0; seed < 64; seed++) {
		RandoWorld world;
		RandoPrng prng, twin;
		int64_t tr, mm;

		open_world(&world, RANDO_ITEM_POOL_NORMAL);
		seeded(&prng, seed, 0);
		seeded(&twin, seed, 0);
		rando_set_medallions(&world, &prng);
		tr = rando_random_int(&twin, 0, 2);
		mm = rando_random_int(&twin, 0, 2);
		CHECK(rando_world_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION) == by_index[tr]);
		CHECK(rando_world_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION) == by_index[mm]);
		seen[tr]++;
		seen[mm]++;
	}
	CHECK(seen[0] && seen[1] && seen[2]);
}

/* A location that already holds an item is skipped without a draw. */
static void check_medallion_preset(void)
{
	RandoWorld world;
	RandoPrng prng, twin;
	int draws = 0;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	CHECK(rando_world_set_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Bombos));
	seeded(&prng, 7, 0);
	seeded(&twin, 7, 0);
	prng.observe = count_draw;
	prng.observe_ctx = &draws;
	rando_set_medallions(&world, &prng);
	CHECK(draws == 1);
	CHECK(rando_world_item(&world, RANDO_LOC_MISERY_MIRE_MEDALLION) == RANDO_ITEM_Bombos);
	CHECK(rando_world_item(&world, RANDO_LOC_TURTLE_ROCK_MEDALLION) == by_index[rando_random_int(&twin, 0, 2)]);
}

static void functionality_world(RandoWorld *world, const char *functionality)
{
	RandoOptions options;
	RandoConfig config;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "item_functionality", functionality));
	rando_config_from_options(&options, 7, 7, &config);
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
}

/* cases/baseline seed 1, attempt 0: draws 71 and 72 are 1..6 -> 3, 6; cases/item_functionality-hard: 1..5 -> 2, 2. */
static void check_fountains_golden(void)
{
	RandoWorld world;
	RandoPrng prng;

	functionality_world(&world, "normal");
	seeded(&prng, 1, 70);
	rando_set_fountains(&world, &prng);
	CHECK(rando_world_item(&world, RANDO_LOC_WATERFALL_BOTTLE) == RANDO_ITEM_BottleWithBluePotion);
	CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_BOTTLE) == RANDO_ITEM_BottleWithFairy);

	functionality_world(&world, "hard");
	seeded(&prng, 1, 70);
	rando_set_fountains(&world, &prng);
	CHECK(rando_world_item(&world, RANDO_LOC_WATERFALL_BOTTLE) == RANDO_ITEM_BottleWithGreenPotion);
	CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_BOTTLE) == RANDO_ITEM_BottleWithGreenPotion);
}

/* World::getBottle's list, indexed by get_random_int(1, 6), or (1, 5) without rom.CatchableFairies. */
static const RandoItemId vt_bottles[] = {
	RANDO_ITEM_Bottle,
	RANDO_ITEM_BottleWithRedPotion,
	RANDO_ITEM_BottleWithGreenPotion,
	RANDO_ITEM_BottleWithBluePotion,
	RANDO_ITEM_BottleWithBee,
	RANDO_ITEM_BottleWithGoldBee,
	RANDO_ITEM_BottleWithFairy,
};

static void check_fountain_index(const char *functionality, int64_t last)
{
	int seen[7] = {0, 0, 0, 0, 0, 0, 0};
	uint32_t seed;
	int64_t i;

	for (seed = 0; seed < 64; seed++) {
		RandoWorld world;
		RandoPrng prng, twin;
		int64_t waterfall, pyramid;

		functionality_world(&world, functionality);
		seeded(&prng, seed, 0);
		seeded(&twin, seed, 0);
		rando_set_fountains(&world, &prng);
		waterfall = rando_random_int(&twin, 1, last);
		pyramid = rando_random_int(&twin, 1, last);
		CHECK(rando_world_item(&world, RANDO_LOC_WATERFALL_BOTTLE) == vt_bottles[waterfall]);
		CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_BOTTLE) == vt_bottles[pyramid]);
		seen[waterfall]++;
		seen[pyramid]++;
	}
	CHECK(!seen[0]);
	for (i = 1; i <= last; i++)
		CHECK(seen[i]);
	for (; i < 7; i++)
		CHECK(!seen[i]);
}

typedef struct {
	int n;
	int64_t min, max;
	bool mixed;
} DrawRange;

static void record_range(void *ctx, int64_t min, int64_t max, int64_t r)
{
	DrawRange *range = ctx;

	(void)r;
	if (range->n++ > 0 && (range->min != min || range->max != max))
		range->mixed = true;
	range->min = min;
	range->max = max;
}

static void check_fountain_range(const char *functionality, int64_t last)
{
	RandoWorld world;
	RandoPrng prng;
	DrawRange range = {0, 0, 0, false};

	functionality_world(&world, functionality);
	seeded(&prng, 3, 0);
	prng.observe = record_range;
	prng.observe_ctx = &range;
	rando_set_fountains(&world, &prng);
	CHECK(range.n == 2);
	CHECK(!range.mixed && range.min == 1 && range.max == last);
}

static void check_fountain_preset(void)
{
	RandoWorld world;
	RandoPrng prng, twin;
	int draws = 0;

	functionality_world(&world, "normal");
	CHECK(rando_world_set_item(&world, RANDO_LOC_PYRAMID_BOTTLE, RANDO_ITEM_Bottle));
	seeded(&prng, 7, 0);
	seeded(&twin, 7, 0);
	prng.observe = count_draw;
	prng.observe_ctx = &draws;
	rando_set_fountains(&world, &prng);
	CHECK(draws == 1);
	CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_BOTTLE) == RANDO_ITEM_Bottle);
	CHECK(rando_world_item(&world, RANDO_LOC_WATERFALL_BOTTLE) == vt_bottles[rando_random_int(&twin, 1, 6)]);
}

#define H RANDO_DROP_HEART
#define RG RANDO_DROP_RUPEE_GREEN
#define RB RANDO_DROP_RUPEE_BLUE
#define RR RANDO_DROP_RUPEE_RED
#define B1 RANDO_DROP_BOMB_REFILL_1
#define B4 RANDO_DROP_BOMB_REFILL_4
#define B8 RANDO_DROP_BOMB_REFILL_8
#define MS RANDO_DROP_MAGIC_REFILL_SMALL
#define MF RANDO_DROP_MAGIC_REFILL_FULL
#define A5 RANDO_DROP_ARROW_REFILL_5
#define A10 RANDO_DROP_ARROW_REFILL_10
#define F RANDO_DROP_FAIRY

/* VT's packs 0 to 6 after Randomizer::randomize() on cases/baseline attempt 0 (trace lines 73-79:
 * 1 0 1 3 0 1 0), dumped from VT itself when step 2.3.5 was written. */
static const RandoDrop baseline_packs[7][RANDO_PRIZE_PACK_SLOTS] = {
	{MF, MS, MS, RB, MF, MS, H, MS},
	{A5, H, A5, A10, A5, H, A5, A10},
	{MS, RG, H, A5, MS, B1, RG, H},
	{B1, B1, B1, B4, B1, B1, B8, B1},
	{H, F, MF, RR, B8, H, RR, A10},
	{H, H, H, H, RG, H, H, RG},
	{RB, RG, RB, RR, RB, RG, RB, RB},
};

static void baseline_prize_world(RandoWorld *world, RandoPrng *prng)
{
	open_world(world, RANDO_ITEM_POOL_NORMAL);
	seeded(prng, 1, 72);
}

static bool special_packs_empty(const RandoWorld *world)
{
	int p, s;

	for (p = RANDO_PRIZE_PACK_PULL; p < RANDO_PRIZE_PACK_COUNT; p++)
		for (s = 0; s < rando_prize_pack_slots((RandoPrizePack)p); s++)
			if (rando_world_drop(world, (RandoPrizePack)p, s) != RANDO_DROP_NONE)
				return false;
	return true;
}

static void check_prize_packs_golden(void)
{
	RandoWorld world;
	RandoPrng prng;
	int p, s;

	baseline_prize_world(&world, &prng);
	rando_shuffle_prize_packs(&world, &prng);
	for (p = 0; p < 7; p++)
		for (s = 0; s < RANDO_PRIZE_PACK_SLOTS; s++)
			CHECK(rando_world_drop(&world, (RandoPrizePack)p, s) == baseline_packs[p][s]);
	CHECK(special_packs_empty(&world));
}

typedef struct {
	int n;
	int64_t max[8];
	bool nonzero_min;
} PackRanges;

static void record_pack_range(void *ctx, int64_t min, int64_t max, int64_t r)
{
	PackRanges *ranges = ctx;

	(void)r;
	if (min != 0)
		ranges->nonzero_min = true;
	if (ranges->n < 8)
		ranges->max[ranges->n] = max;
	ranges->n++;
}

/* fy_shuffle over seven rows: 0..6 down to 0..0 (every golden's shufflePrizePacks lines). */
static void check_prize_pack_range(void)
{
	RandoWorld world;
	RandoPrng prng;
	PackRanges ranges = {0, {0}, false};
	int i;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	seeded(&prng, 5, 0);
	prng.observe = record_pack_range;
	prng.observe_ctx = &ranges;
	rando_shuffle_prize_packs(&world, &prng);
	CHECK(ranges.n == 7 && !ranges.nonzero_min);
	for (i = 0; i < 7; i++)
		CHECK(ranges.max[i] == 6 - i);
}

/* VT's $random_vanilla_packs[$key][$i]: slots 0, 3 and 6 of each vanilla row tell the seven apart. */
static const RandoDrop vanilla_marks[7][3] = {
	{H, H, H}, {RB, RR, RB}, {MF, RB, H}, {B1, B4, B8}, {A5, A10, A5}, {MS, A5, RG}, {H, RR, RR},
};

static void check_prize_pack_index(void)
{
	int seen[7] = {0, 0, 0, 0, 0, 0, 0};
	uint32_t seed;
	int k;

	for (seed = 0; seed < 64; seed++) {
		RandoWorld world;
		RandoPrng prng, twin;
		uint8_t order[7] = {0, 1, 2, 3, 4, 5, 6};

		open_world(&world, RANDO_ITEM_POOL_NORMAL);
		seeded(&prng, seed, 0);
		seeded(&twin, seed, 0);
		rando_shuffle_prize_packs(&world, &prng);
		rando_fy_shuffle(&twin, order, 7, 1);
		for (k = 0; k < 7; k++) {
			CHECK(rando_world_drop(&world, (RandoPrizePack)k, 0) == vanilla_marks[order[k]][0]);
			CHECK(rando_world_drop(&world, (RandoPrizePack)k, 3) == vanilla_marks[order[k]][1]);
			CHECK(rando_world_drop(&world, (RandoPrizePack)k, 6) == vanilla_marks[order[k]][2]);
		}
		seen[order[0]]++;
	}
	for (k = 0; k < 7; k++)
		CHECK(seen[k]);
}

/* customPrizePacks: no draw, every pack stays empty. */
static void check_prize_packs_custom(void)
{
	RandoWorld world;
	RandoConfig config;
	RandoPrng prng;
	int draws = 0, p, s;

	rando_config_init(&config);
	CHECK(!config.custom_prize_packs);
	config.custom_prize_packs = true;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	seeded(&prng, 1, 0);
	prng.observe = count_draw;
	prng.observe_ctx = &draws;
	rando_shuffle_prize_packs(&world, &prng);
	CHECK(draws == 0);
	for (p = 0; p < RANDO_PRIZE_PACK_COUNT; p++)
		for (s = 0; s < rando_prize_pack_slots((RandoPrizePack)p); s++)
			CHECK(rando_world_drop(&world, (RandoPrizePack)p, s) == RANDO_DROP_NONE);
}

/* shufflePrizePacks touches no location, shop or collected flag. */
static void check_prize_packs_leave_world(void)
{
	RandoWorld world, before;
	RandoPrng prng;

	baseline_prize_world(&world, &prng);
	before = world;
	rando_shuffle_prize_packs(&world, &prng);
	CHECK(memcmp(world.location_items, before.location_items, sizeof world.location_items) == 0);
	CHECK(memcmp(world.shop_items, before.shop_items, sizeof world.shop_items) == 0);
	CHECK(memcmp(world.collected, before.collected, sizeof world.collected) == 0);
	CHECK(memcmp(world.shop_collected, before.shop_collected, sizeof world.shop_collected) == 0);
}

/* Randomizer::__construct: three hearts, ten bombs and thirty arrows, only into an empty collection. */
static void check_start_equipment(void)
{
	RandoWorld world;

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	rando_prepare_start_equipment(&world);
	CHECK(rando_items_count(&world.pre_collected) == 7);
	CHECK(world.pre_collected.counts[RANDO_ITEM_BossHeartContainer] == 3);
	CHECK(world.pre_collected.values[RANDO_ITEM_BossHeartContainer] == 3);
	CHECK(world.pre_collected.counts[RANDO_ITEM_BombUpgrade10] == 1);
	CHECK(world.pre_collected.counts[RANDO_ITEM_ArrowUpgrade10] == 3);
	CHECK(rando_items_has_health(&world.pre_collected, 3));
	CHECK(rando_world_pre_collected_count(&world) == 7);
	CHECK(rando_world_pre_collected_item(&world, 2) == RANDO_ITEM_BossHeartContainer);
	CHECK(rando_world_pre_collected_item(&world, 3) == RANDO_ITEM_BombUpgrade10);
	CHECK(rando_world_pre_collected_item(&world, 6) == RANDO_ITEM_ArrowUpgrade10);

	open_world(&world, RANDO_ITEM_POOL_NORMAL);
	rando_items_add(&world.pre_collected, RANDO_ITEM_PegasusBoots);
	rando_prepare_start_equipment(&world);
	CHECK(rando_items_count(&world.pre_collected) == 1);
}

static void goal_world(RandoWorld *world, RandoGoal goal)
{
	RandoConfig config;

	rando_config_init(&config);
	config.goal = goal;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
}

/* prepareWorld's goal switch: the Triforce at Ganon, on the pedestal for pedestal, nowhere for triforce-hunt. */
static void check_goal_triforce(void)
{
	static const RandoGoal at_ganon[] = {
		RANDO_GOAL_GANON, RANDO_GOAL_FAST_GANON, RANDO_GOAL_DUNGEONS, RANDO_GOAL_GANONHUNT, RANDO_GOAL_COMPLETIONIST,
	};
	RandoWorld world;
	size_t i;

	for (i = 0; i < sizeof at_ganon / sizeof at_ganon[0]; i++) {
		goal_world(&world, at_ganon[i]);
		rando_prepare_goal(&world);
		CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_Triforce);
		CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_NONE);
	}
	goal_world(&world, RANDO_GOAL_PEDESTAL);
	rando_prepare_goal(&world);
	CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_Triforce);
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	goal_world(&world, RANDO_GOAL_TRIFORCE_HUNT);
	rando_prepare_goal(&world);
	CHECK(rando_world_item(&world, RANDO_LOC_GANON) == RANDO_ITEM_DefeatGanon);
	CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) == RANDO_ITEM_NONE);
}

/* completionist forces accessibility locations; other goals keep the world's value. */
static void check_goal_accessibility(void)
{
	RandoWorld world;

	goal_world(&world, RANDO_GOAL_COMPLETIONIST);
	world.config.accessibility = RANDO_ACCESSIBILITY_ITEMS;
	rando_prepare_goal(&world);
	CHECK(world.config.accessibility == RANDO_ACCESSIBILITY_LOCATIONS);
	goal_world(&world, RANDO_GOAL_GANON);
	world.config.accessibility = RANDO_ACCESSIBILITY_ITEMS;
	rando_prepare_goal(&world);
	CHECK(world.config.accessibility == RANDO_ACCESSIBILITY_ITEMS);
}

static void logic_world(RandoWorld *world, RandoVariant variant, RandoLogic logic)
{
	RandoConfig config;

	rando_config_init(&config);
	config.logic = logic;
	rando_world_init(world, variant, &config);
}

static void hand_pools(RandoPools *pools, const RandoItemId *advancement, size_t nadvancement)
{
	size_t i;

	memset(pools, 0, sizeof *pools);
	for (i = 0; i < nadvancement; i++)
		pools->items[RANDO_POOL_ADVANCEMENT][i] = (uint8_t)advancement[i];
	pools->count[RANDO_POOL_ADVANCEMENT] = (uint16_t)nadvancement;
	pools->items[RANDO_POOL_ITEMS][0] = RANDO_ITEM_Arrow;
	pools->count[RANDO_POOL_ITEMS] = 1;
}

static int same_list(const RandoPools *pools, RandoPoolList list, const RandoItemId *expected, size_t n)
{
	size_t i;

	if (pools->count[list] != n)
		return 0;
	for (i = 0; i < n; i++)
		if (pools->items[list][i] != expected[i])
			return 0;
	return 1;
}

/* prepareWorld after its getters: RescueZelda outside standard; under the four glitched logics,
 * PegasusBoots pre-collected and the first one unset from the advancement list for a pushed TwentyRupees. */
static void check_pre_collect(void)
{
	static const RandoLogic glitched[] = {
		RANDO_LOGIC_OVERWORLD_GLITCHES, RANDO_LOGIC_HYBRID_MAJOR_GLITCHES, RANDO_LOGIC_MAJOR_GLITCHES,
		RANDO_LOGIC_NO_LOGIC,
	};
	static const RandoItemId advancement[] = {
		RANDO_ITEM_Hammer, RANDO_ITEM_PegasusBoots, RANDO_ITEM_Lamp, RANDO_ITEM_PegasusBoots, RANDO_ITEM_Flippers,
	};
	static const RandoItemId edited[] = {
		RANDO_ITEM_Hammer, RANDO_ITEM_Lamp, RANDO_ITEM_PegasusBoots, RANDO_ITEM_Flippers,
	};
	static const RandoItemId no_boots[] = {RANDO_ITEM_Hammer, RANDO_ITEM_Lamp};
	static const RandoItemId arrow[] = {RANDO_ITEM_Arrow};
	static const RandoItemId arrow_rupees[] = {RANDO_ITEM_Arrow, RANDO_ITEM_TwentyRupees};
	RandoWorld world;
	RandoPools pools;
	size_t i;

	logic_world(&world, RANDO_VARIANT_OPEN, RANDO_LOGIC_NO_GLITCHES);
	hand_pools(&pools, advancement, 5);
	rando_prepare_pre_collect(&world, &pools);
	CHECK(rando_items_has(&world.pre_collected, RANDO_ITEM_RescueZelda));
	CHECK(!rando_items_has(&world.pre_collected, RANDO_ITEM_PegasusBoots));
	CHECK(rando_items_count(&world.pre_collected) == 1);
	CHECK(rando_world_pre_collected_count(&world) == 1);
	CHECK(rando_world_pre_collected_item(&world, 0) == RANDO_ITEM_RescueZelda);
	CHECK(same_list(&pools, RANDO_POOL_ADVANCEMENT, advancement, 5));
	CHECK(same_list(&pools, RANDO_POOL_ITEMS, arrow, 1));

	logic_world(&world, RANDO_VARIANT_STANDARD, RANDO_LOGIC_NO_GLITCHES);
	hand_pools(&pools, advancement, 5);
	rando_prepare_pre_collect(&world, &pools);
	CHECK(rando_items_count(&world.pre_collected) == 0);

	for (i = 0; i < sizeof glitched / sizeof glitched[0]; i++) {
		logic_world(&world, RANDO_VARIANT_STANDARD, glitched[i]);
		hand_pools(&pools, advancement, 5);
		rando_prepare_pre_collect(&world, &pools);
		CHECK(rando_items_has(&world.pre_collected, RANDO_ITEM_PegasusBoots));
		CHECK(rando_items_count(&world.pre_collected) == 1);
		CHECK(rando_world_pre_collected_count(&world) == 1);
		CHECK(rando_world_pre_collected_item(&world, 0) == RANDO_ITEM_PegasusBoots);
		CHECK(same_list(&pools, RANDO_POOL_ADVANCEMENT, edited, 4));
		CHECK(same_list(&pools, RANDO_POOL_ITEMS, arrow_rupees, 2));

		logic_world(&world, RANDO_VARIANT_STANDARD, glitched[i]);
		hand_pools(&pools, no_boots, 2);
		rando_prepare_pre_collect(&world, &pools);
		CHECK(rando_items_has(&world.pre_collected, RANDO_ITEM_PegasusBoots));
		CHECK(same_list(&pools, RANDO_POOL_ADVANCEMENT, no_boots, 2));
		CHECK(same_list(&pools, RANDO_POOL_ITEMS, arrow, 1));
	}
}

static const struct {
	RandoRegionId region;
	RandoBossSlot slot;
	RandoBossId boss;
} placed_bosses[] = {
	{RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_DESERT_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_TOWER_OF_HERA, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOLDORM},
	{RANDO_REGION_PALACE_OF_DARKNESS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_HELMASAUR_KING},
	{RANDO_REGION_SWAMP_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARRGHUS},
	{RANDO_REGION_SKULL_WOODS, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_MOTHULA},
	{RANDO_REGION_THIEVES_TOWN, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_BLIND},
	{RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_KHOLDSTARE},
	{RANDO_REGION_MISERY_MIRE, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_VITREOUS},
	{RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_TRINEXX},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOTTOM, RANDO_BOSS_ARMOS_KNIGHTS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE, RANDO_BOSS_LANMOLAS},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, RANDO_BOSS_MOLDORM},
	{RANDO_REGION_HYRULE_CASTLE_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM},
	{RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_AGAHNIM2},
};

/* placeBosses with bossShuffle none: the baseline golden's Bosses block, whatever the slots held before. */
static void check_place_bosses(RandoWeapons weapons)
{
	RandoWorld world;
	RandoConfig config;
	size_t i;

	rando_config_init(&config);
	config.mode_weapons = weapons;
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	for (i = 0; i < sizeof placed_bosses / sizeof placed_bosses[0]; i++)
		rando_world_set_boss(&world, placed_bosses[i].region, placed_bosses[i].slot, RANDO_BOSS_TRINEXX);
	rando_world_set_boss(&world, RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, RANDO_BOSS_ARRGHUS);
	rando_place_bosses(&world);
	for (i = 0; i < sizeof placed_bosses / sizeof placed_bosses[0]; i++)
		CHECK(rando_world_boss(&world, placed_bosses[i].region, placed_bosses[i].slot) == placed_bosses[i].boss);
}

static const RandoItemId prizes[] = {
	RANDO_ITEM_Crystal1, RANDO_ITEM_Crystal2, RANDO_ITEM_Crystal3, RANDO_ITEM_Crystal4, RANDO_ITEM_Crystal5,
	RANDO_ITEM_Crystal6, RANDO_ITEM_Crystal7, RANDO_ITEM_PendantOfCourage, RANDO_ITEM_PendantOfPower,
	RANDO_ITEM_PendantOfWisdom,
};

/* The generator's open world just before fillPrizes, with the given item.count entries at 0. */
static void prize_world(RandoWorld *world, RandoPrng *prng, const RandoItemId *zeroed, size_t nzeroed)
{
	RandoConfig config;
	RandoPools pools;
	size_t i;

	memset(&pools, 0, sizeof pools);
	rando_config_init(&config);
	for (i = 0; i < nzeroed; i++)
		config.item_count[zeroed[i]] = 0;
	rando_world_init(world, RANDO_VARIANT_OPEN, &config);
	rando_prepare_start_equipment(world);
	rando_prepare_goal(world);
	rando_prepare_pre_collect(world, &pools);
	rando_set_shops(world, NULL);
	seeded(prng, 1, 0);
	rando_set_medallions(world, prng);
	rando_place_bosses(world);
}

/* fillPrizes' assumed collection with all ten prizes still to place. */
static void assume_all(RandoWorld *world, RandoPrng *prng, RandoItems *assumed)
{
	RandoItems start;
	size_t i;

	rando_world_reset_collected(world);
	rando_items_clear(&start);
	rando_pool_add_items(world, prng, RANDO_POOL_DUNGEON, &start);
	rando_pool_add_items(world, prng, RANDO_POOL_ADVANCEMENT, &start);
	for (i = 0; i < sizeof prizes / sizeof prizes[0]; i++)
		rando_items_add(&start, prizes[i]);
	rando_world_collect_items(world, &start, assumed);
}

static void count_location_order(void *ctx, int64_t min, int64_t max, int64_t r)
{
	(void)r;
	if (min == 0 && max == 13)
		++*(int *)ctx;
}

static bool holds_prize(const RandoWorld *world, RandoLocationId location)
{
	size_t i;

	for (i = 0; i < sizeof prizes / sizeof prizes[0]; i++)
		if (rando_world_has_item(world, location, prizes[i]))
			return true;
	return false;
}

/* No candidate reaches Turtle Rock without CaneOfSomaria: six passes (VT's $attempts 5 down to 0), each
 * opened by its randomCollection draw over 14 locations, then Cannot Place Prize. */
static void check_fill_prizes_all_refused(void)
{
	static const RandoItemId zeroed[] = {RANDO_ITEM_CaneOfSomaria};
	RandoWorld world;
	RandoPrng prng;
	RandoItems assumed;
	int passes = 0;

	prize_world(&world, &prng, zeroed, 1);
	assume_all(&world, &prng, &assumed);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_TURTLE_ROCK_PRIZE, &assumed));

	prng.observe = count_location_order;
	prng.observe_ctx = &passes;
	CHECK(!rando_fill_prizes(&world, &prng));
	CHECK(passes == 6);
	CHECK(!rando_world_has_item(&world, RANDO_LOC_TURTLE_ROCK_PRIZE, RANDO_ITEM_NONE));
}

static const RandoLocationId crystal_locations[] = {
	RANDO_LOC_PALACE_OF_DARKNESS_PRIZE, RANDO_LOC_SWAMP_PALACE_PRIZE, RANDO_LOC_SKULL_WOODS_PRIZE,
	RANDO_LOC_THIEVES_TOWN_PRIZE, RANDO_LOC_ICE_PALACE_PRIZE, RANDO_LOC_MISERY_MIRE_PRIZE,
	RANDO_LOC_TURTLE_ROCK_PRIZE,
};

/* Without BigKeyP2, Desert Palace (a Prize\Pendant) stays empty: each pendant-phase reset clears only the
 * pendant locations, so the crystal locations keep their prizes to the end. */
static void check_fill_prizes_pendant_failure(void)
{
	static const RandoItemId zeroed[] = {RANDO_ITEM_BigKeyP2};
	RandoWorld world;
	RandoPrng prng;
	RandoItems assumed;
	size_t i;
	int passes = 0;

	prize_world(&world, &prng, zeroed, 1);
	assume_all(&world, &prng, &assumed);
	CHECK(!rando_world_can_access(&world, RANDO_LOC_DESERT_PALACE_PRIZE, &assumed));
	for (i = 0; i < sizeof crystal_locations / sizeof crystal_locations[0]; i++)
		CHECK(rando_world_can_access(&world, crystal_locations[i], &assumed));
	CHECK(rando_world_check_win_condition(&world, &assumed));

	prng.observe = count_location_order;
	prng.observe_ctx = &passes;
	CHECK(!rando_fill_prizes(&world, &prng));
	CHECK(passes == 6);
	for (i = 0; i < sizeof crystal_locations / sizeof crystal_locations[0]; i++)
		CHECK(holds_prize(&world, crystal_locations[i]));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_DESERT_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_NONE));
	CHECK(!rando_world_has_item(&world, RANDO_LOC_TOWER_OF_HERA_PRIZE, RANDO_ITEM_NONE));
}

static void set_list(RandoPools *pools, RandoPoolList list, const RandoItemId *items, size_t n)
{
	size_t i;

	assert(n <= RANDO_POOL_CAPACITY);
	for (i = 0; i < n; i++)
		pools->items[list][i] = (uint8_t)items[i];
	pools->count[list] = (uint16_t)n;
}

#define SET_LIST(pools, list, items) set_list(pools, list, items, sizeof items / sizeof items[0])

static void config_world(RandoWorld *world, const RandoConfig *config)
{
	rando_world_init(world, config->mode_state == RANDO_STATE_STANDARD ? RANDO_VARIANT_STANDARD : RANDO_VARIANT_OPEN,
			 config);
}

/* expected holds the lists before VT's three shuffles; a twin PRNG shuffles them in VT's order
 * (advancement, nice, item pool), and they must then equal what rando_prepare_pools leaves. */
static void check_prepared(RandoWorld *world, RandoPools *pools, RandoPools *expected, int line)
{
	RandoPrng prng, twin;
	int list, i;

	seeded(&prng, 7, 0);
	seeded(&twin, 7, 0);
	rando_prepare_pools(world, &prng, pools);
	rando_fy_shuffle(&twin, expected->items[RANDO_POOL_ADVANCEMENT], expected->count[RANDO_POOL_ADVANCEMENT], 1);
	rando_fy_shuffle(&twin, expected->items[RANDO_POOL_NICE], expected->count[RANDO_POOL_NICE], 1);
	rando_fy_shuffle(&twin, expected->items[RANDO_POOL_ITEMS], expected->count[RANDO_POOL_ITEMS], 1);
	for (list = 0; list < RANDO_POOL_LIST_COUNT; list++) {
		if (pools->count[list] != expected->count[list])
			test_fail(TEST, "line %d, list %d: %d items, expected %d", line, list, pools->count[list],
				  expected->count[list]);
		for (i = 0; i < expected->count[list]; i++)
			if (pools->items[list][i] != expected->items[list][i])
				test_fail(TEST, "line %d, list %d[%d]: %s, expected %s", line, list, i,
					  rando_item_info((RandoItemId)pools->items[list][i])->name,
					  rando_item_info((RandoItemId)expected->items[list][i])->name);
	}
}

static const RandoItemId branch_advancement[] = {
	RANDO_ITEM_Hammer, RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_L1Sword, RANDO_ITEM_Bottle, RANDO_ITEM_Lamp,
	RANDO_ITEM_MasterSword, RANDO_ITEM_BottleWithBee,
};
static const RandoItemId branch_nice[] = {
	RANDO_ITEM_L3Sword, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_ProgressiveArmor, RANDO_ITEM_Cape,
	RANDO_ITEM_ProgressiveSword,
};
static const RandoItemId branch_items[] = {RANDO_ITEM_Arrow, RANDO_ITEM_PieceOfHeart, RANDO_ITEM_TenArrows};

/* The walks collect the swords in list order, L1Sword, MasterSword, L3Sword, ProgressiveSword: the
 * uncle pops the last one. */
static void branch_pools(RandoPools *pools)
{
	memset(pools, 0, sizeof *pools);
	SET_LIST(pools, RANDO_POOL_ADVANCEMENT, branch_advancement);
	SET_LIST(pools, RANDO_POOL_NICE, branch_nice);
	SET_LIST(pools, RANDO_POOL_ITEMS, branch_items);
}

static void branch_config(RandoConfig *config, RandoWeapons weapons, RandoItemPlacement placement)
{
	rando_config_init(config);
	config->mode_weapons = weapons;
	config->item_placement = placement;
}

/* SilverArrowUpgrade joins the nice list before its walk; health then armor go to the nice list unless
 * basic; the uncle's UncleSword then the master sword are pushed; one bottle back, the rest to nice. */
static void check_pools_randomized(void)
{
	static const RandoItemId advancement[] = {
		RANDO_ITEM_Hammer, RANDO_ITEM_Lamp, RANDO_ITEM_UncleSword, RANDO_ITEM_L3Sword, RANDO_ITEM_BottleWithBee,
	};
	static const RandoItemId nice[] = {
		RANDO_ITEM_Cape, RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_PieceOfHeart,
		RANDO_ITEM_ProgressiveArmor, RANDO_ITEM_L1Sword, RANDO_ITEM_MasterSword, RANDO_ITEM_Bottle,
	};
	static const RandoItemId items[] = {RANDO_ITEM_Arrow, RANDO_ITEM_TenArrows};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	branch_config(&config, RANDO_WEAPONS_RANDOMIZED, RANDO_PLACEMENT_ADVANCED);
	config_world(&world, &config);
	branch_pools(&pools);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ADVANCEMENT, advancement);
	SET_LIST(&expected, RANDO_POOL_NICE, nice);
	SET_LIST(&expected, RANDO_POOL_ITEMS, items);
	check_prepared(&world, &pools, &expected, __LINE__);
	CHECK(!rando_items_has(&world.pre_collected, RANDO_ITEM_UncleSword));
}

/* basic: health then armor to the advancement list; the World derives region.requireBetterSword from it. */
static void check_pools_basic(void)
{
	static const RandoItemId advancement[] = {
		RANDO_ITEM_Hammer, RANDO_ITEM_Lamp, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_PieceOfHeart,
		RANDO_ITEM_ProgressiveArmor, RANDO_ITEM_UncleSword, RANDO_ITEM_L3Sword, RANDO_ITEM_MasterSword,
		RANDO_ITEM_BottleWithBee,
	};
	static const RandoItemId nice[] = {RANDO_ITEM_Cape, RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_L1Sword, RANDO_ITEM_Bottle};
	static const RandoItemId items[] = {RANDO_ITEM_Arrow, RANDO_ITEM_TenArrows};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	branch_config(&config, RANDO_WEAPONS_RANDOMIZED, RANDO_PLACEMENT_BASIC);
	config_world(&world, &config);
	branch_pools(&pools);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ADVANCEMENT, advancement);
	SET_LIST(&expected, RANDO_POOL_NICE, nice);
	SET_LIST(&expected, RANDO_POOL_ITEMS, items);
	check_prepared(&world, &pools, &expected, __LINE__);
}

/* requireBetterSword pushes a third sword; takeAnys swaps one of those left for a TwentyRupees. The World
 * derives requireBetterSword from itemPlacement, so it is set after rando_world_init. */
static void check_pools_better_sword(void)
{
	static const RandoItemId advancement[] = {
		RANDO_ITEM_Hammer, RANDO_ITEM_Lamp, RANDO_ITEM_UncleSword, RANDO_ITEM_L3Sword, RANDO_ITEM_MasterSword,
		RANDO_ITEM_BottleWithBee,
	};
	static const RandoItemId nice[] = {
		RANDO_ITEM_Cape, RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_PieceOfHeart,
		RANDO_ITEM_ProgressiveArmor, RANDO_ITEM_Bottle,
	};
	static const RandoItemId items[] = {RANDO_ITEM_Arrow, RANDO_ITEM_TenArrows, RANDO_ITEM_TwentyRupees};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	branch_config(&config, RANDO_WEAPONS_RANDOMIZED, RANDO_PLACEMENT_ADVANCED);
	config.region_take_anys = true;
	config_world(&world, &config);
	world.config.region_require_better_sword = true;
	branch_pools(&pools);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ADVANCEMENT, advancement);
	SET_LIST(&expected, RANDO_POOL_NICE, nice);
	SET_LIST(&expected, RANDO_POOL_ITEMS, items);
	check_prepared(&world, &pools, &expected, __LINE__);
}

static void check_pools_assured(void)
{
	static const RandoItemId advancement[] = {
		RANDO_ITEM_Hammer, RANDO_ITEM_Lamp, RANDO_ITEM_L3Sword, RANDO_ITEM_BottleWithBee,
	};
	static const RandoItemId nice[] = {
		RANDO_ITEM_Cape, RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_PieceOfHeart,
		RANDO_ITEM_ProgressiveArmor, RANDO_ITEM_L1Sword, RANDO_ITEM_MasterSword, RANDO_ITEM_Bottle,
	};
	static const RandoItemId items[] = {RANDO_ITEM_Arrow, RANDO_ITEM_TenArrows, RANDO_ITEM_FiftyRupees};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	branch_config(&config, RANDO_WEAPONS_ASSURED, RANDO_PLACEMENT_ADVANCED);
	config_world(&world, &config);
	branch_pools(&pools);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ADVANCEMENT, advancement);
	SET_LIST(&expected, RANDO_POOL_NICE, nice);
	SET_LIST(&expected, RANDO_POOL_ITEMS, items);
	check_prepared(&world, &pools, &expected, __LINE__);
	CHECK(rando_items_has(&world.pre_collected, RANDO_ITEM_UncleSword));
	CHECK(rando_world_pre_collected_count(&world) == 1);
	CHECK(rando_world_pre_collected_item(&world, 0) == RANDO_ITEM_UncleSword);
}

/* Vanilla pre-fills from the end of the sword list and merges none back; with the Triforce on the
 * pedestal, the pedestal's sword becomes a TwentyRupees in the item pool. */
static void check_pools_vanilla(bool pedestal_goal)
{
	static const RandoItemId advancement[] = {RANDO_ITEM_Hammer, RANDO_ITEM_Lamp, RANDO_ITEM_BottleWithBee};
	static const RandoItemId nice[] = {
		RANDO_ITEM_Cape, RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_PieceOfHeart,
		RANDO_ITEM_ProgressiveArmor, RANDO_ITEM_Bottle,
	};
	static const RandoItemId items[] = {RANDO_ITEM_Arrow, RANDO_ITEM_TenArrows, RANDO_ITEM_TwentyRupees};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	branch_config(&config, RANDO_WEAPONS_VANILLA, RANDO_PLACEMENT_ADVANCED);
	if (pedestal_goal)
		config.goal = RANDO_GOAL_PEDESTAL;
	config_world(&world, &config);
	rando_prepare_goal(&world);
	branch_pools(&pools);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ADVANCEMENT, advancement);
	SET_LIST(&expected, RANDO_POOL_NICE, nice);
	set_list(&expected, RANDO_POOL_ITEMS, items, pedestal_goal ? 3 : 2);
	check_prepared(&world, &pools, &expected, __LINE__);
	CHECK(rando_world_item(&world, RANDO_LOC_LINKS_UNCLE) == RANDO_ITEM_UncleSword);
	CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_FAIRY_LEFT) == RANDO_ITEM_L3Sword);
	CHECK(rando_world_item(&world, RANDO_LOC_BLACKSMITH) == RANDO_ITEM_MasterSword);
	CHECK(rando_world_item(&world, RANDO_LOC_MASTER_SWORD_PEDESTAL) ==
	      (pedestal_goal ? RANDO_ITEM_Triforce : RANDO_ITEM_L1Sword));
}

/* Swordless: a TwentyRupees2 per sword; SilverArrowUpgrade pushed only when the collected items plus the
 * advancement list hold fewer than two ProgressiveBow and the nice list has none. */
static void check_pools_swordless(void)
{
	static const RandoItemId one_bow[] = {RANDO_ITEM_ProgressiveBow, RANDO_ITEM_L1Sword, RANDO_ITEM_Lamp};
	static const RandoItemId two_bows[] = {
		RANDO_ITEM_ProgressiveBow, RANDO_ITEM_ProgressiveBow, RANDO_ITEM_L1Sword, RANDO_ITEM_Lamp,
	};
	static const RandoItemId silvers[] = {
		RANDO_ITEM_ProgressiveBow, RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_L1Sword, RANDO_ITEM_Lamp,
	};
	static const RandoItemId sword[] = {RANDO_ITEM_ProgressiveSword};
	static const RandoItemId one_bow_out[] = {RANDO_ITEM_ProgressiveBow, RANDO_ITEM_Lamp, RANDO_ITEM_SilverArrowUpgrade};
	static const RandoItemId two_bows_out[] = {RANDO_ITEM_ProgressiveBow, RANDO_ITEM_ProgressiveBow, RANDO_ITEM_Lamp};
	static const RandoItemId rupees[] = {RANDO_ITEM_TwentyRupees2, RANDO_ITEM_TwentyRupees2};
	static const RandoItemId silvers_nice[] = {
		RANDO_ITEM_SilverArrowUpgrade, RANDO_ITEM_TwentyRupees2, RANDO_ITEM_TwentyRupees2,
	};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	branch_config(&config, RANDO_WEAPONS_SWORDLESS, RANDO_PLACEMENT_ADVANCED);

	config_world(&world, &config);
	memset(&pools, 0, sizeof pools);
	SET_LIST(&pools, RANDO_POOL_ADVANCEMENT, one_bow);
	SET_LIST(&pools, RANDO_POOL_NICE, sword);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ADVANCEMENT, one_bow_out);
	SET_LIST(&expected, RANDO_POOL_NICE, rupees);
	check_prepared(&world, &pools, &expected, __LINE__);

	config_world(&world, &config);
	memset(&pools, 0, sizeof pools);
	SET_LIST(&pools, RANDO_POOL_ADVANCEMENT, two_bows);
	SET_LIST(&pools, RANDO_POOL_NICE, sword);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ADVANCEMENT, two_bows_out);
	SET_LIST(&expected, RANDO_POOL_NICE, rupees);
	check_prepared(&world, &pools, &expected, __LINE__);

	config_world(&world, &config);
	memset(&pools, 0, sizeof pools);
	SET_LIST(&pools, RANDO_POOL_ADVANCEMENT, silvers);
	SET_LIST(&pools, RANDO_POOL_NICE, sword);
	memset(&expected, 0, sizeof expected);
	set_list(&expected, RANDO_POOL_ADVANCEMENT, one_bow_out, 2);
	SET_LIST(&expected, RANDO_POOL_NICE, silvers_nice);
	check_prepared(&world, &pools, &expected, __LINE__);
}

/* rom.rupeeBow: every Arrow and arrow upgrade leaves the item pool for a FiveRupees appended after it. */
static void check_pools_rupee_bow(void)
{
	static const RandoItemId items[] = {
		RANDO_ITEM_Arrow, RANDO_ITEM_ThreeBombs, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_TenArrows, RANDO_ITEM_RedPotion,
	};
	static const RandoItemId replaced[] = {
		RANDO_ITEM_ThreeBombs, RANDO_ITEM_RedPotion, RANDO_ITEM_FiveRupees, RANDO_ITEM_FiveRupees,
		RANDO_ITEM_FiveRupees,
	};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	rando_config_init(&config);
	config.rom_rupee_bow = true;
	config_world(&world, &config);
	memset(&pools, 0, sizeof pools);
	SET_LIST(&pools, RANDO_POOL_ITEMS, items);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_ITEMS, replaced);
	check_prepared(&world, &pools, &expected, __LINE__);
}

static void check_pools_skull_woods_key(void)
{
	static const RandoItemId dungeon[] = {RANDO_ITEM_KeyD1, RANDO_ITEM_KeyD3, RANDO_ITEM_BigKeyD3, RANDO_ITEM_KeyD3};
	static const RandoItemId left[] = {RANDO_ITEM_KeyD1, RANDO_ITEM_BigKeyD3, RANDO_ITEM_KeyD3};
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;

	rando_config_init(&config);
	config.region_force_skull_woods_key = true;
	config_world(&world, &config);
	memset(&pools, 0, sizeof pools);
	SET_LIST(&pools, RANDO_POOL_DUNGEON, dungeon);
	memset(&expected, 0, sizeof expected);
	SET_LIST(&expected, RANDO_POOL_DUNGEON, left);
	check_prepared(&world, &pools, &expected, __LINE__);
	CHECK(rando_world_item(&world, RANDO_LOC_SKULL_WOODS_PINBALL_ROOM) == RANDO_ITEM_KeyD3);
}

static const RandoItemId wild_dungeon[] = {
	RANDO_ITEM_KeyH2, RANDO_ITEM_BigKeyD1, RANDO_ITEM_MapD1, RANDO_ITEM_CompassD1, RANDO_ITEM_KeyD1,
	RANDO_ITEM_BigKeyD2, RANDO_ITEM_MapD2,
};

typedef struct WildCase {
	bool big_keys, keys, maps, compasses;
	RandoState state;
	RandoLogic logic;
	RandoItemId dungeon[8];
	RandoItemId advancement[8];
} WildCase;

#define N RANDO_ITEM_NONE

/* The four walks run big keys, keys, maps, compasses; standard keeps KeyH2 unless NoLogic. */
static const WildCase wild_cases[] = {
	{true, false, false, false, RANDO_STATE_OPEN, RANDO_LOGIC_NO_GLITCHES,
	 {RANDO_ITEM_KeyH2, RANDO_ITEM_MapD1, RANDO_ITEM_CompassD1, RANDO_ITEM_KeyD1, RANDO_ITEM_MapD2, N},
	 {RANDO_ITEM_BigKeyD1, RANDO_ITEM_BigKeyD2, N}},
	{false, true, false, false, RANDO_STATE_OPEN, RANDO_LOGIC_NO_GLITCHES,
	 {RANDO_ITEM_BigKeyD1, RANDO_ITEM_MapD1, RANDO_ITEM_CompassD1, RANDO_ITEM_BigKeyD2, RANDO_ITEM_MapD2, N},
	 {RANDO_ITEM_KeyH2, RANDO_ITEM_KeyD1, N}},
	{false, true, false, false, RANDO_STATE_STANDARD, RANDO_LOGIC_NO_GLITCHES,
	 {RANDO_ITEM_KeyH2, RANDO_ITEM_BigKeyD1, RANDO_ITEM_MapD1, RANDO_ITEM_CompassD1, RANDO_ITEM_BigKeyD2,
	  RANDO_ITEM_MapD2, N},
	 {RANDO_ITEM_KeyD1, N}},
	{false, true, false, false, RANDO_STATE_STANDARD, RANDO_LOGIC_NO_LOGIC,
	 {RANDO_ITEM_BigKeyD1, RANDO_ITEM_MapD1, RANDO_ITEM_CompassD1, RANDO_ITEM_BigKeyD2, RANDO_ITEM_MapD2, N},
	 {RANDO_ITEM_KeyH2, RANDO_ITEM_KeyD1, N}},
	{false, false, true, false, RANDO_STATE_OPEN, RANDO_LOGIC_NO_GLITCHES,
	 {RANDO_ITEM_KeyH2, RANDO_ITEM_BigKeyD1, RANDO_ITEM_CompassD1, RANDO_ITEM_KeyD1, RANDO_ITEM_BigKeyD2, N},
	 {RANDO_ITEM_MapD1, RANDO_ITEM_MapD2, N}},
	{false, false, false, true, RANDO_STATE_OPEN, RANDO_LOGIC_NO_GLITCHES,
	 {RANDO_ITEM_KeyH2, RANDO_ITEM_BigKeyD1, RANDO_ITEM_MapD1, RANDO_ITEM_KeyD1, RANDO_ITEM_BigKeyD2,
	  RANDO_ITEM_MapD2, N},
	 {RANDO_ITEM_CompassD1, N}},
	{true, true, true, true, RANDO_STATE_OPEN, RANDO_LOGIC_NO_GLITCHES,
	 {N},
	 {RANDO_ITEM_BigKeyD1, RANDO_ITEM_BigKeyD2, RANDO_ITEM_KeyH2, RANDO_ITEM_KeyD1, RANDO_ITEM_MapD1,
	  RANDO_ITEM_MapD2, RANDO_ITEM_CompassD1, N}},
};

#undef N

static size_t until_none(const RandoItemId *items)
{
	size_t n = 0;

	while (items[n] != RANDO_ITEM_NONE)
		n++;
	return n;
}

static void check_pools_wild(void)
{
	size_t c;

	for (c = 0; c < sizeof wild_cases / sizeof wild_cases[0]; c++) {
		const WildCase *w = &wild_cases[c];
		RandoConfig config;
		RandoWorld world;
		RandoPools pools, expected;

		rando_config_init(&config);
		config.region_wild_big_keys = w->big_keys;
		config.region_wild_keys = w->keys;
		config.region_wild_maps = w->maps;
		config.region_wild_compasses = w->compasses;
		config.mode_state = w->state;
		config.logic = w->logic;
		config_world(&world, &config);
		memset(&pools, 0, sizeof pools);
		SET_LIST(&pools, RANDO_POOL_DUNGEON, wild_dungeon);
		memset(&expected, 0, sizeof expected);
		set_list(&expected, RANDO_POOL_DUNGEON, w->dungeon, until_none(w->dungeon));
		set_list(&expected, RANDO_POOL_ADVANCEMENT, w->advancement, until_none(w->advancement));
		check_prepared(&world, &pools, &expected, (int)c);
	}
}

static const RandoLocationId boss_locations[] = {
	RANDO_LOC_DESERT_PALACE_BOSS, RANDO_LOC_EASTERN_PALACE_BOSS, RANDO_LOC_ICE_PALACE_BOSS,
	RANDO_LOC_MISERY_MIRE_BOSS, RANDO_LOC_PALACE_OF_DARKNESS_BOSS, RANDO_LOC_SKULL_WOODS_BOSS,
	RANDO_LOC_SWAMP_PALACE_BOSS, RANDO_LOC_THIEVES_TOWN_BOSS, RANDO_LOC_TURTLE_ROCK_BOSS,
	RANDO_LOC_TOWER_OF_HERA_BOSS,
};

/* Pyramid Fairy - Bow gets VT's trade item; the ten bosses get BossHeartContainer when the hearts are not
 * in the pool, otherwise region.bossesHaveItem's item, otherwise nothing. */
static void check_pools_prefills(bool hearts_in_pool, RandoItemId bosses_have, bool bow_upgrade, RandoItemId boss_item)
{
	RandoConfig config;
	RandoWorld world;
	RandoPools pools, expected;
	size_t i;

	rando_config_init(&config);
	config.region_boss_hearts_in_pool = hearts_in_pool;
	config.region_bosses_have_item = bosses_have;
	config.region_pyramid_bow_upgrade = bow_upgrade;
	config_world(&world, &config);
	memset(&pools, 0, sizeof pools);
	memset(&expected, 0, sizeof expected);
	check_prepared(&world, &pools, &expected, __LINE__);
	CHECK(rando_world_item(&world, RANDO_LOC_PYRAMID_FAIRY_BOW) ==
	      (bow_upgrade ? RANDO_ITEM_BowAndSilverArrows : RANDO_ITEM_BowAndArrows));
	for (i = 0; i < sizeof boss_locations / sizeof boss_locations[0]; i++)
		CHECK(rando_world_item(&world, boss_locations[i]) == boss_item);
}

typedef struct Ranges {
	int n;
	int64_t max[RANDO_POOL_CAPACITY];
} Ranges;

static void record_max(void *ctx, int64_t min, int64_t max, int64_t r)
{
	Ranges *ranges = ctx;

	(void)r;
	CHECK(min == 0);
	CHECK(ranges->n < RANDO_POOL_CAPACITY);
	ranges->max[ranges->n++] = max;
}

static RandoGen gen;

/* tests/rando/cases/baseline seed 1: prepareWorld's 153 draws are three countdowns, 0..42, 0..39, 0..69. */
static void check_pools_baseline_draws(void)
{
	static const int sizes[] = {43, 40, 70};
	RandoOptions options;
	RandoWorld world;
	RandoPools pools;
	RandoPrng prng;
	Ranges ranges;
	int s, k, d = 0;

	rando_options_init(&options);
	CHECK(rando_generate_until(&gen, &options, 1, 1, NULL, NULL, RANDO_STAGE_SHUFFLE_PRIZE_PACKS) == RANDO_INCOMPLETE);
	world = gen.world;
	pools = gen.pools;
	seeded(&prng, 1, 0);
	ranges.n = 0;
	prng.observe = record_max;
	prng.observe_ctx = &ranges;
	rando_prepare_pools(&world, &prng, &pools);
	CHECK(ranges.n == 153);
	for (s = 0; s < 3; s++)
		for (k = sizes[s] - 1; k >= 0; k--)
			CHECK(ranges.max[d++] == k);
}

static RandoItemId first_item(RandoPoolList list)
{
	return (RandoItemId)gen.pools.items[list][0];
}

static RandoItemId last_item(RandoPoolList list)
{
	return (RandoItemId)gen.pools.items[list][gen.pools.count[list] - 1];
}

/* oracle/prepared_pools.txt's baseline record: 63/43/40/70 items, first and last of each list
 * BigKeyA2..CompassP3, BossHeartContainer..Powder, BottleWithBluePotion..PieceOfHeart, TwentyRupees..ThreeBombs. */
static void check_pools_baseline_anchors(void)
{
	RandoOptions options;

	rando_options_init(&options);
	CHECK(rando_generate_until(&gen, &options, 1, 1, NULL, NULL, RANDO_STAGE_PREPARE_WORLD) == RANDO_INCOMPLETE);
	CHECK(gen.pools.count[RANDO_POOL_DUNGEON] == 63);
	CHECK(gen.pools.count[RANDO_POOL_ADVANCEMENT] == 43);
	CHECK(gen.pools.count[RANDO_POOL_NICE] == 40);
	CHECK(gen.pools.count[RANDO_POOL_ITEMS] == 70);
	CHECK(first_item(RANDO_POOL_DUNGEON) == RANDO_ITEM_BigKeyA2);
	CHECK(last_item(RANDO_POOL_DUNGEON) == RANDO_ITEM_CompassP3);
	CHECK(first_item(RANDO_POOL_ADVANCEMENT) == RANDO_ITEM_BossHeartContainer);
	CHECK(last_item(RANDO_POOL_ADVANCEMENT) == RANDO_ITEM_Powder);
	CHECK(first_item(RANDO_POOL_NICE) == RANDO_ITEM_BottleWithBluePotion);
	CHECK(last_item(RANDO_POOL_NICE) == RANDO_ITEM_PieceOfHeart);
	CHECK(first_item(RANDO_POOL_ITEMS) == RANDO_ITEM_TwentyRupees);
	CHECK(last_item(RANDO_POOL_ITEMS) == RANDO_ITEM_ThreeBombs);
	CHECK(rando_world_item(&gen.world, RANDO_LOC_PYRAMID_FAIRY_BOW) == RANDO_ITEM_BowAndArrows);
}

static int vanilla_swords(const char *file, const char *method)
{
	return strcmp(file, "RandomizerTest.php") == 0
	       && (strcmp(method, "testVanillaSwordsSet") == 0 || strcmp(method, "testVanillaSwordsSetWithPedestalGoal") == 0);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	int rows, total;

	check_shops_baseline();
	check_shops_hard();
	check_shops_expert();
	check_shops_crowd_control(RANDO_ITEM_TwentyRupees2);
	check_shops_crowd_control(RANDO_ITEM_Arrow);
	check_shops_retro();
	check_shops_retro_old_man(RANDO_WEAPONS_SWORDLESS);
	check_shops_retro_old_man(RANDO_WEAPONS_VANILLA);
	check_shops_retro_forest();
	check_shops_retro_hard();
	check_shops_retro_inverted();
	check_shops_rupee_bow_alone();
	check_medallions_golden();
	check_medallion_index();
	check_medallion_preset();
	check_fountains_golden();
	check_fountain_index("normal", 6);
	check_fountain_index("hard", 5);
	check_fountain_range("normal", 6);
	check_fountain_range("hard", 5);
	check_fountain_preset();
	check_prize_packs_golden();
	check_prize_pack_range();
	check_prize_pack_index();
	check_prize_packs_leave_world();
	check_prize_packs_custom();
	check_start_equipment();
	check_goal_triforce();
	check_goal_accessibility();
	check_pre_collect();
	check_place_bosses(RANDO_WEAPONS_RANDOMIZED);
	check_place_bosses(RANDO_WEAPONS_SWORDLESS);
	check_fill_prizes_all_refused();
	check_fill_prizes_pendant_failure();
	check_pools_randomized();
	check_pools_basic();
	check_pools_better_sword();
	check_pools_assured();
	check_pools_vanilla(false);
	check_pools_vanilla(true);
	check_pools_swordless();
	check_pools_rupee_bow();
	check_pools_skull_woods_key();
	check_pools_wild();
	check_pools_prefills(true, RANDO_ITEM_NONE, false, RANDO_ITEM_NONE);
	check_pools_prefills(true, RANDO_ITEM_TwentyRupees, true, RANDO_ITEM_TwentyRupees);
	check_pools_prefills(false, RANDO_ITEM_TwentyRupees, false, RANDO_ITEM_BossHeartContainer);
	check_pools_baseline_draws();
	check_pools_baseline_anchors();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	rows = fixture_replay_rows(TEST, FIXTURE_NOGLITCHES_ROWS, vanilla_swords, &total);
	if (rows != 8)
		test_fail(TEST, "%d RandomizerTest rows replayed, testVanillaSwordsSet and its pedestal variant have 8", rows);
	printf(TEST ": %d of %d rows replayed (RandomizerTest)\n", rows, total);
	return 0;
}
