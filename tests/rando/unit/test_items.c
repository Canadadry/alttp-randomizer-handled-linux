#include "rando_items.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_items"

static const char *const class_names[RANDO_ITEM_CLASS_COUNT] = {
	[RANDO_ITEM_CLASS_PLAIN] = "Item",
	[RANDO_ITEM_CLASS_SWORD] = "Item\\Sword",
	[RANDO_ITEM_CLASS_SHIELD] = "Item\\Shield",
	[RANDO_ITEM_CLASS_BOW] = "Item\\Bow",
	[RANDO_ITEM_CLASS_BOTTLE_CONTENTS] = "Item\\BottleContents",
	[RANDO_ITEM_CLASS_MEDALLION] = "Item\\Medallion",
	[RANDO_ITEM_CLASS_BOTTLE] = "Item\\Bottle",
	[RANDO_ITEM_CLASS_HEALTH] = "Item\\Upgrade\\Health",
	[RANDO_ITEM_CLASS_CRYSTAL] = "Item\\Crystal",
	[RANDO_ITEM_CLASS_ARMOR] = "Item\\Armor",
	[RANDO_ITEM_CLASS_KEY] = "Item\\Key",
	[RANDO_ITEM_CLASS_COMPASS] = "Item\\Compass",
	[RANDO_ITEM_CLASS_BIG_KEY] = "Item\\BigKey",
	[RANDO_ITEM_CLASS_MAP] = "Item\\Map",
	[RANDO_ITEM_CLASS_PENDANT] = "Item\\Pendant",
	[RANDO_ITEM_CLASS_ARROW] = "Item\\Arrow",
	[RANDO_ITEM_CLASS_UPGRADE_BOMB] = "Item\\Upgrade\\Bomb",
	[RANDO_ITEM_CLASS_UPGRADE_ARROW] = "Item\\Upgrade\\Arrow",
	[RANDO_ITEM_CLASS_UPGRADE_MAGIC] = "Item\\Upgrade\\Magic",
	[RANDO_ITEM_CLASS_PROGRAMMABLE] = "Item\\Programmable",
	[RANDO_ITEM_CLASS_EVENT] = "Item\\Event",
	[RANDO_ITEM_CLASS_ALIAS] = "ItemAlias",
};

static void check_item_record(const Fixture *fx, int id)
{
	const RandoItemInfo *info;
	const char *target;

	if (id >= RANDO_ITEM_COUNT)
		fixture_fail(fx, "more items in the dump than in the C table");
	info = rando_item_info((RandoItemId)id);
	if (strcmp(info->name, fx->field[1]) != 0)
		fixture_fail(fx, "item name or order differs");
	if (rando_item_find(fx->field[1]) != (RandoItemId)id)
		fixture_fail(fx, "rando_item_find does not return the item's id");
	if (strcmp(class_names[info->item_class], fx->field[2]) != 0)
		fixture_fail(fx, "item class differs");
	if (info->power_quarters != atoi(fx->field[3]))
		fixture_fail(fx, "heart power differs");
	target = info->alias_target == RANDO_ITEM_NONE ? "-" : rando_item_info(info->alias_target)->name;
	if (strcmp(target, fx->field[4]) != 0)
		fixture_fail(fx, "alias target differs");
}

static void check_views_equal(const Fixture *fx, const uint16_t *want, const uint16_t *got)
{
	int i;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (want[i] != got[i]) {
			fprintf(stderr, "item %s: VT %u, C %u\n", rando_item_info((RandoItemId)i)->name,
				want[i], got[i]);
			fixture_fail(fx, "view differs");
		}
}

