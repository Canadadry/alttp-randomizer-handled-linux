#ifndef RANDO_LAST_SETTINGS_H
#define RANDO_LAST_SETTINGS_H

#include "rando_options.h"
#include "rando_settings.h"

#include <stdbool.h>

#define RANDO_LAST_SETTINGS_MAX 4096

typedef enum RandoLastSettingsStatus {
	RANDO_LAST_SETTINGS_LOADED,
	RANDO_LAST_SETTINGS_MISSING,
	RANDO_LAST_SETTINGS_INVALID,
} RandoLastSettingsStatus;

bool rando_last_settings_save(const char *path, const RandoOptions *options, RandoHeartBeep heart_beep);

RandoLastSettingsStatus rando_last_settings_load(const char *path, RandoOptions *options, RandoHeartBeep *heart_beep);

#endif
