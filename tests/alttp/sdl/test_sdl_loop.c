#include "sdl_loop.h"
#include "support.h"
#include "ui_app.h"
#include "ui_grid.h"
#include "ui_strings.h"

#include <SDL/SDL.h>
#include <string.h>

#define TEST "test_sdl_loop"
#define FRAMES_MAX 64
#define PAGE (UI_GRID_DEFAULT_LINES - 4)
#define WIDTH (UI_GRID_DEFAULT_COLS - 2)

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef struct Fake {
	int run_calls;
	/* Frames presented when the run was called. */
	int frames_at_run;
	bool push_during_run;
} Fake;

static UiApp app;
static UiAppInit init;
static Fake fake;
static UiGrid grid;
static char frames[FRAMES_MAX][UI_GRID_TEXT_MAX];
static int frame_count;
/* present fails every frame after the first fail_from; -1 never. */
static int fail_from;
static char expected[UI_GRID_TEXT_MAX];

static void push_key(Uint8 type, SDLKey key)
{
	SDL_Event e;

	memset(&e, 0, sizeof(e));
	e.type = type;
	e.key.type = type;
	e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
	e.key.keysym.sym = key;
	CHECK(SDL_PushEvent(&e) == 0);
}

static void tap(SDLKey key)
{
	push_key(SDL_KEYDOWN, key);
	push_key(SDL_KEYUP, key);
}

static void push_type(Uint8 type)
{
	SDL_Event e;

	memset(&e, 0, sizeof(e));
	e.type = type;
	if (type == SDL_ACTIVEEVENT) {
		e.active.gain = 1;
		e.active.state = SDL_APPINPUTFOCUS;
	}
	CHECK(SDL_PushEvent(&e) == 0);
}

/* The app's current screen, in text form, into out. */
static void current(char *out)
{
	static UiScreen screen;
	static UiGrid g;

	ui_app_screen(&app, &screen);
	CHECK(ui_grid_init(&g, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&g, &screen));
	CHECK(ui_grid_text(&g, out, UI_GRID_TEXT_MAX) > 0);
}

static bool present(void *ctx, const UiScreen *screen, const UiGrid *g)
{
	static UiGrid from_screen;
	static char text[UI_GRID_TEXT_MAX];

	CHECK(ctx == &frame_count);
	CHECK(frame_count < FRAMES_MAX);
	CHECK(ui_grid_text(g, frames[frame_count], UI_GRID_TEXT_MAX) > 0);
	CHECK(ui_grid_init(&from_screen, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&from_screen, screen));
	CHECK(ui_grid_text(&from_screen, text, sizeof text) > 0 && strcmp(text, frames[frame_count]) == 0);
	frame_count++;
	return fail_from < 0 || frame_count <= fail_from;
}

static void fake_run(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	Fake *f = ctx;

	(void)request;
	f->run_calls++;
	f->frames_at_run = frame_count;
	current(expected);
	CHECK(frame_count > 0 && strcmp(frames[frame_count - 1], expected) == 0);
	CHECK(strstr(frames[frame_count - 1], "Generating Seed") != NULL);
	memset(outcome, 0, sizeof(*outcome));
	outcome->ok = true;
	strcpy(outcome->rom, "Open-Ganon-1CA10A.sfc");
	outcome->status = UI_STR_NONE;
	outcome->status_arg = UI_STR_NONE;
	outcome->advice = UI_STR_NONE;
	if (f->push_during_run) {
		tap(SDLK_DOWN);
		tap(SDLK_SPACE);
		tap(SDLK_RETURN);
		push_type(SDL_QUIT);
	}
}

static void fake_scan(void *ctx, UiHistoryList *out)
{
	(void)ctx;
	(void)out;
}

static bool fake_spoiler(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out)
{
	(void)ctx;
	(void)entry;
	out->len = 0;
	out->cut = false;
	return true;
}

static void fake_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome)
{
	(void)ctx;
	(void)entry;
	outcome->ok = true;
	outcome->list_stale = false;
	outcome->status = UI_STR_NONE;
}

static void drain(void)
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
		;
}

static void start_games(void)
{
	drain();
	memset(&fake, 0, sizeof(fake));
	memset(&init, 0, sizeof(init));
	init.strings = ui_str_english();
	init.page = PAGE;
	init.width = WIDTH;
	ui_generate_init_defaults(&init.generate, ui_str_english(), PAGE);
	init.run = fake_run;
	init.run_ctx = &fake;
	init.scan = fake_scan;
	init.spoiler_fn = fake_spoiler;
	init.delete_fn = fake_delete;
	init.history_ctx = &fake;
	init.game = UI_GAME_ALTTP;
	init.remember = NULL;
	init.remember_ctx = NULL;
	ui_ladx_generate_init_defaults(&init.ladx.generate, ui_str_english(), PAGE);
	init.ladx.run = fake_run;
	init.ladx.run_ctx = &fake;
	init.ladx.scan = fake_scan;
	init.ladx.spoiler_fn = fake_spoiler;
	init.ladx.delete_fn = fake_delete;
	init.ladx.history_ctx = &fake;
	ui_app_init(&app, &init);
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	frame_count = 0;
	fail_from = -1;
}

