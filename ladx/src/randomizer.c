#include "ladx_randomizer.h"

LadxRandomizeStatus ladx_randomize(LadxRandomizer *r, const LadxSettings *validated, const uint8_t *seed,
				   size_t seed_len, LadxRngSink sink, void *ctx)
{
	LadxRandomizeStatus status;

	r->settings = *validated;
	r->outcome.kind = LADX_OUTCOME_UNFINISHED;
	r->outcome.type = NULL;
	r->outcome.message = NULL;
	ladx_world_setup_defaults(&r->ws);
	r->ws_done = false;
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
	switch (ladx_logic_build(&r->logic, &r->settings, &r->ws)) {
	case LADX_LOGIC_OK:
		return LADX_RANDOMIZE_OK;
	case LADX_LOGIC_UNSUPPORTED:
		return LADX_RANDOMIZE_UNSUPPORTED;
	default:
		return LADX_RANDOMIZE_BROKEN;
	}
}

const LadxOutcome *ladx_randomizer_outcome(const LadxRandomizer *r)
{
	return &r->outcome;
}

const LadxWorldSetup *ladx_randomizer_world_setup(const LadxRandomizer *r)
{
	return r->ws_done ? &r->ws : NULL;
}
