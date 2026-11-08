#include "ladx_groups.h"
#include "ladx_items.h"

#include <stdio.h>
#include <string.h>

#define BANK(b) ((uint32_t)(b) * LADX_ROM_BANK)
#define ROOM_EVENTS BANK(0x14)		 /* + room - 0x100 */
#define INDOOR_TILESET (BANK(0x20) + 0x2EB3u) /* + room - 0x100 */
#define COLOR_WARPS (BANK(0x14) + 0x04E0u)
#define COLOR_WARP_COUNT 64
#define DROP_TABLE (BANK(0x3E) + 0x3800u)
#define BOSS_ITEM (BANK(0x3E) + 0x3300u)
#define GENIE (BANK(0x04) + 0x0521u)
#define EAGLE_CHECK (BANK(0x02) + 0x1FC8u)
#define FISH_FLAG (BANK(0x03) + 0x1A0Fu)
#define EAGLE_FLAG (BANK(0x03) + 0x1A04u)
#define GHOMA_COLLISION (BANK(0x03) + 0x034Fu)
#define ROLLING_BONES_FLAGS (BANK(0x03) + 0x02F1u + 0x81u)
#define FISH 3
#define EAGLE 6
#define SLIME_EEL 4
#define FACADE 5
#define FISH_ROOM 0x1EF
#define EAGLE_ROOM 0x2F8
#define D7_TOP 0x22E
#define D7_NEXT 0x22C
#define WARP_ENTITY 0x61
#define BOSSES_UNITS 3

static const uint32_t facade_at[3] = {BANK(0x04) + 0x121Du, BANK(0x04) + 0x1226u, BANK(0x04) + 0x127Fu};

static const int boss_rooms[LADX_BOSS_ROOM_COUNT] = {0x106, 0x12b, 0x15a, 0x166, 0x185, 0x1bc, 0x223, 0x234, 0x300};

static const LadxRoomEntity boss_entities[LADX_BOSS_ROOM_COUNT] = {
	{3, 2, 0x59}, {4, 2, 0x5C}, {4, 3, 0x5B}, {0, 0, -1}, {4, 3, 0x5D},
	{4, 3, 0x5A}, {0, 0, -1},   {4, 3, 0x62}, {5, 2, 0xF9},
};

static const int miniboss_rooms[LADX_MINIBOSS_KEY_COUNT] = {0x111, 0x128, 0x145, 0x164, 0x193, 0x1C5,
							   0x228, 0x23F, 0x30C, 0x303, 0x2E1, 0x27F};

static const struct {
	int n;
	LadxRoomEntity e[2];
} miniboss_entities[LADX_MINIBOSS_COUNT] = {
	{2, {{8, 3, 0x81}, {6, 3, 0x82}}}, {1, {{5, 2, 0x89}}}, {2, {{3, 2, 0x60}, {5, 2, 0x60}}},
	{1, {{1, 1, 0x8e}}},		   {2, {{2, 1, 0x5e}, {2, 4, 0x5e}}}, {1, {{5, 2, 0x92}}},
	{1, {{4, 0, 0xbc}}},		   {1, {{5, 3, 0xbe}}},		   {1, {{5, 1, 0xf4}}},
	{1, {{4, 2, 0xf8}}},		   {1, {{5, 5, 0xe4}}},		   {1, {{4, 3, 0x88}}},
};

int ladx_boss_room(int dungeon)
{
	return dungeon >= 0 && dungeon < LADX_BOSS_ROOM_COUNT ? boss_rooms[dungeon] : -1;
}

int ladx_miniboss_room(int key)
{
	return key >= 0 && key < LADX_MINIBOSS_KEY_COUNT ? miniboss_rooms[key] : -1;
}

typedef struct Run {
	LadxEncoder *enc;
	LadxSpan group;
	LadxRoom *room;
	LadxOutcome *raised;
} Run;

static LadxBossesStatus raise_error(Run *r, const char *type, const char *message)
{
	r->raised->kind = LADX_OUTCOME_CRASH;
	r->raised->type = type;
	snprintf(r->raised->text, sizeof r->raised->text, "%s", message);
	r->raised->message = r->raised->text;
	return LADX_BOSSES_RAISED;
}

