#ifndef RANDO_LADX_NAME_H
#define RANDO_LADX_NAME_H

#include "ladx_settings.h"
#include "rando_name.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_LADX_SEED_BYTES 16
#define RANDO_LADX_SEED_HEX (2 * RANDO_LADX_SEED_BYTES)

const char *rando_ladx_name_logic(int logic);
const char *rando_ladx_name_goal(int goal);

/* The low 24 bits of the CRC-32 of "seed=<seed_hex>\n" then "<key>=<value>\n" per setting, table order. */
uint32_t rando_ladx_name_code(const LadxSettings *settings, const char *seed_hex);

/* "<Logic>-<Goal>-<CODE>[-<suffix>].gbc"; the code part as rando_name_make's. */
void rando_ladx_name_make(const LadxSettings *settings, uint32_t code, int suffix, RandoName *out);

/* The first name, suffix 1 to RANDO_NAME_SUFFIX_MAX, that taken refuses; false when all are taken. */
bool rando_ladx_name_choose(const LadxSettings *settings, const char *seed_hex, RandoNameTakenFn taken, void *ctx,
			    RandoName *out);

#endif
