#include "rando_settings.h"
#include "support.h"
#include "ui_grid.h"
#include "ui_history.h"
#include "ui_strings.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_ui_history"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define SNAPSHOTS "unit/snapshots/"
#define PAGE (UI_GRID_DEFAULT_LINES - 4)
#define WIDTH (UI_GRID_DEFAULT_COLS - 2)

/* The fake SD side: counts calls, records the entries handed, replies as scripted. */
typedef struct Fake {
	int spoiler_calls, delete_calls;
	char spoiler_rom[RANDO_NAME_ROM_MAX], deleted_rom[RANDO_NAME_ROM_MAX];
	const char *outline;
	bool spoiler_fails, spoiler_cut;
	UiHistoryDeleteOutcome reply;
} Fake;

static UiHistory history;
static UiHistoryList list;
static Fake fake;
static char text[UI_GRID_TEXT_MAX];

static const char outline[] = "Bosses\n"
			      "  Ganon: Ganon\n"
			      "Light World\n"
			      "  Mini Moldorm Cave - Far Right: ProgressiveSword\n"
			      "playthrough\n"
			      "  1\n"
			      "    Light World\n"
			      "      Blind's Hideout - Far Left: Flippers\n";

static bool fake_spoiler(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out)
{
	Fake *f = ctx;

	f->spoiler_calls++;
	snprintf(f->spoiler_rom, sizeof f->spoiler_rom, "%s", entry->name.rom);
	if (f->spoiler_fails)
		return false;
	out->len = strlen(f->outline);
	memcpy(out->text, f->outline, out->len);
	out->cut = f->spoiler_cut;
	return true;
}

static void fake_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome)
{
	Fake *f = ctx;

	f->delete_calls++;
	snprintf(f->deleted_rom, sizeof f->deleted_rom, "%s", entry->name.rom);
	*outcome = f->reply;
}

static UiHistoryEntry entry_of(const char *code, const char *rom, int year, int month, int day, int hour, int minute,
			       uint32_t seed, bool has_spoiler)
{
	UiHistoryEntry e;
	int id;

	memset(&e, 0, sizeof e);
	snprintf(e.name.code, sizeof e.name.code, "%s", code);
	snprintf(e.name.rom, sizeof e.name.rom, "%s", rom);
	e.date.year = year;
	e.date.month = month;
	e.date.day = day;
	e.date.hour = hour;
	e.date.minute = minute;
	e.date.second = 59;
	e.seed = seed;
	rando_options_init(&e.options);
	e.heart_beep = RANDO_HEART_BEEP_HALF;
	for (id = 0; id < RANDO_SETTING_COUNT; id++)
		rando_setting_set(&e.options, &e.heart_beep, (RandoSettingId)id, rando_setting_default((RandoSettingId)id));
	e.has_spoiler = has_spoiler;
	return e;
}

/* Newest first: a spoiler seed, an Inverted All Dungeons seed without one, an older spoiler seed. */
static void three_seeds(void)
{
	memset(&list, 0, sizeof list);
	list.entries[0] = entry_of("1CA10A-2", "Open-Ganon-1CA10A-2.sfc", 2026, 11, 1, 14, 5, 4294967295u, true);
	list.entries[1] = entry_of("2B3C4D", "Inverted-Dungeons-2B3C4D.sfc", 2026, 10, 30, 9, 41, 7, false);
	rando_setting_set(&list.entries[1].options, &list.entries[1].heart_beep, RANDO_SETTING_STATE,
			  rando_setting_value_find(RANDO_SETTING_STATE, "inverted"));
	rando_setting_set(&list.entries[1].options, &list.entries[1].heart_beep, RANDO_SETTING_GOAL,
			  rando_setting_value_find(RANDO_SETTING_GOAL, "dungeons"));
	list.entries[2] = entry_of("1CA10A", "Open-Ganon-1CA10A.sfc", 2026, 10, 28, 23, 59, 1, true);
	list.count = 3;
}

/* count seeds Open-Ganon-1000NN.sfc, minute 59 - NN % 60. */
static void many_seeds(int count)
{
	int i;

	memset(&list, 0, sizeof list);
	for (i = 0; i < count; i++) {
		char code[16], rom[48];

		snprintf(code, sizeof code, "1000%02d", i);
		snprintf(rom, sizeof rom, "Open-Ganon-%s.sfc", code);
		list.entries[i] = entry_of(code, rom, 2026, 11, 1, 12, 59 - i % 60, (uint32_t)i, false);
	}
	list.count = count;
}

