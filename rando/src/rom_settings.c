#include "rom_emit.h"
#include "rom_settings_data.h"

#include <assert.h>
#include <string.h>

/* The values every alttpr.com path leaves at VT's config() default. */
#define OVERFLOW_COUNT_BOTTLE 4
#define OVERFLOW_REPLACEMENT RANDO_ITEM_TwentyRupees2
#define BOMB_UPGRADE_FILL 50
#define ARROW_UPGRADE_FILL 70

static void emit(RomOut *out, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	rando_rom_emit(out, address, bytes, count, method);
}

static void emit_byte(RomOut *out, uint32_t address, uint8_t byte, const char *method)
{
	rando_rom_emit(out, address, &byte, 1, method);
}

static void put_u16(uint8_t *bytes, unsigned value)
{
	bytes[0] = (uint8_t)(value & 0xFF);
	bytes[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void emit_u16(RomOut *out, uint32_t address, unsigned value, const char *method)
{
	uint8_t bytes[2];

	put_u16(bytes, value);
	rando_rom_emit(out, address, bytes, 2, method);
}

static uint8_t clamp_crystals(int crystals)
{
	return (uint8_t)(crystals > 7 ? 7 : crystals < 0 ? 0 : crystals);
}

static void write_functionality(RomOut *out, const RandoConfig *c)
{
	static const uint8_t byrna_spike_cave[3] = {0x04, 0x02, 0x01};
	static const uint8_t cape_spike_cave[3] = {0x04, 0x08, 0x10};
	static const uint8_t byrna_magic[3] = {0x04, 0x02, 0x01};
	uint8_t cape[3], bottle[2];

	emit(out, 0x18016B, byrna_spike_cave, 3, "setCaneOfByrnaSpikeCaveUsage");
	emit(out, 0x18016E, cape_spike_cave, 3, "setCapeSpikeCaveUsage");
	emit_byte(out, 0x180195, 0x08, "setByrnaCaveSpikeDamage");
	emit(out, 0x45C42, byrna_magic, 3, "writeToRom");
	cape[0] = (uint8_t)c->rom_cape_magic_usage_normal;
	cape[1] = (uint8_t)c->rom_cape_magic_usage_half;
	cape[2] = (uint8_t)c->rom_cape_magic_usage_quarter;
	emit(out, 0x3ADA7, cape, 3, "setCapeRegularMagicUsage");
	emit_byte(out, 0x18004F, c->rom_cane_of_byrna_invulnerability ? 0x01 : 0x00, "setCaneOfByrnaInvulnerability");
	emit_byte(out, 0x36DD0, (uint8_t)c->rom_powdered_sprite_fairy_prize, "setPowderedSpriteFairyPrize");
	bottle[0] = (uint8_t)c->rom_bottle_fill_health;
	bottle[1] = (uint8_t)c->rom_bottle_fill_magic;
	emit(out, 0x180084, bottle, 2, "setBottleFills");
	emit_byte(out, 0x34FD6, c->rom_catchable_fairies ? 0xF0 : 0x80, "setCatchableFairies");
	emit_byte(out, 0xF5D73, 0xF0, "setCatchableBees");
	emit_byte(out, 0xF5F10, 0xF0, "setCatchableBees");
	emit_byte(out, 0x180180, (uint8_t)c->rom_stun_items, "setStunItems");
	emit_byte(out, 0x180181, c->rom_silvers_only_at_ganon ? 0x01 : 0x00, "setSilversOnlyAtGanon");
}

#define ESCAPE_RUPEES 300

static void write_escape_refills(RomOut *out, uint8_t flags, const uint8_t refills[3][3])
{
	static const char method[] = "setEscapeFills";

	emit_byte(out, 0x18004E, flags, method);
	emit_u16(out, 0x180183, ESCAPE_RUPEES, method);
	emit(out, 0x180185, refills[0], 3, method);
	emit(out, 0x180188, refills[1], 3, method);
	emit(out, 0x18018B, refills[2], 3, method);
}

/* World::setEscapeFills, with rom.EscapeRefills.* and rom.EscapeAssist at their defaults. */
static void write_escape_fills(RomOut *out, const RandoWorld *world)
{
	static const uint8_t sword[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
	static const uint8_t magic[3][3] = {{0x80, 0, 0}, {0x20, 0, 0}, {0x20, 0, 0}};
	static const uint8_t arrows[3][3] = {{0, 0, 70}, {0, 0, 10}, {0, 0, 10}};
	static const uint8_t bombs[3][3] = {{0, 50, 0}, {0, 3, 0}, {0, 3, 0}};
	RandoConfig check = world->config;
	RandoItems items;
	RandoItemId uncle = rando_world_item(world, RANDO_LOC_LINKS_UNCLE);

	rando_items_clear(&items);
	rando_items_add(&items, uncle == RANDO_ITEM_NONE ? RANDO_ITEM_Nothing : uncle);
	check.ignore_can_kill_escape_things = false;
	if (!rando_items_can_kill_escape_things(&items, &check))
		rando_items_merge(&items, &items, &world->pre_collected);
	if (rando_items_has_sword(&items, 1) || rando_items_has(&items, RANDO_ITEM_Hammer))
		write_escape_refills(out, 0x00, sword);
	else if (rando_items_has(&items, RANDO_ITEM_FireRod) || rando_items_has(&items, RANDO_ITEM_CaneOfSomaria)
		 || (rando_items_has(&items, RANDO_ITEM_CaneOfByrna)
		     && world->config.enemizer_enemy_health == RANDO_ENEMY_HEALTH_DEFAULT))
		write_escape_refills(out, 0x04, magic);
	else if (rando_items_can_shoot_arrows(&items, &world->config, 1))
		write_escape_refills(out, 0x01, arrows);
	else if (rando_items_has(&items, RANDO_ITEM_TenBombs) || world->config.logic != RANDO_LOGIC_NO_LOGIC)
		write_escape_refills(out, 0x02, bombs);
}

#define SHOP_BYTES 8
#define SHOP_SRAM_MAX 36
#define TAKE_ANY_SHOPKEEPER 0xE2
#define UPGRADE_MAX 7

static void write_shops(RomOut *out, const RandoWorld *world)
{
	uint8_t shop_bytes[SHOP_BYTES * RANDO_SHOP_COUNT];
	uint8_t item_bytes[SHOP_BYTES * RANDO_SHOP_COUNT * RANDO_SHOP_SLOTS + SHOP_BYTES];
	size_t ns = 0, ni = 0;
	int active = 0, s, slot;
	unsigned id = 0, sram = 0;

	for (s = 0; s < RANDO_SHOP_COUNT; s++)
		if (rando_world_shop_active(world, (RandoShopId)s))
			active++;
	assert(active > 0);
	for (s = 0; s < RANDO_SHOP_COUNT; s++) {
		const RomShop *rom = &rando_rom_shops[s];
		RandoShopKind kind = rando_shop_info((RandoShopId)s)->kind;
		uint8_t shop_id;
		int n = 0;

		if (!rando_world_shop_active(world, (RandoShopId)s))
			continue;
		shop_id = (uint8_t)(id == (unsigned)active - 1 ? 0xFF : id);
		if (rom->extra_address != 0)
			emit_byte(out, rom->extra_address, rom->extra_byte, "setupCustomShops");
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++)
			if (rando_world_shop_item(world, (RandoShopId)s, slot) != RANDO_ITEM_NONE)
				n++;
		shop_bytes[ns++] = shop_id;
		put_u16(shop_bytes + ns, rom->room_id);
		ns += 2;
		shop_bytes[ns++] = rom->door_id;
		shop_bytes[ns++] = 0x00;
		shop_bytes[ns++] = (uint8_t)((rom->config & 0xFC) + n);
		shop_bytes[ns++] = kind == RANDO_SHOP_KIND_TAKE_ANY ? TAKE_ANY_SHOPKEEPER : rom->shopkeeper;
		shop_bytes[ns++] = (uint8_t)sram;
		sram += kind == RANDO_SHOP_KIND_TAKE_ANY ? 1u : (unsigned)n;
		assert(sram <= SHOP_SRAM_MAX);
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++) {
			RandoItemId item = rando_world_shop_item(world, (RandoShopId)s, slot);

			if (item == RANDO_ITEM_NONE)
				continue;
			item_bytes[ni++] = shop_id;
			item_bytes[ni++] = rando_rom_item_byte(item);
			put_u16(item_bytes + ni, (unsigned)rando_world_shop_price(world, (RandoShopId)s, slot));
			ni += 2;
			item_bytes[ni++] = kind == RANDO_SHOP_KIND_UPGRADE ? UPGRADE_MAX : 0;
			item_bytes[ni++] = 0xFF;
			put_u16(item_bytes + ni, 0);
			ni += 2;
		}
		id++;
	}
	memset(item_bytes + ni, 0xFF, SHOP_BYTES);
	ni += SHOP_BYTES;
	emit(out, 0x184800, shop_bytes, ns, "setupCustomShops");
	emit(out, 0x184900, item_bytes, ni, "setupCustomShops");
}

static void write_rupee_arrow(RomOut *out, bool on)
{
	static const uint8_t thief_on[4] = {0xA9, 0x00, 0xEA, 0xEA}, thief_off[4] = {0xAF, 0x77, 0xF3, 0x7E};
	static const uint8_t chest_game_on[2] = {0x35, 0x41}, chest_game_off[2] = {0x43, 0x44};
	static const char method[] = "setRupeeArrow";

	emit_byte(out, 0x30052, on ? 0xDB : 0xE2, method);
	emit_byte(out, 0x301FC, on ? 0xDA : 0xE1, method);
	emit(out, 0xECB4E, on ? thief_on : thief_off, 4, method);
	emit(out, 0xF0D96, on ? thief_on : thief_off, 4, method);
	emit_byte(out, 0x180175, on ? 0x01 : 0x00, method);
	emit_u16(out, 0x180176, on ? 0x0A : 0x00, method);
	emit_u16(out, 0x180178, on ? 0x32 : 0x00, method);
	emit(out, 0xEDA5, on ? chest_game_on : chest_game_off, 2, method);
}

static void write_fixed_features(RomOut *out)
{
	static const uint8_t well_table[2] = {0x14, 0x01};
	static const uint8_t hylia_fairy_shop[6] = {0x1A, 0x1E, 0x01, 0x1A, 0x1E, 0x01};

	emit(out, 0xE9AE, well_table, 2, "setWishingWellChests");
	emit(out, 0xE9CF, well_table, 2, "setWishingWellChests");
	emit(out, 0x1F714, rando_wishing_well_room, WISHING_WELL_ROOM_SIZE, "setWishingWellChests");
	emit_byte(out, 0x348DB, 0x2A, "setWishingWellUpgrade");
	emit_byte(out, 0x348EB, 0x05, "setWishingWellUpgrade");
	emit(out, 0x1F810, hylia_fairy_shop, 6, "setHyliaFairyShop");
	emit_byte(out, 0x18017E, 0x01, "setRestrictFairyPonds");
}

static void write_limit(RomOut *out, uint32_t address, int count, RandoItemId replacement, const char *method)
{
	uint8_t bytes[2];

	bytes[0] = (uint8_t)count;
	bytes[1] = rando_rom_item_byte(replacement);
	emit(out, address, bytes, 2, method);
}

static void write_limits(RomOut *out, const RandoConfig *c)
{
	write_limit(out, 0x180090, c->item_overflow_count_sword, OVERFLOW_REPLACEMENT, "setLimitProgressiveSword");
	write_limit(out, 0x180092, c->item_overflow_count_shield, c->item_overflow_replacement_shield,
		    "setLimitProgressiveShield");
	write_limit(out, 0x180094, c->item_overflow_count_armor, OVERFLOW_REPLACEMENT, "setLimitProgressiveArmor");
	write_limit(out, 0x180096, OVERFLOW_COUNT_BOTTLE, OVERFLOW_REPLACEMENT, "setLimitBottle");
	write_limit(out, 0x180098, c->item_overflow_count_bow, OVERFLOW_REPLACEMENT, "setLimitProgressiveBow");
}

static void write_substitutions(RomOut *out, const RandoConfig *c)
{
	uint8_t bytes[28] = {
		0x12, 0x01, 0x35, 0xFF, 0x51, 0x06, 0x52, 0xFF, 0x53, 0x06, 0x54, 0xFF, 0x58, 0x01,
		0x43, 0xFF, 0x3E, 0x00, 0x00, 0xFF, 0x17, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	};

	if (c->rom_rupee_bow)
		bytes[14] = 0x36;
	bytes[17] = (uint8_t)c->item_overflow_count_boss_heart_container;
	bytes[18] = rando_rom_item_byte(OVERFLOW_REPLACEMENT);
	bytes[21] = (uint8_t)c->item_overflow_count_piece_of_heart;
	bytes[22] = rando_rom_item_byte(OVERFLOW_REPLACEMENT);
	emit(out, 0x184000, bytes, sizeof bytes, "setSubstitutions");
}

static void write_goal(RomOut *out, RandoGoal goal)
{
	static const uint8_t invincible[] = {
		[RANDO_GOAL_GANON] = 0x04,	   [RANDO_GOAL_FAST_GANON] = 0x04, [RANDO_GOAL_DUNGEONS] = 0x02,
		[RANDO_GOAL_PEDESTAL] = 0x01,	   [RANDO_GOAL_TRIFORCE_HUNT] = 0x01, [RANDO_GOAL_GANONHUNT] = 0x05,
		[RANDO_GOAL_COMPLETIONIST] = 0x0B,
	};

	assert((size_t)goal < sizeof invincible);
	if (goal == RANDO_GOAL_TRIFORCE_HUNT)
		emit_byte(out, 0x180194, 0x01, "enableTriforceTurnIn");
	emit_byte(out, 0x1801A8, invincible[goal], "setGanonInvincible");
}

static unsigned map_reveal_of(const RandoWorld *world, RandoItemId item)
{
	static const uint16_t reveal[RANDO_REGION_COUNT] = {
		[RANDO_REGION_EASTERN_PALACE] = 0x2000,	    [RANDO_REGION_DESERT_PALACE] = 0x1000,
		[RANDO_REGION_TOWER_OF_HERA] = 0x0020,	    [RANDO_REGION_PALACE_OF_DARKNESS] = 0x0200,
		[RANDO_REGION_SWAMP_PALACE] = 0x0400,	    [RANDO_REGION_SKULL_WOODS] = 0x0080,
		[RANDO_REGION_THIEVES_TOWN] = 0x0010,	    [RANDO_REGION_ICE_PALACE] = 0x0040,
		[RANDO_REGION_MISERY_MIRE] = 0x0100,	    [RANDO_REGION_TURTLE_ROCK] = 0x0008,
	};
	int i, n = rando_layout_location_count(world->variant);

	for (i = 0; i < n; i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);

		if (rando_world_item(world, location) == item) {
			RandoRegionId region = rando_layout_location_region(world->variant, location);

			assert(region < RANDO_REGION_COUNT);
			return reveal[region];
		}
	}
	assert(!"rom.mapOnPickup with the item placed nowhere");
	return 0;
}

