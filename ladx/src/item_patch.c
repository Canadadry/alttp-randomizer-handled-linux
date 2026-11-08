#include "ladx_groups.h"
#include "ladx_room.h"

#include <stdio.h>
#include <string.h>

#define TEXTS 0
#define BANK(b) ((uint32_t)(b) * LADX_ROM_BANK)
#define CHEST_TABLE (BANK(0x14) + 0x560u)
#define DROP_TABLE (BANK(0x3E) + 0x3800u)
#define BOSS_ITEM (BANK(0x3E) + 0x3300u)
#define OWL_TABLE (BANK(0x3E) + 0x3B16u)
#define BONUS_TABLE (BANK(0x3E) + 0x3E30u)
#define BATTER_TABLE (BANK(0x18) + 0x0F90u)
#define FAIRY_TABLE (BANK(0x36) + 0x11BFu)
#define WITCH_ITEM (BANK(0x05) + 0x08D5u)
#define GIFT_ITEM (BANK(0x19) + 0x072Bu)
#define POT_CHEST (BANK(0x03) + 0x145Du)
#define MULTICHEST_ROOM 0x2F2
#define POT_CHEST_ROOM 0x1B6
#define BEACH_ROOM 0xF2
#define HOOK (BANK(0x02) + 0x2FA9u)
#define HOOK_TARGET (BANK(0x02) + 0x2FAAu)
#define HOOK_OPCODE 0xF0
#define KEYHOLE_CODE 12
#define KEYHOLE_SLOT0 (BANK(0x02) + 0x3FEFu)
#define GEL_DISGUISE_MIN 0x51
#define GEL_DISGUISE_MAX 0x57
#define SHOP_PRICE_0 200
#define SHOP_PRICE_1 980

enum {
	UNIT_BEACH,
	UNIT_START,
	UNIT_POT_CHEST,
	UNIT_TRADE,
	UNIT_GIFT,
	UNIT_HOOK,
	UNIT_CASTLE,
	UNIT_KEYHOLE,
	UNIT_TOTAL = UNIT_KEYHOLE + 5
};

static const int keyhole_rooms[5] = {0x0D3, 0x0B5, 0x02B, 0x08C, 0x00E};
static const uint16_t trade_operands[4] = {0x063F, 0x06C1, 0x06FC, 0x0710};