static void start_game(UiGame game)
{
	UiHistoryInit init;

	memset(&init, 0, sizeof init);
	init.game = game;
	memset(&fake, 0, sizeof fake);
	fake.outline = outline;
	fake.reply.ok = true;
	fake.reply.status = UI_STR_NONE;
	init.strings = ui_str_english();
	init.page = PAGE;
	init.width = WIDTH;
	init.list = &list;
	init.spoiler_fn = fake_spoiler;
	init.delete_fn = fake_delete;
	init.ctx = &fake;
	ui_history_init(&history, &init);
}

static void start(void)
{
	start_game(UI_GAME_ALTTP);
}

/* The text form of the tab's screen at 40 x 15; a History screen never carries help. */
static const char *render(void)
{
	static UiScreen screen;
	static UiGrid grid;

	ui_history_screen(&history, &screen);
	CHECK(screen.message_kind != UI_MESSAGE_HELP);
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(ui_grid_render(&grid, &screen));
	CHECK(ui_grid_text(&grid, text, sizeof text) > 0);
	return text;
}

static void snapshot(const char *name)
{
	char path[256];

	snprintf(path, sizeof path, SNAPSHOTS "%s", name);
	test_snapshot(TEST, path, render());
}

static UiHistoryEvent key(UiButton button)
{
	return ui_history_key(&history, button);
}

static void keys(UiButton button, int n)
{
	while (n-- > 0)
		CHECK(key(button) == UI_HISTORY_NONE);
}

/* The rendered screen, kept for comparison. */
static char before[UI_GRID_TEXT_MAX];

static void remember(void)
{
	snprintf(before, sizeof before, "%s", render());
}

static bool unchanged(void)
{
	return strcmp(before, render()) == 0;
}

/* The current screen's hints, as "A Details|X Delete", buttons as a number before each. */
static const char *hints(void)
{
	static char out[128];
	UiScreen screen;
	size_t len = 0;
	int i;

	ui_history_screen(&history, &screen);
	out[0] = '\0';
	for (i = 0; i < screen.hint_count; i++) {
		const UiHint *h = &screen.hints[i];

		len += (size_t)snprintf(out + len, sizeof out - len, "%s%d:%s %s%s", i > 0 ? "|" : "", (int)h->button,
					h->key, h->label, h->primary ? "*" : "");
	}
	return out;
}

static void check_hints(const char *expected)
{
	const char *got = hints();

	if (strcmp(got, expected) != 0)
		test_fail(TEST, "hints \"%s\", expected \"%s\"", got, expected);
}

static void test_hints(void)
{
	char list_hints[64], detail_hints[64], confirm_hints[64];

	snprintf(list_hints, sizeof list_hints, "%d:A Details|%d:X Delete", UI_HINT_A, UI_HINT_X);
	snprintf(detail_hints, sizeof detail_hints, "%d:B Back|%d:X Delete", UI_HINT_B, UI_HINT_X);
	snprintf(confirm_hints, sizeof confirm_hints, "%d:A Delete|%d:B Keep", UI_HINT_A, UI_HINT_B);
	memset(&list, 0, sizeof list);
	start();
	check_hints("");
	three_seeds();
	start();
	check_hints(list_hints);
	keys(UI_BUTTON_A, 1);
	check_hints(detail_hints);
	keys(UI_BUTTON_X, 1);
	check_hints(confirm_hints);
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_X, 1);
	check_hints(confirm_hints);
}

/* A screen's style fields, as "alert notice 0": tone, body, then the emphasised rows' indexes. */
static const char *style_of(const UiScreen *screen)
{
	static char out[128];
	size_t len;
	int i;

	len = (size_t)snprintf(out, sizeof out, "%s %s", screen->title_tone == UI_TITLE_ALERT ? "alert" : "normal",
			       screen->body_kind == UI_BODY_NOTICE ? "notice" : "list");
	for (i = 0; i < screen->row_count; i++)
		if (screen->rows[i].emphasis)
			len += (size_t)snprintf(out + len, sizeof out - len, " %d", i);
	return out;
}

static void check_style(const UiScreen *screen, const char *expected)
{
	const char *got = style_of(screen);

	if (strcmp(got, expected) != 0)
		test_fail(TEST, "style \"%s\", expected \"%s\"", got, expected);
}

