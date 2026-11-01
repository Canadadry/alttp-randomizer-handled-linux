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

/* A world of the variant with only its prizes placed (VT's writeItem refuses an empty prize), and the
 * prize music drawn, which the items group reads under rom.mapOnPickup. */
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

/* TODO step 7, chunk 3: setupCustomShops, writePrizePacksToRom and removeUncles* go in their places. */
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
	{0x01FC16, 6, "setPyramidFairyChests"},
	{0x180029, 1, "setSmithyQuickItemGive"},
	{0x180174, 1, "setGameState"},
	{0x180038, 1, "setGameState"},
	{0x18003F, 1, "setSwordlessMode"},
	{0x180041, 1, "setSwordlessMode"},
	{0x180044, 1, "setSwordlessMode"},
	{0x18005D, 1, "setSwordlessMode"},
	{0x180169, 1, "setLockAgahnimDoorInEscape"},
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
	check_refusal();
	check_post();
	check_gen_unchanged();
	printf(TEST ": passed\n");
	return 0;
}
