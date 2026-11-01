#ifndef RANDO_HISTORY_H
#define RANDO_HISTORY_H

#include "rando_record.h"
#include "rando_seed_save.h"
#include "ui_history.h"

#include <stdbool.h>

#define RANDO_HISTORY_PROFILES 3

/* Caller-owned and long-lived, about 64 KiB: a static object. */
typedef struct RandoHistory {
	char root[RANDO_SEED_DIR_MAX];
	char rom_dir[RANDO_SEED_DIR_MAX], history_dir[RANDO_SEED_DIR_MAX], game_config_dir[RANDO_SEED_DIR_MAX];
	/* Current, main, guest. */
	char profiles[RANDO_HISTORY_PROFILES][RANDO_SEED_DIR_MAX];
	char buf[RANDO_RECORD_TEXT_MAX + 1];
	RandoRecord record;
} RandoHistory;

/* Joins the paths under sd_root; false when any would reach RANDO_SEED_DIR_MAX. */
bool rando_history_init(RandoHistory *h, const char *sd_root);

/* Lists the strictly parsed records <code>.json of the history folder whose ROM is a regular file of the ROM
 * folder, newest first (date, then code, descending), at most UI_HISTORY_MAX. Never writes the card: an
 * unreadable record sets unreadable, a record without its ROM is hidden. A missing folder is an empty list. */
void rando_history_scan(RandoHistory *h, UiHistoryList *out);

/* A UiHistorySpoilerFn; ctx is the RandoHistory. Re-reads the entry's record, which must still parse with
 * the entry's ROM and hold a spoiler, and writes its outline. */
bool rando_history_spoiler(void *ctx, const UiHistoryEntry *entry, UiHistorySpoiler *out);

typedef enum RandoHistoryDeleteStatus {
	RANDO_HISTORY_DELETE_OK,
	RANDO_HISTORY_DELETE_ROM,    /* the ROM is still there: nothing else was touched */
	RANDO_HISTORY_DELETE_FILES,  /* a core override, save or state is still there: the record was kept */
	RANDO_HISTORY_DELETE_RECORD, /* the record is still there */
	RANDO_HISTORY_DELETE_STATUS_COUNT, /* never returned */
} RandoHistoryDeleteStatus;

/* The result line's id; asserts status is in range. */
UiStrId rando_history_delete_text(RandoHistoryDeleteStatus status);

/* A UiHistoryDeleteFn; ctx is the RandoHistory. Asserts a valid code and a ".sfc" ROM. Removes the ROM (stops
 * if it stays), its .game_config override, then in each profile folder present its Supafaust .srm and every
 * regular file of states/Supafaust starting with "<base>.state", then the record if all went, then the SNES
 * list caches whenever the ROM went. ok only when the record is gone. */
void rando_history_delete(void *ctx, const UiHistoryEntry *entry, UiHistoryDeleteOutcome *outcome);

#endif