static void check_history_style(const char *expected)
{
	UiScreen screen;

	ui_history_screen(&history, &screen);
	check_style(&screen, expected);
}

static void test_styles(void)
{
	memset(&list, 0, sizeof list);
	start();
	check_history_style("normal notice");
	three_seeds();
	start();
	check_history_style("normal list");
	keys(UI_BUTTON_A, 1);
	check_history_style("normal list");
	keys(UI_BUTTON_X, 1);
	check_history_style("alert notice 0");
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_B, 1);
	keys(UI_BUTTON_X, 1);
	check_history_style("alert notice 0");
}

static void test_list(void)
{
	three_seeds();
	start();
	snapshot("history_list.txt");
	list.unreadable = true;
	start();
	snapshot("history_unreadable.txt");
	keys(UI_BUTTON_LEFT, 1);
	snapshot("history_list.txt");
}

static void test_list_moves(void)
{
	many_seeds(30);
	start();
	keys(UI_BUTTON_R2, 1);
	snapshot("history_list_scrolled.txt");
	keys(UI_BUTTON_L2, 1);
	CHECK(strstr(render(), ">| Open-Ganon-100000.sfc ") != NULL);
	keys(UI_BUTTON_UP, 1);
	CHECK(strstr(render(), ">| Open-Ganon-100029.sfc                 v|") == NULL);
	CHECK(strstr(render(), ">| Open-Ganon-100029.sfc                  |") != NULL);
	CHECK(strstr(render(), " | Open-Ganon-100019.sfc                 ^|") != NULL);
	keys(UI_BUTTON_DOWN, 1);
	CHECK(strstr(render(), ">| Open-Ganon-100000.sfc                  |") != NULL);
	keys(UI_BUTTON_R2, 5);
	CHECK(strstr(render(), ">| Open-Ganon-100029.sfc ") != NULL);
	keys(UI_BUTTON_UP, 1);
	CHECK(strstr(render(), ">| Open-Ganon-100028.sfc ") != NULL);
}

static void test_empty(void)
{
	static const UiButton ignored[] = {UI_BUTTON_UP, UI_BUTTON_DOWN, UI_BUTTON_L2, UI_BUTTON_R2, UI_BUTTON_A,
					   UI_BUTTON_X, UI_BUTTON_Y, UI_BUTTON_START, UI_BUTTON_SELECT};
	size_t i;

	memset(&list, 0, sizeof list);
	start();
	snapshot("history_empty.txt");
	for (i = 0; i < sizeof ignored / sizeof ignored[0]; i++)
		CHECK(key(ignored[i]) == UI_HISTORY_NONE);
	snapshot("history_empty.txt");
	CHECK(fake.spoiler_calls == 0 && fake.delete_calls == 0);
	CHECK(key(UI_BUTTON_B) == UI_HISTORY_BACK);
	CHECK(key(UI_BUTTON_L1) == UI_HISTORY_TAB_PREV);
}

static void test_notices(void)
{
	memset(&list, 0, sizeof list);
	list.truncated = true;
	list.unreadable = true;
	list.folder_unreadable = true;
	start();
	CHECK(strstr(render(), "i| History folder could not be read ") != NULL);
	CHECK(strstr(render(), " | No seeds yet ") != NULL);
	keys(UI_BUTTON_Y, 1);
	CHECK(strstr(render(), "History folder") == NULL);
	list.folder_unreadable = false;
	start();
	CHECK(strstr(render(), "i| Some History records are unreadable ") != NULL);
	list.unreadable = false;
	many_seeds(UI_HISTORY_MAX);
	list.truncated = true;
	start();
	CHECK(strstr(render(), "i| Showing the newest 256 seeds ") != NULL);
	keys(UI_BUTTON_DOWN, 1);
	CHECK(strstr(render(), "i| Generated 2026-11-01 12:58 ") != NULL);
}

static void test_list_keys(void)
{
	static const UiButton ignored[] = {UI_BUTTON_LEFT, UI_BUTTON_RIGHT, UI_BUTTON_Y, UI_BUTTON_START,
					   UI_BUTTON_SELECT};
	size_t i;

	three_seeds();
	start();
	remember();
	for (i = 0; i < sizeof ignored / sizeof ignored[0]; i++) {
		CHECK(key(ignored[i]) == UI_HISTORY_NONE);
		CHECK(unchanged());
	}
	CHECK(key(UI_BUTTON_B) == UI_HISTORY_BACK);
	CHECK(key(UI_BUTTON_MENU) == UI_HISTORY_QUIT);
	CHECK(key(UI_BUTTON_L1) == UI_HISTORY_TAB_PREV);
	CHECK(key(UI_BUTTON_R1) == UI_HISTORY_TAB_NEXT);
	CHECK(unchanged());
	CHECK(fake.spoiler_calls == 0 && fake.delete_calls == 0);
}

