#ifndef RANDO_SPOILER_H
#define RANDO_SPOILER_H

#include "rando_options.h"
#include "rando_playthrough.h"
#include "rando_pool.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdio.h>

bool rando_spoiler_write_json(FILE *out, const RandoWorld *world, const RandoOptions *options,
			      const RandoPlaythrough *playthrough);

bool rando_spoiler_write_pool(FILE *out, const RandoWorld *world, const RandoPools *pools);

#endif