static void write_map_reveals(RomOut *out, const RandoWorld *world)
{
	emit_u16(out, 0x18017A, map_reveal_of(world, RANDO_ITEM_PendantOfCourage), "setMapRevealSahasrahla");
	emit_u16(out, 0x18017C,
		 map_reveal_of(world, RANDO_ITEM_Crystal5) | map_reveal_of(world, RANDO_ITEM_Crystal6),
		 "setMapRevealBombShop");
}

static void write_dungeon_modes(RomOut *out, const RandoGen *gen)
{
	static const uint8_t compass_totals[16] = {0x08, 0x08, 0x06, 0x06, 0x02, 0x0A, 0x0E, 0x08,
						   0x08, 0x08, 0x06, 0x08, 0x0C, 0x1B, 0x00, 0x00};
	const RandoConfig *c = &gen->world.config;

	emit_byte(out, 0x18003B, c->rom_map_on_pickup ? 0x01 : 0x00, "setMapMode");
	emit_byte(out, 0x18003C, c->rom_dungeon_count_pickup ? 0x01 : 0x00, "setCompassMode");
	emit(out, 0x187000, compass_totals, sizeof compass_totals, "setCompassCountTotals");
	emit_byte(out, 0x18016A, (uint8_t)c->rom_free_item_text, "setFreeItemTextMode");
	emit_byte(out, 0x180045, (uint8_t)c->rom_free_item_menu, "setFreeItemMenu");
}

