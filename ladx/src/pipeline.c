#include "ladx_pipeline.h"

#include <string.h>

const LadxPipelineGroup *ladx_pipeline_groups(int *count)
{
	*count = 0;
	return NULL;
}

static LadxPipelineStatus encoder_error(LadxPipelineError *err, LadxEncStatus st)
{
	err->status = LADX_PIPELINE_ENCODER;
	err->encoder = st;
	return err->status;
}

static LadxPipelineStatus tables_error(LadxPipelineError *err)
{
	err->status = LADX_PIPELINE_TABLES;
	return err->status;
}

static int span_is(LadxSpan sp, const char *s)
{
	return sp.len == strlen(s) && memcmp(sp.at, s, sp.len) == 0;
}

/* The unit's entry through the encoder's fixed calls. */
static LadxEncStatus apply_entry(LadxPipeline *p, LadxEncoder *enc, const LadxTablesStep *step, LadxPipelineError *err)
{
	LadxTablesRecords it;
	LadxTablesRecord rec;
	LadxEncStatus st = LADX_ENC_OK;

	ladx_tables_records_init(&it, p->tables, step);
	while (st == LADX_ENC_OK && ladx_tables_records_next(&it, &rec, p->scratch, LADX_WLOG_ATOM_MAX, &err->tables)) {
		if (!rec.table_record)
			st = ladx_enc_fixed_bank(enc, step->group, rec.offset, rec.atoms, NULL, rec.n);
		else if (rec.pointer)
			st = ladx_enc_fixed_pointer(enc, step->group, rec.table, rec.key, rec.value);
		else
			st = ladx_enc_fixed_table(enc, step->group, rec.table, rec.key, rec.atoms, NULL, rec.n);
	}
	return st;
}

LadxPipelineStatus ladx_pipeline_run(LadxPipeline *p, LadxEncoder *enc, LadxPipelineError *err)
{
	LadxTablesWalk w;
	LadxTablesStep step;
	char ran[LADX_PIPELINE_GROUP_MAX];
	LadxEncStatus st;
	int g;

	memset(err, 0, sizeof *err);
	memset(ran, 0, sizeof ran);
	if (p->group_count < 0 || p->group_count > LADX_PIPELINE_GROUP_MAX) {
		err->status = LADX_PIPELINE_BAD_INPUT;
		return err->status;
	}
	ladx_tables_walk_init(&w, p->tables, p->settings, p->spot_count);
	while (ladx_tables_walk_next(&w, &step, &err->tables)) {
		if (!step.c_row) {
			st = apply_entry(p, enc, &step, err);
			if (st != LADX_ENC_OK)
				return encoder_error(err, st);
			if (err->tables.status != LADX_TABLES_OK)
				return tables_error(err);
			continue;
		}
		for (g = 0; g < p->group_count && !span_is(step.group, p->groups[g].name); g++)
			;
		if (g == p->group_count || ran[g] || (p->groups[g].needs_rom && !p->rom))
			continue;
		ran[g] = 1;
		if (!p->groups[g].run(enc, p->randomizer, p->rom, p->ctx)) {
			err->status = LADX_PIPELINE_GROUP;
			err->group = p->groups[g].name;
			return err->status;
		}
	}
	if (err->tables.status != LADX_TABLES_OK)
		return tables_error(err);
	st = ladx_enc_end(enc);
	return st == LADX_ENC_OK ? LADX_PIPELINE_OK : encoder_error(err, st);
}
