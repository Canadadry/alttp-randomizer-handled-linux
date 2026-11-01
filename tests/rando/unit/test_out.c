#define _POSIX_C_SOURCE 200809L

#include "rando_out.h"
#include "support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST "test_out"
#define CAP 16u

static char root[] = "/tmp/test_out_XXXXXX";
static char path[256];
static uint8_t buf[CAP + 1];

static const char *at(const char *name)
{
	snprintf(path, sizeof path, "%s/%s", root, name);
	return path;
}

static void write_bytes(const char *name, size_t n)
{
	FILE *f = fopen(at(name), "wb");
	size_t i;
	if (f == NULL)
		test_fail(TEST, "cannot create %s", path);
	for (i = 0; i < n; i++)
		fputc((int)(i & 0xFF), f);
	fclose(f);
}

static void check_read(const char *what, const char *name, RandoOutRead want, size_t want_size, long want_total)
{
	size_t size = 12345;
	long total = 12345;
	RandoOutRead got;
	memset(buf, 0xEE, sizeof buf);
	got = rando_out_read_capped(at(name), buf, CAP, &size, &total);
	if (got != want)
		test_fail(TEST, "%s: status %d, want %d", what, (int)got, (int)want);
	if (want != RANDO_OUT_READ_OK && want != RANDO_OUT_READ_TOO_LARGE)
		return;
	if (size != want_size || total != want_total)
		test_fail(TEST, "%s: size %lu total %ld, want %lu %ld", what, (unsigned long)size, total,
			  (unsigned long)want_size, want_total);
	if (size > 0 && (buf[0] != 0 || buf[size - 1] != (uint8_t)((size - 1) & 0xFF)))
		test_fail(TEST, "%s: bytes not read", what);
}

static void test_read_capped(void)
{
	write_bytes("cap", CAP);
	write_bytes("over", CAP + 1);
	write_bytes("far_over", 1000);
	write_bytes("empty", 0);
	if (mkdir(at("dir"), 0755) != 0)
		test_fail(TEST, "cannot create %s", path);
	check_read("exactly the cap", "cap", RANDO_OUT_READ_OK, CAP, CAP);
	check_read("cap + 1", "over", RANDO_OUT_READ_TOO_LARGE, CAP + 1, CAP + 1);
	check_read("far over the cap", "far_over", RANDO_OUT_READ_TOO_LARGE, CAP + 1, 1000);
	check_read("empty", "empty", RANDO_OUT_READ_OK, 0, 0);
	check_read("missing", "absent", RANDO_OUT_READ_MISSING, 0, 0);
	check_read("directory", "dir", RANDO_OUT_READ_READ, 0, 0);
	remove(at("cap"));
	remove(at("over"));
	remove(at("far_over"));
	remove(at("empty"));
	rmdir(at("dir"));
}

static void test_random_seed(void)
{
	uint32_t a = rando_out_random_seed();
	uint32_t b = rando_out_random_seed();
	if (a == b)
		test_fail(TEST, "two seed draws gave %lu twice", (unsigned long)a);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	if (mkdtemp(root) == NULL)
		test_fail(TEST, "mkdtemp failed");
	test_read_capped();
	test_random_seed();
	rmdir(root);
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
