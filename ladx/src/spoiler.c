#include "ladx_spoiler.h"

#include <stdio.h>
#include <string.h>


#define BANK(b) ((uint32_t)(b) * LADX_ROM_BANK)
#define RACE_BYTE (BANK(0x00) + 0x0007u)
#define SEED_AT (BANK(0x3E) + 0x2F00u)
#define CHEST_TABLE (BANK(0x14) + 0x560u)
#define DROP_TABLE (BANK(0x3E) + 0x3800u)
#define OWL_TABLE (BANK(0x3E) + 0x3B16u)
#define BONUS_TABLE (BANK(0x3E) + 0x3E30u)
#define BATTER_TABLE (BANK(0x18) + 0x0F90u)
#define FAIRY_TABLE (BANK(0x36) + 0x11BFu)
#define WITCH_FLAG (BANK(0x05) + 0x08EFu)
#define WITCH_ITEM (BANK(0x05) + 0x08D5u)
#define TRADE_FLAG (BANK(0x19) + 0x06C5u)
#define TRADE_INVENTORY (BANK(0x19) + 0x0640u)
#define GIFT_ITEM (BANK(0x19) + 0x072Bu)
#define GEL_DISGUISE_MIN 0x51
#define GEL_DISGUISE_MAX 0x57
#define BEACH_ROOM 0x0F2
#define BEACH_SWORD_ENTITY 0x31
#define OVERWORLD_PROBE_ROOM 0x010
#define FISH_ROOM 0x1EF
#define EAGLE_ROOM 0x2F8
#define NIGHTMARE_ENTITY 0x13

