#ifndef LADX_RUN_TEXT_H
#define LADX_RUN_TEXT_H

#include "ladx_itempool.h"
#include "ladx_randomizer.h"
#include "ladx_rng.h"
#include "ladx_settings.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LADX_FLOAT_HEX_SIZE 32
#define LADX_RUN_TEXT_MAX_LINE 8192

void ladx_float_hex(double x, char out[LADX_FLOAT_HEX_SIZE]);

size_t ladx_run_text_trace_line(char *buf, size_t cap, unsigned long n, const LadxRngRecord *r);

size_t ladx_run_text_seed_line(char *buf, size_t cap, const uint8_t *seed, size_t size);

size_t ladx_run_text_settings_line(char *buf, size_t cap, const char *short_string);

size_t ladx_run_text_outcome_line(char *buf, size_t cap, const LadxOutcome *outcome);

#define LADX_RUN_TEXT_WS_LINES 13

size_t ladx_run_text_ws_line(char *buf, size_t cap, const LadxWorldSetup *ws, int i);

size_t ladx_run_text_words_line(char *buf, size_t cap, const char *tag, uint64_t words);

size_t ladx_run_text_pool_line(char *buf, size_t cap, const char *tag, const LadxItemPool *pool);

size_t ladx_run_text_place_line(char *buf, size_t cap, const LadxLogic *logic, int i);

typedef struct LadxRunTextTrace {
	FILE *out;
	unsigned long count;
	int failed;
} LadxRunTextTrace;

void ladx_run_text_trace_init(LadxRunTextTrace *t, FILE *out);

void ladx_run_text_trace_sink(void *ctx, const LadxRngRecord *record);

int ladx_run_text_write_run(FILE *out, const LadxSettings *settings, const uint8_t *seed, size_t seed_len,
			    const LadxRandomizer *r, FILE *trace, const LadxOutcome *raised);

#endif