static void test_detail(void)
{
	three_seeds();
	start();
	keys(UI_BUTTON_A, 1);
	CHECK(fake.spoiler_calls == 1);
	CHECK(strcmp(fake.spoiler_rom, "Open-Ganon-1CA10A-2.sfc") == 0);
	snapshot("history_detail_top.txt");
	keys(UI_BUTTON_UP, 1);
	snapshot("history_detail_top.txt");
	keys(UI_BUTTON_R2, 2);
	snapshot("history_detail_spoiler.txt");
	keys(UI_BUTTON_DOWN, 1);
	snapshot("history_detail_spoiler.txt");
	keys(UI_BUTTON_UP, 17);
	snapshot("history_detail_top.txt");
	keys(UI_BUTTON_DOWN, 17);
	snapshot("history_detail_spoiler.txt");
	keys(UI_BUTTON_L2, 2);
	snapshot("history_detail_top.txt");
	CHECK(fake.spoiler_calls == 1);
	keys(UI_BUTTON_B, 1);
	snapshot("history_list.txt");
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(fake.spoiler_calls == 1);
	keys(UI_BUTTON_R2, 1);
	snapshot("history_detail_no_spoiler.txt");
	keys(UI_BUTTON_B, 1);
	CHECK(strstr(render(), ">| Inverted-Dungeons-2B3C4D.sfc ") != NULL);
	fake.spoiler_fails = true;
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(fake.spoiler_calls == 2);
	keys(UI_BUTTON_R2, 2);
	snapshot("history_detail_unreadable.txt");
}

static void test_detail_keys(void)
{
	static const UiButton ignored[] = {UI_BUTTON_LEFT, UI_BUTTON_RIGHT, UI_BUTTON_A, UI_BUTTON_Y, UI_BUTTON_START,
					   UI_BUTTON_SELECT};
	size_t i;

	three_seeds();
	start();
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_DOWN, 3);
	remember();
	for (i = 0; i < sizeof ignored / sizeof ignored[0]; i++) {
		CHECK(key(ignored[i]) == UI_HISTORY_NONE);
		CHECK(unchanged());
	}
	CHECK(key(UI_BUTTON_L1) == UI_HISTORY_TAB_PREV);
	CHECK(key(UI_BUTTON_R1) == UI_HISTORY_TAB_NEXT);
	CHECK(unchanged());
	CHECK(key(UI_BUTTON_MENU) == UI_HISTORY_QUIT);
	CHECK(unchanged());
	CHECK(fake.spoiler_calls == 1 && fake.delete_calls == 0);
}

static void test_detail_cut(void)
{
	three_seeds();
	start();
	fake.spoiler_cut = true;
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_R2, 3);
	CHECK(strstr(render(), " |         Flippers                       |\n"
			       " | Spoiler cut: too long                  |\n") != NULL);
}

static void test_confirm(void)
{
	static const UiButton ignored[] = {UI_BUTTON_UP, UI_BUTTON_DOWN, UI_BUTTON_LEFT, UI_BUTTON_RIGHT,
					   UI_BUTTON_X, UI_BUTTON_Y, UI_BUTTON_L1, UI_BUTTON_R1,
					   UI_BUTTON_L2, UI_BUTTON_R2, UI_BUTTON_START, UI_BUTTON_SELECT};
	size_t i;

	three_seeds();
	start();
	keys(UI_BUTTON_X, 1);
	snapshot("history_confirm.txt");
	for (i = 0; i < sizeof ignored / sizeof ignored[0]; i++) {
		CHECK(key(ignored[i]) == UI_HISTORY_NONE);
		snapshot("history_confirm.txt");
	}
	CHECK(key(UI_BUTTON_MENU) == UI_HISTORY_QUIT);
	keys(UI_BUTTON_B, 1);
	snapshot("history_list.txt");
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_X, 1);
	snapshot("history_confirm.txt");
	keys(UI_BUTTON_B, 1);
	snapshot("history_detail_top.txt");
	CHECK(fake.delete_calls == 0);
}

