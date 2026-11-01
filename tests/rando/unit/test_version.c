#include "rando_version.h"
#include "support.h"

#include <string.h>

int main(void)
{
	const char *commit = rando_vt_commit();

	if (strcmp(commit, "219fcafd029dab597b8db400efafd8f56f8b4edb") != 0)
		test_fail("test_version", "VT commit is %s", commit);
	return 0;
}
