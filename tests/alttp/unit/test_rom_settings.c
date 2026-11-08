#include "rando.h"
#include "rando_rom.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_rom_settings"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define WRITES_MAX 2048
#define POOL_MAX (64 * 1024)

typedef struct Write {
	uint32_t address;
	size_t count;
	size_t at;
	const char *method;
} Write;

/* Only the settings writes: those whose tag belongs to no other group of write_groups.txt. */
typedef struct Recorder {
	Write writes[WRITES_MAX];
	int n;
	uint8_t pool[POOL_MAX];
	size_t used;
	/* The settings write that returns false, counted from 1; 0 accepts every call. */
	int refuse_at;
	int calls;
} Recorder;

static bool other_group(const char *method)
{
	static const char *const tags[] = {"writeItem",	       "writeCredits",	    "writeText",
					   "writeInitialSram", "setTotalItemCount", "setHeartBeepSpeed"};
	size_t i;

	for (i = 0; i < sizeof tags / sizeof tags[0]; i++)
		if (strcmp(method, tags[i]) == 0)
			return true;
	return false;
}

static bool record(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	Recorder *r = ctx;

	if (other_group(method))
		return true;
	r->calls++;
	if (r->n < WRITES_MAX && r->used + count <= POOL_MAX) {
		r->writes[r->n].address = address;
		r->writes[r->n].count = count;
		r->writes[r->n].at = r->used;
		r->writes[r->n].method = method;
		memcpy(r->pool + r->used, bytes, count);
		r->used += count;
		r->n++;
	}
	return r->refuse_at == 0 || r->calls < r->refuse_at;
}

static RandoGen gen, before;
static Recorder rec;

static const RandoItemId pendants[3] = {RANDO_ITEM_PendantOfCourage, RANDO_ITEM_PendantOfWisdom,
					RANDO_ITEM_PendantOfPower};
static const RandoItemId crystals[7] = {RANDO_ITEM_Crystal1, RANDO_ITEM_Crystal2, RANDO_ITEM_Crystal3,
					RANDO_ITEM_Crystal4, RANDO_ITEM_Crystal5, RANDO_ITEM_Crystal6,
					RANDO_ITEM_Crystal7};
static const RandoLocationId pendant_prizes[3] = {RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_LOC_DESERT_PALACE_PRIZE,
						 RANDO_LOC_TOWER_OF_HERA_PRIZE};
static const RandoLocationId crystal_prizes[7] = {
	RANDO_LOC_PALACE_OF_DARKNESS_PRIZE, RANDO_LOC_SWAMP_PALACE_PRIZE, RANDO_LOC_SKULL_WOODS_PRIZE,
	RANDO_LOC_THIEVES_TOWN_PRIZE,	    RANDO_LOC_ICE_PALACE_PRIZE,	  RANDO_LOC_MISERY_MIRE_PRIZE,
	RANDO_LOC_TURTLE_ROCK_PRIZE,
};

/* A world of the variant with only its prizes placed (VT's writeItem refuses an empty prize), the prize
 * music drawn, which the items group reads under rom.mapOnPickup, setShops' default shops active and
 * every drop a Heart. */
static void build_variant(RandoGen *g, RandoVariant variant, void (*tweak)(RandoConfig *))
{
	RandoPrizeMusic *music = &g->post.prize_music;
	RandoConfig config;
	int i;

	memset(g, 0, sizeof *g);
	rando_config_init(&config);
	if (tweak)
		tweak(&config);
	rando_world_init(&g->world, variant, &config);
	rando_postfill_clear(&g->post);
	for (i = 0; i < 3; i++) {
		CHECK(rando_world_set_item(&g->world, pendant_prizes[i], pendants[i]));
		music->locations[music->count] = pendant_prizes[i];
		music->music[music->count++] = 0x11;
	}
	for (i = 0; i < 7; i++) {
		CHECK(rando_world_set_item(&g->world, crystal_prizes[i], crystals[i]));
		music->locations[music->count] = crystal_prizes[i];
		music->music[music->count++] = 0x16;
	}
	music->locations[music->count] = RANDO_LOC_AGAHNIM_2;
	music->music[music->count++] = 0x16;
	for (i = 0; i < RANDO_SHOP_COUNT; i++)
		if (rando_shop_info((RandoShopId)i)->kind != RANDO_SHOP_KIND_TAKE_ANY)
			rando_world_shop_set_active(&g->world, (RandoShopId)i, true);
	for (i = 0; i < RANDO_PRIZE_PACK_COUNT; i++) {
		int j;

		for (j = 0; j < rando_prize_pack_slots((RandoPrizePack)i); j++)
			rando_world_set_drop(&g->world, (RandoPrizePack)i, j, RANDO_DROP_HEART);
	}
}

static void build(RandoGen *g, void (*tweak)(RandoConfig *))
{
	build_variant(g, RANDO_VARIANT_OPEN, tweak);
}

