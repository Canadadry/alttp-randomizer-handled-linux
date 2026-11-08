#include "ladx_assemble.h"
#include "ladx_inputs.h"
#include "ladx_settings.h"

#include <stdio.h>
#include <string.h>

#ifndef LADX_TABLES_DEFAULT
#define LADX_TABLES_DEFAULT "data/ladx"
#endif

#define PROG "ladx_patch"
#define MAX_PATH 4096

static const char usage[] =
	"usage: ladx_patch --base <file> --out <file> [--seed <32 lowercase hex>] [--tables <dir>] [<key>=<value> ...]\n";

static LadxAssembler assembler;
static LadxTablesDir tables;
static uint8_t base[LADX_ROM_SIZE];
static uint8_t image[LADX_ROM_SIZE];

static void print_hex(FILE *f, const uint8_t *bytes, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		fprintf(f, "%02x", bytes[i]);
}

static int random_seed(uint8_t seed[LADX_INPUTS_SEED_BYTES])
{
	FILE *f = fopen("/dev/urandom", "rb");
	size_t n;

	if (!f) {
		fprintf(stderr, PROG ": cannot open /dev/urandom\n");
		return 0;
	}
	n = fread(seed, 1, LADX_INPUTS_SEED_BYTES, f);
	fclose(f);
	if (n != LADX_INPUTS_SEED_BYTES) {
		fprintf(stderr, PROG ": cannot read /dev/urandom\n");
		return 0;
	}
	return 1;
}

/* LADXR's error or raise, or a C failure, on stderr: always exit 1. */
static int generation_failed(LadxAssembleStatus st)
{
	const LadxOutcome *o = NULL;

	if (st == LADX_ASSEMBLE_OUTCOME)
		o = ladx_randomizer_outcome(&assembler.randomizer);
	else if (st == LADX_ASSEMBLE_PIPELINE && assembler.pipeline_error.status == LADX_PIPELINE_RAISED)
		o = assembler.pipeline_error.outcome;
	if (o)
		fprintf(stderr, PROG ": LADXR %s: %s%s%s\n", o->kind == LADX_OUTCOME_ERROR ? "error" : "raises",
			o->type ? o->type : "", o->type ? ": " : "", o->message ? o->message : "");
	else if (st == LADX_ASSEMBLE_PIPELINE && assembler.pipeline_error.status == LADX_PIPELINE_GROUP)
		fprintf(stderr, PROG ": group %s failed\n", assembler.pipeline_error.group);
	else
		fprintf(stderr, PROG ": generation failed: %s\n", ladx_assemble_status_name(st));
	return 1;
}

static int write_out(const char *path)
{
	char part[MAX_PATH];
	FILE *f;
	int n = snprintf(part, sizeof part, "%s.part", path);

	if (n < 0 || (size_t)n >= sizeof part) {
		fprintf(stderr, PROG ": path too long: %s\n", path);
		return 2;
	}
	f = fopen(part, "wb");
	if (!f) {
		fprintf(stderr, PROG ": cannot write %s\n", part);
		return 1;
	}
	if (fwrite(image, 1, sizeof image, f) != sizeof image || fclose(f) != 0) {
		fprintf(stderr, PROG ": cannot write %s\n", part);
		remove(part);
		return 1;
	}
	if (rename(part, path) != 0) {
		fprintf(stderr, PROG ": cannot rename %s to %s\n", part, path);
		remove(part);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *base_path = NULL, *out_path = NULL, *seed_text = NULL, *tables_path = LADX_TABLES_DEFAULT;
	uint8_t seed[LADX_INPUTS_SEED_BYTES];
	LadxSettings settings;
	LadxSettingPair finding;
	LadxAssembleInput in;
	LadxAssembleStatus st;
	size_t n;
	int i;

	ladx_settings_defaults(&settings);
	for (i = 1; i < argc; i++) {
		const char **opt = strcmp(argv[i], "--base") == 0     ? &base_path
				   : strcmp(argv[i], "--out") == 0    ? &out_path
				   : strcmp(argv[i], "--seed") == 0   ? &seed_text
				   : strcmp(argv[i], "--tables") == 0 ? &tables_path
								      : NULL;

		if (opt) {
			if (i + 1 == argc) {
				fputs(usage, stderr);
				return 2;
			}
			*opt = argv[++i];
		} else if (argv[i][0] == '-' || !strchr(argv[i], '=')) {
			fputs(usage, stderr);
			return 2;
		} else if (ladx_settings_set(&settings, argv[i]) != LADX_SETTINGS_OK) {
			fprintf(stderr, PROG ": bad setting: %s\n", argv[i]);
			return 2;
		}
	}
	if (!base_path || !out_path) {
		fputs(usage, stderr);
		return 2;
	}
	ladx_settings_validate(&settings);
	if (!ladx_settings_supported(&settings, &finding)) {
		fprintf(stderr, PROG ": unsupported settings: %s\n", ladx_reason_id(finding.reason));
		return 2;
	}
	if (seed_text && !ladx_inputs_seed_hex(seed_text, strlen(seed_text), seed)) {
		fprintf(stderr, PROG ": --seed: not 32 lowercase hex digits\n");
		return 2;
	}
	if (!ladx_inputs_file(PROG, base_path, base, sizeof base, &n, 1)) {
		if (n == 0)
			fprintf(stderr, PROG ": --base: no file at %s\n", base_path);
		return n == 0 ? 3 : 1;
	}
	if (n > sizeof base) {
		fprintf(stderr, PROG ": --base: not the LADX English 1.0 ROM (over 1 MiB)\n");
		return 3;
	}
	if (!ladx_inputs_tables(PROG, tables_path, &tables))
		return 2;
	if (!seed_text) {
		if (!random_seed(seed))
			return 1;
		printf("seed ");
		print_hex(stdout, seed, sizeof seed);
		printf("\n");
	}
	memset(&in, 0, sizeof in);
	in.base = base;
	in.base_len = n;
	in.tables = &tables.tables;
	in.settings = &settings;
	in.seed = seed;
	in.seed_len = sizeof seed;
	st = ladx_assemble(&assembler, &in, image);
	if (st == LADX_ASSEMBLE_WRONG_BASE) {
		fprintf(stderr, PROG ": --base: not the LADX English 1.0 ROM (SHA-1 ");
		print_hex(stderr, assembler.base_digest, LADX_SHA1_SIZE);
		fprintf(stderr, ")\n");
		return 3;
	}
	if (st != LADX_ASSEMBLE_OK)
		return generation_failed(st);
	return write_out(out_path);
}
