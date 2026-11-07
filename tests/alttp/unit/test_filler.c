#include "rando_filler.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_filler"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define MAX_DRAWS 64

typedef struct Draws {
	int n;
	int64_t min[MAX_DRAWS], max[MAX_DRAWS], r[MAX_DRAWS];
} Draws;

static void record(void *ctx, int64_t min, int64_t max, int64_t r)
{
	Draws *d = ctx;

	if (d->n < MAX_DRAWS) {
		d->min[d->n] = min;
		d->max[d->n] = max;
		d->r[d->n] = r;
	}
	d->n++;
}

static void seeded(RandoPrng *prng, uint32_t seed, Draws *draws)
{
	memset(prng, 0, sizeof *prng);
	rando_prng_seed(prng, rando_splitmix32(seed));
	if (draws) {
		memset(draws, 0, sizeof *draws);
		prng->observe = record;
		prng->observe_ctx = draws;
	}
}

/* Every location filled with an item its class accepts, except empties[0..n). */
static void hand_world(RandoWorld *world, const RandoConfig *config, const RandoLocationId *empties, int n)
{
	int i;

	rando_world_init(world, RANDO_VARIANT_OPEN, config);
	for (i = 0; i < rando_layout_location_count(RANDO_VARIANT_OPEN); i++) {
		RandoLocationId l = rando_layout_location(RANDO_VARIANT_OPEN, i);
		RandoLocationClass c = rando_location_info(l)->location_class;
		RandoItemId item = RANDO_ITEM_TwentyRupees;

		if (rando_world_has_item(world, l, RANDO_ITEM_NONE))
			continue;
		if (c == RANDO_LOCATION_CLASS_PRIZE_PENDANT)
			item = RANDO_ITEM_PendantOfCourage;
		else if (rando_location_is_a(c, RANDO_LOCATION_CLASS_PRIZE))
			item = RANDO_ITEM_Crystal1;
		else if (c == RANDO_LOCATION_CLASS_MEDALLION)
			item = RANDO_ITEM_Ether;
		CHECK(rando_world_set_item(world, l, item));
	}
	for (i = 0; i < n; i++)
		CHECK(rando_world_set_item(world, empties[i], RANDO_ITEM_NONE));
}

static void default_world(RandoWorld *world, const RandoLocationId *empties, int n)
{
	RandoConfig config;

	rando_config_init(&config);
	hand_world(world, &config, empties, n);
}

static void set_list(RandoPools *pools, RandoPoolList list, const RandoItemId *items, int n)
{
	int i;

	for (i = 0; i < n; i++)
		pools->items[list][i] = (uint8_t)items[i];
	pools->count[list] = (uint16_t)n;
}

static int count_empty(const RandoWorld *world)
{
	int i, n = 0;

	for (i = 0; i < rando_layout_location_count(world->variant); i++)
		n += !rando_world_has_item(world, rando_layout_location(world->variant, i), RANDO_ITEM_NONE);
	return n;
}

/* Collection::randomCollection over ids[0..n) with twin's draws; ids is consumed. */
static int twin_collection(RandoPrng *twin, RandoLocationId *ids, int n, int number, RandoLocationId *out)
{
	int k = 0;

	while (number-- > 0 && n > 0) {
		int r = (int)rando_random_int(twin, 0, n - 1);

		out[k++] = ids[r];
		memmove(ids + r, ids + r + 1, (size_t)(n - r - 1) * sizeof ids[0]);
		n--;
	}
	return k;
}

/* In location order, reachable with RescueZelda alone, which the Zelda location holds. */
static const RandoLocationId reachable[] = {
	RANDO_LOC_KAKARIKO_TAVERN, RANDO_LOC_CHICKEN_HOUSE, RANDO_LOC_MUSHROOM, RANDO_LOC_LINKS_HOUSE,
	RANDO_LOC_SANCTUARY,
};
#define NREACHABLE ((int)(sizeof reachable / sizeof reachable[0]))

