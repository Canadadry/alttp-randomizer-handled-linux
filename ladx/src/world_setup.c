#include "ladx_logic.h"

#include <string.h>

static const char *const miniboss_names[LADX_MINIBOSS_COUNT] = {
	"ROLLING_BONES", "HINOX", "DODONGO", "CUE_BALL", "GHOMA", "SMASHER",
	"GRIM_CREEPER", "BLAINO", "AVALAUNCH", "GIANT_BUZZ_BLOB", "MOBLIN_KING", "ARMOS_KNIGHT",
};

static const char *const miniboss_keys[LADX_MINIBOSS_KEY_COUNT] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "c1", "c2", "moblin_cave", "armos_temple",
};

/* LADX_GOAL_SPECIFIC has no name: LADXR stores "=<digits>". */
static const char *const goal_names[LADX_GOAL_COUNT] = {
	"vanilla", "instruments", "open", "seashells", "maze", NULL,
};

void ladx_world_setup_defaults(LadxWorldSetup *ws)
{
	int i;

	for (i = 0; i < 9; i++)
		ws->boss_mapping[i] = i;
	for (i = 0; i < LADX_MINIBOSS_KEY_COUNT; i++)
		ws->miniboss_mapping[i] = (LadxMiniboss)i;
	ws->goal = LADX_GOAL_VANILLA;
	ws->goal_instruments[0] = '\0';
	ws->goal_count = 8;
	ws->sign_count = 0;
	ws->multichest = LADX_ITEM_RUPEES_20;
	ws->inside_to_outside = true;
	ws->keep_two_way = true;
	ws->one_on_one = true;
}

const char *ladx_miniboss_key(int i)
{
	return i >= 0 && i < LADX_MINIBOSS_KEY_COUNT ? miniboss_keys[i] : NULL;
}

const char *ladx_miniboss_name(int miniboss)
{
	return miniboss >= 0 && miniboss < LADX_MINIBOSS_COUNT ? miniboss_names[miniboss] : NULL;
}

int ladx_miniboss_find(const char *name)
{
	int i;

	for (i = 0; i < LADX_MINIBOSS_COUNT; i++)
		if (strcmp(miniboss_names[i], name) == 0)
			return i;
	return -1;
}

const char *ladx_goal_name(int goal)
{
	return goal >= 0 && goal < LADX_GOAL_COUNT ? goal_names[goal] : NULL;
}

int ladx_goal_find(const char *name)
{
	int i;

	for (i = 0; i < LADX_GOAL_COUNT; i++)
		if (goal_names[i] && strcmp(goal_names[i], name) == 0)
			return i;
	return -1;
}
