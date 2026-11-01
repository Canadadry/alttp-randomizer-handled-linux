#ifndef RANDO_TEST_SUPPORT_H
#define RANDO_TEST_SUPPORT_H

/* Prints "<test>: FAIL: <message>" to stderr and exits 1. */
void test_fail(const char *test, const char *fmt, ...);

#endif