/* Returns the number of records checked. */
static int check_world_dump(void)
{
	Fixture fx;
	RandoItems all;
	int items = 0, records = 0;

	rando_items_all(&all);
	fixture_open(&fx, FIXTURE_WORLD_DUMP);
	while (fixture_next(&fx)) {
		if (strcmp(fx.field[0], "item") == 0) {
			check_item_record(&fx, items++);
		} else if (strcmp(fx.field[0], "all_items") == 0) {
			RandoItems want;
			int counts = strcmp(fx.field[1], "counts") == 0;

			fixture_parse_view(&fx, fx.field[2], counts ? want.counts : want.values,
					   counts ? all.counts : all.values);
			check_views_equal(&fx, counts ? want.counts : want.values, counts ? all.counts : all.values);
		} else {
			continue;
		}
		records++;
	}
	fixture_close(&fx);
	if (items != RANDO_ITEM_COUNT)
		test_fail(TEST, "the dump has %d items, the C table %d", items, RANDO_ITEM_COUNT);
	if (rando_item_find("NoSuchItem") != RANDO_ITEM_NONE)
		test_fail(TEST, "rando_item_find of an unknown name is not RANDO_ITEM_NONE");
	return records;
}

#define COLUMN(fn, call) \
	static int fn(const RandoItems *i, const RandoConfig *c) \
	{ \
		(void)i; \
		(void)c; \
		return call; \
	}

COLUMN(col0, rando_items_can_lift_rocks(i))
COLUMN(col1, rando_items_can_lift_dark_rocks(i))
COLUMN(col2, rando_items_can_light_torches(i))
COLUMN(col3, rando_items_can_melt_things(i, c))
COLUMN(col4, rando_items_can_fly(i, c))
COLUMN(col5, rando_items_can_spin_speed(i))
COLUMN(col6, rando_items_can_acquire_fairy(i, NULL))
COLUMN(col7, rando_items_can_acquire_fairy(i, c))
COLUMN(col8, rando_items_can_bunny_revive(i, NULL))
COLUMN(col9, rando_items_can_bunny_revive(i, c))
COLUMN(col10, rando_items_can_shoot_arrows(i, c, 1))
COLUMN(col11, rando_items_can_shoot_arrows(i, c, 2))
COLUMN(col12, rando_items_can_block_lasers(i))
COLUMN(col13, rando_items_can_extend_magic(i, NULL, 2))
COLUMN(col14, rando_items_can_extend_magic(i, NULL, 3))
COLUMN(col15, rando_items_can_extend_magic(i, c, 1))
COLUMN(col16, rando_items_can_extend_magic(i, c, 2))
COLUMN(col17, rando_items_can_extend_magic(i, c, 3))
COLUMN(col18, rando_items_can_extend_magic(i, c, 4))
COLUMN(col19, rando_items_glitched_link_in_dark_world(i))
COLUMN(col20, rando_items_has_health(i, 7))
COLUMN(col21, rando_items_has_health(i, 12))
COLUMN(col22, rando_items_heart_count(i, 12))
COLUMN(col23, rando_items_can_kill_escape_things(i, c))
COLUMN(col24, rando_items_can_kill_most_things(i, c, 5))
COLUMN(col25, rando_items_can_kill_most_things(i, c, 8))
COLUMN(col26, rando_items_can_bomb_things(i))
COLUMN(col27, rando_items_can_get_good_bee(i))
COLUMN(col28, rando_items_has_sword(i, 1))
COLUMN(col29, rando_items_has_sword(i, 2))
COLUMN(col30, rando_items_has_sword(i, 3))
COLUMN(col31, rando_items_has_sword(i, 4))
COLUMN(col32, rando_items_has_armor(i, 1))
COLUMN(col33, rando_items_has_armor(i, 2))
COLUMN(col34, rando_items_has_bottle(i, 1))
COLUMN(col35, rando_items_has_bottle(i, 2))
COLUMN(col36, rando_items_bottle_count(i))
COLUMN(col37, rando_items_has_a_bottle(i))
COLUMN(col38, rando_items_count(i))
COLUMN(col39, rando_items_has(i, RANDO_ITEM_KeyH2))
COLUMN(col40, rando_items_has_n(i, RANDO_ITEM_KeyP1, 2))
COLUMN(col41, rando_items_has(i, RANDO_ITEM_BigKeyP1))
COLUMN(col42, rando_items_has_n(i, RANDO_ITEM_ProgressiveSword, 2))
COLUMN(col43, rando_items_has_n(i, RANDO_ITEM_ProgressiveSword, 0))