/* locations/constants.py CHEST_ITEMS; 0 marks an item it lacks (every id is stored plus one). */
static const unsigned char chest_ids[LADX_ITEM_COUNT] = {
	[LADX_ITEM_POWER_BRACELET] = 1 + 0x00, [LADX_ITEM_SHIELD] = 1 + 0x01, [LADX_ITEM_BOW] = 1 + 0x02,
	[LADX_ITEM_HOOKSHOT] = 1 + 0x03, [LADX_ITEM_MAGIC_ROD] = 1 + 0x04, [LADX_ITEM_PEGASUS_BOOTS] = 1 + 0x05,
	[LADX_ITEM_OCARINA] = 1 + 0x06, [LADX_ITEM_FEATHER] = 1 + 0x07, [LADX_ITEM_SHOVEL] = 1 + 0x08,
	[LADX_ITEM_MAGIC_POWDER] = 1 + 0x09, [LADX_ITEM_BOMB] = 1 + 0x0A, [LADX_ITEM_SWORD] = 1 + 0x0B,
	[LADX_ITEM_FLIPPERS] = 1 + 0x0C, [LADX_ITEM_MAGNIFYING_LENS] = 1 + 0x0D, [LADX_ITEM_MEDICINE] = 1 + 0x10,
	[LADX_ITEM_TAIL_KEY] = 1 + 0x11, [LADX_ITEM_ANGLER_KEY] = 1 + 0x12, [LADX_ITEM_FACE_KEY] = 1 + 0x13,
	[LADX_ITEM_BIRD_KEY] = 1 + 0x14, [LADX_ITEM_GOLD_LEAF] = 1 + 0x15, [LADX_ITEM_RUPEES_50] = 1 + 0x1B,
	[LADX_ITEM_RUPEES_20] = 1 + 0x1C, [LADX_ITEM_RUPEES_100] = 1 + 0x1D, [LADX_ITEM_RUPEES_200] = 1 + 0x1E,
	[LADX_ITEM_RUPEES_500] = 1 + 0x1F, [LADX_ITEM_SEASHELL] = 1 + 0x20, [LADX_ITEM_MESSAGE] = 1 + 0x21,
	[LADX_ITEM_GEL] = 1 + 0x22, [LADX_ITEM_MAP] = 1 + 0x16, [LADX_ITEM_COMPASS] = 1 + 0x17,
	[LADX_ITEM_STONE_BEAK] = 1 + 0x18, [LADX_ITEM_NIGHTMARE_KEY] = 1 + 0x19, [LADX_ITEM_KEY] = 1 + 0x1A,
	[LADX_ITEM_ROOSTER] = 1 + 0x96, [LADX_ITEM_HAMMER] = 1 + 0x57, [LADX_ITEM_BOOMERANG] = 1 + 0x0E,
	[LADX_ITEM_SLIME_KEY] = 1 + 0x0F, [LADX_ITEM_TOADSTOOL] = 1 + 0x50,
	[LADX_ITEM_TAIL_CAVE_OPENED] = 1 + 0x58, [LADX_ITEM_KEY_CAVERN_OPENED] = 1 + 0x59,
	[LADX_ITEM_ANGLER_TUNNEL_OPENED] = 1 + 0x5A, [LADX_ITEM_FACE_SHRINE_OPENED] = 1 + 0x5B,
	[LADX_ITEM_CASTLE_GATE_OPENED] = 1 + 0x5C, [LADX_ITEM_EAGLE_TOWER_OPENED] = 1 + 0x5D,
	[LADX_ITEM_HEART_PIECE] = 1 + 0x80, [LADX_ITEM_BOWWOW] = 1 + 0x81, [LADX_ITEM_ARROWS_10] = 1 + 0x82,
	[LADX_ITEM_SINGLE_ARROW] = 1 + 0x83, [LADX_ITEM_MAX_POWDER_UPGRADE] = 1 + 0x84,
	[LADX_ITEM_MAX_BOMBS_UPGRADE] = 1 + 0x85, [LADX_ITEM_MAX_ARROWS_UPGRADE] = 1 + 0x86,
	[LADX_ITEM_RED_TUNIC] = 1 + 0x87, [LADX_ITEM_BLUE_TUNIC] = 1 + 0x88, [LADX_ITEM_HEART_CONTAINER] = 1 + 0x89,
	[LADX_ITEM_BAD_HEART_CONTAINER] = 1 + 0x8A, [LADX_ITEM_SONG1] = 1 + 0x8B, [LADX_ITEM_SONG2] = 1 + 0x8C,
	[LADX_ITEM_SONG3] = 1 + 0x8D, [LADX_ITEM_INSTRUMENT1] = 1 + 0x8E,
	[LADX_ITEM_INSTRUMENT2] = 1 + 0x8F, [LADX_ITEM_INSTRUMENT3] = 1 + 0x90, [LADX_ITEM_INSTRUMENT4] = 1 + 0x91,
	[LADX_ITEM_INSTRUMENT5] = 1 + 0x92, [LADX_ITEM_INSTRUMENT6] = 1 + 0x93, [LADX_ITEM_INSTRUMENT7] = 1 + 0x94,
	[LADX_ITEM_INSTRUMENT8] = 1 + 0x95, [LADX_ITEM_TRADING_ITEM_YOSHI_DOLL] = 1 + 0x97,
	[LADX_ITEM_TRADING_ITEM_RIBBON] = 1 + 0x98, [LADX_ITEM_TRADING_ITEM_DOG_FOOD] = 1 + 0x99,
	[LADX_ITEM_TRADING_ITEM_BANANAS] = 1 + 0x9A, [LADX_ITEM_TRADING_ITEM_STICK] = 1 + 0x9B,
	[LADX_ITEM_TRADING_ITEM_HONEYCOMB] = 1 + 0x9C, [LADX_ITEM_TRADING_ITEM_PINEAPPLE] = 1 + 0x9D,
	[LADX_ITEM_TRADING_ITEM_HIBISCUS] = 1 + 0x9E, [LADX_ITEM_TRADING_ITEM_LETTER] = 1 + 0x9F,
	[LADX_ITEM_TRADING_ITEM_BROOM] = 1 + 0xA0, [LADX_ITEM_TRADING_ITEM_FISHING_HOOK] = 1 + 0xA1,
	[LADX_ITEM_TRADING_ITEM_NECKLACE] = 1 + 0xA2, [LADX_ITEM_TRADING_ITEM_SCALE] = 1 + 0xA3,
	[LADX_ITEM_TRADING_ITEM_MAGNIFYING_GLASS] = 1 + 0xA4, [LADX_ITEM_KEY1] = 1 + 0x23, [LADX_ITEM_KEY2] = 1 + 0x24,
	[LADX_ITEM_KEY3] = 1 + 0x25, [LADX_ITEM_KEY4] = 1 + 0x26, [LADX_ITEM_KEY5] = 1 + 0x27, [LADX_ITEM_KEY6] = 1 + 0x28,
	[LADX_ITEM_KEY7] = 1 + 0x29, [LADX_ITEM_KEY8] = 1 + 0x2A, [LADX_ITEM_KEY0] = 1 + 0x2B, [LADX_ITEM_MAP1] = 1 + 0x2C,
	[LADX_ITEM_MAP2] = 1 + 0x2D, [LADX_ITEM_MAP3] = 1 + 0x2E, [LADX_ITEM_MAP4] = 1 + 0x2F, [LADX_ITEM_MAP5] = 1 + 0x30,
	[LADX_ITEM_MAP6] = 1 + 0x31, [LADX_ITEM_MAP7] = 1 + 0x32, [LADX_ITEM_MAP8] = 1 + 0x33, [LADX_ITEM_MAP0] = 1 + 0x34,
	[LADX_ITEM_COMPASS1] = 1 + 0x35, [LADX_ITEM_COMPASS2] = 1 + 0x36, [LADX_ITEM_COMPASS3] = 1 + 0x37,
	[LADX_ITEM_COMPASS4] = 1 + 0x38, [LADX_ITEM_COMPASS5] = 1 + 0x39, [LADX_ITEM_COMPASS6] = 1 + 0x3A,
	[LADX_ITEM_COMPASS7] = 1 + 0x3B, [LADX_ITEM_COMPASS8] = 1 + 0x3C, [LADX_ITEM_COMPASS0] = 1 + 0x3D,
	[LADX_ITEM_STONE_BEAK1] = 1 + 0x3E, [LADX_ITEM_STONE_BEAK2] = 1 + 0x3F, [LADX_ITEM_STONE_BEAK3] = 1 + 0x40,
	[LADX_ITEM_STONE_BEAK4] = 1 + 0x41, [LADX_ITEM_STONE_BEAK5] = 1 + 0x42, [LADX_ITEM_STONE_BEAK6] = 1 + 0x43,
	[LADX_ITEM_STONE_BEAK7] = 1 + 0x44, [LADX_ITEM_STONE_BEAK8] = 1 + 0x45, [LADX_ITEM_STONE_BEAK0] = 1 + 0x46,
	[LADX_ITEM_NIGHTMARE_KEY1] = 1 + 0x47, [LADX_ITEM_NIGHTMARE_KEY2] = 1 + 0x48,
	[LADX_ITEM_NIGHTMARE_KEY3] = 1 + 0x49, [LADX_ITEM_NIGHTMARE_KEY4] = 1 + 0x4A,
	[LADX_ITEM_NIGHTMARE_KEY5] = 1 + 0x4B, [LADX_ITEM_NIGHTMARE_KEY6] = 1 + 0x4C,
	[LADX_ITEM_NIGHTMARE_KEY7] = 1 + 0x4D, [LADX_ITEM_NIGHTMARE_KEY8] = 1 + 0x4E,
	[LADX_ITEM_NIGHTMARE_KEY0] = 1 + 0x4F,
};