static LadxBossesStatus put(Run *r, uint32_t offset, const uint8_t *values, const int32_t *origins, size_t n)
{
	int32_t known[4];
	size_t i;

	if (!origins) {
		for (i = 0; i < n && i < 4; i++)
			known[i] = LADX_ENC_KNOWN;
		origins = known;
	}
	return ladx_enc_bank(r->enc, r->group, offset, values, origins, n) == LADX_ENC_OK ? LADX_BOSSES_OK
											   : LADX_BOSSES_FAILED;
}

static LadxBossesStatus put_byte(Run *r, uint32_t offset, int value)
{
	uint8_t v = (uint8_t)value;

	return put(r, offset, &v, NULL, 1);
}

static LadxBossesStatus load(Run *r, int room)
{
	return ladx_room_load(r->room, r->enc, room, NULL) == LADX_ROOM_OK ? LADX_BOSSES_OK : LADX_BOSSES_FAILED;
}

static LadxBossesStatus store(Run *r)
{
	return ladx_room_store(r->room, r->enc, r->group, -1, NULL) == LADX_ROOM_OK ? LADX_BOSSES_OK
										    : LADX_BOSSES_FAILED;
}

static LadxBossesStatus append(Run *r, LadxRoomObject o)
{
	if (r->room->object_count == LADX_ROOM_OBJECT_MAX)
		return LADX_BOSSES_FAILED;
	r->room->objects[r->room->object_count++] = o;
	return LADX_BOSSES_OK;
}

static LadxRoomObject object(LadxRoomObjectKind kind, int x, int y, int type_id, int count)
{
	LadxRoomObject o;

	memset(&o, 0, sizeof o);
	o.kind = kind;
	o.x = x;
	o.y = y;
	o.type_id = type_id;
	o.count = count;
	o.type_origin = LADX_ENC_KNOWN;
	return o;
}

static LadxRoomObject warp(int warp_type, int map_nr, int room_nr, int target_x, int target_y)
{
	LadxRoomObject o;

	ladx_room_warp(&o, warp_type, map_nr, room_nr, target_x, target_y);
	return o;
}

/* re.objects[-1] = o: Python's IndexError on an empty list. */
static LadxBossesStatus set_last(Run *r, LadxRoomObject o)
{
	if (r->room->object_count == 0)
		return raise_error(r, "IndexError", "list assignment index out of range");
	r->room->objects[r->room->object_count - 1] = o;
	return LADX_BOSSES_OK;
}

static int map_of(int dungeon)
{
	return dungeon < 8 ? dungeon : 0xFF;
}

static int status_flag(int dungeon)
{
	return boss_rooms[dungeon] >= 0x300 ? 0xDDE0 - 0x300 + boss_rooms[dungeon] : 0xD800 + boss_rooms[dungeon];
}

static LadxBossesStatus fix_dungeon_item(Run *r, int item_chest_id, int dungeon, int *out, int *same)
{
	static const int families[5] = {LADX_ITEM_MAP, LADX_ITEM_COMPASS, LADX_ITEM_KEY, LADX_ITEM_NIGHTMARE_KEY,
					LADX_ITEM_STONE_BEAK};
	char name[32], quoted[40];
	size_t i;
	int id;

	for (i = 0; i < 5; i++) {
		if (item_chest_id != ladx_items_chest_id(families[i]))
			continue;
		snprintf(name, sizeof name, "%s%d", ladx_item_name(families[i]), dungeon + 1);
		if ((id = ladx_items_chest_id(ladx_item_find(name))) < 0) {
			snprintf(quoted, sizeof quoted, "'%s'", name);
			return raise_error(r, "KeyError", quoted);
		}
		*out = id;
		*same = 0;
		return LADX_BOSSES_OK;
	}
	*out = item_chest_id;
	*same = 1;
	return LADX_BOSSES_OK;
}

