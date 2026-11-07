#ifndef LADX_GROUPS_H
#define LADX_GROUPS_H

#include "ladx_pipeline.h"

/*
 * The ported C groups of generateRom (notes/ladx-inventory.md's Groups),
 * each a LadxPipelineRun registered by ladx_pipeline_groups
 * (tests/ladx/README.md, "Pipeline").
 */

/*
 * patches/bank3e.py addBank3E: data/ladx/bank3e.txt's constants, text
 * 0xBE as a copy of 0x111, seed[:0x20] at bank 0x3E 0x2F00 and, unless
 * race, the short settings string's length and bytes at 0x2F20. Needs
 * p->rom and the randomizer.
 */
int ladx_group_bank3e(LadxEncoder *enc, LadxPipeline *p);

/*
 * patches/titleScreen.py setRomInfo: bank 0 offset 7 (0x01 on race, else
 * 0x52), then title backgrounds 3 and 4, tiles then attributes, each
 * decoded, edited by ladx_title_edit and re-encoded, with no origin. The
 * text is the seed's upper-case hex, or "Race". Needs p->rom and the
 * randomizer; 0 on a malformed background.
 */
int ladx_group_title(LadxEncoder *enc, LadxPipeline *p);

/* titleScreen._encode: each of text's n bytes as a title tile into out. */
void ladx_title_encode_text(const char *text, size_t n, uint8_t *out);

/*
 * setRomInfo's seednr modulo LADX_TITLE_SEED_MODULUS (all its pattern
 * reads): text's hex digits when n > 0 and all are, else the MD5 of text,
 * each byte over 0x7F made '?'. Python int()'s sign, spaces, 0x and
 * underscores are not read.
 */
#define LADX_TITLE_SEED_MODULUS 17623416832ull /* 112 ** 5 */
uint64_t ladx_title_seed_number(const char *text, size_t n);

/*
 * setRomInfo's edits of one background (attributes: its attribute map):
 * text's two lines of 16, the guards removed, the seed number's pattern.
 * 0 when text is over 64 bytes.
 */
int ladx_title_edit(LadxBackground *bg, int attributes, const char *text, size_t n, uint64_t seed_number);

#endif
