#include "rando.h"
#include "rando_filler.h"
#include "rando_prepare.h"

#include <assert.h>
#include <stdbool.h>

static const char *const stage_names[RANDO_STAGE_COUNT] = {
	"crystals",
	"getAdvancementItems",
	"setShops",
	"setMedallions",
	"placeBosses",
	"fillPrizes",
	"setFountains",
	"shufflePrizePacks",
	"prepareWorld",
	"fill",
};

const char *rando_stage_name(RandoStage stage)
{
	assert(stage < RANDO_STAGE_COUNT);
	return stage_names[stage];
}

static void observe_draw(void *ctx, int64_t min, int64_t max, int64_t r)
{
	RandoGen *gen = ctx;

	assert(gen->stage != RANDO_STAGE_NONE && "draw outside any stage: label it with its VT stage");
	gen->draw++;
	if (gen->trace)
		gen->trace(gen->trace_ctx, gen->attempt, gen->draw, stage_names[gen->stage], min, max, r);
}

static int resolve_crystals(RandoGen *gen, RandoCrystals crystals)
{
	if (crystals == RANDO_CRYSTALS_RANDOM)
		return (int)rando_random_int(&gen->prng, 0, 7);
	return (int)crystals;
}

/* Closes the running stage; true when the caller must stop there with RANDO_INCOMPLETE. */
static bool end_stage(RandoGen *gen, RandoStatus *status)
{
	bool stop = gen->stage == gen->last;

	gen->stage = RANDO_STAGE_NONE;
	if (stop)
		*status = RANDO_INCOMPLETE;
	return stop;
}

#ifdef RANDO_FAULTS
static bool fault_fires(RandoGen *gen, RandoFault point)
{
	if (gen->fault != point || gen->attempt >= gen->fault_attempts)
		return false;
	gen->fault_fired++;
	return true;
}
#else
#define fault_fires(gen, point) false
#endif

/* Returns false when the attempt failed and the loop must retry; otherwise stores the final status. */
static bool run_attempt(RandoGen *gen, RandoStatus *status)
{
	RandoConfig config;
	int crystals_ganon, crystals_tower;
	RandoVariant variant;
	RandoFillState fill;

	gen->stage = RANDO_STAGE_NONE;
	gen->draw = 0;
	rando_prng_seed(&gen->prng, rando_splitmix32((uint32_t)(gen->seed + gen->attempt)));

	gen->stage = RANDO_STAGE_CRYSTALS;
	crystals_ganon = resolve_crystals(gen, gen->options.crystals_ganon);
	crystals_tower = resolve_crystals(gen, gen->options.crystals_tower);
	if (end_stage(gen, status))
		return true;

	switch (gen->options.state) {
	case RANDO_OPTION_STATE_STANDARD:
		variant = RANDO_VARIANT_STANDARD;
		break;
	case RANDO_OPTION_STATE_INVERTED:
		variant = RANDO_VARIANT_INVERTED;
		break;
	case RANDO_OPTION_STATE_RETRO:
		variant = RANDO_VARIANT_RETRO;
		break;
	case RANDO_OPTION_STATE_OPEN:
	default:
		variant = RANDO_VARIANT_OPEN;
		break;
	}
	rando_config_from_options(&gen->options, crystals_ganon, crystals_tower, &config);
	rando_world_init(&gen->world, variant, &config);
	rando_prepare_start_equipment(&gen->world);
	rando_prepare_goal(&gen->world);

	gen->stage = RANDO_STAGE_GET_ADVANCEMENT_ITEMS;
	rando_pool_build(&gen->world, &gen->prng, &gen->pools);
	if (end_stage(gen, status))
		return true;
	rando_prepare_pre_collect(&gen->world, &gen->pools);

	/* None of these stages may call collectItems: VT caches the collectable shop items on the first call. */
	gen->stage = RANDO_STAGE_SET_SHOPS;
	rando_set_shops(&gen->world, &gen->prng);
	if (end_stage(gen, status))
		return true;
	gen->stage = RANDO_STAGE_SET_MEDALLIONS;
	rando_set_medallions(&gen->world, &gen->prng);
	if (end_stage(gen, status))
		return true;
	gen->stage = RANDO_STAGE_PLACE_BOSSES;
	rando_place_bosses(&gen->world);
	if (end_stage(gen, status))
		return true;

	gen->stage = RANDO_STAGE_FILL_PRIZES;
	if (!rando_fill_prizes(&gen->world, &gen->prng)) {
		gen->stage = RANDO_STAGE_NONE;
		return false;
	}
	if (end_stage(gen, status))
		return true;
	gen->stage = RANDO_STAGE_SET_FOUNTAINS;
	rando_set_fountains(&gen->world, &gen->prng);
	if (end_stage(gen, status))
		return true;
	gen->stage = RANDO_STAGE_SHUFFLE_PRIZE_PACKS;
	rando_shuffle_prize_packs(&gen->world, &gen->prng);
	if (end_stage(gen, status))
		return true;
	gen->stage = RANDO_STAGE_PREPARE_WORLD;
	rando_prepare_pools(&gen->world, &gen->prng, &gen->pools);
	if (end_stage(gen, status))
		return true;

	gen->stage = RANDO_STAGE_FILL;
	if (!rando_fill_dungeon(&gen->world, &gen->prng, &gen->pools, &fill)
	    || fault_fires(gen, RANDO_FAULT_MID_FILL)
	    || !rando_fill_rest(&gen->world, &gen->prng, &gen->pools, &fill)) {
		gen->stage = RANDO_STAGE_NONE;
		return false;
	}
	if (end_stage(gen, status))
		return true;
	if (fault_fires(gen, RANDO_FAULT_IS_WINNABLE) || !rando_world_is_winnable(&gen->world))
		return false;

	*status = RANDO_COMPLETE;
	return true;
}

RandoStatus rando_generate(RandoGen *gen, const RandoOptions *options, uint32_t seed, uint32_t max_attempts,
			   RandoTraceFn trace, void *trace_ctx)
{
	return rando_generate_until(gen, options, seed, max_attempts, trace, trace_ctx, RANDO_STAGE_NONE);
}

RandoStatus rando_generate_until(RandoGen *gen, const RandoOptions *options, uint32_t seed, uint32_t max_attempts,
				 RandoTraceFn trace, void *trace_ctx, RandoStage last)
{
	RandoStatus status;

	assert(last <= RANDO_STAGE_NONE);
	gen->options = *options;
	gen->last = last;
	gen->seed = seed;
	gen->trace = trace;
	gen->trace_ctx = trace_ctx;
	gen->prng.observe = observe_draw;
	gen->prng.observe_ctx = gen;
	for (gen->attempt = 0;; gen->attempt++) {
		if (run_attempt(gen, &status))
			return status;
		if (max_attempts != 0 && gen->attempt + 1 >= max_attempts)
			return RANDO_GAVE_UP;
	}
}