static void write_post(RomOut *out, const RandoPostFill *post)
{
	emit_byte(out, 0x180020, post->digging_game, "setDiggingGameRng");
	emit_byte(out, 0xEFD95, post->digging_game, "setDiggingGameRng");
	emit(out, 0x178000, post->rng_block, RANDO_RNG_BLOCK_SIZE, "writeRNGBlock");
}

#define DROP_BYTES 63

static uint8_t drop_byte(RandoDrop drop, const RandoConfig *c)
{
	uint8_t byte = (uint8_t)(0xD7 + drop);

	assert(drop != RANDO_DROP_NONE);
	if (c->rom_no_farie_drops && byte == 0xE0)
		byte = 0xDF;
	else if (c->rom_no_farie_drops && byte == 0xE3)
		byte = 0xD8;
	if (c->rom_rupee_bow && byte == 0xE1)
		byte = 0xDA;
	else if (c->rom_rupee_bow && byte == 0xE2)
		byte = 0xDB;
	return byte;
}

static void write_prize_packs(RomOut *out, const RandoWorld *world)
{
	static const char method[] = "writePrizePacksToRom";
	uint8_t bytes[DROP_BYTES];
	int pack, slot, n = 0;

	for (pack = 0; pack < RANDO_PRIZE_PACK_COUNT; pack++)
		for (slot = 0; slot < rando_prize_pack_slots((RandoPrizePack)pack); slot++) {
			assert(n < DROP_BYTES);
			bytes[n++] = drop_byte(rando_world_drop(world, (RandoPrizePack)pack, slot), &world->config);
		}
	assert(n == DROP_BYTES);
	if (world->config.rom_rupee_bow)
		emit(out, 0x180100, rando_retro_dig_prizes, RETRO_DIG_PRIZES_SIZE, method);
	emit(out, 0x37A78, bytes, 56, method);
	emit(out, 0xEFBD4, bytes + 56, 3, method);
	emit_byte(out, 0x329C8, bytes[59], method);
	emit_byte(out, 0x329C4, bytes[60], method);
	emit_byte(out, 0x37993, bytes[61], method);
	emit_byte(out, 0xE82CC, bytes[62], method);
}

