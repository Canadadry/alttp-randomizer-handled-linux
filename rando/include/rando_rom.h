#ifndef RANDO_ROM_H
#define RANDO_ROM_H

#include "rando.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RandoHeartBeep {
	RANDO_HEART_BEEP_OFF,
	RANDO_HEART_BEEP_HALF,
	RANDO_HEART_BEEP_QUARTER,
	RANDO_HEART_BEEP_DOUBLE,
	RANDO_HEART_BEEP_NORMAL,
} RandoHeartBeep;

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

#endif
