#ifndef RANDO_RECORD_H
#define RANDO_RECORD_H

#include "rando_name.h"
#include "rando_options.h"
#include "rando_playthrough.h"
#include "rando_rom.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define RANDO_RECORD_FORMAT 1
#define RANDO_RECORD_TEXT_MAX (64 * 1024)
#define RANDO_RECORD_ERROR_MAX 256
#define RANDO_RECORD_OUTLINE_DEPTH 8

typedef struct RandoRecordDate {
	int year, month, day, hour, minute, second;
} RandoRecordDate;

typedef struct RandoRecord {
	RandoName name;
	RandoRecordDate date;
	uint32_t seed;
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool has_spoiler;
	size_t spoiler_offset, spoiler_size;
} RandoRecord;

bool rando_record_write(FILE *out, const RandoRecord *record, const RandoWorld *world,
			const RandoPlaythrough *playthrough);

bool rando_record_settings_write(FILE *out, const RandoOptions *options, RandoHeartBeep heart_beep, int depth);

bool rando_record_settings_parse(const char *text, size_t len, RandoOptions *options, RandoHeartBeep *heart_beep,
				 char error[RANDO_RECORD_ERROR_MAX]);

bool rando_record_parse(const char *text, size_t len, RandoRecord *out, char error[RANDO_RECORD_ERROR_MAX]);

bool rando_record_spoiler_outline(const char *text, size_t offset, size_t size, char *out, size_t cap,
				  size_t *len, bool *cut);

#endif
