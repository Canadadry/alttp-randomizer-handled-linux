#ifndef LADX_TEST_SYNTH_ROM_H
#define LADX_TEST_SYNTH_ROM_H

#include <stdint.h>

/*
 * A synthetic 1 MiB ROM in LADXR's ROMWithTables layout (romTables.py at
 * the pin, written out here, not taken from the library): pointer tables,
 * the texts' banks table, alt pointers and entries of every kind, every
 * byte drawn from a fixed PRNG. It records where it put each entry.
 */

#define SYNTH_ROM_SIZE 0x100000u
#define SYNTH_ROM_BANK 0x4000u
#define SYNTH_TABLES 11
#define SYNTH_ENTRY_MAX 3200

typedef struct SynthEntry {
	int table;	 /* ROMWithTables' order */
	uint32_t index;	 /* the index key, 0 for an alt */
	const char *alt; /* NULL for an index key */
	int integer;	 /* an integer pointer: pointer holds it, no span */
	uint32_t pointer;
	uint32_t start, stop;
	uint8_t bank;
	uint32_t pointer_at;
	uint8_t pointer_len;
	int32_t bank_at; /* -1: no bank byte */
	int entry_rebuilds; /* 0: planted so that its fields do not rebuild it */
	int bank_rebuilds;  /* 0: the bank byte has bits over 0x3F */
} SynthEntry;

typedef struct SynthRom {
	uint8_t rom[SYNTH_ROM_SIZE];
	SynthEntry entry[SYNTH_ENTRY_MAX]; /* per table, index keys ascending, then alt keys */
	int entry_count;
	int table_first[SYNTH_TABLES];
	int table_count[SYNTH_TABLES]; /* index and alt keys */
} SynthRom;

extern const char *const synth_table_names[SYNTH_TABLES];

void synth_rom_build(SynthRom *s, uint32_t seed);

/* The boss bytes of known_source.BOSSES. */
#define SYNTH_BOSS_ZERO (0x14u * SYNTH_ROM_BANK + 0x04E0u)
#define SYNTH_BOSS_ZERO_COUNT 64
#define SYNTH_BOSS_ITEM (0x3Eu * SYNTH_ROM_BANK + 0x3300u)
#define SYNTH_BOSS_ROOMS 9
extern const uint32_t synth_boss_rooms[SYNTH_BOSS_ROOMS];

#endif
