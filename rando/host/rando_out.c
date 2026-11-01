#include "rando_out.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>

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
