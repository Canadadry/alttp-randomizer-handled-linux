#include "rando_rom.h"
#include "rom_emit.h"

#include <assert.h>
#include <string.h>

void rando_rom_emit(RomOut *out, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	assert(count > 0);
	if (out->ok)
		out->ok = out->writer->write(out->writer->ctx, address, bytes, count, method);
}

/* TODO step 10. */
static void write_initial_sram(RomOut *out, const RandoGen *gen)
{
	(void)out;
	(void)gen;
}

/* TODO step 10. */
static void write_total_item_count(RomOut *out, const RandoGen *gen)
{
	(void)out;
	(void)gen;
}

/* TODO step 11. */
static void write_heart_beep(RomOut *out, RandoHeartBeep heart_beep)
{
	(void)out;
	(void)heart_beep;
}

bool rando_rom_write(const RandoGen *gen, RandoHeartBeep heart_beep, const RandoRomWriter *writer)
{
	RomOut out;

	assert(gen != NULL && writer != NULL && writer->write != NULL);
	out.writer = writer;
	out.ok = true;
	rando_rom_write_items(&out, gen);
	rando_rom_write_settings(&out, gen);
	rando_rom_write_credits(&out, gen);
	rando_rom_write_text(&out, gen);
	write_initial_sram(&out, gen);
	write_total_item_count(&out, gen);
	write_heart_beep(&out, heart_beep);
	return out.ok;
}

bool rando_rom_buffer_write(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	RandoRomBuffer *rom = ctx;

	(void)method;
	/* Compared as size - address, never address + count, which could wrap. */
	if (address > rom->size || count > rom->size - address)
		return false;
	memcpy(rom->bytes + address, bytes, count);
	return true;
}