static void run(void)
{
	RandoRomWriter writer = {record, &rec};

	memset(&rec, 0, sizeof rec);
	CHECK(rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
}

/* The index of the only settings write at address, or -1 when there is none. */
static int find(uint32_t address)
{
	int i, at = -1;

	for (i = 0; i < rec.n; i++)
		if (rec.writes[i].address == address) {
			CHECK(at < 0);
			at = i;
		}
	return at;
}

/* The only write at address is bytes[0..count), tagged method. */
static void check_write(uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	int i = find(address);

	CHECK(i >= 0);
	if (i < 0)
		return;
	CHECK(rec.writes[i].count == count);
	CHECK(strcmp(rec.writes[i].method, method) == 0);
	if (rec.writes[i].count == count)
		CHECK(memcmp(rec.pool + rec.writes[i].at, bytes, count) == 0);
}

static void check_byte(uint32_t address, uint8_t byte, const char *method)
{
	check_write(address, &byte, 1, method);
}

typedef struct Expected {
	uint32_t address;
	size_t count;
	const char *method;
} Expected;

static const Expected open_order[] = {
	{0x180167, 2, "setGoalRequiredCount"},
	{0x180165, 2, "setGoalIcon"},
	{0x18016B, 3, "setCaneOfByrnaSpikeCaveUsage"},
	{0x18016E, 3, "setCapeSpikeCaveUsage"},
	{0x180195, 1, "setByrnaCaveSpikeDamage"},
	{0x045C42, 3, "writeToRom"},
	{0x03ADA7, 3, "setCapeRegularMagicUsage"},
	{0x18004F, 1, "setCaneOfByrnaInvulnerability"},
	{0x036DD0, 1, "setPowderedSpriteFairyPrize"},
	{0x180084, 2, "setBottleFills"},
	{0x034FD6, 1, "setCatchableFairies"},
	{0x0F5D73, 1, "setCatchableBees"},
	{0x0F5F10, 1, "setCatchableBees"},
	{0x180180, 1, "setStunItems"},
	{0x180181, 1, "setSilversOnlyAtGanon"},
	{0x180036, 2, "setRupoorValue"},
	{0x180086, 1, "setGanonAgahnimRng"},
	{0x18019A, 1, "setTowerCrystalRequirement"},
	{0x1801A6, 1, "setGanonCrystalRequirement"},
	{0x180172, 1, "setGenericKeys"},
	{0x184800, 80, "setupCustomShops"},
	{0x184900, 240, "setupCustomShops"},
	{0x030052, 1, "setRupeeArrow"},
	{0x0301FC, 1, "setRupeeArrow"},
	{0x0ECB4E, 4, "setRupeeArrow"},
	{0x0F0D96, 4, "setRupeeArrow"},
	{0x180175, 1, "setRupeeArrow"},
	{0x180176, 2, "setRupeeArrow"},
	{0x180178, 2, "setRupeeArrow"},
	{0x00EDA5, 2, "setRupeeArrow"},
	{0x00E9AE, 2, "setWishingWellChests"},
	{0x00E9CF, 2, "setWishingWellChests"},
	{0x01F714, 205, "setWishingWellChests"},
	{0x0348DB, 1, "setWishingWellUpgrade"},
	{0x0348EB, 1, "setWishingWellUpgrade"},
	{0x01F810, 6, "setHyliaFairyShop"},
	{0x18017E, 1, "setRestrictFairyPonds"},
	{0x180090, 2, "setLimitProgressiveSword"},
	{0x180092, 2, "setLimitProgressiveShield"},
	{0x180094, 2, "setLimitProgressiveArmor"},
	{0x180096, 2, "setLimitBottle"},
	{0x180098, 2, "setLimitProgressiveBow"},
	{0x180182, 1, "setSilversEquip"},
	{0x184000, 28, "setSubstitutions"},
	{0x1801A8, 1, "setGanonInvincible"},
	{0x18003B, 1, "setMapMode"},
	{0x18003C, 1, "setCompassMode"},
	{0x187000, 16, "setCompassCountTotals"},
	{0x18016A, 1, "setFreeItemTextMode"},
	{0x180045, 1, "setFreeItemMenu"},
	{0x180020, 1, "setDiggingGameRng"},
	{0x0EFD95, 1, "setDiggingGameRng"},
	{0x178000, 1024, "writeRNGBlock"},
	{0x037A78, 56, "writePrizePacksToRom"},
	{0x0EFBD4, 3, "writePrizePacksToRom"},
	{0x0329C8, 1, "writePrizePacksToRom"},
	{0x0329C4, 1, "writePrizePacksToRom"},
	{0x037993, 1, "writePrizePacksToRom"},
	{0x0E82CC, 1, "writePrizePacksToRom"},
	{0x01FC16, 6, "setPyramidFairyChests"},
	{0x180029, 1, "setSmithyQuickItemGive"},
	{0x180174, 1, "setGameState"},
	{0x180038, 1, "setGameState"},
	{0x18003F, 1, "setSwordlessMode"},
	{0x180041, 1, "setSwordlessMode"},
	{0x180044, 1, "setSwordlessMode"},
	{0x18005D, 1, "setSwordlessMode"},
	{0x180169, 1, "setLockAgahnimDoorInEscape"},
	{0x06D263, 6, "removeUnclesSword"},
	{0x06D26B, 6, "removeUnclesSword"},
	{0x06D293, 6, "removeUnclesSword"},
	{0x06D29B, 6, "removeUnclesSword"},
	{0x06D2B3, 6, "removeUnclesSword"},
	{0x06D2BB, 6, "removeUnclesSword"},
	{0x06D2E3, 6, "removeUnclesSword"},
	{0x06D2EB, 6, "removeUnclesSword"},
	{0x06D31B, 6, "removeUnclesSword"},
	{0x06D323, 6, "removeUnclesSword"},
	{0x06D253, 6, "removeUnclesShield"},
	{0x06D25B, 6, "removeUnclesShield"},
	{0x06D283, 6, "removeUnclesShield"},
	{0x06D28B, 6, "removeUnclesShield"},
	{0x06D2CB, 6, "removeUnclesShield"},
	{0x06D2FB, 6, "removeUnclesShield"},
	{0x06D313, 6, "removeUnclesShield"},
	{0x186FFF, 1, "setBallNChainDungeon"},
	{0x180080, 4, "setCapacityUpgradeFills"},
	{0x180190, 3, "setClockMode"},
	{0x180204, 4, "setBlueClock"},
	{0x180200, 4, "setRedClock"},
	{0x180208, 4, "setGreenClock"},
	{0x180042, 1, "setSaveAndQuitFromBossRoom"},
	{0x1800A3, 1, "setWorldOnAgahnimDeath"},
	{0x180358, 1, "setAllowAccidentalMajorGlitch"},
	{0x1800A4, 1, "setSQEGFix"},
	{0x0159A8, 1, "setZeldaMirrorFix"},
	{0x180039, 1, "enableHudItemCounter"},
	{0x180211, 1, "setGameType"},
	{0x18008E, 1, "setPseudoBoots"},
	{0x187032, 1, "enableFastRom"},
};

static void check_order(const Expected *expected, int n)
{
	int i;

	CHECK(rec.n == n);
	for (i = 0; i < n && i < rec.n; i++) {
		if (rec.writes[i].address != expected[i].address || rec.writes[i].count != expected[i].count
		    || strcmp(rec.writes[i].method, expected[i].method) != 0)
			test_fail(TEST, "write %d: %06X %zu %s, expected %06X %zu %s", i, (unsigned)rec.writes[i].address,
				  rec.writes[i].count, rec.writes[i].method, (unsigned)expected[i].address,
				  expected[i].count, expected[i].method);
	}
}

static void check_open_order(void)
{
	build(&gen, NULL);
	run();
	check_order(open_order, (int)(sizeof open_order / sizeof open_order[0]));
}

static RandoGoal goal;
static int crystal_count;

static void set_goal(RandoConfig *config)
{
	config->goal = goal;
}

/* VT's goal switch: Ganon's vulnerability, triforce-hunt's turn-in before it, the count and the HUD. */
static void check_goals(void)
{
	static const struct {
		RandoGoal goal;
		uint8_t invincible;
		uint16_t required;
		uint8_t hud;
	} cases[] = {
		{RANDO_GOAL_GANON, 0x04, 0, 0x00},	    {RANDO_GOAL_FAST_GANON, 0x04, 0, 0x00},
		{RANDO_GOAL_DUNGEONS, 0x02, 0, 0x00},	    {RANDO_GOAL_PEDESTAL, 0x01, 0, 0x00},
		{RANDO_GOAL_TRIFORCE_HUNT, 0x01, 20, 0x00}, {RANDO_GOAL_GANONHUNT, 0x05, 40, 0x00},
		{RANDO_GOAL_COMPLETIONIST, 0x0B, 0, 0x01},
	};
	size_t i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		uint8_t required[2];

		goal = cases[i].goal;
		build(&gen, set_goal);
		run();
		required[0] = (uint8_t)(cases[i].required & 0xFF);
		required[1] = (uint8_t)(cases[i].required >> 8);
		check_write(0x180167, required, 2, "setGoalRequiredCount");
		check_byte(0x1801A8, cases[i].invincible, "setGanonInvincible");
		check_byte(0x180039, cases[i].hud, "enableHudItemCounter");
		if (goal == RANDO_GOAL_TRIFORCE_HUNT) {
			check_byte(0x180194, 0x01, "enableTriforceTurnIn");
			CHECK(find(0x1801A8) == find(0x180194) + 1);
		} else {
			CHECK(find(0x180194) < 0);
		}
	}
}