static void write_swords_in_pool(RomOut *out, bool in_pool)
{
	static const uint8_t chests_on[6] = {0xB1, 0xC6, 0xF9, 0xC9, 0xC6, 0xF9};
	static const uint8_t chests_off[6] = {0xA8, 0xB8, 0x3D, 0xD0, 0xB8, 0x3D};

	emit(out, 0x1FC16, in_pool ? chests_on : chests_off, 6, "setPyramidFairyChests");
	emit_byte(out, 0x180029, in_pool ? 0x01 : 0x00, "setSmithyQuickItemGive");
}

static void write_game_state(RomOut *out, RandoState state)
{
	static const char method[] = "setGameState";

	emit_byte(out, 0x180174, 0x00, method);
	switch (state) {
	case RANDO_STATE_OPEN:
	case RANDO_STATE_RETRO:
		emit_byte(out, 0x180038, 0x00, method);
		break;
	case RANDO_STATE_INVERTED: {
		size_t i;

		emit_byte(out, 0x180038, 0x00, method);
		for (i = 0; i < INVERTED_WRITES; i++) {
			const RomWrite *w = &rando_inverted_writes[i];

			emit(out, w->address, rando_inverted_bytes + w->offset, w->size, method);
		}
		break;
	}
	case RANDO_STATE_STANDARD:
		emit_byte(out, 0x180038, 0x01, method);
		break;
	case RANDO_STATE_UNSET:
		break;
	}
}

