#include "rando_last_settings.h"
#include "rando_record.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define PATH_MAX_LEN 1024

bool rando_last_settings_save(const char *path, const RandoOptions *options, RandoHeartBeep heart_beep)
{
	char tmp[PATH_MAX_LEN];
	FILE *f;
	bool ok;

	if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
		return false;
	f = fopen(tmp, "wb");
	if (f == NULL)
		return false;
	ok = rando_record_settings_write(f, options, heart_beep, 0) && fputc('\n', f) != EOF;
	if (fclose(f) != 0)
		ok = false;
	if (ok && rename(tmp, path) == 0)
		return true;
	remove(tmp);
	return false;
}

static void defaults(RandoOptions *options, RandoHeartBeep *heart_beep)
{
	rando_options_init(options);
	*heart_beep = (RandoHeartBeep)rando_setting_default(RANDO_SETTING_HEART_BEEP);
}

RandoLastSettingsStatus rando_last_settings_load(const char *path, RandoOptions *options, RandoHeartBeep *heart_beep)
{
	char text[RANDO_LAST_SETTINGS_MAX + 1];
	char error[RANDO_RECORD_ERROR_MAX];
	RandoOptions parsed;
	RandoHeartBeep parsed_beep;
	FILE *f = fopen(path, "rb");
	size_t n;
	bool failed;

	defaults(options, heart_beep);
	if (f == NULL)
		return errno == ENOENT ? RANDO_LAST_SETTINGS_MISSING : RANDO_LAST_SETTINGS_INVALID;
	n = fread(text, 1, sizeof text, f);
	failed = ferror(f) != 0;
	fclose(f);
	if (failed || n > RANDO_LAST_SETTINGS_MAX || !rando_record_settings_parse(text, n, &parsed, &parsed_beep, error))
		return RANDO_LAST_SETTINGS_INVALID;
	*options = parsed;
	*heart_beep = parsed_beep;
	return RANDO_LAST_SETTINGS_LOADED;
}