static LadxBossesStatus copy_items(Run *r, int dungeon, int to)
{
	static const uint32_t tables[2] = {DROP_TABLE, BOSS_ITEM};
	LadxBossesStatus st;
	uint8_t v;
	int32_t origin;
	int id, same, t;

	for (t = 0; t < 2; t++) {
		if (!ladx_enc_bank_value(r->enc, tables[t] + (uint32_t)boss_rooms[dungeon], &v, &origin))
			return LADX_BOSSES_FAILED;
		if ((st = fix_dungeon_item(r, v, dungeon, &id, &same)) != LADX_BOSSES_OK)
			return st;
		v = (uint8_t)id;
		if (!same)
			origin = LADX_ENC_KNOWN;
		if ((st = put(r, tables[t] + (uint32_t)to, &v, &origin, 1)) != LADX_BOSSES_OK)
			return st;
	}
	return LADX_BOSSES_OK;
}

static LadxBossesStatus put_ld_hl(Run *r, uint32_t offset, const uint8_t *code, int value)
{
	uint8_t b[3];

	b[0] = code[0];
	b[1] = (uint8_t)(value & 0xFF);
	b[2] = (uint8_t)(value >> 8);
	return put(r, offset, b, NULL, 3);
}

/* getCleanBossRoom's D7 adjustments; done before room 0x223 is loaded, which they do not touch. */
static LadxBossesStatus d7_adjust(Run *r)
{
	static const struct {
		uint32_t at;
		int value;
	} map_bytes[7] = {{0x03a0 + 6 + 1 * 8, 0x00}, {0x03a0 + 7 + 2 * 8, 0x2C}, {0x03a0 + 7 + 3 * 8, 0x23},
			  {0x03a0 + 6 + 5 * 8, 0x00}, {0x0520 + 7 + 2 * 8, 0x2C}, {0x0520 + 7 + 3 * 8, 0x23},
			  {0x0520 + 6 + 5 * 8, 0x00}};
	LadxBossesStatus st = LADX_BOSSES_OK;
	size_t i;

	for (i = 0; i < 7 && st == LADX_BOSSES_OK; i++)
		st = put_byte(r, BANK(0x14) + map_bytes[i].at, map_bytes[i].value);
	if (st != LADX_BOSSES_OK || (st = load(r, D7_TOP)) != LADX_BOSSES_OK ||
	    (st = set_last(r, warp(1, 6, boss_rooms[6], 24, 112))) != LADX_BOSSES_OK || (st = store(r)) != LADX_BOSSES_OK)
		return st;
	if ((st = load(r, D7_TOP)) != LADX_BOSSES_OK)
		return st;
	ladx_room_remove_object(r->room, 4, 0);
	if ((st = store(r)) != LADX_BOSSES_OK || (st = put_byte(r, ROOM_EVENTS + D7_TOP - 0x100, 0x00)) != LADX_BOSSES_OK ||
	    (st = load(r, D7_NEXT)) != LADX_BOSSES_OK)
		return st;
	if (ladx_room_change_object(r->room, 0, 7, 0x03) != LADX_ROOM_OK ||
	    ladx_room_change_object(r->room, 2, 7, 0x03) != LADX_ROOM_OK)
		return LADX_BOSSES_FAILED;
	return store(r);
}

