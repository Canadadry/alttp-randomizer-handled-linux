#include "rando.h"
#include "rando_case.h"
#include "rando_out.h"
#include "rando_playthrough.h"
#include "rando_spoiler.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
	EXIT_COMPLETE = 0,
	EXIT_INPUT = 1,
	EXIT_USAGE = 2,
	EXIT_INCOMPLETE = 3,
	EXIT_UNSUPPORTED = 4,
	EXIT_GAVE_UP = 5,
};

#define ORACLE_WATCHDOG 1000u

static RandoGen gen;
static RandoPlaythrough playthrough;
static bool complete;

typedef bool (*Writer)(FILE *out);

static bool spoiler_json(FILE *out)
{
	return rando_spoiler_write_json(out, &gen.world, &gen.options, complete ? &playthrough : NULL);
}

static bool pool_txt(FILE *out)
{
	return rando_spoiler_write_pool(out, &gen.world, &gen.pools);
}

static bool write_file(const char *out_dir, const char *name, Writer writer)
{
	char path[RANDO_OUT_ERROR_MAX];
	FILE *f;
	bool ok;

	snprintf(path, sizeof path, "%s/%s", out_dir, name);
	f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "rando_gen: %s: %s\n", path, strerror(errno));
		return false;
	}
	ok = writer(f);
	if (fclose(f) != 0)
		ok = false;
	if (!ok)
		fprintf(stderr, "rando_gen: %s: write failed\n", path);
	return ok;
}

/* RANDO_STAGE_COUNT when name is no stage. */
static RandoStage stage_named(const char *name)
{
	RandoStage stage;

	for (stage = 0; stage < RANDO_STAGE_COUNT; stage++)
		if (strcmp(rando_stage_name(stage), name) == 0)
			break;
	return stage;
}

int main(int argc, char **argv)
{
	RandoCase c;
	RandoTraceFile trace;
	RandoStatus status;
	char case_error[RANDO_CASE_ERROR_MAX], out_error[RANDO_OUT_ERROR_MAX];
	uint32_t cap;
	RandoStage last = RANDO_STAGE_NONE;

	if (argc == 4)
		last = stage_named(argv[3]);
	if ((argc != 3 && argc != 4) || (argc == 4 && last == RANDO_STAGE_COUNT)) {
		fprintf(stderr, "usage: rando_gen <case_dir> <out_dir> [<stage>]\n");
		return EXIT_USAGE;
	}
	if (!rando_case_read(argv[1], &c, case_error)) {
		fprintf(stderr, "rando_gen: %s\n", case_error);
		return EXIT_INPUT;
	}
	/* The golden's attempts file holds the 0-based index of its winning attempt. */
	cap = !c.has_attempts ? ORACLE_WATCHDOG : c.attempts == UINT32_MAX ? 0 : c.attempts + 1;
	if (!rando_trace_open(&trace, argv[2], out_error)) {
		fprintf(stderr, "rando_gen: %s\n", out_error);
		return EXIT_INPUT;
	}
	status = rando_generate_until(&gen, &c.options, c.seed, cap, rando_trace_write, &trace, last);
	if (!rando_trace_close(&trace, out_error)) {
		fprintf(stderr, "rando_gen: %s\n", out_error);
		return EXIT_INPUT;
	}
	complete = status == RANDO_COMPLETE;
	if (complete)
		rando_playthrough(&gen.world, &playthrough);
	if ((status == RANDO_COMPLETE || status == RANDO_INCOMPLETE)
	    && !write_file(argv[2], "spoiler.json", spoiler_json))
		return EXIT_INPUT;
	if (status == RANDO_INCOMPLETE && !write_file(argv[2], "pool.txt", pool_txt))
		return EXIT_INPUT;
	switch (status) {
	case RANDO_COMPLETE:
		if (!rando_attempts_write(argv[2], gen.attempt, out_error)) {
			fprintf(stderr, "rando_gen: %s\n", out_error);
			return EXIT_INPUT;
		}
		return EXIT_COMPLETE;
	case RANDO_INCOMPLETE:
		return EXIT_INCOMPLETE;
	case RANDO_UNSUPPORTED:
		return EXIT_UNSUPPORTED;
	case RANDO_GAVE_UP:
		fprintf(stderr, "rando_gen: gave up after %u attempts\n", (unsigned)cap);
		return EXIT_GAVE_UP;
	}
	return EXIT_INPUT;
}