/* A goal piece count turns the completionist HUD counter off. */
static void required_completionist(RandoConfig *config)
{
	config->goal = RANDO_GOAL_COMPLETIONIST;
	config->item_goal_required = 3;
}

static void check_hud_required(void)
{
	static const uint8_t three[2] = {0x03, 0x00};

	build(&gen, required_completionist);
	run();
	check_write(0x180167, three, 2, "setGoalRequiredCount");
	check_byte(0x180039, 0x00, "enableHudItemCounter");
}

static void set_crystals(RandoConfig *config)
{
	config->crystals_tower = crystal_count;
	config->crystals_ganon = crystal_count;
}

/* VT's max(min($crystals, 7), 0). */
static void check_crystals(void)
{
	static const struct {
		int config;
		uint8_t written;
	} cases[] = {{0, 0}, {3, 3}, {7, 7}, {8, 7}, {200, 7}, {-1, 0}};
	size_t i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		crystal_count = cases[i].config;
		build(&gen, set_crystals);
		run();
		check_byte(0x18019A, cases[i].written, "setTowerCrystalRequirement");
		check_byte(0x1801A6, cases[i].written, "setGanonCrystalRequirement");
	}
}

static RandoItemFunctionality functionality;

static void set_functionality(RandoConfig *config)
{
	config->item_functionality = functionality;
}

/* Rows 8 to 15 from a world built with item_functionality normal, hard and expert. */
static void check_functionality(void)
{
	static const struct {
		RandoItemFunctionality functionality;
		uint8_t cape[3], byrna, powder, bottle[2], fairies, stun, silvers;
	} cases[] = {
		{RANDO_ITEM_FUNCTIONALITY_NORMAL, {0x04, 0x08, 0x10}, 0x01, 0xE3, {0xA0, 0x80}, 0xF0, 0x03, 0x00},
		{RANDO_ITEM_FUNCTIONALITY_HARD, {0x02, 0x04, 0x08}, 0x00, 0xD8, {0x38, 0x40}, 0x80, 0x02, 0x01},
		{RANDO_ITEM_FUNCTIONALITY_EXPERT, {0x02, 0x04, 0x08}, 0x00, 0xD8, {0x20, 0x20}, 0x80, 0x00, 0x01},
	};
	size_t i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		functionality = cases[i].functionality;
		build(&gen, set_functionality);
		run();
		check_write(0x3ADA7, cases[i].cape, 3, "setCapeRegularMagicUsage");
		check_byte(0x18004F, cases[i].byrna, "setCaneOfByrnaInvulnerability");
		check_byte(0x36DD0, cases[i].powder, "setPowderedSpriteFairyPrize");
		check_write(0x180084, cases[i].bottle, 2, "setBottleFills");
		check_byte(0x34FD6, cases[i].fairies, "setCatchableFairies");
		check_byte(0xF5D73, 0xF0, "setCatchableBees");
		check_byte(0xF5F10, 0xF0, "setCatchableBees");
		check_byte(0x180180, cases[i].stun, "setStunItems");
		check_byte(0x180181, cases[i].silvers, "setSilversOnlyAtGanon");
	}
}

static void hard_pool(RandoConfig *config)
{
	config->item_pool = RANDO_ITEM_POOL_HARD;
}

static void check_substitutions(uint8_t silvers, uint8_t boss_hearts, uint8_t pieces)
{
	uint8_t expected[28] = {0x12, 0x01, 0x35, 0xFF, 0x51, 0x06, 0x52, 0xFF, 0x53, 0x06, 0x54, 0xFF, 0x58, 0x01,
				silvers, 0xFF, 0x3E, boss_hearts, 0x47, 0xFF, 0x17, pieces, 0x47, 0xFF, 0xFF, 0xFF,
				0xFF, 0xFF};

	check_write(0x184000, expected, sizeof expected, "setSubstitutions");
}

/* Each limit is the overflow count then TwentyRupees2's 0x47; hard: Sword 3, Shield 2, Armor 0, Bow 1. */
static void check_pools(void)
{
	static const uint8_t normal[5][2] = {{4, 0x47}, {3, 0x47}, {2, 0x47}, {4, 0x47}, {2, 0x47}};
	static const uint8_t hard[5][2] = {{3, 0x47}, {2, 0x47}, {0, 0x47}, {4, 0x47}, {1, 0x47}};
	static const uint32_t addresses[5] = {0x180090, 0x180092, 0x180094, 0x180096, 0x180098};
	static const char *const methods[5] = {"setLimitProgressiveSword", "setLimitProgressiveShield",
					       "setLimitProgressiveArmor", "setLimitBottle", "setLimitProgressiveBow"};
	int i;

	build(&gen, NULL);
	run();
	for (i = 0; i < 5; i++)
		check_write(addresses[i], normal[i], 2, methods[i]);
	check_substitutions(0x43, 10, 24);

	build(&gen, hard_pool);
	run();
	for (i = 0; i < 5; i++)
		check_write(addresses[i], hard[i], 2, methods[i]);
	check_substitutions(0x43, 6, 16);
}

