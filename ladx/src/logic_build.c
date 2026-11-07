#include "logic_internal.h"

#include <stddef.h>

LadxReq ladx_dungeon_enemy(const LadxDungeonContext *c, LadxEnemy enemy)
{
	if ((int)enemy < 0 || enemy >= LADX_ENEMY_COUNT || !c->r->enemy_present[enemy]) {
		c->logic->broken = true;
		return ladx_req_none();
	}
	return c->r->enemy_requirements[enemy];
}

/* Every value supported alone, and no pair but the one LADXR's fill
 * refuses: its logic builds (logic_vectors.txt holds five such). */
static bool supported(const LadxSettings *settings)
{
	const LadxSettingPair *p;
	int i;

	for (i = 0; i < LADX_SETTING_COUNT; i++)
		if (ladx_setting_value_reason((LadxSettingId)i, settings->value[i]) != LADX_REASON_NONE)
			return false;
	for (i = 0; (p = ladx_setting_pair(i)) != NULL; i++)
		if (p->reason != LADX_REASON_LADXR_REFUSES && settings->value[p->a] == p->value_a &&
		    (p->b == LADX_SETTING_COUNT || settings->value[p->b] == p->value_b))
			return false;
	return true;
}

static bool world_setup_valid(const LadxWorldSetup *ws)
{
	int i;

	for (i = 0; i < 9; i++)
		if (ws->boss_mapping[i] < 0 || ws->boss_mapping[i] > 8)
			return false;
	for (i = 0; i < LADX_MINIBOSS_KEY_COUNT; i++)
		if ((int)ws->miniboss_mapping[i] < 0 || ws->miniboss_mapping[i] >= LADX_MINIBOSS_COUNT)
			return false;
	return true;
}

typedef int (*DungeonBuild)(const LadxDungeonContext *c);

static const DungeonBuild dungeons[9] = {
	NULL, ladx_dungeon1, ladx_dungeon2, ladx_dungeon3, ladx_dungeon4, NULL, NULL, NULL, NULL,
};

LadxLogicStatus ladx_logic_build_dungeon(LadxLogic *out, const LadxSettings *validated, const LadxWorldSetup *ws,
					 int n)
{
	LadxRequirementsSettings r;
	LadxDungeonContext c;
	int entrance;

	ladx_logic_begin(out);
	if (!supported(validated)) {
		out->broken = true;
		return LADX_LOGIC_UNSUPPORTED;
	}
	if (n < 0 || n > 8 || !dungeons[n] || !world_setup_valid(ws)) {
		out->broken = true;
		return LADX_LOGIC_BROKEN;
	}
	ladx_requirements_settings(&r, out, validated);
	c.logic = out;
	c.options = validated;
	c.world_setup = ws;
	c.r = &r;
	entrance = dungeons[n](&c);
	ladx_logic_finish(out, entrance);
	ladx_logic_configure(out, validated);
	return ladx_logic_broken(out) ? LADX_LOGIC_BROKEN : LADX_LOGIC_OK;
}
