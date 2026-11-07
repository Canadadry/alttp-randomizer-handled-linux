#ifndef LADX_RANDOMIZER_H
#define LADX_RANDOMIZER_H

#include "ladx_itempool.h"
#include "ladx_logic.h"
#include "ladx_outcome.h"
#include "ladx_rng.h"
#include "ladx_settings.h"

#include <stddef.h>
#include <stdint.h>

/* Caller-owned; read through the accessors. */
typedef struct LadxRandomizer {
	LadxSettings settings;
	LadxRng rnd;
	LadxRng global;
	LadxWorldSetup ws;
	bool ws_done; /* WorldSetup.randomize returned */
	LadxLogic logic;
	LadxItemPool pool;	/* ItemPool's toDict() as built */
	bool pool_done;
	uint64_t words_before, words_after;
	LadxItemPool read_pool; /* what readItemPool returned */
	bool read_done;
	LadxItemPool placer_pool; /* the placer's _item_pool */
	int spots[LADX_LOGIC_SPOT_CAP]; /* the placer's _spots, spot indexes */
	int spot_count;
	LadxOutcome outcome;
} LadxRandomizer;

/*
 * LADXR's Randomizer.__init__ on validated settings and the seed's bytes,
 * every draw of both generators reported to sink (may be NULL) with ctx.
 * Ported so far: the seeding, the race draw, the world setup, the global
 * generator's copy, the logic, readItemPool and the placer's addItem calls.
 */
LadxRandomizeStatus ladx_randomize(LadxRandomizer *r, const LadxSettings *validated, const uint8_t *seed,
				   size_t seed_len, LadxRngSink sink, void *ctx);

const LadxOutcome *ladx_randomizer_outcome(const LadxRandomizer *r);
/* The world setup randomize gave; NULL when it did not return. */
const LadxWorldSetup *ladx_randomizer_world_setup(const LadxRandomizer *r);

/*
 * ItemPool's toDict() as built, with rnd's MT word counts when ItemPool was
 * entered and returned (either may be NULL); NULL when it did not return.
 */
const LadxItemPool *ladx_randomizer_pool(const LadxRandomizer *r, uint64_t *words_before, uint64_t *words_after);
/* readItemPool's result, zero and negative counts kept; NULL when it did not return. */
const LadxItemPool *ladx_randomizer_read_pool(const LadxRandomizer *r);

/*
 * worldSetup.py WorldSetup.randomize for tier 1 on validated settings,
 * patches/maze.py buildMaze included; pickEntrances draws nothing in tier 1.
 * OK with outcome UNFINISHED when it returned, or with a CRASH outcome
 * (buildMaze's IndexError or RuntimeError), ws then partly set.
 */
LadxRandomizeStatus ladx_world_setup_randomize(LadxWorldSetup *ws, const LadxSettings *validated, LadxRng *rnd,
					       LadxOutcome *outcome);

#endif
