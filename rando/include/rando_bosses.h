#ifndef RANDO_BOSSES_H
#define RANDO_BOSSES_H

#include "rando_config.h"
#include "rando_items.h"

#include <stdbool.h>

typedef enum RandoBossId {
	RANDO_BOSS_ARMOS_KNIGHTS,
	RANDO_BOSS_LANMOLAS,
	RANDO_BOSS_MOLDORM,
	RANDO_BOSS_AGAHNIM,
	RANDO_BOSS_HELMASAUR_KING,
	RANDO_BOSS_ARRGHUS,
	RANDO_BOSS_MOTHULA,
	RANDO_BOSS_BLIND,
	RANDO_BOSS_KHOLDSTARE,
	RANDO_BOSS_VITREOUS,
	RANDO_BOSS_TRINEXX,
	RANDO_BOSS_AGAHNIM2,
	RANDO_BOSS_COUNT,
	RANDO_BOSS_NONE = RANDO_BOSS_COUNT,
	RANDO_BOSS_RULELESS,
} RandoBossId;

const char *rando_boss_name(RandoBossId id);
RandoBossId rando_boss_find(const char *name);
bool rando_boss_can_beat(RandoBossId id, const RandoConfig *config, const RandoItems *items);

#endif