static const int dungeon_families[5] = {LADX_ITEM_KEY, LADX_ITEM_NIGHTMARE_KEY, LADX_ITEM_MAP, LADX_ITEM_COMPASS,
					LADX_ITEM_STONE_BEAK};

int ladx_items_chest_id(int item)
{
	if (item < 0 || item >= LADX_ITEM_COUNT || !chest_ids[item])
		return -1;
	return chest_ids[item] - 1;
}

int ladx_items_inventory_id(int item)
{
	switch (item) {
	case LADX_ITEM_SWORD:
		return 0x01;
	case LADX_ITEM_BOMB:
		return 0x02;
	case LADX_ITEM_POWER_BRACELET:
		return 0x03;
	case LADX_ITEM_SHIELD:
		return 0x04;
	case LADX_ITEM_BOW:
		return 0x05;
	case LADX_ITEM_HOOKSHOT:
		return 0x06;
	case LADX_ITEM_MAGIC_ROD:
		return 0x07;
	case LADX_ITEM_PEGASUS_BOOTS:
		return 0x08;
	case LADX_ITEM_OCARINA:
		return 0x09;
	case LADX_ITEM_FEATHER:
		return 0x0A;
	case LADX_ITEM_SHOVEL:
		return 0x0B;
	case LADX_ITEM_MAGIC_POWDER:
		return 0x0C;
	case LADX_ITEM_BOOMERANG:
		return 0x0D;
	case LADX_ITEM_TOADSTOOL:
		return 0x0E;
	default:
		return -1;
	}
}

