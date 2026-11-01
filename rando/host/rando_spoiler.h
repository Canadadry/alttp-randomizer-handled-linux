#ifndef RANDO_SPOILER_H
#define RANDO_SPOILER_H

#include "rando_options.h"
#include "rando_playthrough.h"
#include "rando_pool.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdio.h>

/* Writes VT's getSpoiler() for the world as it stands, in the oracle's normalized JSON (keys
 * sorted bytewise, PHP pretty-print, final newline). options are the case's, for meta. A NULL
 * playthrough writes no playthrough key. False on a stream error; the caller owns and closes out. */
bool rando_spoiler_write_json(FILE *out, const RandoWorld *world, const RandoOptions *options,
			      const RandoPlaythrough *playthrough);

/* Writes pool.txt: one line per item, dungeon, advancement, nice, then item pool, in list order, each
 * named as the world's region sections name a placed item. False on a stream error; the caller owns
 * and closes out. */
bool rando_spoiler_write_pool(FILE *out, const RandoWorld *world, const RandoPools *pools);

#endif
