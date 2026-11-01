#ifndef RANDO_LAST_SETTINGS_H
#define RANDO_LAST_SETTINGS_H

#include "rando_options.h"
#include "rando_settings.h"

#include <stdbool.h>

#define RANDO_LAST_SETTINGS_MAX 4096

typedef enum RandoLastSettingsStatus {
	RANDO_LAST_SETTINGS_LOADED,
	RANDO_LAST_SETTINGS_MISSING, /* no file */
	RANDO_LAST_SETTINGS_INVALID, /* unreadable, over RANDO_LAST_SETTINGS_MAX bytes, or refused by the parser */
} RandoLastSettingsStatus;

/* Writes <path>.tmp with the record's settings writer at depth 0 and a newline, then renames it over path.
 * On any failure removes the temporary file, leaves path as it was and returns false. */
bool rando_last_settings_save(const char *path, const RandoOptions *options, RandoHeartBeep heart_beep);

/* Fills the outputs with the file's settings when LOADED, with the table's defaults otherwise: never with
 * partly parsed values. */
RandoLastSettingsStatus rando_last_settings_load(const char *path, RandoOptions *options, RandoHeartBeep *heart_beep);

#endif