static LadxBossesStatus clean_room(Run *r, int dungeon)
{
	static const int d4_objects[4][2] = {{1, 1}, {8, 1}, {1, 6}, {8, 6}};
	LadxRoom *re = r->room;
	LadxBossesStatus st;
	size_t i, n = 0;

	if (dungeon == 6 && (st = d7_adjust(r)) != LADX_BOSSES_OK)
		return st;
	if ((st = load(r, boss_rooms[dungeon])) != LADX_BOSSES_OK)
		return st;
	for (i = 0; i < re->object_count; i++) {
		LadxRoomObject o = re->objects[i];
		int t = o.type_id;

		if (o.kind == LADX_ROOM_WARP || t == 0xBE || t == 0x06)
			continue;
		t = t == 0x1c ? 0x01 : t == 0x1e ? 0xaf : t == 0x1f ? 0xb0 : t == 0xF5 ? 0xF1 : t;
		if (t != o.type_id) {
			o.type_id = t;
			o.type_origin = LADX_ENC_KNOWN;
		}
		re->objects[n++] = o;
	}
	re->object_count = n;
	if (dungeon == 3) {
		re->object_count = n < 2 ? n : 2;
		for (i = 0; i < 4; i++)
			if ((st = append(r, object(LADX_ROOM_SINGLE, d4_objects[i][0], d4_objects[i][1], 0xAC, 0))) !=
			    LADX_BOSSES_OK)
				return st;
	}
	if (dungeon == 6) {
		re->floor_object &= 0x0F;
		re->floor_origin = LADX_ENC_KNOWN;
		if ((st = append(r, object(LADX_ROOM_SINGLE, 4, 0, 0xF0, 0))) != LADX_BOSSES_OK ||
		    (st = append(r, object(LADX_ROOM_SINGLE, 1, 6, 0xBE, 0))) != LADX_BOSSES_OK ||
		    (st = append(r, warp(1, dungeon, D7_TOP, 24, 16))) != LADX_BOSSES_OK)
			return st;
	}
	re->entity_count = 0;
	return LADX_BOSSES_OK;
}

static LadxBossesStatus warp_back(Run *r, int room, int dungeon, int target_room, int x, int y)
{
	LadxBossesStatus st;

	if ((st = load(r, room)) != LADX_BOSSES_OK ||
	    (st = set_last(r, warp(1, map_of(dungeon), target_room, x, y))) != LADX_BOSSES_OK)
		return st;
	return store(r);
}

static LadxBossesStatus fish(Run *r, const LadxBossesAsm *code, int dungeon)
{
	LadxBossesStatus st;
	size_t i;

	if (dungeon == 6) {
		if ((st = load(r, D7_TOP)) != LADX_BOSSES_OK)
			return st;
		for (i = 0; i < r->room->object_count; i++) {
			LadxRoomObject *o = &r->room->objects[i];

			if (o->kind != LADX_ROOM_WARP)
				continue;
			o->warp_type = 2;
			o->map_nr = 3;
			o->room = FISH_ROOM;
			o->target_x = 24;
			o->target_y = 16;
		}
		if ((st = store(r)) != LADX_BOSSES_OK || (st = warp_back(r, FISH_ROOM, dungeon, D7_TOP, 24, 16)) != LADX_BOSSES_OK)
			return st;
	} else {
		if ((st = put_byte(r, ROOM_EVENTS + (uint32_t)boss_rooms[dungeon] - 0x100, 0x2A)) != LADX_BOSSES_OK ||
		    (st = clean_room(r, dungeon)) != LADX_BOSSES_OK ||
		    (st = append(r, object(LADX_ROOM_SINGLE, 4, 4, 0xBE, 0))) != LADX_BOSSES_OK ||
		    (st = append(r, warp(2, 3, FISH_ROOM, 24, 16))) != LADX_BOSSES_OK || (st = store(r)) != LADX_BOSSES_OK ||
		    (st = warp_back(r, FISH_ROOM, dungeon, boss_rooms[dungeon], 72, 80)) != LADX_BOSSES_OK)
			return st;
	}
	if ((st = put_ld_hl(r, FISH_FLAG, code->fish_flag, dungeon == 6 ? 0xD800 + D7_TOP : status_flag(dungeon))) !=
	    LADX_BOSSES_OK)
		return st;
	return copy_items(r, dungeon, 0x01ff);
}

