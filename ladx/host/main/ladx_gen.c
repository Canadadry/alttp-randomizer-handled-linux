#define _POSIX_C_SOURCE 199309L

#include "ladx_assemble.h"
#include "ladx_inputs.h"
#include "ladx_ks.h"
#include "ladx_pipeline.h"
#include "ladx_spoiler.h"
#include "ladx_randomizer.h"
#include "ladx_rom.h"
#include "ladx_run_text.h"
#include "ladx_settings.h"
#include "ladx_tables.h"
#include "ladx_wlog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEED_BYTES LADX_INPUTS_SEED_BYTES
#define MAX_PATH 4096
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

static int read_case(const char *case_dir, Case *c)
{
	return ladx_inputs_case("ladx_gen", case_dir, &c->settings, c->seed);
}

static LadxTablesDir tables_dir;
static LadxWlog wlog;

static int open_tables(const char *dir)
{
	return ladx_inputs_tables("ladx_gen", dir, &tables_dir);
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

static int run_pipeline(const LadxSettings *settings, LadxRandomizer *r, const LadxPipelineRom *given)
{
	LadxPipelineError err;

	if (ladx_enc_init(&encoder, given ? given->bytes : NULL, given ? given->len : 0, given ? given->ks : NULL,
			  given ? given->tables : NULL, &wlog) != LADX_ENC_OK) {
		fprintf(stderr, "ladx_gen: tables: write log bad input\n");
		return 0;
	}
	pipeline.tables = &tables_dir.tables;
	pipeline.settings = settings;
	pipeline.spot_count = ladx_tables_spot_count(ladx_randomizer_placement(r));
	pipeline.randomizer = r;
	pipeline.rnd = &r->rnd;
	pipeline.global = &r->global;
	pipeline.rom = given;
	pipeline.groups = ladx_pipeline_groups(&pipeline.group_count);
	pipeline.ctx = NULL;
	pipeline.spoiler = NULL;
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
	case LADX_PIPELINE_RAISED:
		return 2;
	default:
		fprintf(stderr, "ladx_gen: over %d groups\n", LADX_PIPELINE_GROUP_MAX);
		return 0;
	}
}

static int write_rom_writes(const char *out_dir, const LadxSettings *settings, LadxRandomizer *r,
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
	ok = ladx_wlog_begin(&wlog, ladx_tables_names(&tables_dir.tables), file_sink, &sink) == LADX_WLOG_OK
		     ? run_pipeline(settings, r, given)
		     : 0;
	if (fclose(sink.f) != 0)
		sink.failed = 1;
	if (ok != 1) {
		remove(path);
		return ok == 2 ? 2 : -1;
	}
	if (sink.failed) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		return 0;
	}
	return 1;
}

static int write_spoiler(const char *out_dir, const LadxSpoiler *spoiler)
{
	char path[MAX_PATH];
	FileSink sink;
	int ok;

	if (!join(path, out_dir, "spoiler.json"))
		return 0;
	sink.f = fopen(path, "w");
	sink.failed = 0;
	if (!sink.f) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		return 0;
	}
	ok = ladx_spoiler_render(spoiler, file_sink, &sink);
	if (fclose(sink.f) != 0 || sink.failed || !ok) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		remove(path);
		return 0;
	}
	return 1;
}

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

static LadxAssembler assembler;
static uint8_t image[LADX_ROM_SIZE];

static int pipeline_failure(const LadxPipelineError *err)
{
	switch (err->status) {
	case LADX_PIPELINE_RAISED:
		return 0;
	case LADX_PIPELINE_TABLES:
		tables_error(&err->tables);
		return 2;
	case LADX_PIPELINE_ENCODER:
		fprintf(stderr, "ladx_gen: tables: write log %s\n",
			err->encoder == LADX_ENC_OVERFLOW ? "overflow" : "bad input");
		return 2;
	case LADX_PIPELINE_GROUP:
		fprintf(stderr, "ladx_gen: group %s failed\n", err->group);
		return 2;
	default:
		fprintf(stderr, "ladx_gen: over %d groups\n", LADX_PIPELINE_GROUP_MAX);
		return 2;
	}
}