int ladx_items_dungeon_item(int item, int dungeon)
{
	const char *name = ladx_item_name(item);
	size_t i, len;

	if (!name)
		return item;
	len = strlen(name);
	for (i = 0; i < sizeof dungeon_families / sizeof dungeon_families[0]; i++) {
		const char *base = ladx_item_name(dungeon_families[i]);
		size_t blen = strlen(base);

		if (len == blen + 1 && strncmp(name, base, blen) == 0 && name[blen] >= '0' && name[blen] <= '9')
			return dungeon == name[blen] - '0' ? dungeon_families[i] : item;
	}
	return item;
}

typedef struct Run {
	LadxEncoder *enc;
	LadxPipeline *p;
	const LadxLogic *logic;
} Run;

static LadxSpan group_name(void)
{
	LadxSpan s;

	s.at = "items";
	s.len = 5;
	return s;
}

static int raise_error(Run *r, const char *type, const char *fmt, const char *arg)
{
	r->p->raised.kind = LADX_OUTCOME_CRASH;
	r->p->raised.type = type;
	snprintf(r->p->raised.text, sizeof r->p->raised.text, fmt, arg);
	r->p->raised.message = r->p->raised.text;
	return 0;
}

static int put(Run *r, uint32_t offset, const uint8_t *values, size_t n)
{
	int32_t origins[LADX_ROOM_OVERLAY];
	size_t i;

	if (n > LADX_ROOM_OVERLAY)
		return 0;
	for (i = 0; i < n; i++)
		origins[i] = LADX_ENC_KNOWN;
	return ladx_enc_bank(r->enc, group_name(), offset, values, origins, n) == LADX_ENC_OK;
}

static int put_byte(Run *r, uint32_t offset, int value)
{
	uint8_t v = (uint8_t)value;

	return put(r, offset, &v, 1);
}

static int chest_id(Run *r, int item, int *id)
{
	*id = ladx_items_chest_id(item);
	if (*id >= 0)
		return 1;
	return raise_error(r, "KeyError", "'%s'", ladx_item_name(item) ? ladx_item_name(item) : "None");
}

static int write_bytes_text(Run *r, unsigned id, const uint8_t *bytes, size_t n)
{
	LadxWlogKey key;
	size_t i;

	for (i = 0; i < n; i++)
		r->p->origins[i] = LADX_ENC_KNOWN;
	key.index = id;
	key.alt.at = NULL;
	key.alt.len = 0;
	return ladx_enc_table(r->enc, group_name(), TEXTS, key, bytes, r->p->origins, n) == LADX_ENC_OK;
}

