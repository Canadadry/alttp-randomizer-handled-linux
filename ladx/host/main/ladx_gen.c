#define _POSIX_C_SOURCE 199309L

#include "ladx_ks.h"
#include "ladx_pipeline.h"
#include "ladx_randomizer.h"
#include "ladx_rom.h"
#include "ladx_run_text.h"
#include "ladx_settings.h"
#include "ladx_tables.h"
#include "ladx_wlog.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEED_BYTES 16
#define MAX_PATH 4096
#define MAX_CONFIG_LINE 256
/* data/ladx/ in all: 534,590 bytes today. */
#define TABLES_TEXT_MAX (2 * 1024 * 1024)
#define TABLES_NAME_MAX 64
/* rom_writes.txt: 224 KiB per case at most (the goldens' budget). */
#define LOG_TEXT_MAX (4 * 1024 * 1024)
#define LOG_GROUPS_MAX 64

static const char usage[] = "usage: ladx_gen [--tables <dir> [--rom <file>]] <case_dir> <out_dir>\n"
			    "       ladx_gen --rom <file> --ks-spans <out>\n"
			    "       ladx_gen --time <case_dir>\n"
			    "       ladx_gen --project <rom_writes.txt> <out_dir>\n";

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

static char tables_text[TABLES_TEXT_MAX];
static size_t tables_text_len;
static char tables_names[LADX_TABLES_GROUP_MAX][TABLES_NAME_MAX];
static LadxTables tables;
static LadxWlog wlog;

/* The file's text appended to tables_text; 0 when unreadable or too large. */
static int read_tables_file(const char *dir, const char *name, LadxTablesFile *file)
{
	char path[MAX_PATH];
	size_t n;
	FILE *f;

	if (!join(path, dir, name))
		return 0;
	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "ladx_gen: tables: cannot open %s\n", path);
		return 0;
	}
	n = fread(tables_text + tables_text_len, 1, TABLES_TEXT_MAX - tables_text_len, f);
	if (ferror(f) || n == TABLES_TEXT_MAX - tables_text_len) {
		fprintf(stderr, "ladx_gen: tables: cannot read %s, or the tables are over %d bytes\n", path,
			TABLES_TEXT_MAX);
		fclose(f);
		return 0;
	}
	fclose(f);
	file->name = name;
	file->text.at = tables_text + tables_text_len;
	file->text.len = n;
	tables_text_len += n;
	return 1;
}

static int name_cmp(const void *a, const void *b)
{
	return strcmp(a, b);
}

/* order.txt and every other .txt file of dir, indexed into tables. */
static int open_tables(const char *dir)
{
	LadxTablesFile order, groups[LADX_TABLES_GROUP_MAX];
	LadxTablesError err;
	struct dirent *e;
	int n = 0, i;
	DIR *d = opendir(dir);

	if (!d) {
		fprintf(stderr, "ladx_gen: tables: cannot open %s\n", dir);
		return 0;
	}
	while ((e = readdir(d)) != NULL) {
		size_t len = strlen(e->d_name);

		if (len < 5 || strcmp(e->d_name + len - 4, ".txt") != 0 || strcmp(e->d_name, "order.txt") == 0)
			continue;
		if (n == LADX_TABLES_GROUP_MAX || len >= TABLES_NAME_MAX) {
			fprintf(stderr, "ladx_gen: tables: %s: over %d group files or a long name\n", dir,
				LADX_TABLES_GROUP_MAX);
			closedir(d);
			return 0;
		}
		strcpy(tables_names[n++], e->d_name);
	}
	closedir(d);
	qsort(tables_names, (size_t)n, sizeof tables_names[0], name_cmp);
	tables_text_len = 0;
	if (!read_tables_file(dir, "order.txt", &order))
		return 0;
	for (i = 0; i < n; i++)
		if (!read_tables_file(dir, tables_names[i], &groups[i]))
			return 0;
	if (ladx_tables_open(&tables, &order, groups, n, &err) != LADX_TABLES_OK) {
		fprintf(stderr, "ladx_gen: tables: %s %s:%d\n", ladx_tables_status_name(err.status),
			err.file ? err.file : "-", err.line);
		return 0;
	}
	return 1;
}

typedef struct FileSink {
	FILE *f;
	int failed;
} FileSink;

static void file_sink(void *ctx, const char *text, size_t len)
{
	FileSink *sink = ctx;

	if (!sink->failed && fwrite(text, 1, len, sink->f) != len)
		sink->failed = 1;
}

static int tables_error(const LadxTablesError *err)
{
	if (err->status == LADX_TABLES_NO_ENTRY)
		fprintf(stderr, "ladx_gen: tables: unit %s: no entry for %s\n", err->unit, err->values);
	else
		fprintf(stderr, "ladx_gen: tables: %s %s:%d\n", ladx_tables_status_name(err->status),
			err->file ? err->file : "-", err->line);
	return 0;
}

