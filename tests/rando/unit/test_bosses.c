#include "rando_bosses.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_bosses"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define CAN_BEAT(fn, boss) \
	static int fn(const RandoItems *i, const RandoConfig *c) \
	{ \
		return rando_boss_can_beat(boss, c, i); \
	}

CAN_BEAT(armos_knights, RANDO_BOSS_ARMOS_KNIGHTS)
CAN_BEAT(lanmolas, RANDO_BOSS_LANMOLAS)
CAN_BEAT(moldorm, RANDO_BOSS_MOLDORM)
CAN_BEAT(agahnim, RANDO_BOSS_AGAHNIM)
CAN_BEAT(helmasaur_king, RANDO_BOSS_HELMASAUR_KING)
CAN_BEAT(arrghus, RANDO_BOSS_ARRGHUS)
CAN_BEAT(mothula, RANDO_BOSS_MOTHULA)
CAN_BEAT(blind, RANDO_BOSS_BLIND)
CAN_BEAT(kholdstare, RANDO_BOSS_KHOLDSTARE)
CAN_BEAT(vitreous, RANDO_BOSS_VITREOUS)
CAN_BEAT(trinexx, RANDO_BOSS_TRINEXX)
CAN_BEAT(agahnim2, RANDO_BOSS_AGAHNIM2)

static const PredicateColumn columns[] = {
	{"canBeat:Armos Knights", armos_knights},
	{"canBeat:Lanmolas", lanmolas},
	{"canBeat:Moldorm", moldorm},
	{"canBeat:Agahnim", agahnim},
	{"canBeat:Helmasaur King", helmasaur_king},
	{"canBeat:Arrghus", arrghus},
	{"canBeat:Mothula", mothula},
	{"canBeat:Blind", blind},
	{"canBeat:Kholdstare", kholdstare},
	{"canBeat:Vitreous", vitreous},
	{"canBeat:Trinexx", trinexx},
	{"canBeat:Agahnim2", agahnim2},
};

static void items_of(RandoItems *items, const RandoItemId *ids)
{
	rando_items_clear(items);
	for (; *ids != RANDO_ITEM_NONE; ids++)
		rando_items_add(items, *ids);
}

/* One case per boss, restating Boss::all's closures. */
static void check_can_beat(void)
{
	static const struct {
		RandoBossId boss;
		RandoItemId yes[4];
		RandoItemId no[4];
	} rows[] = {
		{RANDO_BOSS_ARMOS_KNIGHTS, {RANDO_ITEM_Boomerang, RANDO_ITEM_NONE}, {RANDO_ITEM_FireRod, RANDO_ITEM_NONE}},
		{RANDO_BOSS_LANMOLAS, {RANDO_ITEM_IceRod, RANDO_ITEM_NONE}, {RANDO_ITEM_Boomerang, RANDO_ITEM_NONE}},
		{RANDO_BOSS_MOLDORM, {RANDO_ITEM_Hammer, RANDO_ITEM_NONE}, {RANDO_ITEM_Bow, RANDO_ITEM_NONE}},
		{RANDO_BOSS_AGAHNIM, {RANDO_ITEM_BugCatchingNet, RANDO_ITEM_NONE}, {RANDO_ITEM_FireRod, RANDO_ITEM_NONE}},
		{RANDO_BOSS_HELMASAUR_KING, {RANDO_ITEM_L1Sword, RANDO_ITEM_NONE}, {RANDO_ITEM_Hammer, RANDO_ITEM_NONE}},
		{RANDO_BOSS_ARRGHUS, {RANDO_ITEM_Hookshot, RANDO_ITEM_Hammer, RANDO_ITEM_NONE},
		 {RANDO_ITEM_Hammer, RANDO_ITEM_NONE}},
		{RANDO_BOSS_MOTHULA, {RANDO_ITEM_Hammer, RANDO_ITEM_NONE}, {RANDO_ITEM_IceRod, RANDO_ITEM_NONE}},
		{RANDO_BOSS_BLIND, {RANDO_ITEM_CaneOfByrna, RANDO_ITEM_NONE}, {RANDO_ITEM_FireRod, RANDO_ITEM_NONE}},
		{RANDO_BOSS_KHOLDSTARE, {RANDO_ITEM_FireRod, RANDO_ITEM_Hammer, RANDO_ITEM_NONE},
		 {RANDO_ITEM_Bombos, RANDO_ITEM_Hammer, RANDO_ITEM_NONE}},
		{RANDO_BOSS_VITREOUS, {RANDO_ITEM_BowAndArrows, RANDO_ITEM_NONE}, {RANDO_ITEM_FireRod, RANDO_ITEM_NONE}},
		{RANDO_BOSS_TRINEXX, {RANDO_ITEM_FireRod, RANDO_ITEM_IceRod, RANDO_ITEM_Hammer, RANDO_ITEM_NONE},
		 {RANDO_ITEM_FireRod, RANDO_ITEM_IceRod, RANDO_ITEM_L1Sword, RANDO_ITEM_NONE}},
		{RANDO_BOSS_AGAHNIM2, {RANDO_ITEM_L1Sword, RANDO_ITEM_NONE}, {RANDO_ITEM_Bow, RANDO_ITEM_NONE}},
	};
	RandoConfig config;
	RandoItems items;
	size_t r;

	rando_config_init(&config);
	for (r = 0; r < sizeof rows / sizeof rows[0]; r++) {
		items_of(&items, rows[r].yes);
		if (!rando_boss_can_beat(rows[r].boss, &config, &items))
			test_fail(TEST, "%s should be beaten", rando_boss_name(rows[r].boss));
		items_of(&items, rows[r].no);
		if (rando_boss_can_beat(rows[r].boss, &config, &items))
			test_fail(TEST, "%s should not be beaten", rando_boss_name(rows[r].boss));
	}
	config.item_placement = RANDO_PLACEMENT_BASIC;
	items_of(&items, rows[RANDO_BOSS_HELMASAUR_KING].yes);
	CHECK(!rando_boss_can_beat(RANDO_BOSS_HELMASAUR_KING, &config, &items));
}