static const PredicateColumn columns[] = {
	{"canLiftRocks", col0},
	{"canLiftDarkRocks", col1},
	{"canLightTorches", col2},
	{"canMeltThings(w)", col3},
	{"canFly(w)", col4},
	{"canSpinSpeed", col5},
	{"canAcquireFairy", col6},
	{"canAcquireFairy(w)", col7},
	{"canBunnyRevive", col8},
	{"canBunnyRevive(w)", col9},
	{"canShootArrows(w,1)", col10},
	{"canShootArrows(w,2)", col11},
	{"canBlockLasers", col12},
	{"canExtendMagic", col13},
	{"canExtendMagic(null,3)", col14},
	{"canExtendMagic(w,1)", col15},
	{"canExtendMagic(w,2)", col16},
	{"canExtendMagic(w,3)", col17},
	{"canExtendMagic(w,4)", col18},
	{"glitchedLinkInDarkWorld", col19},
	{"hasHealth(7)", col20},
	{"hasHealth(12)", col21},
	{"heartCount*4", col22},
	{"canKillEscapeThings(w)", col23},
	{"canKillMostThings(w)", col24},
	{"canKillMostThings(w,8)", col25},
	{"canBombThings", col26},
	{"canGetGoodBee", col27},
	{"hasSword(1)", col28},
	{"hasSword(2)", col29},
	{"hasSword(3)", col30},
	{"hasSword(4)", col31},
	{"hasArmor(1)", col32},
	{"hasArmor(2)", col33},
	{"hasBottle(1)", col34},
	{"hasBottle(2)", col35},
	{"bottleCount", col36},
	{"hasABottle", col37},
	{"count", col38},
	{"has(KeyH2)", col39},
	{"has(KeyP1,2)", col40},
	{"has(BigKeyP1)", col41},
	{"has(ProgressiveSword,2)", col42},
	{"has(ProgressiveSword,0)", col43},
};

static void check_predicate_vectors(void)
{
	long values;
	int records = fixture_replay_predicates(TEST, columns, (int)(sizeof columns / sizeof columns[0]), &values);

	printf(TEST ": %d predicate records, %ld values checked\n", records, values);
}

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void items_of(RandoItems *items, const RandoItemId *ids, int n)
{
	int k;

	rando_items_clear(items);
	for (k = 0; k < n; k++)
		rando_items_add(items, ids[k]);
}

/* What VT's World constructor sets for item.functionality; NULL is a World built with no config. */
static RandoConfig functionality(const char *setting)
{
	RandoConfig c;

	rando_config_init(&c);
	if (setting == NULL || strcmp(setting, "easy") == 0 || strcmp(setting, "normal") == 0)
		return c;
	c.rom_catchable_fairies = false;
	if (strcmp(setting, "hard") == 0)
		c.rom_bottle_fill_magic = 0x40;
	else if (strcmp(setting, "expert") == 0)
		c.rom_bottle_fill_magic = 0x20;
	else if (strcmp(setting, "superexpert") == 0)
		c.rom_bottle_fill_magic = 0x00;
	else
		test_fail(TEST, "unknown item.functionality %s", setting);
	return c;
}

/* ItemCollectionTest::testCount*, testFilter*, testValues*. */
static void check_counts_and_values(void)
{
	static const RandoItemId sword_rupee[] = {RANDO_ITEM_L1Sword, RANDO_ITEM_OneRupee};
	static const RandoItemId same[] = {RANDO_ITEM_L1Sword, RANDO_ITEM_L1Sword, RANDO_ITEM_Map};
	RandoItems items;

	items_of(&items, sword_rupee, 1);
	CHECK(rando_items_count(&items) == 1);
	items_of(&items, sword_rupee, 2);
	CHECK(rando_items_count(&items) == 2);
	items_of(&items, same, 2);
	CHECK(rando_items_count(&items) == 2);
	items_of(&items, same, 3);
	CHECK(items.values[RANDO_ITEM_L1Sword] == 2 && items.values[RANDO_ITEM_Map] == 1);
	CHECK(rando_items_count(&items) == 3);
}

