#include "rando_pool.h"

#include <assert.h>
#include <string.h>

typedef char pool_item_fits_uint8[RANDO_ITEM_NONE <= UINT8_MAX ? 1 : -1];

/* config/item.php's BottleWithRandom is not an item: getBottle resolves it where it stands. */
#define POOL_RANDOM_BOTTLE RANDO_ITEM_NONE
#define POOL_ENTRY_CAP 216

typedef struct RandoPoolEntry {
	uint8_t item;
	uint8_t count;
} RandoPoolEntry;

/* config/item.php, in its order: every later shuffle and the fill depend on it. */
static const RandoPoolEntry dungeon_table[] = {
	{RANDO_ITEM_BigKeyA2, 1},
	{RANDO_ITEM_BigKeyD1, 1},
	{RANDO_ITEM_BigKeyD2, 1},
	{RANDO_ITEM_BigKeyD3, 1},
	{RANDO_ITEM_BigKeyD4, 1},
	{RANDO_ITEM_BigKeyD5, 1},
	{RANDO_ITEM_BigKeyD6, 1},
	{RANDO_ITEM_BigKeyD7, 1},
	{RANDO_ITEM_BigKeyA1, 0},
	{RANDO_ITEM_BigKeyH2, 0},
	{RANDO_ITEM_BigKeyP1, 1},
	{RANDO_ITEM_BigKeyP2, 1},
	{RANDO_ITEM_BigKeyP3, 1},
	{RANDO_ITEM_KeyA2, 4},
	{RANDO_ITEM_KeyD1, 6},
	{RANDO_ITEM_KeyD2, 1},
	{RANDO_ITEM_KeyD3, 3},
	{RANDO_ITEM_KeyD4, 1},
	{RANDO_ITEM_KeyD5, 2},
	{RANDO_ITEM_KeyD6, 3},
	{RANDO_ITEM_KeyD7, 4},
	{RANDO_ITEM_KeyA1, 2},
	{RANDO_ITEM_KeyH2, 1},
	{RANDO_ITEM_KeyP1, 0},
	{RANDO_ITEM_KeyP2, 1},
	{RANDO_ITEM_KeyP3, 1},
	{RANDO_ITEM_MapA2, 1},
	{RANDO_ITEM_MapD1, 1},
	{RANDO_ITEM_MapD2, 1},
	{RANDO_ITEM_MapD3, 1},
	{RANDO_ITEM_MapD4, 1},
	{RANDO_ITEM_MapD5, 1},
	{RANDO_ITEM_MapD6, 1},
	{RANDO_ITEM_MapD7, 1},
	{RANDO_ITEM_MapA1, 0},
	{RANDO_ITEM_MapH2, 1},
	{RANDO_ITEM_MapP1, 1},
	{RANDO_ITEM_MapP2, 1},
	{RANDO_ITEM_MapP3, 1},
	{RANDO_ITEM_CompassA2, 1},
	{RANDO_ITEM_CompassD1, 1},
	{RANDO_ITEM_CompassD2, 1},
	{RANDO_ITEM_CompassD3, 1},
	{RANDO_ITEM_CompassD4, 1},
	{RANDO_ITEM_CompassD5, 1},
	{RANDO_ITEM_CompassD6, 1},
	{RANDO_ITEM_CompassD7, 1},
	{RANDO_ITEM_CompassA1, 0},
	{RANDO_ITEM_CompassH2, 0},
	{RANDO_ITEM_CompassP1, 1},
	{RANDO_ITEM_CompassP2, 1},
	{RANDO_ITEM_CompassP3, 1},
};

