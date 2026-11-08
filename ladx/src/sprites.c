#include "ladx_groups.h"
#include "ladx_sprite_data.h"

#include <stdio.h>
#include <string.h>

#define GRAVESTONE 0xC5
#define SKIPPED_ROOM 0x2FF
#define TABLE_OVERWORLD 7
#define TABLE_INDOOR 8
#define SPRITES_UNITS 1
#define SET_SLOTS 8 /* CPython's smallest set table: no resize below 5 entries */
#define PERTURB_SHIFT 5

typedef enum Kind { NONE = 0, INT, SET } Kind;

typedef struct Slot {
	Kind kind;
	int value;
	size_t n;
	int set[LADX_SPRITES_SET_MAX];
} Slot;

size_t ladx_sprites_set_order(const int *values, size_t n, int *out)
{
	int table[SET_SLOTS];
	size_t i, k, size = 0;
	unsigned slot, perturb;

	for (i = 0; i < SET_SLOTS; i++)
		table[i] = -1;
	for (k = 0; k < n && k < LADX_SPRITES_SET_MAX; k++) {
		/* set_add_entry: no linear probes on 8 slots, so each collision perturbs at once. */
		slot = (unsigned)values[k] & (SET_SLOTS - 1);
		perturb = (unsigned)values[k];
		while (table[slot] != -1 && table[slot] != values[k]) {
			perturb >>= PERTURB_SHIFT;
			slot = (slot * 5u + 1u + perturb) & (SET_SLOTS - 1);
		}
		if (table[slot] == -1) {
			table[slot] = values[k];
			size++;
		}
	}
	for (i = 0, k = 0; i < SET_SLOTS; i++)
		if (table[i] != -1)
			out[k++] = table[i];
	return size;
}

static size_t iteration(const Slot *s, int *out)
{
	return ladx_sprites_set_order(s->set, s->n, out);
}

static int set_has(const Slot *s, int v)
{
	size_t i;

	for (i = 0; i < s->n; i++)
		if (s->set[i] == v)
			return 1;
	return 0;
}

size_t ladx_sprites_set_intersection(const int *so, size_t so_n, const int *other, size_t other_n, int *out)
{
	int order[LADX_SPRITES_SET_MAX];
	const int *walk = other, *in = so;
	size_t walk_n = other_n, in_n = so_n, n, i, j, k = 0;

	if (ladx_sprites_set_order(other, other_n, order) > ladx_sprites_set_order(so, so_n, order)) {
		walk = so;
		walk_n = so_n;
		in = other;
		in_n = other_n;
	}
	n = ladx_sprites_set_order(walk, walk_n, order);
	for (i = 0; i < n; i++)
		for (j = 0; j < in_n; j++)
			if (in[j] == order[i]) {
				out[k++] = order[i];
				break;
			}
	return k;
}

static void intersect(Slot *so, const Slot *other)
{
	int kept[LADX_SPRITES_SET_MAX];

	so->n = ladx_sprites_set_intersection(so->set, so->n, other->set, other->n, kept);
	memcpy(so->set, kept, so->n * sizeof kept[0]);
}

static int print_slot(char *buf, size_t cap, const Slot *s)
{
	int order[LADX_SPRITES_SET_MAX];
	size_t n, i;
	int len;

	if (s->kind == INT)
		return snprintf(buf, cap, "%d", s->value);
	n = iteration(s, order);
	len = snprintf(buf, cap, "{");
	for (i = 0; i < n; i++)
		len += snprintf(buf + len, cap - (size_t)len, i ? ", %d" : "%d", order[i]);
	return len + snprintf(buf + len, cap - (size_t)len, "}");
}

static LadxSpritesStatus raise_error(LadxOutcome *raised, const char *type)
{
	raised->kind = LADX_OUTCOME_CRASH;
	raised->type = type;
	raised->message = raised->text;
	return LADX_SPRITES_RAISED;
}

static LadxSpritesStatus cannot_load(LadxOutcome *raised, int room, int entity, int idx, const Slot *value,
				     const Slot *active)
{
	char failed[24], now[24], message[160] = "";

	print_slot(failed, sizeof failed, value);
	print_slot(now, sizeof now, active);
	/* At most 92 characters in tier 1: one side is an int, the other a set of two. */
	snprintf(message, sizeof message,
		 "Room: %03x cannot load graphics for entity: %02x (Index: %d Failed: %s, Active: %s)", room, entity,
		 idx, failed, now);
	message[sizeof raised->text - 1] = '\0';
	memcpy(raised->text, message, sizeof raised->text);
	return raise_error(raised, "AssertionError");
}