/* ItemCollectionTest::testCanBunnyRevive. */
static void check_can_bunny_revive(void)
{
	static const struct {
		RandoItemId items[2];
		int n;
		const char *setting;
		bool want;
	} rows[] = {
		{{RANDO_ITEM_NONE}, 0, "normal", false},
		{{RANDO_ITEM_Bottle}, 1, "normal", false},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_BugCatchingNet}, 2, "normal", true},
		{{RANDO_ITEM_BottleWithFairy, RANDO_ITEM_BugCatchingNet}, 2, "normal", true},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_BugCatchingNet}, 2, "hard", false},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_BugCatchingNet}, 2, "expert", false},
	};
	size_t r;

	for (r = 0; r < sizeof rows / sizeof rows[0]; r++) {
		RandoConfig c = functionality(rows[r].setting);
		RandoItems items;

		items_of(&items, rows[r].items, rows[r].n);
		if (rando_items_can_bunny_revive(&items, &c) != rows[r].want)
			test_fail(TEST, "canBunnyRevive row %zu", r);
	}
}

/* ItemCollectionTest::testCanAcquireFairy. */
static void check_can_acquire_fairy(void)
{
	static const struct {
		const char *setting;
		bool want;
	} rows[] = {{"easy", true}, {"normal", true}, {"hard", false}, {"expert", false}, {NULL, true}};
	RandoItems items;
	size_t r;

	rando_items_clear(&items);
	for (r = 0; r < sizeof rows / sizeof rows[0]; r++) {
		RandoConfig c = functionality(rows[r].setting);

		if (rando_items_can_acquire_fairy(&items, &c) != rows[r].want)
			test_fail(TEST, "canAcquireFairy row %zu", r);
	}
}

/* ItemCollectionTest::testCanExtendMagic. */
static void check_can_extend_magic(void)
{
	static const struct {
		RandoItemId items[5];
		int n;
		const char *setting;
		int bars;
		bool want;
	} rows[] = {
		{{RANDO_ITEM_NONE}, 0, "normal", 1, true},
		{{RANDO_ITEM_NONE}, 0, "superexpert", 2, false},
		{{RANDO_ITEM_Bottle}, 1, "normal", 2, true},
		{{RANDO_ITEM_Bottle}, 1, "normal", 3, false},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_HalfMagic}, 2, "normal", 4, true},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_HalfMagic}, 2, "normal", 5, false},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_HalfMagic}, 2, "hard", 3, true},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_HalfMagic}, 2, "hard", 4, false},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_QuarterMagic},
		 5, "expert", 8, true},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_QuarterMagic},
		 5, "expert", 9, false},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_QuarterMagic},
		 5, "superexpert", 4, true},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_QuarterMagic},
		 5, "superexpert", 5, false},
	};
	size_t r;

	for (r = 0; r < sizeof rows / sizeof rows[0]; r++) {
		RandoConfig c = functionality(rows[r].setting);
		RandoItems items;

		items_of(&items, rows[r].items, rows[r].n);
		if (rando_items_can_extend_magic(&items, &c, rows[r].bars) != rows[r].want)
			test_fail(TEST, "canExtendMagic row %zu", r);
	}
}

/* ItemCollectionTest::testBottleCounting. */
static void check_bottle_counting(void)
{
	static const struct {
		RandoItemId items[4];
		int n;
	} rows[] = {
		{{RANDO_ITEM_Bottle}, 1},
		{{RANDO_ITEM_BottleWithBee}, 1},
		{{RANDO_ITEM_BottleWithFairy}, 1},
		{{RANDO_ITEM_BottleWithRedPotion}, 1},
		{{RANDO_ITEM_BottleWithGreenPotion}, 1},
		{{RANDO_ITEM_BottleWithBluePotion}, 1},
		{{RANDO_ITEM_BottleWithGoldBee}, 1},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle}, 2},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle}, 3},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle}, 4},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_BottleWithBee}, 4},
		{{RANDO_ITEM_Bottle, RANDO_ITEM_Bottle, RANDO_ITEM_BottleWithBee, RANDO_ITEM_BottleWithBee}, 4},
	};
	size_t r;

	for (r = 0; r < sizeof rows / sizeof rows[0]; r++) {
		RandoItems items;

		items_of(&items, rows[r].items, rows[r].n);
		if (!rando_items_has_bottle(&items, rows[r].n) || rando_items_has_bottle(&items, rows[r].n + 1))
			test_fail(TEST, "bottle counting row %zu", r);
	}
}