/* Trinexx's canExtendMagic(world, 4) with a level-1 sword, which no predicate vector separates from 3 bars. */
static void check_trinexx_magic(void)
{
	static const RandoItemId three_bars[] = {RANDO_ITEM_FireRod, RANDO_ITEM_IceRod, RANDO_ITEM_L1Sword,
						 RANDO_ITEM_HalfMagic, RANDO_ITEM_Bottle, RANDO_ITEM_NONE};
	RandoConfig config;
	RandoItems items;

	rando_config_init(&config);
	config.rom_bottle_fill_magic = 0x40;
	items_of(&items, three_bars);
	CHECK(!rando_boss_can_beat(RANDO_BOSS_TRINEXX, &config, &items));
	rando_items_add(&items, RANDO_ITEM_Bottle);
	CHECK(rando_boss_can_beat(RANDO_BOSS_TRINEXX, &config, &items));
}

/* Boss::canBeat of `new Boss("Dummy Boss")`, which VT's Inverted ThievesTown tests set. */
static void check_ruleless_boss_beaten_with_nothing(void)
{
	RandoConfig config;
	RandoItems items;

	rando_config_init(&config);
	rando_items_clear(&items);
	CHECK(rando_boss_can_beat(RANDO_BOSS_RULELESS, &config, &items));
}

/* Returns the number of boss records checked. */
static int check_world_dump(void)
{
	Fixture fx;
	int bosses = 0;

	fixture_open(&fx, FIXTURE_WORLD_DUMP);
	while (fixture_next(&fx)) {
		if (strcmp(fx.field[0], "boss") != 0)
			continue;
		if (bosses == RANDO_BOSS_COUNT || strcmp(rando_boss_name((RandoBossId)bosses), fx.field[1]) != 0)
			fixture_fail(&fx, "boss name or order differs");
		if (rando_boss_find(fx.field[1]) != (RandoBossId)bosses)
			fixture_fail(&fx, "rando_boss_find does not return the boss's id");
		bosses++;
	}
	fixture_close(&fx);
	if (bosses != RANDO_BOSS_COUNT)
		test_fail(TEST, "the dump has %d bosses, the C table %d", bosses, RANDO_BOSS_COUNT);
	if (rando_boss_find("Ganon") != RANDO_BOSS_NONE)
		test_fail(TEST, "rando_boss_find of an unknown name is not RANDO_BOSS_NONE");
	return bosses;
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	long values;
	int records;

	check_can_beat();
	check_trinexx_magic();
	check_ruleless_boss_beaten_with_nothing();
	printf(TEST ": hand-written cases passed\n");
	if (!fixtures)
		return 0;
	printf(TEST ": %d world dump records checked\n", check_world_dump());
	records = fixture_replay_predicates(TEST, columns, (int)(sizeof columns / sizeof columns[0]), &values);
	printf(TEST ": %d predicate records, %ld canBeat values checked\n", records, values);
	return 0;
}
