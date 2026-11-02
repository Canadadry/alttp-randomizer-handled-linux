#ifndef RANDO_CASE_H
#define RANDO_CASE_H

#include "rando_options.h"
#include "rando_rom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RANDO_CASE_ERROR_MAX 512

typedef struct RandoCase {
	RandoOptions options;
	RandoHeartBeep heart_beep;
	uint32_t seed;
	bool has_attempts;
	uint32_t attempts;
} RandoCase;

bool rando_case_read(const char *case_dir, RandoCase *out, char error[RANDO_CASE_ERROR_MAX]);

bool rando_case_parse_config(const char *text, size_t len, const char *file, RandoCase *out,
			     char error[RANDO_CASE_ERROR_MAX]);

bool rando_case_parse_uint32(const char *text, size_t len, const char *file, uint32_t *out,
			     char error[RANDO_CASE_ERROR_MAX]);

#endif
