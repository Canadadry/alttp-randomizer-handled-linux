#ifndef LADX_TEST_SUPPORT_H
#define LADX_TEST_SUPPORT_H

int test_fixtures_mode(const char *test, int argc, char **argv);

void test_fail(const char *test, const char *fmt, ...);

#endif
