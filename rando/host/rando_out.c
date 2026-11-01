#include "rando_out.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

#define OUT_PATH_MAX 1024

static bool out_path(char path[OUT_PATH_MAX], const char *out_dir, const char *name,
		     char error[RANDO_OUT_ERROR_MAX])
{
	int n = snprintf(path, OUT_PATH_MAX, "%s/%s", out_dir, name);

	if (n < 0 || n >= OUT_PATH_MAX) {
		snprintf(error, RANDO_OUT_ERROR_MAX, "%s/%s: path too long", out_dir, name);
		return false;
	}
	return true;
}

static bool io_fail(char error[RANDO_OUT_ERROR_MAX], const char *path, int err)
{
	snprintf(error, RANDO_OUT_ERROR_MAX, "%s: %s", path, strerror(err));
	return false;
}

bool rando_trace_open(RandoTraceFile *trace, const char *out_dir, char error[RANDO_OUT_ERROR_MAX])
{
	char path[OUT_PATH_MAX];

	trace->file = NULL;
	trace->failed = false;
	if (!out_path(path, out_dir, "trace.txt", error))
		return false;
	trace->file = fopen(path, "w");
	if (!trace->file)
		return io_fail(error, path, errno);
	if (setvbuf(trace->file, NULL, _IOLBF, BUFSIZ) != 0) {
		fclose(trace->file);
		trace->file = NULL;
		snprintf(error, RANDO_OUT_ERROR_MAX, "%s: cannot line-buffer", path);
		return false;
	}
	return true;
}

void rando_trace_write(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min, int64_t max,
		       int64_t r)
{
	RandoTraceFile *trace = ctx;

	if (fprintf(trace->file, "%" PRIu32 " %" PRIu32 " %s %" PRId64 " %" PRId64 " -> %" PRId64 "\n", attempt,
		    draw, stage, min, max, r) < 0)
		trace->failed = true;
}

bool rando_trace_close(RandoTraceFile *trace, char error[RANDO_OUT_ERROR_MAX])
{
	bool ok = !trace->failed;

	if (fclose(trace->file) != 0)
		ok = false;
	trace->file = NULL;
	if (!ok)
		snprintf(error, RANDO_OUT_ERROR_MAX, "trace.txt: write failed");
	return ok;
}

bool rando_rom_log_open(RandoRomLog *log, const char *out_dir, char error[RANDO_OUT_ERROR_MAX])
{
	char path[OUT_PATH_MAX];

	log->file = NULL;
	log->failed = false;
	if (!out_path(path, out_dir, "rom_writes.txt", error))
		return false;
	log->file = fopen(path, "w");
	if (!log->file)
		return io_fail(error, path, errno);
	return true;
}

bool rando_rom_log_write(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	RandoRomLog *log = ctx;
	size_t i;

	if (count == 0 || log->failed) {
		log->failed = true;
		return false;
	}
	if (fprintf(log->file, "%06" PRIX32 " ", address) < 0)
		log->failed = true;
	for (i = 0; i < count && !log->failed; i++)
		if (fprintf(log->file, "%02X", (unsigned)bytes[i]) < 0)
			log->failed = true;
	if (!log->failed && fprintf(log->file, " %s\n", method) < 0)
		log->failed = true;
	return !log->failed;
}

bool rando_rom_log_close(RandoRomLog *log, char error[RANDO_OUT_ERROR_MAX])
{
	bool ok = !log->failed;

	if (fclose(log->file) != 0)
		ok = false;
	log->file = NULL;
	if (!ok)
		snprintf(error, RANDO_OUT_ERROR_MAX, "rom_writes.txt: write failed");
	return ok;
}

bool rando_attempts_write(const char *out_dir, uint32_t attempt, char error[RANDO_OUT_ERROR_MAX])
{
	char path[OUT_PATH_MAX];
	FILE *f;
	int written;

	if (!out_path(path, out_dir, "attempts", error))
		return false;
	f = fopen(path, "w");
	if (!f)
		return io_fail(error, path, errno);
	written = fprintf(f, "%" PRIu32 "\n", attempt);
	if (fclose(f) != 0 || written < 0)
		return io_fail(error, path, errno);
	return true;
}

bool rando_out_write_file(void *ctx, const uint8_t *bytes, size_t size)
{
	const char *path = ctx;
	FILE *f = fopen(path, "wb");
	bool ok;
	if (f == NULL)
		return false;
	ok = fwrite(bytes, 1, size, f) == size;
	if (fclose(f) != 0)
		ok = false;
	if (!ok)
		remove(path);
	return ok;
}

RandoOutRead rando_out_read_capped(const char *path, uint8_t *buf, size_t max, size_t *size, long *total)
{
	FILE *f = fopen(path, "rb");
	bool failed;
	if (f == NULL)
		return errno == ENOENT ? RANDO_OUT_READ_MISSING : RANDO_OUT_READ_OPEN;
	*size = fread(buf, 1, max + 1, f);
	failed = ferror(f) != 0;
	*total = (long)*size;
	if (!failed && *size > max)
		*total = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
	fclose(f);
	if (failed)
		return RANDO_OUT_READ_READ;
	return *size > max ? RANDO_OUT_READ_TOO_LARGE : RANDO_OUT_READ_OK;
}

uint32_t rando_out_random_seed(void)
{
	uint8_t bytes[4];
	FILE *f = fopen("/dev/urandom", "rb");
	uint32_t seed;
	if (f != NULL) {
		size_t n = fread(bytes, 1, sizeof bytes, f);
		fclose(f);
		if (n == sizeof bytes)
			return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
			       (uint32_t)bytes[3] << 24;
	}
	seed = (uint32_t)time(NULL) * 2654435761u ^ (uint32_t)clock();
	seed ^= seed >> 16;
	seed *= 0x85EBCA6Bu;
	seed ^= seed >> 13;
	return seed;
}