/* The app on ALttP's Generate tab, opened from the choice screen. */
static void start_alttp(void)
{
	start_games();
	CHECK(ui_app_key(&app, UI_BUTTON_A) == UI_APP_NONE);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
}

typedef struct Script {
	const UiButton *button;
	int count, next;
} Script;

static bool script_next(void *ctx, UiButton *out)
{
	Script *s = ctx;

	if (s->next >= s->count)
		return false;
	*out = s->button[s->next++];
	return true;
}

static SdlLoopResult run(long max_frames)
{
	return sdl_loop_run(&app, &grid, present, &frame_count, max_frames, NULL);
}

static SdlLoopResult run_script(const UiButton *buttons, int count, long max_frames, Script *s)
{
	SdlScript script;

	s->button = buttons;
	s->count = count;
	s->next = 0;
	script.next = script_next;
	script.ctx = s;
	return sdl_loop_run(&app, &grid, present, &frame_count, max_frames, &script);
}

static void test_one_frame(void)
{
	SDL_Event e;

	start_alttp();
	current(expected);
	tap(SDLK_DOWN);
	CHECK(run(1) == SDLLOOP_FRAMES);
	CHECK(frame_count == 1 && strcmp(frames[0], expected) == 0);
	CHECK(SDL_PollEvent(&e) == 1 && e.type == SDL_KEYDOWN);
}

static void test_generating_before_run(void)
{
	start_alttp();
	tap(SDLK_RETURN);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(fake.run_calls == 1);
	current(expected);
	CHECK(fake.frames_at_run >= 2 && frame_count == fake.frames_at_run + 1);
	CHECK(strcmp(frames[frame_count - 1], expected) == 0);
	CHECK(strstr(expected, "Seed Ready") != NULL);
}

static void test_keys_during_run_dropped(void)
{
	char result[UI_GRID_TEXT_MAX];

	start_alttp();
	fake.push_during_run = true;
	tap(SDLK_RETURN);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(fake.run_calls == 1);
	current(result);
	CHECK(strstr(result, "Seed Ready") != NULL);
	CHECK(strcmp(frames[frame_count - 1], result) == 0);
	CHECK(frame_count == fake.frames_at_run + 1);
}

static void test_quit(void)
{
	start_alttp();
	tap(SDLK_ESCAPE);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(ui_app_key(&app, UI_BUTTON_DOWN) == UI_APP_QUIT);

	start_alttp();
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(frame_count == 1);
	CHECK(ui_app_key(&app, UI_BUTTON_DOWN) == UI_APP_NONE);
}

static void test_games_view(void)
{
	start_games();
	tap(SDLK_SPACE);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_ALTTP);
	current(expected);
	CHECK(frame_count == 3 && strcmp(frames[2], expected) == 0);

	start_alttp();
	tap(SDLK_b);
	tap(SDLK_b);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(ui_app_view(&app) == UI_APP_VIEW_GAMES);
	CHECK(frame_count == 3 && strstr(frames[2], "Choose a Game") != NULL);
	CHECK(ui_app_key(&app, UI_BUTTON_DOWN) == UI_APP_QUIT);
}

static void test_tab_and_noise(void)
{
	char first[UI_GRID_TEXT_MAX];
	int i;

	start_alttp();
	current(first);
	for (i = 0; i < 16; i++)
		push_type(SDL_ACTIVEEVENT);
	tap(SDLK_z);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(frame_count == 1 && strcmp(frames[0], first) == 0);

	start_alttp();
	tap(SDLK_t);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	current(expected);
	CHECK(strcmp(frames[frame_count - 1], expected) == 0 && strcmp(expected, first) != 0);
}

static void test_expose(void)
{
	start_alttp();
	push_type(SDL_VIDEOEXPOSE);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(frame_count == 2 && strcmp(frames[0], frames[1]) == 0);
}

static void test_chord(void)
{
	char first[UI_GRID_TEXT_MAX];

	start_alttp();
	current(first);
	push_key(SDL_KEYDOWN, SDLK_ESCAPE);
	push_key(SDL_KEYDOWN, SDLK_DOWN);
	push_key(SDL_KEYUP, SDLK_DOWN);
	push_key(SDL_KEYUP, SDLK_ESCAPE);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	CHECK(strcmp(frames[frame_count - 1], first) == 0);
	CHECK(ui_app_key(&app, UI_BUTTON_DOWN) == UI_APP_NONE);
}

