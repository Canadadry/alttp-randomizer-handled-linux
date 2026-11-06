#define _POSIX_C_SOURCE 200809L

#include "rando_seed_save.h"
#include "synthetic_base.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_MAX_LEN 4096

static const char usage[] = "usage: sd_fixture <folder> <fixture>\n"
			    "fixtures: good (synthetic source and base patch), bad-source (source byte 0 inverted)\n"
			    "prints the --source-md5 and --base-md5 flags matching the synthetic base\n";

static char path[PATH_MAX_LEN];

static void fail(const char *what, const char *arg, int status)
{
	fprintf(stderr, "sd_fixture: %s%s%s\n", what, arg != NULL ? ": " : "", arg != NULL ? arg : "");
	if (status == 2)
		fputs(usage, stderr);
	exit(status);
}

static const char *at(const char *dir, const char *rel)
{
	if ((size_t)snprintf(path, sizeof path, "%s/%s", dir, rel) >= sizeof path)
		fail("path too long", dir, 1);
	return path;
}

/* true when dir is missing, or a folder with no entry. */
static bool missing_or_empty(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	bool empty = true;

	if (d == NULL)
		return errno == ENOENT;
	while ((e = readdir(d)) != NULL)
		if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
			empty = false;
	closedir(d);
	return empty;
}

static void make_dir(const char *dir)
{
	if (mkdir(dir, 0755) != 0 && errno != EEXIST)
		fail("cannot create", dir, 1);
}

static void write_bytes(const char *file, const uint8_t *bytes, size_t size)
{
	FILE *f = fopen(file, "wb");

	if (f == NULL || fwrite(bytes, 1, size, f) != size || fclose(f) != 0)
		fail("cannot write", file, 1);
}

static void print_hex(const uint8_t *digest)
{
	int i;

	for (i = 0; i < RANDO_MD5_SIZE; i++)
		printf("%02x", digest[i]);
}

int main(int argc, char **argv)
{
	const char *dir, *fixture;
	bool bad_source;

	if (argc != 3)
		fail("expected a folder and a fixture", NULL, 2);
	dir = argv[1];
	fixture = argv[2];
	if (strcmp(fixture, "good") == 0)
		bad_source = false;
	else if (strcmp(fixture, "bad-source") == 0)
		bad_source = true;
	else
		fail("unknown fixture", fixture, 2);
	if (!missing_or_empty(dir))
		fail("folder exists and is not empty", dir, 2);

	synth_build();
	make_dir(dir);
	make_dir(at(dir, "App"));
	make_dir(at(dir, "App/ZeldaRando"));
	if (bad_source)
		synth_source[0] = (uint8_t)~synth_source[0];
	write_bytes(at(dir, RANDO_SD_BASE_ROM_FILE), synth_source, SYNTH_SOURCE_SIZE);
	write_bytes(at(dir, RANDO_SD_BASE_PATCH_FILE), synth_good_patch.bytes, synth_good_patch.size);
	printf("--source-md5 ");
	print_hex(synth_source_md5);
	printf(" --base-md5 ");
	print_hex(synth_base_md5);
	printf("\n");
	return EXIT_SUCCESS;
}
