#include "ui_strings.h"

static const UiStrTable english = { {
	[UI_STR_NONE] = "",
	[UI_STR_TAB_GENERATE] = "Generate",
	[UI_STR_TAB_HISTORY] = "History",
	[UI_STR_HISTORY_DELETE_CONFIRM] = "Delete {}?",
} };

const UiStrTable *ui_str_english(void)
{
	return &english;
}