static void write_swordless(RomOut *out, bool swordless)
{
	static const char method[] = "setSwordlessMode";
	uint8_t byte = swordless ? 0x01 : 0x00;

	emit_byte(out, 0x18003F, byte, method);
	emit_byte(out, 0x180041, byte, method);
	emit_byte(out, 0x180044, byte, method);
	emit_byte(out, 0x18005D, 0x00, method);
}

static void write_uncle(RomOut *out, const RandoWorld *world)
{
	RandoItemId item = rando_world_item(world, RANDO_LOC_LINKS_UNCLE);
	int i;

	if (item == RANDO_ITEM_NONE || rando_item_info(item)->item_class != RANDO_ITEM_CLASS_SWORD)
		for (i = 0; i < UNCLE_SWORD_ROWS; i++)
			emit(out, rando_uncle_sword_rows[i].address, rando_uncle_sword_rows[i].bytes, UNCLE_ROW_SIZE,
			     "removeUnclesSword");
	/* VT's condition, !instanceof Shield || !hasItem(L1SwordAndShield), holds for every item. */
	for (i = 0; i < UNCLE_SHIELD_ROWS; i++)
		emit(out, rando_uncle_shield_rows[i].address, rando_uncle_shield_rows[i].bytes, UNCLE_ROW_SIZE,
		     "removeUnclesShield");
}

