#ifndef LADX_ROOM_H
#define LADX_ROOM_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_encoder.h"

#define LADX_ROOM_OBJECT_MAX 256
#define LADX_ROOM_ENTITY_MAX 128
#define LADX_ROOM_OVERLAY 80
#define LADX_ROOM_RAW_MAX (2 + 5 * LADX_ROOM_OBJECT_MAX + 1)
#define LADX_ROOM_NONE (-1)

typedef enum LadxRoomObjectKind {
	LADX_ROOM_SINGLE = 0,
	LADX_ROOM_HORIZONTAL,
	LADX_ROOM_VERTICAL,
	LADX_ROOM_WARP
} LadxRoomObjectKind;

typedef struct LadxRoomObject {
	LadxRoomObjectKind kind;
	int x, y, type_id, count;
	int32_t type_origin;
	int warp_type, map_nr, room, target_x, target_y;
} LadxRoomObject;

typedef struct LadxRoomEntity {
	int x, y, type_id;
} LadxRoomEntity;

typedef struct LadxRoom {
	int room;
	char alt[LADX_WLOG_NAME_MAX + 1];
	int animation_id, floor_object;
	int32_t floor_origin;
	LadxRoomObject objects[LADX_ROOM_OBJECT_MAX];
	size_t object_count;
	LadxRoomEntity entities[LADX_ROOM_ENTITY_MAX];
	size_t entity_count;
	int has_overlay;
	uint8_t overlay[LADX_ROOM_OVERLAY];
	int32_t overlay_origin[LADX_ROOM_OVERLAY];
	int tileset_index, palette_index, attribset_bank, attribset_addr;
	uint8_t raw[LADX_ROOM_RAW_MAX];
	int32_t raw_origin[LADX_ROOM_RAW_MAX];
} LadxRoom;

typedef enum LadxRoomStatus {
	LADX_ROOM_OK = 0,
	LADX_ROOM_BAD_ROOM,
	LADX_ROOM_MALFORMED,
	LADX_ROOM_FULL,
	LADX_ROOM_NO_ROM,
	LADX_ROOM_RANGE,
	LADX_ROOM_ENCODER
} LadxRoomStatus;

LadxRoomStatus ladx_room_load(LadxRoom *r, const LadxEncoder *enc, int room, const char *alt);

LadxRoomStatus ladx_room_store(LadxRoom *r, LadxEncoder *enc, LadxSpan group, int new_room, const char *new_alt);

void ladx_room_warp(LadxRoomObject *o, int warp_type, int map_nr, int room_nr, int target_x, int target_y);

LadxRoomStatus ladx_room_insert_entity(LadxRoom *r, size_t index, int x, int y, int type_id);
LadxRoomStatus ladx_room_add_entity(LadxRoom *r, int x, int y, int type_id);
void ladx_room_remove_entities(LadxRoom *r, int type_id);
int ladx_room_has_entity(const LadxRoom *r, int type_id);
int ladx_room_has_object(const LadxRoom *r, int type_id);
void ladx_room_remove_object(LadxRoom *r, int x, int y);
LadxRoomStatus ladx_room_change_object(LadxRoom *r, int x, int y, int new_type);
LadxRoomStatus ladx_room_move_object(LadxRoom *r, int x, int y, int new_x, int new_y);
LadxRoomStatus ladx_room_update_overlay(LadxRoom *r, int preserve_floor);

#endif