/* ItemCollection::has: n == 0, then a null n is false, then a ShopKey makes every Key* name true. */
static void check_has(void)
{
	RandoItems items;

	rando_items_clear(&items);
	CHECK(rando_items_has_n(&items, RANDO_ITEM_Hammer, 0));
	CHECK(!rando_items_has(&items, RANDO_ITEM_KeyH2));
	CHECK(!rando_items_has_n(&items, RANDO_ITEM_TriforcePiece, RANDO_CONFIG_NULL));
	rando_items_add(&items, RANDO_ITEM_ShopKey);
	CHECK(!rando_items_has_n(&items, RANDO_ITEM_KeyH2, RANDO_CONFIG_NULL));
	CHECK(rando_items_has_n(&items, RANDO_ITEM_KeyH2, 5));
	CHECK(rando_items_has(&items, RANDO_ITEM_Key));
	CHECK(!rando_items_has(&items, RANDO_ITEM_BigKeyH2));
	CHECK(rando_items_has(&items, RANDO_ITEM_KeyGK));
}

/* removeItem, offsetUnset, merge and diff on both views. */
static void check_operations(void)
{
	RandoItems a, b, out;

	rando_items_clear(&a);
	rando_items_add(&a, RANDO_ITEM_Bottle);
	rando_items_add(&a, RANDO_ITEM_Bottle);
	rando_items_remove(&a, RANDO_ITEM_Bottle);
	CHECK(a.counts[RANDO_ITEM_Bottle] == 1 && a.values[RANDO_ITEM_Bottle] == 1);

	rando_items_unset(&a, RANDO_ITEM_Bottle);
	CHECK(a.counts[RANDO_ITEM_Bottle] == 0 && a.values[RANDO_ITEM_Bottle] == 1);
	CHECK(!rando_items_has_a_bottle(&a) && rando_items_has_bottle(&a, 1));
	rando_items_remove(&a, RANDO_ITEM_Bottle);
	CHECK(a.values[RANDO_ITEM_Bottle] == 1);

	rando_items_clear(&b);
	rando_items_add(&b, RANDO_ITEM_Hammer);
	rando_items_add(&b, RANDO_ITEM_Hammer);
	rando_items_unset(&b, RANDO_ITEM_Hammer);
	rando_items_add(&b, RANDO_ITEM_Lamp);
	b.counts[RANDO_ITEM_Lamp] = 3;
	rando_items_merge(&out, &a, &b);
	CHECK(out.counts[RANDO_ITEM_Lamp] == 3 && out.values[RANDO_ITEM_Lamp] == 3);
	CHECK(out.counts[RANDO_ITEM_Hammer] == 0 && out.values[RANDO_ITEM_Hammer] == 0);
	CHECK(out.values[RANDO_ITEM_Bottle] == 1);
	rando_items_merge(&a, &a, &b);
	CHECK(memcmp(&a, &out, sizeof a) == 0);

	rando_items_clear(&a);
	rando_items_add(&a, RANDO_ITEM_Lamp);
	rando_items_add(&a, RANDO_ITEM_Lamp);
	rando_items_add(&a, RANDO_ITEM_Lamp);
	rando_items_add(&a, RANDO_ITEM_Hammer);
	rando_items_clear(&b);
	rando_items_add(&b, RANDO_ITEM_Lamp);
	rando_items_add(&b, RANDO_ITEM_Hammer);
	rando_items_add(&b, RANDO_ITEM_Hammer);
	rando_items_diff(&out, &a, &b);
	CHECK(out.counts[RANDO_ITEM_Lamp] == 2 && out.values[RANDO_ITEM_Lamp] == 3);
	CHECK(out.counts[RANDO_ITEM_Hammer] == 0 && out.values[RANDO_ITEM_Hammer] == 1);
	rando_items_diff(&a, &a, &b);
	CHECK(memcmp(&a, &out, sizeof a) == 0);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	check_counts_and_values();
	check_can_bunny_revive();
	check_can_acquire_fairy();
	check_can_extend_magic();
	check_bottle_counting();
	check_has();
	check_operations();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	printf(TEST ": %d world dump records checked\n", check_world_dump());
	check_predicate_vectors();
	return 0;
}
