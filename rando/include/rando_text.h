#ifndef RANDO_TEXT_H
#define RANDO_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* VT's Dialog::convertDialogCompressed default max_bytes. */
#define RANDO_TEXT_ENTRY_MAX 2046

/* VT's Dialog::convertDialogCompressed(utf8, pause) with its default max_bytes and wrap (19). utf8
 * must be valid UTF-8 (asserted). Writes the bytes, cut to RANDO_TEXT_ENTRY_MAX, into out and
 * returns their count, never 0. */
size_t rando_text_encode(const char *utf8, bool pause, uint8_t out[RANDO_TEXT_ENTRY_MAX]);

#endif
