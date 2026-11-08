#include "support.h"
#include "ui_games.h"
#include "ui_grid.h"
#include "ui_strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_ui_games"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define SNAPSHOTS "unit/snapshots/"

static UiGames games;
static UiScreen screen;
static char text[UI_GRID_TEXT_MAX];

static void start(UiGame game)
{
	ui_games_init(&games, ui_str_english(), game);
}

/* The text form of the current screen at 40 x 15. */
static const char *render(void)
{
	static UiGrid grid;

	ui_games_screen(&games, &screen);
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, &screen));
	CHECK(ui_grid_text(&grid, text, sizeof text) > 0);
	return text;
}

static UiGamesEvent key(UiButton b)
{
	return ui_games_key(&games, b);
}

static void test_initial_cursor(void)
{
	start(UI_GAME_ALTTP);
	CHECK(ui_games_selected(&games) == UI_GAME_ALTTP);
	ui_games_screen(&games, &screen);
	CHECK(screen.cursor == 0);
	test_snapshot(TEST, SNAPSHOTS "games_first_launch.txt", render());
	start(UI_GAME_LADX);
	CHECK(ui_games_selected(&games) == UI_GAME_LADX);
	ui_games_screen(&games, &screen);
	CHECK(screen.cursor == 1);
	test_snapshot(TEST, SNAPSHOTS "games_ladx_remembered.txt", render());
}

static void test_cursor_wraps(void)
{
	start(UI_GAME_ALTTP);
	CHECK(key(UI_BUTTON_DOWN) == UI_GAMES_NONE);
	CHECK(ui_games_selected(&games) == UI_GAME_LADX);
	CHECK(key(UI_BUTTON_DOWN) == UI_GAMES_NONE);
	CHECK(ui_games_selected(&games) == UI_GAME_ALTTP);
	CHECK(key(UI_BUTTON_UP) == UI_GAMES_NONE);
	CHECK(ui_games_selected(&games) == UI_GAME_LADX);
	test_snapshot(TEST, SNAPSHOTS "games_ladx_remembered.txt", render());
	CHECK(key(UI_BUTTON_UP) == UI_GAMES_NONE);
	CHECK(ui_games_selected(&games) == UI_GAME_ALTTP);
	test_snapshot(TEST, SNAPSHOTS "games_first_launch.txt", render());
}

static void test_events(void)
{
	static const UiButton ignored[] = {UI_BUTTON_LEFT, UI_BUTTON_RIGHT, UI_BUTTON_X, UI_BUTTON_Y,
					   UI_BUTTON_L1, UI_BUTTON_R1, UI_BUTTON_L2, UI_BUTTON_R2,
					   UI_BUTTON_SELECT, UI_BUTTON_START};
	static char before[UI_GRID_TEXT_MAX];
	size_t i;
	int game;

	for (game = 0; game < UI_GAME_COUNT; game++) {
		start((UiGame)game);
		strcpy(before, render());
		for (i = 0; i < sizeof ignored / sizeof ignored[0]; i++) {
			CHECK(key(ignored[i]) == UI_GAMES_NONE);
			CHECK(strcmp(before, render()) == 0);
		}
		CHECK(key(UI_BUTTON_A) == UI_GAMES_OPEN);
		CHECK(ui_games_selected(&games) == (UiGame)game);
		CHECK(key(UI_BUTTON_B) == UI_GAMES_QUIT);
		CHECK(key(UI_BUTTON_MENU) == UI_GAMES_QUIT);
		CHECK(strcmp(before, render()) == 0);
	}
}

static void test_screen(void)
{
	start(UI_GAME_ALTTP);
	ui_games_screen(&games, &screen);
	CHECK(ui_screen_valid(&screen));
	CHECK(screen.tab_count == 0);
	CHECK(strcmp(screen.title, "Choose a Game") == 0);
	CHECK(screen.body_kind == UI_BODY_LIST);
	CHECK(screen.message_kind == UI_MESSAGE_NONE);
	CHECK(screen.row_count == 2);
	CHECK(screen.rows[0].kind == UI_ROW_ACTION && strcmp(screen.rows[0].label, "A Link to the Past") == 0);
	CHECK(screen.rows[1].kind == UI_ROW_ACTION && strcmp(screen.rows[1].label, "Link's Awakening DX") == 0);
	CHECK(!screen.rows[0].disabled && !screen.rows[1].disabled);
	CHECK(screen.hint_count == 2);
	CHECK(screen.hints[0].button == UI_HINT_A && strcmp(screen.hints[0].label, "Open") == 0 &&
	      screen.hints[0].primary);
	CHECK(screen.hints[1].button == UI_HINT_B && strcmp(screen.hints[1].label, "Quit") == 0 &&
	      !screen.hints[1].primary);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_initial_cursor();
	test_cursor_wraps();
	test_events();
	test_screen();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
