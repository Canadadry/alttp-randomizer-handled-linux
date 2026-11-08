#include "ladx_groups.h"

#include <stdio.h>
#include <string.h>

static const LadxEntranceInfo entrance_info[LADX_ENTRANCE_INFO_COUNT] = {
	{"d8", 0x10, NULL, -1, 0x25D, 0x230, NULL},
	{"phone_d8", 0x11, NULL, -1, 0x299, -1, NULL},
	{"fire_cave_exit", 0x03, NULL, -1, 0x1EE, -1, NULL},
	{"fire_cave_entrance", 0x13, NULL, -1, 0x1FE, -1, NULL},
	{"madbatter_taltal", 0x04, NULL, -1, 0x1E2, -1, NULL},
	{"left_taltal_entrance", 0x15, NULL, -1, 0x2EA, -1, NULL},
	{"obstacle_cave_entrance", 0x17, NULL, -1, 0x2B6, -1, NULL},
	{"left_to_right_taltalentrance", 0x07, NULL, -1, 0x2EE, -1, NULL},
	{"obstacle_cave_outside_chest", 0x18, NULL, 0, 0x2BB, -1, NULL},
	{"obstacle_cave_exit", 0x18, NULL, 1, 0x2BC, -1, NULL},
	{"papahl_entrance", 0x19, NULL, -1, 0x289, -1, NULL},
	{"papahl_exit", 0x0A, NULL, 0, 0x28B, -1, NULL},
	{"rooster_house", 0x0A, NULL, 2, 0x29F, -1, NULL},
	{"bird_cave", 0x0A, NULL, 1, 0x27E, -1, NULL},
	{"multichest_left", 0x1D, NULL, 0, 0x2F9, -1, NULL},
	{"multichest_right", 0x1D, NULL, 1, 0x2FA, -1, NULL},
	{"multichest_top", 0x0D, NULL, -1, 0x2F2, -1, NULL},
	{"right_taltal_connector1", 0x1E, NULL, 0, 0x280, -1, NULL},
	{"right_taltal_connector2", 0x1F, NULL, 0, 0x282, -1, NULL},
	{"right_taltal_connector3", 0x1E, NULL, 1, 0x283, -1, NULL},
	{"right_taltal_connector4", 0x1F, NULL, 2, 0x287, -1, NULL},
	{"right_taltal_connector5", 0x1F, NULL, 1, 0x28C, -1, NULL},
	{"right_taltal_connector6", 0x0F, NULL, -1, 0x28E, -1, NULL},
	{"right_fairy", 0x1F, NULL, 3, 0x1FB, -1, NULL},
	{"d7", 0x0E, "Alt0E", -1, 0x20E, 0x22C, NULL},
	{"writes_cave_left", 0x20, NULL, -1, 0x2AE, -1, NULL},
	{"writes_cave_right", 0x21, NULL, -1, 0x2AF, -1, NULL},
	{"writes_house", 0x30, NULL, -1, 0x2A8, -1, NULL},
	{"writes_phone", 0x31, NULL, -1, 0x29B, -1, NULL},
	{"d2", 0x24, NULL, -1, 0x136, 0x12A, NULL},
	{"moblin_cave", 0x35, NULL, -1, 0x2F0, -1, NULL},
	{"photo_house", 0x37, NULL, -1, 0x2B5, -1, NULL},
	{"mambo", 0x2A, NULL, -1, 0x2FD, -1, NULL},
	{"d4", 0x2B, "Alt2B", 0, 0x17A, 0x162, NULL},
	{"heartpiece_swim_cave", 0x2E, NULL, -1, 0x1F2, -1, NULL},
	{"raft_return_exit", 0x2F, NULL, -1, 0x1E7, -1, NULL},
	{"raft_house", 0x3F, NULL, -1, 0x2B0, -1, NULL},
	{"raft_return_enter", 0x8F, NULL, -1, 0x1F7, -1, NULL},
	{"hookshot_cave", 0x42, NULL, -1, 0x2B3, -1, NULL},
	{"toadstool_exit", 0x50, NULL, -1, 0x2AB, -1, NULL},
	{"forest_madbatter", 0x52, NULL, -1, 0x1E1, -1, NULL},
	{"toadstool_entrance", 0x62, NULL, -1, 0x2BD, -1, NULL},
	{"crazy_tracy", 0x45, NULL, -1, 0x2AD, -1, NULL},
	{"witch", 0x65, NULL, -1, 0x2A2, -1, NULL},
	{"graveyard_cave_left", 0x75, NULL, -1, 0x2DE, -1, NULL},
	{"graveyard_cave_right", 0x76, NULL, -1, 0x2DF, -1, NULL},
	{"d0", 0x77, NULL, -2, 0x312, 0x301, NULL},
	{"castle_jump_cave", 0x78, NULL, -1, 0x1FD, -1, NULL},
	{"castle_main_entrance", 0x69, NULL, -1, 0x2D3, -1, NULL},
	{"castle_upper_left", 0x59, NULL, 0, 0x2D5, -1, NULL},
	{"castle_upper_right", 0x59, NULL, 1, 0x2D6, -1, NULL},
	{"castle_secret_exit", 0x49, NULL, -1, 0x1EB, -1, NULL},
	{"castle_secret_entrance", 0x4A, NULL, -1, 0x1EC, -1, NULL},
	{"castle_phone", 0x4B, NULL, -1, 0x2CC, -1, NULL},
	{"papahl_house_left", 0x82, NULL, 0, 0x2A5, -1, NULL},
	{"papahl_house_right", 0x82, NULL, 1, 0x2A6, -1, NULL},
	{"dream_hut", 0x83, NULL, -1, 0x2AA, -1, NULL},
	{"rooster_grave", 0x92, NULL, -1, 0x1F4, -1, NULL},
	{"shop", 0x93, NULL, -1, 0x2A1, -1, NULL},
	{"madambowwow", 0xA1, NULL, 1, 0x2A7, -1, NULL},
	{"kennel", 0xA1, NULL, 0, 0x2B2, -1, NULL},
	{"start_house", 0xA2, NULL, -1, 0x2A3, -1, NULL},
	{"library", 0xB0, NULL, -1, 0x1FA, -1, NULL},
	{"ulrira", 0xB1, NULL, -1, 0x2A9, -1, NULL},
	{"mabe_phone", 0xB2, NULL, -1, 0x2CB, -1, NULL},
	{"trendy_shop", 0xB3, NULL, -1, 0x2A0, -1, NULL},
	{"prairie_left_phone", 0xA4, NULL, -1, 0x2B4, -1, NULL},
	{"prairie_left_cave1", 0x84, NULL, -1, 0x2CD, -1, NULL},
	{"prairie_left_cave2", 0x86, NULL, -1, 0x2F4, -1, NULL},
	{"prairie_left_fairy", 0x87, NULL, -1, 0x1F3, -1, NULL},
	{"mamu", 0xD4, NULL, -1, 0x2FB, -1, NULL},
	{"d3", 0xB5, NULL, -1, 0x152, 0x159, NULL},
	{"prairie_right_phone", 0x88, NULL, -1, 0x29C, -1, NULL},
	{"seashell_mansion", 0x8A, NULL, -1, 0x2E9, -1, NULL},
	{"prairie_right_cave_top", 0xB8, NULL, 1, 0x292, -1, NULL},
	{"prairie_right_cave_bottom", 0xC8, NULL, -1, 0x293, -1, NULL},
	{"prairie_right_cave_high", 0xB8, NULL, 0, 0x295, -1, NULL},
	{"prairie_to_animal_connector", 0xAA, NULL, -1, 0x2D0, -1, NULL},
	{"animal_to_prairie_connector", 0xAB, NULL, -1, 0x2D1, -1, NULL},
	{"d6", 0x8C, "Alt8C", -1, 0x1D4, 0x1B5, NULL},
	{"d6_connector_exit", 0x9C, NULL, -1, 0x1F0, -1, NULL},
	{"d6_connector_entrance", 0x9D, NULL, -1, 0x1F1, -1, NULL},
	{"armos_fairy", 0x8D, NULL, -1, 0x1AC, -1, NULL},
	{"armos_maze_cave", 0xAE, NULL, -1, 0x2FC, -1, NULL},
	{"armos_temple", 0xAC, NULL, -1, 0x28F, -1, NULL},
	{"d1", 0xD3, NULL, -1, 0x117, 0x102, NULL},
	{"boomerang_cave", 0xF4, NULL, -1, 0x1F5, -1, "Alt1F5"},
	{"banana_seller", 0xE3, NULL, -1, 0x2FE, -1, NULL},
	{"ghost_house", 0xF6, NULL, -1, 0x1E3, -1, NULL},
	{"richard_house", 0xD6, NULL, -1, 0x2C7, -1, NULL},
	{"richard_maze", 0xC6, NULL, -1, 0x2C9, -1, NULL},
	{"prairie_low_phone", 0xE8, NULL, -1, 0x29D, -1, NULL},
	{"prairie_madbatter_connector_entrance", 0xF9, NULL, -1, 0x1F6, -1, NULL},
	{"prairie_madbatter_connector_exit", 0xE7, NULL, -1, 0x1E5, -1, NULL},
	{"prairie_madbatter", 0xE6, NULL, -1, 0x1E0, -1, NULL},
	{"d5", 0xD9, NULL, -1, 0x1A1, 0x182, NULL},
	{"animal_phone", 0xDB, NULL, -1, 0x2E3, -1, NULL},
	{"animal_house1", 0xCC, NULL, 0, 0x2DB, -1, NULL},
	{"animal_house2", 0xCC, NULL, 1, 0x2DD, -1, NULL},
	{"animal_house3", 0xCD, NULL, 1, 0x2D9, -1, NULL},
	{"animal_house4", 0xCD, NULL, 2, 0x2DA, -1, NULL},
	{"animal_house5", 0xDD, NULL, -1, 0x2D7, -1, NULL},
	{"animal_cave", 0xCD, NULL, 0, 0x2F7, -1, NULL},
	{"desert_cave", 0xCF, NULL, -1, 0x1F9, -1, NULL},
};