/* Retro: rom.rupeeBow's silver arrows substitution and arrow writes, and rom.genericKeys. */
static void check_rupee_bow(void)
{
	static const uint8_t thief[4] = {0xA9, 0x00, 0xEA, 0xEA}, chest_game[2] = {0x35, 0x41};
	static const uint8_t wood[2] = {0x0A, 0x00}, silver[2] = {0x32, 0x00};

	build_variant(&gen, RANDO_VARIANT_RETRO, NULL);
	run();
	check_substitutions(0x36, 10, 24);
	check_byte(0x180172, 0x01, "setGenericKeys");
	check_byte(0x30052, 0xDB, "setRupeeArrow");
	check_byte(0x301FC, 0xDA, "setRupeeArrow");
	check_write(0xECB4E, thief, 4, "setRupeeArrow");
	check_write(0xF0D96, thief, 4, "setRupeeArrow");
	check_byte(0x180175, 0x01, "setRupeeArrow");
	check_write(0x180176, wood, 2, "setRupeeArrow");
	check_write(0x180178, silver, 2, "setRupeeArrow");
	check_write(0xEDA5, chest_game, 2, "setRupeeArrow");
	check_byte(0x180038, 0x00, "setGameState");
}

static RandoDungeonItems dungeon_items;
static RandoLogic logic;

static void set_dungeon_items(RandoConfig *config)
{
	config->dungeon_items = dungeon_items;
	config->logic = logic;
}

/* Map mode, compass mode and the free-item bits; major glitches adds 0x10 to the menu. */
static void check_dungeon_items(void)
{
	static const struct {
		RandoDungeonItems items;
		RandoLogic logic;
		uint8_t map, compass, text, menu;
	} cases[] = {
		{RANDO_DUNGEON_ITEMS_STANDARD, RANDO_LOGIC_NO_GLITCHES, 0x00, 0x00, 0x00, 0x00},
		{RANDO_DUNGEON_ITEMS_MC, RANDO_LOGIC_NO_GLITCHES, 0x01, 0x01, 0x16, 0x0C},
		{RANDO_DUNGEON_ITEMS_FULL, RANDO_LOGIC_NO_GLITCHES, 0x01, 0x01, 0x1F, 0x0F},
		{RANDO_DUNGEON_ITEMS_FULL, RANDO_LOGIC_MAJOR_GLITCHES, 0x01, 0x01, 0x1F, 0x1F},
	};
	static const uint8_t totals[16] = {0x08, 0x08, 0x06, 0x06, 0x02, 0x0A, 0x0E, 0x08,
					   0x08, 0x08, 0x06, 0x08, 0x0C, 0x1B, 0x00, 0x00};
	size_t i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		dungeon_items = cases[i].items;
		logic = cases[i].logic;
		build(&gen, set_dungeon_items);
		run();
		check_byte(0x18003B, cases[i].map, "setMapMode");
		check_byte(0x18003C, cases[i].compass, "setCompassMode");
		check_write(0x187000, totals, sizeof totals, "setCompassCountTotals");
		check_byte(0x18016A, cases[i].text, "setFreeItemTextMode");
		check_byte(0x180045, cases[i].menu, "setFreeItemMenu");
	}
}

static void set_logic(RandoConfig *config)
{
	config->logic = logic;
}

/* VT's logic switch, in its order, and setLockAgahnimDoorInEscape before the clocks. */
static void check_logic(void)
{
	static const Expected major[9] = {
		{0x1800A1, 1, "setSwampWaterLevel"},	      {0x1800A2, 1, "setPreAgahnimDarkWorldDeathInDungeon"},
		{0x180042, 1, "setSaveAndQuitFromBossRoom"},   {0x1800A3, 1, "setWorldOnAgahnimDeath"},
		{0x180210, 1, "setRandomizerSeedType"},	      {0x180212, 1, "setWarningFlags"},
		{0x180358, 1, "setAllowAccidentalMajorGlitch"}, {0x1800A4, 1, "setSQEGFix"},
		{0x0159A8, 1, "setZeldaMirrorFix"},
	};
	static const uint8_t major_bytes[9] = {0x00, 0x00, 0x01, 0x00, 0x01, 0x60, 0x01, 0x00, 0x02};
	static const uint8_t overworld_bytes[9] = {0, 0x00, 0x01, 0x00, 0x02, 0x40, 0x01, 0x00, 0x02};
	static const uint8_t none_bytes[9] = {0, 0, 0x01, 0x01, 0, 0, 0x00, 0x01, 0x04};
	static const struct {
		RandoLogic logic;
		const uint8_t *bytes;
		int first;
		uint8_t lock;
	} cases[] = {
		{RANDO_LOGIC_HYBRID_MAJOR_GLITCHES, major_bytes, 0, 0x00},
		{RANDO_LOGIC_MAJOR_GLITCHES, major_bytes, 0, 0x00},
		{RANDO_LOGIC_NO_LOGIC, major_bytes, 0, 0x00},
		{RANDO_LOGIC_OVERWORLD_GLITCHES, overworld_bytes, 1, 0x00},
		{RANDO_LOGIC_NO_GLITCHES, none_bytes, 2, 0x01},
	};
	size_t i;
	int j, at;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		logic = cases[i].logic;
		build(&gen, set_logic);
		run();
		check_byte(0x180169, cases[i].lock, "setLockAgahnimDoorInEscape");
		CHECK(find(0x180169) < find(0x186FFF));
		at = find(0x180208) + 1;
		for (j = cases[i].first; j < 9; j++) {
			if (logic == RANDO_LOGIC_NO_GLITCHES && (j == 4 || j == 5))
				continue;
			CHECK(at < rec.n);
			if (at >= rec.n)
				return;
			CHECK(rec.writes[at].address == major[j].address);
			CHECK(strcmp(rec.writes[at].method, major[j].method) == 0);
			CHECK(rec.writes[at].count == 1 && rec.pool[rec.writes[at].at] == cases[i].bytes[j]);
			at++;
		}
		CHECK(rec.writes[at].address == 0x180039);
	}
}

static void swordless(RandoConfig *config)
{
	config->mode_weapons = RANDO_WEAPONS_SWORDLESS;
}

static void check_swordless(void)
{
	build(&gen, swordless);
	run();
	check_byte(0x18003F, 0x01, "setSwordlessMode");
	check_byte(0x180041, 0x01, "setSwordlessMode");
	check_byte(0x180044, 0x01, "setSwordlessMode");
	check_byte(0x18005D, 0x00, "setSwordlessMode");

	build(&gen, NULL);
	run();
	check_byte(0x18003F, 0x00, "setSwordlessMode");
	check_byte(0x180041, 0x00, "setSwordlessMode");
	check_byte(0x180044, 0x00, "setSwordlessMode");
}

static void swords_out_of_pool(RandoConfig *config)
{
	config->region_swords_in_pool = false;
}

