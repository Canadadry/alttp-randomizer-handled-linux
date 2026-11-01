#include "rando_rom.h"

#include <assert.h>

#define HEADER_ADDRESS 0x7FDCu
#define HEADER_SIZE 4u

bool rando_rom_update_checksum(RandoRomBuffer *rom)
{
	uint32_t sum;
	uint16_t checksum;
	uint16_t inverse;
	size_t i;
	assert(rom != NULL && rom->bytes != NULL);
	if (rom->size != RANDO_ROM_SIZE)
		return false;
	sum = 0x1FEu;
	for (i = 0; i < RANDO_ROM_SIZE; i++) {
		if (i >= HEADER_ADDRESS && i < HEADER_ADDRESS + HEADER_SIZE)
			continue;
		sum += rom->bytes[i];
	}
	checksum = (uint16_t)(sum & 0xFFFFu);
	inverse = (uint16_t)(checksum ^ 0xFFFFu);
	rom->bytes[HEADER_ADDRESS] = (uint8_t)(inverse & 0xFFu);
	rom->bytes[HEADER_ADDRESS + 1] = (uint8_t)(inverse >> 8);
	rom->bytes[HEADER_ADDRESS + 2] = (uint8_t)(checksum & 0xFFu);
	rom->bytes[HEADER_ADDRESS + 3] = (uint8_t)(checksum >> 8);
	return true;
}
