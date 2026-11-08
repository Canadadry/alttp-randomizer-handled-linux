#ifndef LADX_TEST_SYNTH_ROM_H
#define LADX_TEST_SYNTH_ROM_H

#include <stdint.h>

#define SYNTH_ROM_SIZE 0x100000u
#define SYNTH_ROM_BANK 0x4000u
#define SYNTH_TABLES 11
#define SYNTH_ENTRY_MAX 3200

typedef struct SynthEntry {
	int table;
	uint32_t index;
	const char *alt;
	int integer;
	uint32_t pointer;
	uint32_t start, stop;
	uint8_t bank;
	uint32_t pointer_at;
	uint8_t pointer_len;
	int32_t bank_at;
	int entry_rebuilds;
	int bank_rebuilds;
} SynthEntry;

typedef struct SynthRom {
	uint8_t rom[SYNTH_ROM_SIZE];
	SynthEntry entry[SYNTH_ENTRY_MAX];
	int entry_count;
	int table_first[SYNTH_TABLES];
	int table_count[SYNTH_TABLES];
} SynthRom;

extern const char *const synth_table_names[SYNTH_TABLES];

void synth_rom_build(SynthRom *s, uint32_t seed);

#define SYNTH_BOSS_ZERO (0x14u * SYNTH_ROM_BANK + 0x04E0u)
#define SYNTH_BOSS_ZERO_COUNT 64
#define SYNTH_BOSS_ITEM (0x3Eu * SYNTH_ROM_BANK + 0x3300u)
#define SYNTH_BOSS_ROOMS 9
extern const uint32_t synth_boss_rooms[SYNTH_BOSS_ROOMS];

#endif
