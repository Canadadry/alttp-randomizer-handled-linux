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
	RANDO_OUT_READ_MISSING,
	RANDO_OUT_READ_OPEN,
	RANDO_OUT_READ_READ,
	RANDO_OUT_READ_TOO_LARGE,
} RandoOutRead;

typedef struct RandoTraceFile {
	FILE *file;
	bool failed;
} RandoTraceFile;

bool rando_trace_open(RandoTraceFile *trace, const char *out_dir, char error[RANDO_OUT_ERROR_MAX]);

void rando_trace_write(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min, int64_t max,
		       int64_t r);

bool rando_trace_close(RandoTraceFile *trace, char error[RANDO_OUT_ERROR_MAX]);

typedef struct RandoRomLog {
	FILE *file;
	bool failed;
} RandoRomLog;

bool rando_rom_log_open(RandoRomLog *log, const char *out_dir, char error[RANDO_OUT_ERROR_MAX]);

bool rando_rom_log_write(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method);

bool rando_rom_log_close(RandoRomLog *log, char error[RANDO_OUT_ERROR_MAX]);

bool rando_attempts_write(const char *out_dir, uint32_t attempt, char error[RANDO_OUT_ERROR_MAX]);

bool rando_out_write_file(void *ctx, const uint8_t *bytes, size_t size);

RandoOutRead rando_out_read_capped(const char *path, uint8_t *buf, size_t max, size_t *size, long *total);

uint32_t rando_out_random_seed(void);

#endif
