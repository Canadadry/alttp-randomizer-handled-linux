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

typedef struct RandoPatchArgs {
	const char *base_path;
	const char *out_path;
	const char *bps_path;
	bool has_seed;
	uint32_t seed;
	RandoAssembleRequest request;
} RandoPatchArgs;

bool rando_patch_parse_args(int argc, char **argv, RandoPatchArgs *args, char error[RANDO_PATCH_ERROR_MAX]);

int rando_patch_run(const RandoPatchArgs *args, FILE *out, FILE *err);

const char *rando_patch_bps_name(RandoBpsResult result);

#endif