static void test_frame_limit(void)
{
	start_alttp();
	tap(SDLK_DOWN);
	tap(SDLK_DOWN);
	push_type(SDL_QUIT);
	CHECK(run(2) == SDLLOOP_FRAMES);
	CHECK(frame_count == 2);
}

static void test_script_as_pushed(void)
{
	static const UiButton buttons[] = { UI_BUTTON_DOWN, UI_BUTTON_RIGHT, UI_BUTTON_R1, UI_BUTTON_L1 };
	static const SDLKey keys[] = { SDLK_DOWN, SDLK_RIGHT, SDLK_t, SDLK_e };
	static char pushed[FRAMES_MAX][UI_GRID_TEXT_MAX];
	int pushed_count, i;
	Script s;
	SDL_Event e;

	start_alttp();
	for (i = 0; i < 4; i++)
		tap(keys[i]);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_QUIT);
	pushed_count = frame_count;
	memcpy(pushed, frames, sizeof(pushed));
	CHECK(pushed_count == 9);

	start_alttp();
	push_type(SDL_QUIT);
	CHECK(run_script(buttons, 4, 0, &s) == SDLLOOP_SCRIPT_END);
	CHECK(frame_count == pushed_count);
	for (i = 0; i < frame_count; i++)
		CHECK(strcmp(frames[i], pushed[i]) == 0);
	CHECK(SDL_PollEvent(&e) == 1 && e.type == SDL_QUIT);
}

static void test_script_after_run(void)
{
	static const UiButton buttons[] = { UI_BUTTON_START, UI_BUTTON_A, UI_BUTTON_R1 };
	Script s;

	start_alttp();
	fake.push_during_run = true;
	CHECK(run_script(buttons, 3, 0, &s) == SDLLOOP_SCRIPT_END);
	CHECK(fake.run_calls == 1 && s.next == 3);
	CHECK(ui_app_tab(&app) == UI_APP_TAB_HISTORY);
	current(expected);
	CHECK(strcmp(frames[frame_count - 1], expected) == 0);
	CHECK(frame_count == fake.frames_at_run + 1 + 4);
}

static void test_script_frame_cap(void)
{
	static const UiButton buttons[] = { UI_BUTTON_DOWN, UI_BUTTON_START, UI_BUTTON_A };
	Script s;

	start_alttp();
	CHECK(run_script(buttons, 3, 5, &s) == SDLLOOP_FRAMES);
	CHECK(fake.run_calls == 0 && frame_count == 5);
	CHECK(strstr(frames[4], "Generating Seed") != NULL);
}

static void test_script_menu(void)
{
	static const UiButton buttons[] = { UI_BUTTON_DOWN, UI_BUTTON_MENU, UI_BUTTON_DOWN };
	Script s;

	start_alttp();
	CHECK(run_script(buttons, 3, 0, &s) == SDLLOOP_QUIT);
	CHECK(s.next == 2 && frame_count == 4);
	CHECK(ui_app_key(&app, UI_BUTTON_DOWN) == UI_APP_QUIT);
}

static void test_present_failed(void)
{
	static const UiButton buttons[] = { UI_BUTTON_DOWN, UI_BUTTON_DOWN, UI_BUTTON_DOWN };
	Script s;

	start_alttp();
	fail_from = 0;
	tap(SDLK_DOWN);
	CHECK(run(0) == SDLLOOP_PRESENT_FAILED);
	CHECK(frame_count == 1);

	start_alttp();
	fail_from = 2;
	tap(SDLK_DOWN);
	tap(SDLK_DOWN);
	tap(SDLK_DOWN);
	push_type(SDL_QUIT);
	CHECK(run(0) == SDLLOOP_PRESENT_FAILED);
	CHECK(frame_count == 3);

	start_alttp();
	fail_from = 2;
	CHECK(run_script(buttons, 3, 0, &s) == SDLLOOP_PRESENT_FAILED);
	CHECK(frame_count == 3 && s.next == 1);

	start_alttp();
	fail_from = 1;
	tap(SDLK_RETURN);
	CHECK(run(0) == SDLLOOP_PRESENT_FAILED);
	CHECK(fake.run_calls == 0 && frame_count == 2);
}

int main(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		test_fail(TEST, "SDL_Init: %s", SDL_GetError());
	if (SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE) == NULL)
		test_fail(TEST, "SDL_SetVideoMode: %s", SDL_GetError());
	test_one_frame();
	test_generating_before_run();
	test_keys_during_run_dropped();
	test_quit();
	test_games_view();
	test_tab_and_noise();
	test_expose();
	test_chord();
	test_frame_limit();
	test_script_as_pushed();
	test_script_after_run();
	test_script_frame_cap();
	test_script_menu();
	test_present_failed();
	SDL_Quit();
	printf("%s: passed\n", TEST);
	return 0;
}