static LadxEncoder encoder;
static LadxPipeline pipeline;
static uint8_t rom_bytes[LADX_ROM_SIZE];
static LadxRomTables rom_tables;
static LadxKs ks;
static LadxPipelineRom rom;

/* The pipeline's walk into the open log: table entries and the ported C groups (with --rom, every one). */
static int run_pipeline(const LadxSettings *settings, const LadxRandomizer *r, const LadxPipelineRom *given)
{
	LadxPipelineError err;

	if (ladx_enc_init(&encoder, given ? given->bytes : NULL, given ? given->len : 0, given ? given->ks : NULL,
			  given ? given->tables : NULL, &wlog) != LADX_ENC_OK) {
		fprintf(stderr, "ladx_gen: tables: write log bad input\n");
		return 0;
	}
	pipeline.tables = &tables;
	pipeline.settings = settings;
	pipeline.spot_count = ladx_tables_spot_count(ladx_randomizer_placement(r));
	pipeline.randomizer = r;
	pipeline.rom = given;
	pipeline.groups = ladx_pipeline_groups(&pipeline.group_count);
	pipeline.ctx = NULL;
	switch (ladx_pipeline_run(&pipeline, &encoder, &err)) {
	case LADX_PIPELINE_OK:
		return 1;
	case LADX_PIPELINE_TABLES:
		return tables_error(&err.tables);
	case LADX_PIPELINE_ENCODER:
		fprintf(stderr, "ladx_gen: tables: write log %s\n",
			err.encoder == LADX_ENC_OVERFLOW ? "overflow" : "bad input");
		return 0;
	case LADX_PIPELINE_GROUP:
		fprintf(stderr, "ladx_gen: group %s failed\n", err.group);
		return 0;
	default:
		fprintf(stderr, "ladx_gen: over %d groups\n", LADX_PIPELINE_GROUP_MAX);
		return 0;
	}
}

/* <out_dir>/rom_writes.txt: 0 on an I/O failure (1), -1 on a tables error (2). */
static int write_rom_writes(const char *out_dir, const LadxSettings *settings, const LadxRandomizer *r,
			    const LadxPipelineRom *given)
{
	char path[MAX_PATH];
	FileSink sink;
	int ok;

	if (!join(path, out_dir, "rom_writes.txt"))
		return -1;
	sink.f = fopen(path, "w");
	sink.failed = 0;
	if (!sink.f) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		return 0;
	}
	ok = ladx_wlog_begin(&wlog, ladx_tables_names(&tables), file_sink, &sink) == LADX_WLOG_OK &&
	     run_pipeline(settings, r, given);
	if (fclose(sink.f) != 0)
		sink.failed = 1;
	if (!ok) {
		remove(path);
		return -1;
	}
	if (sink.failed) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		return 0;
	}
	return 1;
}

/* The ROM at path into rom_bytes, checked, parsed and its KS built: 0, else the exit code (3 for another ROM). */
static int read_rom(const char *path)
{
	uint8_t digest[LADX_SHA1_SIZE], buf[4096];
	size_t n = 0, k;
	LadxSha1 sha;
	int i;
	FILE *f = fopen(path, "rb");

	if (!f) {
		fprintf(stderr, "ladx_gen: --rom: cannot open %s\n", path);
		return 2;
	}
	ladx_sha1_init(&sha);
	while ((k = fread(buf, 1, sizeof buf, f)) > 0) {
		ladx_sha1_update(&sha, buf, k);
		if (n < sizeof rom_bytes)
			memcpy(rom_bytes + n, buf, k < sizeof rom_bytes - n ? k : sizeof rom_bytes - n);
		n += k;
	}
	ladx_sha1_final(&sha, digest);
	if (ferror(f)) {
		fprintf(stderr, "ladx_gen: --rom: cannot read %s\n", path);
		fclose(f);
		return 1;
	}
	fclose(f);
	if (n != LADX_ROM_SIZE || ladx_rom_check(rom_bytes, n, digest) != LADX_ROM_OK) {
		fprintf(stderr, "ladx_gen: --rom: not the LADX English 1.0 ROM (SHA-1 ");
		for (i = 0; i < LADX_SHA1_SIZE; i++)
			fprintf(stderr, "%02x", digest[i]);
		fprintf(stderr, ")\n");
		return 3;
	}
	if (ladx_rom_tables_parse(rom_bytes, n, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, rom_bytes, n, &rom_tables) != LADX_KS_OK) {
		fprintf(stderr, "ladx_gen: --rom: the LADX ROM's tables do not parse\n");
		return 2;
	}
	rom.bytes = rom_bytes;
	rom.len = n;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	return 0;
}