static void set_int(Slot *s, int v)
{
	s->kind = INT;
	s->value = v;
}

LadxSpritesStatus ladx_sprites_room(const LadxRoom *r, int evilshop, uint8_t out[LADX_SPRITES_SLOTS],
				    LadxOutcome *raised)
{
	Slot values[LADX_SPRITES_SLOTS], value;
	LadxSpriteData d;
	LadxSpriteKind kind;
	int order[LADX_SPRITES_SET_MAX], entity, m, idx;
	Slot *v;
	size_t i;

	memset(values, 0, sizeof values);
	if (r->room == 0x00E) {
		set_int(&values[2], 0xD6);
		set_int(&values[3], 0xD7);
	}
	if (r->room >= 0x211 && r->room <= 0x21E)
		set_int(&values[0], 0x66);
	for (i = 0; i < r->object_count; i++)
		if (r->objects[i].kind != LADX_ROOM_WARP && r->objects[i].type_id == GRAVESTONE && r->room < 0x100)
			set_int(&values[3], 0x82);
	for (i = 0; i < r->entity_count; i++) {
		entity = r->entities[i].type_id & 0xFF;
		kind = ladx_sprite_data(entity, r->room, evilshop, &d);
		if (kind == LADX_SPRITE_ABSENT) {
			snprintf(raised->text, sizeof raised->text, "%d", entity);
			return raise_error(raised, "KeyError");
		}
		if (kind == LADX_SPRITE_NONE)
			continue;
		for (m = 0; m < d.count; m++) {
			idx = d.pair[m].slot;
			v = &values[idx];
			memset(&value, 0, sizeof value);
			if (d.pair[m].count == 1) {
				set_int(&value, d.pair[m].values[0]);
			} else {
				value.kind = SET;
				value.n = (size_t)d.pair[m].count;
				value.set[0] = d.pair[m].values[0];
				value.set[1] = d.pair[m].values[1];
			}
			if (v->kind == NONE) {
				*v = value;
			} else if (v->kind == SET && value.kind == SET) {
				intersect(v, &value);
				if (v->n == 0) {
					raised->text[0] = '\0';
					return raise_error(raised, "AssertionError");
				}
			} else if (v->kind == SET && set_has(v, value.value)) {
				set_int(v, value.value);
			} else if (value.kind == SET && set_has(&value, v->value)) {
				continue;
			} else if (v->kind == INT && value.kind == INT && v->value == value.value) {
				continue;
			} else {
				return cannot_load(raised, r->room, entity, idx, &value, v);
			}
		}
	}
	for (m = 0; m < LADX_SPRITES_SLOTS; m++) {
		if (values[m].kind == SET) {
			iteration(&values[m], order);
			out[m] = (uint8_t)order[0];
		} else {
			out[m] = values[m].kind == INT ? (uint8_t)values[m].value : 0xFF;
		}
	}
	return LADX_SPRITES_OK;
}

static int setting_is(const LadxPipeline *p, LadxSettingId id, const char *value)
{
	const char *name = ladx_setting_value_name(id, p->settings->value[id]);

	return name && strcmp(name, value) == 0;
}

int ladx_group_sprites(LadxEncoder *enc, LadxPipeline *p)
{
	uint8_t data[LADX_SPRITES_SLOTS];
	int32_t origins[LADX_SPRITES_SLOTS];
	LadxWlogKey key;
	LadxSpan group;
	int nr, evilshop, m;

	if (!p->rom || ladx_pipeline_const_unit_count(p, "sprites") != SPRITES_UNITS ||
	    !ladx_pipeline_const(enc, p, "sprites"))
		return 0;
	group.at = "sprites";
	group.len = strlen(group.at);
	evilshop = !setting_is(p, LADX_SETTING_EVILSHOP, "");
	for (m = 0; m < LADX_SPRITES_SLOTS; m++)
		origins[m] = LADX_ENC_KNOWN;
	memset(&key, 0, sizeof key);
	for (nr = 0; nr < LADX_SPRITES_ROOMS; nr++) {
		if (nr == SKIPPED_ROOM)
			continue;
		if (ladx_room_load(&p->room, enc, nr, NULL) != LADX_ROOM_OK)
			return 0;
		if (ladx_sprites_room(&p->room, evilshop, data, &p->raised) != LADX_SPRITES_OK)
			return 0;
		key.index = (uint32_t)(nr < 0x100 ? nr : nr - 0x100);
		if (ladx_enc_table(enc, group, nr < 0x100 ? TABLE_OVERWORLD : TABLE_INDOOR, key, data, origins,
				   LADX_SPRITES_SLOTS) != LADX_ENC_OK)
			return 0;
	}
	return 1;
}