static int write_text(Run *r, unsigned id, const char *fmt, int item, int price, const char *ask)
{
	char text[LADX_TEXT_MAX];
	uint8_t bytes[LADX_WLOG_ATOM_MAX];
	const char *name = item == LADX_ITEM_NONE ? NULL : ladx_item_name(item);
	size_t n;
	int len;

	if (item != LADX_ITEM_NONE && !name)
		return 0;
	if (!name)
		len = snprintf(text, sizeof text, "%s", fmt);
	else if (price >= 0)
		len = snprintf(text, sizeof text, fmt, name, price);
	else
		len = snprintf(text, sizeof text, fmt, name);
	if (len < 0 || (size_t)len >= sizeof text)
		return 0;
	switch (ladx_format_text(text, (size_t)len, 0, ask, bytes, sizeof bytes, &n, &r->p->raised)) {
	case LADX_TEXT_OK:
		return write_bytes_text(r, id, bytes, n);
	case LADX_TEXT_RAISED:
		r->p->raised.message = r->p->raised.text;
		return 0;
	default:
		return 0;
	}
}

static int current(Run *r, uint32_t offset, uint8_t *value)
{
	int32_t origin;

	return ladx_enc_bank_value(r->enc, offset, value, &origin);
}

static int patch_byte(Run *r, uint32_t offset, int old, int value)
{
	char arg[48];
	uint8_t cur;

	if (!current(r, offset, &cur))
		return 0;
	if (cur == old)
		return put_byte(r, offset, value);
	if (cur == value)
		return 1;
	snprintf(arg, sizeof arg, "b'%02x' !=\nb'%02x' at 0x%04x", cur, (unsigned)old,
		 (unsigned)(offset % LADX_ROM_BANK));
	return raise_error(r, "AssertionError", "Patch mismatch:\n%s", arg);
}

static int operand_patch(Run *r, int unit, uint32_t offset, int operand)
{
	uint8_t code[2];

	if (!ladx_pipeline_const_bank(r->p, "items", unit, offset, code, sizeof code))
		return 0;
	code[1] = (uint8_t)operand;
	return put(r, offset, code, sizeof code);
}

static int dungeon_of(Run *r, const LadxSpot *spot)
{
	const LadxLocation *loc = ladx_logic_location(r->logic, spot->location);

	return loc ? loc->dungeon : -1;
}

static int chest(Run *r, const LadxSpot *spot, int item)
{
	int id;

	if (!chest_id(r, item, &id) || !put_byte(r, CHEST_TABLE + (uint32_t)spot->room, id))
		return 0;
	return spot->room != POT_CHEST_ROOM || operand_patch(r, UNIT_POT_CHEST, POT_CHEST, id);
}

static int gel_or_id(Run *r, int room, int item, int *id)
{
	if (!chest_id(r, item, id))
		return 0;
	if (item == LADX_ITEM_GEL)
		*id = GEL_DISGUISE_MIN + room % (GEL_DISGUISE_MAX - GEL_DISGUISE_MIN);
	return 1;
}

static int dropped(Run *r, const LadxSpot *spot, int item)
{
	int id;

	if (spot->room != 0x166 && spot->room != 0x223)
		item = ladx_items_dungeon_item(item, dungeon_of(r, spot));
	if (!gel_or_id(r, spot->room, item, &id) || !put_byte(r, DROP_TABLE + (uint32_t)spot->room, id))
		return 0;
	switch (spot->room) {
	case 0x169:
		return put_byte(r, DROP_TABLE + 0x017Cu, id);
	case 0x166:
		return put_byte(r, DROP_TABLE + 0x01FFu, id);
	case 0x223:
		return put_byte(r, DROP_TABLE + 0x02E8u, id);
	case 0x092:
		return put_byte(r, DROP_TABLE + 0x00DCu, id);
	default:
		return 1;
	}
}

static int angler_key(Run *r, const LadxSpot *spot, int item)
{
	static const uint32_t tables[2] = {DROP_TABLE, BOSS_ITEM};
	int i;

	if (!dropped(r, spot, item))
		return 0;
	for (i = 0; i < 2; i++) {
		uint8_t v;
		int32_t origin;

		if (!ladx_enc_bank_value(r->enc, tables[i] + 0x0CEu, &v, &origin) ||
		    ladx_enc_bank(r->enc, group_name(), tables[i] + 0x1F8u, &v, &origin, 1) != LADX_ENC_OK)
			return 0;
	}
	return 1;
}