static void check_swords_in_pool(void)
{
	static const uint8_t on[6] = {0xB1, 0xC6, 0xF9, 0xC9, 0xC6, 0xF9};
	static const uint8_t off[6] = {0xA8, 0xB8, 0x3D, 0xD0, 0xB8, 0x3D};

	build(&gen, NULL);
	run();
	check_write(0x1FC16, on, 6, "setPyramidFairyChests");
	check_byte(0x180029, 0x01, "setSmithyQuickItemGive");

	build(&gen, swords_out_of_pool);
	run();
	check_write(0x1FC16, off, 6, "setPyramidFairyChests");
	check_byte(0x180029, 0x00, "setSmithyQuickItemGive");
}

/* setFixFakeWorld(false), then the sewers lamp cone: on for standard only. */
static void check_game_state(void)
{
	build(&gen, NULL);
	run();
	check_byte(0x180174, 0x00, "setGameState");
	check_byte(0x180038, 0x00, "setGameState");
	CHECK(find(0x180038) == find(0x180174) + 1);

	build_variant(&gen, RANDO_VARIANT_STANDARD, NULL);
	run();
	check_byte(0x180174, 0x00, "setGameState");
	check_byte(0x180038, 0x01, "setGameState");
	CHECK(find(0x180038) == find(0x180174) + 1);
	check_byte(0x180169, 0x01, "setLockAgahnimDoorInEscape");
}

typedef struct StateWrite {
	uint32_t address;
	uint8_t bytes[4];
	size_t count;
} StateWrite;

/* setInvertedMode's first writes and its last one, PC addresses from VT's snes_to_pc. */
static const StateWrite inverted_head[] = {
	{0x180174, {0x00}, 1}, {0x180038, {0x00}, 1}, {0x18004A, {0x01}, 1}, {0x0103E0, {0xF0}, 1},
	{0x01334D, {0xF0}, 1}, {0x035B78, {0x8B}, 1}, {0x02AF79, {0xF0}, 1}, {0x06B3C5, {0xC6}, 1},
	{0x03A3F4, {0xF0}, 1}, {0x03A3F4, {0xF0}, 1},
};

static int state_write(int k)
{
	int i, seen = 0;

	for (i = 0; i < rec.n; i++)
		if (strcmp(rec.writes[i].method, "setGameState") == 0 && seen++ == k)
			return i;
	return -1;
}

static int state_writes(void)
{
	int k = 0;

	while (state_write(k) >= 0)
		k++;
	return k;
}

static void check_state_write(int k, uint32_t address, const uint8_t *bytes, size_t count)
{
	int i = state_write(k);

	CHECK(i >= 0);
	if (i < 0)
		return;
	CHECK(rec.writes[i].address == address);
	CHECK(rec.writes[i].count == count);
	if (rec.writes[i].count == count)
		CHECK(memcmp(rec.pool + rec.writes[i].at, bytes, count) == 0);
}

/* setFixFakeWorld(false), setOpenMode's lamp cone, then setInvertedMode's list with setFixFakeWorld(true)
 * after the morph poof; it locks the Agahnim door itself. */
static void check_inverted_state(void)
{
	static const uint8_t frog[3] = {0xF0, 0xE1, 0x05};
	static const uint8_t on = 0x01, poof = 0xD0, aga = 0x02;
	size_t k;
	int n, i, morph = -1, fake = -1;

	build_variant(&gen, RANDO_VARIANT_INVERTED, NULL);
	run();
	for (k = 0; k < sizeof inverted_head / sizeof inverted_head[0]; k++)
		check_state_write((int)k, inverted_head[k].address, inverted_head[k].bytes, inverted_head[k].count);
	n = state_writes();
	CHECK(n == 195);
	check_state_write(n - 1, 0x0332AB, frog, 3);
	for (k = 0; (int)k < n; k++) {
		i = state_write((int)k);
		if (rec.writes[i].address == 0x04540C)
			morph = (int)k;
		if (rec.writes[i].address == 0x180174 && k > 0)
			fake = (int)k;
	}
	CHECK(morph > 0 && fake == morph + 1);
	check_state_write(morph, 0x04540C, &poof, 1);
	check_state_write(fake, 0x180174, &on, 1);
	check_byte(0x180169, aga, "setGameState");
}

/* A shop as VT's region constructor and setShops leave it: its ROM bytes, then its records' item byte and
 * price. */
typedef struct VtShop {
	uint8_t config, keeper;
	uint16_t room;
	uint8_t door;
	int n;
	uint8_t items[3];
	uint16_t prices[3];
	uint8_t max;
} VtShop;

/* VT's Shops, in getShops order: Kakariko, LW Lake Hylia, Capacity Upgrade, LW Death Mountain, DW Death
 * Mountain, DW Potion, DW Forest, DW Lumberjack, DW Outcasts, DW Lake Hylia. */
static const VtShop open_shops[10] = {
	{0x03, 0xA0, 0x011F, 0x46, 3, {0x2E, 0x42, 0x31}, {150, 10, 50}, 0},
	{0x03, 0xA0, 0x0112, 0x58, 3, {0x2E, 0x42, 0x31}, {150, 10, 50}, 0},
	{0x12, 0x04, 0x0115, 0x5D, 2, {0x51, 0x53, 0}, {100, 100, 0}, 7},
	{0x43, 0xA0, 0x00FF, 0x00, 3, {0x2E, 0x42, 0x31}, {150, 10, 50}, 0},
	{0x03, 0xC1, 0x0112, 0x6E, 3, {0x2E, 0x42, 0x31}, {150, 10, 50}, 0},
	{0x03, 0xC1, 0x010F, 0x6F, 3, {0x2E, 0x04, 0x31}, {150, 50, 50}, 0},
	{0x03, 0xC1, 0x0110, 0x75, 3, {0x05, 0x0E, 0x44}, {500, 10, 30}, 0},
	{0x03, 0xC1, 0x010F, 0x57, 3, {0x2E, 0x04, 0x31}, {150, 50, 50}, 0},
	{0x03, 0xC1, 0x010F, 0x60, 3, {0x2E, 0x04, 0x31}, {150, 50, 50}, 0},
	{0x03, 0xC1, 0x010F, 0x74, 3, {0x2E, 0x04, 0x31}, {150, 50, 50}, 0},
};

/* setupCustomShops' two writes for shops: ids from 0 up, the last 0xFF, the SRAM offset growing by the
 * inventory size (1 for a take-any), then 8 0xFF after the records. */
