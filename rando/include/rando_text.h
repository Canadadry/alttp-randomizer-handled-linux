#ifndef RANDO_TEXT_H
#define RANDO_TEXT_H

#include "rando.h"
#include "rando_text_ids.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RANDO_TEXT_ENTRY_MAX 2046
#define RANDO_TEXT_TABLE_MAX 0x7355

size_t rando_text_encode(const char *utf8, bool pause, uint8_t out[RANDO_TEXT_ENTRY_MAX]);

const char *rando_text_name(RandoTextId id);
RandoTextId rando_text_find(const char *name);

size_t rando_text_default(RandoTextId id, uint8_t out[RANDO_TEXT_ENTRY_MAX]);

size_t rando_text_seed(const RandoGen *gen, RandoTextId id, uint8_t out[RANDO_TEXT_ENTRY_MAX], bool *seed_set);

#endif
