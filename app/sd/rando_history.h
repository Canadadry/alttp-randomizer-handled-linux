#ifndef RANDO_HISTORY_H
#define RANDO_HISTORY_H

#include "rando_record.h"
#include "rando_seed_files.h"
#include "rando_seed_save.h"
#include "ui_history.h"

#include <stdbool.h>

#define RANDO_HISTORY_PROFILES 3

typedef struct RandoHistory {
	char root[RANDO_SEED_DIR_MAX];
	char rom_dir[RANDO_SEED_DIR_MAX], history_dir[RANDO_SEED_DIR_MAX], game_config_dir[RANDO_SEED_DIR_MAX];
	char profiles[RANDO_HISTORY_PROFILES][RANDO_SEED_DIR_MAX];
	char buf[RANDO_RECORD_TEXT_MAX + 1];
	RandoRecord record;
} RandoHistory;

bool rando_history_init(RandoHistory *h, const char *sd_root);

void rando_history_scan(RandoHistory *h, UiHistoryList *out);

bool rando_history_spoiler(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out);

UiStrId rando_history_delete_text(RandoHistoryDeleteStatus status);

void rando_history_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome);

#endif
