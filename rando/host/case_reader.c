#include "rando_case.h"
#include "rando_settings.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CASE_FILE_MAX 4096
#define CASE_STRING_MAX 64
#define CASE_PATH_MAX 1024

static bool fail(char error[RANDO_CASE_ERROR_MAX], const char *file, const char *fmt, ...)
{
	int n = snprintf(error, RANDO_CASE_ERROR_MAX, "%s: ", file);
	va_list ap;

	if (n < 0 || n >= RANDO_CASE_ERROR_MAX)
		return false;
	va_start(ap, fmt);
	vsnprintf(error + n, (size_t)(RANDO_CASE_ERROR_MAX - n), fmt, ap);
	va_end(ap);
	return false;
}

typedef struct Parser {
	const char *p;
	const char *end;
} Parser;

static void skip_space(Parser *ps)
{
	while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r'))
		ps->p++;
}

static bool at(const Parser *ps, char c)
{
	return ps->p < ps->end && *ps->p == c;
}

static const char *parse_string(Parser *ps, char out[CASE_STRING_MAX])
{
	size_t n = 0;

	ps->p++;
	for (; ps->p < ps->end; ps->p++) {
		unsigned char c = (unsigned char)*ps->p;

		if (c == '"') {
			ps->p++;
			out[n] = '\0';
			return NULL;
		}
		if (c == '\\')
			return "escape in a string";
		if (c < 0x20)
			return "control character in a string";
		if (n + 1 >= CASE_STRING_MAX)
			return "string too long";
		out[n++] = (char)c;
	}
	return "unterminated string";
}

bool rando_case_parse_config(const char *text, size_t len, const char *file, RandoCase *out,
			     char error[RANDO_CASE_ERROR_MAX])
{
	Parser ps = {text, text + len};
	bool seen[RANDO_SETTING_COUNT] = {false};
	char key[CASE_STRING_MAX], value[CASE_STRING_MAX];
	const char *why;
	int k;

	rando_options_init(&out->options);
	out->heart_beep = (RandoHeartBeep)rando_setting_default(RANDO_SETTING_HEART_BEEP);
	skip_space(&ps);
	if (!at(&ps, '{'))
		return fail(error, file, "not a JSON object");
	ps.p++;
	skip_space(&ps);
	if (at(&ps, '}'))
		ps.p++;
	else
		for (;;) {
			RandoSettingId setting;
			int v;

			skip_space(&ps);
			if (!at(&ps, '"'))
				return fail(error, file, "expected a string key");
			if ((why = parse_string(&ps, key)) != NULL)
				return fail(error, file, "%s", why);
			skip_space(&ps);
			if (!at(&ps, ':'))
				return fail(error, file, "expected ':' after %s", key);
			ps.p++;
			skip_space(&ps);
			if (!at(&ps, '"'))
				return fail(error, file, "%s is not a string", key);
			if ((why = parse_string(&ps, value)) != NULL)
				return fail(error, file, "%s: %s", key, why);
			setting = rando_setting_find(key);
			if (setting == RANDO_SETTING_COUNT)
				return fail(error, file, "unknown key %s", key);
			if (seen[setting])
				return fail(error, file, "duplicate key %s", key);
			v = rando_setting_value_find(setting, value);
			if (v < 0)
				return fail(error, file, "unknown %s %s", key, value);
			rando_setting_set(&out->options, &out->heart_beep, setting, v);
			seen[setting] = true;
			skip_space(&ps);
			if (at(&ps, ',')) {
				ps.p++;
				continue;
			}
			if (!at(&ps, '}'))
				return fail(error, file, "expected ',' or '}' after %s", key);
			ps.p++;
			break;
		}
	skip_space(&ps);
	if (ps.p != ps.end)
		return fail(error, file, "trailing data after the object");
	for (k = 0; k < RANDO_OPTION_KEY_COUNT; k++)
		if (!seen[k])
			return fail(error, file, "missing key %s", rando_setting_key((RandoSettingId)k));
	return true;
}

bool rando_case_parse_uint32(const char *text, size_t len, const char *file, uint32_t *out,
			     char error[RANDO_CASE_ERROR_MAX])
{
	if (!rando_parse_uint32(text, len, out))
		return fail(error, file, "not a decimal uint32 on one line");
	return true;
}

typedef enum ReadResult {
	READ_OK,
	READ_ABSENT,
	READ_FAILED,
} ReadResult;

static ReadResult read_file(const char *path, char buf[CASE_FILE_MAX], size_t *len, char error[RANDO_CASE_ERROR_MAX])
{
	FILE *f = fopen(path, "rb");
	bool failed;

	if (f == NULL) {
		if (errno == ENOENT)
			return READ_ABSENT;
		fail(error, path, "cannot open: %s", strerror(errno));
		return READ_FAILED;
	}
	*len = fread(buf, 1, CASE_FILE_MAX, f);
	failed = ferror(f) != 0;
	fclose(f);
	if (failed) {
		fail(error, path, "read error");
		return READ_FAILED;
	}
	if (*len == CASE_FILE_MAX) {
		fail(error, path, "larger than %d bytes", CASE_FILE_MAX - 1);
		return READ_FAILED;
	}
	return READ_OK;
}

static bool case_path(char path[CASE_PATH_MAX], const char *case_dir, const char *name,
		      char error[RANDO_CASE_ERROR_MAX])
{
	if (snprintf(path, CASE_PATH_MAX, "%s/%s", case_dir, name) >= CASE_PATH_MAX)
		return fail(error, case_dir, "path too long");
	return true;
}

bool rando_case_read(const char *case_dir, RandoCase *out, char error[RANDO_CASE_ERROR_MAX])
{
	char path[CASE_PATH_MAX], buf[CASE_FILE_MAX];
	size_t len;
	ReadResult r;

	if (!case_path(path, case_dir, "config.json", error))
		return false;
	r = read_file(path, buf, &len, error);
	if (r == READ_ABSENT)
		return fail(error, path, "missing");
	if (r == READ_FAILED || !rando_case_parse_config(buf, len, path, out, error))
		return false;

	if (!case_path(path, case_dir, "seed", error))
		return false;
	r = read_file(path, buf, &len, error);
	if (r == READ_ABSENT)
		return fail(error, path, "missing");
	if (r == READ_FAILED || !rando_case_parse_uint32(buf, len, path, &out->seed, error))
		return false;

	if (!case_path(path, case_dir, "attempts", error))
		return false;
	r = read_file(path, buf, &len, error);
	if (r == READ_FAILED)
		return false;
	out->has_attempts = r == READ_OK;
	out->attempts = 0;
	return !out->has_attempts || rando_case_parse_uint32(buf, len, path, &out->attempts, error);
}
