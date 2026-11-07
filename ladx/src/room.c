#include "ladx_room.h"

#include <string.h>

#define ENTITIES 1
#define TOP 2
#define BOTTOM 3
#define INDOOR_A 4
#define INDOOR_B 5
#define COLOR 6
#define ROOM_COUNT 0x320
#define BANK 0x4000u

/* roomEditor.py's ALT_ROOM_OVERLAYS: offsets in bank 0x27. */
static const struct {
	const char *alt;
	uint32_t at;
} alt_overlays[] = {
	{"Alt06", 0x1040}, {"Alt0E", 0x1090}, {"Alt1B", 0x10E0}, {"Alt2B", 0x1130}, {"Alt79", 0x1180}, {"Alt8C", 0x11D0},
};

/* The tables an alt room is looked up in, in RoomEditor's order. */
static const int alt_tables[] = {TOP, BOTTOM, INDOOR_A};

static LadxWlogKey index_key(uint32_t index)
{
	LadxWlogKey k;

	k.index = index;
	k.alt.at = NULL;
	k.alt.len = 0;
	return k;
}

static LadxWlogKey alt_key(const char *alt)
{
	LadxWlogKey k;

	k.index = 0;
	k.alt.at = alt;
	k.alt.len = strlen(alt);
	return k;
}

/* The objects entry of an index room. */
static void objects_place(int room, int *table, uint32_t *index)
{
	static const int tables[] = {TOP, BOTTOM, INDOOR_A, INDOOR_A, INDOOR_B, INDOOR_B, COLOR};
	static const int bases[] = {0, 0x80, 0x100, 0x100, 0x200, 0x200, 0x300};
	int k = room < 0x80 ? 0 : room < 0x100 ? 1 : room < 0x200 ? 2 : room < 0x300 ? 4 : 6;

	*table = tables[k];
	*index = (uint32_t)(room - bases[k]);
}

/* The alt key's table, -1 when none holds it. */
static int alt_table(const LadxEncoder *enc, const char *alt)
{
	size_t i;
	uint32_t pointer;

	for (i = 0; i < sizeof alt_tables / sizeof alt_tables[0]; i++)
		if (ladx_enc_entry_pointer(enc, alt_tables[i], alt_key(alt), &pointer) >= 0)
			return alt_tables[i];
	return -1;
}

/* The overlay's absolute offset, -1 for none. */
static int32_t overlay_at(int room, const char *alt)
{
	size_t i;

	if (!alt)
		return room < 0xCC    ? (int32_t)(0x26 * BANK + (uint32_t)room * 80)
		       : room < 0x100 ? (int32_t)(0x27 * BANK + (uint32_t)(room - 0xCC) * 80)
				      : -1;
	for (i = 0; i < sizeof alt_overlays / sizeof alt_overlays[0]; i++)
		if (strcmp(alt, alt_overlays[i].alt) == 0)
			return (int32_t)(0x27 * BANK + alt_overlays[i].at);
	return -1;
}

static LadxRoomStatus read_entry(LadxRoom *r, const LadxEncoder *enc, int table, LadxWlogKey key, size_t *n)
{
	uint32_t pointer;
	int kind = ladx_enc_entry_pointer(enc, table, key, &pointer);

	if (kind != 0)
		return LADX_ROOM_MALFORMED;
	if (!ladx_enc_entry_origins(enc, table, key, r->raw_origin, LADX_ROOM_RAW_MAX, n))
		return LADX_ROOM_FULL;
	if (!ladx_enc_entry_value(enc, table, key, r->raw, r->raw_origin, LADX_ROOM_RAW_MAX, n))
		return LADX_ROOM_NO_ROM;
	return LADX_ROOM_OK;
}

static LadxRoomStatus parse_entities(LadxRoom *r, const uint8_t *raw, size_t n)
{
	size_t idx = 0;

	r->entity_count = 0;
	while (idx < n && raw[idx] != 0xFF) {
		if (idx + 1 >= n)
			return LADX_ROOM_MALFORMED;
		if (r->entity_count == LADX_ROOM_ENTITY_MAX)
			return LADX_ROOM_FULL;
		r->entities[r->entity_count].x = raw[idx] & 0x0F;
		r->entities[r->entity_count].y = raw[idx] >> 4;
		r->entities[r->entity_count].type_id = raw[idx + 1];
		r->entity_count++;
		idx += 2;
	}
	return idx + 1 == n ? LADX_ROOM_OK : LADX_ROOM_MALFORMED;
}

