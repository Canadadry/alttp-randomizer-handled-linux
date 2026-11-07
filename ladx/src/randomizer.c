#include "ladx_randomizer.h"

LadxRandomizeStatus ladx_randomize(LadxRandomizer *r, const LadxSettings *validated, const uint8_t *seed,
				   size_t seed_len, LadxRngSink sink, void *ctx)
{
	r->settings = *validated;
	r->outcome.kind = LADX_OUTCOME_UNFINISHED;
	r->outcome.type = NULL;
	r->outcome.message = NULL;
	ladx_rng_init(&r->rnd, LADX_RNG_RND, sink, ctx);
	ladx_rng_init(&r->global, LADX_RNG_GLOBAL, sink, ctx);
	ladx_rng_seed(&r->rnd, LADX_STAGE_RANDOMIZER_INIT, seed, seed_len);
	if (r->settings.value[LADX_SETTING_RACE])
		ladx_rng_random(&r->rnd, LADX_STAGE_RANDOMIZER_INIT);
	return LADX_RANDOMIZE_OK;
}

const LadxOutcome *ladx_randomizer_outcome(const LadxRandomizer *r)
{
	return &r->outcome;
}
