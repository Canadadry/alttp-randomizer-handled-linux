#include "rom_emit.h"
#include "rom_settings_data.h"

#include <assert.h>

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

static void emit_u16(RomOut *out, uint32_t address, unsigned value, const char *method)
{
	uint8_t bytes[2];

	bytes[0] = (uint8_t)(value & 0xFF);
	bytes[1] = (uint8_t)((value >> 8) & 0xFF);
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
	case RANDO_STATE_INVERTED:
		/* TODO step 7, chunk 4: the rest of setInvertedMode, which starts with setOpenMode. */
		emit_byte(out, 0x180038, 0x00, method);
		break;
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

	/* TODO step 7, chunk 3: setEscapeFills under mode.state standard. */
	emit_u16(out, 0x180167, c->item_goal_required == RANDO_CONFIG_NULL ? 0u : (unsigned)c->item_goal_required,
		 "setGoalRequiredCount");
	emit(out, 0x180165, triforce_icon, 2, "setGoalIcon");
	write_functionality(out, c);
	emit_u16(out, 0x180036, 0, "setRupoorValue");
	emit_byte(out, 0x180086, 0x00, "setGanonAgahnimRng");
	emit_byte(out, 0x18019A, clamp_crystals(c->crystals_tower), "setTowerCrystalRequirement");
	emit_byte(out, 0x1801A6, clamp_crystals(c->crystals_ganon), "setGanonCrystalRequirement");
	emit_byte(out, 0x180172, c->rom_generic_keys ? 0x01 : 0x00, "setGenericKeys");
	/* TODO step 7, chunk 3: setupCustomShops. */
	write_rupee_arrow(out, c->rom_rupee_bow);
	write_fixed_features(out);
	write_limits(out, c);
	emit_byte(out, 0x180182, 0x01, "setSilversEquip");
	write_substitutions(out, c);
	write_goal(out, c->goal);
	/* TODO step 7, chunk 3: the map reveals under rom.mapOnPickup. */
	write_dungeon_modes(out, gen);
	write_post(out, &gen->post);
	/* TODO step 7, chunk 3: writePrizePacksToRom. */
	write_swords_in_pool(out, c->region_swords_in_pool);
	write_game_state(out, c->mode_state);
	write_swordless(out, c->mode_weapons == RANDO_WEAPONS_SWORDLESS);
	if (c->mode_state != RANDO_STATE_INVERTED)
		emit_byte(out, 0x180169, c->logic == RANDO_LOGIC_NO_GLITCHES ? 0x01 : 0x00, "setLockAgahnimDoorInEscape");
	/* TODO step 7, chunk 3: removeUnclesSword and removeUnclesShield. */
	write_clocks(out);
	write_logic(out, c->logic);
	emit_byte(out, 0x180039, hud_item_counter(c) ? 0x01 : 0x00, "enableHudItemCounter");
	emit_byte(out, 0x180211, 0x04, "setGameType");
	emit_byte(out, 0x18008E, 0x00, "setPseudoBoots");
	emit_byte(out, 0x187032, 0x01, "enableFastRom");
}