/* shuffleLocations: randomCollection(count) over the empty locations, one draw per location. */
static void check_shuffle_draws(void)
{
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;
	Draws draws;
	int i;

	default_world(&world, reachable, NREACHABLE);
	memset(&pools, 0, sizeof pools);
	seeded(&prng, 1, &draws);
	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(draws.n == NREACHABLE);
	for (i = 0; i < NREACHABLE; i++)
		CHECK(draws.min[i] == 0 && draws.max[i] == NREACHABLE - 1 - i);
	CHECK(count_empty(&world) == NREACHABLE);
}

/* In location order. */
static const RandoLocationId eastern[] = {
	RANDO_LOC_LINKS_HOUSE, RANDO_LOC_EASTERN_PALACE_COMPASS_CHEST, RANDO_LOC_EASTERN_PALACE_CANNONBALL_CHEST,
	RANDO_LOC_EASTERN_PALACE_BIG_KEY_CHEST, RANDO_LOC_EASTERN_PALACE_MAP_CHEST,
};
#define NEASTERN ((int)(sizeof eastern / sizeof eastern[0]))

/* Eastern Palace needs RescueZelda, here only in the pre-collected items. */
static void zelda_pre_collected(RandoWorld *world)
{
	CHECK(rando_world_set_item(world, RANDO_LOC_ZELDA, RANDO_ITEM_BigRedBomb));
	rando_world_add_pre_collected(world, RANDO_ITEM_RescueZelda);
}

/* The Big Key Chest needs the Lamp, which nothing assumes. */
static void eastern_world(RandoWorld *world, RandoPools *pools, RandoItemId dungeon_item)
{
	default_world(world, eastern, NEASTERN);
	zelda_pre_collected(world);
	memset(pools, 0, sizeof *pools);
	set_list(pools, RANDO_POOL_DUNGEON, &dungeon_item, 1);
}

static bool eastern_fillable(RandoLocationId location)
{
	return location == RANDO_LOC_EASTERN_PALACE_COMPASS_CHEST
		|| location == RANDO_LOC_EASTERN_PALACE_CANNONBALL_CHEST
		|| location == RANDO_LOC_EASTERN_PALACE_MAP_CHEST;
}

/* fillItemsInLocations: a non-map item goes to $fillable_locations->first() of the shuffled order. */
static void check_dungeon_first_fillable(uint32_t seed, int skipped)
{
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng, twin;
	RandoLocationId ids[NEASTERN], order[NEASTERN];
	Draws draws;
	int i;

	eastern_world(&world, &pools, RANDO_ITEM_BigKeyP1);
	seeded(&prng, seed, &draws);
	seeded(&twin, seed, NULL);
	memcpy(ids, eastern, sizeof ids);
	CHECK(twin_collection(&twin, ids, NEASTERN, NEASTERN, order) == NEASTERN);
	for (i = 0; !eastern_fillable(order[i]); i++)
		;
	CHECK(i == skipped);
	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(draws.n == NEASTERN);
	CHECK(rando_world_item(&world, order[i]) == RANDO_ITEM_BigKeyP1);
	CHECK(count_empty(&world) == NEASTERN - 1);
}

/* A map or a compass: $fillable_locations->random(), one draw 0..fillable-1 after the shuffle. */
static void check_dungeon_map_random(RandoItemId item, uint32_t seed)
{
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng, twin;
	RandoLocationId ids[NEASTERN], order[NEASTERN], fillable[NEASTERN];
	Draws draws;
	int i, nfillable = 0, pick;

	eastern_world(&world, &pools, item);
	seeded(&prng, seed, &draws);
	seeded(&twin, seed, NULL);
	memcpy(ids, eastern, sizeof ids);
	twin_collection(&twin, ids, NEASTERN, NEASTERN, order);
	for (i = 0; i < NEASTERN; i++)
		if (eastern_fillable(order[i]))
			fillable[nfillable++] = order[i];
	pick = (int)rando_random_int(&twin, 0, nfillable - 1);
	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(draws.n == NEASTERN + 1);
	CHECK(draws.min[NEASTERN] == 0 && draws.max[NEASTERN] == 2);
	CHECK(rando_world_item(&world, fillable[pick]) == item);
}