static LadxBossesStatus eagle(Run *r, const LadxBossesAsm *code, int dungeon)
{
	static const struct {
		LadxRoomObjectKind kind;
		int x, y, type_id, count;
	} stairs[7] = {{LADX_ROOM_SINGLE, 3, 3, 0x2C, 0}, {LADX_ROOM_HORIZONTAL, 4, 3, 0x22, 2},
		       {LADX_ROOM_SINGLE, 6, 3, 0x2B, 0}, {LADX_ROOM_SINGLE, 3, 4, 0x2A, 0},
		       {LADX_ROOM_SINGLE, 4, 4, 0xA2, 0}, {LADX_ROOM_SINGLE, 5, 4, 0x21, 0},
		       {LADX_ROOM_SINGLE, 6, 4, 0x29, 0}};
	LadxBossesStatus st;
	size_t i;

	if ((st = put_byte(r, ROOM_EVENTS + (uint32_t)boss_rooms[dungeon] - 0x100, 0x2A)) != LADX_BOSSES_OK ||
	    (st = put_ld_hl(r, EAGLE_FLAG, code->eagle_flag, status_flag(dungeon))) != LADX_BOSSES_OK ||
	    (st = clean_room(r, dungeon)) != LADX_BOSSES_OK)
		return st;
	for (i = 0; i < 7; i++)
		if ((st = append(r, object(stairs[i].kind, stairs[i].x, stairs[i].y, stairs[i].type_id, stairs[i].count))) !=
		    LADX_BOSSES_OK)
			return st;
	if ((st = append(r, warp(2, 6, EAGLE_ROOM, 72, 80))) != LADX_BOSSES_OK || (st = store(r)) != LADX_BOSSES_OK ||
	    (st = put_byte(r, INDOOR_TILESET + (uint32_t)boss_rooms[dungeon] - 0x100, 0)) != LADX_BOSSES_OK ||
	    (st = warp_back(r, EAGLE_ROOM, dungeon, boss_rooms[dungeon], 72, 80)) != LADX_BOSSES_OK)
		return st;
	return copy_items(r, dungeon, 0x02E8);
}

static LadxBossesStatus plain(Run *r, const LadxBossesAsm *code, int dungeon, int target)
{
	LadxBossesStatus st;
	uint32_t room = (uint32_t)boss_rooms[dungeon];
	int n, i;

	if ((st = put_byte(r, ROOM_EVENTS + room - 0x100, 0x21)) != LADX_BOSSES_OK ||
	    (st = clean_room(r, dungeon)) != LADX_BOSSES_OK)
		return st;
	r->room->entities[0] = boss_entities[target];
	r->room->entity_count = 1;
	if (target == SLIME_EEL) {
		if ((st = put_byte(r, INDOOR_TILESET + room - 0x100, 0x06)) != LADX_BOSSES_OK)
			return st;
		if (dungeon == 1) {
			ladx_room_remove_object(r->room, 3, 3);
			ladx_room_remove_object(r->room, 6, 3);
			for (n = 0; n < 10; n++) {
				ladx_room_remove_object(r->room, n, 0);
				ladx_room_remove_object(r->room, n, 1);
			}
		}
	}
	/* A second facade's rom.patch is skipped as already applied: the same bytes, the same net effect. */
	if (target == FACADE)
		for (i = 0; i < 3; i++)
			if ((st = put(r, facade_at[i], code->facade[i], NULL, 2)) != LADX_BOSSES_OK)
				return st;
	return store(r);
}

LadxBossesStatus ladx_bosses_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room, const LadxBossesAsm *code,
				    const int *mapping, LadxOutcome *raised)
{
	Run r;
	LadxBossesStatus st;
	uint8_t v, check[2];
	int d, eagle_dungeon = 0xFE;
	uint32_t a;

	r.enc = enc;
	r.group = group;
	r.room = room;
	r.raised = raised;
	for (d = 0; d < LADX_BOSS_ROOM_COUNT; d++)
		if (mapping[d] < 0 || mapping[d] >= LADX_BOSS_ROOM_COUNT)
			return LADX_BOSSES_FAILED;
	for (a = 0; a < COLOR_WARP_COUNT; a++) {
		int32_t origin;

		if (a == 1 + 3 * 8 || a == 2 + 6 * 8)
			continue;
		if (!ladx_enc_bank_value(enc, COLOR_WARPS + a, &v, &origin))
			return LADX_BOSSES_FAILED;
		if (v == 0x00 && (st = put_byte(&r, COLOR_WARPS + a, 0xFF)) != LADX_BOSSES_OK)
			return st;
	}
	if ((st = put(&r, GENIE, code->genie, NULL, 2)) != LADX_BOSSES_OK)
		return st;
	for (d = 0; d < LADX_BOSS_ROOM_COUNT && eagle_dungeon == 0xFE; d++)
		if (mapping[d] == EAGLE)
			eagle_dungeon = d;
	check[0] = code->eagle_check[0];
	check[1] = (uint8_t)(eagle_dungeon < 8 ? eagle_dungeon : 0xFF);
	if ((st = put(&r, EAGLE_CHECK, check, NULL, 2)) != LADX_BOSSES_OK)
		return st;
	for (d = 0; d < LADX_BOSS_ROOM_COUNT; d++) {
		int target = mapping[d];

		if (target == d)
			continue;
		st = target == FISH ? fish(&r, code, d) : target == EAGLE ? eagle(&r, code, d)
									    : plain(&r, code, d, target);
		if (st != LADX_BOSSES_OK)
			return st;
	}
	return LADX_BOSSES_OK;
}