static int assemble(const Case *c, const char *out_dir, const char *rom_path, LadxRunTextTrace *trace,
		    const LadxRandomizer **r, const LadxOutcome **raised, int *written)
{
	LadxAssembleInput in;
	LadxAssembleStatus st;
	char path[MAX_PATH];
	FileSink sink;
	size_t n;
	int i, code = 0;

	if (!ladx_inputs_file("ladx_gen", rom_path, rom_bytes, sizeof rom_bytes, &n, 0))
		return n == 0 ? 2 : 1;
	if (n > sizeof rom_bytes) {
		fprintf(stderr, "ladx_gen: --rom: not the LADX English 1.0 ROM (over 1 MiB)\n");
		return 3;
	}
	if (!join(path, out_dir, "rom_writes.txt"))
		return 2;
	sink.f = fopen(path, "w");
	sink.failed = 0;
	if (!sink.f) {
		fprintf(stderr, "ladx_gen: cannot write %s\n", path);
		return 1;
	}
	memset(&in, 0, sizeof in);
	in.base = rom_bytes;
	in.base_len = n;
	in.tables = &tables_dir.tables;
	in.settings = &c->settings;
	in.seed = c->seed;
	in.seed_len = SEED_BYTES;
	in.trace = ladx_run_text_trace_sink;
	in.trace_ctx = trace;
	in.log = file_sink;
	in.log_ctx = &sink;
	st = ladx_assemble(&assembler, &in, image);
	if (fclose(sink.f) != 0)
		sink.failed = 1;
	*r = &assembler.randomizer;
	*raised = NULL;
	*written = st == LADX_ASSEMBLE_OK;
	switch (st) {
	case LADX_ASSEMBLE_OK:
		if (sink.failed) {
			fprintf(stderr, "ladx_gen: cannot write %s\n", path);
			code = 1;
		}
		break;
	case LADX_ASSEMBLE_WRONG_BASE:
		fprintf(stderr, "ladx_gen: --rom: not the LADX English 1.0 ROM (SHA-1 ");
		for (i = 0; i < LADX_SHA1_SIZE; i++)
			fprintf(stderr, "%02x", assembler.base_digest[i]);
		fprintf(stderr, ")\n");
		code = 3;
		break;
	case LADX_ASSEMBLE_BAD_BASE:
		fprintf(stderr, "ladx_gen: --rom: the LADX ROM's tables do not parse\n");
		code = 2;
		break;
	case LADX_ASSEMBLE_OUTCOME:
		break;
	case LADX_ASSEMBLE_PIPELINE:
		code = pipeline_failure(&assembler.pipeline_error);
		if (!code)
			*raised = &assembler.pipeline.raised;
		break;
	default:
		fprintf(stderr, "ladx_gen: %s\n", st == LADX_ASSEMBLE_BROKEN ? "broken run" : ladx_assemble_status_name(st));
		code = 2;
	}
	if (st != LADX_ASSEMBLE_OK)
		remove(path);
	return code;
}

static int generate(const char *case_dir, const char *out_dir, const char *tables_path, const char *rom_path)
{
	static LadxRandomizer own;
	const LadxRandomizer *r = &own;
	const LadxOutcome *raised = NULL;
	LadxRunTextTrace sink;
	char path[MAX_PATH];
	FILE *trace, *out;
	Case c;
	int ok, written = 0;

	if (!read_case(case_dir, &c))
		return 2;
	if (tables_path && !open_tables(tables_path))
		return 2;
	trace = tmpfile();
	if (!trace) {
		fprintf(stderr, "ladx_gen: cannot open a temporary file\n");
		return 1;
	}
	ladx_run_text_trace_init(&sink, trace);
	if (rom_path) {
		ok = assemble(&c, out_dir, rom_path, &sink, &r, &raised, &written);
		if (ok) {
			fclose(trace);
			return ok;
		}
	} else {
		if (ladx_randomize(&own, &c.settings, c.seed, SEED_BYTES, ladx_run_text_trace_sink, &sink) !=
		    LADX_RANDOMIZE_OK) {
			fprintf(stderr, "ladx_gen: broken run\n");
			fclose(trace);
			return 2;
		}
		if (tables_path && ladx_randomizer_placement(&own)) {
			written = write_rom_writes(out_dir, &c.settings, &own, NULL);
			if (written <= 0) {
				fclose(trace);
				return written < 0 ? 2 : 1;
			}
			if (written == 2)
				raised = &pipeline.raised;
		}
	}
	if (written == 1 && rom_path && assembler.spoiler.built && !write_spoiler(out_dir, &assembler.spoiler)) {
		fclose(trace);
		return 1;
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
	ok = ladx_run_text_write_run(out, &c.settings, c.seed, SEED_BYTES, r, trace, raised);
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

static char log_text[LOG_TEXT_MAX];

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
