#ifndef RANDO_ROM_EMIT_H
#define RANDO_ROM_EMIT_H

#include "rando.h"
#include "rando_rom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RomOut {
	const RandoRomWriter *writer;
	bool ok;
} RomOut;

/* Every group writes through here, so nothing is written after a refused write. count is never 0. */
void rando_rom_emit(RomOut *out, uint32_t address, const uint8_t *bytes, size_t count, const char *method);

/* VT's World::writeToRom item loop (writeItem). */
void rando_rom_write_items(RomOut *out, const RandoGen *gen);

#endif
