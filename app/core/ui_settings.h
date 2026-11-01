#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include "rando_settings.h"
#include "ui_strings.h"

/* The most values a setting has; every rando_setting_value_count is at most this. */
#define UI_SETTING_VALUES_MAX 9

/* The string ids of a setting's label and of its value index. Assert the ranges. */
UiStrId ui_setting_label(RandoSettingId id);
UiStrId ui_setting_value(RandoSettingId id, int value);

#endif
