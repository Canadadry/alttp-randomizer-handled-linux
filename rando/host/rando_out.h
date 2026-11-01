#ifndef RANDO_OUT_H
#define RANDO_OUT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define RANDO_OUT_ERROR_MAX 1280

typedef struct RandoTraceFile {
	FILE *file;
	bool failed;
} RandoTraceFile;

/* Creates <out_dir>/trace.txt, line-buffered so every line written survives a later abort. On
 * failure returns false and writes the reason, path first, into error. */
bool rando_trace_open(RandoTraceFile *trace, const char *out_dir, char error[RANDO_OUT_ERROR_MAX]);

/* A RandoTraceFn; ctx is the RandoTraceFile. A write error is kept for rando_trace_close. */
void rando_trace_write(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min, int64_t max,
		       int64_t r);

/* Closes the file; false when any write or the close failed. */
bool rando_trace_close(RandoTraceFile *trace, char error[RANDO_OUT_ERROR_MAX]);

/* Writes "<attempt>\n" into <out_dir>/attempts, as the oracle does on success. */
bool rando_attempts_write(const char *out_dir, uint32_t attempt, char error[RANDO_OUT_ERROR_MAX]);

#endif
