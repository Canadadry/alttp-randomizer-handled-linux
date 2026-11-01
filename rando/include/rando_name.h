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
	/* "1CA10A", or "1CA10A-<n>" with 2 <= n <= RANDO_NAME_SUFFIX_MAX. */
	char code[RANDO_NAME_CODE_MAX];
	/* "<Mode>-<Goal>-<code>.sfc". */
	char rom[RANDO_NAME_ROM_MAX];
} RandoName;

/* File-name words, not UI text. Assert the range; static strings, never freed. */
const char *rando_name_mode(RandoOptionState state);
const char *rando_name_goal(RandoOptionGoal goal);

/* The 24-bit code: zlib CRC32 & 0xFFFFFF of the text "seed=<decimal>\n", then "<key>=<value>\n" for each
 * RandoOptionKey in enum order, then "hints=<on|off>\n" and "heart_beep=<name>\n". */
uint32_t rando_name_code(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed);

/* Builds the name of a 24-bit code for suffix 1 (none) or 2..RANDO_NAME_SUFFIX_MAX. Asserts both ranges. */
void rando_name_make(const RandoOptions *options, uint32_t code, int suffix, RandoName *out);

/* True when a ROM or a record already uses candidate. */
typedef bool (*RandoNameTakenFn)(void *ctx, const RandoName *candidate);

/* Tries suffix 1, 2, ... RANDO_NAME_SUFFIX_MAX in order, calling taken on each until one is free, and fills
 * out with it. False, out unspecified, when every suffix is taken. */
bool rando_name_choose(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed,
		       RandoNameTakenFn taken, void *ctx, RandoName *out);

/* True when code is 6 uppercase hex digits, optionally followed by "-<n>", n in 2..RANDO_NAME_SUFFIX_MAX
 * without a leading zero: the codes rando_name_make writes. */
bool rando_name_code_valid(const char *code);

#endif
