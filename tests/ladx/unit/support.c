#include "support.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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
