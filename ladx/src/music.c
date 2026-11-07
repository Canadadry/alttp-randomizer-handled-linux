#include "ladx_groups.h"

#include <string.h>

#define BANK2 (0x02u * 0x4000u)
#define SLOTS_AT 0x100u

/* _LOOPING_MUSIC as runs of consecutive tracks, first and last. */
static const uint8_t track_runs[][2] = {
	{0x01, 0x0E}, {0x11, 0x19}, {0x1C, 0x1D}, {0x1F, 0x1F}, {0x21, 0x27}, {0x2F, 0x2F},
	{0x31, 0x33}, {0x37, 0x37}, {0x39, 0x3A}, {0x3C, 0x3C}, {0x3E, 0x3E}, {0x40, 0x40},
	{0x48, 0x4B}, {0x4E, 0x4E}, {0x50, 0x50}, {0x53, 0x55}, {0x57, 0x61},
};

/*
 * _OVERWORLD_MUSIC_REGIONS as rectangles of the 16x16 overworld (room
 * 16y + x): region, x, y, width, height, each region's in order, the
 * regions in LADXR's order (Turtle Rock first, Yarna Desert last).
 */
static const uint8_t region_rects[][5] = {
	{0, 0, 0, 3, 2},
	{1, 3, 0, 3, 2},
	{2, 6, 0, 1, 2},
	{3, 7, 0, 7, 2},
	{4, 14, 0, 2, 2},
	{5, 0, 2, 2, 2},
	{6, 2, 2, 4, 2},
	{7, 6, 2, 10, 2},
	{8, 0, 4, 4, 4}, {8, 0, 8, 1, 1},
	{9, 4, 4, 2, 4},
	{10, 6, 4, 2, 4},
	{11, 8, 4, 4, 4},
	{12, 12, 4, 4, 4},
	{13, 1, 8, 4, 2}, {13, 0, 9, 1, 3}, {13, 1, 10, 3, 2},
	{14, 5, 8, 3, 4}, {14, 4, 10, 1, 2},
	{15, 8, 8, 4, 4},
	{16, 12, 8, 4, 2},
	{17, 12, 10, 4, 2},
	{18, 0, 12, 4, 2},
	{19, 0, 14, 6, 2},
	{20, 4, 12, 4, 2},
	{21, 8, 12, 4, 4}, {21, 6, 14, 2, 2},
	{22, 12, 12, 2, 2},
	{23, 12, 14, 2, 2},
	{24, 14, 12, 2, 4},
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* _LOOPING_MUSIC[index]; index < LADX_MUSIC_TRACK_COUNT. */
static uint8_t track(size_t index)
{
	size_t r;

	for (r = 0; index > (size_t)(track_runs[r][1] - track_runs[r][0]); r++)
		index -= (size_t)(track_runs[r][1] - track_runs[r][0]) + 1;
	return (uint8_t)(track_runs[r][0] + index);
}

static void fill(uint8_t *drawn, const uint8_t *rect, uint8_t value)
{
	int x, y;

	for (y = rect[2]; y < rect[2] + rect[4]; y++)
		for (x = rect[1]; x < rect[1] + rect[3]; x++)
			drawn[16 * y + x] = value;
}

LadxRngStatus ladx_music_randomize(LadxRng *rnd, uint8_t table[LADX_MUSIC_TABLE_SIZE])
{
	uint8_t drawn[LADX_MUSIC_TABLE_SIZE];
	LadxRngStatus st;
	size_t pick, r = 0, i;
	int region;

	for (region = 0; region < LADX_MUSIC_REGION_COUNT; region++) {
		if ((st = ladx_rng_choice(rnd, LADX_STAGE_MUSIC_RANDOMIZE, LADX_MUSIC_TRACK_COUNT, &pick)) != LADX_RNG_OK)
			return st;
		for (; r < COUNT(region_rects) && region_rects[r][0] == region; r++)
			fill(drawn, region_rects[r], track(pick));
	}
	for (i = 0; i < LADX_MUSIC_SLOT_COUNT; i++) {
		if ((st = ladx_rng_choice(rnd, LADX_STAGE_MUSIC_RANDOMIZE, LADX_MUSIC_TRACK_COUNT, &pick)) != LADX_RNG_OK)
			return st;
		drawn[SLOTS_AT + i] = track(pick);
	}
	memcpy(table, drawn, sizeof drawn);
	return LADX_RNG_OK;
}

static int is_random(const LadxPipeline *p)
{
	const char *name = ladx_setting_value_name(LADX_SETTING_MUSIC, p->settings->value[LADX_SETTING_MUSIC]);

	return name && strcmp(name, "random") == 0;
}

int ladx_group_music(LadxEncoder *enc, LadxPipeline *p)
{
	uint8_t table[LADX_MUSIC_TABLE_SIZE];
	LadxSpan group;
	size_t i;

	if (!p->rnd || !p->rom)
		return 0;
	if (!is_random(p))
		return 1;
	if (ladx_music_randomize(p->rnd, table) != LADX_RNG_OK)
		return 0;
	/* Every byte written once: in one record, each is still encoded against its own offset's state. */
	for (i = 0; i < LADX_MUSIC_TABLE_SIZE; i++)
		p->origins[i] = LADX_ENC_KNOWN;
	group.at = "music";
	group.len = 5;
	return ladx_enc_bank(enc, group, BANK2, table, p->origins, LADX_MUSIC_TABLE_SIZE) == LADX_ENC_OK;
}

int ladx_group_music_draws(LadxEncoder *enc, LadxPipeline *p)
{
	uint8_t table[LADX_MUSIC_TABLE_SIZE];

	(void)enc;
	if (!p->rnd)
		return 0;
	return !is_random(p) || ladx_music_randomize(p->rnd, table) == LADX_RNG_OK;
}
