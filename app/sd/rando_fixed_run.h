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

RandoFixedKeyStatus rando_fixed_keys_add(RandoFixedKeys *keys, const char *token);

RandoFixedKeyStatus rando_fixed_keys_read(RandoFixedKeys *keys, FILE *in, char bad[RANDO_FIXED_TOKEN_MAX + 1]);

RandoFixedKeyStatus rando_fixed_keys_arg(RandoFixedKeys *keys, const char *arg, FILE *in,
					 char buf[RANDO_FIXED_TOKEN_MAX + 1], const char **bad);

bool rando_fixed_run_takes(const char *option);

bool rando_fixed_run_option(RandoFixedRun *run, const char *option, const char *value);

const RandoSettingPair *rando_fixed_run_unsupported(const RandoFixedRun *run);

void rando_fixed_run_apply(const RandoFixedRun *run, RandoAppShell *shell);

#endif