static void set_warp(LadxRoomObject *o, int warp_type, int map_nr, int room_nr, int target_x, int target_y)
{
	if (warp_type > 0)
		room_nr += map_nr == 0xFF ? 0x300 : (map_nr >= 0x06 && map_nr < 0x1A) ? 0x200 : 0x100;
	o->kind = LADX_ROOM_WARP;
	o->x = o->y = o->type_id = o->count = 0;
	o->type_origin = LADX_ENC_KNOWN;
	o->warp_type = warp_type;
	o->map_nr = map_nr;
	o->room = room_nr;
	o->target_x = target_x;
	o->target_y = target_y;
}

static LadxRoomStatus parse_objects(LadxRoom *r, const uint8_t *raw, const int32_t *origins, size_t n)
{
	size_t idx = 2;

	if (n < 3)
		return LADX_ROOM_MALFORMED;
	r->animation_id = raw[0];
	r->floor_object = raw[1];
	r->floor_origin = origins[1];
	r->object_count = 0;
	while (idx < n && raw[idx] != 0xFE) {
		LadxRoomObject *o;
		int kind = raw[idx] >> 4, width;

		if (r->object_count == LADX_ROOM_OBJECT_MAX)
			return LADX_ROOM_FULL;
		o = &r->objects[r->object_count];
		width = kind == 0x08 || kind == 0x0C ? 3 : kind == 0x0E ? 5 : 2;
		if (idx + (size_t)width > n)
			return LADX_ROOM_MALFORMED;
		if (kind == 0x0E) {
			set_warp(o, raw[idx] & 0x0F, raw[idx + 1], raw[idx + 2], raw[idx + 3], raw[idx + 4]);
		} else if (width == 3) {
			o->kind = kind == 0x08 ? LADX_ROOM_HORIZONTAL : LADX_ROOM_VERTICAL;
			o->count = raw[idx] & 0x0F;
			o->x = raw[idx + 1] & 0x0F;
			o->y = raw[idx + 1] >> 4;
			o->type_id = raw[idx + 2];
			o->type_origin = origins[idx + 2];
		} else {
			o->kind = LADX_ROOM_SINGLE;
			o->count = 0;
			o->x = raw[idx] & 0x0F;
			o->y = raw[idx] >> 4;
			o->type_id = raw[idx + 1];
			o->type_origin = origins[idx + 1];
		}
		r->object_count++;
		idx += (size_t)width;
	}
	return idx + 1 == n ? LADX_ROOM_OK : LADX_ROOM_MALFORMED;
}

LadxRoomStatus ladx_room_load(LadxRoom *r, const LadxEncoder *enc, int room, const char *alt)
{
	LadxRoomStatus st;
	int table;
	uint32_t index = 0;
	int32_t at;
	size_t n, i;

	if (alt) {
		if (strlen(alt) > LADX_WLOG_NAME_MAX || (table = alt_table(enc, alt)) < 0)
			return LADX_ROOM_BAD_ROOM;
		r->room = -1;
		strcpy(r->alt, alt);
	} else {
		if (room < 0 || room >= ROOM_COUNT)
			return LADX_ROOM_BAD_ROOM;
		r->room = room;
		r->alt[0] = '\0';
		objects_place(room, &table, &index);
	}
	r->entity_count = 0;
	r->tileset_index = r->palette_index = r->attribset_bank = r->attribset_addr = LADX_ROOM_NONE;
	if (!alt) {
		if ((st = read_entry(r, enc, ENTITIES, index_key((uint32_t)room), &n)) != LADX_ROOM_OK ||
		    (st = parse_entities(r, r->raw, n)) != LADX_ROOM_OK)
			return st;
	}
	if ((st = read_entry(r, enc, table, alt ? alt_key(r->alt) : index_key(index), &n)) != LADX_ROOM_OK ||
	    (st = parse_objects(r, r->raw, r->raw_origin, n)) != LADX_ROOM_OK)
		return st;
	at = overlay_at(room, alt ? r->alt : NULL);
	r->has_overlay = at >= 0;
	for (i = 0; at >= 0 && i < LADX_ROOM_OVERLAY; i++)
		if (!ladx_enc_bank_value(enc, (uint32_t)at + (uint32_t)i, &r->overlay[i], &r->overlay_origin[i]))
			return LADX_ROOM_NO_ROM;
	return LADX_ROOM_OK;
}

