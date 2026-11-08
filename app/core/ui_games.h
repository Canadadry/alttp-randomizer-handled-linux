#ifndef UI_GAMES_H
#define UI_GAMES_H

#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

typedef enum UiGame {
	UI_GAME_ALTTP,
	UI_GAME_LADX,
	UI_GAME_COUNT
} UiGame;

typedef enum UiGamesEvent {
	UI_GAMES_NONE,
	UI_GAMES_OPEN,
	UI_GAMES_QUIT
} UiGamesEvent;

typedef struct UiGames {
	const UiStrTable *strings;
	UiGame cursor;
} UiGames;

void ui_games_init(UiGames *state, const UiStrTable *strings, UiGame initial);

UiGamesEvent ui_games_key(UiGames *state, UiButton button);

UiGame ui_games_selected(const UiGames *state);

void ui_games_screen(const UiGames *state, UiScreen *out);

#endif
