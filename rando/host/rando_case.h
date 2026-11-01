#ifndef RANDO_CASE_H
#define RANDO_CASE_H

#include "rando_options.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RANDO_CASE_ERROR_MAX 512

typedef struct RandoCase {
	RandoOptions options;
	uint32_t seed;
	bool has_attempts;
	uint32_t attempts;
} RandoCase;

/* Reads <case_dir>/config.json, seed and, when the file exists, attempts. On failure returns false
 * and writes into error a message that starts with the offending file's path. */
bool rando_case_read(const char *case_dir, RandoCase *out, char error[RANDO_CASE_ERROR_MAX]);

/* The oracle wrapper's config.json check, stricter: exactly one JSON object holding the 11 option
 * keys once each, string values without escapes. file only names the source in error; out is
 * unspecified on failure. */
bool rando_case_parse_config(const char *text, size_t len, const char *file, RandoOptions *out,
			     char error[RANDO_CASE_ERROR_MAX]);

/* `^[0-9]{1,10}\n?$`, at most 2^32 - 1, as the oracle wrapper reads seed. */
bool rando_case_parse_uint32(const char *text, size_t len, const char *file, uint32_t *out,
			     char error[RANDO_CASE_ERROR_MAX]);

#endif