static void check_shops(const VtShop *shops, const bool *take_any, int n)
{
	uint8_t shop_bytes[8 * 41], item_bytes[8 * 41 * 3 + 8];
	size_t ns = 0, ni = 0;
	int i, j, sram = 0;

	for (i = 0; i < n; i++) {
		uint8_t id = (uint8_t)(i == n - 1 ? 0xFF : i);

		shop_bytes[ns++] = id;
		shop_bytes[ns++] = (uint8_t)(shops[i].room & 0xFF);
		shop_bytes[ns++] = (uint8_t)(shops[i].room >> 8);
		shop_bytes[ns++] = shops[i].door;
		shop_bytes[ns++] = 0x00;
		shop_bytes[ns++] = (uint8_t)((shops[i].config & 0xFC) + shops[i].n);
		shop_bytes[ns++] = shops[i].keeper;
		shop_bytes[ns++] = (uint8_t)sram;
		sram += take_any != NULL && take_any[i] ? 1 : shops[i].n;
		for (j = 0; j < shops[i].n; j++) {
			item_bytes[ni++] = id;
			item_bytes[ni++] = shops[i].items[j];
			item_bytes[ni++] = (uint8_t)(shops[i].prices[j] & 0xFF);
			item_bytes[ni++] = (uint8_t)(shops[i].prices[j] >> 8);
			item_bytes[ni++] = shops[i].max;
			item_bytes[ni++] = 0xFF;
			item_bytes[ni++] = 0x00;
			item_bytes[ni++] = 0x00;
		}
	}
	memset(item_bytes + ni, 0xFF, 8);
	ni += 8;
	check_write(0x184800, shop_bytes, ns, "setupCustomShops");
	check_write(0x184900, item_bytes, ni, "setupCustomShops");
	CHECK(find(0x184900) == find(0x184800) + 1);
}

static void check_open_shops(void)
{
	build(&gen, NULL);
	run();
	check_shops(open_shops, NULL, 10);
	CHECK(find(0x184800) == find(0x180172) + 1);
}

/* Retro's take-anys: each one's extra write first, in getShops order, then shopkeeper 0xE2 and one SRAM
 * slot each; ShopArrow and ShopKey written as Arrow's and KeyGK's bytes. */
static void check_take_any_shops(void)
{
	static VtShop shops[12];
	static bool take_any[12];
	static const uint8_t fairy[1] = {0x58};
	int n = 0, i;

	build_variant(&gen, RANDO_VARIANT_RETRO, NULL);
	rando_world_shop_set_active(&gen.world, RANDO_SHOP_LONG_FAIRY_CAVE, true);
	rando_world_shop_add(&gen.world, RANDO_SHOP_LONG_FAIRY_CAVE, 0, RANDO_ITEM_ProgressiveSword, 0);
	rando_world_shop_set_active(&gen.world, RANDO_SHOP_LAKE_HYLIA_FAIRY, true);
	rando_world_shop_add(&gen.world, RANDO_SHOP_LAKE_HYLIA_FAIRY, 0, RANDO_ITEM_BluePotion, 0);
	rando_world_shop_add(&gen.world, RANDO_SHOP_LAKE_HYLIA_FAIRY, 1, RANDO_ITEM_BossHeartContainer, 0);
	rando_world_shop_add(&gen.world, RANDO_SHOP_LIGHT_WORLD_KAKARIKO_SHOP, 0, RANDO_ITEM_ShopArrow, 80);
	rando_world_shop_add(&gen.world, RANDO_SHOP_LIGHT_WORLD_KAKARIKO_SHOP, 1, RANDO_ITEM_ShopKey, 100);
	run();

	shops[n] = (VtShop){0x83, 0xE2, 0x0112, 0x55, 1, {0x5E, 0, 0}, {0, 0, 0}, 0};
	take_any[n++] = true;
	shops[n] = (VtShop){0x83, 0xE2, 0x0112, 0x5E, 2, {0x30, 0x3E, 0}, {0, 0, 0}, 0};
	take_any[n++] = true;
	shops[n] = (VtShop){0x03, 0xA0, 0x011F, 0x46, 3, {0x43, 0xAF, 0x31}, {80, 100, 50}, 0};
	take_any[n++] = false;
	for (i = 1; i < 10; i++) {
		shops[n] = open_shops[i];
		take_any[n++] = false;
	}
	check_shops(shops, take_any, n);
	check_byte(0xDBBC7, 0x58, "setupCustomShops");
	check_write(0xDBBD0, fairy, 1, "setupCustomShops");
	CHECK(find(0xDBBD0) == find(0xDBBC7) + 1);
	CHECK(find(0x184800) == find(0xDBBD0) + 1);
}

/* Hard and expert pools clear the Capacity Upgrade, which stays active with nothing to sell. */
static void check_cleared_upgrade_shop(void)
{
	VtShop shops[10];

	build(&gen, NULL);
	rando_world_shop_clear(&gen.world, RANDO_SHOP_CAPACITY_UPGRADE);
	run();
	memcpy(shops, open_shops, sizeof shops);
	shops[2].n = 0;
	check_shops(shops, NULL, 10);
}

static void no_farie_drops(RandoConfig *config)
{
	config->rom_no_farie_drops = true;
}

/* The drop bytes in getAllDrops order (0xD7 + drop id), every drop id in turn, then replaced as VT's
 * str_replace does. */
static void set_drops(RandoGen *g, uint8_t *bytes, bool no_farie, bool rupee_bow)
{
	int pack, slot, n = 0;

	for (pack = 0; pack < RANDO_PRIZE_PACK_COUNT; pack++)
		for (slot = 0; slot < rando_prize_pack_slots((RandoPrizePack)pack); slot++) {
			RandoDrop drop = (RandoDrop)(1 + n % (RANDO_DROP_COUNT - 1));
			uint8_t byte = (uint8_t)(0xD7 + drop);

			rando_world_set_drop(&g->world, (RandoPrizePack)pack, slot, drop);
			if (no_farie && byte == 0xE0)
				byte = 0xDF;
			if (no_farie && byte == 0xE3)
				byte = 0xD8;
			if (rupee_bow && byte == 0xE1)
				byte = 0xDA;
			if (rupee_bow && byte == 0xE2)
				byte = 0xDB;
			bytes[n++] = byte;
		}
	CHECK(n == 63);
}

static void check_drop_writes(const uint8_t *bytes)
{
	check_write(0x37A78, bytes, 56, "writePrizePacksToRom");
	check_write(0xEFBD4, bytes + 56, 3, "writePrizePacksToRom");
	check_byte(0x329C8, bytes[59], "writePrizePacksToRom");
	check_byte(0x329C4, bytes[60], "writePrizePacksToRom");
	check_byte(0x37993, bytes[61], "writePrizePacksToRom");
	check_byte(0xE82CC, bytes[62], "writePrizePacksToRom");
	CHECK(find(0x37A78) == find(0x178000) + 1 || find(0x180100) == find(0x178000) + 1);
	CHECK(find(0xE82CC) == find(0x37A78) + 5);
	CHECK(find(0x1FC16) == find(0xE82CC) + 1);
}

