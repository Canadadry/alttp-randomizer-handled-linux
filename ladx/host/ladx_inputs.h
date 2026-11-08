#ifndef LADX_INPUTS_H
#define LADX_INPUTS_H

#include "ladx_settings.h"
#include "ladx_tables.h"

#include <stddef.h>
#include <stdint.h>

#define LADX_INPUTS_SEED_BYTES 16
#define LADX_INPUTS_TABLES_TEXT_MAX (2 * 1024 * 1024)
#define LADX_INPUTS_NAME_MAX 64

int ladx_inputs_case(const char *prog, const char *dir, LadxSettings *settings,
		     uint8_t seed[LADX_INPUTS_SEED_BYTES]);

int ladx_inputs_seed_hex(const char *text, size_t len, uint8_t seed[LADX_INPUTS_SEED_BYTES]);

typedef struct LadxTablesDir {
	char text[LADX_INPUTS_TABLES_TEXT_MAX];
	size_t len;
	char names[LADX_TABLES_GROUP_MAX][LADX_INPUTS_NAME_MAX];
	LadxTables tables;
} LadxTablesDir;

int ladx_inputs_tables(const char *prog, const char *dir, LadxTablesDir *d);

int ladx_inputs_file(const char *prog, const char *path, uint8_t *buf, size_t cap, size_t *n, int quiet);

#endif
