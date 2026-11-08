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

#define LADX_PLACER_OPTION_CAP 512
#define LADX_PLACER_SET_WORDS ((LADX_ITEM_COUNT + 31) / 32)

typedef struct LadxPlacer {
	LadxLogic *logic;
	bool beaks_first, keys_first, nightmare_keys_first, goal_only;
	LadxItemPool pool;
	int spots[LADX_LOGIC_SPOT_CAP];
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

void ladx_placer_init(LadxPlacer *p, LadxLogic *logic, const LadxSettings *validated);
bool ladx_placer_add_item(LadxPlacer *p, int item, int count);
bool ladx_placer_add_spot(LadxPlacer *p, int spot);
LadxRandomizeStatus ladx_placer_run(LadxPlacer *p, LadxRng *rnd, LadxOutcome *outcome);

#define LADX_RANDOMIZER_SEED_MAX 32

typedef struct LadxRandomizer {
	LadxSettings settings;
	LadxRng rnd;
	LadxRng global;
	LadxWorldSetup ws;
	bool ws_done;
	LadxLogic logic;
	LadxItemPool pool;
	bool pool_done;
	uint64_t words_before, words_after;
	LadxItemPool read_pool;
	bool read_done;
	LadxPlacer placer;
	LadxOutcome outcome;
	uint8_t seed[LADX_RANDOMIZER_SEED_MAX];
	size_t seed_len;
} LadxRandomizer;

LadxRandomizeStatus ladx_randomize(LadxRandomizer *r, const LadxSettings *validated, const uint8_t *seed,
				   size_t seed_len, LadxRngSink sink, void *ctx);

const LadxOutcome *ladx_randomizer_outcome(const LadxRandomizer *r);
const uint8_t *ladx_randomizer_seed(const LadxRandomizer *r, size_t *n);
const LadxWorldSetup *ladx_randomizer_world_setup(const LadxRandomizer *r);

const LadxItemPool *ladx_randomizer_pool(const LadxRandomizer *r, uint64_t *words_before, uint64_t *words_after);
const LadxItemPool *ladx_randomizer_read_pool(const LadxRandomizer *r);
const LadxLogic *ladx_randomizer_placement(const LadxRandomizer *r);
const LadxRng *ladx_randomizer_rng(const LadxRandomizer *r, LadxRngGen gen);

LadxRandomizeStatus ladx_world_setup_randomize(LadxWorldSetup *ws, const LadxSettings *validated, LadxRng *rnd,
					       LadxOutcome *outcome);

#endif