/* writePrizePacksToRom: plain, under rom.NoFarieDrops, and under rom.rupeeBow with its dig prizes first. */
static void check_prize_packs(void)
{
	static const uint8_t dig[64] = {
		0xB2, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD9, 0xD9, 0xD9, 0xD9, 0xD9, 0xDA, 0xDA,
		0xDA, 0xDA, 0xDA, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDD, 0xDD, 0xDD,
		0xDD, 0xDD, 0xDE, 0xDE, 0xDE, 0xDE, 0xDE, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xE0, 0xE0, 0xE0, 0xE0,
		0xE0, 0xDA, 0xDA, 0xDA, 0xDA, 0xDA, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xE3, 0xE3, 0xE3, 0xE3, 0xE3,
	};
	uint8_t bytes[63];

	build(&gen, NULL);
	set_drops(&gen, bytes, false, false);
	run();
	check_drop_writes(bytes);
	CHECK(find(0x180100) < 0);

	build(&gen, no_farie_drops);
	set_drops(&gen, bytes, true, false);
	run();
	check_drop_writes(bytes);

	build_variant(&gen, RANDO_VARIANT_RETRO, NULL);
	set_drops(&gen, bytes, false, true);
	run();
	check_drop_writes(bytes);
	check_write(0x180100, dig, sizeof dig, "writePrizePacksToRom");
	CHECK(find(0x37A78) == find(0x180100) + 1);
}

static const uint32_t uncle_sword_rows[10] = {0x6D263, 0x6D26B, 0x6D293, 0x6D29B, 0x6D2B3,
					      0x6D2BB, 0x6D2E3, 0x6D2EB, 0x6D31B, 0x6D323};
static const uint8_t uncle_sword_bytes[10][6] = {
	{0x00, 0x00, 0xF6, 0xFF, 0x00, 0x0E}, {0x00, 0x00, 0xF6, 0xFF, 0x00, 0x0E}, {0x00, 0x00, 0xF6, 0xFF, 0x00, 0x0E},
	{0x00, 0x00, 0xF7, 0xFF, 0x00, 0x0E}, {0x00, 0x00, 0xF6, 0xFF, 0x02, 0x0E}, {0x00, 0x00, 0xF6, 0xFF, 0x02, 0x0E},
	{0x00, 0x00, 0xF7, 0xFF, 0x02, 0x0E}, {0x00, 0x00, 0xF7, 0xFF, 0x02, 0x0E}, {0x00, 0x00, 0xE4, 0xFF, 0x08, 0x0E},
	{0x00, 0x00, 0xE4, 0xFF, 0x08, 0x0E},
};
static const uint32_t uncle_shield_rows[7] = {0x6D253, 0x6D25B, 0x6D283, 0x6D28B, 0x6D2CB, 0x6D2FB, 0x6D313};
static const uint8_t uncle_shield_bytes[7][6] = {
	{0x00, 0x00, 0xF6, 0xFF, 0x00, 0x0E}, {0x00, 0x00, 0xF6, 0xFF, 0x00, 0x0E}, {0x00, 0x00, 0xF6, 0xFF, 0x00, 0x0E},
	{0x00, 0x00, 0xF7, 0xFF, 0x00, 0x0E}, {0x00, 0x00, 0xF6, 0xFF, 0x02, 0x0E}, {0x00, 0x00, 0xF7, 0xFF, 0x02, 0x0E},
	{0x00, 0x00, 0xE4, 0xFF, 0x08, 0x0E},
};

/* removeUnclesSword unless the uncle holds an Item\Sword (UncleSword is an alias); removeUnclesShield
 * always, right after it, before setBallNChainDungeon. */
static void check_uncle(void)
{
	static const RandoItemId items[4] = {RANDO_ITEM_ProgressiveSword, RANDO_ITEM_UncleSword, RANDO_ITEM_FiveRupees,
					     RANDO_ITEM_NONE};
	int i, j;

	for (i = 0; i < 4; i++) {
		bool sword = items[i] == RANDO_ITEM_ProgressiveSword;

		build(&gen, NULL);
		CHECK(rando_world_set_item(&gen.world, RANDO_LOC_LINKS_UNCLE, items[i]));
		run();
		for (j = 0; j < 10; j++) {
			if (sword) {
				CHECK(find(uncle_sword_rows[j]) < 0);
				continue;
			}
			check_write(uncle_sword_rows[j], uncle_sword_bytes[j], 6, "removeUnclesSword");
			CHECK(find(uncle_sword_rows[j]) == find(0x180169) + 1 + j);
		}
		for (j = 0; j < 7; j++) {
			check_write(uncle_shield_rows[j], uncle_shield_bytes[j], 6, "removeUnclesShield");
			CHECK(find(uncle_shield_rows[j]) == find(0x180169) + (sword ? 1 : 11) + j);
		}
		CHECK(find(0x186FFF) == find(0x6D313) + 1);
	}
}

static void map_on_pickup(RandoConfig *config)
{
	config->rom_map_on_pickup = true;
}

/* Region::$map_reveal of the regions holding PendantOfCourage, and Crystal5 OR Crystal6: Swamp 0x0400,
 * Eastern 0x2000, Ice 0x0040. Before setMapMode, only under rom.mapOnPickup. */
static void check_map_reveal(void)
{
	static const uint8_t sahasrahla[2] = {0x00, 0x04}, bomb_shop[2] = {0x40, 0x20};

	build(&gen, map_on_pickup);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_SWAMP_PALACE_PRIZE, RANDO_ITEM_PendantOfCourage));
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_ITEM_Crystal5));
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_ICE_PALACE_PRIZE, RANDO_ITEM_Crystal6));
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_MISERY_MIRE_PRIZE, RANDO_ITEM_Crystal2));
	run();
	check_write(0x18017A, sahasrahla, 2, "setMapRevealSahasrahla");
	check_write(0x18017C, bomb_shop, 2, "setMapRevealBombShop");
	CHECK(find(0x18017A) == find(0x1801A8) + 1);
	CHECK(find(0x18003B) == find(0x18017C) + 1);

	build(&gen, NULL);
	run();
	CHECK(find(0x18017A) < 0);
	CHECK(find(0x18017C) < 0);
}

static RandoLogic escape_logic;
static bool escape_ignore;
static RandoEnemyHealth escape_health;