const LadxEntranceInfo *ladx_entrance_info(size_t *count)
{
	*count = LADX_ENTRANCE_INFO_COUNT;
	return entrance_info;
}

/* An int room (alt NULL) or an alt key: Python never finds the two equal. */
typedef struct RoomId {
	int room;
	const char *alt;
} RoomId;

typedef struct Warp {
	int warp_type, map_nr, room, target_x, target_y;
} Warp;

typedef struct Change {
	size_t room;
	int target;
	Warp warp;
} Change;

#define KEYS (2 * LADX_ENTRANCES_MAX)
#define CHANGES (2 * KEYS)

typedef struct State {
	Warp warp_to[KEYS];
	int vanilla[KEYS];
	RoomId from[KEYS][2];
	int from_n[KEYS];
	RoomId rooms[CHANGES];
	size_t room_n;
	Change change[CHANGES];
	size_t change_n;
} State;

static int same_room(RoomId a, RoomId b)
{
	return a.alt || b.alt ? a.alt && b.alt && strcmp(a.alt, b.alt) == 0 : a.room == b.room;
}

static RoomId index_room(int room)
{
	RoomId id;

	id.room = room;
	id.alt = NULL;
	return id;
}

static RoomId alt_room(const char *alt)
{
	RoomId id;

	id.room = -1;
	id.alt = alt;
	return id;
}

