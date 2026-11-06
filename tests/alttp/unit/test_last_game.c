#define _POSIX_C_SOURCE 200809L

#include "rando_last_game.h"
#include "rando_seed_save.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_last_game"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static char root[64], path[128], tmp_path[160];

static void make_root(void)
{
	strcpy(root, "/tmp/test_last_game_XXXXXX");
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	snprintf(path, sizeof path, "%s/settings.json", root);
	snprintf(tmp_path, sizeof tmp_path, "%s.tmp", path);
}

static void remove_root(void)
{
	remove(tmp_path);
	remove(path);
	rmdir(path);
	rmdir(root);
}

static bool exists(const char *p)
{
	struct stat st;

	return stat(p, &st) == 0;
}

static void write_file(const char *p, const char *bytes, size_t n)
{
	FILE *f = fopen(p, "wb");

	CHECK(f != NULL);
	CHECK(fwrite(bytes, 1, n, f) == n);
	CHECK(fclose(f) == 0);
}

/* Loads path holding bytes; the game must be ALttP whenever the status is not loaded. */
static void expect(const char *what, const char *bytes, size_t n, RandoLastGameStatus status, UiGame game)
{
	UiGame loaded = UI_GAME_COUNT;
	RandoLastGameStatus got;

	make_root();
	write_file(path, bytes, n);
	got = rando_last_game_load(path, &loaded);
	remove_root();
	if (got != status || loaded != game)
		test_fail(TEST, "%s: status %d game %d, expected %d and %d", what, (int)got, (int)loaded, (int)status,
			  (int)game);
}

static void expect_text(const char *what, const char *text, RandoLastGameStatus status, UiGame game)
{
	expect(what, text, strlen(text), status, game);
}

static void test_missing(void)
{
	UiGame game = UI_GAME_LADX;

	make_root();
	CHECK(rando_last_game_load(path, &game) == RANDO_LAST_GAME_MISSING);
	CHECK(game == UI_GAME_ALTTP);
	remove_root();
}

static void test_valid(void)
{
	expect_text("alttp", "{\"last_game\":\"alttp\"}\n", RANDO_LAST_GAME_LOADED, UI_GAME_ALTTP);
	expect_text("ladx", "{\"last_game\":\"ladx\"}\n", RANDO_LAST_GAME_LOADED, UI_GAME_LADX);
	expect_text("no newline", "{\"last_game\":\"ladx\"}", RANDO_LAST_GAME_LOADED, UI_GAME_LADX);
	expect_text("whitespace", " \t{\r\n  \"last_game\" :\n\t\"ladx\"  }\r\n\n ", RANDO_LAST_GAME_LOADED,
		    UI_GAME_LADX);
}

static void test_invalid(void)
{
	static char big[RANDO_LAST_GAME_MAX + 2];
	static const char nul[] = "{\"last_game\":\"ladx\"}\0";
	UiGame game = UI_GAME_LADX;

	expect_text("empty", "", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("garbage", "not json\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("wrong key", "{\"game\":\"ladx\"}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("unknown value", "{\"last_game\":\"zelda1\"}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("value case", "{\"last_game\":\"LADX\"}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("value prefix", "{\"last_game\":\"ladxx\"}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("not a string", "{\"last_game\":1}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("extra member", "{\"last_game\":\"ladx\",\"x\":1}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("repeated member", "{\"last_game\":\"ladx\",\"last_game\":\"ladx\"}\n", RANDO_LAST_GAME_INVALID,
		    UI_GAME_ALTTP);
	expect_text("trailing comma", "{\"last_game\":\"ladx\",}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("trailing text", "{\"last_game\":\"ladx\"} x\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("two objects", "{\"last_game\":\"ladx\"}{}\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("truncated", "{\"last_game\":\"lad", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("no closing brace", "{\"last_game\":\"ladx\"\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect_text("an array", "[\"ladx\"]\n", RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect("a NUL byte", nul, sizeof nul - 1, RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	memset(big, ' ', sizeof big);
	memcpy(big, "{\"last_game\":\"ladx\"}", 20);
	expect("over the cap", big, sizeof big, RANDO_LAST_GAME_INVALID, UI_GAME_ALTTP);
	expect("at the cap", big, RANDO_LAST_GAME_MAX, RANDO_LAST_GAME_LOADED, UI_GAME_LADX);

	make_root();
	CHECK(mkdir(path, 0700) == 0);
	CHECK(rando_last_game_load(path, &game) == RANDO_LAST_GAME_INVALID);
	CHECK(game == UI_GAME_ALTTP);
	remove_root();
}

static void test_round_trip(void)
{
	static const char *const texts[UI_GAME_COUNT] = {"{\"last_game\":\"alttp\"}\n", "{\"last_game\":\"ladx\"}\n"};
	char bytes[64];
	int game;

	for (game = 0; game < UI_GAME_COUNT; game++) {
		UiGame loaded = UI_GAME_COUNT;
		FILE *f;
		size_t n;

		make_root();
		CHECK(rando_last_game_save(path, (UiGame)game));
		CHECK(!exists(tmp_path));
		f = fopen(path, "rb");
		CHECK(f != NULL);
		n = fread(bytes, 1, sizeof bytes - 1, f);
		fclose(f);
		bytes[n] = '\0';
		CHECK(strcmp(bytes, texts[game]) == 0);
		CHECK(rando_last_game_load(path, &loaded) == RANDO_LAST_GAME_LOADED);
		CHECK(loaded == (UiGame)game);
		CHECK(rando_last_game_save(path, (UiGame)(1 - game)));
		CHECK(rando_last_game_load(path, &loaded) == RANDO_LAST_GAME_LOADED);
		CHECK(loaded == (UiGame)(1 - game));
		remove_root();
	}
}

static void test_failed_save(void)
{
	char missing[192], missing_dir[160];
	UiGame loaded;

	make_root();
	snprintf(missing_dir, sizeof missing_dir, "%s/no_such_dir", root);
	snprintf(missing, sizeof missing, "%s/settings.json", missing_dir);
	CHECK(!rando_last_game_save(missing, UI_GAME_LADX));
	CHECK(!exists(missing_dir));
	CHECK(!exists(path));
	CHECK(mkdir(path, 0700) == 0);
	CHECK(!rando_last_game_save(path, UI_GAME_LADX));
	CHECK(!exists(tmp_path));
	CHECK(rando_last_game_load(path, &loaded) == RANDO_LAST_GAME_INVALID);
	remove_root();
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	CHECK(strcmp(RANDO_SD_LAST_GAME_FILE, "App/ZeldaRando/settings.json") == 0);
	test_missing();
	test_valid();
	test_invalid();
	test_round_trip();
	test_failed_save();
	printf("%s: passed\n", TEST);
	return 0;
}
