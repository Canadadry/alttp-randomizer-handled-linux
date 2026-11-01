#ifndef RANDO_ROM_H
#define RANDO_ROM_H

#include "rando.h"
#include "rando_settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* VT's Rom::SIZE, the randomized image, and the headerless JP 1.0 ROM it is built from. */
#define RANDO_ROM_SIZE 0x200000u
#define RANDO_ROM_SOURCE_SIZE 0x100000u

/* One write of count bytes (never 0) at a PC address. method is the oracle's tag for the write, a
 * static string the module owns: never freed. Returning false refuses the write. */
typedef bool (*RandoRomWriteFn)(void *ctx, uint32_t address, const uint8_t *bytes, size_t count,
				const char *method);

typedef struct RandoRomWriter {
	RandoRomWriteFn write;
	void *ctx;
} RandoRomWriter;

/* gen must come from a RANDO_COMPLETE generation; it is only read, and nothing is drawn. Writes VT's
 * writeToRom groups in order (items, settings, credits, text, initial SRAM, setTotalItemCount, heart
 * beep), each group's writes in VT's call order. Returns false at the first refused write, after
 * which nothing more is written; true when every write was accepted. */
bool rando_rom_write(const RandoGen *gen, RandoHeartBeep heart_beep, const RandoRomWriter *writer);

/* A caller-owned ROM image. */
typedef struct RandoRomBuffer {
	uint8_t *bytes;
	size_t size;
} RandoRomBuffer;

/* A RandoRomWriteFn; ctx is the RandoRomBuffer. Copies the bytes when address + count <= size,
 * otherwise refuses the write and copies nothing. */
bool rando_rom_buffer_write(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method);

/* VT's Rom::updateChecksum: writes the inverse then the checksum, little-endian, at 0x7FDC. Returns
 * false and writes nothing unless rom->size is RANDO_ROM_SIZE. */
bool rando_rom_update_checksum(RandoRomBuffer *rom);

/* alttpr.com's browser writes at VT's defaults, in step 1.2's order: quickswap, music, flashing,
 * menu speed, red hearts, then the default Link sprite copied from source (the JP ROM, header
 * stripped, only read, not overlapping rom). Returns false and writes nothing unless rom->size is
 * RANDO_ROM_SIZE and source_size is RANDO_ROM_SOURCE_SIZE. */
bool rando_rom_write_cosmetic_defaults(RandoRomBuffer *rom, const uint8_t *source, size_t source_size);

#endif