/* Store ------------------------------------------------------------------------------------------- */

static int byte_ok(int v)
{
	return v >= 0 && v <= 0xFF;
}

/* Object.export() and its subclasses' onto out; 0 on a byte out of range. */
static size_t export_object(const LadxRoomObject *o, uint8_t *out)
{
	int b[5], n, i;

	switch (o->kind) {
	case LADX_ROOM_WARP:
		b[0] = 0xE0 | o->warp_type;
		b[1] = o->map_nr;
		b[2] = o->room & 0xFF;
		b[3] = o->target_x;
		b[4] = o->target_y;
		n = 5;
		if (o->warp_type < 0 || o->room < 0)
			return 0;
		break;
	case LADX_ROOM_HORIZONTAL:
	case LADX_ROOM_VERTICAL:
		b[0] = (o->kind == LADX_ROOM_HORIZONTAL ? 0x80 : 0xC0) | o->count;
		b[1] = o->x | (o->y << 4);
		b[2] = o->type_id;
		n = 3;
		if (o->count < 0 || o->x < 0 || o->y < 0)
			return 0;
		break;
	default:
		b[0] = o->x | (o->y << 4);
		b[1] = o->type_id;
		n = 2;
		if (o->x < 0 || o->y < 0)
			return 0;
	}
	for (i = 0; i < n; i++) {
		if (!byte_ok(b[i]))
			return 0;
		out[i] = (uint8_t)b[i];
	}
	return (size_t)n;
}

static LadxRoomStatus put_byte(LadxEncoder *enc, LadxSpan group, uint32_t offset, int value)
{
	uint8_t v = (uint8_t)value;
	int32_t origin = LADX_ENC_KNOWN;

	return ladx_enc_bank(enc, group, offset, &v, &origin, 1) == LADX_ENC_OK ? LADX_ROOM_OK : LADX_ROOM_ENCODER;
}

/* The entry in r->raw, its bytes with no origin (LADXR builds plain bytearrays). */
static LadxRoomStatus put_entry(LadxRoom *r, LadxEncoder *enc, LadxSpan group, int table, LadxWlogKey key, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		r->raw_origin[i] = LADX_ENC_KNOWN;
	return ladx_enc_table(enc, group, table, key, r->raw, r->raw_origin, n) == LADX_ENC_OK ? LADX_ROOM_OK
											       : LADX_ROOM_ENCODER;
}

/* The optional bytes RoomEditor.store writes for an index room. */
static LadxRoomStatus store_extras(const LadxRoom *r, LadxEncoder *enc, LadxSpan group, int nr)
{
	LadxRoomStatus st = LADX_ROOM_OK;

	if (nr < 0x100) {
		if (r->tileset_index != LADX_ROOM_NONE)
			st = put_byte(enc, group, 0x3F * BANK + 0x3F00 + (uint32_t)nr, r->tileset_index & 0xFF);
		if (st == LADX_ROOM_OK && r->attribset_bank != LADX_ROOM_NONE) {
			if (!byte_ok(r->attribset_bank) || r->attribset_addr < 0 || r->attribset_addr > 0xFFFF)
				return LADX_ROOM_RANGE;
			st = put_byte(enc, group, 0x1A * BANK + 0x2476 + (uint32_t)nr, r->attribset_bank);
			if (st == LADX_ROOM_OK)
				st = put_byte(enc, group, 0x1A * BANK + 0x1E76 + (uint32_t)nr * 2,
					      r->attribset_addr & 0xFF);
			if (st == LADX_ROOM_OK)
				st = put_byte(enc, group, 0x1A * BANK + 0x1E76 + (uint32_t)nr * 2 + 1,
					      r->attribset_addr >> 8);
		}
		if (st == LADX_ROOM_OK && r->palette_index != LADX_ROOM_NONE) {
			if (!byte_ok(r->palette_index))
				return LADX_ROOM_RANGE;
			st = put_byte(enc, group, 0x21 * BANK + 0x02EF + (uint32_t)nr, r->palette_index);
		}
	} else if (r->tileset_index != LADX_ROOM_NONE) {
		if (!byte_ok(r->tileset_index))
			return LADX_ROOM_RANGE;
		st = put_byte(enc, group, 0x20 * BANK + 0x2EB3 + (uint32_t)(nr - 0x100), r->tileset_index);
	}
	return st;
}

