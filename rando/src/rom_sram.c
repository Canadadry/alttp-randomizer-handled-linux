#include "rando_items.h"
#include "rom_emit.h"

#include <assert.h>
#include <string.h>

/* VT's InitialSram::SRAM_SIZE. */
#define SRAM_SIZE 0x500
#define SRAM_ADDRESS 0x183000u
#define TOTAL_ITEM_COUNT_ADDRESS 0x180196u
/* VT's rom.timerStart, never set on the generation path. */
#define TIMER_START_SECONDS 0u

/* InitialSram::setValue: ORs, never assigns. */
static void set_value(uint8_t *sram, int idx, int val)
{
	assert(idx >= 0 && idx < SRAM_SIZE);
	assert(val >= 0 && val <= 0xFF);
	sram[idx] |= (uint8_t)val;
}

static int min_int(int a, int b)
{
	return a < b ? a : b;
}

static int count_set_bits(uint8_t byte)
{
	int n = 0;

	for (; byte != 0; byte &= (uint8_t)(byte - 1))
		n++;
	return n;
}

static bool is_open_state(RandoState state)
{
	return state == RANDO_STATE_OPEN || state == RANDO_STATE_RETRO || state == RANDO_STATE_INVERTED;
}

/* ItemCollection::heartCount(0) < 1, in quarters: a PieceOfHeart counts 1, any other Health item 4. */
static bool below_one_heart(const RandoWorld *world)
{
	int quarters = 0, i;

	for (i = 0; i < rando_world_pre_collected_count(world); i++) {
		RandoItemId item = rando_world_pre_collected_item(world, i);

		if (rando_item_info(item)->item_class == RANDO_ITEM_CLASS_HEALTH)
			quarters += item == RANDO_ITEM_PieceOfHeart ? 1 : 4;
	}
	return quarters < 4;
}

/* InitialSram::setStartingEquipment for the items this generator can pre-collect. */
static void set_starting_equipment(uint8_t *sram, const RandoWorld *world, RandoWeapons weapons)
{
	int rupees = 0, bomb_capacity = 0, arrow_capacity = 0, i;

	if (below_one_heart(world)) {
		sram[0x36C] = 0x18;
		sram[0x36D] = 0x18;
	}
	for (i = 0; i < rando_world_pre_collected_count(world); i++) {
		RandoItemId item = rando_world_pre_collected_item(world, i);
		const RandoItemInfo *info = rando_item_info(item);

		if (info->item_class == RANDO_ITEM_CLASS_ALIAS)
			item = info->alias_target;
		switch (item) {
		case RANDO_ITEM_BossHeartContainer:
		case RANDO_ITEM_HeartContainer:
		case RANDO_ITEM_HeartContainerNoAnimation:
			sram[0x36C] = (uint8_t)min_int(sram[0x36C] + 0x08, 0xA0);
			sram[0x36D] = (uint8_t)min_int(sram[0x36D] + 0x08, 0xA0);
			break;
		case RANDO_ITEM_BombUpgrade10:
			bomb_capacity += 10;
			break;
		case RANDO_ITEM_ArrowUpgrade10:
			arrow_capacity += 10;
			break;
		case RANDO_ITEM_PegasusBoots:
			sram[0x355] = 0x01;
			sram[0x379] |= 0x04;
			break;
		case RANDO_ITEM_ProgressiveSword:
			sram[0x359] = (uint8_t)min_int(sram[0x359] + 1, 4);
			break;
		default:
			/* VT's switch has ~110 more labels; no generation path pre-collects their items. */
			assert(rando_item_info(item)->item_class == RANDO_ITEM_CLASS_EVENT &&
			       "port its setStartingEquipment label");
			break;
		}
	}
	sram[0x360] = sram[0x362] = (uint8_t)(rupees & 0xFF);
	sram[0x361] = sram[0x363] = (uint8_t)(rupees >> 8);
	sram[0x476] = (uint8_t)count_set_bits(sram[0x37A]);
	sram[0x429] = (uint8_t)count_set_bits(sram[0x374]);
	sram[0x417] = sram[0x359];
	sram[0x422] = sram[0x35A];
	sram[0x46E] = sram[0x35B];
	set_value(sram, 0x370, bomb_capacity);
	set_value(sram, 0x371, arrow_capacity);
	if (weapons == RANDO_WEAPONS_SWORDLESS) {
		sram[0x359] = 0xFF;
		sram[0x417] = 0x00;
	}
}

/* InitialSram::setStartingTimer: assigns, little-endian. */
static void set_starting_timer(uint8_t *sram, uint32_t seconds)
{
	uint32_t frames = seconds * 60;
	int i;

	for (i = 0; i < 4; i++)
		sram[0x454 + i] = (uint8_t)(frames >> (8 * i));
}

void rando_rom_write_initial_sram(RomOut *out, const RandoGen *gen)
{
	const RandoConfig *config = &gen->world.config;
	uint8_t sram[SRAM_SIZE];

	/* VT's call order, kept: setStartingEquipment and setStartingTimer assign where the others OR. */
	memset(sram, 0, sizeof sram);
	sram[0x20D] = 0xF0;
	sram[0x20F] = 0xF0;
	sram[0x379] = 0x68;
	sram[0x401] = 0xFF;
	sram[0x402] = 0xFF;
	if (config->goal == RANDO_GOAL_FAST_GANON || config->goal == RANDO_GOAL_GANONHUNT)
		set_value(sram, 0x2DB, 0x20);
	if (is_open_state(config->mode_state)) {
		set_value(sram, 0x29B, 0x20);
		set_value(sram, 0x3C5, 0x02);
		set_value(sram, 0x3C6, 0x14);
		set_value(sram, 0x3C8, 0x01);
	}
	if (config->mode_weapons == RANDO_WEAPONS_SWORDLESS) {
		set_value(sram, 0x061, 0x80);
		set_value(sram, 0x093, 0x80);
	}
	set_starting_equipment(sram, &gen->world, config->mode_weapons);
	set_starting_timer(sram, TIMER_START_SECONDS);
	/* VT tests `crystals.tower === 0`: the menu's '0' is a string, so only a drawn 0 matches. */
	if (gen->options.crystals_tower == RANDO_CRYSTALS_RANDOM && config->crystals_tower == 0)
		set_value(sram, 0x2C3, 0x20);
	rando_rom_emit(out, SRAM_ADDRESS, sram, sizeof sram, "writeInitialSram");
}

static bool counts_as_item(RandoLocationClass c)
{
	return !rando_location_is_a(c, RANDO_LOCATION_CLASS_PRIZE) && !rando_location_is_a(c, RANDO_LOCATION_CLASS_FOUNTAIN)
	       && !rando_location_is_a(c, RANDO_LOCATION_CLASS_MEDALLION)
	       && !rando_location_is_a(c, RANDO_LOCATION_CLASS_TRADE);
}

void rando_rom_write_total_item_count(RomOut *out, const RandoGen *gen)
{
	RandoVariant variant = gen->world.variant;
	int count = 0, i;
	uint8_t bytes[2];

	for (i = 0; i < rando_layout_location_count(variant); i++)
		if (counts_as_item(rando_location_info(rando_layout_location(variant, i))->location_class))
			count++;
	assert(count <= 0xFFFF);
	bytes[0] = (uint8_t)(count & 0xFF);
	bytes[1] = (uint8_t)(count >> 8);
	rando_rom_emit(out, TOTAL_ITEM_COUNT_ADDRESS, bytes, sizeof bytes, "setTotalItemCount");
}
