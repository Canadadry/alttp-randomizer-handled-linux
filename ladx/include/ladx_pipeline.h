#ifndef LADX_PIPELINE_H
#define LADX_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_encoder.h"
#include "ladx_ks.h"
#include "ladx_randomizer.h"
#include "ladx_rom.h"
#include "ladx_settings.h"
#include "ladx_tables.h"

/*
 * generateRom's walk of order.txt (tests/ladx/README.md, "Pipeline"):
 * each table unit's entry through the encoder's fixed calls at its first
 * row; at a C row, the registered group of that row's group, once, at its
 * group's first C row, skipped when none is registered or when it needs
 * the ROM and none is given. Then the encoder's log is ended.
 */

/* The parsed source ROM, every pointer outliving the run. */
typedef struct LadxPipelineRom {
	const uint8_t *bytes;
	size_t len;
	const LadxRomTables *tables;
	const LadxKs *ks;
} LadxPipelineRom;

typedef struct LadxPipeline LadxPipeline;

/* The run's pipeline: p->rom NULL when no ROM is given (never for a needs_rom group); returns 1 on success. */
typedef int (*LadxPipelineRun)(LadxEncoder *enc, LadxPipeline *p);

typedef struct LadxPipelineGroup {
	const char *name; /* the inventory's group, as order.txt's C rows name it */
	int needs_rom;	  /* 1 exactly for a rom-group of tests/ladx/claims.txt */
	LadxPipelineRun run;
} LadxPipelineGroup;

#define LADX_PIPELINE_GROUP_MAX 32

/* The library's ported C groups; *count 0 and NULL when none is ported. */
const LadxPipelineGroup *ladx_pipeline_groups(int *count);

typedef enum LadxPipelineStatus {
	LADX_PIPELINE_OK = 0,
	LADX_PIPELINE_TABLES,  /* a walk or record error: err.tables */
	LADX_PIPELINE_ENCODER, /* the encoder refused or overflowed: err.encoder */
	LADX_PIPELINE_GROUP,   /* a group's run returned 0: err.group */
	LADX_PIPELINE_BAD_INPUT /* over LADX_PIPELINE_GROUP_MAX groups */
} LadxPipelineStatus;

typedef struct LadxPipelineError {
	LadxPipelineStatus status;
	LadxTablesError tables;
	LadxEncStatus encoder;
	const char *group;
} LadxPipelineError;

struct LadxPipeline {
	const LadxTables *tables;
	const LadxSettings *settings; /* validated */
	int spot_count;		      /* ladx_tables_spot_count of the placement */
	const LadxRandomizer *randomizer;
	const LadxPipelineRom *rom; /* NULL: no ROM */
	const LadxPipelineGroup *groups;
	int group_count;
	void *ctx; /* the caller's, for every run */
	LadxAtom scratch[LADX_WLOG_ATOM_MAX]; /* the run's, one record's atoms */
	uint8_t values[LADX_WLOG_ATOM_MAX];   /* a C group's, one record's bytes */
	int32_t origins[LADX_WLOG_ATOM_MAX];
};

/* enc initialised over p->rom's bytes, KS and tables (or none), its log begun. */
LadxPipelineStatus ladx_pipeline_run(LadxPipeline *p, LadxEncoder *enc, LadxPipelineError *err);

/*
 * Writes every unit of group's constants file (data/ladx/<group>.txt; its
 * entry for the key of p's settings and spot count), in file order, as the
 * C group's computed bytes: a reference or keep resolved from p->rom. 0
 * when the group has no file, a unit no entry, a reference falls outside
 * the ROM (or there is none), or the encoder refuses.
 */
int ladx_pipeline_const(LadxEncoder *enc, LadxPipeline *p, const char *group);

#endif