static LadxRoomObject blaino_warp(int room)
{
	static const struct {
		int lo, hi, map, target;
	} dungeons[8] = {{0x100, 0x11D, 0, 0x117}, {0x11D, 0x140, 1, 0x136}, {0x140, 0x15D, 2, 0x152},
			 {0x15D, 0x180, 3, 0x174}, {0x180, 0x1AC, 4, 0x1A1}, {0x1B0, 0x1DE, 5, 0x1D4},
			 {0x200, 0x22D, 6, 0x20E}, {0x22D, 0x26C, 7, 0x25D}};
	size_t i;

	for (i = 0; i < 8; i++)
		if (room >= dungeons[i].lo && room < dungeons[i].hi)
			return warp(1, dungeons[i].map, dungeons[i].target, 80, 80);
	if (room >= 0x300)
		return warp(1, 0xFF, 0x312, 80, 80);
	if (room == 0x2E1)
		return warp(1, 0x15, 0x2F0, 0x50, 0x7C);
	if (room == 0x27F)
		return warp(1, 0x16, 0x28F, 0x50, 0x7C);
	return warp(1, 0x10, 0x2A3, 0x50, 0x7C);
}

static int has_warp(const LadxRoom *re)
{
	size_t i;

	for (i = 0; i < re->object_count; i++)
		if (re->objects[i].kind == LADX_ROOM_WARP)
			return 1;
	return 0;
}

