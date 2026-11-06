#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include "rando_settings.h"
#include "ui_strings.h"

#include <stdbool.h>

#define UI_SETTING_VALUES_MAX 9

UiStrId ui_setting_label(RandoSettingId id);
UiStrId ui_setting_value(RandoSettingId id, int value);
UiStrId ui_setting_help(RandoSettingId id, int value);
UiStrId ui_spoiler_help(bool on);

#endif
