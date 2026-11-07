#include "ladx_randomizer.h"
#include "randomizer_internal.h"

#include <stdio.h>

#define I(name) LADX_ITEM_##name

void ladx_outcome_key_error(LadxOutcome *outcome, int item)
{
	snprintf(outcome->text, sizeof outcome->text, "'%s'", ladx_item_name(item));
	outcome->kind = LADX_OUTCOME_CRASH;
	outcome->type = "KeyError";
	outcome->message = outcome->text;
}

/* Randomizer.readItemPool with no plan: false on a KeyError outcome (or a full spot list, the logic then checked). */
static bool read_item_pool(LadxRandomizer *r)
{
	static const int fillers[] = {I(RUPEES_50), I(RUPEES_20), I(RUPEES_200), I(MESSAGE), I(MEDICINE), I(GEL),
				      I(SEASHELL)};
	LadxItemPool *pool = &r->read_pool;
	int need_to_remove = 0, i, item, count, first;
	size_t f;

	*pool = r->pool;
	for (i = 0; i < ladx_logic_spot_count(&r->logic); i++) {
		const LadxSpot *spot = ladx_logic_spot(&r->logic, i);

		if (spot->forced_item != LADX_ITEM_NONE) {
			ladx_itempool_set(pool, spot->forced_item, ladx_itempool_get(pool, spot->forced_item) - 1);
			ladx_logic_place(&r->logic, i, spot->forced_item);
		} else if (ladx_logic_spot_options(&r->logic, i, &first, 1) == 1) {
			if (!ladx_itempool_contains(pool, first)) {
				ladx_outcome_key_error(&r->outcome, first);
				return false;
			}
			ladx_itempool_set(pool, first, ladx_itempool_get(pool, first) - 1);
			ladx_logic_place(&r->logic, i, first);
		} else {
			if (!ladx_placer_add_spot(&r->placer, i))
				return false;
			ladx_logic_place(&r->logic, i, LADX_ITEM_NONE);
		}
	}
	for (i = 0; ladx_itempool_entry(pool, i, &item, &count); i++)
		if (count < 0)
			need_to_remove -= count;
	for (f = 0; f < sizeof fillers / sizeof fillers[0]; f++) {
		int have = ladx_itempool_get(pool, fillers[f]);
		int remove = need_to_remove < have ? need_to_remove : have;

		if (remove) {
			ladx_itempool_set(pool, fillers[f], have - remove);
			need_to_remove -= remove;
		}
	}
	return true;
}

static LadxRandomizeStatus logic_status(LadxLogicStatus status)
{
	switch (status) {
	case LADX_LOGIC_OK:
		return LADX_RANDOMIZE_OK;
	case LADX_LOGIC_UNSUPPORTED:
		return LADX_RANDOMIZE_UNSUPPORTED;
	default:
		return LADX_RANDOMIZE_BROKEN;
	}
}

LadxRandomizeStatus ladx_randomize(LadxRandomizer *r, const LadxSettings *validated, const uint8_t *seed,
				   size_t seed_len, LadxRngSink sink, void *ctx)
{
	LadxRandomizeStatus status;
	int i, item, count;

	r->settings = *validated;
	r->outcome.kind = LADX_OUTCOME_UNFINISHED;
	r->outcome.type = NULL;
	r->outcome.message = NULL;
	ladx_world_setup_defaults(&r->ws);
	r->ws_done = false;
	r->pool_done = false;
	r->read_done = false;
	ladx_rng_init(&r->rnd, LADX_RNG_RND, sink, ctx);
	ladx_rng_init(&r->global, LADX_RNG_GLOBAL, sink, ctx);
	ladx_rng_seed(&r->rnd, LADX_STAGE_RANDOMIZER_INIT, seed, seed_len);
	if (r->settings.value[LADX_SETTING_RACE])
		ladx_rng_random(&r->rnd, LADX_STAGE_RANDOMIZER_INIT);
	status = ladx_world_setup_randomize(&r->ws, &r->settings, &r->rnd, &r->outcome);
	if (status != LADX_RANDOMIZE_OK || r->outcome.kind != LADX_OUTCOME_UNFINISHED)
		return status;
	r->ws_done = true;
	ladx_rng_copy_state(&r->global, &r->rnd, LADX_STAGE_RANDOMIZER_INIT);
	status = logic_status(ladx_logic_build(&r->logic, &r->settings, &r->ws));
	if (status != LADX_RANDOMIZE_OK)
		return status;

	r->words_before = r->rnd.py.words;
	status = ladx_itempool_build(&r->pool, &r->settings, &r->rnd, &r->outcome);
	if (status != LADX_RANDOMIZE_OK || r->outcome.kind != LADX_OUTCOME_UNFINISHED)
		return status;
	r->words_after = r->rnd.py.words;
	r->pool_done = true;
	ladx_placer_init(&r->placer, &r->logic, &r->settings);
	if (!read_item_pool(r))
		return r->outcome.kind == LADX_OUTCOME_UNFINISHED ? LADX_RANDOMIZE_BROKEN : LADX_RANDOMIZE_OK;
	if (ladx_logic_broken(&r->logic))
		return LADX_RANDOMIZE_BROKEN;
	r->read_done = true;
	for (i = 0; ladx_itempool_entry(&r->read_pool, i, &item, &count); i++)
		if (count > 0)
			ladx_placer_add_item(&r->placer, item, count);
	return ladx_placer_run(&r->placer, &r->rnd, &r->outcome);
}

const LadxOutcome *ladx_randomizer_outcome(const LadxRandomizer *r)
{
	return &r->outcome;
}

const LadxWorldSetup *ladx_randomizer_world_setup(const LadxRandomizer *r)
{
	return r->ws_done ? &r->ws : NULL;
}

const LadxItemPool *ladx_randomizer_pool(const LadxRandomizer *r, uint64_t *words_before, uint64_t *words_after)
{
	if (!r->pool_done)
		return NULL;
	if (words_before)
		*words_before = r->words_before;
	if (words_after)
		*words_after = r->words_after;
	return &r->pool;
}

const LadxItemPool *ladx_randomizer_read_pool(const LadxRandomizer *r)
{
	return r->read_done ? &r->read_pool : NULL;
}

const LadxLogic *ladx_randomizer_placement(const LadxRandomizer *r)
{
	return r->outcome.kind == LADX_OUTCOME_OK ? &r->logic : NULL;
}

const LadxRng *ladx_randomizer_rng(const LadxRandomizer *r, LadxRngGen gen)
{
	return gen == LADX_RNG_GLOBAL ? &r->global : &r->rnd;
}