static void write_clocks(RomOut *out)
{
	static const uint8_t off[4] = {0x00, 0x00, 0x00, 0x00};
	uint8_t fills[4] = {BOMB_UPGRADE_FILL, BOMB_UPGRADE_FILL, ARROW_UPGRADE_FILL, ARROW_UPGRADE_FILL};

	emit_byte(out, 0x186FFF, 0x02, "setBallNChainDungeon");
	emit(out, 0x180080, fills, 4, "setCapacityUpgradeFills");
	emit(out, 0x180190, off, 3, "setClockMode");
	emit(out, 0x180204, off, 4, "setBlueClock");
	emit(out, 0x180200, off, 4, "setRedClock");
	emit(out, 0x180208, off, 4, "setGreenClock");
}

static void write_logic(RomOut *out, RandoLogic logic)
{
	switch (logic) {
	case RANDO_LOGIC_HYBRID_MAJOR_GLITCHES:
	case RANDO_LOGIC_MAJOR_GLITCHES:
	case RANDO_LOGIC_NO_LOGIC:
		emit_byte(out, 0x1800A1, 0x00, "setSwampWaterLevel");
		emit_byte(out, 0x1800A2, 0x00, "setPreAgahnimDarkWorldDeathInDungeon");
		emit_byte(out, 0x180042, 0x01, "setSaveAndQuitFromBossRoom");
		emit_byte(out, 0x1800A3, 0x00, "setWorldOnAgahnimDeath");
		emit_byte(out, 0x180210, 0x01, "setRandomizerSeedType");
		emit_byte(out, 0x180212, 0x60, "setWarningFlags");
		emit_byte(out, 0x180358, 0x01, "setAllowAccidentalMajorGlitch");
		emit_byte(out, 0x1800A4, 0x00, "setSQEGFix");
		emit_byte(out, 0x159A8, 0x02, "setZeldaMirrorFix");
		break;
	case RANDO_LOGIC_OVERWORLD_GLITCHES:
		emit_byte(out, 0x1800A2, 0x00, "setPreAgahnimDarkWorldDeathInDungeon");
		emit_byte(out, 0x180042, 0x01, "setSaveAndQuitFromBossRoom");
		emit_byte(out, 0x1800A3, 0x00, "setWorldOnAgahnimDeath");
		emit_byte(out, 0x180210, 0x02, "setRandomizerSeedType");
		emit_byte(out, 0x180212, 0x40, "setWarningFlags");
		emit_byte(out, 0x180358, 0x01, "setAllowAccidentalMajorGlitch");
		emit_byte(out, 0x1800A4, 0x00, "setSQEGFix");
		emit_byte(out, 0x159A8, 0x02, "setZeldaMirrorFix");
		break;
	case RANDO_LOGIC_NO_GLITCHES:
		emit_byte(out, 0x180042, 0x01, "setSaveAndQuitFromBossRoom");
		emit_byte(out, 0x1800A3, 0x01, "setWorldOnAgahnimDeath");
		emit_byte(out, 0x180358, 0x00, "setAllowAccidentalMajorGlitch");
		emit_byte(out, 0x1800A4, 0x01, "setSQEGFix");
		emit_byte(out, 0x159A8, 0x04, "setZeldaMirrorFix");
		break;
	}
}