LadxRoomStatus ladx_room_store(LadxRoom *r, LadxEncoder *enc, LadxSpan group, int new_room, const char *new_alt)
{
	uint8_t *raw = r->raw;
	LadxRoomStatus st;
	size_t n = 2, i, w;
	int table;
	uint32_t index = 0;
	int32_t at;

	if (new_room < 0 && !new_alt) {
		new_room = r->room;
		new_alt = r->room < 0 ? r->alt : NULL;
	}
	if (new_alt) {
		if (strlen(new_alt) > LADX_WLOG_NAME_MAX || (table = alt_table(enc, new_alt)) < 0)
			return LADX_ROOM_BAD_ROOM;
	} else {
		if (new_room < 0 || new_room >= ROOM_COUNT)
			return LADX_ROOM_BAD_ROOM;
		objects_place(new_room, &table, &index);
	}
	if (!byte_ok(r->animation_id) || !byte_ok(r->floor_object))
		return LADX_ROOM_RANGE;
	raw[0] = (uint8_t)r->animation_id;
	raw[1] = (uint8_t)r->floor_object;
	for (i = 0; i < r->object_count; i++) {
		if (n + 5 + 1 > LADX_ROOM_RAW_MAX)
			return LADX_ROOM_RANGE;
		if (!(w = export_object(&r->objects[i], raw + n)))
			return LADX_ROOM_RANGE;
		n += w;
	}
	raw[n++] = 0xFE;
	if ((st = put_entry(r, enc, group, table, new_alt ? alt_key(new_alt) : index_key(index), n)) != LADX_ROOM_OK)
		return st;
	if (new_alt) {
		at = overlay_at(-1, new_alt);
	} else {
		if ((st = store_extras(r, enc, group, new_room)) != LADX_ROOM_OK)
			return st;
		n = 0;
		for (i = 0; i < r->entity_count; i++) {
			const LadxRoomEntity *e = &r->entities[i];

			if (n + 2 + 1 > LADX_ROOM_RAW_MAX)
				return LADX_ROOM_RANGE;
			if (e->x < 0 || e->y < 0 || !byte_ok(e->x | (e->y << 4)) || !byte_ok(e->type_id))
				return LADX_ROOM_RANGE;
			raw[n++] = (uint8_t)(e->x | (e->y << 4));
			raw[n++] = (uint8_t)e->type_id;
		}
		raw[n++] = 0xFF;
		if ((st = put_entry(r, enc, group, ENTITIES, index_key((uint32_t)new_room), n)) != LADX_ROOM_OK)
			return st;
		at = overlay_at(new_room, NULL);
	}
	if (at < 0)
		return LADX_ROOM_OK;
	if (!r->has_overlay)
		return LADX_ROOM_RANGE;
	return ladx_enc_bank(enc, group, (uint32_t)at, r->overlay, r->overlay_origin, LADX_ROOM_OVERLAY) == LADX_ENC_OK
		       ? LADX_ROOM_OK
		       : LADX_ROOM_ENCODER;
}

/* Edits ------------------------------------------------------------------------------------------- */

LadxRoomStatus ladx_room_insert_entity(LadxRoom *r, size_t index, int x, int y, int type_id)
{
	if (r->entity_count == LADX_ROOM_ENTITY_MAX)
		return LADX_ROOM_FULL;
	if (index > r->entity_count)
		index = r->entity_count;
	memmove(r->entities + index + 1, r->entities + index, (r->entity_count - index) * sizeof r->entities[0]);
	r->entities[index].x = x;
	r->entities[index].y = y;
	r->entities[index].type_id = type_id;
	r->entity_count++;
	return LADX_ROOM_OK;
}

LadxRoomStatus ladx_room_add_entity(LadxRoom *r, int x, int y, int type_id)
{
	return ladx_room_insert_entity(r, r->entity_count, x, y, type_id);
}

