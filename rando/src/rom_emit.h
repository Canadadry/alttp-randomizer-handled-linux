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

void rando_rom_emit(RomOut *out, uint32_t address, const uint8_t *bytes, size_t count, const char *method);

void rando_rom_write_items(RomOut *out, const RandoGen *gen);

uint8_t rando_rom_item_byte(RandoItemId item);

void rando_rom_write_settings(RomOut *out, const RandoGen *gen);

void rando_rom_write_credits(RomOut *out, const RandoGen *gen);

void rando_rom_write_text(RomOut *out, const RandoGen *gen);

void rando_rom_write_initial_sram(RomOut *out, const RandoGen *gen);

void rando_rom_write_total_item_count(RomOut *out, const RandoGen *gen);

#endif
