#include "wlog_token.h"

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

int ladx_wlog_hex(LadxSpan s, uint32_t *v)
{
	size_t i;

	if (s.len == 0 || s.len > 8 || (s.len > 1 && s.at[0] == '0'))
		return 0;
	*v = 0;
	for (i = 0; i < s.len; i++) {
		int d = hex_digit(s.at[i]);

		if (d < 0)
			return 0;
		*v = *v << 4 | (uint32_t)d;
	}
	return 1;
}

static LadxSpan rest_of(LadxSpan s, size_t from, size_t to)
{
	LadxSpan r;

	r.at = s.at + from;
	r.len = to - from;
	return r;
}

int ladx_wlog_token(LadxSpan tok, LadxWlogToken *t)
{
	size_t i;

	if (tok.len == 0)
		return 0;
	t->q = t->n = 0;
	t->hex = NULL;
	if (tok.at[0] == '=') {
		t->kind = LADX_WLOG_TOKEN_KEEP;
		return ladx_wlog_hex(rest_of(tok, 1, tok.len), &t->n) && t->n > 0;
	}
	if (tok.at[0] == 'p') {
		t->kind = LADX_WLOG_TOKEN_POINTER;
		return ladx_wlog_hex(rest_of(tok, 1, tok.len), &t->q);
	}
	if (tok.at[0] == '@') {
		size_t plus = 1;

		while (plus < tok.len && tok.at[plus] != '+')
			plus++;
		t->kind = LADX_WLOG_TOKEN_COPY;
		return plus < tok.len && ladx_wlog_hex(rest_of(tok, 1, plus), &t->q) &&
		       ladx_wlog_hex(rest_of(tok, plus + 1, tok.len), &t->n) && t->n > 0 &&
		       (uint64_t)t->q + t->n <= 0x80000000u;
	}
	if (tok.len % 2)
		return 0;
	for (i = 0; i < tok.len; i++)
		if (hex_digit(tok.at[i]) < 0)
			return 0;
	t->kind = LADX_WLOG_TOKEN_LITERAL;
	t->n = (uint32_t)(tok.len / 2);
	t->hex = tok.at;
	return 1;
}

uint8_t ladx_wlog_token_byte(const LadxWlogToken *t, uint32_t i)
{
	return (uint8_t)(hex_digit(t->hex[2 * i]) << 4 | hex_digit(t->hex[2 * i + 1]));
}
