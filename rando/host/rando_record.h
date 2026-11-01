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

/* Local time, no offset. */
typedef struct RandoRecordDate {
	int year, month, day, hour, minute, second;
} RandoRecordDate;

typedef struct RandoRecord {
	RandoName name;
	RandoRecordDate date;
	uint32_t seed;
	/* hints included. */
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool has_spoiler;
	/* Reader output only: the spoiler object's bytes are text[spoiler_offset, +spoiler_size). */
	size_t spoiler_offset, spoiler_size;
} RandoRecord;

/* Writes the history record JSON. world and playthrough are read only when has_spoiler (then neither may
 * be NULL); the spoiler's meta uses record->options. Asserts the date's ranges (year 1970..9999). False on
 * a stream error; the caller owns and closes out. */
bool rando_record_write(FILE *out, const RandoRecord *record, const RandoWorld *world,
			const RandoPlaythrough *playthrough);

#endif
