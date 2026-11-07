#ifndef RANDO_ROM_H
#define RANDO_ROM_H

#include "rando.h"
#include "rando_settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RANDO_ROM_SIZE 0x200000u
#define RANDO_ROM_SOURCE_SIZE 0x100000u

typedef bool (*RandoRomWriteFn)(void *ctx, uint32_t address, const uint8_t *bytes, size_t count,
				const char *method);

typedef struct RandoRomWriter {
	RandoRomWriteFn write;
	void *ctx;
} RandoRomWriter;

bool rando_rom_write(const RandoGen *gen, RandoHeartBeep heart_beep, const RandoRomWriter *writer);

typedef struct RandoRomBuffer {
	uint8_t *bytes;
	size_t size;
} RandoRomBuffer;

bool rando_rom_buffer_write(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method);

bool rando_rom_update_checksum(RandoRomBuffer *rom);

bool rando_rom_write_cosmetic_defaults(RandoRomBuffer *rom, const uint8_t *source, size_t source_size);

#endif