void ladx_room_remove_entities(LadxRoom *r, int type_id)
{
	size_t i, n = 0;

	for (i = 0; i < r->entity_count; i++)
		if (r->entities[i].type_id != type_id)
			r->entities[n++] = r->entities[i];
	r->entity_count = n;
}

int ladx_room_has_entity(const LadxRoom *r, int type_id)
{
	size_t i;

	for (i = 0; i < r->entity_count; i++)
		if (r->entities[i].type_id == type_id)
			return 1;
	return 0;
}

int ladx_room_has_object(const LadxRoom *r, int type_id)
{
	size_t i;

	for (i = 0; i < r->object_count; i++)
		if (r->objects[i].kind != LADX_ROOM_WARP && r->objects[i].type_id == type_id)
			return 1;
	return 0;
}

static int at_xy(const LadxRoomObject *o, int x, int y)
{
	return o->kind != LADX_ROOM_WARP && o->x == x && o->y == y;
}

void ladx_room_remove_object(LadxRoom *r, int x, int y)
{
	size_t i, n = 0;

	for (i = 0; i < r->object_count; i++)
		if (!at_xy(&r->objects[i], x, y))
			r->objects[n++] = r->objects[i];
	r->object_count = n;
}

/* overlay[i] = value, Python's index rules; 0 where it raises. */
static int set_overlay(LadxRoom *r, int i, int value, int32_t origin)
{
	if (i < 0)
		i += LADX_ROOM_OVERLAY;
	if (i < 0 || i >= LADX_ROOM_OVERLAY || !byte_ok(value))
		return 0;
	r->overlay[i] = (uint8_t)value;
	r->overlay_origin[i] = origin;
	return 1;
}

LadxRoomStatus ladx_room_change_object(LadxRoom *r, int x, int y, int new_type)
{
	size_t i;

	for (i = 0; i < r->object_count; i++) {
		if (!at_xy(&r->objects[i], x, y))
			continue;
		r->objects[i].type_id = new_type;
		r->objects[i].type_origin = LADX_ENC_KNOWN;
		if (r->has_overlay && !set_overlay(r, x + y * 10, new_type, LADX_ENC_KNOWN))
			return LADX_ROOM_RANGE;
	}
	return LADX_ROOM_OK;
}

LadxRoomStatus ladx_room_move_object(LadxRoom *r, int x, int y, int new_x, int new_y)
{
	size_t i;

	for (i = 0; i < r->object_count; i++) {
		LadxRoomObject *o = &r->objects[i];

		if (!at_xy(o, x, y))
			continue;
		if (r->has_overlay && (!set_overlay(r, x + y * 10, r->floor_object, r->floor_origin) ||
				       !set_overlay(r, new_x + new_y * 10, o->type_id, o->type_origin)))
			return LADX_ROOM_RANGE;
		o->x = new_x;
		o->y = new_y;
	}
	return LADX_ROOM_OK;
}

LadxRoomStatus ladx_room_update_overlay(LadxRoom *r, int preserve_floor)
{
	size_t i;
	int n;

	if (!r->has_overlay)
		return LADX_ROOM_OK;
	if (!preserve_floor)
		for (n = 0; n < LADX_ROOM_OVERLAY; n++)
			if (!set_overlay(r, n, r->floor_object, r->floor_origin))
				return LADX_ROOM_RANGE;
	for (i = 0; i < r->object_count; i++) {
		const LadxRoomObject *o = &r->objects[i];
		int ok = 1;

		if (o->kind == LADX_ROOM_HORIZONTAL)
			for (n = 0; ok && n < o->count; n++)
				ok = set_overlay(r, o->x + n + o->y * 10, o->type_id, o->type_origin);
		else if (o->kind == LADX_ROOM_VERTICAL)
			for (n = 0; ok && n < o->count; n++)
				ok = set_overlay(r, o->x + n * 10 + o->y * 10, o->type_id, o->type_origin);
		else if (o->kind == LADX_ROOM_SINGLE)
			ok = set_overlay(r, o->x + o->y * 10, o->type_id, o->type_origin);
		if (!ok)
			return LADX_ROOM_RANGE;
	}
	return LADX_ROOM_OK;
}
