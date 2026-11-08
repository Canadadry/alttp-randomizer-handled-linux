#include "support.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_fixtures_mode(const char *test, int argc, char **argv)
{
	if (argc == 1)
		return 0;
	if (argc == 2 && strcmp(argv[1], "--fixtures") == 0)
		return 1;
	fprintf(stderr, "usage: %s [--fixtures]\n", test);
	exit(2);
}

void test_fail(const char *test, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: FAIL ", test);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}
