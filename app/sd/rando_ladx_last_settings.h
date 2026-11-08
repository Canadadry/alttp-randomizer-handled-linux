#ifndef RANDO_LADX_LAST_SETTINGS_H
#define RANDO_LADX_LAST_SETTINGS_H

#include "ladx_settings.h"
#include "rando_last_settings.h"

#include <stdbool.h>

bool rando_ladx_last_settings_save(const char *path, const LadxSettings *settings);

RandoLastSettingsStatus rando_ladx_last_settings_load(const char *path, LadxSettings *settings);

#endif
