#include "ladx_groups.h"
#include "ladx_room.h"

#include <stdio.h>
#include <string.h>

#define TEXTS 0
#define GOAL_TEXT 0x1A3u
#define SIGN_ENTITY 0x4C
#define EGG_ENTITY 0xDE
#define EGG_ROOM 0x06
#define LAST_ARROW 0x40
#define SIGN_DATA (0x0Cu * LADX_ROM_BANK + 0x3800u)
#define SIGN_NUMBER (0x0Cu * LADX_ROM_BANK + 0x3900u)

/* data/ladx/goal.txt's units, 0-based: the maze's, the counts -1..7, then each specific list by mask 1..255. */
#define UNIT_MAZE 0
#define UNIT_COUNT 1
#define UNIT_SPECIFIC 10
#define UNIT_TOTAL 265

static LadxSpan group_name(void)
{
	LadxSpan s;

	s.at = "goal";
	s.len = 4;
	return s;
}

static int write_text(LadxEncoder *enc, LadxPipeline *p, const char *text)
{
	LadxWlogKey key;
	size_t n, i;

	if (ladx_format_text(text, strlen(text), 0, NULL, p->values, LADX_WLOG_ATOM_MAX, &n, &p->raised) != LADX_TEXT_OK)
		return 0;
	for (i = 0; i < n; i++)
		p->origins[i] = LADX_ENC_KNOWN;
	key.index = GOAL_TEXT;
	key.alt.at = NULL;
	key.alt.len = 0;
	return ladx_enc_table(enc, group_name(), TEXTS, key, p->values, p->origins, n) == LADX_ENC_OK;
}

/* setRequiredInstrumentCount(rom, count). */
static int instrument_count(LadxEncoder *enc, LadxPipeline *p, int count)
{
	char text[48];

	snprintf(text, sizeof text, "You need %d instruments", count);
	if (!write_text(enc, p, text))
		return 0;
	if (count >= 8)
		return 1;
	return ladx_pipeline_const_units(enc, p, "goal", UNIT_COUNT + 1 + (count < 0 ? -1 : count), 1);
}

/* setSpecificInstruments(rom, digits): the text in the drawn order, the code by the sorted set. */
static int specific_instruments(LadxEncoder *enc, LadxPipeline *p, const char *digits)
{
	char text[16 + 8 * 16];
	size_t len = (size_t)snprintf(text, sizeof text, "You need:");
	unsigned mask = 0;
	const char *d;

	for (d = digits; *d; d++) {
		if (*d < '1' || *d > '8' || (mask & (1u << (*d - '1'))))
			return 0;
		mask |= 1u << (*d - '1');
		len += (size_t)snprintf(text + len, sizeof text - len, "\n{INSTRUMENT%c}", *d);
	}
	if (!mask || len >= sizeof text || !write_text(enc, p, text))
		return 0;
	return ladx_pipeline_const_units(enc, p, "goal", UNIT_SPECIFIC + (int)mask - 1, 1);
}

static int put_byte(LadxEncoder *enc, uint32_t offset, int value)
{
	uint8_t v = (uint8_t)value;
	int32_t origin = LADX_ENC_KNOWN;

	return value >= 0 && value <= 0xFF && ladx_enc_bank(enc, group_name(), offset, &v, &origin, 1) == LADX_ENC_OK;
}

/*
 * patchMaze(rom, signs, directions). LADXR zeroes the sign tables, places the signs, stores room 6,
 * then patches; the unit holds the zeroes and the patches, which share no position with the rest.
 */
static int sign_maze(LadxEncoder *enc, LadxPipeline *p, const LadxWorldSetup *ws, LadxRoom *re)
{
	int idx;

	if (ws->sign_count <= 0 || ws->sign_count > LADX_SIGN_MAZE_SIGNS ||
	    !ladx_pipeline_const_units(enc, p, "goal", UNIT_MAZE, 1))
		return 0;
	for (idx = 0; idx < ws->sign_count; idx++) {
		int x = ws->signs[idx].x, y = ws->signs[idx].y, room;
		int arrow = idx < ws->sign_count - 1 ? ws->directions[idx] : LAST_ARROW;

		if (x < 0 || y < 0)
			return 0;
		room = (x / 10) | ((y / 8) << 4);
		if (room > 0xFF || ladx_room_load(re, enc, room, NULL) != LADX_ROOM_OK ||
		    ladx_room_insert_entity(re, 0, x % 10, y % 8, SIGN_ENTITY) != LADX_ROOM_OK ||
		    ladx_room_store(re, enc, group_name(), -1, NULL) != LADX_ROOM_OK ||
		    !put_byte(enc, SIGN_DATA + (uint32_t)room, arrow) || !put_byte(enc, SIGN_NUMBER + (uint32_t)room, idx))
			return 0;
	}
	if (ladx_room_load(re, enc, EGG_ROOM, NULL) != LADX_ROOM_OK)
		return 0;
	ladx_room_remove_entities(re, EGG_ENTITY);
	return ladx_room_store(re, enc, group_name(), -1, NULL) == LADX_ROOM_OK;
}

int ladx_group_goal(LadxEncoder *enc, LadxPipeline *p)
{
	const LadxWorldSetup *ws = ladx_randomizer_world_setup(p->randomizer);

	if (!p->rom || !ws || ladx_pipeline_const_unit_count(p, "goal") != UNIT_TOTAL)
		return 0;
	switch (ws->goal) {
	case LADX_GOAL_MAZE:
		return sign_maze(enc, p, ws, &p->room);
	case LADX_GOAL_SPECIFIC:
		return specific_instruments(enc, p, ws->goal_instruments);
	case LADX_GOAL_OPEN:
		return instrument_count(enc, p, -1);
	case LADX_GOAL_SEASHELLS:
		return 1; /* the goal-fixed table row */
	case LADX_GOAL_VANILLA:
	case LADX_GOAL_INSTRUMENTS:
		return instrument_count(enc, p, ws->goal_count);
	default:
		return 0;
	}
}
