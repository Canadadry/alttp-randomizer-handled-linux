#include "ladx_groups.h"

LadxRngStatus ladx_endscreen_draw(LadxRng *global, size_t *cat)
{
	return ladx_rng_choice(global, LADX_STAGE_ENDSCREEN_UPDATE, LADX_ENDSCREEN_CAT_COUNT, cat);
}

int ladx_group_endscreen(LadxEncoder *enc, LadxPipeline *p)
{
	size_t cat;

	if (!p->rom || !p->global ||
	    ladx_pipeline_const_unit_count(p, "endscreen") != 1 + LADX_ENDSCREEN_CAT_COUNT)
		return 0;
	/* LADXR patches first, then draws: the draw reads no ROM byte and no two writes share a position. */
	return ladx_pipeline_const_units(enc, p, "endscreen", 0, 1) &&
	       ladx_endscreen_draw(p->global, &cat) == LADX_RNG_OK &&
	       ladx_pipeline_const_units(enc, p, "endscreen", 1 + (int)cat, 1);
}

int ladx_group_endscreen_draws(LadxEncoder *enc, LadxPipeline *p)
{
	size_t cat;

	(void)enc;
	return p->global && ladx_endscreen_draw(p->global, &cat) == LADX_RNG_OK;
}
