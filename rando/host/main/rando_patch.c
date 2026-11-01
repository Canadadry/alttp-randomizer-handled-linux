#include "rando_patch.h"

#include <stdio.h>

int main(int argc, char **argv)
{
	static RandoPatchArgs args;
	char error[RANDO_PATCH_ERROR_MAX];

	if (!rando_patch_parse_args(argc, argv, &args, error)) {
		if (argc > 1)
			fprintf(stderr, "rando_patch: %s\n", error);
		fprintf(stderr, "%s\n", RANDO_PATCH_USAGE);
		return 2;
	}
	return rando_patch_run(&args, stdout, stderr);
}