/* C's KS span list (tests/ladx/README.md, "KS span list") into out. */
static int write_ks_spans(const char *rom_path, const char *out_path)
{
	FileSink sink;
	int st = read_rom(rom_path);

	if (st)
		return st;
	sink.f = fopen(out_path, "w");
	sink.failed = 0;
	if (!sink.f) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", out_path);
		return 1;
	}
	ladx_ks_render_spans(&ks, file_sink, &sink);
	if (fclose(sink.f) != 0 || sink.failed) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", out_path);
		return 1;
	}
	return 0;
}

static int generate(const char *case_dir, const char *out_dir, const char *tables_dir, const char *rom_path)
{
	static LadxRandomizer r;
	LadxRunTextTrace sink;
	char path[MAX_PATH];
	FILE *trace, *out;
	Case c;
	int ok;

	if (!read_case(case_dir, &c))
		return 2;
	if (tables_dir && !open_tables(tables_dir))
		return 2;
	if (rom_path && (ok = read_rom(rom_path)) != 0)
		return ok;
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
	ok = ladx_run_text_write_run(out, &c.settings, c.seed, SEED_BYTES, &r, trace);
	fclose(trace);
	if (fclose(out) != 0 || !ok) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		return 1;
	}
	if (tables_dir && ladx_randomizer_placement(&r)) {
		ok = write_rom_writes(out_dir, &c.settings, &r, rom_path ? &rom : NULL);
		if (ok <= 0)
			return ok < 0 ? 2 : 1;
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

static char log_text[LOG_TEXT_MAX];

/* <out_dir>/<group>.txt, write_log.py project's text, for every group of the log. */
static int project_log(const char *log_path, const char *out_dir)
{
	LadxSpan text, groups[LOG_GROUPS_MAX];
	char name[LADX_WLOG_NAME_MAX + 5], path[MAX_PATH];
	int count, line, i;
	LadxWlogStatus st;
	size_t n;
	FILE *f = fopen(log_path, "rb");

	if (!f) {
		fprintf(stderr, "ladx_gen: cannot open %s\n", log_path);
		return 2;
	}
	n = fread(log_text, 1, sizeof log_text, f);
	if (ferror(f) || n == sizeof log_text) {
		fprintf(stderr, "ladx_gen: cannot read %s, or it is over %d bytes\n", log_path, LOG_TEXT_MAX);
		fclose(f);
		return 2;
	}
	fclose(f);
	text.at = log_text;
	text.len = n;
	st = ladx_wlog_groups(text, groups, LOG_GROUPS_MAX, &count, &line);
	if (st != LADX_WLOG_OK) {
		if (st == LADX_WLOG_MALFORMED)
			fprintf(stderr, "ladx_gen: %s:%d: malformed write log\n", log_path, line);
		else
			fprintf(stderr, "ladx_gen: %s: over %d groups\n", log_path, LOG_GROUPS_MAX);
		return 2;
	}
	for (i = 0; i < count; i++) {
		FileSink sink;

		memcpy(name, groups[i].at, groups[i].len);
		strcpy(name + groups[i].len, ".txt");
		if (name[0] == '.' || !join(path, out_dir, name)) {
			fprintf(stderr, "ladx_gen: %s: group %s is not a file name\n", log_path, name);
			return 2;
		}
		sink.f = fopen(path, "w");
		sink.failed = 0;
		if (!sink.f) {
			fprintf(stderr, "ladx_gen: cannot write %s\n", path);
			return 1;
		}
		st = ladx_wlog_project(&wlog, text, groups[i], file_sink, &sink, &line);
		if (fclose(sink.f) != 0)
			sink.failed = 1;
		if (st != LADX_WLOG_OK) {
			fprintf(stderr, "ladx_gen: %s: projection of %s: write log overflow\n", log_path, name);
			return 2;
		}
		if (sink.failed) {
			fprintf(stderr, "ladx_gen: cannot write %s\n", path);
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 4 && strcmp(argv[1], "--project") == 0)
		return project_log(argv[2], argv[3]);
	if (argc == 3 && strcmp(argv[1], "--time") == 0)
		return time_run(argv[2]);
	if (argc == 5 && strcmp(argv[1], "--rom") == 0 && strcmp(argv[3], "--ks-spans") == 0)
		return write_ks_spans(argv[2], argv[4]);
	if (argc == 3 && argv[1][0] != '-')
		return generate(argv[1], argv[2], NULL, NULL);
	if (argc == 5 && strcmp(argv[1], "--tables") == 0 && argv[3][0] != '-')
		return generate(argv[3], argv[4], argv[2], NULL);
	if (argc == 7 && strcmp(argv[1], "--tables") == 0 && strcmp(argv[3], "--rom") == 0 && argv[5][0] != '-')
		return generate(argv[5], argv[6], argv[2], argv[4]);
	fputs(usage, stderr);
	return 2;
}
