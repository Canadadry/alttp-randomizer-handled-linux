#ifndef RANDO_LADX_RECORD_H
#define RANDO_LADX_RECORD_H

#include "ladx_settings.h"
#include "rando_ladx_name.h"
#include "rando_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define RANDO_LADX_RECORD_FORMAT 1

typedef struct RandoLadxRecord {
	RandoName name;
	RandoRecordDate date;
	char seed[RANDO_LADX_SEED_HEX + 1];
	LadxSettings settings;
	bool has_spoiler;
} RandoLadxRecord;

bool rando_ladx_record_write(FILE *out, const RandoLadxRecord *record);

bool rando_ladx_record_parse(const char *text, size_t len, RandoLadxRecord *out,
			     char error[RANDO_RECORD_ERROR_MAX]);

bool rando_ladx_settings_write(FILE *out, const LadxSettings *settings);

bool rando_ladx_settings_parse(const char *text, size_t len, LadxSettings *out, char error[RANDO_RECORD_ERROR_MAX]);

#endif
