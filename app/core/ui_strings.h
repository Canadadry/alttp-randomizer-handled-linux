#ifndef UI_STRINGS_H
#define UI_STRINGS_H

#include "ui_grid.h"

#include <stdbool.h>
#include <stddef.h>

/* X(id, arity): every text the app shows. A new text is one line here and one in ui_strings_en.c. */
#define UI_STR_LIST(X) \
	X(UI_STR_NONE, 0) \
	X(UI_STR_TAB_GENERATE, 0) \
	X(UI_STR_TAB_HISTORY, 0) \
	X(UI_STR_HISTORY_DELETE_CONFIRM, 1) \
	X(UI_STR_SETTING_GLITCHES, 0) \
	X(UI_STR_SETTING_ITEM_PLACEMENT, 0) \
	X(UI_STR_SETTING_DUNGEON_ITEMS, 0) \
	X(UI_STR_SETTING_ACCESSIBILITY, 0) \
	X(UI_STR_SETTING_GOAL, 0) \
	X(UI_STR_SETTING_CRYSTALS_TOWER, 0) \
	X(UI_STR_SETTING_CRYSTALS_GANON, 0) \
	X(UI_STR_SETTING_STATE, 0) \
	X(UI_STR_SETTING_WEAPONS, 0) \
	X(UI_STR_SETTING_ITEM_POOL, 0) \
	X(UI_STR_SETTING_ITEM_FUNCTIONALITY, 0) \
	X(UI_STR_SETTING_HINTS, 0) \
	X(UI_STR_SETTING_HEART_BEEP, 0) \
	X(UI_STR_SETTING_SPOILER, 0) \
	X(UI_STR_GLITCHES_NONE, 0) \
	X(UI_STR_GLITCHES_OVERWORLD, 0) \
	X(UI_STR_GLITCHES_HYBRID_MAJOR, 0) \
	X(UI_STR_GLITCHES_MAJOR, 0) \
	X(UI_STR_GLITCHES_NO_LOGIC, 0) \
	X(UI_STR_PLACEMENT_ADVANCED, 0) \
	X(UI_STR_PLACEMENT_BASIC, 0) \
	X(UI_STR_DUNGEON_ITEMS_STANDARD, 0) \
	X(UI_STR_DUNGEON_ITEMS_MC, 0) \
	X(UI_STR_DUNGEON_ITEMS_MCS, 0) \
	X(UI_STR_DUNGEON_ITEMS_FULL, 0) \
	X(UI_STR_ACCESSIBILITY_ITEMS, 0) \
	X(UI_STR_ACCESSIBILITY_LOCATIONS, 0) \
	X(UI_STR_ACCESSIBILITY_NONE, 0) \
	X(UI_STR_GOAL_GANON, 0) \
	X(UI_STR_GOAL_FAST_GANON, 0) \
	X(UI_STR_GOAL_DUNGEONS, 0) \
	X(UI_STR_GOAL_PEDESTAL, 0) \
	X(UI_STR_GOAL_TRIFORCE_HUNT, 0) \
	X(UI_STR_GOAL_GANONHUNT, 0) \
	X(UI_STR_GOAL_COMPLETIONIST, 0) \
	X(UI_STR_CRYSTALS_TOWER_0, 0) \
	X(UI_STR_CRYSTALS_TOWER_1, 0) \
	X(UI_STR_CRYSTALS_TOWER_2, 0) \
	X(UI_STR_CRYSTALS_TOWER_3, 0) \
	X(UI_STR_CRYSTALS_TOWER_4, 0) \
	X(UI_STR_CRYSTALS_TOWER_5, 0) \
	X(UI_STR_CRYSTALS_TOWER_6, 0) \
	X(UI_STR_CRYSTALS_TOWER_7, 0) \
	X(UI_STR_CRYSTALS_TOWER_RANDOM, 0) \
	X(UI_STR_CRYSTALS_GANON_0, 0) \
	X(UI_STR_CRYSTALS_GANON_1, 0) \
	X(UI_STR_CRYSTALS_GANON_2, 0) \
	X(UI_STR_CRYSTALS_GANON_3, 0) \
	X(UI_STR_CRYSTALS_GANON_4, 0) \
	X(UI_STR_CRYSTALS_GANON_5, 0) \
	X(UI_STR_CRYSTALS_GANON_6, 0) \
	X(UI_STR_CRYSTALS_GANON_7, 0) \
	X(UI_STR_CRYSTALS_GANON_RANDOM, 0) \
	X(UI_STR_STATE_OPEN, 0) \
	X(UI_STR_STATE_STANDARD, 0) \
	X(UI_STR_STATE_INVERTED, 0) \
	X(UI_STR_STATE_RETRO, 0) \
	X(UI_STR_WEAPONS_RANDOMIZED, 0) \
	X(UI_STR_WEAPONS_ASSURED, 0) \
	X(UI_STR_WEAPONS_VANILLA, 0) \
	X(UI_STR_WEAPONS_SWORDLESS, 0) \
	X(UI_STR_ITEM_POOL_NORMAL, 0) \
	X(UI_STR_ITEM_POOL_HARD, 0) \
	X(UI_STR_ITEM_POOL_EXPERT, 0) \
	X(UI_STR_ITEM_POOL_CROWD_CONTROL, 0) \
	X(UI_STR_ITEM_FUNCTIONALITY_NORMAL, 0) \
	X(UI_STR_ITEM_FUNCTIONALITY_HARD, 0) \
	X(UI_STR_ITEM_FUNCTIONALITY_EXPERT, 0) \
	X(UI_STR_HINTS_ON, 0) \
	X(UI_STR_HINTS_OFF, 0) \
	X(UI_STR_HEART_BEEP_OFF, 0) \
	X(UI_STR_HEART_BEEP_HALF, 0) \
	X(UI_STR_HEART_BEEP_QUARTER, 0) \
	X(UI_STR_HEART_BEEP_DOUBLE, 0) \
	X(UI_STR_HEART_BEEP_NORMAL, 0) \
	X(UI_STR_SPOILER_OFF, 0) \
	X(UI_STR_SPOILER_ON, 0) \
	X(UI_STR_GENERATE_TITLE, 0) \
	X(UI_STR_GENERATE_ACTION, 0) \
	X(UI_STR_GENERATE_FOOTER, 0) \
	X(UI_STR_GENERATE_UNSUPPORTED, 1) \
	X(UI_STR_GENERATE_SETTINGS_UNREADABLE, 0)