/* RandomAssumed.php:69: more items than empty locations throws before any placement or draw. */
static void check_dungeon_too_many(void)
{
	static const RandoItemId dungeon[] = {RANDO_ITEM_MapP1, RANDO_ITEM_CompassP1, RANDO_ITEM_BigKeyP1};
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;
	Draws draws;

	default_world(&world, eastern + 1, 2);
	zelda_pre_collected(&world);
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_DUNGEON, dungeon, 3);
	seeded(&prng, 2, &draws);
	CHECK(!rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(draws.n == 2);
	CHECK(count_empty(&world) == 2);
}

/* RandomAssumed.php:113: a BigKeyP1 with no fillable Eastern Palace location throws. */
static void check_dungeon_no_location(void)
{
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;
	RandoItemId big_key = RANDO_ITEM_BigKeyP1;
	Draws draws;

	default_world(&world, reachable, NREACHABLE);
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_DUNGEON, &big_key, 1);
	seeded(&prng, 3, &draws);
	CHECK(!rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(draws.n == NREACHABLE);
	CHECK(count_empty(&world) == NREACHABLE);
}

/* The assumed items start from the pre-collected ones: without RescueZelda, Eastern Palace is shut. */
static void check_dungeon_needs_pre_collected(void)
{
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;
	RandoItemId big_key = RANDO_ITEM_BigKeyP1;

	default_world(&world, eastern, NEASTERN);
	CHECK(rando_world_set_item(&world, RANDO_LOC_ZELDA, RANDO_ITEM_BigRedBomb));
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_DUNGEON, &big_key, 1);
	seeded(&prng, 3, NULL);
	CHECK(!rando_fill_dungeon(&world, &prng, &pools, &state));
}

/* The advancement and nice lists are assumed: a Lamp in either opens the Big Key Chest. */
static void check_dungeon_assumes_lists(RandoPoolList list, bool expected)
{
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;
	RandoLocationId big_key_chest = RANDO_LOC_EASTERN_PALACE_BIG_KEY_CHEST;
	RandoItemId big_key = RANDO_ITEM_BigKeyP1, lamp = RANDO_ITEM_Lamp;

	default_world(&world, &big_key_chest, 1);
	zelda_pre_collected(&world);
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_DUNGEON, &big_key, 1);
	set_list(&pools, list, &lamp, 1);
	seeded(&prng, 4, NULL);
	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state) == expected);
	CHECK(rando_world_has_item(&world, big_key_chest, RANDO_ITEM_BigKeyP1) == expected);
}

/* accessibility none skips the access check when the assumed items already win, except for a dungeon
 * item whose wild flag is off. Only the Lamp-locked Big Key Chest is empty. */
static void check_accessibility_none(RandoItemId item, bool triforce, bool wild_keys, bool expected)
{
	RandoWorld world;
	RandoConfig config;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;
	RandoLocationId big_key_chest = RANDO_LOC_EASTERN_PALACE_BIG_KEY_CHEST;

	rando_config_init(&config);
	config.accessibility = RANDO_ACCESSIBILITY_NONE;
	config.region_wild_keys = wild_keys;
	hand_world(&world, &config, &big_key_chest, 1);
	zelda_pre_collected(&world);
	if (triforce)
		rando_world_add_pre_collected(&world, RANDO_ITEM_Triforce);
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_DUNGEON, &item, 1);
	seeded(&prng, 9, NULL);
	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state) == expected);
	CHECK(rando_world_has_item(&world, big_key_chest, item) == expected);
}

static void twin_shuffle(RandoPrng *twin, uint8_t *items, int n)
{
	rando_fy_shuffle(twin, items, (size_t)n, 1);
}