static bool hud_item_counter(const RandoConfig *c)
{
	if (c->goal == RANDO_GOAL_TRIFORCE_HUNT || c->goal == RANDO_GOAL_GANONHUNT || c->item_goal_required > 0)
		return false;
	return c->goal == RANDO_GOAL_COMPLETIONIST;
}

void rando_rom_write_settings(RomOut *out, const RandoGen *gen)
{
	const RandoConfig *c = &gen->world.config;
	static const uint8_t triforce_icon[2] = {0x0E, 0x28};

	if (c->mode_state == RANDO_STATE_STANDARD)
		write_escape_fills(out, &gen->world);
	emit_u16(out, 0x180167, c->item_goal_required == RANDO_CONFIG_NULL ? 0u : (unsigned)c->item_goal_required,
		 "setGoalRequiredCount");
	emit(out, 0x180165, triforce_icon, 2, "setGoalIcon");
	write_functionality(out, c);
	emit_u16(out, 0x180036, 0, "setRupoorValue");
	emit_byte(out, 0x180086, 0x00, "setGanonAgahnimRng");
	emit_byte(out, 0x18019A, clamp_crystals(c->crystals_tower), "setTowerCrystalRequirement");
	emit_byte(out, 0x1801A6, clamp_crystals(c->crystals_ganon), "setGanonCrystalRequirement");
	emit_byte(out, 0x180172, c->rom_generic_keys ? 0x01 : 0x00, "setGenericKeys");
	write_shops(out, &gen->world);
	write_rupee_arrow(out, c->rom_rupee_bow);
	write_fixed_features(out);
	write_limits(out, c);
	emit_byte(out, 0x180182, 0x01, "setSilversEquip");
	write_substitutions(out, c);
	write_goal(out, c->goal);
	if (c->rom_map_on_pickup)
		write_map_reveals(out, &gen->world);
	write_dungeon_modes(out, gen);
	write_post(out, &gen->post);
	write_prize_packs(out, &gen->world);
	write_swords_in_pool(out, c->region_swords_in_pool);
	write_game_state(out, c->mode_state);
	write_swordless(out, c->mode_weapons == RANDO_WEAPONS_SWORDLESS);
	if (c->mode_state != RANDO_STATE_INVERTED)
		emit_byte(out, 0x180169, c->logic == RANDO_LOGIC_NO_GLITCHES ? 0x01 : 0x00, "setLockAgahnimDoorInEscape");
	write_uncle(out, &gen->world);
	write_clocks(out);
	write_logic(out, c->logic);
	emit_byte(out, 0x180039, hud_item_counter(c) ? 0x01 : 0x00, "enableHudItemCounter");
	emit_byte(out, 0x180211, 0x04, "setGameType");
	emit_byte(out, 0x18008E, 0x00, "setPseudoBoots");
	emit_byte(out, 0x187032, 0x01, "enableFastRom");
}
