#ifndef RANDO_OUT_H
#define RANDO_OUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define RANDO_OUT_ERROR_MAX 1280
#define RANDO_OUT_PATCH_MAX (4u << 20)

typedef enum RandoOutRead {
	RANDO_OUT_READ_OK,
	RANDO_OUT_READ_MISSING, /* fopen failed with ENOENT */
	RANDO_OUT_READ_OPEN,
	RANDO_OUT_READ_READ,
	RANDO_OUT_READ_TOO_LARGE,
} RandoOutRead;

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

typedef struct RandoRomLog {
	FILE *file;
	bool failed;
} RandoRomLog;

/* Creates <out_dir>/rom_writes.txt. On failure returns false and writes the reason, path first,
 * into error. */
bool rando_rom_log_open(RandoRomLog *log, const char *out_dir, char error[RANDO_OUT_ERROR_MAX]);

/* A RandoRomWriteFn; ctx is the RandoRomLog. One `%06X <HEX> <method>` line per write, as the oracle's
 * rom_writes.txt. Refuses a zero-byte write; a write error refuses the write and is kept for
 * rando_rom_log_close. */
bool rando_rom_log_write(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method);

/* Closes the file; false when any write or the close failed. */
bool rando_rom_log_close(RandoRomLog *log, char error[RANDO_OUT_ERROR_MAX]);

/* Writes "<attempt>\n" into <out_dir>/attempts, as the oracle does on success. */
bool rando_attempts_write(const char *out_dir, uint32_t attempt, char error[RANDO_OUT_ERROR_MAX]);

/* A RandoAssembleOutputFn; ctx is the path (const char *). Writes the file with "wb" and removes it on any
 * failure after it was created. */
bool rando_out_write_file(void *ctx, const uint8_t *bytes, size_t size);

/* Reads at most max + 1 bytes of path into buf (max + 1 bytes long). On OK, *size <= max and *total
 * = *size; on TOO_LARGE, *size = max + 1 and *total the file's size, -1 when it cannot be told. On
 * MISSING and OPEN, errno is fopen's. Prints nothing. */
RandoOutRead rando_out_read_capped(const char *path, uint8_t *buf, size_t max, size_t *size, long *total);

/* From /dev/urandom, else a mix of the time and the clock. */
uint32_t rando_out_random_seed(void);

#endif
