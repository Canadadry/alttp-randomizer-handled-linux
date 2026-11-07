#include "ladx_groups.h"
#include "ladx_sprite_data.h"

#include <string.h>

#define SLOTS 4
#define GRAVESTONE 0xC5
#define ENEMIZER_UNITS 1

/* ENEMY_TYPES as a membership test; the sorted list is 0..255 scanned. */
static int is_enemy(int type)
{
	switch (type) {
	case 0x09: case 0x0B: case 0x0D: case 0x0E: case 0x0F: case 0x12: case 0x14: case 0x15: case 0x16:
	case 0x17: case 0x1A: case 0x1B: case 0x1C: case 0x1E: case 0x1F: case 0x21: case 0x23: case 0x24:
	case 0x28: case 0x29: case 0x55: case 0x56: case 0x57: case 0x91: case 0x99: case 0x9B: case 0x9C:
	case 0x9F: case 0xA0: case 0xA1: case 0xAE: case 0xB9: case 0xBA: case 0xBB: case 0xBD: case 0xC6:
	case 0xCC: case 0xE3: case 0xF2: case 0xF3: case 0xEC: case 0xED: case 0xEE:
		return 1;
	default:
		return 0;
	}
}

/* A sprite slot: unset (LADXR's None) or a set of values, possibly empty. */
typedef struct Slot {
	int set;
	uint32_t bits[8];
} Slot;

static void slot_values(uint32_t bits[8], const LadxSpritePair *pair)
{
	int i;

	memset(bits, 0, 8 * sizeof bits[0]);
	for (i = 0; i < pair->count; i++)
		bits[pair->values[i] >> 5 & 7] |= 1u << (pair->values[i] & 31);
}

/* sprite_data[idx] = value if None else sprite_data[idx].intersection(value) */
static void slot_meet(Slot *s, const LadxSpritePair *pair)
{
	uint32_t bits[8];
	int i;

	slot_values(bits, pair);
	if (!s->set) {
		s->set = 1;
		memcpy(s->bits, bits, sizeof bits);
		return;
	}
	for (i = 0; i < 8; i++)
		s->bits[i] &= bits[i];
}

static int slot_disjoint(const Slot *s, const LadxSpritePair *pair)
{
	uint32_t bits[8];
	int i;

	if (!s->set)
		return 0;
	slot_values(bits, pair);
	for (i = 0; i < 8; i++)
		if (s->bits[i] & bits[i])
			return 0;
	return 1;
}

LadxRngStatus ladx_enemizer_pick(LadxRoom *r, LadxRng *rng, int evilshop, int *found)
{
	Slot slots[SLOTS];
	int list[LADX_ENEMY_TYPE_COUNT], picks[256], present[256];
	LadxSpriteData d;
	LadxRngStatus st;
	size_t i, index;
	int type, m, n = 0, valid;

	memset(slots, 0, sizeof slots);
	memset(present, 0, sizeof present);
	for (type = 0; type < 256; type++)
		if (is_enemy(type))
			list[n++] = type;
	*found = 0;
	for (i = 0; i < r->entity_count; i++) {
		type = r->entities[i].type_id & 0xFF;
		if (is_enemy(type)) {
			present[type] = 1;
			*found = 1;
		} else if (ladx_sprite_data(type, r->room, evilshop, &d) == LADX_SPRITE_DATA) {
			for (m = 0; m < d.count; m++)
				slot_meet(&slots[d.pair[m].slot], &d.pair[m]);
		}
	}
	if (!*found)
		return LADX_RNG_OK;
	for (i = 0; i < r->object_count; i++)
		if (r->objects[i].kind != LADX_ROOM_WARP && r->objects[i].type_id == GRAVESTONE) {
			slots[3].set = 1;
			memset(slots[3].bits, 0, sizeof slots[3].bits);
			slots[3].bits[0x82 >> 5] = 1u << (0x82 & 31);
		}
	for (type = 0; type < 256; type++) {
		if (!present[type])
			continue;
		for (;;) {
			if ((st = ladx_rng_choice(rng, LADX_STAGE_ENEMIES_RANDOMIZE, LADX_ENEMY_TYPE_COUNT, &index)) !=
			    LADX_RNG_OK)
				return st;
			if (ladx_sprite_data(list[index], r->room, evilshop, &d) != LADX_SPRITE_DATA)
				break;
			valid = 1;
			for (m = 0; m < d.count && valid; m++)
				valid = !slot_disjoint(&slots[d.pair[m].slot], &d.pair[m]);
			if (!valid)
				continue;
			for (m = 0; m < d.count; m++)
				slot_meet(&slots[d.pair[m].slot], &d.pair[m]);
			break;
		}
		picks[type] = list[index];
	}
	for (i = 0; i < r->entity_count; i++) {
		type = r->entities[i].type_id & 0xFF;
		if (present[type])
			r->entities[i].type_id = picks[type];
	}
	return LADX_RNG_OK;
}

static int logic_important(int room)
{
	return room == 0x24 || room == 0xE2 || room == 0xF2 || room == 0x5A || room == 0x58;
}

int ladx_enemizer_rooms(LadxEncoder *enc, LadxSpan group, LadxRoom *room, LadxRng *rng, int evilshop)
{
	int n, found;

	for (n = 0; n < 0x100; n++) {
		if (logic_important(n))
			continue;
		if (ladx_room_load(room, enc, n, NULL) != LADX_ROOM_OK)
			return 0;
		if (ladx_enemizer_pick(room, rng, evilshop, &found) != LADX_RNG_OK)
			return 0;
		if (found && ladx_room_store(room, enc, group, -1, NULL) != LADX_ROOM_OK)
			return 0;
	}
	return 1;
}

static int setting_is(const LadxPipeline *p, LadxSettingId id, const char *value)
{
	const char *name = ladx_setting_value_name(id, p->settings->value[id]);

	return name && strcmp(name, value) == 0;
}

int ladx_group_enemizer(LadxEncoder *enc, LadxPipeline *p)
{
	LadxRng rng;
	LadxSpan group;
	const uint8_t *seed;
	size_t n;

	if (!p->rom || !p->rnd)
		return 0;
	if (!setting_is(p, LADX_SETTING_ENEMIES, "overworld"))
		return 1;
	if (ladx_pipeline_const_unit_count(p, "enemizer") != ENEMIZER_UNITS)
		return 0;
	seed = ladx_randomizer_seed(p->randomizer, &n);
	ladx_rng_init(&rng, LADX_RNG_RND, p->rnd->sink, p->rnd->ctx);
	ladx_rng_seed(&rng, LADX_STAGE_ENEMIES_RANDOMIZE, seed, n);
	group.at = "enemizer";
	group.len = strlen(group.at);
	return ladx_enemizer_rooms(enc, group, &p->room, &rng, !setting_is(p, LADX_SETTING_EVILSHOP, "")) &&
	       ladx_pipeline_const(enc, p, "enemizer");
}
