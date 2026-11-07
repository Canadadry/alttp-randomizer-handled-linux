#include "ladx_rng.h"

#include <string.h>

static const char *const gen_names[LADX_RNG_GEN_COUNT] = {"rnd", "global"};

static const char *const stage_names[LADX_STAGE_COUNT] = {
	"randomizer:Randomizer.__init__",
	"worldSetup:WorldSetup.randomize",
	"patches.maze:buildMaze",
	"itempool:ItemPool.__randomizeRupees",
	"randomizer:RandomItemPlacer.run",
	"randomizer:RandomItemPlacer.placeSpecificItem",
	"randomizer:RandomItemPlacer.__placeItem",
	"patches.aesthetics:getMarinStartingText",
	"utils:randomNumber",
	"hints:addHints",
};

static const char *const method_names[LADX_RNG_METHOD_COUNT] = {
	"seed", "random", "choice", "choices", "shuffle", "randint", "getstate", "setstate", "randrange",
};

void ladx_rng_init(LadxRng *rng, LadxRngGen gen, LadxRngSink sink, void *ctx)
{
	memset(rng, 0, sizeof *rng);
	rng->gen = gen;
	rng->sink = sink;
	rng->ctx = ctx;
}

const char *ladx_rng_gen_name(LadxRngGen gen)
{
	return (unsigned)gen < LADX_RNG_GEN_COUNT ? gen_names[gen] : NULL;
}

const char *ladx_rng_stage_name(LadxRngStage stage)
{
	return (unsigned)stage < LADX_STAGE_COUNT ? stage_names[stage] : NULL;
}

const char *ladx_rng_method_name(LadxRngMethod method)
{
	return (unsigned)method < LADX_RNG_METHOD_COUNT ? method_names[method] : NULL;
}

static void start(const LadxRng *rng, LadxRngRecord *r, LadxRngStage stage, LadxRngMethod method)
{
	memset(r, 0, sizeof *r);
	r->gen = rng->gen;
	r->stage = stage;
	r->method = method;
	r->words = rng->py.words;
}

static void emit(const LadxRng *rng, LadxRngRecord *r)
{
	r->words = rng->py.words - r->words;
	if (rng->sink)
		rng->sink(rng->ctx, r);
}

static LadxRngStatus status_of(LadxPyRandomStatus s)
{
	switch (s) {
	case LADX_PYRANDOM_OK:
		return LADX_RNG_OK;
	case LADX_PYRANDOM_INDEX_ERROR:
		return LADX_RNG_INDEX_ERROR;
	case LADX_PYRANDOM_VALUE_ERROR:
		return LADX_RNG_VALUE_ERROR;
	default:
		return LADX_RNG_UNSUPPORTED;
	}
}

void ladx_rng_seed(LadxRng *rng, LadxRngStage stage, const uint8_t *seed, size_t size)
{
	LadxRngRecord r;

	ladx_pyrandom_seed(&rng->py, seed, size);
	start(rng, &r, stage, LADX_RNG_SEED);
	r.seed = seed;
	r.seed_len = size;
	emit(rng, &r);
}

double ladx_rng_random(LadxRng *rng, LadxRngStage stage)
{
	LadxRngRecord r;

	start(rng, &r, stage, LADX_RNG_RANDOM);
	r.real = ladx_pyrandom_random(&rng->py);
	emit(rng, &r);
	return r.real;
}

LadxRngStatus ladx_rng_choice(LadxRng *rng, LadxRngStage stage, size_t n, size_t *index)
{
	LadxRngRecord r;
	LadxPyRandomStatus s;

	start(rng, &r, stage, LADX_RNG_CHOICE);
	s = ladx_pyrandom_choice(&rng->py, n, &r.index);
	if (s != LADX_PYRANDOM_OK)
		return status_of(s);
	r.n = n;
	emit(rng, &r);
	*index = r.index;
	return LADX_RNG_OK;
}