static void test_delete(void)
{
	three_seeds();
	start();
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_X, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(fake.delete_calls == 1);
	CHECK(strcmp(fake.deleted_rom, "Inverted-Dungeons-2B3C4D.sfc") == 0);
	snapshot("history_deleted.txt");
	keys(UI_BUTTON_Y, 1);
	CHECK(strstr(render(), "i| Generated 2026-10-28 23:59 ") != NULL);

	three_seeds();
	start();
	fake.reply.list_stale = true;
	keys(UI_BUTTON_DOWN, 1);
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_X, 1);
	keys(UI_BUTTON_A, 1);
	snapshot("history_deleted_stale.txt");

	three_seeds();
	start();
	keys(UI_BUTTON_UP, 1);
	keys(UI_BUTTON_X, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(strcmp(fake.deleted_rom, "Open-Ganon-1CA10A.sfc") == 0);
	CHECK(strstr(render(), ">| Inverted-Dungeons-2B3C4D.sfc ") != NULL);
	keys(UI_BUTTON_X, 1);
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_X, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(fake.delete_calls == 3);
	CHECK(strstr(render(), " | No seeds yet ") != NULL);
	CHECK(strstr(render(), "i| Seed deleted ") != NULL);
}

static void test_failed_delete(void)
{
	three_seeds();
	start();
	fake.reply.ok = false;
	fake.reply.status = UI_STR_HISTORY_DELETE_FILES;
	keys(UI_BUTTON_X, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(fake.delete_calls == 1);
	snapshot("history_delete_failed.txt");
	keys(UI_BUTTON_LEFT, 1);
	snapshot("history_list.txt");
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_X, 1);
	keys(UI_BUTTON_A, 1);
	CHECK(fake.delete_calls == 2);
	CHECK(strstr(render(), "#| Open-Ganon-1CA10A-2.sfc ") != NULL);
	CHECK(strstr(render(), "E| Some save files were not deleted ") != NULL);
}

/* Every delete status renders whole as the error line. */
static void test_failure_texts(void)
{
	static const UiStrId statuses[] = {UI_STR_HISTORY_DELETE_ROM, UI_STR_HISTORY_DELETE_FILES,
					   UI_STR_HISTORY_DELETE_RECORD};
	size_t i;

	for (i = 0; i < sizeof statuses / sizeof statuses[0]; i++) {
		char line[64];

		three_seeds();
		start();
		fake.reply.ok = false;
		fake.reply.status = statuses[i];
		keys(UI_BUTTON_X, 1);
		keys(UI_BUTTON_A, 1);
		snprintf(line, sizeof line, "E| %s ", ui_str(NULL, statuses[i]));
		CHECK(strstr(render(), line) != NULL);
		CHECK(strstr(render(), "Open-Ganon-1CA10A-2.sfc") != NULL);
	}
}

/* The detail rows of a one-line spoiler at width, as "|<row>" lines from the spoiler title on. */
static const char *wrapped(const char *line, int width)
{
	static char spoiler[512];
	static UiScreen screen;
	static char out[2048];
	UiHistoryInit init;
	int i;
	size_t n = 0;

	memset(&list, 0, sizeof list);
	list.entries[0] = entry_of("1CA10A", "Open-Ganon-1CA10A.sfc", 2026, 11, 1, 14, 5, 1, true);
	list.count = 1;
	memset(&fake, 0, sizeof fake);
	snprintf(spoiler, sizeof spoiler, "%s\n", line);
	fake.outline = spoiler;
	init.strings = NULL;
	init.page = UI_ROWS_MAX;
	init.width = width;
	init.list = &list;
	init.spoiler_fn = fake_spoiler;
	init.delete_fn = fake_delete;
	init.ctx = &fake;
	ui_history_init(&history, &init);
	keys(UI_BUTTON_A, 1);
	ui_history_screen(&history, &screen);
	for (i = RANDO_SETTING_COUNT + 5; i < screen.row_count; i++)
		n += (size_t)snprintf(out + n, sizeof out - n, "|%s", screen.rows[i].label);
	return out;
}

static void wraps_to(const char *line, int width, const char *expected)
{
	const char *got = wrapped(line, width);

	if (strcmp(got, expected) != 0)
		test_fail(TEST, "wrap of \"%s\" at %d: \"%s\", expected \"%s\"", line, width, got, expected);
}

