#include "rando_name.h"

#include "rando_crc32.h"
#include "rando_settings.h"

#include <assert.h>
#include <string.h>

static const char *const mode_words[] = {
	[RANDO_OPTION_STATE_OPEN] = "Open",
	[RANDO_OPTION_STATE_STANDARD] = "Standard",
	[RANDO_OPTION_STATE_INVERTED] = "Inverted",
	[RANDO_OPTION_STATE_RETRO] = "Retro",
};

static const char *const goal_words[] = {
	[RANDO_OPTION_GOAL_GANON] = "Ganon",
	[RANDO_OPTION_GOAL_FAST_GANON] = "FastGanon",
	[RANDO_OPTION_GOAL_DUNGEONS] = "Dungeons",
	[RANDO_OPTION_GOAL_PEDESTAL] = "Pedestal",
	[RANDO_OPTION_GOAL_TRIFORCE_HUNT] = "TriforceHunt",
	[RANDO_OPTION_GOAL_GANONHUNT] = "GanonHunt",
	[RANDO_OPTION_GOAL_COMPLETIONIST] = "Completionist",
};

const char *rando_name_mode(RandoOptionState state)
{
	assert((unsigned)state < sizeof mode_words / sizeof mode_words[0]);
	return mode_words[state];
}

const char *rando_name_goal(RandoOptionGoal goal)
{
	assert((unsigned)goal < sizeof goal_words / sizeof goal_words[0]);
	return goal_words[goal];
}

static char *append(char *at, const char *s)
{
	size_t n = strlen(s);

	memcpy(at, s, n);
	return at + n;
}

#define CODE_TEXT_MAX 512

static char *append_line(char *at, const char *end, const char *key, const char *value)
{
	assert((size_t)(end - at) > strlen(key) + strlen(value) + 2);
	at = append(at, key);
	*at++ = '=';
	at = append(at, value);
	*at++ = '\n';
	return at;
}

uint32_t rando_name_code(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed)
{
	char text[CODE_TEXT_MAX], digits[11];
	char *at = text, *end = text + sizeof text, *d = digits + sizeof digits;
	int k;

	*--d = '\0';
	do {
		*--d = (char)('0' + seed % 10u);
		seed /= 10u;
	} while (seed != 0);
	at = append_line(at, end, "seed", d);
	for (k = 0; k < RANDO_SETTING_COUNT; k++)
		at = append_line(at, end, rando_setting_key((RandoSettingId)k),
				 rando_setting_value_name((RandoSettingId)k,
							  rando_setting_get(options, heart_beep, (RandoSettingId)k)));
	return rando_crc32((const uint8_t *)text, (size_t)(at - text)) & 0xFFFFFFu;
}

void rando_name_make(const RandoOptions *options, uint32_t code, int suffix, RandoName *out)
{
	static const char hex[] = "0123456789ABCDEF";
	char *at = out->code;
	int i;

	assert(code <= 0xFFFFFFu);
	assert(suffix >= 1 && suffix <= RANDO_NAME_SUFFIX_MAX);
	for (i = 5; i >= 0; i--)
		*at++ = hex[(code >> (4 * i)) & 0xFu];
	if (suffix > 1) {
		*at++ = '-';
		if (suffix >= 10)
			*at++ = (char)('0' + suffix / 10);
		*at++ = (char)('0' + suffix % 10);
	}
	*at = '\0';

	at = append(out->rom, rando_name_mode(options->state));
	*at++ = '-';
	at = append(at, rando_name_goal(options->goal));
	*at++ = '-';
	at = append(at, out->code);
	at = append(at, ".sfc");
	*at = '\0';
}

bool rando_name_choose(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed,
		       RandoNameTakenFn taken, void *ctx, RandoName *out)
{
	uint32_t code = rando_name_code(options, heart_beep, seed);
	int suffix;

	for (suffix = 1; suffix <= RANDO_NAME_SUFFIX_MAX; suffix++) {
		rando_name_make(options, code, suffix, out);
		if (!taken(ctx, out))
			return true;
	}
	return false;
}

bool rando_name_code_valid(const char *code)
{
	int i, suffix;

	for (i = 0; i < 6; i++, code++)
		if (!((*code >= '0' && *code <= '9') || (*code >= 'A' && *code <= 'F')))
			return false;
	if (*code == '\0')
		return true;
	if (*code++ != '-' || *code < '1' || *code > '9')
		return false;
	suffix = *code++ - '0';
	if (*code >= '0' && *code <= '9')
		suffix = suffix * 10 + (*code++ - '0');
	return *code == '\0' && suffix >= 2 && suffix <= RANDO_NAME_SUFFIX_MAX;
}
