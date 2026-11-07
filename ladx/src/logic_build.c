#include "logic_internal.h"

#include <stddef.h>
#include <stdio.h>

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
	ladx_dungeon_color, ladx_dungeon1, ladx_dungeon2, ladx_dungeon3, ladx_dungeon4,
	ladx_dungeon5, ladx_dungeon6, ladx_dungeon7, ladx_dungeon8,
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
	if (n < 0 || n > 8 || !world_setup_valid(ws)) {
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

/* World and its entrances, then main.py's loop over the identity
 * entrance_mapping: every key to its ":inside" in ENTRANCE_INFO's order,
 * then every ":inside" back. */
static void connect_entrances(LadxLogic *logic, LadxWorld *w)
{
	LadxReqArena *a = ladx_logic_arena_mut(logic);
	bool cycle[2 * LADX_ENTRANCE_COUNT];
	int i, source, target, k;

	for (i = 0; i < 2 * LADX_ENTRANCE_COUNT; i++)
		if (!w->entrance[i].added) {
			logic->broken = true; /* assert k in world.entrances */
			return;
		}
	for (i = 0; i < 2 * LADX_ENTRANCE_COUNT; i++) {
		LadxEntranceExterior *se, *te;

		source = i < LADX_ENTRANCE_COUNT ? 2 * i : 2 * (i - LADX_ENTRANCE_COUNT) + 1;
		target = source ^ 1;
		se = &w->entrance[source];
		if (se->location < 0)
			continue;
		te = &w->entrance[target];
		for (k = 0; k < 2 * LADX_ENTRANCE_COUNT; k++)
			cycle[k] = false;
		cycle[target] = true;
		while (te->location < 0) {
			if (te->requirement.tag != LADX_REQ_NONE || te->enter_set || te->exit_set) {
				logic->broken = true;
				return;
			}
			target ^= 1;
			if (cycle[target])
				break;
			cycle[target] = true;
			te = &w->entrance[target];
		}
		if (te->location < 0 || te->location == se->location)
			continue;
		if (se->requirement.tag != LADX_REQ_NONE && te->requirement.tag != LADX_REQ_NONE)
			ladx_logic_connect_back(logic, se->location, te->location,
						LADX_AND(a, se->requirement, te->requirement), ladx_req_false());
		else if (se->requirement.tag != LADX_REQ_NONE)
			ladx_logic_connect_back(logic, se->location, te->location, se->requirement, ladx_req_false());
		else
			ladx_logic_connect_back(logic, se->location, te->location, te->requirement, ladx_req_false());
		if (se->enter_set)
			ladx_logic_connect_back(logic, se->location, te->location,
						LADX_AND(a, te->exit_set ? te->one_way_exit : te->requirement,
							 se->one_way_enter),
						ladx_req_false());
		if (te->exit_set)
			ladx_logic_connect_back(logic, se->location, te->location,
						LADX_AND(a, se->enter_set ? se->one_way_enter : se->requirement,
							 te->one_way_exit),
						ladx_req_false());
	}
}

static const int instrument_items[8] = {
	LADX_ITEM_INSTRUMENT1, LADX_ITEM_INSTRUMENT2, LADX_ITEM_INSTRUMENT3, LADX_ITEM_INSTRUMENT4,
	LADX_ITEM_INSTRUMENT5, LADX_ITEM_INSTRUMENT6, LADX_ITEM_INSTRUMENT7, LADX_ITEM_INSTRUMENT8,
};

/* main.py's egg requirement, on world.nightmare -> world.egg. */
static void connect_egg(LadxLogic *logic, const LadxSettings *options, const LadxWorldSetup *ws, const LadxWorld *w)
{
	LadxReqArena *a = ladx_logic_arena_mut(logic);
	LadxReq ocarina = ladx_req_item(LADX_ITEM_OCARINA), song1 = ladx_req_item(LADX_ITEM_SONG1);
	LadxReq egg_trigger = LADX_AND(a, ocarina, song1), terms[9];
	char name[16];
	int n, item;

	if (ladx_setting_is(options, LADX_SETTING_LOGIC, "glitched") || ladx_setting_is(options, LADX_SETTING_LOGIC, "hell"))
		egg_trigger = LADX_OR(a, LADX_AND(a, ocarina, song1), ladx_req_item(LADX_ITEM_BOMB));

	if (ws->goal == LADX_GOAL_SEASHELLS)
		ladx_logic_connect(logic, w->nightmare, w->egg, ladx_req_count(a, LADX_ITEM_SEASHELL, 20));
	else if (ws->goal == LADX_GOAL_MAZE)
		ladx_logic_connect(logic, w->nightmare, w->egg, egg_trigger);
	else if (ws->goal == LADX_GOAL_SPECIFIC) {
		terms[0] = egg_trigger;
		for (n = 0; ws->goal_instruments[n]; n++) {
			snprintf(name, sizeof name, "INSTRUMENT%c", ws->goal_instruments[n]);
			item = ladx_item_find(name);
			if (n >= 8 || item == LADX_ITEM_NONE) {
				logic->broken = true;
				return;
			}
			terms[n + 1] = ladx_req_item(item);
		}
		ladx_logic_connect(logic, w->nightmare, w->egg, ladx_req_and(a, n + 1, terms));
	} else if (ws->goal == LADX_GOAL_OPEN)
		ladx_logic_connect(logic, w->nightmare, w->egg, ladx_req_none());
	else if (ws->goal_count == 0)
		ladx_logic_connect(logic, w->nightmare, w->egg, egg_trigger);
	else if (ws->goal_count == 8) {
		terms[0] = egg_trigger;
		for (n = 0; n < 8; n++)
			terms[n + 1] = ladx_req_item(instrument_items[n]);
		ladx_logic_connect(logic, w->nightmare, w->egg, ladx_req_and(a, 9, terms));
	} else
		ladx_logic_connect(logic, w->nightmare, w->egg,
				   LADX_AND(a, egg_trigger, ladx_req_counts(a, 8, instrument_items, ws->goal_count)));
}

static void add_key_location(LadxLogic *logic, int start, const char *format, int n)
{
	char name[24];
	int item;

	snprintf(name, sizeof name, format, n);
	item = ladx_item_find(name);
	if (item == LADX_ITEM_NONE)
		logic->broken = true;
	else
		ladx_logic_add_item_info(logic, start, LADX_SPOT_KEY_LOCATION, -1, item);
}

LadxLogicStatus ladx_logic_build(LadxLogic *out, const LadxSettings *validated, const LadxWorldSetup *ws)
{
	static const char *const indoor[9] = {"d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d0"};
	LadxRequirementsSettings r;
	LadxDungeonContext c;
	LadxWorld w;
	int i, n, count;

	ladx_logic_begin(out);
	if (!supported(validated)) {
		out->broken = true;
		return LADX_LOGIC_UNSUPPORTED;
	}
	if (!world_setup_valid(ws) || (int)ws->goal < 0 || ws->goal >= LADX_GOAL_COUNT) {
		out->broken = true;
		return LADX_LOGIC_BROKEN;
	}
	ladx_requirements_settings(&r, out, validated);
	c.logic = out;
	c.options = validated;
	c.world_setup = ws;
	c.r = &r;
	ladx_overworld(&c, &w);
	for (i = 0; i < 9; i++)
		ladx_world_update_indoor(out, &w, indoor[i], dungeons[(i + 1) % 9](&c));
	connect_entrances(out, &w);
	connect_egg(out, validated, ws, &w);
	/* LADXR's unused inner loop: nine KeyLocations per key, 81 in all. */
	if (ladx_setting_is(validated, LADX_SETTING_DUNGEON_KEYS, "removed"))
		for (n = 0; n < 9; n++)
			for (count = 0; count < 9; count++)
				add_key_location(out, w.start, "KEY%d", n);
	if (ladx_setting_is(validated, LADX_SETTING_NIGHTMARE_KEYS, "removed"))
		for (n = 0; n < 9; n++)
			add_key_location(out, w.start, "NIGHTMARE_KEY%d", n);
	if (ladx_setting_is(validated, LADX_SETTING_DUNGEON_BEAKS, "removed"))
		for (n = 0; n < 9; n++)
			add_key_location(out, w.start, "STONE_BEAK%d", n);
	out->windfish = w.windfish;
	out->egg = w.egg;
	out->nightmare = w.nightmare;
	ladx_logic_finish(out, w.start);
	ladx_logic_configure(out, validated);
	return ladx_logic_broken(out) ? LADX_LOGIC_BROKEN : LADX_LOGIC_OK;
}
