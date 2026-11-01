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

/* The record's settings object: "{", the thirteen settings in bytewise key order, each indented
 * depth + 1 levels of four spaces, then "}" indented depth levels, with no newline after it. False on a
 * stream error. */
bool rando_record_settings_write(FILE *out, const RandoOptions *options, RandoHeartBeep heart_beep, int depth);

/* The whole text is one settings object, whitespace around it allowed, under the record's rules: every
 * setting once, known values, nothing else. On failure returns false with a reason in error; the outputs
 * are then unspecified. */
bool rando_record_settings_parse(const char *text, size_t len, RandoOptions *options, RandoHeartBeep *heart_beep,
				 char error[RANDO_RECORD_ERROR_MAX]);

/* Parses a whole record, strictly: keys in any order, each once, spoiler optional, rom equal to the name the
 * settings and code make. The spoiler span points into text, which the caller keeps. On failure returns
 * false and writes into error a reason naming the offending key or value; out is unspecified. */
bool rando_record_parse(const char *text, size_t len, RandoRecord *out, char error[RANDO_RECORD_ERROR_MAX]);

/* Writes the spoiler span text[offset, +size) as an indented outline into out[0, *len), not NUL-terminated:
 * "key: value" lines, containers as their key alone, VT's ":<digits>" suffix dropped, the top-level meta
 * skipped. On overflow keeps whole lines only and sets *cut. False on a malformed span or one nested deeper
 * than RANDO_RECORD_OUTLINE_DEPTH (the root counts 1); *len and *cut are then unspecified. */
bool rando_record_spoiler_outline(const char *text, size_t offset, size_t size, char *out, size_t cap,
				  size_t *len, bool *cut);

#endif