static int beach_sword(Run *r, const LadxSpot *spot, int item)
{
	LadxRoom *re = &r->p->room;

	if (!dropped(r, spot, item) || ladx_room_load(re, r->enc, BEACH_ROOM, NULL) != LADX_ROOM_OK)
		return 0;
	ladx_room_remove_entities(re, 0x31);
	return ladx_room_add_entity(re, 5, 5, 0x35) == LADX_ROOM_OK &&
	       ladx_room_store(re, r->enc, group_name(), -1, NULL) == LADX_ROOM_OK &&
	       ladx_pipeline_const_units(r->enc, r->p, "items", UNIT_BEACH, 1);
}

static int start_item(Run *r, const LadxSpot *spot, int item)
{
	if (r->p->settings->value[LADX_SETTING_BOWWOW] != ladx_setting_value_find(LADX_SETTING_BOWWOW, "normal")) {
		item = LADX_ITEM_BOWWOW;
		if (!write_text(r, 0xC8, "Got BowWow!", LADX_ITEM_NONE, -1, NULL))
			return 0;
	}
	if (item != LADX_ITEM_SHIELD && !ladx_pipeline_const_units(r->enc, r->p, "items", UNIT_START, 1))
		return 0;
	return dropped(r, spot, item);
}

static int keyhole_handler(Run *r, int unit)
{
	uint8_t code[KEYHOLE_CODE], lo, hi, word[2];
	unsigned target;

	if (!ladx_pipeline_const_bank(r->p, "items", unit, KEYHOLE_SLOT0, code, sizeof code) ||
	    !current(r, HOOK_TARGET, &lo) || !current(r, HOOK_TARGET + 1, &hi))
		return 0;
	target = (unsigned)lo | (unsigned)hi << 8;
	if (target < 0x4000u + KEYHOLE_CODE || target > 0x8000u)
		return 0;
	target -= KEYHOLE_CODE;
	word[0] = (uint8_t)(target & 0xFF);
	word[1] = (uint8_t)(target >> 8);
	return put(r, BANK(0x02) + target - 0x4000u, code, sizeof code) && put(r, HOOK_TARGET, word, 2);
}

static int keyhole(Run *r, const LadxSpot *spot, int item)
{
	char where[48];
	uint8_t hook;
	int id, i;

	if (!gel_or_id(r, spot->room, item, &id) || !put_byte(r, OWL_TABLE + (uint32_t)spot->room, id))
		return 0;
	if (item == spot->default_item)
		return 1;
	if (!current(r, HOOK, &hook))
		return 0;
	if (hook == HOOK_OPCODE && !ladx_pipeline_const_units(r->enc, r->p, "items", UNIT_HOOK, 1))
		return 0;
	for (i = 0; i < 5; i++)
		if (spot->room == keyhole_rooms[i])
			return keyhole_handler(r, UNIT_KEYHOLE + i);
	if (spot->room == 0x2C3)
		return ladx_pipeline_const_units(r->enc, r->p, "items", UNIT_CASTLE, 1);
	snprintf(where, sizeof where, "%03x:%s", (unsigned)spot->room,
		 ladx_item_name(spot->default_item) ? ladx_item_name(spot->default_item) : "None");
	return raise_error(r, "RuntimeError", "Don't know how to patch keyhole at %s", where);
}

static int owl_statue(Run *r, const LadxSpot *spot, int item)
{
	int id;

	return chest_id(r, ladx_items_dungeon_item(item, dungeon_of(r, spot)), &id) &&
	       put_byte(r, OWL_TABLE + (uint32_t)spot->room, id);
}

