#ifndef LADX_INPUTS_H
#define LADX_INPUTS_H

#include "ladx_settings.h"
#include "ladx_tables.h"

#include <stddef.h>
#include <stdint.h>

/*
 * The host mains' inputs: a case directory, a tables directory, a whole
 * file. Every function prints its own failure to stderr, prefixed
 * "<prog>: ", and returns 0 then.
 */

#define LADX_INPUTS_SEED_BYTES 16
/* data/ladx/ in all: 1,394,493 bytes today. */
#define LADX_INPUTS_TABLES_TEXT_MAX (2 * 1024 * 1024)
#define LADX_INPUTS_NAME_MAX 64

/* <dir>/config.txt applied over the defaults and validated, <dir>/seed read, the settings support-checked. */
int ladx_inputs_case(const char *prog, const char *dir, LadxSettings *settings,
		     uint8_t seed[LADX_INPUTS_SEED_BYTES]);

/* 32 lowercase hex digits into seed; 0 for anything else (no message). */
int ladx_inputs_seed_hex(const char *text, size_t len, uint8_t seed[LADX_INPUTS_SEED_BYTES]);

/* Caller-owned (about 2.2 MiB): the tables' text and the index over it. */
typedef struct LadxTablesDir {
	char text[LADX_INPUTS_TABLES_TEXT_MAX];
	size_t len;
	char names[LADX_TABLES_GROUP_MAX][LADX_INPUTS_NAME_MAX];
	LadxTables tables;
} LadxTablesDir;

/* order.txt and every other .txt file of dir, read whole and indexed into d->tables. */
int ladx_inputs_tables(const char *prog, const char *dir, LadxTablesDir *d);

/*
 * The file at path into buf (cap bytes): 1 with *n its size, cap + 1 when
 * it is longer (buf then holds its first cap bytes); 0 when it cannot be
 * opened (*n 0, no message when quiet) or read.
 */
int ladx_inputs_file(const char *prog, const char *path, uint8_t *buf, size_t cap, size_t *n, int quiet);

#endif
