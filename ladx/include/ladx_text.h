#ifndef LADX_TEXT_H
#define LADX_TEXT_H

#include "ladx_outcome.h"

#include <stddef.h>
#include <stdint.h>

#define LADX_TEXT_MAX 2048

typedef enum LadxTextStatus {
	LADX_TEXT_OK = 0,
	LADX_TEXT_RAISED,
	LADX_TEXT_OVERFLOW,
	LADX_TEXT_UNSUPPORTED
} LadxTextStatus;

LadxTextStatus ladx_format_text(const char *in, size_t n, int center, const char *ask, uint8_t *out, size_t cap,
				size_t *out_len, LadxOutcome *raised);

#endif
