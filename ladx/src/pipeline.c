#include "ladx_pipeline.h"
#include "ladx_groups.h"
#include "ladx_spoiler.h"

#include <string.h>

static const LadxPipelineGroup groups[] = {
	{"bank3e", 1, ladx_group_bank3e, NULL},
	{"marin", 1, ladx_group_marin, ladx_group_marin_draws},
	{"hints", 0, ladx_group_hints, NULL},
	{"music", 1, ladx_group_music, ladx_group_music_draws},
	{"notext", 1, ladx_group_notext, NULL},
	{"goal", 1, ladx_group_goal, NULL},
	{"items", 1, ladx_group_items, NULL},
	{"entrances", 1, ladx_group_entrances, NULL},
	{"bosses", 1, ladx_group_bosses, NULL},
	{"enemizer", 1, ladx_group_enemizer, NULL},
	{"warp-home", 1, ladx_group_warp_home, NULL},
	{"title", 1, ladx_group_title, NULL},
	{"endscreen", 1, ladx_group_endscreen, ladx_group_endscreen_draws},
	{"sprites", 1, ladx_group_sprites, NULL},
	{"save", 1, ladx_group_save, NULL},
	{"spoiler", 1, ladx_group_spoiler, NULL},
	{"header", 1, ladx_group_header, NULL},
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
	LadxPipelineRun run;
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
		if (g == p->group_count || ran[g])
			continue;
		run = p->groups[g].needs_rom && !p->rom ? p->groups[g].draw : p->groups[g].run;
		if (!run)
			continue;
		ran[g] = 1;
		memset(&p->raised, 0, sizeof p->raised);
		if (!run(enc, p)) {
			err->status = p->raised.kind != LADX_OUTCOME_UNFINISHED ? LADX_PIPELINE_RAISED : LADX_PIPELINE_GROUP;
			err->outcome = err->status == LADX_PIPELINE_RAISED ? &p->raised : NULL;
			err->group = p->groups[g].name;
			return err->status;
		}
	}
	if (err->tables.status != LADX_TABLES_OK)
		return tables_error(err);
	st = ladx_enc_end(enc);
	return st == LADX_ENC_OK ? LADX_PIPELINE_OK : encoder_error(err, st);
}

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

int ladx_pipeline_const_unit_count(const LadxPipeline *p, const char *group)
{
	LadxSpan name;
	int g, first;

	name.at = group;
	name.len = strlen(group);
	if ((g = ladx_tables_find_group(p->tables, name)) < 0)
		return -1;
	return ladx_tables_group_units(p->tables, g, &first);
}

int ladx_pipeline_const(LadxEncoder *enc, LadxPipeline *p, const char *group)
{
	int count = ladx_pipeline_const_unit_count(p, group);

	return count >= 0 && ladx_pipeline_const_units(enc, p, group, 0, count);
}

int ladx_pipeline_const_units(LadxEncoder *enc, LadxPipeline *p, const char *group, int from, int n)
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
	if (from < 0 || n < 0 || from > count || n > count - from)
		return 0;
	for (u = first + from; u < first + from + n; u++) {
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

int ladx_pipeline_const_bank(LadxPipeline *p, const char *group, int unit, uint32_t offset, uint8_t *out, size_t n)
{
	LadxSpan name;
	LadxTablesStep step;
	LadxTablesError err;
	LadxTablesRecords it;
	LadxTablesRecord rec;
	size_t found = 0, i;
	int g, first, count;

	name.at = group;
	name.len = strlen(group);
	if ((g = ladx_tables_find_group(p->tables, name)) < 0)
		return 0;
	count = ladx_tables_group_units(p->tables, g, &first);
	if (unit < 0 || unit >= count || !ladx_tables_select(p->tables, first + unit, p->settings, p->spot_count, &step, &err))
		return 0;
	ladx_tables_records_init(&it, p->tables, &step);
	while (ladx_tables_records_next(&it, &rec, p->scratch, LADX_WLOG_ATOM_MAX, &err)) {
		if (rec.table_record || rec.offset >= offset + n || rec.offset + rec.n <= offset)
			continue;
		if (!resolve(p, rec.atoms, rec.n, p->values, p->origins))
			return 0;
		for (i = 0; i < rec.n; i++)
			if (rec.offset + i >= offset && rec.offset + i < offset + n) {
				out[rec.offset + i - offset] = p->values[i];
				found++;
			}
	}
	return err.status == LADX_TABLES_OK && found == n;
}