static int load(LadxRoom *r, const LadxEncoder *enc, RoomId id)
{
	return ladx_room_load(r, enc, id.alt ? 0 : id.room, id.alt) == LADX_ROOM_OK;
}

static Warp warp_of(const LadxRoomObject *o)
{
	Warp w;

	w.warp_type = o->warp_type;
	w.map_nr = o->map_nr;
	w.room = o->room;
	w.target_x = o->target_x;
	w.target_y = o->target_y;
	return w;
}

static const LadxRoomObject *nth_warp(const LadxRoom *r, size_t i)
{
	size_t k;

	for (k = 0; k < r->object_count; k++)
		if (r->objects[k].kind == LADX_ROOM_WARP && i-- == 0)
			return &r->objects[k];
	return NULL;
}

static LadxEntrancesStatus crash(LadxOutcome *raised, const char *type)
{
	raised->kind = LADX_OUTCOME_CRASH;
	raised->type = type;
	raised->message = raised->text;
	return LADX_ENTRANCES_RAISED;
}

static int add_change(State *s, RoomId source, int target, Warp warp)
{
	size_t i;

	for (i = 0; i < s->room_n && !same_room(s->rooms[i], source); i++)
		;
	if (i == s->room_n) {
		if (s->room_n == CHANGES)
			return 0;
		s->rooms[s->room_n++] = source;
	}
	for (i = 0; i < s->change_n; i++)
		if (same_room(s->rooms[s->change[i].room], source) && s->change[i].target == target) {
			s->change[i].warp = warp;
			return 1;
		}
	if (s->change_n == CHANGES)
		return 0;
	for (s->change[s->change_n].room = 0; !same_room(s->rooms[s->change[s->change_n].room], source);
	     s->change[s->change_n].room++)
		;
	s->change[s->change_n].target = target;
	s->change[s->change_n].warp = warp;
	s->change_n++;
	return 1;
}

