#ifndef RANDO_LAST_GAME_H
#define RANDO_LAST_GAME_H

#include "ui_games.h"

#include <stdbool.h>

#define RANDO_LAST_GAME_MAX 256

typedef enum RandoLastGameStatus {
	RANDO_LAST_GAME_LOADED,
	RANDO_LAST_GAME_MISSING,
	RANDO_LAST_GAME_INVALID,
} RandoLastGameStatus;

bool rando_last_game_save(const char *path, UiGame game);

RandoLastGameStatus rando_last_game_load(const char *path, UiGame *game);

#endif
