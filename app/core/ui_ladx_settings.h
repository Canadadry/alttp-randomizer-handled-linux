#ifndef UI_LADX_SETTINGS_H
#define UI_LADX_SETTINGS_H

#include "ladx_settings.h"
#include "ui_strings.h"

#include <stdbool.h>

UiStrId ui_ladx_category_label(LadxSettingCategory category);
UiStrId ui_ladx_setting_label(LadxSettingId id);
UiStrId ui_ladx_setting_value(LadxSettingId id, int value);
UiStrId ui_ladx_setting_help(LadxSettingId id);
UiStrId ui_ladx_reason(LadxReason reason);

bool ui_ladx_setting_shown(const LadxSettings *settings, LadxSettingId id);

#endif
