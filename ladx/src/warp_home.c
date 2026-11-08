#include "ladx_groups.h"

#include <stdio.h>
#include <string.h>

#define HANDLER_OPERAND (1u * LADX_ROM_BANK + 0x3E39u) /* + 5k: type, map, room, x, y */
#define LD_A 0x3E
#define BG_TILES 9
#define SQ_BACKGROUND 0x0D
#define WARP_HOME_UNITS 1

static const char *const one_way_names[] = {
	"d0",		  "d1",		  "d3",		     "d4",		    "d6",
	"d8",		  "animal_cave",  "right_fairy",     "rooster_grave",	    "prairie_left_cave2",
	"prairie_left_fairy", "armos_fairy", "boomerang_cave", "madbatter_taltal", "forest_madbatter",
};

static LadxWarpHomeStatus raise_error(LadxOutcome *raised, const char *type, const char *message)
{
	raised->kind = LADX_OUTCOME_CRASH;
	raised->type = type;
	snprintf(raised->text, sizeof raised->text, "%s", message);
	raised->message = raised->text;
	return LADX_WARP_HOME_RAISED;
}

static int one_way(int room)
{
	const LadxEntranceInfo *info;
	size_t count, i, k;

	info = ladx_entrance_info(&count);
	for (k = 0; k < sizeof one_way_names / sizeof one_way_names[0]; k++)
		for (i = 0; i < count; i++)
			if (strcmp(info[i].name, one_way_names[k]) == 0 && info[i].room == room)
				return 1;
	return 0;
}

LadxWarpHomeStatus ladx_warp_home_target(const LadxRoom *r, int force_inside, uint8_t out[LADX_WARP_HOME_OPERANDS],
					 LadxOutcome *raised)
{
	const LadxRoomObject *warp = NULL;
	int room;
	size_t i;

	for (i = 0; i < r->object_count && !warp; i++)
		if (r->objects[i].kind == LADX_ROOM_WARP)
			warp = &r->objects[i];
	if (!warp)
		return raise_error(raised, "IndexError", "list index out of range");
	if (one_way(warp->room) || force_inside || warp->room > 0x100) {
		out[0] = 0x01;
		out[1] = 0x10;
		out[2] = 0xA3;
		out[3] = 0x50;
		out[4] = 0x7F;
		return LADX_WARP_HOME_OK;
	}
	room = warp->room;
	/* LADXR formats the room as "$%02x": 0x100 overflows its 8-bit ld on the handler's line 21. */
	if (room == 0x100)
		return raise_error(raised, "AssemblerException", "([NUMBER:256:21], '8 bit value out of range')");
	out[0] = 0x00;
	out[1] = 0x00;
	out[2] = (uint8_t)room;
	out[3] = (uint8_t)warp->target_x;
	out[4] = (uint8_t)warp->target_y;
	return LADX_WARP_HOME_OK;
}

static int move(LadxBackground *bg, uint32_t to, uint32_t from, LadxOutcome *raised)
{
	char key[16];
	uint8_t v;

	if (!ladx_background_get(bg, from, &v)) {
		snprintf(key, sizeof key, "%lu", (unsigned long)from);
		raise_error(raised, "KeyError", key);
		return 0;
	}
	return ladx_background_set(bg, to, v);
}

LadxWarpHomeStatus ladx_warp_home_edit(LadxBackground *bg, LadxOutcome *raised)
{
	static const uint8_t option[5] = {0xCE, 0xCF, 0xC4, 0x7F, 0x7F};
	uint32_t n;

	for (n = 2; n < 18; n++)
		if (!move(bg, 0x99C0u + n, 0x9980u + n, raised) || !move(bg, 0x99A0u + n, 0x9960u + n, raised) ||
		    !move(bg, 0x9980u + n, 0x9940u + n, raised) || !move(bg, 0x9960u + n, 0x98E0u + n, raised))
			return LADX_WARP_HOME_RAISED;
	for (n = 0; n < 5; n++)
		ladx_background_set(bg, 0x9960u + 10u + n, option[n]);
	return LADX_WARP_HOME_OK;
}

static int setting_is(const LadxPipeline *p, LadxSettingId id, const char *value)
{
	const char *name = ladx_setting_value_name(id, p->settings->value[id]);

	return name && strcmp(name, value) == 0;
}

static int force_inside(const LadxPipeline *p)
{
	return setting_is(p, LADX_SETTING_OVERWORLD, "dungeonchain") ||
	       setting_is(p, LADX_SETTING_ENTRANCESHUFFLE, "chaos") ||
	       setting_is(p, LADX_SETTING_ENTRANCESHUFFLE, "insane") ||
	       setting_is(p, LADX_SETTING_ENTRANCESHUFFLE, "madness");
}

int ladx_group_warp_home(LadxEncoder *enc, LadxPipeline *p)
{
	uint8_t operands[LADX_WARP_HOME_OPERANDS], opcode;
	LadxWlogKey key;
	LadxSpan group;
	size_t len, i;
	uint32_t at;

	if (!p->rom || ladx_pipeline_const_unit_count(p, "warp-home") != WARP_HOME_UNITS)
		return 0;
	group.at = "warp-home";
	group.len = strlen(group.at);
	for (i = 0; i < LADX_WARP_HOME_OPERANDS; i++) {
		at = HANDLER_OPERAND + 5u * (uint32_t)i;
		if (!ladx_pipeline_const_bank(p, "warp-home", 0, at - 1u, &opcode, 1) || opcode != LD_A)
			return 0;
	}
	if (ladx_room_load(&p->room, enc, LADX_WARP_HOME_ROOM, NULL) != LADX_ROOM_OK)
		return 0;
	if (ladx_warp_home_target(&p->room, force_inside(p), operands, &p->raised) != LADX_WARP_HOME_OK)
		return 0;
	if (!ladx_pipeline_const(enc, p, "warp-home"))
		return 0;
	for (i = 0; i < LADX_WARP_HOME_OPERANDS; i++) {
		p->origins[0] = LADX_ENC_KNOWN;
		if (ladx_enc_bank(enc, group, HANDLER_OPERAND + 5u * (uint32_t)i, &operands[i], p->origins, 1) !=
		    LADX_ENC_OK)
			return 0;
	}
	key.index = SQ_BACKGROUND;
	key.alt.at = NULL;
	key.alt.len = 0;
	if (!ladx_enc_entry_value(enc, BG_TILES, key, p->values, p->origins, LADX_WLOG_ATOM_MAX, &len) ||
	    !ladx_background_decode(&p->background, p->values, len))
		return 0;
	if (ladx_warp_home_edit(&p->background, &p->raised) != LADX_WARP_HOME_OK)
		return 0;
	if ((len = ladx_background_encode(&p->background, p->values, LADX_WLOG_ATOM_MAX)) == 0)
		return 0;
	for (i = 0; i < len; i++)
		p->origins[i] = LADX_ENC_KNOWN;
	return ladx_enc_table(enc, group, BG_TILES, key, p->values, p->origins, len) == LADX_ENC_OK;
}
