#include "rando_fixed_run.h"

#include "rando_settings.h"

#include <assert.h>
#include <string.h>

static const char *const key_names[] = {"up", "down", "left", "right", "a",	  "b",	   "x",	   "y",
					"l1", "r1",   "l2",   "r2",    "select", "start", "menu"};

typedef char key_names_match_buttons[sizeof key_names / sizeof key_names[0] == UI_BUTTON_COUNT ? 1 : -1];

static uint32_t fixed_seed_value;
static RandoRecordDate fixed_date_value;

static uint32_t fixed_seed(void)
{
	return fixed_seed_value;
}

static void fixed_date(RandoRecordDate *out)
{
	*out = fixed_date_value;
}

const char *rando_fixed_key_name(UiButton button)
{
	assert((unsigned)button < UI_BUTTON_COUNT);
	return key_names[button];
}

static bool parse_number(const char *s, uint32_t *out)
{
	uint32_t v;

	if (!rando_parse_uint32(s, strlen(s), &v) || strchr(s, '\n') != NULL)
		return false;
	*out = v;
	return true;
}

RandoFixedKeyStatus rando_fixed_keys_add(RandoFixedKeys *keys, const char *token)
{
	char name[RANDO_FIXED_TOKEN_MAX];
	const char *star = strchr(token, '*');
	size_t len = star != NULL ? (size_t)(star - token) : strlen(token);
	uint32_t count = 1;
	int b;

	if (len >= sizeof name)
		return RANDO_FIXED_KEY_UNKNOWN;
	memcpy(name, token, len);
	name[len] = '\0';
	if (star != NULL && (!parse_number(star + 1, &count) || count < 1 || count > RANDO_FIXED_REPEAT_MAX))
		return RANDO_FIXED_KEY_BAD_REPEAT;
	for (b = 0; b < UI_BUTTON_COUNT; b++)
		if (strcmp(name, key_names[b]) == 0)
			break;
	if (b == UI_BUTTON_COUNT)
		return RANDO_FIXED_KEY_UNKNOWN;
	if (keys->count == RANDO_FIXED_KEYS_MAX)
		return RANDO_FIXED_KEY_TOO_MANY;
	keys->key[keys->count].button = (UiButton)b;
	keys->key[keys->count].count = (int)count;
	keys->count++;
	keys->presses += (long)count;
	return RANDO_FIXED_KEY_OK;
}

RandoFixedKeyStatus rando_fixed_keys_read(RandoFixedKeys *keys, FILE *in, char bad[RANDO_FIXED_TOKEN_MAX + 1])
{
	char token[RANDO_FIXED_TOKEN_MAX + 1];
	size_t len = 0;
	int c;

	do {
		c = getc(in);
		if (c == EOF || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (len > 0) {
				RandoFixedKeyStatus status;

				token[len] = '\0';
				status = rando_fixed_keys_add(keys, token);
				if (status != RANDO_FIXED_KEY_OK) {
					memcpy(bad, token, len + 1);
					return status;
				}
				len = 0;
			}
		} else {
			if (len == RANDO_FIXED_TOKEN_MAX) {
				token[len] = '\0';
				memcpy(bad, token, len + 1);
				return RANDO_FIXED_KEY_UNKNOWN;
			}
			token[len++] = (char)c;
		}
	} while (c != EOF);
	return RANDO_FIXED_KEY_OK;
}

RandoFixedKeyStatus rando_fixed_keys_arg(RandoFixedKeys *keys, const char *arg, FILE *in,
					 char buf[RANDO_FIXED_TOKEN_MAX + 1], const char **bad)
{
	if (strcmp(arg, "-") == 0) {
		*bad = buf;
		return rando_fixed_keys_read(keys, in, buf);
	}
	*bad = arg;
	return rando_fixed_keys_add(keys, arg);
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool parse_md5(const char *s, uint8_t out[RANDO_MD5_SIZE])
{
	uint8_t md5[RANDO_MD5_SIZE];
	int i;

	if (strlen(s) != 2 * RANDO_MD5_SIZE)
		return false;
	for (i = 0; i < RANDO_MD5_SIZE; i++) {
		int hi = hex_digit(s[2 * i]), lo = hex_digit(s[2 * i + 1]);

		if (hi < 0 || lo < 0)
			return false;
		md5[i] = (uint8_t)(hi * 16 + lo);
	}
	memcpy(out, md5, RANDO_MD5_SIZE);
	return true;
}

static bool field(const char *s, int from, int n, int min, int max, int *out)
{
	int i, v = 0;

	for (i = from; i < from + n; i++) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		v = v * 10 + (s[i] - '0');
	}
	*out = v;
	return v >= min && v <= max;
}