/* fastFillItemsInLocations on a twin: items popped from the end onto the still empty locations. */
static void twin_fast_fill(RandoItemId *expected, const uint8_t *items, int n, const RandoLocationId *locations,
			   int nlocations)
{
	int i;

	for (i = 0; i < nlocations && n > 0; i++)
		if (expected[locations[i]] == RANDO_ITEM_NONE)
			expected[locations[i]] = (RandoItemId)items[--n];
}

/* Draw 1 of the rest is get_random_int over getGanonsTowerJunkFillRange(). */
static void check_junk_range(RandoGoal goal, RandoLogic logic, RandoState state_mode, int tower, int64_t lower,
			     int64_t upper)
{
	RandoWorld world;
	RandoConfig config;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;
	Draws draws;

	rando_config_init(&config);
	config.goal = goal;
	config.logic = logic;
	config.crystals_tower = tower;
	hand_world(&world, &config, NULL, 0);
	world.config.mode_state = state_mode;
	memset(&pools, 0, sizeof pools);
	seeded(&prng, 10, &draws);
	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(rando_fill_rest(&world, &prng, &pools, &state));
	CHECK(draws.n == 1);
	CHECK(draws.min[0] == lower && draws.max[0] == upper);
}

static const RandoLocationId tower_and_two[] = {
	RANDO_LOC_KAKARIKO_TAVERN, RANDO_LOC_LINKS_HOUSE, RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT,
	RANDO_LOC_GANONS_TOWER_TILE_ROOM, RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST,
};
#define NTOWER_AND_TWO ((int)(sizeof tower_and_two / sizeof tower_and_two[0]))

/* The junk fill takes k = get_random_int(0, 15) tower locations (randomCollection, region order), shuffles
 * the item pool and fast-fills its first k items there; the rest of the pool is shuffled again and
 * fast-filled on the reshuffled remaining locations, the surplus left unplaced. Returns k. */
static int check_junk_fill(uint32_t seed)
{
	static const RandoItemId extra[] = {
		RANDO_ITEM_Arrow, RANDO_ITEM_TenArrows, RANDO_ITEM_OneRupee, RANDO_ITEM_FiveRupees,
		RANDO_ITEM_FiftyRupees, RANDO_ITEM_ThreeBombs,
	};
	enum { NEXTRA = sizeof extra / sizeof extra[0] };
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng, twin;
	RandoItemId expected[RANDO_LOCATION_COUNT];
	RandoLocationId ids[NTOWER_AND_TWO], order[NTOWER_AND_TWO], chosen[3], rev[NTOWER_AND_TWO];
	uint8_t items[NEXTRA];
	Draws draws, twin_draws;
	int i, k, nchosen, ntrash, nrev = 0, nunused = 0;

	default_world(&world, tower_and_two, NTOWER_AND_TWO);
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_ITEMS, extra, NEXTRA);
	seeded(&prng, seed, &draws);
	seeded(&twin, seed, &twin_draws);

	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		expected[i] = RANDO_ITEM_NONE;
	memcpy(ids, tower_and_two, sizeof ids);
	twin_collection(&twin, ids, NTOWER_AND_TWO, NTOWER_AND_TWO, order);
	k = (int)rando_random_int(&twin, 0, 15);
	memcpy(ids, tower_and_two + 2, 3 * sizeof ids[0]);
	nchosen = twin_collection(&twin, ids, 3, k, chosen);
	for (i = 0; i < NEXTRA; i++)
		items[i] = (uint8_t)extra[i];
	twin_shuffle(&twin, items, NEXTRA);
	ntrash = nchosen;
	twin_fast_fill(expected, items, ntrash, chosen, nchosen);
	for (i = NTOWER_AND_TWO; i-- > 0;)
		if (expected[order[i]] == RANDO_ITEM_NONE)
			rev[nrev++] = order[i];
	twin_shuffle(&twin, NULL, 0);
	nrev = twin_collection(&twin, rev, nrev, nrev, order);
	twin_shuffle(&twin, items + ntrash, NEXTRA - ntrash);
	twin_fast_fill(expected, items + ntrash, NEXTRA - ntrash, order, nrev);

	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(rando_fill_rest(&world, &prng, &pools, &state));
	CHECK(draws.n == twin_draws.n);
	CHECK(draws.min[NTOWER_AND_TWO] == 0 && draws.max[NTOWER_AND_TWO] == 15);
	for (i = 0; i < NTOWER_AND_TWO; i++)
		CHECK(rando_world_item(&world, tower_and_two[i]) == expected[tower_and_two[i]]);
	for (i = 0; i < NEXTRA; i++) {
		int j, placed = 0;

		for (j = 0; j < NTOWER_AND_TWO; j++)
			placed += rando_world_item(&world, tower_and_two[j]) == extra[i];
		nunused += !placed;
	}
	CHECK(nunused == NEXTRA - NTOWER_AND_TWO);
	return k;
}

