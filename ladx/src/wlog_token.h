#ifndef LADX_WLOG_TOKEN_H
#define LADX_WLOG_TOKEN_H

#include "ladx_wlog.h"

#include <stddef.h>
#include <stdint.h>

/*
 * One write-log v1 token decoded alone (tests/ladx/README.md,
 * "rom_writes.txt, version 1"), shared by the write-log parser and the
 * patch tables' reader. Where a token may stand, and whether it continues
 * the one before, is the caller's rule.
 */

typedef enum LadxWlogTokenKind {
	LADX_WLOG_TOKEN_KEEP,	 /* =<n>: n bytes at the record's own offsets */
	LADX_WLOG_TOKEN_COPY,	 /* @<q>+<n> */
	LADX_WLOG_TOKEN_LITERAL, /* n bytes, hex holding their 2n digits */
	LADX_WLOG_TOKEN_POINTER	 /* p<q> */
} LadxWlogTokenKind;

typedef struct LadxWlogToken {
	LadxWlogTokenKind kind;
	uint32_t q, n;
	const char *hex;
} LadxWlogToken;

/* Canonical lowercase hex of at most 32 bits: no leading zero, "0" alone allowed. */
int ladx_wlog_hex(LadxSpan s, uint32_t *v);

/*
 * 0 when tok is no token: an empty one, a length 0, a hex field not
 * canonical, a copy reaching past offset 0x7fffffff, an odd or uppercase
 * literal.
 */
int ladx_wlog_token(LadxSpan tok, LadxWlogToken *t);

/* The i-th byte of a literal token. */
uint8_t ladx_wlog_token_byte(const LadxWlogToken *t, uint32_t i);

#endif