static int shop_item(Run *r, const LadxSpot *spot, int item)
{
	int id;

	if (!chest_id(r, item, &id))
		return 0;
	if (spot->index == 0)
		return put_byte(r, DROP_TABLE + (uint32_t)spot->room, id) &&
		       write_text(r, 0x030, "Deluxe {%s} %d {RUPEES}!", item, SHOP_PRICE_0, "Buy  No Way");
	if (spot->index == 1)
		return put_byte(r, CHEST_TABLE + (uint32_t)spot->room, id) &&
		       write_text(r, 0x02C, "{%s} Only %d {RUPEES}!", item, SHOP_PRICE_1, "Buy  No Way");
	return 1;
}

static int trade_item(Run *r, const LadxSpot *spot, int item)
{
	static const uint8_t no_text = 0xFF;
	int id;

	if (!chest_id(r, item, &id) || !put_byte(r, OWL_TABLE + (uint32_t)spot->room, id))
		return 0;
	if (item == spot->default_item)
		return 1;
	switch (spot->default_item) {
	case LADX_ITEM_TRADING_ITEM_DOG_FOOD:
		return write_text(r, 0x181,
				  "Make-up! Jewels! Dresses! I want it all! Sigh... And some new accessories would be "
				  "nice... Oh! That Ribbon! I need it! Will you trade for my {%s}?",
				  item, -1, "Yes  No!") &&
		       write_bytes_text(r, 0x182, &no_text, 1) &&
		       write_text(r, 0x183, "Lucky! Thanks! Well, here's your {%s}!", item, -1, NULL);
	case LADX_ITEM_TRADING_ITEM_BANANAS:
		return write_text(r, 0x1CA,
				  "MUNCH MUNCH!! ... ... ... ... That was great! I know it's not a fair trade, but "
				  "here's some {%s}! YUM...",
				  item, -1, NULL);
	case LADX_ITEM_TRADING_ITEM_PINEAPPLE:
		return write_text(r, 0x1CF,
				  "Hi ho! Hey you! Is that possibly a <honeycomb> you have? I just ran out! Will you "
				  "swap it for a {%s}?",
				  item, -1, "Yes  No");
	case LADX_ITEM_TRADING_ITEM_HIBISCUS:
		return write_text(r, 0x173, "AH! This isn't meant to be a reward... Here, take this {%s}!", item, -1,
				  NULL);
	case LADX_ITEM_TRADING_ITEM_LETTER:
		return write_text(r, 0x168, "I would like you to take this {%s}, please!", item, -1, NULL);
	case LADX_ITEM_TRADING_ITEM_BROOM:
		return write_text(r, 0x135,
				  "Mmm... She's so beautiful... I must give you something for your trouble... Hmm...  "
				  "Well, it looks like all I have is this {%s}... how'll that be?",
				  item, -1, "Fine No...");
	case LADX_ITEM_TRADING_ITEM_FISHING_HOOK:
		return write_text(r, 0x15D,
				  "Okay! In return you can have this {%s} I found when I swept by the river bank!", item,
				  -1, NULL);
	default:
		return 1;
	}
}

static int witch(Run *r, int spot, int item)
{
	int options[1], id;

	if (ladx_logic_spot_options(r->logic, spot, options, 0) == 1)
		return 1;
	return chest_id(r, item, &id) && patch_byte(r, WITCH_ITEM, 0x09, id);
}

static int boomerang_guy(Run *r, int item)
{
	int i, inv, id;

	if (r->p->settings->value[LADX_SETTING_BOOMERANG] == ladx_setting_value_find(LADX_SETTING_BOOMERANG, "trade")) {
		if ((inv = ladx_items_inventory_id(item)) < 0)
			return raise_error(r, "KeyError", "'%s'", ladx_item_name(item) ? ladx_item_name(item) : "None");
		for (i = 0; i < 4; i++)
			if (!operand_patch(r, UNIT_TRADE, BANK(0x19) + trade_operands[i], inv))
				return 0;
		return write_text(r, 0x222, "Okay, let's do it!", LADX_ITEM_NONE, -1, NULL) &&
		       write_text(r, 0x224, "You got the {%s} in exchange for the item you had.", item, -1, NULL) &&
		       write_text(r, 0x225, "Give me back my {%s}, I beg you! I'll return the item you gave me", item,
				  -1, "Okay Not Now") &&
		       write_text(r, 0x226, "The item came back to you. You returned the other item.", LADX_ITEM_NONE,
				  -1, NULL);
	}
	return write_text(r, 0x221, "I found a good item washed up on the beach... Want to have it?",
			  LADX_ITEM_NONE, -1, "Okay No") &&
	       ladx_pipeline_const_units(r->enc, r->p, "items", UNIT_GIFT, 1) && chest_id(r, item, &id) &&
	       patch_byte(r, GIFT_ITEM, 0x00, id) &&
	       write_text(r, 0x225, "It's a secret to everybody.", LADX_ITEM_NONE, -1, NULL);
}

