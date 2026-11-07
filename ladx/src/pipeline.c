#include "ladx_pipeline.h"
#include "ladx_groups.h"

#include <string.h>

static const LadxPipelineGroup groups[] = {
	{"bank3e", 1, ladx_group_bank3e},
	{"title", 1, ladx_group_title},
};

const LadxPipelineGroup *ladx_pipeline_groups(int *count)
{
	*count = (int)(sizeof groups / sizeof groups[0]);
	return groups;
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
		if (!p->groups[g].run(enc, p)) {
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

/* An entry's atoms as bytes: a literal's byte, else the source byte of a reference. */
static int resolve(const LadxPipeline *p, const LadxAtom *atoms, size_t n, uint8_t *values, int32_t *origins)
{
	size_t i;

	for (i = 0; i < n; i++) {
		if (atoms[i] >= 0 && (!p->rom || (size_t)atoms[i] >= p->rom->len))
			return 0;
		values[i] = (uint8_t)(atoms[i] >= 0 ? p->rom->bytes[atoms[i]] : -1 - atoms[i]);
		origins[i] = LADX_ENC_KNOWN;
	}
	return 1;
}

int ladx_pipeline_const(LadxEncoder *enc, LadxPipeline *p, const char *group)
{
	LadxSpan name;
	LadxTablesStep step;
	LadxTablesError err;
	LadxTablesRecords it;
	LadxTablesRecord rec;
	LadxEncStatus st = LADX_ENC_OK;
	int g, first, count, u;

	name.at = group;
	name.len = strlen(group);
	if ((g = ladx_tables_find_group(p->tables, name)) < 0)
		return 0;
	count = ladx_tables_group_units(p->tables, g, &first);
	for (u = first; u < first + count; u++) {
		if (!ladx_tables_select(p->tables, u, p->settings, p->spot_count, &step, &err))
			return 0;
		ladx_tables_records_init(&it, p->tables, &step);
		while (ladx_tables_records_next(&it, &rec, p->scratch, LADX_WLOG_ATOM_MAX, &err)) {
			if (rec.pointer) {
				st = ladx_enc_pointer(enc, name, rec.table, rec.key, rec.value);
			} else if (!resolve(p, rec.atoms, rec.n, p->values, p->origins)) {
				return 0;
			} else if (rec.table_record) {
				st = ladx_enc_table(enc, name, rec.table, rec.key, p->values, p->origins, rec.n);
			} else {
				st = ladx_enc_bank(enc, name, rec.offset, p->values, p->origins, rec.n);
			}
			if (st != LADX_ENC_OK)
				return 0;
		}
		if (err.status != LADX_TABLES_OK)
			return 0;
	}
	return 1;
}
