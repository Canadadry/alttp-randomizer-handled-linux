#include "rando_bosses.h"

#include <assert.h>
#include <string.h>

typedef bool (*CanBeat)(const RandoConfig *config, const RandoItems *items);

typedef struct BossInfo {
	const char *name;
	CanBeat can_beat;
} BossInfo;

static bool has(const RandoItems *items, RandoItemId id)
{
	return rando_items_has(items, id);
}

static bool sword(const RandoItems *items, int min_level)
{
	return rando_items_has_sword(items, min_level);
}

static bool magic(const RandoItems *items, const RandoConfig *config, int bars)
{
	return rando_items_can_extend_magic(items, config, bars);
}

static bool arrows(const RandoItems *items, const RandoConfig *config)
{
	return rando_items_can_shoot_arrows(items, config, 1);
}

static bool not_basic(const RandoConfig *config)
{
	return config->item_placement != RANDO_PLACEMENT_BASIC;
}

static bool swordless(const RandoConfig *config)
{
	return config->mode_weapons == RANDO_WEAPONS_SWORDLESS;
}

static bool armos_knights(const RandoConfig *config, const RandoItems *items)
{
	return sword(items, 1) || has(items, RANDO_ITEM_Hammer) || arrows(items, config)
		|| has(items, RANDO_ITEM_Boomerang) || has(items, RANDO_ITEM_RedBoomerang)
		|| (magic(items, config, 4) && (has(items, RANDO_ITEM_FireRod) || has(items, RANDO_ITEM_IceRod)))
		|| (magic(items, config, 2)
		    && (has(items, RANDO_ITEM_CaneOfByrna) || has(items, RANDO_ITEM_CaneOfSomaria)));
}

static bool lanmolas(const RandoConfig *config, const RandoItems *items)
{
	return sword(items, 1) || has(items, RANDO_ITEM_Hammer)
		|| arrows(items, config) || has(items, RANDO_ITEM_FireRod) || has(items, RANDO_ITEM_IceRod)
		|| has(items, RANDO_ITEM_CaneOfByrna) || has(items, RANDO_ITEM_CaneOfSomaria);
}

static bool moldorm(const RandoConfig *config, const RandoItems *items)
{
	(void)config;
	return sword(items, 1) || has(items, RANDO_ITEM_Hammer);
}

static bool agahnim(const RandoConfig *config, const RandoItems *items)
{
	(void)config;
	return sword(items, 1) || has(items, RANDO_ITEM_Hammer) || has(items, RANDO_ITEM_BugCatchingNet);
}

static bool helmasaur_king(const RandoConfig *config, const RandoItems *items)
{
	return (rando_items_can_bomb_things(items) || has(items, RANDO_ITEM_Hammer))
		&& (sword(items, 2) || arrows(items, config)
		    || (not_basic(config) && sword(items, 1)));
}

static bool arrghus(const RandoConfig *config, const RandoItems *items)
{
	return (not_basic(config) || swordless(config) || sword(items, 2))
		&& has(items, RANDO_ITEM_Hookshot)
		&& (has(items, RANDO_ITEM_Hammer) || sword(items, 1)
		    || ((magic(items, config, 2) || arrows(items, config))
			&& (has(items, RANDO_ITEM_FireRod) || has(items, RANDO_ITEM_IceRod))));
}

static bool mothula(const RandoConfig *config, const RandoItems *items)
{
	return (not_basic(config) || sword(items, 2) || (magic(items, config, 2) && has(items, RANDO_ITEM_FireRod)))
		&& (sword(items, 1) || has(items, RANDO_ITEM_Hammer)
		    || (magic(items, config, 2)
			&& (has(items, RANDO_ITEM_FireRod) || has(items, RANDO_ITEM_CaneOfSomaria)
			    || has(items, RANDO_ITEM_CaneOfByrna)))
		    || rando_items_can_get_good_bee(items));
}

static bool blind(const RandoConfig *config, const RandoItems *items)
{
	return (not_basic(config) || swordless(config)
		|| (sword(items, 1) && (has(items, RANDO_ITEM_Cape) || has(items, RANDO_ITEM_CaneOfByrna))))
		&& (sword(items, 1) || has(items, RANDO_ITEM_Hammer)
		    || has(items, RANDO_ITEM_CaneOfSomaria) || has(items, RANDO_ITEM_CaneOfByrna));
}

static bool kholdstare(const RandoConfig *config, const RandoItems *items)
{
	return (not_basic(config) || sword(items, 2) || (magic(items, config, 3) && has(items, RANDO_ITEM_FireRod))
		|| (has(items, RANDO_ITEM_Bombos) && (swordless(config) || sword(items, 1))
		    && magic(items, config, 2) && has(items, RANDO_ITEM_FireRod)))
		&& rando_items_can_melt_things(items, config)
		&& (has(items, RANDO_ITEM_Hammer) || sword(items, 1)
		    || (magic(items, config, 3) && has(items, RANDO_ITEM_FireRod))
		    || (magic(items, config, 2) && has(items, RANDO_ITEM_FireRod) && has(items, RANDO_ITEM_Bombos)
			&& swordless(config)));
}

static bool vitreous(const RandoConfig *config, const RandoItems *items)
{
	return (not_basic(config) || sword(items, 2) || arrows(items, config))
		&& (has(items, RANDO_ITEM_Hammer) || sword(items, 1) || arrows(items, config));
}

static bool trinexx(const RandoConfig *config, const RandoItems *items)
{
	return has(items, RANDO_ITEM_FireRod) && has(items, RANDO_ITEM_IceRod)
		&& (not_basic(config) || swordless(config) || sword(items, 3)
		    || (magic(items, config, 2) && sword(items, 2)))
		&& (sword(items, 3) || has(items, RANDO_ITEM_Hammer)
		    || (magic(items, config, 2) && sword(items, 2))
		    || (magic(items, config, 4) && sword(items, 1)));
}

static bool agahnim2(const RandoConfig *config, const RandoItems *items)
{
	(void)config;
	return sword(items, 1) || has(items, RANDO_ITEM_Hammer) || has(items, RANDO_ITEM_BugCatchingNet);
}

static const BossInfo boss_table[RANDO_BOSS_COUNT] = {
	{"Armos Knights", armos_knights},
	{"Lanmolas", lanmolas},
	{"Moldorm", moldorm},
	{"Agahnim", agahnim},
	{"Helmasaur King", helmasaur_king},
	{"Arrghus", arrghus},
	{"Mothula", mothula},
	{"Blind", blind},
	{"Kholdstare", kholdstare},
	{"Vitreous", vitreous},
	{"Trinexx", trinexx},
	{"Agahnim2", agahnim2},
};

const char *rando_boss_name(RandoBossId id)
{
	assert(id < RANDO_BOSS_COUNT && "rando_boss_name: not a boss id");
	return boss_table[id].name;
}

RandoBossId rando_boss_find(const char *name)
{
	int i;

	for (i = 0; i < RANDO_BOSS_COUNT; i++)
		if (strcmp(boss_table[i].name, name) == 0)
			return (RandoBossId)i;
	return RANDO_BOSS_NONE;
}

bool rando_boss_can_beat(RandoBossId id, const RandoConfig *config, const RandoItems *items)
{
	if (id == RANDO_BOSS_RULELESS)
		return true;
	assert(id < RANDO_BOSS_COUNT && "rando_boss_can_beat: not a boss id");
	return boss_table[id].can_beat(config, items);
}
