#ifndef LADX_RANDOMIZER_H
#define LADX_RANDOMIZER_H

#include "ladx_logic.h"
#include "ladx_rng.h"
#include "ladx_settings.h"

#include <stddef.h>
#include <stdint.h>

typedef enum LadxOutcomeKind {
	/* The run stopped at the end of the stages ported so far: LADXR's outcome is not known yet. */
	LADX_OUTCOME_UNFINISHED = 0,
	LADX_OUTCOME_OK,
	LADX_OUTCOME_ERROR, /* LADXR's utils.Error */
	LADX_OUTCOME_CRASH  /* any other exception LADXR raises */
} LadxOutcomeKind;

/* type: the exception's type name, CRASH only, else NULL; message: static text, NULL when UNFINISHED or OK. */
typedef struct LadxOutcome {
	LadxOutcomeKind kind;
	const char *type;
	const char *message;
} LadxOutcome;

typedef enum LadxRandomizeStatus {
	LADX_RANDOMIZE_OK = 0,
	LADX_RANDOMIZE_UNSUPPORTED, /* a value outside tier 1 (goal=bingo*, scope cut goal-bingo) */
	LADX_RANDOMIZE_BROKEN	    /* a capacity overflow or a logic failure: no outcome, a C bug */
} LadxRandomizeStatus;

/* Caller-owned; read through the accessors. */
typedef struct LadxRandomizer {
	LadxSettings settings;
	LadxRng rnd;
	LadxRng global;
	LadxWorldSetup ws;
	bool ws_done; /* WorldSetup.randomize returned */
	LadxLogic logic;
	LadxOutcome outcome;
} LadxRandomizer;

/*
 * LADXR's Randomizer.__init__ on validated settings and the seed's bytes,
 * every draw of both generators reported to sink (may be NULL) with ctx.
 * Ported so far: the seeding, the race draw, the world setup, the global
 * generator's copy and the logic.
 */
LadxRandomizeStatus ladx_randomize(LadxRandomizer *r, const LadxSettings *validated, const uint8_t *seed,
				   size_t seed_len, LadxRngSink sink, void *ctx);

const LadxOutcome *ladx_randomizer_outcome(const LadxRandomizer *r);
/* The world setup randomize gave; NULL when it did not return. */
const LadxWorldSetup *ladx_randomizer_world_setup(const LadxRandomizer *r);

/*
 * worldSetup.py WorldSetup.randomize for tier 1 on validated settings,
 * patches/maze.py buildMaze included; pickEntrances draws nothing in tier 1.
 * OK with outcome UNFINISHED when it returned, or with a CRASH outcome
 * (buildMaze's IndexError or RuntimeError), ws then partly set.
 */
LadxRandomizeStatus ladx_world_setup_randomize(LadxWorldSetup *ws, const LadxSettings *validated, LadxRng *rnd,
					       LadxOutcome *outcome);

#endif