/* k = 9, 0 and 1: more junk than tower locations, none, and fewer. */
static void check_junk_fills(void)
{
	CHECK(check_junk_fill(0) == 9);
	CHECK(check_junk_fill(14) == 0);
	CHECK(check_junk_fill(39) == 1);
}

/* Advancement items, shuffled, each on the first empty location of the reversed order; then the
 * remaining locations reshuffled, the nice list and the item pool fast-filled, the surplus unplaced. */
static void check_advancement(uint32_t seed)
{
	static const RandoItemId advancement[] = {RANDO_ITEM_Hookshot, RANDO_ITEM_Hammer, RANDO_ITEM_FireRod};
	static const RandoItemId nice[] = {RANDO_ITEM_BossHeartContainer};
	static const RandoItemId extra[] = {RANDO_ITEM_Arrow, RANDO_ITEM_TenArrows, RANDO_ITEM_OneRupee};
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng, twin;
	RandoItemId expected[RANDO_LOCATION_COUNT];
	RandoLocationId ids[NREACHABLE], order[NREACHABLE], rev[NREACHABLE];
	RandoItems zelda;
	uint8_t adv[3], nice_items[1], extra_items[3];
	Draws draws, twin_draws;
	int i;

	default_world(&world, reachable, NREACHABLE);
	rando_items_clear(&zelda);
	rando_items_add(&zelda, RANDO_ITEM_RescueZelda);
	for (i = 0; i < NREACHABLE; i++)
		CHECK(rando_world_can_access(&world, reachable[i], &zelda));
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_ADVANCEMENT, advancement, 3);
	set_list(&pools, RANDO_POOL_NICE, nice, 1);
	set_list(&pools, RANDO_POOL_ITEMS, extra, 3);
	seeded(&prng, seed, &draws);
	seeded(&twin, seed, &twin_draws);

	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		expected[i] = RANDO_ITEM_NONE;
	memcpy(ids, reachable, sizeof ids);
	twin_collection(&twin, ids, NREACHABLE, NREACHABLE, order);
	rando_random_int(&twin, 0, 15);
	for (i = 0; i < 3; i++) {
		adv[i] = (uint8_t)advancement[i];
		extra_items[i] = (uint8_t)extra[i];
	}
	nice_items[0] = (uint8_t)nice[0];
	twin_shuffle(&twin, extra_items, 3);
	for (i = 0; i < NREACHABLE; i++)
		rev[i] = order[NREACHABLE - 1 - i];
	twin_shuffle(&twin, adv, 3);
	for (i = 0; i < 3; i++)
		expected[rev[i]] = (RandoItemId)adv[i];
	twin_collection(&twin, rev + 3, NREACHABLE - 3, NREACHABLE - 3, order);
	twin_shuffle(&twin, nice_items, 1);
	twin_fast_fill(expected, nice_items, 1, order, NREACHABLE - 3);
	twin_shuffle(&twin, extra_items, 3);
	twin_fast_fill(expected, extra_items, 3, order, NREACHABLE - 3);

	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(rando_fill_rest(&world, &prng, &pools, &state));
	CHECK(draws.n == twin_draws.n);
	for (i = 0; i < NREACHABLE; i++)
		CHECK(rando_world_item(&world, reachable[i]) == expected[reachable[i]]);
	CHECK(count_empty(&world) == 0);
}

