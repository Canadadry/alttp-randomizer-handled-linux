#include "rando_last_game.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define PATH_MAX_LEN 1024
#define KEY "last_game"

static const char *const values[UI_GAME_COUNT] = {
	[UI_GAME_ALTTP] = "alttp",
	[UI_GAME_LADX] = "ladx",
};

bool rando_last_game_save(const char *path, UiGame game)
{
	char tmp[PATH_MAX_LEN];
	FILE *f;
	bool ok;

	if ((int)game < 0 || game >= UI_GAME_COUNT || snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
		return false;
	f = fopen(tmp, "wb");
	if (f == NULL)
		return false;
	ok = fprintf(f, "{\"" KEY "\":\"%s\"}\n", values[game]) > 0;
	if (fclose(f) != 0)
		ok = false;
	if (ok && rename(tmp, path) == 0)
		return true;
	remove(tmp);
	return false;
}

typedef struct Cursor {
	const char *at, *end;
} Cursor;

static void skip_space(Cursor *c)
{
	while (c->at < c->end && (*c->at == ' ' || *c->at == '\t' || *c->at == '\n' || *c->at == '\r'))
		c->at++;
}

static bool take(Cursor *c, char ch)
{
	skip_space(c);
	if (c->at >= c->end || *c->at != ch)
		return false;
	c->at++;
	return true;
}

static bool take_string(Cursor *c, const char *word)
{
	size_t n = strlen(word);

	if (!take(c, '"') || (size_t)(c->end - c->at) < n + 1 || memcmp(c->at, word, n) != 0 || c->at[n] != '"')
		return false;
	c->at += n + 1;
	return true;
}

static bool parse(const char *text, size_t n, UiGame *game)
{
	Cursor c;
	int g;

	if (memchr(text, '\0', n) != NULL)
		return false;
	c.at = text;
	c.end = text + n;
	if (!take(&c, '{') || !take_string(&c, KEY) || !take(&c, ':'))
		return false;
	for (g = 0; g < UI_GAME_COUNT; g++) {
		Cursor v = c;

		if (take_string(&v, values[g]) && take(&v, '}')) {
			skip_space(&v);
			if (v.at != v.end)
				return false;
			*game = (UiGame)g;
			return true;
		}
	}
	return false;
}

RandoLastGameStatus rando_last_game_load(const char *path, UiGame *game)
{
	char text[RANDO_LAST_GAME_MAX + 1];
	FILE *f = fopen(path, "rb");
	size_t n;
	bool failed;

	*game = UI_GAME_ALTTP;
	if (f == NULL)
		return errno == ENOENT ? RANDO_LAST_GAME_MISSING : RANDO_LAST_GAME_INVALID;
	n = fread(text, 1, sizeof text, f);
	failed = ferror(f) != 0;
	fclose(f);
	if (failed || n > RANDO_LAST_GAME_MAX || !parse(text, n, game))
		return RANDO_LAST_GAME_INVALID;
	return RANDO_LAST_GAME_LOADED;
}
