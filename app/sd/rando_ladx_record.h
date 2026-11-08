#ifndef RANDO_LADX_RECORD_H
#define RANDO_LADX_RECORD_H

#include "ladx_settings.h"
#include "rando_ladx_name.h"
#include "rando_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define RANDO_LADX_RECORD_FORMAT 1

/* A LADX seed's History record: App/ZeldaRando/ladx/history/<code>.json. */
typedef struct RandoLadxRecord {
	RandoName name;
	RandoRecordDate date;
	char seed[RANDO_LADX_SEED_HEX + 1]; /* lower-case hex */
	LadxSettings settings;		    /* as generated: validated */
	bool has_spoiler;		    /* <code>.spoiler.json kept beside the record */
} RandoLadxRecord;

/* The record's JSON: code, date, format, rom, seed, settings (every setting, table order), spoiler. */
bool rando_ladx_record_write(FILE *out, const RandoLadxRecord *record);

/*
 * Every key exactly once, nothing else; the code valid, the rom the one
 * the code and settings make. False with a one-line reason in error.
 */
bool rando_ladx_record_parse(const char *text, size_t len, RandoLadxRecord *out,
			     char error[RANDO_RECORD_ERROR_MAX]);

/* Every setting's key and value name in table order, one JSON object. */
bool rando_ladx_settings_write(FILE *out, const LadxSettings *settings);

/* That object, every key exactly once and nothing else; out untouched on failure. */
bool rando_ladx_settings_parse(const char *text, size_t len, LadxSettings *out, char error[RANDO_RECORD_ERROR_MAX]);

#endif
