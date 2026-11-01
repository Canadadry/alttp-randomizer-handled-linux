#ifndef RANDO_PATCH_H
#define RANDO_PATCH_H

#include "rando_assemble.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define RANDO_PATCH_ERROR_MAX 512
#define RANDO_PATCH_DEFAULT_BPS "data/base.bps"
#define RANDO_PATCH_USAGE \
	"usage: rando_patch --base <jp.sfc> --out <file> [--bps <patch>] [--seed <n>] [key=value ...]"

/* The paths point into the argv given to rando_patch_parse_args. */
typedef struct RandoPatchArgs {
	const char *base_path;
	const char *out_path;
	const char *bps_path;
	bool has_seed;
	uint32_t seed;
	RandoAssembleRequest request; /* digests, options, hints, heart beep; no buffer, seed or output */
} RandoPatchArgs;

/* argv[1..argc): --base and --out required, --bps and --seed optional, every other argument
 * key=value (an option key, hints or heart_beep). Missing keys keep rando_assemble_request_init's
 * values. Returns false with a message in error on any refusal; args is then unspecified. */
bool rando_patch_parse_args(int argc, char **argv, RandoPatchArgs *args, char error[RANDO_PATCH_ERROR_MAX]);

/* Reads the base and the patch, draws a seed unless one was given, assembles and writes the ROM to
 * out_path, removing it on a failed write. Prints the seed and the path to out, one message on err
 * on failure. Returns the exit code: 0, 1 input or output, 4 unsupported, 5 gave up. */
int rando_patch_run(const RandoPatchArgs *args, FILE *out, FILE *err);

#endif
