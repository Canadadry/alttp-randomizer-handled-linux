#ifndef RANDO_TEXT_TABLE_H
#define RANDO_TEXT_TABLE_H

#include "rando_text.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct TextDefault {
	const char *text;
	bool pause;
	const uint8_t *raw;
	size_t raw_count;
} TextDefault;

extern const char *const rando_text_names[RANDO_TEXT_COUNT];
extern const TextDefault rando_text_defaults[RANDO_TEXT_COUNT];

#endif