static void test_wrapping(void)
{
	wraps_to("12345678901234567890", 20, "|12345678901234567890");
	wraps_to("123456789012345678901", 20, "|12345678901234567890|  1");
	wraps_to("abc defghij klmnopqrstu", 20, "|abc defghij|  klmnopqrstu");
	wraps_to("abcdefghij klmnopqrs tuv", 20, "|abcdefghij klmnopqrs|  tuv");
	wraps_to("  abcdefghijklmnopqrstuvwxyz", 20, "|  abcdefghijklmnopqr|    stuvwxyz");
	wraps_to("          abcdefgh ijklmnopqrstuvwxyz", 20,
		 "|          abcdefgh|            ijklmnop|            qrstuvwx|            yz");
	wraps_to("           abcdefgh ijklmnopqrstuvwxyz", 20, "|           abcdefgh|           ijklmnopq|           rstuvwxyz");
	wraps_to("              abcdefghij", 20, "|            abcdefgh|            ij");
	wraps_to("            abcdefghijk", 20, "|            abcdefgh|            ijk");
	wraps_to("", 20, "|");
}

static void test_line_index_full(void)
{
	static char big[UI_HISTORY_SPOILER_MAX];
	size_t n = 0;
	int i;

	three_seeds();
	for (i = 0; i < UI_HISTORY_LINES_MAX + 1; i++)
		big[n++] = '\n';
	big[n] = '\0';
	start();
	fake.outline = big;
	keys(UI_BUTTON_A, 1);
	keys(UI_BUTTON_R2, UI_HISTORY_LINES_MAX / PAGE + 4);
	CHECK(strstr(render(), " | Spoiler cut: too long                  |\n"
			       " |                                        |\n"
			       "_| B Back  X Delete") != NULL);
}

static void test_strings(void)
{
	UiStrId bad;
	UiStrFault fault;

	CHECK(ui_strings_check(ui_str_english(), true, &bad, &fault));
}

/* A LADX seed: its 32-digit seed, the settings Generate shows (Goal Count with the instruments goal). */
static void test_ladx(void)
{
	static char trace[32 * 1024];
	UiHistoryEntry *e;
	int i;

	memset(&list, 0, sizeof list);
	e = &list.entries[0];
	e->game = UI_GAME_LADX;
	snprintf(e->name.code, sizeof e->name.code, "%s", "3E4F50");
	snprintf(e->name.rom, sizeof e->name.rom, "%s", "Hard-Instruments-3E4F50.gbc");
	e->date.year = 2026;
	e->date.month = 11;
	e->date.day = 7;
	e->date.hour = 18;
	e->date.minute = 5;
	snprintf(e->ladx_seed, sizeof e->ladx_seed, "%s", "00112233445566778899aabbccddeeff");
	ladx_settings_defaults(&e->ladx);
	e->ladx.value[LADX_SETTING_LOGIC] = 2;
	e->ladx.value[LADX_SETTING_GOAL] = (uint8_t)ladx_setting_value_find(LADX_SETTING_GOAL, "instruments");
	e->has_spoiler = true;
	list.count = 1;
	start_game(UI_GAME_LADX);
	CHECK(ui_history_key(&history, UI_BUTTON_A) == UI_HISTORY_NONE);
	trace[0] = '\0';
	for (i = 0; i < 6; i++) {
		size_t used = strlen(trace);

		snprintf(trace + used, sizeof trace - used, "@ page %d\n%s", i, render());
		CHECK(ui_history_key(&history, UI_BUTTON_R2) == UI_HISTORY_NONE);
	}
	test_snapshot(TEST, SNAPSHOTS "history_ladx_detail.txt", trace);
	fake.reply.list_stale = true;
	CHECK(ui_history_key(&history, UI_BUTTON_X) == UI_HISTORY_NONE);
	CHECK(ui_history_key(&history, UI_BUTTON_A) == UI_HISTORY_NONE);
	CHECK(fake.delete_calls == 1 && strcmp(fake.deleted_rom, "Hard-Instruments-3E4F50.gbc") == 0);
	snapshot("history_ladx_deleted_stale.txt");
}

int main(void)
{
	test_list();
	test_list_moves();
	test_empty();
	test_notices();
	test_list_keys();
	test_detail();
	test_detail_keys();
	test_detail_cut();
	test_confirm();
	test_delete();
	test_failed_delete();
	test_failure_texts();
	test_wrapping();
	test_line_index_full();
	test_strings();
	test_hints();
	test_styles();
	test_ladx();
	printf(TEST ": passed\n");
	return 0;
}
