#ifndef RANDO_LADX_HISTORY_H
#define RANDO_LADX_HISTORY_H

#include "rando_history.h"
#include "rando_ladx_record.h"
#include "rando_seed_save.h"
#include "ui_history.h"

#include <stdbool.h>

#define RANDO_LADX_SPOILER_MAX (256 * 1024)

typedef struct RandoLadxHistoryList {
	RandoLadxRecord entries[UI_HISTORY_MAX];
	int count;
	bool unreadable, truncated, folder_unreadable;
} RandoLadxHistoryList;

typedef struct RandoLadxHistory {
	char root[RANDO_SEED_DIR_MAX];
	char rom_dir[RANDO_SEED_DIR_MAX], history_dir[RANDO_SEED_DIR_MAX], game_config_dir[RANDO_SEED_DIR_MAX];
	char profiles[RANDO_HISTORY_PROFILES][RANDO_SEED_DIR_MAX];
	char buf[RANDO_LADX_SPOILER_MAX + 1];
	RandoLadxRecord record;
} RandoLadxHistory;

bool rando_ladx_history_init(RandoLadxHistory *h, const char *sd_root);

void rando_ladx_history_scan(RandoLadxHistory *h, RandoLadxHistoryList *out);

bool rando_ladx_history_spoiler(RandoLadxHistory *h, const RandoName *name, UiHistorySpoiler *out);

void rando_ladx_history_delete(RandoLadxHistory *h, const RandoName *name, UiHistoryDeleteOutcome *outcome);

#endif
