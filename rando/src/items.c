#include "rando_items.h"

#include <assert.h>
#include <string.h>

static const RandoItemInfo item_table[RANDO_ITEM_COUNT] = {
	{"Nothing", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"L1Sword", RANDO_ITEM_CLASS_SWORD, 0, RANDO_ITEM_NONE, false},
	{"L1SwordAndShield", RANDO_ITEM_CLASS_SWORD, 0, RANDO_ITEM_NONE, false},
	{"L2Sword", RANDO_ITEM_CLASS_SWORD, 0, RANDO_ITEM_NONE, false},
	{"MasterSword", RANDO_ITEM_CLASS_SWORD, 0, RANDO_ITEM_NONE, false},
	{"L3Sword", RANDO_ITEM_CLASS_SWORD, 0, RANDO_ITEM_NONE, false},
	{"L4Sword", RANDO_ITEM_CLASS_SWORD, 0, RANDO_ITEM_NONE, false},
	{"BlueShield", RANDO_ITEM_CLASS_SHIELD, 0, RANDO_ITEM_NONE, false},
	{"RedShield", RANDO_ITEM_CLASS_SHIELD, 0, RANDO_ITEM_NONE, false},
	{"MirrorShield", RANDO_ITEM_CLASS_SHIELD, 0, RANDO_ITEM_NONE, false},
	{"FireRod", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"IceRod", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Hammer", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Hookshot", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Bow", RANDO_ITEM_CLASS_BOW, 0, RANDO_ITEM_NONE, false},
	{"Boomerang", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Powder", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Bee", RANDO_ITEM_CLASS_BOTTLE_CONTENTS, 0, RANDO_ITEM_NONE, false},
	{"Bombos", RANDO_ITEM_CLASS_MEDALLION, 0, RANDO_ITEM_NONE, false},
	{"Ether", RANDO_ITEM_CLASS_MEDALLION, 0, RANDO_ITEM_NONE, false},
	{"Quake", RANDO_ITEM_CLASS_MEDALLION, 0, RANDO_ITEM_NONE, false},
	{"Lamp", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Shovel", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"OcarinaInactive", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"CaneOfSomaria", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Bottle", RANDO_ITEM_CLASS_BOTTLE, 0, RANDO_ITEM_NONE, false},
	{"PieceOfHeart", RANDO_ITEM_CLASS_HEALTH, 1, RANDO_ITEM_NONE, false},
	{"CaneOfByrna", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Cape", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"MagicMirror", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"PowerGlove", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"TitansMitt", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"BookOfMudora", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Flippers", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"MoonPearl", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Crystal", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"BugCatchingNet", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"BlueMail", RANDO_ITEM_CLASS_ARMOR, 0, RANDO_ITEM_NONE, false},
	{"RedMail", RANDO_ITEM_CLASS_ARMOR, 0, RANDO_ITEM_NONE, false},
	{"Key", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"Compass", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"HeartContainerNoAnimation", RANDO_ITEM_CLASS_HEALTH, 4, RANDO_ITEM_NONE, false},
	{"Bomb", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"ThreeBombs", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Mushroom", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"RedBoomerang", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"BottleWithRedPotion", RANDO_ITEM_CLASS_BOTTLE, 0, RANDO_ITEM_NONE, false},
	{"BottleWithGreenPotion", RANDO_ITEM_CLASS_BOTTLE, 0, RANDO_ITEM_NONE, false},
	{"BottleWithBluePotion", RANDO_ITEM_CLASS_BOTTLE, 0, RANDO_ITEM_NONE, false},
	{"RedPotion", RANDO_ITEM_CLASS_BOTTLE_CONTENTS, 0, RANDO_ITEM_NONE, false},
	{"GreenPotion", RANDO_ITEM_CLASS_BOTTLE_CONTENTS, 0, RANDO_ITEM_NONE, false},
	{"BluePotion", RANDO_ITEM_CLASS_BOTTLE_CONTENTS, 0, RANDO_ITEM_NONE, false},
	{"TenBombs", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"BigKey", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"Map", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"OneRupee", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"FiveRupees", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"TwentyRupees", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"PendantOfCourage", RANDO_ITEM_CLASS_PENDANT, 0, RANDO_ITEM_NONE, false},
	{"PendantOfWisdom", RANDO_ITEM_CLASS_PENDANT, 0, RANDO_ITEM_NONE, false},
	{"PendantOfPower", RANDO_ITEM_CLASS_PENDANT, 0, RANDO_ITEM_NONE, false},
	{"BowAndArrows", RANDO_ITEM_CLASS_BOW, 0, RANDO_ITEM_NONE, false},
	{"BowAndSilverArrows", RANDO_ITEM_CLASS_BOW, 0, RANDO_ITEM_NONE, false},
	{"BottleWithBee", RANDO_ITEM_CLASS_BOTTLE, 0, RANDO_ITEM_NONE, false},
	{"BottleWithFairy", RANDO_ITEM_CLASS_BOTTLE, 0, RANDO_ITEM_NONE, false},
	{"BossHeartContainer", RANDO_ITEM_CLASS_HEALTH, 4, RANDO_ITEM_NONE, false},
	{"HeartContainer", RANDO_ITEM_CLASS_HEALTH, 4, RANDO_ITEM_NONE, false},
	{"OneHundredRupees", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"FiftyRupees", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Heart", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Arrow", RANDO_ITEM_CLASS_ARROW, 0, RANDO_ITEM_NONE, false},
	{"TenArrows", RANDO_ITEM_CLASS_ARROW, 0, RANDO_ITEM_NONE, false},
	{"SmallMagic", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"ThreeHundredRupees", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"TwentyRupees2", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"BottleWithGoldBee", RANDO_ITEM_CLASS_BOTTLE, 0, RANDO_ITEM_NONE, false},
	{"OcarinaActive", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"PegasusBoots", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"BombUpgrade5", RANDO_ITEM_CLASS_UPGRADE_BOMB, 0, RANDO_ITEM_NONE, false},
	{"BombUpgrade10", RANDO_ITEM_CLASS_UPGRADE_BOMB, 0, RANDO_ITEM_NONE, false},
	{"BombUpgrade50", RANDO_ITEM_CLASS_UPGRADE_BOMB, 0, RANDO_ITEM_NONE, false},
	{"ArrowUpgrade5", RANDO_ITEM_CLASS_UPGRADE_ARROW, 0, RANDO_ITEM_NONE, false},
	{"ArrowUpgrade10", RANDO_ITEM_CLASS_UPGRADE_ARROW, 0, RANDO_ITEM_NONE, false},
	{"ArrowUpgrade70", RANDO_ITEM_CLASS_UPGRADE_ARROW, 0, RANDO_ITEM_NONE, false},
	{"HalfMagic", RANDO_ITEM_CLASS_UPGRADE_MAGIC, 0, RANDO_ITEM_NONE, false},
	{"QuarterMagic", RANDO_ITEM_CLASS_UPGRADE_MAGIC, 0, RANDO_ITEM_NONE, false},
	{"Programmable1", RANDO_ITEM_CLASS_PROGRAMMABLE, 0, RANDO_ITEM_NONE, false},
	{"Programmable2", RANDO_ITEM_CLASS_PROGRAMMABLE, 0, RANDO_ITEM_NONE, false},
	{"Programmable3", RANDO_ITEM_CLASS_PROGRAMMABLE, 0, RANDO_ITEM_NONE, false},
	{"SilverArrowUpgrade", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"Rupoor", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"RedClock", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"BlueClock", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"GreenClock", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"ProgressiveSword", RANDO_ITEM_CLASS_SWORD, 0, RANDO_ITEM_NONE, false},
	{"ProgressiveShield", RANDO_ITEM_CLASS_SHIELD, 0, RANDO_ITEM_NONE, false},
	{"ProgressiveArmor", RANDO_ITEM_CLASS_ARMOR, 0, RANDO_ITEM_NONE, false},
	{"ProgressiveGlove", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"singleRNG", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"multiRNG", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"ProgressiveBow", RANDO_ITEM_CLASS_BOW, 0, RANDO_ITEM_NONE, false},
	{"ProgressiveBowAlternate", RANDO_ITEM_CLASS_BOW, 0, RANDO_ITEM_NONE, false},
	{"Triforce", RANDO_ITEM_CLASS_EVENT, 0, RANDO_ITEM_NONE, false},
	{"PowerStar", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"TriforcePiece", RANDO_ITEM_CLASS_PLAIN, 0, RANDO_ITEM_NONE, false},
	{"MapLW", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapDW", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapA2", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapD7", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapD4", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapP3", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapD5", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapD3", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapD6", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapD1", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapD2", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapA1", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapP2", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapP1", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapH1", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"MapH2", RANDO_ITEM_CLASS_MAP, 0, RANDO_ITEM_NONE, false},
	{"CompassA2", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassD7", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassD4", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassP3", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassD5", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassD3", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassD6", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassD1", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassD2", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassA1", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassP2", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassP1", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassH1", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"CompassH2", RANDO_ITEM_CLASS_COMPASS, 0, RANDO_ITEM_NONE, false},
	{"BigKeyA2", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyD7", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyD4", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyP3", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyD5", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyD3", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyD6", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyD1", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyD2", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyA1", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyP2", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyP1", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyH1", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"BigKeyH2", RANDO_ITEM_CLASS_BIG_KEY, 0, RANDO_ITEM_NONE, false},
	{"KeyH2", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyH1", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyP1", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyP2", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyA1", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyD2", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyD1", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyD6", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyD3", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyD5", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyP3", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyD4", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyD7", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyA2", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"KeyGK", RANDO_ITEM_CLASS_KEY, 0, RANDO_ITEM_NONE, true},
	{"Crystal1", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"Crystal2", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"Crystal3", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"Crystal4", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"Crystal5", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"Crystal6", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"Crystal7", RANDO_ITEM_CLASS_CRYSTAL, 0, RANDO_ITEM_NONE, false},
	{"RescueZelda", RANDO_ITEM_CLASS_EVENT, 0, RANDO_ITEM_NONE, false},
	{"DefeatAgahnim", RANDO_ITEM_CLASS_EVENT, 0, RANDO_ITEM_NONE, false},
	{"BigRedBomb", RANDO_ITEM_CLASS_EVENT, 0, RANDO_ITEM_NONE, false},
	{"DefeatAgahnim2", RANDO_ITEM_CLASS_EVENT, 0, RANDO_ITEM_NONE, false},
	{"DefeatGanon", RANDO_ITEM_CLASS_EVENT, 0, RANDO_ITEM_NONE, false},
	{"UncleSword", RANDO_ITEM_CLASS_ALIAS, 0, RANDO_ITEM_ProgressiveSword, false},
	{"ShopKey", RANDO_ITEM_CLASS_ALIAS, 0, RANDO_ITEM_KeyGK, false},
	{"ShopArrow", RANDO_ITEM_CLASS_ALIAS, 0, RANDO_ITEM_Arrow, false},
};

bool rando_dungeon_item_confined(const RandoConfig *c, RandoItemClass item_class)
{
	return (!c->region_wild_keys && item_class == RANDO_ITEM_CLASS_KEY)
		|| (!c->region_wild_big_keys && item_class == RANDO_ITEM_CLASS_BIG_KEY)
		|| (!c->region_wild_maps && item_class == RANDO_ITEM_CLASS_MAP)
		|| (!c->region_wild_compasses && item_class == RANDO_ITEM_CLASS_COMPASS);
}

const RandoItemInfo *rando_item_info(RandoItemId id)
{
	assert(id < RANDO_ITEM_COUNT && "rando_item_info: not an item id");
	return &item_table[id];
}

RandoItemId rando_item_find(const char *name)
{
	int i;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (strcmp(item_table[i].name, name) == 0)
			return (RandoItemId)i;
	return RANDO_ITEM_NONE;
}

void rando_items_clear(RandoItems *items)
{
	memset(items, 0, sizeof *items);
}

void rando_items_all(RandoItems *items)
{
	int i;

	rando_items_clear(items);
	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		rando_items_add(items, (RandoItemId)i);
	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (item_table[i].key_prefixed)
			items->counts[i] = 10;
}

void rando_items_add(RandoItems *items, RandoItemId id)
{
	assert(id < RANDO_ITEM_COUNT && "rando_items_add: not an item id");
	items->counts[id]++;
	items->values[id]++;
}

void rando_items_remove(RandoItems *items, RandoItemId id)
{
	assert(id < RANDO_ITEM_COUNT && "rando_items_remove: not an item id");
	if (items->counts[id] == 0)
		return;
	items->counts[id]--;
	if (items->values[id] > 0)
		items->values[id]--;
}

void rando_items_unset(RandoItems *items, RandoItemId id)
{
	assert(id < RANDO_ITEM_COUNT && "rando_items_unset: not an item id");
	items->counts[id] = 0;
}

void rando_items_merge(RandoItems *out, const RandoItems *a, const RandoItems *b)
{
	int i;

	for (i = 0; i < RANDO_ITEM_COUNT; i++) {
		uint16_t add = b->counts[i];

		out->counts[i] = (uint16_t)(a->counts[i] + add);
		out->values[i] = (uint16_t)(a->values[i] + add);
	}
}

void rando_items_diff(RandoItems *out, const RandoItems *a, const RandoItems *b)
{
	int i;

	for (i = 0; i < RANDO_ITEM_COUNT; i++) {
		out->values[i] = a->values[i];
		out->counts[i] = b->counts[i] < a->counts[i] ? (uint16_t)(a->counts[i] - b->counts[i]) : 0;
	}
}

bool rando_items_has(const RandoItems *items, RandoItemId id)
{
	return rando_items_has_n(items, id, 1);
}

bool rando_items_has_n(const RandoItems *items, RandoItemId id, int at_least)
{
	assert(id < RANDO_ITEM_COUNT && "rando_items_has_n: not an item id");
	if (at_least == 0)
		return true;
	if (at_least == RANDO_CONFIG_NULL)
		return false;
	if (items->counts[RANDO_ITEM_ShopKey] > 0 && item_table[id].key_prefixed)
		return true;
	return items->counts[id] >= at_least;
}

int rando_items_count(const RandoItems *items)
{
	int i, n = 0;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		n += items->counts[i];
	return n;
}

static bool has(const RandoItems *items, RandoItemId id)
{
	return rando_items_has_n(items, id, 1);
}

static bool has_n(const RandoItems *items, RandoItemId id, int at_least)
{
	return rando_items_has_n(items, id, at_least);
}

static bool enemy_health_default(const RandoConfig *config)
{
	return config->enemizer_enemy_health == RANDO_ENEMY_HEALTH_DEFAULT;
}

static int values_of_class(const RandoItems *items, RandoItemClass item_class)
{
	int i, n = 0;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (item_table[i].item_class == item_class)
			n += items->values[i];
	return n;
}

bool rando_items_can_lift_rocks(const RandoItems *items)
{
	return has(items, RANDO_ITEM_PowerGlove)
		|| has(items, RANDO_ITEM_ProgressiveGlove)
		|| has(items, RANDO_ITEM_TitansMitt);
}

bool rando_items_can_lift_dark_rocks(const RandoItems *items)
{
	return has(items, RANDO_ITEM_TitansMitt)
		|| has_n(items, RANDO_ITEM_ProgressiveGlove, 2);
}

bool rando_items_can_light_torches(const RandoItems *items)
{
	return has(items, RANDO_ITEM_FireRod) || has(items, RANDO_ITEM_Lamp);
}

bool rando_items_can_melt_things(const RandoItems *items, const RandoConfig *config)
{
	return has(items, RANDO_ITEM_FireRod)
		|| (has(items, RANDO_ITEM_Bombos)
		    && (config->mode_weapons == RANDO_WEAPONS_SWORDLESS || rando_items_has_sword(items, 1)));
}

static bool can_activate_ocarina(const RandoItems *items, const RandoConfig *config)
{
	if (config->mode_state == RANDO_STATE_INVERTED) {
		return has(items, RANDO_ITEM_MoonPearl)
			&& (has(items, RANDO_ITEM_DefeatAgahnim)
			    || (((has(items, RANDO_ITEM_Hammer) && rando_items_can_lift_rocks(items))
				 || rando_items_can_lift_dark_rocks(items))));
	}
	return true;
}

bool rando_items_can_fly(const RandoItems *items, const RandoConfig *config)
{
	return has(items, RANDO_ITEM_OcarinaActive)
		|| (has(items, RANDO_ITEM_OcarinaInactive) && can_activate_ocarina(items, config));
}

bool rando_items_can_spin_speed(const RandoItems *items)
{
	return has(items, RANDO_ITEM_PegasusBoots)
		&& (rando_items_has_sword(items, 1) || has(items, RANDO_ITEM_Hookshot));
}

bool rando_items_can_acquire_fairy(const RandoItems *items, const RandoConfig *config)
{
	(void)items;
	if (config != NULL && !config->rom_catchable_fairies)
		return false;
	return true;
}

bool rando_items_can_bunny_revive(const RandoItems *items, const RandoConfig *config)
{
	bool can_bunny_revive = rando_items_has_a_bottle(items) && has(items, RANDO_ITEM_BugCatchingNet);

	if (config != NULL)
		can_bunny_revive = can_bunny_revive && rando_items_can_acquire_fairy(items, config);
	return can_bunny_revive;
}

bool rando_items_can_shoot_arrows(const RandoItems *items, const RandoConfig *config, int min_level)
{
	switch (min_level) {
	case 2:
		return has(items, RANDO_ITEM_BowAndSilverArrows)
			|| (has_n(items, RANDO_ITEM_ProgressiveBow, 2)
			    && (!config->rom_rupee_bow || has(items, RANDO_ITEM_ShopArrow)))
			|| (has(items, RANDO_ITEM_SilverArrowUpgrade)
			    && (has(items, RANDO_ITEM_Bow) || has(items, RANDO_ITEM_BowAndArrows)
				|| has(items, RANDO_ITEM_ProgressiveBow)));
	case 1:
	default:
		return ((has(items, RANDO_ITEM_Bow) || has(items, RANDO_ITEM_ProgressiveBow))
			&& (!config->rom_rupee_bow
			    || has(items, RANDO_ITEM_ShopArrow) || has(items, RANDO_ITEM_SilverArrowUpgrade)))
			|| has(items, RANDO_ITEM_BowAndArrows)
			|| has(items, RANDO_ITEM_BowAndSilverArrows);
	}
}

bool rando_items_can_block_lasers(const RandoItems *items)
{
	return has(items, RANDO_ITEM_MirrorShield)
		|| has_n(items, RANDO_ITEM_ProgressiveShield, 3);
}

bool rando_items_can_extend_magic(const RandoItems *items, const RandoConfig *config, int bars)
{
	long fill = 0x80, base_magic;

	if (config != NULL) {
		fill = config->rom_bottle_fill_magic;
		if (fill > 0x80)
			fill = 0x80;
	}
	base_magic = has(items, RANDO_ITEM_QuarterMagic) ? 4 : (has(items, RANDO_ITEM_HalfMagic) ? 2 : 1);
	return 0x80 * base_magic + base_magic * rando_items_bottle_count(items) * fill >= 0x80L * bars;
}

bool rando_items_glitched_link_in_dark_world(const RandoItems *items)
{
	return has(items, RANDO_ITEM_MoonPearl)
		|| rando_items_has_a_bottle(items);
}

bool rando_items_has_health(const RandoItems *items, int minimum_hearts)
{
	int i, quarters = 0;

	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (item_table[i].item_class == RANDO_ITEM_CLASS_HEALTH)
			quarters += items->values[i] * item_table[i].power_quarters;
	return quarters >= 4 * minimum_hearts;
}

int rando_items_heart_count(const RandoItems *items, int initial_quarters)
{
	/* VT compares getName(), "PieceOfHeart:<world id>", with 'PieceOfHeart': a Piece of Heart counts
	 * a full heart here, unlike in hasHealth. */
	return initial_quarters + 4 * values_of_class(items, RANDO_ITEM_CLASS_HEALTH);
}

bool rando_items_can_kill_escape_things(const RandoItems *items, const RandoConfig *config)
{
	return has(items, RANDO_ITEM_UncleSword)
		|| has(items, RANDO_ITEM_CaneOfSomaria)
		|| (has(items, RANDO_ITEM_TenBombs) && enemy_health_default(config))
		|| (has(items, RANDO_ITEM_CaneOfByrna) && enemy_health_default(config))
		|| rando_items_can_shoot_arrows(items, config, 1)
		|| has(items, RANDO_ITEM_Hammer)
		|| has(items, RANDO_ITEM_FireRod)
		|| config->ignore_can_kill_escape_things;
}

bool rando_items_can_kill_most_things(const RandoItems *items, const RandoConfig *config, int enemies)
{
	return rando_items_has_sword(items, 1)
		|| has(items, RANDO_ITEM_CaneOfSomaria)
		|| (rando_items_can_bomb_things(items) && enemies < 6 && enemy_health_default(config))
		|| (has(items, RANDO_ITEM_CaneOfByrna) && (enemies < 6 || rando_items_can_extend_magic(items, NULL, 2))
		    && enemy_health_default(config))
		|| rando_items_can_shoot_arrows(items, config, 1)
		|| has(items, RANDO_ITEM_Hammer)
		|| has(items, RANDO_ITEM_FireRod);
}

bool rando_items_can_bomb_things(const RandoItems *items)
{
	(void)items;
	return true;
}

bool rando_items_can_get_good_bee(const RandoItems *items)
{
	return has(items, RANDO_ITEM_BugCatchingNet)
		&& rando_items_has_a_bottle(items)
		&& (has(items, RANDO_ITEM_PegasusBoots)
		    || (rando_items_has_sword(items, 1) && has(items, RANDO_ITEM_Quake)));
}

bool rando_items_has_sword(const RandoItems *items, int min_level)
{
	switch (min_level) {
	case 4:
		return has_n(items, RANDO_ITEM_ProgressiveSword, 4)
			|| (has(items, RANDO_ITEM_UncleSword) && has_n(items, RANDO_ITEM_ProgressiveSword, 3))
			|| has(items, RANDO_ITEM_L4Sword);
	case 3:
		return has_n(items, RANDO_ITEM_ProgressiveSword, 3)
			|| (has(items, RANDO_ITEM_UncleSword) && has_n(items, RANDO_ITEM_ProgressiveSword, 2))
			|| has(items, RANDO_ITEM_L3Sword)
			|| has(items, RANDO_ITEM_L4Sword);
	case 2:
		return has_n(items, RANDO_ITEM_ProgressiveSword, 2)
			|| (has(items, RANDO_ITEM_UncleSword) && has(items, RANDO_ITEM_ProgressiveSword))
			|| has(items, RANDO_ITEM_L2Sword)
			|| has(items, RANDO_ITEM_MasterSword)
			|| has(items, RANDO_ITEM_L3Sword)
			|| has(items, RANDO_ITEM_L4Sword);
	case 1:
	default:
		return has(items, RANDO_ITEM_ProgressiveSword)
			|| has(items, RANDO_ITEM_UncleSword)
			|| has(items, RANDO_ITEM_L1Sword)
			|| has(items, RANDO_ITEM_L1SwordAndShield)
			|| has(items, RANDO_ITEM_L2Sword)
			|| has(items, RANDO_ITEM_MasterSword)
			|| has(items, RANDO_ITEM_L3Sword)
			|| has(items, RANDO_ITEM_L4Sword);
	}
}

bool rando_items_has_armor(const RandoItems *items, int min_level)
{
	switch (min_level) {
	case 2:
		return has_n(items, RANDO_ITEM_ProgressiveArmor, 2)
			|| has(items, RANDO_ITEM_RedMail);
	case 1:
	default:
		return has(items, RANDO_ITEM_ProgressiveArmor)
			|| has(items, RANDO_ITEM_BlueMail)
			|| has(items, RANDO_ITEM_RedMail);
	}
}

bool rando_items_has_bottle(const RandoItems *items, int at_least)
{
	return rando_items_bottle_count(items) >= at_least;
}

int rando_items_bottle_count(const RandoItems *items)
{
	return values_of_class(items, RANDO_ITEM_CLASS_BOTTLE);
}

bool rando_items_has_a_bottle(const RandoItems *items)
{
	return has(items, RANDO_ITEM_BottleWithBee)
		|| has(items, RANDO_ITEM_BottleWithFairy)
		|| has(items, RANDO_ITEM_BottleWithRedPotion)
		|| has(items, RANDO_ITEM_BottleWithGreenPotion)
		|| has(items, RANDO_ITEM_BottleWithBluePotion)
		|| has(items, RANDO_ITEM_Bottle)
		|| has(items, RANDO_ITEM_BottleWithGoldBee);
}
