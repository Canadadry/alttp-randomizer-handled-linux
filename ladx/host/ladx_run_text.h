#ifndef LADX_RUN_TEXT_H
#define LADX_RUN_TEXT_H

#include "ladx_itempool.h"
#include "ladx_randomizer.h"
#include "ladx_rng.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Room for any float.hex text and its NUL. */
#define LADX_FLOAT_HEX_SIZE 32
/* Room for any trace line and its NUL. */
#define LADX_RUN_TEXT_MAX_LINE 8192

/* CPython's float.hex(x): 13 hex digits, 0x0.0p+0 for zero, sign kept, inf and nan as Python. */
void ladx_float_hex(double x, char out[LADX_FLOAT_HEX_SIZE]);

/*
 * The run.txt line `trace <n> <gen> <stage> <method> <args> -> <result>
 * [<words>]`, no newline. Returns its length; 0, with buf empty when cap > 0,
 * when it does not fit cap or the record is not one ladx_rng emits.
 */
size_t ladx_run_text_trace_line(char *buf, size_t cap, unsigned long n, const LadxRngRecord *r);

/* `seed <hex>`, `-` for an empty seed; same return as ladx_run_text_trace_line. */
size_t ladx_run_text_seed_line(char *buf, size_t cap, const uint8_t *seed, size_t size);

/* `settings <short string>`, `""` when empty; same return as ladx_run_text_trace_line. */
size_t ladx_run_text_settings_line(char *buf, size_t cap, const char *short_string);

/*
 * `outcome ok | error <message> | crash <type> <message>`, `""` for an empty
 * message; same return as ladx_run_text_trace_line, 0 for an UNFINISHED run.
 */
size_t ladx_run_text_outcome_line(char *buf, size_t cap, const LadxOutcome *outcome);

/* The ws lines of run.txt, one per WorldSetup attribute. */
#define LADX_RUN_TEXT_WS_LINES 13

/*
 * Line i of the world setup, `ws <field> <value>`, in WorldSetup's attribute
 * order (ladx_oracle.ws_lines); same return as ladx_run_text_trace_line, 0
 * for i outside 0 to LADX_RUN_TEXT_WS_LINES - 1.
 */
size_t ladx_run_text_ws_line(char *buf, size_t cap, const LadxWorldSetup *ws, int i);

/* `<tag> <n>` (words_before, words_after); same return as ladx_run_text_trace_line. */
size_t ladx_run_text_words_line(char *buf, size_t cap, const char *tag, uint64_t words);

/*
 * `<tag> <item>=<count> ...` in the pool's order, zero and negative counts
 * kept (pool, placer_pool); same return as ladx_run_text_trace_line.
 */
size_t ladx_run_text_pool_line(char *buf, size_t cap, const char *tag, const LadxItemPool *pool);

/*
 * `place <i> <class> <nameId> <item or ->` for spot i of the logic
 * (ladx_oracle.place_lines); same return as ladx_run_text_trace_line, 0 for
 * a spot out of range.
 */
size_t ladx_run_text_place_line(char *buf, size_t cap, const LadxLogic *logic, int i);

/* A trace sink numbering the records from 0 and writing each line to out. */
typedef struct LadxRunTextTrace {
	FILE *out;
	unsigned long count;
	int failed; /* a line that did not render or write; later records are dropped */
} LadxRunTextTrace;

void ladx_run_text_trace_init(LadxRunTextTrace *t, FILE *out);

/* A LadxRngSink; ctx is a LadxRunTextTrace. */
void ladx_run_text_trace_sink(void *ctx, const LadxRngRecord *record);

#endif