LadxRngStatus ladx_rng_choices(LadxRng *rng, LadxRngStage stage, const int *weights, size_t n, size_t k,
			       size_t *out)
{
	double real_weights[LADX_RNG_MAX_LIST];
	size_t picks[LADX_RNG_MAX_LIST];
	LadxRngRecord r;
	LadxPyRandomStatus s;
	size_t i;

	if (n > LADX_RNG_MAX_LIST || k > LADX_RNG_MAX_LIST)
		return LADX_RNG_OVERFLOW;
	if (!weights)
		return LADX_RNG_UNSUPPORTED;
	for (i = 0; i < n; i++)
		real_weights[i] = (double)weights[i];
	start(rng, &r, stage, LADX_RNG_CHOICES);
	s = ladx_pyrandom_choices(&rng->py, real_weights, n, k, picks);
	if (s != LADX_PYRANDOM_OK)
		return status_of(s);
	r.n = n;
	r.weights = weights;
	r.k = k;
	r.list = picks;
	emit(rng, &r);
	memcpy(out, picks, k * sizeof *picks);
	return LADX_RNG_OK;
}

LadxRngStatus ladx_rng_shuffle(LadxRng *rng, LadxRngStage stage, void *base, size_t count, size_t size)
{
	size_t perm[LADX_RNG_MAX_LIST];
	unsigned char *bytes = base;
	LadxRngRecord r;
	size_t i;

	if (count > LADX_RNG_MAX_LIST)
		return LADX_RNG_OVERFLOW;
	for (i = 0; i < count; i++)
		perm[i] = i;
	start(rng, &r, stage, LADX_RNG_SHUFFLE);
	ladx_pyrandom_shuffle(&rng->py, perm, count, sizeof *perm);
	/* Gathering x[to] = before[perm[to]] equals swapping x as perm was swapped: walk perm's cycles. */
	for (i = 0; i < count; i++) {
		size_t to, from, at;

		if (perm[i] == i)
			continue;
		/* Each cycle is walked once, from its smallest index. */
		for (from = perm[i]; from != i; from = perm[from])
			if (from < i)
				break;
		if (from != i)
			continue;
		for (at = 0; at < size; at++) {
			unsigned char first = bytes[i * size + at];

			to = i;
			for (from = perm[to]; from != i; to = from, from = perm[from])
				bytes[to * size + at] = bytes[from * size + at];
			bytes[to * size + at] = first;
		}
	}
	r.n = count;
	r.list = perm;
	emit(rng, &r);
	return LADX_RNG_OK;
}

LadxRngStatus ladx_rng_randint(LadxRng *rng, LadxRngStage stage, int64_t a, int64_t b, int64_t *out)
{
	LadxRngRecord r;
	LadxPyRandomStatus s;

	start(rng, &r, stage, LADX_RNG_RANDINT);
	s = ladx_pyrandom_randint(&rng->py, a, b, &r.value);
	if (s != LADX_PYRANDOM_OK)
		return status_of(s);
	r.a = a;
	r.b = b;
	emit(rng, &r);
	*out = r.value;
	return LADX_RNG_OK;
}

LadxRngStatus ladx_rng_randrange(LadxRng *rng, LadxRngStage stage, int64_t lo, int64_t hi, int64_t *out)
{
	LadxRngRecord r;
	LadxPyRandomStatus s;

	start(rng, &r, stage, LADX_RNG_RANDRANGE);
	s = ladx_pyrandom_randrange(&rng->py, lo, hi, &r.value);
	if (s != LADX_PYRANDOM_OK)
		return status_of(s);
	r.a = lo;
	r.b = hi;
	emit(rng, &r);
	*out = r.value;
	return LADX_RNG_OK;
}

void ladx_rng_copy_state(LadxRng *to, LadxRng *from, LadxRngStage stage)
{
	LadxRngRecord r;

	start(from, &r, stage, LADX_RNG_GETSTATE);
	emit(from, &r);
	to->py = from->py;
	start(to, &r, stage, LADX_RNG_SETSTATE);
	emit(to, &r);
}