static LadxBossesStatus miniboss(Run *r, int key, LadxMiniboss name)
{
	static const struct {
		LadxRoomObjectKind kind;
		int x, y, type_id, count;
	} cue_ball[6] = {{LADX_ROOM_SINGLE, 3, 3, 0x2c, 0}, {LADX_ROOM_HORIZONTAL, 4, 3, 0x22, 2},
			 {LADX_ROOM_SINGLE, 6, 3, 0x2b, 0}, {LADX_ROOM_SINGLE, 3, 4, 0x2a, 0},
			 {LADX_ROOM_HORIZONTAL, 4, 4, 0x21, 2}, {LADX_ROOM_SINGLE, 6, 4, 0x29, 0}};
	LadxRoom *re = r->room;
	LadxBossesStatus st;
	size_t i, n = 0;
	int x, y;

	if ((st = load(r, miniboss_rooms[key])) != LADX_BOSSES_OK)
		return st;
	for (i = 0; i < re->entity_count; i++)
		if (re->entities[i].type_id == WARP_ENTITY)
			re->entities[n++] = re->entities[i];
	re->entity_count = n;
	for (i = 0; i < (size_t)miniboss_entities[name].n; i++) {
		const LadxRoomEntity *e = &miniboss_entities[name].e[i];

		if (ladx_room_add_entity(re, e->x, e->y, e->type_id) != LADX_ROOM_OK)
			return LADX_BOSSES_FAILED;
	}
	if (re->room == 0x228 && name != LADX_MINIBOSS_GRIM_CREEPER)
		for (x = 3; x < 7; x++)
			for (y = 0; y < 3; y++)
				ladx_room_remove_object(re, x, y);
	if (name == LADX_MINIBOSS_GHOMA && key != 4 && (st = put_byte(r, GHOMA_COLLISION, 0xD5)) != LADX_BOSSES_OK)
		return st;
	if (name == LADX_MINIBOSS_CUE_BALL)
		for (i = 0; i < 6; i++)
			if ((st = append(r, object(cue_ball[i].kind, cue_ball[i].x, cue_ball[i].y, cue_ball[i].type_id,
						   cue_ball[i].count))) != LADX_BOSSES_OK)
				return st;
	if (name == LADX_MINIBOSS_BLAINO && !has_warp(re) && (st = append(r, blaino_warp(re->room))) != LADX_BOSSES_OK)
		return st;
	if (name == LADX_MINIBOSS_DODONGO) {
		for (i = 0, n = 0; i < re->object_count; i++)
			if (re->objects[i].kind == LADX_ROOM_WARP || re->objects[i].type_id != 0xDF)
				re->objects[n++] = re->objects[i];
		re->object_count = n;
	}
	if (name == LADX_MINIBOSS_ROLLING_BONES && key == 2) {
		uint8_t v;
		int32_t origin;

		/* rom.patch(0x03, 0x02F1 + 0x81, "84", "95"): skipped when already applied. */
		if (!ladx_enc_bank_value(r->enc, ROLLING_BONES_FLAGS, &v, &origin))
			return LADX_BOSSES_FAILED;
		if (v == 0x84 && (st = put_byte(r, ROLLING_BONES_FLAGS, 0x95)) != LADX_BOSSES_OK)
			return st;
		if (v != 0x84 && v != 0x95)
			return LADX_BOSSES_FAILED;
	}
	return store(r);
}

LadxBossesStatus ladx_minibosses_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room, const LadxMiniboss *mapping,
					LadxOutcome *raised)
{
	Run r;
	LadxBossesStatus st;
	int k;

	r.enc = enc;
	r.group = group;
	r.room = room;
	r.raised = raised;
	for (k = 0; k < LADX_MINIBOSS_KEY_COUNT; k++)
		if ((int)mapping[k] < 0 || mapping[k] >= LADX_MINIBOSS_COUNT)
			return LADX_BOSSES_FAILED;
	for (k = 0; k < LADX_MINIBOSS_KEY_COUNT; k++)
		if ((st = miniboss(&r, k, mapping[k])) != LADX_BOSSES_OK)
			return st;
	return LADX_BOSSES_OK;
}

int ladx_group_bosses(LadxEncoder *enc, LadxPipeline *p)
{
	LadxBossesAsm code;
	LadxSpan group;
	int i, ok;

	if (!p->rom || ladx_pipeline_const_unit_count(p, "bosses") != BOSSES_UNITS)
		return 0;
	ok = ladx_pipeline_const_bank(p, "bosses", 1, GENIE, code.genie, 2) &&
	     ladx_pipeline_const_bank(p, "bosses", 1, EAGLE_CHECK, code.eagle_check, 2) &&
	     ladx_pipeline_const_bank(p, "bosses", 2, FISH_FLAG, code.fish_flag, 3) &&
	     ladx_pipeline_const_bank(p, "bosses", 2, EAGLE_FLAG, code.eagle_flag, 3);
	for (i = 0; i < 3 && ok; i++)
		ok = ladx_pipeline_const_bank(p, "bosses", 2, facade_at[i], code.facade[i], 2);
	if (!ok)
		return 0;
	group.at = "bosses";
	group.len = strlen(group.at);
	return ladx_bosses_change(enc, group, &p->room, &code, p->randomizer->ws.boss_mapping, &p->raised) ==
		       LADX_BOSSES_OK &&
	       ladx_pipeline_const_units(enc, p, "bosses", 0, 1) &&
	       ladx_minibosses_change(enc, group, &p->room, p->randomizer->ws.miniboss_mapping, &p->raised) ==
		       LADX_BOSSES_OK;
}
