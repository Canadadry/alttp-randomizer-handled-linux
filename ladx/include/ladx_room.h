#ifndef LADX_ROOM_H
#define LADX_ROOM_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_encoder.h"

/*
 * roomEditor.py's RoomEditor at the pin (tests/ladx/README.md, "Room
 * editor"): a room's objects, entities and overlay read from an encoder's
 * current state, edited, and stored back through it as a C group's writes.
 * Caller-owned, no allocation. Not ported: loadFromJson, buildObjectList,
 * getTileArray.
 */

#define LADX_ROOM_OBJECT_MAX 256
#define LADX_ROOM_ENTITY_MAX 128
#define LADX_ROOM_OVERLAY 80
/* The longest entry read or stored: the header, every object as a warp, the terminator. */
#define LADX_ROOM_RAW_MAX (2 + 5 * LADX_ROOM_OBJECT_MAX + 1)
#define LADX_ROOM_NONE (-1) /* LADXR's None for the optional store fields */

typedef enum LadxRoomObjectKind {
	LADX_ROOM_SINGLE = 0, /* Object */
	LADX_ROOM_HORIZONTAL,
	LADX_ROOM_VERTICAL,
	LADX_ROOM_WARP /* x, y and type_id are LADXR's None: no x/y/type helper matches it */
} LadxRoomObjectKind;

typedef struct LadxRoomObject {
	LadxRoomObjectKind kind;
	int x, y, type_id, count;
	int32_t type_origin; /* the origin type_id carries (a parsed byte's), else LADX_ENC_KNOWN */
	/* LADX_ROOM_WARP only: room is the target room, 0x100/0x200/0x300 added by map as LADXR does */
	int warp_type, map_nr, room, target_x, target_y;
} LadxRoomObject;

typedef struct LadxRoomEntity {
	int x, y, type_id;
} LadxRoomEntity;

typedef struct LadxRoom {
	int room;			   /* the index, or -1 for an alt room */
	char alt[LADX_WLOG_NAME_MAX + 1]; /* the alt key, "" for an index room */
	int animation_id, floor_object;
	int32_t floor_origin; /* the origin floor_object carries */
	LadxRoomObject objects[LADX_ROOM_OBJECT_MAX];
	size_t object_count;
	LadxRoomEntity entities[LADX_ROOM_ENTITY_MAX]; /* index rooms only */
	size_t entity_count;
	int has_overlay;
	uint8_t overlay[LADX_ROOM_OVERLAY];
	int32_t overlay_origin[LADX_ROOM_OVERLAY];
	int tileset_index, palette_index, attribset_bank, attribset_addr; /* LADX_ROOM_NONE: not stored */
	uint8_t raw[LADX_ROOM_RAW_MAX]; /* load's and store's scratch */
	int32_t raw_origin[LADX_ROOM_RAW_MAX];
} LadxRoom;

typedef enum LadxRoomStatus {
	LADX_ROOM_OK = 0,
	LADX_ROOM_BAD_ROOM,  /* an index over 0x31F, an alt key no rooms table holds */
	LADX_ROOM_MALFORMED, /* an entry LADXR's parse asserts on or reads past: unknown, an integer, no terminator */
	LADX_ROOM_FULL,	     /* over LADX_ROOM_OBJECT_MAX objects, LADX_ROOM_ENTITY_MAX entities or LADX_ROOM_RAW_MAX bytes */
	LADX_ROOM_NO_ROM,    /* a byte to read is a source reference and the encoder has no ROM */
	LADX_ROOM_RANGE,     /* a field outside its byte, an overlay index Python refuses, an overlay to store and none */
	LADX_ROOM_ENCODER    /* store: the encoder refused */
} LadxRoomStatus;

/* RoomEditor(rom, room): room an index (alt NULL) or an alt key (room ignored). */
LadxRoomStatus ladx_room_load(LadxRoom *r, const LadxEncoder *enc, int room, const char *alt);

/* r.store(rom, new_room_nr): new_room -1 with new_alt NULL stores under r's own room. */
LadxRoomStatus ladx_room_store(LadxRoom *r, LadxEncoder *enc, LadxSpan group, int new_room, const char *new_alt);

/* ObjectWarp(warp_type, map_nr, room_nr, target_x, target_y) into *o, room_nr offset by map as LADXR's constructor does. */
void ladx_room_warp(LadxRoomObject *o, int warp_type, int map_nr, int room_nr, int target_x, int target_y);

/* entities.insert(index, (x, y, type_id)) (index clamped as Python's); FULL when full. */
LadxRoomStatus ladx_room_insert_entity(LadxRoom *r, size_t index, int x, int y, int type_id);
LadxRoomStatus ladx_room_add_entity(LadxRoom *r, int x, int y, int type_id);
void ladx_room_remove_entities(LadxRoom *r, int type_id);
int ladx_room_has_entity(const LadxRoom *r, int type_id);
int ladx_room_has_object(const LadxRoom *r, int type_id);
void ladx_room_remove_object(LadxRoom *r, int x, int y);
/* The overlay edits below give RANGE where Python's index raises (a negative index wraps, as in Python). */
LadxRoomStatus ladx_room_change_object(LadxRoom *r, int x, int y, int new_type);
LadxRoomStatus ladx_room_move_object(LadxRoom *r, int x, int y, int new_x, int new_y);
/* updateOverlay(preserve_floor): nothing without an overlay. */
LadxRoomStatus ladx_room_update_overlay(LadxRoom *r, int preserve_floor);

#endif