static const LadxEntrancePos entrance_pos[LADX_ENTRANCE_INFO_COUNT] = {
	{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{104, 18}, {136, 18}, {-1, -1}, {24, 112}, {72, 34}, {120, 112}, {24, 48}, {120, 80},
	{-1, -1}, {56, 16}, {40, 16}, {120, 16}, {88, 64}, {120, 16}, {-1, -1}, {56, 80},
	{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {72, 34}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {24, 48}, {88, 64}, {-1, -1}, {-1, -1}, {-1, -1}, {88, 82}, {120, 82},
	{-1, -1}, {-1, -1}, {-1, -1}, {56, 66}, {88, 66}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {-1, -1}, {120, 96}, {-1, -1}, {88, 48}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1}, {40, 80}, {120, 80}, {40, 80}, {88, 80}, {-1, -1}, {136, 32}, {-1, -1},
};

/* BOSS_ENTITIES' values in dict order: 0 to 8 (3 and 6 None), then the five nightmares. */
#define BOSS_VALUES 14
static const struct {
	int key; /* -1: None; 9: a nightmare key */
	int x, y, type;
} boss_values[BOSS_VALUES] = {
	{0, 3, 2, 0x59}, {1, 4, 2, 0x5C}, {2, 4, 3, 0x5B}, {-1, 0, 0, 0}, {4, 4, 3, 0x5D},
	{5, 4, 3, 0x5A}, {-1, 0, 0, 0}, {7, 4, 3, 0x62}, {8, 5, 2, 0xF9}, {9, 4, 3, NIGHTMARE_ENTITY},
	{9, 4, 3, NIGHTMARE_ENTITY}, {9, 4, 3, NIGHTMARE_ENTITY}, {9, 4, 3, NIGHTMARE_ENTITY},
	{9, 4, 3, NIGHTMARE_ENTITY},
};

static const int boss_rooms[9] = {0x106, 0x12b, 0x15a, 0x166, 0x185, 0x1bc, 0x223, 0x234, 0x300};

/* MINIBOSS_ENTITIES' last entity types in dict order: LadxMiniboss's, then the five nightmares. */
#define MINIBOSS_VALUES (LADX_MINIBOSS_COUNT + 5)
static const int miniboss_last_type[MINIBOSS_VALUES] = {
	0x82, 0x89, 0x60, 0x8e, 0x5e, 0x92, 0xbc, 0xbe, 0xf4, 0xf8, 0xe4, 0x88, 0x13, 0x13, 0x13, 0x13, 0x13,
};

const LadxEntrancePos *ladx_entrance_pos(size_t *count)
{
	*count = LADX_ENTRANCE_INFO_COUNT;
	return entrance_pos;
}

static LadxSpoilerStatus raise_crash(LadxOutcome *raised, const char *type, const char *message)
{
	raised->kind = LADX_OUTCOME_CRASH;
	raised->type = type;
	snprintf(raised->text, sizeof raised->text, "%s", message);
	raised->message = raised->text;
	return LADX_SPOILER_RAISED;
}

static int byte_at(const LadxEncoder *enc, uint32_t offset, int *value)
{
	uint8_t v;
	int32_t origin;

	if (!ladx_enc_bank_value(enc, offset, &v, &origin))
		return 0;
	*value = v;
	return 1;
}

static int chest_item(int value)
{
	int item;

	for (item = 0; item < LADX_ITEM_COUNT; item++)
		if (ladx_items_chest_id(item) == value)
			return item;
	return LADX_ITEM_NONE;
}

static int inventory_item(int value)
{
	int item;

	for (item = 0; item < LADX_ITEM_COUNT; item++)
		if (ladx_items_inventory_id(item) == value)
			return item;
	return LADX_ITEM_NONE;
}

static int is_dungeon_family(int item)
{
	return item == LADX_ITEM_MAP || item == LADX_ITEM_COMPASS || item == LADX_ITEM_STONE_BEAK ||
	       item == LADX_ITEM_NIGHTMARE_KEY || item == LADX_ITEM_KEY;
}

/* "%s%d" % (item, dungeon) as an item. */
static int numbered(int item, int dungeon)
{
	char name[32];

	snprintf(name, sizeof name, "%s%d", ladx_item_name(item), dungeon);
	return ladx_item_find(name);
}

static int spot_dungeon(const LadxSpoiler *sp, const LadxSpot *spot)
{
	const LadxLocation *loc = ladx_logic_location(&sp->logic, spot->location);

	return loc ? loc->dungeon : -1;
}

typedef enum Lookup { PLAIN, GEL_RULE, FAMILY } Lookup;

/* CHEST_ITEMS' key of the byte at offset, or LADXR's ValueError (what names the class's message). */
static LadxSpoilerStatus chest_lookup(const LadxSpoiler *sp, const LadxEncoder *enc, const LadxSpot *spot,
				      uint32_t offset, Lookup rule, const char *what, int *item, LadxOutcome *raised)
{
	char text[LADX_OUTCOME_TEXT_MAX];
	int value, found;

	if (!byte_at(enc, offset, &value))
		return LADX_SPOILER_FAILED;
	if (rule != PLAIN && value >= GEL_DISGUISE_MIN && value < GEL_DISGUISE_MAX) {
		*item = LADX_ITEM_GEL;
		return LADX_SPOILER_OK;
	}
	found = chest_item(value);
	if (found == LADX_ITEM_NONE) {
		snprintf(text, sizeof text, "Could not find %s contents in ROM (0x%02x)", what, (unsigned)value);
		return raise_crash(raised, "ValueError", text);
	}
	if (rule == FAMILY && is_dungeon_family(found)) {
		int dungeon = spot_dungeon(sp, spot);

		if (dungeon < 0) {
			snprintf(text, sizeof text, "Dungeon item outside of dungeon? %s:%03x",
				 ladx_spot_kind_name(spot->kind), (unsigned)spot->room);
			return raise_crash(raised, "AssertionError", text);
		}
		found = numbered(found, dungeon);
		if (found == LADX_ITEM_NONE)
			return LADX_SPOILER_FAILED;
	}
	*item = found;
	return LADX_SPOILER_OK;
}

LadxSpoilerStatus ladx_spoiler_read_item(const LadxSpoiler *sp, const LadxEncoder *enc, int index, LadxRoom *room,
					 int *item, LadxOutcome *raised)
{
	const LadxSpot *spot = ladx_logic_spot(&sp->logic, index);
	uint32_t room_at;
	LadxSpoilerStatus st;
	int value, found;

	if (!spot)
		return LADX_SPOILER_FAILED;
	room_at = (uint32_t)spot->room;
	switch (spot->kind) {
	case LADX_SPOT_KEY_LOCATION:
		*item = spot->default_item;
		return LADX_SPOILER_OK;
	case LADX_SPOT_CHEST:
		return chest_lookup(sp, enc, spot, CHEST_TABLE + room_at, PLAIN, "chest", item, raised);
	case LADX_SPOT_DUNGEON_CHEST: {
		int dungeon = spot_dungeon(sp, spot);

		st = chest_lookup(sp, enc, spot, CHEST_TABLE + room_at, PLAIN, "chest", item, raised);
		if (st != LADX_SPOILER_OK || !is_dungeon_family(*item))
			return st;
		if (dungeon < 0 || (*item = numbered(*item, dungeon)) == LADX_ITEM_NONE)
			return LADX_SPOILER_FAILED;
		return LADX_SPOILER_OK;
	}
	case LADX_SPOT_BEACH_SWORD:
		if (ladx_room_load(room, enc, BEACH_ROOM, NULL) != LADX_ROOM_OK)
			return LADX_SPOILER_FAILED;
		if (ladx_room_has_entity(room, BEACH_SWORD_ENTITY)) {
			*item = LADX_ITEM_SWORD;
			return LADX_SPOILER_OK;
		}
		return chest_lookup(sp, enc, spot, DROP_TABLE + room_at, FAMILY, "chest", item, raised);
	case LADX_SPOT_TOADSTOOL:
		if (ladx_logic_spot_options(&sp->logic, index, NULL, 0) == 1) {
			*item = LADX_ITEM_TOADSTOOL;
			return LADX_SPOILER_OK;
		}
		return chest_lookup(sp, enc, spot, DROP_TABLE + room_at, FAMILY, "chest", item, raised);
	case LADX_SPOT_ANGLER_KEY:
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
	case LADX_SPOT_START_ITEM:
		return chest_lookup(sp, enc, spot, DROP_TABLE + room_at, FAMILY, "chest", item, raised);
	case LADX_SPOT_KEY_HOLE:
		return chest_lookup(sp, enc, spot, OWL_TABLE + room_at, GEL_RULE, "chest", item, raised);
	case LADX_SPOT_OWL_STATUE:
		/* OwlStatue's read has no gel rule, only the dungeon families. */
		st = chest_lookup(sp, enc, spot, OWL_TABLE + room_at, PLAIN, "owl statue", item, raised);
		if (st != LADX_SPOILER_OK || !is_dungeon_family(*item))
			return st;
		if (spot_dungeon(sp, spot) < 0) {
			char text[LADX_OUTCOME_TEXT_MAX];

			snprintf(text, sizeof text, "Dungeon item outside of dungeon? %s:%03x",
				 ladx_spot_kind_name(spot->kind), (unsigned)spot->room);
			return raise_crash(raised, "AssertionError", text);
		}
		*item = numbered(*item, spot_dungeon(sp, spot));
		return *item == LADX_ITEM_NONE ? LADX_SPOILER_FAILED : LADX_SPOILER_OK;
	case LADX_SPOT_TRADE_SEQUENCE_ITEM:
		return chest_lookup(sp, enc, spot, OWL_TABLE + room_at, PLAIN, "owl statue", item, raised);
	case LADX_SPOT_MAD_BATTER:
		return chest_lookup(sp, enc, spot, BATTER_TABLE + (room_at & 0x0Fu), PLAIN, "mad batter", item,
				    raised);
	case LADX_SPOT_SEASHELL_MANSION_BONUS:
		return chest_lookup(sp, enc, spot, BONUS_TABLE + (uint32_t)spot->index, PLAIN, "bonus seashell", item,
				    raised);
	case LADX_SPOT_SHOP_ITEM:
		return chest_lookup(sp, enc, spot, spot->index == 1 ? CHEST_TABLE + room_at : DROP_TABLE + room_at,
				    PLAIN, "shop item", item, raised);
	case LADX_SPOT_TUNIC_FAIRY:
		return chest_lookup(sp, enc, spot, FAIRY_TABLE + (uint32_t)spot->index, PLAIN, "tunic fairy", item,
				    raised);
	case LADX_SPOT_WITCH:
		if (!byte_at(enc, WITCH_FLAG, &value))
			return LADX_SPOILER_FAILED;
		if (value != 0) {
			*item = LADX_ITEM_MAGIC_POWDER;
			return LADX_SPOILER_OK;
		}
		return chest_lookup(sp, enc, spot, WITCH_ITEM, PLAIN, "witch", item, raised);
	case LADX_SPOT_BOOMERANG_GUY:
		if (!byte_at(enc, TRADE_FLAG, &value))
			return LADX_SPOILER_FAILED;
		if (value == 0) {
			if (!byte_at(enc, GIFT_ITEM, &value))
				return LADX_SPOILER_FAILED;
			found = chest_item(value);
		} else {
			if (!byte_at(enc, TRADE_INVENTORY, &value))
				return LADX_SPOILER_FAILED;
			found = inventory_item(value);
		}
		if (found == LADX_ITEM_NONE)
			return raise_crash(raised, "ValueError", "");
		*item = found;
		return LADX_SPOILER_OK;
	default:
		return LADX_SPOILER_FAILED;
	}
}

static LadxSpoilerStatus load(LadxRoom *room, const LadxEncoder *enc, int index, const char *alt)
{
	return ladx_room_load(room, enc, index, alt) == LADX_ROOM_OK ? LADX_SPOILER_OK : LADX_SPOILER_FAILED;
}

static const LadxRoomObject *last_object(const LadxRoom *room)
{
	return room->object_count ? &room->objects[room->object_count - 1] : NULL;
}

LadxSpoilerStatus ladx_spoiler_read_bosses(const LadxEncoder *enc, LadxRoom *room, int *mapping, LadxOutcome *raised)
{
	char text[LADX_OUTCOME_TEXT_MAX];
	int d, k;

	for (d = 0; d < 9; d++) {
		if (load(room, enc, boss_rooms[d], NULL) != LADX_SPOILER_OK)
			return LADX_SPOILER_FAILED;
		if (room->entity_count) {
			const LadxRoomEntity *e = &room->entities[0];

			for (k = 0; k < BOSS_VALUES; k++)
				if (boss_values[k].key >= 0 && boss_values[k].x == e->x && boss_values[k].y == e->y &&
				    boss_values[k].type == e->type_id)
					break;
			if (k == BOSS_VALUES) {
				snprintf(text, sizeof text, "(%d, %d, %d) is not in list", e->x, e->y, e->type_id);
				return raise_crash(raised, "ValueError", text);
			}
			if (boss_values[k].key > 8)
				return LADX_SPOILER_FAILED;
			mapping[d] = boss_values[k].key;
		} else {
			const LadxRoomObject *o = last_object(room);

			if (!o)
				return raise_crash(raised, "IndexError", "list index out of range");
			if (o->kind == LADX_ROOM_WARP && o->room == FISH_ROOM)
				mapping[d] = 3;
			else if (o->kind == LADX_ROOM_WARP && o->room == EAGLE_ROOM)
				mapping[d] = 6;
			else
				mapping[d] = d;
		}
	}
	return LADX_SPOILER_OK;
}

LadxSpoilerStatus ladx_spoiler_read_minibosses(const LadxEncoder *enc, LadxRoom *room, LadxMiniboss *mapping,
					       LadxOutcome *raised)
{
	int key, k, found;

	for (key = 0; key < LADX_MINIBOSS_KEY_COUNT; key++) {
		if (load(room, enc, ladx_miniboss_room(key), NULL) != LADX_SPOILER_OK)
			return LADX_SPOILER_FAILED;
		if (!room->entity_count)
			return raise_crash(raised, "IndexError", "list index out of range");
		found = -1;
		for (k = 0; k < MINIBOSS_VALUES; k++)
			if (miniboss_last_type[k] == room->entities[room->entity_count - 1].type_id)
				found = k;
		if (found < 0 || found >= LADX_MINIBOSS_COUNT)
			return LADX_SPOILER_FAILED;
		mapping[key] = (LadxMiniboss)found;
	}
	return LADX_SPOILER_OK;
}

static const LadxRoomObject *nth_warp(const LadxRoom *r, size_t n)
{
	size_t i;

	for (i = 0; i < r->object_count; i++)
		if (r->objects[i].kind == LADX_ROOM_WARP && n-- == 0)
			return &r->objects[i];
	return NULL;
}

/* The two assignments of readEntrances' inner loop for warp w into mapping[slot]. */
static void match_warp(const LadxRoomObject *w, const LadxEntranceInfo *info, const LadxEntrancePos *pos,
		       size_t count, int *slot)
{
	size_t o;

	for (o = 0; o < count; o++) {
		if (w->room == info[o].target)
			*slot = (int)(count + o);
		if (w->room == info[o].room &&
		    (pos[o].x < 0 || (pos[o].x == w->target_x && pos[o].y == w->target_y)))
			*slot = (int)o;
	}
}

LadxSpoilerStatus ladx_spoiler_read_entrances(const LadxEncoder *enc, LadxRoom *room, const LadxEntranceInfo *info,
					      const LadxEntrancePos *pos, size_t count, int *mapping)
{
	const LadxRoomObject *w;
	size_t k;
	int index;

	if (count > LADX_ENTRANCES_MAX)
		return LADX_SPOILER_FAILED;
	for (k = 0; k < 2 * count; k++)
		mapping[k] = -1;
	for (k = 0; k < count; k++) {
		if (load(room, enc, info[k].alt_room ? -1 : info[k].room, info[k].alt_room) != LADX_SPOILER_OK)
			return LADX_SPOILER_FAILED;
		index = info[k].index >= 0 ? info[k].index : 0;
		w = nth_warp(room, (size_t)index);
		if (!w)
			continue;
		match_warp(w, info, pos, count, &mapping[k]);
		if (load(room, enc, info[k].target, NULL) != LADX_SPOILER_OK)
			return LADX_SPOILER_FAILED;
		w = nth_warp(room, 0);
		if (w)
			match_warp(w, info, pos, count, &mapping[count + k]);
	}
	return LADX_SPOILER_OK;
}

LadxSpoilerStatus ladx_spoiler_begin(LadxSpoiler *sp, const LadxSettings *validated, const LadxWorldSetup *ws,
				     const char *seed_hex)
{
	sp->settings = *validated;
	sp->ws = *ws;
	snprintf(sp->seed, sizeof sp->seed, "%s", seed_hex);
	sp->accessible_count = sp->inaccessible_count = 0;
	return ladx_logic_build(&sp->logic, validated, &sp->ws) == LADX_LOGIC_OK ? LADX_SPOILER_OK
										 : LADX_SPOILER_FAILED;
}

LadxSpoilerStatus ladx_spoiler_spheres(LadxSpoiler *sp, const int *items)
{
	const LadxLocation *loc;
	int spots = ladx_logic_spot_count(&sp->logic), locations = ladx_logic_location_count(&sp->logic);
	int i, j, remaining = spots, sphere = 0, any, reached;

	for (i = 0; i < spots; i++) {
		sp->item[i] = items[i];
		sp->sphere[i] = -1;
		sp->remaining[i] = 1;
		if (!ladx_logic_place(&sp->logic, i, LADX_ITEM_NONE))
			return LADX_SPOILER_FAILED;
	}
	memset(sp->last, 0, sizeof sp->last);
	ladx_explorer_init(&sp->explorer);
	while (remaining) {
		ladx_explorer_init(&sp->explorer);
		if (!ladx_explorer_visit(&sp->explorer, &sp->logic, ladx_logic_start(&sp->logic)))
			return LADX_SPOILER_FAILED;
		any = 0;
		for (i = 0; i < locations; i++) {
			if (!ladx_explorer_reached(&sp->explorer, i) || sp->last[i])
				continue;
			any = 1;
			loc = ladx_logic_location(&sp->logic, i);
			for (j = loc->spot_first; j < loc->spot_first + loc->spot_count; j++) {
				sp->sphere[j] = sphere;
				if (!ladx_logic_place(&sp->logic, j, sp->item[j]))
					return LADX_SPOILER_FAILED;
				if (sp->remaining[j]) {
					sp->remaining[j] = 0;
					remaining--;
				}
			}
		}
		if (!any)
			break;
		for (i = 0; i < locations; i++)
			sp->last[i] = (char)ladx_explorer_reached(&sp->explorer, i);
		sphere++;
	}
	sp->accessible_count = sp->inaccessible_count = 0;
	reached = 0;
	for (i = 0; i < locations; i++) {
		int in = ladx_explorer_reached(&sp->explorer, i);

		loc = ladx_logic_location(&sp->logic, i);
		reached += in;
		for (j = loc->spot_first; j < loc->spot_first + loc->spot_count; j++)
			if (in)
				sp->accessible[sp->accessible_count++] = j;
	}
	if (reached != locations)
		for (i = 0; i < locations; i++) {
			if (ladx_explorer_reached(&sp->explorer, i))
				continue;
			loc = ladx_logic_location(&sp->logic, i);
			for (j = loc->spot_first; j < loc->spot_first + loc->spot_count; j++)
				sp->inaccessible[sp->inaccessible_count++] = j;
		}
	return LADX_SPOILER_OK;
}

static LadxSpoilerStatus read_world_setup(LadxSpoiler *sp, const LadxEncoder *enc, LadxRoom *room,
					  LadxOutcome *raised)
{
	const LadxEntranceInfo *info;
	const LadxEntrancePos *pos;
	LadxSpoilerStatus st;
	size_t count, k;

	ladx_world_setup_defaults(&sp->ws);
	if (load(room, enc, OVERWORLD_PROBE_ROOM, NULL) != LADX_SPOILER_OK || !nth_warp(room, 0))
		return LADX_SPOILER_FAILED;
	info = ladx_entrance_info(&count);
	pos = ladx_entrance_pos(&count);
	if ((st = ladx_spoiler_read_entrances(enc, room, info, pos, count, sp->mapping)) != LADX_SPOILER_OK)
		return st;
	/* C's logic takes the identity mapping alone; LADXR's read-back dict equals it in another key order. */
	for (k = 0; k < count; k++)
		if (sp->mapping[k] != (int)(count + k) || sp->mapping[count + k] != (int)k)
			return LADX_SPOILER_FAILED;
	if ((st = ladx_spoiler_read_bosses(enc, room, sp->ws.boss_mapping, raised)) != LADX_SPOILER_OK)
		return st;
	return ladx_spoiler_read_minibosses(enc, room, sp->ws.miniboss_mapping, raised);
}

LadxSpoilerStatus ladx_spoiler_build(LadxSpoiler *sp, const LadxEncoder *enc, const LadxSettings *validated,
				     LadxRoom *room, LadxOutcome *raised)
{
	static const char hex[] = "0123456789ABCDEF";
	char seed[LADX_SPOILER_SEED_HEX + 1];
	LadxWorldSetup ws;
	LadxSpoilerStatus st;
	int i, value, items[LADX_LOGIC_SPOT_CAP];

	sp->built = 0;
	if (validated->value[LADX_SETTING_RACE])
		return LADX_SPOILER_NONE;
	if (!byte_at(enc, RACE_BYTE, &value))
		return LADX_SPOILER_FAILED;
	if (value == 0x01)
		return raise_crash(raised, "RaceRomException", "");
	for (i = 0; i < LADX_SPOILER_SEED_HEX / 2; i++) {
		if (!byte_at(enc, SEED_AT + (uint32_t)i, &value))
			return LADX_SPOILER_FAILED;
		seed[2 * i] = hex[value >> 4];
		seed[2 * i + 1] = hex[value & 0x0F];
	}
	seed[LADX_SPOILER_SEED_HEX] = '\0';
	memcpy(sp->seed, seed, sizeof seed);
	if ((st = read_world_setup(sp, enc, room, raised)) != LADX_SPOILER_OK)
		return st;
	ws = sp->ws;
	if ((st = ladx_spoiler_begin(sp, validated, &ws, seed)) != LADX_SPOILER_OK)
		return st;
	for (i = 0; i < ladx_logic_spot_count(&sp->logic); i++)
		if ((st = ladx_spoiler_read_item(sp, enc, i, room, &items[i], raised)) != LADX_SPOILER_OK)
			return st;
	if ((st = ladx_spoiler_spheres(sp, items)) != LADX_SPOILER_OK)
		return st;
	sp->built = 1;
	return LADX_SPOILER_OK;
}

int ladx_group_spoiler(LadxEncoder *enc, LadxPipeline *p)
{
	if (!p->spoiler)
		return 1;
	switch (ladx_spoiler_build(p->spoiler, enc, p->settings, &p->room, &p->raised)) {
	case LADX_SPOILER_OK:
	case LADX_SPOILER_NONE:
		return 1;
	default:
		return 0;
	}
}

/* --- outputJson ------------------------------------------------------------------------------ */

typedef struct Out {
	LadxSpoilerSink sink;
	void *ctx;
	int ok;
} Out;

static void put(Out *o, const char *s)
{
	o->sink(o->ctx, s, strlen(s));
}

/* json.dumps' string with ensure_ascii: a non-ASCII byte is refused. */
static void put_string(Out *o, const char *s)
{
	char esc[8];

	put(o, "\"");
	for (; s && *s; s++) {
		unsigned char c = (unsigned char)*s;

		if (c >= 0x80) {
			o->ok = 0;
			continue;
		}
		switch (c) {
		case '"':
			put(o, "\\\"");
			break;
		case '\\':
			put(o, "\\\\");
			break;
		case '\n':
			put(o, "\\n");
			break;
		case '\r':
			put(o, "\\r");
			break;
		case '\t':
			put(o, "\\t");
			break;
		case '\b':
			put(o, "\\b");
			break;
		case '\f':
			put(o, "\\f");
			break;
		default:
			if (c < 0x20) {
				snprintf(esc, sizeof esc, "\\u%04x", c);
				put(o, esc);
			} else {
				o->sink(o->ctx, (const char *)s, 1);
			}
		}
	}
	put(o, "\"");
}

static void put_key(Out *o, const char *indent, const char *key)
{
	put(o, indent);
	put_string(o, key);
	put(o, ": ");
}

static void put_items(Out *o, const LadxSpoiler *sp, const char *key, const int *spots, int n, int last)
{
	char number[16];
	int i;

	put_key(o, "  ", key);
	if (!n) {
		put(o, last ? "[]\n" : "[],\n");
		return;
	}
	put(o, "[\n");
	for (i = 0; i < n; i++) {
		const LadxSpot *spot = ladx_logic_spot(&sp->logic, spots[i]);

		put(o, "    {\n");
		put_key(o, "      ", "id");
		put_string(o, spot->name_id);
		put(o, ",\n");
		put_key(o, "      ", "area");
		put_string(o, ladx_spot_metadata_area(spot));
		put(o, ",\n");
		put_key(o, "      ", "locationName");
		put_string(o, ladx_spot_metadata_name(spot));
		put(o, ",\n");
		put_key(o, "      ", "sphere");
		if (sp->sphere[spots[i]] < 0) {
			put(o, "null");
		} else {
			snprintf(number, sizeof number, "%d", sp->sphere[spots[i]]);
			put(o, number);
		}
		put(o, ",\n");
		put_key(o, "      ", "itemName");
		put_string(o, ladx_item_name(sp->item[spots[i]]));
		put(o, i + 1 < n ? "\n    },\n" : "\n    }\n");
	}
	put(o, last ? "  ]\n" : "  ],\n");
}

static void put_options(Out *o, const LadxSettings *s)
{
	int id;

	put_key(o, "  ", "options");
	put(o, "{\n");
	put_key(o, "    ", "seed");
	put(o, "\"\",\n");
	for (id = 0; id < LADX_SETTING_COUNT; id++) {
		put_key(o, "    ", ladx_setting_key((LadxSettingId)id));
		if (ladx_setting_is_bool((LadxSettingId)id))
			put(o, s->value[id] ? "true" : "false");
		else
			put_string(o, ladx_setting_value_name((LadxSettingId)id, s->value[id]));
		put(o, id + 1 < LADX_SETTING_COUNT ? ",\n" : "\n");
		if (id == LADX_SETTING_LOGIC) {
			put_key(o, "    ", "forwardfactor");
			put(o, "0.0,\n");
		}
	}
	put(o, "  },\n");
}

int ladx_spoiler_render(const LadxSpoiler *sp, LadxSpoilerSink sink, void *ctx)
{
	Out o;

	o.sink = sink;
	o.ctx = ctx;
	o.ok = 1;
	put(&o, "{\n");
	put_items(&o, sp, "accessibleItems", sp->accessible, sp->accessible_count, 0);
	put_items(&o, sp, "inaccessibleItems", sp->inaccessible, sp->inaccessible_count, 0);
	put_options(&o, &sp->settings);
	put_key(&o, "  ", "entrances");
	put(&o, "{},\n");
	put_key(&o, "  ", "seed");
	put_string(&o, sp->seed);
	put(&o, "\n}");
	return o.ok;
}