static void set_escape_config(RandoConfig *config)
{
	config->logic = escape_logic;
	config->ignore_can_kill_escape_things = escape_ignore;
	config->enemizer_enemy_health = escape_health;
}

/* setEscapeFills' five outcomes: 0 sword or hammer, 1 magic, 2 arrows, 3 bombs, 4 nothing. */
static void check_escape_outcome(RandoItemId uncle, RandoItemId pre_collected, RandoLogic l, bool ignore,
				 int outcome)
{
	static const uint8_t flags[4] = {0x00, 0x04, 0x01, 0x02};
	static const uint8_t refills[4][3][3] = {
		{{0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}},
		{{0x80, 0x00, 0x00}, {0x20, 0x00, 0x00}, {0x20, 0x00, 0x00}},
		{{0x00, 0x00, 0x46}, {0x00, 0x00, 0x0A}, {0x00, 0x00, 0x0A}},
		{{0x00, 0x32, 0x00}, {0x00, 0x03, 0x00}, {0x00, 0x03, 0x00}},
	};
	static const uint8_t rupees[2] = {0x2C, 0x01};

	escape_logic = l;
	escape_ignore = ignore;
	build_variant(&gen, RANDO_VARIANT_STANDARD, set_escape_config);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_LINKS_UNCLE, uncle));
	if (pre_collected != RANDO_ITEM_NONE)
		rando_world_add_pre_collected(&gen.world, pre_collected);
	run();
	if (outcome == 4) {
		CHECK(find(0x18004E) < 0 && find(0x180183) < 0 && find(0x180185) < 0);
		CHECK(rec.writes[0].address == 0x180167);
		return;
	}
	check_byte(0x18004E, flags[outcome], "setEscapeFills");
	check_write(0x180183, rupees, 2, "setEscapeFills");
	check_write(0x180185, refills[outcome][0], 3, "setEscapeFills");
	check_write(0x180188, refills[outcome][1], 3, "setEscapeFills");
	check_write(0x18018B, refills[outcome][2], 3, "setEscapeFills");
	CHECK(find(0x18004E) == 0 && find(0x18018B) == 4 && rec.writes[5].address == 0x180167);
	CHECK(find(0x18004D) < 0);
}

static void check_escape_fills(void)
{
	const RandoLogic ng = RANDO_LOGIC_NO_GLITCHES, nl = RANDO_LOGIC_NO_LOGIC;

	check_escape_outcome(RANDO_ITEM_ProgressiveSword, RANDO_ITEM_NONE, ng, false, 0);
	check_escape_outcome(RANDO_ITEM_Hammer, RANDO_ITEM_NONE, ng, false, 0);
	check_escape_outcome(RANDO_ITEM_FireRod, RANDO_ITEM_NONE, ng, false, 1);
	check_escape_outcome(RANDO_ITEM_CaneOfSomaria, RANDO_ITEM_NONE, ng, false, 1);
	check_escape_outcome(RANDO_ITEM_CaneOfByrna, RANDO_ITEM_NONE, ng, false, 1);
	check_escape_outcome(RANDO_ITEM_ProgressiveBow, RANDO_ITEM_NONE, ng, false, 2);
	check_escape_outcome(RANDO_ITEM_TenBombs, RANDO_ITEM_NONE, nl, false, 3);
	check_escape_outcome(RANDO_ITEM_NONE, RANDO_ITEM_NONE, ng, false, 3);
	check_escape_outcome(RANDO_ITEM_NONE, RANDO_ITEM_NONE, nl, false, 4);
	check_escape_outcome(RANDO_ITEM_FiveRupees, RANDO_ITEM_NONE, nl, false, 4);
	/* No killing item: the pre-collected items count, even under ignoreCanKillEscapeThings. */
	check_escape_outcome(RANDO_ITEM_FiveRupees, RANDO_ITEM_ProgressiveSword, ng, false, 0);
	check_escape_outcome(RANDO_ITEM_FiveRupees, RANDO_ITEM_ProgressiveSword, ng, true, 0);
	/* A killing item: the pre-collected sword is not merged in. */
	check_escape_outcome(RANDO_ITEM_FireRod, RANDO_ITEM_ProgressiveSword, ng, false, 1);
	/* CaneOfByrna counts only at default enemy health. */
	escape_health = RANDO_ENEMY_HEALTH_HARD;
	check_escape_outcome(RANDO_ITEM_CaneOfByrna, RANDO_ITEM_NONE, ng, false, 3);
	check_escape_outcome(RANDO_ITEM_CaneOfByrna, RANDO_ITEM_NONE, nl, false, 4);
	escape_health = RANDO_ENEMY_HEALTH_DEFAULT;

	build(&gen, NULL);
	run();
	CHECK(find(0x18004E) < 0);
	CHECK(rec.writes[0].address == 0x180167);
}

/* A refused settings write is the last call the writer gets. */
static void check_refusal(void)
{
	RandoRomWriter writer = {record, &rec};

	build(&gen, NULL);
	memset(&rec, 0, sizeof rec);
	rec.refuse_at = 1;
	CHECK(!rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.calls == 1 && rec.writes[0].address == 0x180167);
}

/* The digging count and the RNG block are post's draws, written as drawn. */
static void check_post(void)
{
	int i;

	build(&gen, NULL);
	gen.post.digging_game = 23;
	for (i = 0; i < RANDO_RNG_BLOCK_SIZE; i++)
		gen.post.rng_block[i] = (uint8_t)(i * 37 + 5);
	run();
	check_byte(0x180020, 23, "setDiggingGameRng");
	check_byte(0x0EFD95, 23, "setDiggingGameRng");
	CHECK(find(0x0EFD95) == find(0x180020) + 1);
	check_write(0x178000, gen.post.rng_block, RANDO_RNG_BLOCK_SIZE, "writeRNGBlock");
}

static void check_gen_unchanged(void)
{
	build(&gen, NULL);
	gen.post.digging_game = 7;
	memcpy(&before, &gen, sizeof gen);
	run();
	CHECK(memcmp(&before, &gen, sizeof gen) == 0);
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	check_open_order();
	check_goals();
	check_hud_required();
	check_crystals();
	check_functionality();
	check_pools();
	check_rupee_bow();
	check_dungeon_items();
	check_logic();
	check_swordless();
	check_swords_in_pool();
	check_game_state();
	check_inverted_state();
	check_open_shops();
	check_take_any_shops();
	check_cleared_upgrade_shop();
	check_prize_packs();
	check_uncle();
	check_map_reveal();
	check_escape_fills();
	check_refusal();
	check_post();
	check_gen_unchanged();
	printf(TEST ": passed\n");
	return 0;
}
