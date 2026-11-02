#ifndef RANDO_NAME_H
#define RANDO_NAME_H

#include "rando_options.h"
#include "rando_rom.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_NAME_CODE_MAX 10
#define RANDO_NAME_ROM_MAX 48
#define RANDO_NAME_SUFFIX_MAX 99

typedef struct RandoName {
	char code[RANDO_NAME_CODE_MAX];
	char rom[RANDO_NAME_ROM_MAX];
} RandoName;

const char *rando_name_mode(RandoOptionState state);
const char *rando_name_goal(RandoOptionGoal goal);

uint32_t rando_name_code(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed);

void rando_name_make(const RandoOptions *options, uint32_t code, int suffix, RandoName *out);

typedef bool (*RandoNameTakenFn)(void *ctx, const RandoName *candidate);

bool rando_name_choose(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed,
		       RandoNameTakenFn taken, void *ctx, RandoName *out);

bool rando_name_code_valid(const char *code);

#endif
