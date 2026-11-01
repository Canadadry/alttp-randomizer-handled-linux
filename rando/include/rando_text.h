#ifndef RANDO_TEXT_H
#define RANDO_TEXT_H

#include "rando_text_ids.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* VT's Dialog::convertDialogCompressed default max_bytes. */
#define RANDO_TEXT_ENTRY_MAX 2046
/* VT's Text::getByteArray bound on the whole table. */
#define RANDO_TEXT_TABLE_MAX 0x7355

/* VT's Dialog::convertDialogCompressed(utf8, pause) with its default max_bytes and wrap (19). utf8
 * must be valid UTF-8 (asserted). Writes the bytes, cut to RANDO_TEXT_ENTRY_MAX, into out and
 * returns their count, never 0. */
size_t rando_text_encode(const char *utf8, bool pause, uint8_t out[RANDO_TEXT_ENTRY_MAX]);

/* VT's text id; asserts id < RANDO_TEXT_COUNT. */
const char *rando_text_name(RandoTextId id);
/* RANDO_TEXT_NONE when unknown. */
RandoTextId rando_text_find(const char *name);

/* The id's bytes as VT's Rom constructor leaves them (Text::en(), then removeUnwanted()), into out;
 * returns their count, never 0. Asserts id < RANDO_TEXT_COUNT. */
size_t rando_text_default(RandoTextId id, uint8_t out[RANDO_TEXT_ENTRY_MAX]);

#endif
