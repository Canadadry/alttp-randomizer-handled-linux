#define _POSIX_C_SOURCE 199309L

#include "ladx_randomizer.h"
#include "ladx_run_text.h"
#include "ladx_settings.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define SEED_BYTES 16
#define MAX_PATH 4096
#define MAX_CONFIG_LINE 256

static const char usage[] = "usage: ladx_gen <case_dir> <out_dir>\n"
			    "       ladx_gen --time <case_dir>\n";

typedef struct Case {
	LadxSettings settings;
	uint8_t seed[SEED_BYTES];
} Case;

static int join(char *out, const char *dir, const char *name)
{
	int n = snprintf(out, MAX_PATH, "%s/%s", dir, name);

	if (n < 0 || n >= MAX_PATH) {
		fprintf(stderr, "ladx_gen: path too long: %s/%s\n", dir, name);
		return 0;
	}
	return 1;
}

static int hex_digit(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

/* 32 lowercase hex digits, one optional newline. */
static int read_seed(const char *case_dir, uint8_t seed[SEED_BYTES])
{
	char path[MAX_PATH], text[2 * SEED_BYTES + 3];
	size_t len, i;
	FILE *f;

	if (!join(path, case_dir, "seed"))
		return 0;
	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "ladx_gen: cannot open %s\n", path);
		return 0;
	}
	len = fread(text, 1, sizeof text - 1, f);
	fclose(f);
	text[len] = '\0';
	if (len == 2 * SEED_BYTES + 1 && text[2 * SEED_BYTES] == '\n')
		len--;
	for (i = 0; i < len && hex_digit((unsigned char)text[i]) >= 0; i++)
		;
	if (len != 2 * SEED_BYTES || i != len) {
		fprintf(stderr, "ladx_gen: %s: not 32 lowercase hex digits\n", path);
		return 0;
	}
	for (i = 0; i < SEED_BYTES; i++)
		seed[i] = (uint8_t)(hex_digit(text[2 * i]) << 4 | hex_digit(text[2 * i + 1]));
	return 1;
}

/* One key=value per line, as ladx_settings_set takes it; then validated. */
static int read_config(const char *case_dir, LadxSettings *settings)
{
	char path[MAX_PATH], line[MAX_CONFIG_LINE];
	int n = 0, ok = 1;
	FILE *f;

	if (!join(path, case_dir, "config.txt"))
		return 0;
	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "ladx_gen: cannot open %s\n", path);
		return 0;
	}
	ladx_settings_defaults(settings);
	while (ok && fgets(line, sizeof line, f)) {
		size_t len = strlen(line);

		n++;
		if (len > 0 && line[len - 1] == '\n')
			line[--len] = '\0';
		else if (!feof(f)) {
			fprintf(stderr, "ladx_gen: %s:%d: line too long\n", path, n);
			ok = 0;
			break;
		}
		if (ladx_settings_set(settings, line) != LADX_SETTINGS_OK) {
			fprintf(stderr, "ladx_gen: %s:%d: bad setting: %s\n", path, n, line);
			ok = 0;
		}
	}
	if (ferror(f)) {
		fprintf(stderr, "ladx_gen: cannot read %s\n", path);
		ok = 0;
	}
	fclose(f);
	if (ok)
		ladx_settings_validate(settings);
	return ok;
}

static int read_case(const char *case_dir, Case *c)
{
	LadxSettingPair finding;

	if (!read_config(case_dir, &c->settings) || !read_seed(case_dir, c->seed))
		return 0;
	if (!ladx_settings_supported(&c->settings, &finding)) {
		fprintf(stderr, "ladx_gen: unsupported settings: %s\n", ladx_reason_id(finding.reason));
		return 0;
	}
	return 1;
}

static int put_line(FILE *out, const char *line, size_t len)
{
	return len > 0 && fputs(line, out) != EOF && fputc('\n', out) != EOF;
}

/* The fill projection of run.txt: the sections, then the trace held in trace, then the outcome. */
static int write_run(FILE *out, const Case *c, const LadxRandomizer *r, FILE *trace)
{
	char line[LADX_RUN_TEXT_MAX_LINE];
	char short_string[LADX_SETTINGS_SHORT_MAX + 1];
	const LadxOutcome *outcome = ladx_randomizer_outcome(r);
	size_t got;

	if (ladx_settings_short_string(&c->settings, short_string, sizeof short_string) < 0)
		return 0;
	if (!put_line(out, line, ladx_run_text_seed_line(line, sizeof line, c->seed, SEED_BYTES)) ||
	    !put_line(out, line, ladx_run_text_settings_line(line, sizeof line, short_string)))
		return 0;
	rewind(trace);
	while ((got = fread(line, 1, sizeof line, trace)) > 0)
		if (fwrite(line, 1, got, out) != got)
			return 0;
	if (ferror(trace))
		return 0;
	if (outcome->kind != LADX_OUTCOME_UNFINISHED &&
	    !put_line(out, line, ladx_run_text_outcome_line(line, sizeof line, outcome)))
		return 0;
	return 1;
}

static int generate(const char *case_dir, const char *out_dir)
{
	static LadxRandomizer r;
	LadxRunTextTrace sink;
	char path[MAX_PATH];
	FILE *trace, *out;
	Case c;
	int ok;

	if (!read_case(case_dir, &c))
		return 2;
	trace = tmpfile();
	if (!trace) {
		fprintf(stderr, "ladx_gen: cannot open a temporary file\n");
		return 1;
	}
	ladx_run_text_trace_init(&sink, trace);
	if (ladx_randomize(&r, &c.settings, c.seed, SEED_BYTES, ladx_run_text_trace_sink, &sink) !=
	    LADX_RANDOMIZE_OK) {
		fprintf(stderr, "ladx_gen: broken run\n");
		fclose(trace);
		return 2;
	}
	if (sink.failed) {
		fprintf(stderr, "ladx_gen: a trace line did not render or write\n");
		fclose(trace);
		return 1;
	}
	if (!join(path, out_dir, "run.txt")) {
		fclose(trace);
		return 2;
	}
	out = fopen(path, "w");
	if (!out) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		fclose(trace);
		return 1;
	}
	ok = write_run(out, &c, &r, trace);
	fclose(trace);
	if (fclose(out) != 0 || !ok) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		return 1;
	}
	return 0;
}

static int time_run(const char *case_dir)
{
	static LadxRandomizer r;
	struct timespec start, end;
	Case c;
	LadxRandomizeStatus status;

	if (!read_case(case_dir, &c))
		return 2;
	clock_gettime(CLOCK_MONOTONIC, &start);
	status = ladx_randomize(&r, &c.settings, c.seed, SEED_BYTES, NULL, NULL);
	clock_gettime(CLOCK_MONOTONIC, &end);
	if (status != LADX_RANDOMIZE_OK) {
		fprintf(stderr, "ladx_gen: broken run\n");
		return 2;
	}
	printf("%ld\n", (long)((end.tv_sec - start.tv_sec) * 1000000L + (end.tv_nsec - start.tv_nsec) / 1000L));
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "--time") == 0)
		return time_run(argv[2]);
	if (argc == 3 && argv[1][0] != '-')
		return generate(argv[1], argv[2]);
	fputs(usage, stderr);
	return 2;
}
