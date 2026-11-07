#ifndef LADX_TEXT_H
#define LADX_TEXT_H

#include "ladx_outcome.h"

#include <stddef.h>
#include <stdint.h>

/* utils.formatText (tests/ladx/README.md, "Pipeline"). */

/* The longest text, after its {NAME} fields, formatText takes. */
#define LADX_TEXT_MAX 2048

typedef enum LadxTextStatus {
	LADX_TEXT_OK = 0,
	LADX_TEXT_RAISED,      /* LADXR raises: *raised filled, a CRASH */
	LADX_TEXT_OVERFLOW,    /* over cap or LADX_TEXT_MAX */
	LADX_TEXT_UNSUPPORTED  /* a str.format construct LADXR's texts never use */
} LadxTextStatus;

/*
 * formatText(in, center=center, ask=ask) with LADXR's _NAMES and
 * _CHARACTERS: in's n bytes stand for a Python str, one character each
 * (Latin-1). OK: the bytes in out, their count in *out_len. RAISED:
 * UnicodeEncodeError (a character over 0x7F) or KeyError (a name _NAMES
 * lacks), with CPython's message in raised->text. ask NULL: no ask.
 */
LadxTextStatus ladx_format_text(const char *in, size_t n, int center, const char *ask, uint8_t *out, size_t cap,
				size_t *out_len, LadxOutcome *raised);

#endif