static LadxEntrancesStatus read_entrance(State *s, LadxRoom *r, const LadxEncoder *enc, const LadxEntranceInfo *info,
					 size_t k, size_t count, LadxOutcome *raised)
{
	size_t inside = count + k, i;
	const LadxRoomObject *warp;
	int index = info->index >= 0 ? info->index : 0, found = 0;
	Warp in;

	if (!load(r, enc, info->alt_room ? alt_room(info->alt_room) : index_room(info->room)))
		return LADX_ENTRANCES_FAILED;
	if (!(warp = nth_warp(r, (size_t)index))) {
		snprintf(raised->text, sizeof raised->text, "list index out of range");
		return crash(raised, "IndexError");
	}
	in = warp_of(warp);
	s->warp_to[inside] = in;
	s->vanilla[k] = in.room;
	if (info->target != in.room) {
		snprintf(raised->text, sizeof raised->text, "%s != %03x", info->name, in.room);
		return crash(raised, "AssertionError");
	}
	s->from[inside][0] = index_room(info->target);
	s->from_n[inside] = 1;
	if (info->instrument_alt || info->instrument_room >= 0)
		s->from[inside][s->from_n[inside]++] =
			info->instrument_alt ? alt_room(info->instrument_alt) : index_room(info->instrument_room);
	if (!load(r, enc, index_room(in.room)))
		return LADX_ENTRANCES_FAILED;
	for (i = 0; (warp = nth_warp(r, i)) != NULL; i++)
		if (warp->room == info->room) {
			s->warp_to[k] = warp_of(warp);
			s->vanilla[inside] = warp->room;
			found = 1;
		}
	if (!found) {
		snprintf(raised->text, sizeof raised->text, "Missing warp to outdoor on %s", info->name);
		return crash(raised, "AssertionError");
	}
	s->from[k][0] = index_room(info->room);
	s->from_n[k] = 1;
	if (info->alt_room)
		s->from[k][s->from_n[k]++] = alt_room(info->alt_room);
	return LADX_ENTRANCES_OK;
}

static LadxEntrancesStatus apply_room(const State *s, size_t room, LadxRoom *r, LadxEncoder *enc, LadxSpan group)
{
	size_t i, c;

	if (!load(r, enc, s->rooms[room]))
		return LADX_ENTRANCES_FAILED;
	for (i = 0; i < r->object_count; i++) {
		LadxRoomObject *o = &r->objects[i];

		if (o->kind != LADX_ROOM_WARP)
			continue;
		for (c = 0; c < s->change_n && !(s->change[c].room == room && s->change[c].target == o->room); c++)
			;
		if (c < s->change_n) {
			o->warp_type = s->change[c].warp.warp_type;
			o->map_nr = s->change[c].warp.map_nr;
			o->room = s->change[c].warp.room;
			o->target_x = s->change[c].warp.target_x;
			o->target_y = s->change[c].warp.target_y;
		}
	}
	return ladx_room_store(r, enc, group, -1, NULL) == LADX_ROOM_OK ? LADX_ENTRANCES_OK : LADX_ENTRANCES_FAILED;
}

LadxEntrancesStatus ladx_entrances_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room,
					  const LadxEntranceInfo *info, size_t count, const int *mapping,
					  LadxOutcome *raised)
{
	State s;
	LadxEntrancesStatus st;
	size_t k;
	int j;

	if (count > LADX_ENTRANCES_MAX)
		return LADX_ENTRANCES_FAILED;
	memset(&s, 0, sizeof s);
	for (k = 0; k < count; k++)
		if ((st = read_entrance(&s, room, enc, &info[k], k, count, raised)) != LADX_ENTRANCES_OK)
			return st;
	for (k = 0; k < 2 * count; k++) {
		if (mapping[k] < 0 || (size_t)mapping[k] >= 2 * count)
			return LADX_ENTRANCES_FAILED;
		for (j = 0; j < s.from_n[k]; j++)
			if (!add_change(&s, s.from[k][j], s.vanilla[k], s.warp_to[mapping[k]]))
				return LADX_ENTRANCES_FAILED;
	}
	for (k = 0; k < s.room_n; k++)
		if ((st = apply_room(&s, k, room, enc, group)) != LADX_ENTRANCES_OK)
			return st;
	return LADX_ENTRANCES_OK;
}

int ladx_group_entrances(LadxEncoder *enc, LadxPipeline *p)
{
	int mapping[2 * LADX_ENTRANCE_INFO_COUNT];
	LadxSpan group;
	size_t k;

	if (!p->rom)
		return 0;
	for (k = 0; k < LADX_ENTRANCE_INFO_COUNT; k++) {
		mapping[k] = (int)(LADX_ENTRANCE_INFO_COUNT + k);
		mapping[LADX_ENTRANCE_INFO_COUNT + k] = (int)k;
	}
	group.at = "entrances";
	group.len = strlen(group.at);
	return ladx_entrances_change(enc, group, &p->room, entrance_info, LADX_ENTRANCE_INFO_COUNT, mapping,
				     &p->raised) == LADX_ENTRANCES_OK;
}
