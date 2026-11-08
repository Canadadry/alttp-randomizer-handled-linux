#include "rando_ladx_name.h"

#include "rando_crc32.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *const logic_words[] = {"Casual", "Normal", "Hard", "Glitched", "Hell"};

static const char *const goal_words[] = {"Vanilla",   "Instruments", "Specific",    "Open",
					 "Short",     "Long",	     "Seashells",   "Bingo",
					 "DoubleBingo", "TripleBingo", "Bingo25", "SignMaze"};

typedef char logic_words_match[sizeof logic_words / sizeof logic_words[0] == 5 ? 1 : -1];
typedef char goal_words_match[sizeof goal_words / sizeof goal_words[0] == 12 ? 1 : -1];

const char *rando_ladx_name_logic(int logic)
{
	assert(logic >= 0 && logic < ladx_setting_value_count(LADX_SETTING_LOGIC));
	return logic_words[logic];
}

const char *rando_ladx_name_goal(int goal)
{
	assert(goal >= 0 && goal < ladx_setting_value_count(LADX_SETTING_GOAL));
	return goal_words[goal];
}

/* "seed=" and 32 digits, then at most 47 settings of a 20-byte key and a 20-byte value. */
#define CODE_TEXT_MAX 4096

static size_t append(char *text, size_t at, const char *key, const char *value)
{
	int n = snprintf(text + at, CODE_TEXT_MAX - at, "%s=%s\n", key, value);

	assert(n > 0 && (size_t)n < CODE_TEXT_MAX - at);
	return at + (size_t)n;
}

uint32_t rando_ladx_name_code(const LadxSettings *settings, const char *seed_hex)
{
	char text[CODE_TEXT_MAX];
	size_t at;
	int k;

	assert(strlen(seed_hex) == RANDO_LADX_SEED_HEX);
	at = append(text, 0, "seed", seed_hex);
	for (k = 0; k < LADX_SETTING_COUNT; k++)
		at = append(text, at, ladx_setting_key((LadxSettingId)k),
			    ladx_setting_value_name((LadxSettingId)k, settings->value[k]));
	return rando_crc32((const uint8_t *)text, at) & 0xFFFFFFu;
}

void rando_ladx_name_make(const LadxSettings *settings, uint32_t code, int suffix, RandoName *out)
{
	int n;

	assert(code <= 0xFFFFFFu);
	assert(suffix >= 1 && suffix <= RANDO_NAME_SUFFIX_MAX);
	if (suffix > 1)
		snprintf(out->code, sizeof out->code, "%06lX-%d", (unsigned long)code, suffix);
	else
		snprintf(out->code, sizeof out->code, "%06lX", (unsigned long)code);
	n = snprintf(out->rom, sizeof out->rom, "%s-%s-%s.gbc", rando_ladx_name_logic(settings->value[LADX_SETTING_LOGIC]),
		     rando_ladx_name_goal(settings->value[LADX_SETTING_GOAL]), out->code);
	assert(n > 0 && (size_t)n < sizeof out->rom);
	(void)n;
}

bool rando_ladx_name_choose(const LadxSettings *settings, const char *seed_hex, RandoNameTakenFn taken, void *ctx,
			    RandoName *out)
{
	uint32_t code = rando_ladx_name_code(settings, seed_hex);
	int suffix;

	for (suffix = 1; suffix <= RANDO_NAME_SUFFIX_MAX; suffix++) {
		rando_ladx_name_make(settings, code, suffix, out);
		if (!taken(ctx, out))
			return true;
	}
	return false;
}
