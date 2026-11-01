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

static const char *const heart_beep_names[RANDO_HEART_BEEP_COUNT] = {
	[RANDO_HEART_BEEP_OFF] = "off",		[RANDO_HEART_BEEP_HALF] = "half",
	[RANDO_HEART_BEEP_QUARTER] = "quarter", [RANDO_HEART_BEEP_DOUBLE] = "double",
	[RANDO_HEART_BEEP_NORMAL] = "normal",
};

const char *rando_heart_beep_name(RandoHeartBeep heart_beep)
{
	assert((unsigned)heart_beep < RANDO_HEART_BEEP_COUNT);
	return heart_beep_names[heart_beep];
}

bool rando_heart_beep_find(const char *name, RandoHeartBeep *out)
{
	int i;

	for (i = 0; i < RANDO_HEART_BEEP_COUNT; i++)
		if (strcmp(name, heart_beep_names[i]) == 0) {
			*out = (RandoHeartBeep)i;
			return true;
		}
	return false;
}

static void write_heart_beep(RomOut *out, RandoHeartBeep heart_beep)
{
	static const uint8_t speeds[RANDO_HEART_BEEP_COUNT] = {
		[RANDO_HEART_BEEP_OFF] = 0x00,	  [RANDO_HEART_BEEP_HALF] = 0x40,
		[RANDO_HEART_BEEP_QUARTER] = 0x80, [RANDO_HEART_BEEP_DOUBLE] = 0x10,
		[RANDO_HEART_BEEP_NORMAL] = 0x20,
	};

	assert((unsigned)heart_beep < sizeof speeds);
	rando_rom_emit(out, 0x180033, &speeds[heart_beep], 1, "setHeartBeepSpeed");
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
	rando_rom_write_initial_sram(&out, gen);
	rando_rom_write_total_item_count(&out, gen);
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
