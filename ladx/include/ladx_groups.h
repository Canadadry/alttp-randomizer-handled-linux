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

#endif