static const RandoPoolEntry advancement_table[] = {
	{RANDO_ITEM_L1Sword, 0},
	{RANDO_ITEM_MasterSword, 0},
	{RANDO_ITEM_ProgressiveSword, 4},
	{RANDO_ITEM_BossHeartContainer, 10},
	{POOL_RANDOM_BOTTLE, 4},
	{RANDO_ITEM_Bottle, 0},
	{RANDO_ITEM_BottleWithRedPotion, 0},
	{RANDO_ITEM_BottleWithGreenPotion, 0},
	{RANDO_ITEM_BottleWithBluePotion, 0},
	{RANDO_ITEM_BottleWithBee, 0},
	{RANDO_ITEM_BottleWithGoldBee, 0},
	{RANDO_ITEM_BottleWithFairy, 0},
	{RANDO_ITEM_Bombos, 1},
	{RANDO_ITEM_BookOfMudora, 1},
	{RANDO_ITEM_Bow, 0},
	{RANDO_ITEM_BowAndArrows, 0},
	{RANDO_ITEM_CaneOfSomaria, 1},
	{RANDO_ITEM_Cape, 1},
	{RANDO_ITEM_Ether, 1},
	{RANDO_ITEM_FireRod, 1},
	{RANDO_ITEM_Flippers, 1},
	{RANDO_ITEM_Hammer, 1},
	{RANDO_ITEM_Hookshot, 1},
	{RANDO_ITEM_IceRod, 1},
	{RANDO_ITEM_Lamp, 1},
	{RANDO_ITEM_MagicMirror, 1},
	{RANDO_ITEM_MoonPearl, 1},
	{RANDO_ITEM_Mushroom, 1},
	{RANDO_ITEM_OcarinaInactive, 1},
	{RANDO_ITEM_OcarinaActive, 0},
	{RANDO_ITEM_PegasusBoots, 1},
	{RANDO_ITEM_Powder, 1},
	{RANDO_ITEM_PowerGlove, 0},
	{RANDO_ITEM_Quake, 1},
	{RANDO_ITEM_Shovel, 1},
	{RANDO_ITEM_TitansMitt, 0},
	{RANDO_ITEM_BowAndSilverArrows, 0},
	{RANDO_ITEM_SilverArrowUpgrade, 0},
	{RANDO_ITEM_ProgressiveGlove, 2},
	{RANDO_ITEM_ProgressiveBow, 2},
	{RANDO_ITEM_Triforce, 0},
	{RANDO_ITEM_TriforcePiece, 0},
	{RANDO_ITEM_PowerStar, 0},
	{RANDO_ITEM_BugCatchingNet, 1},
	{RANDO_ITEM_MirrorShield, 0},
	{RANDO_ITEM_ProgressiveShield, 3},
	{RANDO_ITEM_CaneOfByrna, 1},
	{RANDO_ITEM_TenBombs, 1},
	{RANDO_ITEM_HalfMagic, 1},
	{RANDO_ITEM_QuarterMagic, 0},
};

static const RandoPoolEntry nice_table[] = {
	{RANDO_ITEM_L3Sword, 0},
	{RANDO_ITEM_L4Sword, 0},
	{RANDO_ITEM_HeartContainer, 1},
	{RANDO_ITEM_BlueShield, 0},
	{RANDO_ITEM_ProgressiveArmor, 2},
	{RANDO_ITEM_BlueMail, 0},
	{RANDO_ITEM_Boomerang, 1},
	{RANDO_ITEM_RedBoomerang, 1},
	{RANDO_ITEM_RedShield, 0},
	{RANDO_ITEM_RedMail, 0},
	{RANDO_ITEM_BlueClock, 0},
	{RANDO_ITEM_RedClock, 0},
	{RANDO_ITEM_GreenClock, 0},
	{RANDO_ITEM_OneHundredRupees, 1},
	{RANDO_ITEM_ThreeHundredRupees, 5},
};

static const RandoPoolEntry junk_table[] = {
	{RANDO_ITEM_PieceOfHeart, 24},
	{RANDO_ITEM_BombUpgrade5, 0},
	{RANDO_ITEM_BombUpgrade10, 0},
	{RANDO_ITEM_ArrowUpgrade5, 0},
	{RANDO_ITEM_ArrowUpgrade10, 0},
	{RANDO_ITEM_Arrow, 1},
	{RANDO_ITEM_TenArrows, 12},
	{RANDO_ITEM_Bomb, 0},
	{RANDO_ITEM_ThreeBombs, 16},
	{RANDO_ITEM_OneRupee, 2},
	{RANDO_ITEM_FiveRupees, 4},
	{RANDO_ITEM_TwentyRupees, 28},
	{RANDO_ITEM_TwentyRupees2, 0},
	{RANDO_ITEM_FiftyRupees, 7},
	{RANDO_ITEM_Heart, 0},
	{RANDO_ITEM_SmallMagic, 0},
	{RANDO_ITEM_Rupoor, 0},
};

RandoItemId rando_get_bottle(const RandoWorld *world, RandoPrng *prng, bool filled)
{
	static const RandoItemId bottles[] = {
		RANDO_ITEM_Bottle,
		RANDO_ITEM_BottleWithRedPotion,
		RANDO_ITEM_BottleWithGreenPotion,
		RANDO_ITEM_BottleWithBluePotion,
		RANDO_ITEM_BottleWithBee,
		RANDO_ITEM_BottleWithGoldBee,
		RANDO_ITEM_BottleWithFairy,
	};
	const int64_t fairy = (int64_t)(sizeof bottles / sizeof bottles[0]) - 1;

	assert(bottles[fairy] == RANDO_ITEM_BottleWithFairy
	       && "getBottle: without CatchableFairies the draw drops the last bottle, which must be the fairy one");
	return bottles[rando_random_int(prng, filled ? 1 : 0, world->config.rom_catchable_fairies ? fairy : fairy - 1)];
}