static const RandoItemId three_advancement[] = {RANDO_ITEM_Hookshot, RANDO_ITEM_Hammer, RANDO_ITEM_FireRod};
static const RandoItemId eastern_big_key[] = {RANDO_ITEM_BigKeyP1};

/* The advancement phase throws as the dungeon phase does: too many items (:69), or none fillable (:113). */
static void check_advancement_throws(const RandoItemId *advancement, int n, int nempty)
{
	RandoWorld world;
	RandoPools pools;
	RandoFillState state;
	RandoPrng prng;

	default_world(&world, reachable, nempty);
	memset(&pools, 0, sizeof pools);
	set_list(&pools, RANDO_POOL_ADVANCEMENT, advancement, n);
	seeded(&prng, 11, NULL);
	CHECK(rando_fill_dungeon(&world, &prng, &pools, &state));
	CHECK(!rando_fill_rest(&world, &prng, &pools, &state));
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	check_shuffle_draws();
	check_dungeon_first_fillable(0, 0);
	check_dungeon_first_fillable(2, 2);
	check_dungeon_map_random(RANDO_ITEM_MapP1, 7);
	check_dungeon_map_random(RANDO_ITEM_CompassP1, 8);
	check_dungeon_too_many();
	check_dungeon_no_location();
	check_dungeon_needs_pre_collected();
	check_dungeon_assumes_lists(RANDO_POOL_ADVANCEMENT, true);
	check_dungeon_assumes_lists(RANDO_POOL_NICE, true);
	check_dungeon_assumes_lists(RANDO_POOL_ITEMS, false);
	check_accessibility_none(RANDO_ITEM_Hookshot, true, false, true);
	check_accessibility_none(RANDO_ITEM_Hookshot, false, false, false);
	check_accessibility_none(RANDO_ITEM_KeyP1, true, false, false);
	check_accessibility_none(RANDO_ITEM_KeyP1, true, true, true);
	check_junk_range(RANDO_GOAL_GANON, RANDO_LOGIC_NO_GLITCHES, RANDO_STATE_OPEN, 7, 0, 15);
	check_junk_range(RANDO_GOAL_GANON, RANDO_LOGIC_NO_GLITCHES, RANDO_STATE_OPEN, 3, 0, 6);
	check_junk_range(RANDO_GOAL_PEDESTAL, RANDO_LOGIC_NO_GLITCHES, RANDO_STATE_OPEN, 7, 15, 25);
	check_junk_range(RANDO_GOAL_TRIFORCE_HUNT, RANDO_LOGIC_NO_GLITCHES, RANDO_STATE_OPEN, 4, 8, 14);
	check_junk_range(RANDO_GOAL_GANON, RANDO_LOGIC_NO_LOGIC, RANDO_STATE_OPEN, 7, 0, 0);
	check_junk_range(RANDO_GOAL_GANON, RANDO_LOGIC_OVERWORLD_GLITCHES, RANDO_STATE_OPEN, 7, 0, 0);
	check_junk_range(RANDO_GOAL_GANON, RANDO_LOGIC_HYBRID_MAJOR_GLITCHES, RANDO_STATE_OPEN, 7, 0, 0);
	check_junk_range(RANDO_GOAL_PEDESTAL, RANDO_LOGIC_MAJOR_GLITCHES, RANDO_STATE_OPEN, 7, 0, 0);
	check_junk_range(RANDO_GOAL_GANON, RANDO_LOGIC_OVERWORLD_GLITCHES, RANDO_STATE_INVERTED, 7, 0, 15);
	check_junk_fills();
	check_advancement(12);
	check_advancement(13);
	check_advancement_throws(three_advancement, 3, 2);
	check_advancement_throws(eastern_big_key, 1, NREACHABLE);
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
