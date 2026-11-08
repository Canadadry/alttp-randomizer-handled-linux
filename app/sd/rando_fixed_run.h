#ifndef RANDO_FIXED_RUN_H
#define RANDO_FIXED_RUN_H

#include "rando_app_shell.h"
#include "rando_ladx_name.h"
#include "rando_md5.h"
#include "rando_record.h"
#include "rando_settings.h"
#include "ui_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define RANDO_FIXED_KEYS_MAX 4096
#define RANDO_FIXED_REPEAT_MAX 999
#define RANDO_FIXED_TOKEN_MAX 32

typedef enum RandoFixedKeyStatus {
	RANDO_FIXED_KEY_OK,
	RANDO_FIXED_KEY_UNKNOWN,
	RANDO_FIXED_KEY_BAD_REPEAT,
	RANDO_FIXED_KEY_TOO_MANY,
} RandoFixedKeyStatus;

typedef struct RandoFixedKey {
	UiButton button;
	int count;
} RandoFixedKey;

typedef struct RandoFixedKeys {
	RandoFixedKey key[RANDO_FIXED_KEYS_MAX];
	int count;
	long presses;
} RandoFixedKeys;

typedef struct RandoFixedRun {
	bool has_seed, has_date, has_source_md5, has_base_md5, has_unsupported, has_ladx_seed;
	uint32_t seed;
	uint8_t ladx_seed[RANDO_LADX_SEED_BYTES];
	RandoRecordDate date;
	uint8_t source_md5[RANDO_MD5_SIZE], base_md5[RANDO_MD5_SIZE];
	RandoSettingPair unsupported;
} RandoFixedRun;

const char *rando_fixed_key_name(UiButton button);

/* <key> or <key>*<n>; on failure keys is unchanged. */
RandoFixedKeyStatus rando_fixed_keys_add(RandoFixedKeys *keys, const char *token);

/* Whitespace-separated tokens up to EOF; on failure bad holds the offending token, cut to
   RANDO_FIXED_TOKEN_MAX characters, and the keys before it stay added. */
RandoFixedKeyStatus rando_fixed_keys_read(RandoFixedKeys *keys, FILE *in, char bad[RANDO_FIXED_TOKEN_MAX + 1]);

/* A positional key argument: "-" reads in, anything else is one token. On failure *bad points
   to arg itself or, for a token read from in, to buf. */
RandoFixedKeyStatus rando_fixed_keys_arg(RandoFixedKeys *keys, const char *arg, FILE *in,
					 char buf[RANDO_FIXED_TOKEN_MAX + 1], const char **bad);

bool rando_fixed_run_takes(const char *option);

/* option must be one rando_fixed_run_takes accepts; false on a bad value, run unchanged. */
bool rando_fixed_run_option(RandoFixedRun *run, const char *option, const char *value);

/* The --unsupported pair, a test-only override of the library's list; NULL when not given. It points
   into run, so run must outlive the shell it is handed to. */
const RandoSettingPair *rando_fixed_run_unsupported(const RandoFixedRun *run);

/* The fixed seeds and date live in this module's statics: one fixed run per process. */
void rando_fixed_run_apply(const RandoFixedRun *run, RandoAppShell *shell);

#endif
