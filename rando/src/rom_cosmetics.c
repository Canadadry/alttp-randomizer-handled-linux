#include "rando_rom.h"

#include <assert.h>
#include <string.h>

#define VERSION_ADDRESS 0x7FE2u
#define GFX_ADDRESS 0x80000u
#define GFX_SIZE 0x7000u
#define PALETTE_ADDRESS 0xDD308u
#define PALETTE_SIZE 120u
#define GLOVES_ADDRESS 0xDEDF5u
#define GLOVES_SIZE 4u
#define AUTHOR_CELLS 28u
#define AUTHOR_HI_ADDRESS 0x118002u
#define AUTHOR_LO_ADDRESS 0x118020u

/* setHeartColor red. VT reads the base-patched ROM's own version byte, and its old branch also
 * writes 0x65561. */
static void write_heart_color(uint8_t *bytes)
{
	uint8_t version = bytes[VERSION_ADDRESS];
	uint32_t address;
	if (version <= 0x03 || version == 0xFF) {
		for (address = 0x6FA1E; address <= 0x6FA30; address += 2)
			bytes[address] = 0x24;
		bytes[0x65561] = 0x05;
	} else {
		bytes[0x187020] = 0x00;
	}
}

/* center-align 1.0.1 pads both sides to 28 cells: a left-only pad would leave ten cells unwritten. */
static const uint8_t author_hi[AUTHOR_CELLS] = {
	0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x6A, 0x65, 0x6A, 0x70,
	0x61, 0x6A, 0x60, 0x6B, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F,
};
static const uint8_t author_lo[AUTHOR_CELLS] = {
	0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x90, 0x8B, 0x90, 0x96,
	0x87, 0x90, 0x86, 0x91, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F,
};

/* The default Link sprite is the JP ROM's own bytes, copied back over the base patch's. VT
 * writes the author only over the credits line headers it expects, never over an older build. */
static void write_default_sprite(uint8_t *bytes, const uint8_t *source)
{
	if (bytes[0x118000] == 0x02 && bytes[0x118001] == 0x37 && bytes[0x11801E] == 0x02 &&
	    bytes[0x11801F] == 0x37) {
		memcpy(bytes + AUTHOR_HI_ADDRESS, author_hi, AUTHOR_CELLS);
		memcpy(bytes + AUTHOR_LO_ADDRESS, author_lo, AUTHOR_CELLS);
	}
	memcpy(bytes + GFX_ADDRESS, source + GFX_ADDRESS, GFX_SIZE);
	memcpy(bytes + PALETTE_ADDRESS, source + PALETTE_ADDRESS, PALETTE_SIZE);
	memcpy(bytes + GLOVES_ADDRESS, source + GLOVES_ADDRESS, GLOVES_SIZE);
}

bool rando_rom_write_cosmetic_defaults(RandoRomBuffer *rom, const uint8_t *source, size_t source_size)
{
	uint8_t *bytes;
	assert(rom != NULL && rom->bytes != NULL && source != NULL);
	if (rom->size != RANDO_ROM_SIZE || source_size != RANDO_ROM_SOURCE_SIZE)
		return false;
	bytes = rom->bytes;
	bytes[0x18004B] = 0x00;
	/* setMusicVolume's flag is inverted: music on writes 0x00. */
	bytes[0x18021A] = 0x00;
	bytes[0x18017F] = 0x00;
	bytes[0x180048] = 0x08;
	bytes[0x6DD9A] = 0x11;
	bytes[0x6DF2A] = 0x12;
	bytes[0x6E0E9] = 0x12;
	write_heart_color(bytes);
	write_default_sprite(bytes, source);
	return true;
}