static int patch_spot(Run *r, int i)
{
	const LadxSpot *spot = ladx_logic_spot(r->logic, i);
	int item = spot->item;

	switch (spot->kind) {
	case LADX_SPOT_KEY_LOCATION:
		return 1;
	case LADX_SPOT_DUNGEON_CHEST:
		return chest(r, spot, ladx_items_dungeon_item(item, dungeon_of(r, spot)));
	case LADX_SPOT_CHEST:
		return chest(r, spot, item);
	case LADX_SPOT_ANGLER_KEY:
		return angler_key(r, spot, item);
	case LADX_SPOT_BEACH_SWORD:
		return beach_sword(r, spot, item);
	case LADX_SPOT_START_ITEM:
		return start_item(r, spot, item);
	case LADX_SPOT_BIRD_KEY:
	case LADX_SPOT_DROPPED_KEY:
	case LADX_SPOT_FACE_KEY:
	case LADX_SPOT_FISHING_MINIGAME:
	case LADX_SPOT_GOLD_LEAF:
	case LADX_SPOT_HEART_CONTAINER:
	case LADX_SPOT_HEART_PIECE:
	case LADX_SPOT_HOOKSHOT_DROP:
	case LADX_SPOT_INSTRUMENT:
	case LADX_SPOT_SEASHELL:
	case LADX_SPOT_SEASHELL_MANSION:
	case LADX_SPOT_SLIME_KEY:
	case LADX_SPOT_SONG:
	case LADX_SPOT_TOADSTOOL:
		return dropped(r, spot, item);
	case LADX_SPOT_KEY_HOLE:
		return keyhole(r, spot, item);
	case LADX_SPOT_OWL_STATUE:
		return owl_statue(r, spot, item);
	case LADX_SPOT_MAD_BATTER: {
		int id;

		return chest_id(r, item, &id) && put_byte(r, BATTER_TABLE + (uint32_t)(spot->room & 0x0F), id);
	}
	case LADX_SPOT_SEASHELL_MANSION_BONUS: {
		int id;

		return chest_id(r, item, &id) && put_byte(r, BONUS_TABLE + (uint32_t)spot->index, id);
	}
	case LADX_SPOT_TUNIC_FAIRY: {
		int id;

		return chest_id(r, item, &id) && put_byte(r, FAIRY_TABLE + (uint32_t)spot->index, id);
	}
	case LADX_SPOT_SHOP_ITEM:
		return shop_item(r, spot, item);
	case LADX_SPOT_TRADE_SEQUENCE_ITEM:
		return trade_item(r, spot, item);
	case LADX_SPOT_WITCH:
		return witch(r, i, item);
	case LADX_SPOT_BOOMERANG_GUY:
		return boomerang_guy(r, item);
	default:
		return 0;
	}
}

int ladx_group_items(LadxEncoder *enc, LadxPipeline *p)
{
	const LadxWorldSetup *ws;
	Run r;
	int i, count, id;

	if (!p->rom || !p->randomizer || !(ws = ladx_randomizer_world_setup(p->randomizer)) ||
	    !(r.logic = ladx_randomizer_placement(p->randomizer)) ||
	    ladx_pipeline_const_unit_count(p, "items") != UNIT_TOTAL)
		return 0;
	r.enc = enc;
	r.p = p;
	if (!chest_id(&r, ws->multichest, &id) || !put_byte(&r, CHEST_TABLE + MULTICHEST_ROOM, id))
		return 0;
	count = ladx_logic_spot_count(r.logic);
	for (i = 0; i < count; i++)
		if (!patch_spot(&r, i))
			return 0;
	return 1;
}
