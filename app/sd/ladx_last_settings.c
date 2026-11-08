#include "rando_ladx_last_settings.h"
#include "rando_ladx_record.h"

#include <errno.h>
#include <stdio.h>

#define PATH_MAX_LEN 1024

bool rando_ladx_last_settings_save(const char *path, const LadxSettings *settings)
{
	char tmp[PATH_MAX_LEN];
	FILE *f;
	bool ok;

	if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
		return false;
	f = fopen(tmp, "wb");
	if (f == NULL)
		return false;
	ok = rando_ladx_settings_write(f, settings);
	if (fclose(f) != 0)
		ok = false;
	if (ok && rename(tmp, path) == 0)
		return true;
	remove(tmp);
	return false;
}

RandoLastSettingsStatus rando_ladx_last_settings_load(const char *path, LadxSettings *settings)
{
	char text[RANDO_LAST_SETTINGS_MAX + 1];
	char error[RANDO_RECORD_ERROR_MAX];
	FILE *f = fopen(path, "rb");
	size_t n;
	bool failed;

	ladx_settings_defaults(settings);
	if (f == NULL)
		return errno == ENOENT ? RANDO_LAST_SETTINGS_MISSING : RANDO_LAST_SETTINGS_INVALID;
	n = fread(text, 1, sizeof text, f);
	failed = ferror(f) != 0;
	fclose(f);
	if (failed || n > RANDO_LAST_SETTINGS_MAX || !rando_ladx_settings_parse(text, n, settings, error))
		return RANDO_LAST_SETTINGS_INVALID;
	return RANDO_LAST_SETTINGS_LOADED;
}
