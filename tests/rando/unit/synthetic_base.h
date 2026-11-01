#ifndef RANDO_TEST_SYNTHETIC_BASE_H
#define RANDO_TEST_SYNTHETIC_BASE_H

#include "rando_md5.h"

#include <stddef.h>
#include <stdint.h>

#define SYNTH_SOURCE_SIZE 0x100000u
#define SYNTH_ROM_SIZE 0x200000u
#define SYNTH_PATCH_MAX 0x10000u

typedef struct SynthPatch {
	uint8_t bytes[SYNTH_PATCH_MAX];
	size_t size;
} SynthPatch;

/* Filled by synth_build. */
extern uint8_t synth_source[SYNTH_SOURCE_SIZE];
extern uint8_t synth_base[SYNTH_ROM_SIZE];
extern uint8_t synth_sprite_base[SYNTH_ROM_SIZE];
extern SynthPatch synth_good_patch, synth_sprite_patch;
extern uint8_t synth_source_md5[RANDO_MD5_SIZE], synth_base_md5[RANDO_MD5_SIZE],
	synth_sprite_base_md5[RANDO_MD5_SIZE];

/* A synthetic 1 MiB source and two BPS patches building 2 MiB base images from it: synth_base (the
 * source with a version byte, then a filler with the two `02 37` author guards) and synth_sprite_base
 * (the same with distinct sprite bytes), with every digest. */
void synth_build(void);

/* A valid BPS header and footer with no action. */
void synth_header_only(SynthPatch *p, size_t source_size, size_t target_size);

#endif
