#ifndef LADX_WLOG_TOKEN_H
#define LADX_WLOG_TOKEN_H

#include "ladx_wlog.h"

#include <stddef.h>
#include <stdint.h>

typedef enum LadxWlogTokenKind {
	LADX_WLOG_TOKEN_KEEP,
	LADX_WLOG_TOKEN_COPY,
	LADX_WLOG_TOKEN_LITERAL,
	LADX_WLOG_TOKEN_POINTER
} LadxWlogTokenKind;

typedef struct LadxWlogToken {
	LadxWlogTokenKind kind;
	uint32_t q, n;
	const char *hex;
} LadxWlogToken;

int ladx_wlog_hex(LadxSpan s, uint32_t *v);

int ladx_wlog_token(LadxSpan tok, LadxWlogToken *t);

uint8_t ladx_wlog_token_byte(const LadxWlogToken *t, uint32_t i);

#endif