#define UI_STR_ENUM_ENTRY(id, arity) id,
typedef enum UiStrId {
	UI_STR_LIST(UI_STR_ENUM_ENTRY)
	UI_STR_COUNT
} UiStrId;
#undef UI_STR_ENUM_ENTRY

/* The longest text, {} counting zero: a list row's field at the default grid. */
#define UI_STR_LEN_MAX (UI_GRID_DEFAULT_COLS - 2)

/* A NULL entry falls back to English. */
typedef struct UiStrTable {
	const char *text[UI_STR_COUNT];
} UiStrTable;

typedef enum UiStrFault {
	UI_STR_FAULT_NONE,
	UI_STR_FAULT_MISSING,
	UI_STR_FAULT_EMPTY,
	UI_STR_FAULT_BYTE,
	UI_STR_FAULT_ARITY,
	UI_STR_FAULT_BRACE,
	UI_STR_FAULT_TOO_LONG
} UiStrFault;

const UiStrTable *ui_str_english(void);

/* The table's text, English when its entry is NULL or table is NULL, "" for UI_STR_NONE (whatever the
 * table holds) or an id out of range. Never NULL. */
const char *ui_str(const UiStrTable *table, UiStrId id);

/* 0 or 1, the number of {} in the id's text; 0 out of range. */
int ui_str_arity(UiStrId id);

/* Copies pattern into buf with its {} replaced by arg (NULL = empty); NUL-terminated when cap >= 1. False
 * when the text was cut to cap - 1 characters, or cap is 0 (nothing written). */
bool ui_str_fill(const char *pattern, const char *arg, char *buf, size_t cap);

/* True when every non-NULL entry is sound and, if complete (the English table), no entry is NULL; else
 * false with the first faulty id and its fault. UI_STR_NONE, never shown, is not checked. */
bool ui_strings_check(const UiStrTable *table, bool complete, UiStrId *bad_id, UiStrFault *fault);

#endif
