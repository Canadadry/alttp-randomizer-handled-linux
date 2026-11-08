#include "ui_games.h"

#include <assert.h>

static const UiStrId names[UI_GAME_COUNT] = {
	[UI_GAME_ALTTP] = UI_STR_GAME_ALTTP,
	[UI_GAME_LADX] = UI_STR_GAME_LADX,
};

void ui_games_init(UiGames *state, const UiStrTable *strings, UiGame initial)
{
	assert((int)initial >= 0 && initial < UI_GAME_COUNT);
	state->strings = strings;
	state->cursor = initial;
}

static UiGame step(UiGame game, int delta)
{
	return (UiGame)(((int)game + delta + UI_GAME_COUNT) % UI_GAME_COUNT);
}

UiGamesEvent ui_games_key(UiGames *state, UiButton button)
{
	switch (button) {
	case UI_BUTTON_UP: state->cursor = step(state->cursor, -1); break;
	case UI_BUTTON_DOWN: state->cursor = step(state->cursor, 1); break;
	case UI_BUTTON_A: return UI_GAMES_OPEN;
	case UI_BUTTON_B:
	case UI_BUTTON_MENU: return UI_GAMES_QUIT;
	case UI_BUTTON_LEFT:
	case UI_BUTTON_RIGHT:
	case UI_BUTTON_X:
	case UI_BUTTON_Y:
	case UI_BUTTON_L1:
	case UI_BUTTON_R1:
	case UI_BUTTON_L2:
	case UI_BUTTON_R2:
	case UI_BUTTON_SELECT:
	case UI_BUTTON_START:
	case UI_BUTTON_COUNT: break;
	}
	return UI_GAMES_NONE;
}

UiGame ui_games_selected(const UiGames *state)
{
	return state->cursor;
}

void ui_games_screen(const UiGames *state, UiScreen *out)
{
	const UiStrTable *t = state->strings;
	int game;

	ui_screen_init(out);
	out->title = ui_str(t, UI_STR_GAMES_TITLE);
	for (game = 0; game < UI_GAME_COUNT; game++) {
		UiRow row = {UI_ROW_ACTION, NULL, NULL, false, false};

		row.label = ui_str(t, names[game]);
		ui_screen_add_row(out, row);
	}
	out->cursor = (int)state->cursor;
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_A, UI_STR_HINT_OPEN, true));
	ui_screen_add_hint(out, ui_str_hint(t, UI_HINT_B, UI_STR_HINT_QUIT, false));
}
