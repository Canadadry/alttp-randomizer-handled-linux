#ifndef LADX_RANDOMIZER_H
#define LADX_RANDOMIZER_H

#include "ladx_itempool.h"
#include "ladx_logic.h"
#include "ladx_outcome.h"
#include "ladx_rng.h"
#include "ladx_settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The longest getOptions() list the placer reads. */
#define LADX_PLACER_OPTION_CAP 512
#define LADX_PLACER_SET_WORDS ((LADX_ITEM_COUNT + 31) / 32)

/*
 * randomizer.py RandomItemPlacer over a finished logic it does not own,
 * with Python's list semantics for its spots. Caller-owned; the fields
 * past spot_count are its scratch.
 */
typedef struct LadxPlacer {
	LadxLogic *logic;
	bool beaks_first, keys_first, nightmare_keys_first, goal_only;
	LadxItemPool pool; /* _item_pool */
	int spots[LADX_LOGIC_SPOT_CAP]; /* _spots, spot indexes */
	int spot_count;
	bool broken;
	LadxExplorer explorer;
	uint32_t option_set[LADX_LOGIC_SPOT_CAP][LADX_PLACER_SET_WORDS];
	int option_count[LADX_LOGIC_SPOT_CAP];
	int name_rank[LADX_LOGIC_SPOT_CAP];
	int options[LADX_PLACER_OPTION_CAP];
	int dungeon_keys[LADX_RNG_MAX_LIST];
	int dungeon_key_count;
	int sorted[LADX_LOGIC_SPOT_CAP];
	unsigned long sort_key[LADX_LOGIC_SPOT_CAP];
	int pick[LADX_LOGIC_SPOT_CAP];
	int priority[LADX_ITEM_COUNT];
	int item_spot_count[LADX_ITEM_COUNT];
	int remaining[LADX_ITEM_COUNT];
} LadxPlacer;

/*
 * RandomItemPlacer(logic, ...) with validated settings' dungeon_keys,
 * nightmare_keys, dungeon_beaks, owlstatues and accessibility: an empty pool
 * and spot list. The logic must stay alive and finished while p is used.
 */
void ladx_placer_init(LadxPlacer *p, LadxLogic *logic, const LadxSettings *validated);
/* addItem(item, count); false, nothing done, for an item outside the table. */
bool ladx_placer_add_item(LadxPlacer *p, int item, int count);
/* addSpot(spot); false, nothing done, for a bad spot or a full list. */
bool ladx_placer_add_spot(LadxPlacer *p, int spot);
/*
 * run(rnd): OK with outcome OK when every item is placed (spots' items set
 * in the logic), else OK with LADXR's Error or exception as the outcome;
 * BROKEN on a capacity overflow, the outcome then UNFINISHED.
 */
LadxRandomizeStatus ladx_placer_run(LadxPlacer *p, LadxRng *rnd, LadxOutcome *outcome);

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
	LadxPlacer placer;
	LadxOutcome outcome;
} LadxRandomizer;

/*
 * LADXR's Randomizer.__init__ on validated settings and the seed's bytes,
 * every draw of both generators reported to sink (may be NULL) with ctx.
 * up to the end of RandomItemPlacer.run (forwardfactor 0, no plan).
 * OK with LADXR's outcome (ok, its Error or the exception it raises), or
 * UNSUPPORTED or BROKEN with no outcome. It does not check the settings'
 * support: the pair LADXR's fill refuses gives LADXR's AssertionError.
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
/* The logic with every spot's item placed; NULL unless the outcome is OK. */
const LadxLogic *ladx_randomizer_placement(const LadxRandomizer *r);
/* The generator's state where the run stopped (rnd, or the global copy once made). */
const LadxRng *ladx_randomizer_rng(const LadxRandomizer *r, LadxRngGen gen);

/*
 * worldSetup.py WorldSetup.randomize for tier 1 on validated settings,
 * patches/maze.py buildMaze included; pickEntrances draws nothing in tier 1.
 * OK with outcome UNFINISHED when it returned, or with a CRASH outcome
 * (buildMaze's IndexError or RuntimeError), ws then partly set.
 */
LadxRandomizeStatus ladx_world_setup_randomize(LadxWorldSetup *ws, const LadxSettings *validated, LadxRng *rnd,
					       LadxOutcome *outcome);

#endif
