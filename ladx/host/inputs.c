#include "ladx_inputs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define MAX_CONFIG_LINE 256

static int join(const char *prog, char *out, const char *dir, const char *name)
{
	int n = snprintf(out, MAX_PATH, "%s/%s", dir, name);

	if (n < 0 || n >= MAX_PATH) {
		fprintf(stderr, "%s: path too long: %s/%s\n", prog, dir, name);
		return 0;
	}
	return 1;
}

static int hex_digit(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

int ladx_inputs_seed_hex(const char *text, size_t len, uint8_t seed[LADX_INPUTS_SEED_BYTES])
{
	size_t i;

	if (len != 2 * LADX_INPUTS_SEED_BYTES)
		return 0;
	for (i = 0; i < len; i++)
		if (hex_digit((unsigned char)text[i]) < 0)
			return 0;
	for (i = 0; i < LADX_INPUTS_SEED_BYTES; i++)
		seed[i] = (uint8_t)(hex_digit(text[2 * i]) << 4 | hex_digit(text[2 * i + 1]));
	return 1;
}

static int read_seed(const char *prog, const char *case_dir, uint8_t seed[LADX_INPUTS_SEED_BYTES])
{
	char path[MAX_PATH], text[2 * LADX_INPUTS_SEED_BYTES + 3];
	size_t len;
	FILE *f;

	if (!join(prog, path, case_dir, "seed"))
		return 0;
	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "%s: cannot open %s\n", prog, path);
		return 0;
	}
	len = fread(text, 1, sizeof text - 1, f);
	fclose(f);
	text[len] = '\0';
	if (len == 2 * LADX_INPUTS_SEED_BYTES + 1 && text[2 * LADX_INPUTS_SEED_BYTES] == '\n')
		len--;
	if (!ladx_inputs_seed_hex(text, len, seed)) {
		fprintf(stderr, "%s: %s: not 32 lowercase hex digits\n", prog, path);
		return 0;
	}
	return 1;
}

static int read_config(const char *prog, const char *case_dir, LadxSettings *settings)
{
	char path[MAX_PATH], line[MAX_CONFIG_LINE];
	int n = 0, ok = 1;
	FILE *f;

	if (!join(prog, path, case_dir, "config.txt"))
		return 0;
	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "%s: cannot open %s\n", prog, path);
		return 0;
	}
	ladx_settings_defaults(settings);
	while (ok && fgets(line, sizeof line, f)) {
		size_t len = strlen(line);

		n++;
		if (len > 0 && line[len - 1] == '\n')
			line[--len] = '\0';
		else if (!feof(f)) {
			fprintf(stderr, "%s: %s:%d: line too long\n", prog, path, n);
			ok = 0;
			break;
		}
		if (ladx_settings_set(settings, line) != LADX_SETTINGS_OK) {
			fprintf(stderr, "%s: %s:%d: bad setting: %s\n", prog, path, n, line);
			ok = 0;
		}
	}
	if (ferror(f)) {
		fprintf(stderr, "%s: cannot read %s\n", prog, path);
		ok = 0;
	}
	fclose(f);
	if (ok)
		ladx_settings_validate(settings);
	return ok;
}

int ladx_inputs_case(const char *prog, const char *dir, LadxSettings *settings,
		     uint8_t seed[LADX_INPUTS_SEED_BYTES])
{
	LadxSettingPair finding;

	if (!read_config(prog, dir, settings) || !read_seed(prog, dir, seed))
		return 0;
	if (!ladx_settings_supported(settings, &finding)) {
		fprintf(stderr, "%s: unsupported settings: %s\n", prog, ladx_reason_id(finding.reason));
		return 0;
	}
	return 1;
}

static int read_tables_file(const char *prog, const char *dir, const char *name, LadxTablesDir *d,
			    LadxTablesFile *file)
{
	char path[MAX_PATH];
	size_t n;
	FILE *f;

	if (!join(prog, path, dir, name))
		return 0;
	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "%s: tables: cannot open %s\n", prog, path);
		return 0;
	}
	n = fread(d->text + d->len, 1, LADX_INPUTS_TABLES_TEXT_MAX - d->len, f);
	if (ferror(f) || n == LADX_INPUTS_TABLES_TEXT_MAX - d->len) {
		fprintf(stderr, "%s: tables: cannot read %s, or the tables are over %d bytes\n", prog, path,
			LADX_INPUTS_TABLES_TEXT_MAX);
		fclose(f);
		return 0;
	}
	fclose(f);
	file->name = name;
	file->text.at = d->text + d->len;
	file->text.len = n;
	d->len += n;
	return 1;
}

static int name_cmp(const void *a, const void *b)
{
	return strcmp(a, b);
}

int ladx_inputs_tables(const char *prog, const char *dir, LadxTablesDir *d)
{
	LadxTablesFile order, groups[LADX_TABLES_GROUP_MAX];
	LadxTablesError err;
	struct dirent *e;
	int n = 0, i;
	DIR *dd = opendir(dir);

	if (!dd) {
		fprintf(stderr, "%s: tables: cannot open %s\n", prog, dir);
		return 0;
	}
	while ((e = readdir(dd)) != NULL) {
		size_t len = strlen(e->d_name);

		if (len < 5 || strcmp(e->d_name + len - 4, ".txt") != 0 || strcmp(e->d_name, "order.txt") == 0)
			continue;
		if (n == LADX_TABLES_GROUP_MAX || len >= LADX_INPUTS_NAME_MAX) {
			fprintf(stderr, "%s: tables: %s: over %d group files or a long name\n", prog, dir,
				LADX_TABLES_GROUP_MAX);
			closedir(dd);
			return 0;
		}
		strcpy(d->names[n++], e->d_name);
	}
	closedir(dd);
	qsort(d->names, (size_t)n, sizeof d->names[0], name_cmp);
	d->len = 0;
	if (!read_tables_file(prog, dir, "order.txt", d, &order))
		return 0;
	for (i = 0; i < n; i++)
		if (!read_tables_file(prog, dir, d->names[i], d, &groups[i]))
			return 0;
	if (ladx_tables_open(&d->tables, &order, groups, n, &err) != LADX_TABLES_OK) {
		fprintf(stderr, "%s: tables: %s %s:%d\n", prog, ladx_tables_status_name(err.status),
			err.file ? err.file : "-", err.line);
		return 0;
	}
	return 1;
}

int ladx_inputs_file(const char *prog, const char *path, uint8_t *buf, size_t cap, size_t *n, int quiet)
{
	uint8_t extra;
	FILE *f = fopen(path, "rb");

	*n = 0;
	if (!f) {
		if (!quiet)
			fprintf(stderr, "%s: cannot open %s\n", prog, path);
		return 0;
	}
	*n = fread(buf, 1, cap, f);
	if (*n == cap && fread(&extra, 1, 1, f) == 1)
		*n = cap + 1;
	if (ferror(f)) {
		fprintf(stderr, "%s: cannot read %s\n", prog, path);
		fclose(f);
		return 0;
	}
	fclose(f);
	return 1;
}