static bool parse_date(const char *s, RandoRecordDate *out)
{
	RandoRecordDate d;

	if (!(strlen(s) == 19 && s[4] == '-' && s[7] == '-' && s[10] == 'T' && s[13] == ':' && s[16] == ':' &&
	      field(s, 0, 4, 0, 9999, &d.year) && field(s, 5, 2, 1, 12, &d.month) && field(s, 8, 2, 1, 31, &d.day) &&
	      field(s, 11, 2, 0, 23, &d.hour) && field(s, 14, 2, 0, 59, &d.minute) &&
	      field(s, 17, 2, 0, 59, &d.second) && rando_record_date_valid(&d)))
		return false;
	*out = d;
	return true;
}

/* <setting>=<value> in s[0..len), both library names; false on anything else. */
static bool parse_setting(const char *s, size_t len, RandoSettingId *id, int *value)
{
	char buf[RANDO_FIXED_TOKEN_MAX + 1];
	const char *eq;

	if (len >= sizeof buf)
		return false;
	memcpy(buf, s, len);
	buf[len] = '\0';
	eq = strchr(buf, '=');
	if (eq == NULL)
		return false;
	buf[eq - buf] = '\0';
	*id = rando_setting_find(buf);
	if (*id == RANDO_SETTING_COUNT)
		return false;
	*value = rando_setting_value_find(*id, eq + 1);
	return *value >= 0;
}

static bool parse_pair(const char *s, RandoSettingPair *out)
{
	const char *comma = strchr(s, ',');
	RandoSettingPair pair;

	if (comma == NULL || !parse_setting(s, (size_t)(comma - s), &pair.a, &pair.value_a) ||
	    !parse_setting(comma + 1, strlen(comma + 1), &pair.b, &pair.value_b) || pair.a == pair.b)
		return false;
	*out = pair;
	return true;
}

bool rando_fixed_run_takes(const char *option)
{
	return strcmp(option, "--seed") == 0 || strcmp(option, "--date") == 0 ||
	       strcmp(option, "--source-md5") == 0 || strcmp(option, "--base-md5") == 0 ||
	       strcmp(option, "--unsupported") == 0;
}

bool rando_fixed_run_option(RandoFixedRun *run, const char *option, const char *value)
{
	assert(rando_fixed_run_takes(option));
	if (strcmp(option, "--seed") == 0) {
		if (!parse_number(value, &run->seed))
			return false;
		run->has_seed = true;
	} else if (strcmp(option, "--date") == 0) {
		if (!parse_date(value, &run->date))
			return false;
		run->has_date = true;
	} else if (strcmp(option, "--source-md5") == 0) {
		if (!parse_md5(value, run->source_md5))
			return false;
		run->has_source_md5 = true;
	} else if (strcmp(option, "--base-md5") == 0) {
		if (!parse_md5(value, run->base_md5))
			return false;
		run->has_base_md5 = true;
	} else {
		if (!parse_pair(value, &run->unsupported))
			return false;
		run->has_unsupported = true;
	}
	return true;
}

const RandoSettingPair *rando_fixed_run_unsupported(const RandoFixedRun *run)
{
	return run->has_unsupported ? &run->unsupported : NULL;
}

void rando_fixed_run_apply(const RandoFixedRun *run, RandoAppShell *shell)
{
	if (run->has_seed) {
		fixed_seed_value = run->seed;
		shell->gen.seed = fixed_seed;
	}
	if (run->has_date) {
		fixed_date_value = run->date;
		shell->gen.date = fixed_date;
	}
	if (run->has_source_md5)
		memcpy(shell->gen.source_md5, run->source_md5, RANDO_MD5_SIZE);
	if (run->has_base_md5)
		memcpy(shell->gen.base_md5, run->base_md5, RANDO_MD5_SIZE);
}