static void assert_room(RandoPoolList list, int count)
{
	bool full = count >= RANDO_POOL_CAPACITY;

	assert(!(full && list == RANDO_POOL_DUNGEON) && "dungeon list over RANDO_POOL_CAPACITY");
	assert(!(full && list == RANDO_POOL_ADVANCEMENT) && "advancement list over RANDO_POOL_CAPACITY");
	assert(!(full && list == RANDO_POOL_NICE) && "nice list over RANDO_POOL_CAPACITY");
	assert(!(full && list == RANDO_POOL_ITEMS) && "items list over RANDO_POOL_CAPACITY");
	(void)full;
	(void)list;
}

/* Exactly one of list_items and collection is set. */
typedef struct PoolSink {
	uint8_t *list_items;
	uint16_t *list_count;
	RandoItems *collection;
} PoolSink;

static void walk_list(const RandoWorld *world, RandoPrng *prng, RandoPoolList list, PoolSink *sink)
{
	static const struct {
		const RandoPoolEntry *entries;
		int n;
	} tables[RANDO_POOL_LIST_COUNT] = {
#define TABLE(t) {t, (int)(sizeof t / sizeof t[0])}
		TABLE(dungeon_table),
		TABLE(advancement_table),
		TABLE(nice_table),
		TABLE(junk_table),
#undef TABLE
	};
	const RandoPoolEntry *table;
	int e, i;

	assert(list < RANDO_POOL_LIST_COUNT);
	table = tables[list].entries;
	for (e = 0; e < tables[list].n; e++) {
		int count = table[e].count;
		int n;

		if (table[e].item != POOL_RANDOM_BOTTLE && world->config.item_count[table[e].item] != RANDO_CONFIG_NULL)
			count = world->config.item_count[table[e].item];
		n = count < POOL_ENTRY_CAP ? count : POOL_ENTRY_CAP;
		for (i = 0; i < n; i++) {
			RandoItemId item = (RandoItemId)table[e].item;

			if (item == POOL_RANDOM_BOTTLE) {
				/* VT would label this draw with a stage the stage table lacks. */
				assert(list == RANDO_POOL_ADVANCEMENT && "random bottle outside the advancement list");
				item = rando_get_bottle(world, prng, false);
			}
			if (sink->collection) {
				rando_items_add(sink->collection, item);
			} else {
				assert_room(list, *sink->list_count);
				sink->list_items[(*sink->list_count)++] = (uint8_t)item;
			}
		}
	}
}

void rando_pool_build(const RandoWorld *world, RandoPrng *prng, RandoPools *pools)
{
	int list;

	for (list = 0; list < RANDO_POOL_LIST_COUNT; list++) {
		PoolSink sink = {pools->items[list], &pools->count[list], NULL};

		pools->count[list] = 0;
		walk_list(world, prng, (RandoPoolList)list, &sink);
	}
}

void rando_pool_add_items(const RandoWorld *world, RandoPrng *prng, RandoPoolList list, RandoItems *items)
{
	PoolSink sink = {NULL, NULL, items};

	walk_list(world, prng, list, &sink);
}

void rando_pool_append(RandoPools *pools, RandoPoolList list, RandoItemId item)
{
	assert(list < RANDO_POOL_LIST_COUNT);
	assert(pools->count[list] < RANDO_POOL_CAPACITY && "a pool list outgrew RANDO_POOL_CAPACITY");
	pools->items[list][pools->count[list]++] = (uint8_t)item;
}

int rando_pool_find(const RandoPools *pools, RandoPoolList list, RandoItemId item)
{
	int i;

	assert(list < RANDO_POOL_LIST_COUNT);
	for (i = 0; i < pools->count[list]; i++)
		if (pools->items[list][i] == item)
			return i;
	return -1;
}

bool rando_pool_remove_first(RandoPools *pools, RandoPoolList list, RandoItemId item)
{
	int i = rando_pool_find(pools, list, item);

	if (i < 0)
		return false;
	memmove(pools->items[list] + i, pools->items[list] + i + 1, (size_t)(pools->count[list] - i - 1));
	pools->count[list]--;
	return true;
}
