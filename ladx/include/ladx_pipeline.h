#ifndef LADX_PIPELINE_H
#define LADX_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_background.h"
#include "ladx_encoder.h"
#include "ladx_ks.h"
#include "ladx_randomizer.h"
#include "ladx_rom.h"
#include "ladx_room.h"
#include "ladx_settings.h"
#include "ladx_store.h"
#include "ladx_tables.h"

/*
 * generateRom's walk of order.txt (tests/ladx/README.md, "Pipeline"):
 * each table unit's entry through the encoder's fixed calls at its first
 * row; at a C row, the registered group of that row's group, once, at its
 * group's first C row, skipped when none is registered or when it needs
 * the ROM, none is given and it has no draw. Then the encoder's log is ended.
 */

/* The parsed source ROM, every pointer outliving the run. */
typedef struct LadxPipelineRom {
	const uint8_t *bytes;
	size_t len;
	const LadxRomTables *tables;
	const LadxKs *ks;
} LadxPipelineRom;

typedef struct LadxPipeline LadxPipeline;

#define LADX_SAVE_BYTES_MAX 131072 /* one table's entry bytes at save time: LADXR's texts hold about 58 KiB */

/* save's scratch: one table's entries at store time. */
typedef struct LadxPipelineSave {
	uint8_t bytes[LADX_SAVE_BYTES_MAX];
	int32_t origins[LADX_SAVE_BYTES_MAX];
	uint32_t at[LADX_STORE_ENTRY_MAX]; /* each entry's first byte in bytes */
	LadxStoreEntry entries[LADX_STORE_ENTRY_MAX];
	LadxStoreSpan spans[LADX_STORE_ENTRY_MAX];
	LadxStoreDone done;
} LadxPipelineSave;

/* The run's pipeline: p->rom NULL when no ROM is given (never for a needs_rom group); returns 1 on success. */
typedef int (*LadxPipelineRun)(LadxEncoder *enc, LadxPipeline *p);

typedef struct LadxPipelineGroup {
	const char *name; /* the inventory's group, as order.txt's C rows name it */
	int needs_rom;	  /* 1 exactly for a rom-group of tests/ladx/claims.txt */
	LadxPipelineRun run;
	LadxPipelineRun draw; /* run instead of run when needs_rom and no ROM: its draws alone; NULL: skipped */
} LadxPipelineGroup;

#define LADX_PIPELINE_GROUP_MAX 32

/* The library's ported C groups; *count 0 and NULL when none is ported. */
const LadxPipelineGroup *ladx_pipeline_groups(int *count);

typedef enum LadxPipelineStatus {
	LADX_PIPELINE_OK = 0,
	LADX_PIPELINE_TABLES,  /* a walk or record error: err.tables */
	LADX_PIPELINE_ENCODER, /* the encoder refused or overflowed: err.encoder */
	LADX_PIPELINE_GROUP,   /* a group's run returned 0: err.group */
	LADX_PIPELINE_RAISED,  /* LADXR raises in err.group: its outcome in err.outcome */
	LADX_PIPELINE_BAD_INPUT /* over LADX_PIPELINE_GROUP_MAX groups */
} LadxPipelineStatus;

typedef struct LadxPipelineError {
	LadxPipelineStatus status;
	LadxTablesError tables;
	LadxEncStatus encoder;
	const char *group;
	const LadxOutcome *outcome; /* RAISED: the pipeline's raised */
} LadxPipelineError;

struct LadxPipeline {
	const LadxTables *tables;
	const LadxSettings *settings; /* validated */
	int spot_count;		      /* ladx_tables_spot_count of the placement */
	const LadxRandomizer *randomizer;
	LadxRng *rnd, *global; /* the randomizer's generators, continued by every drawing group; NULL: it fails */
	const LadxPipelineRom *rom; /* NULL: no ROM */
	const LadxPipelineGroup *groups;
	int group_count;
	void *ctx; /* the caller's, for every run */
	LadxOutcome raised; /* a run returning 0 with raised.kind set: LADXR raised */
	LadxAtom scratch[LADX_WLOG_ATOM_MAX]; /* the run's, one record's atoms */
	uint8_t values[LADX_WLOG_ATOM_MAX];   /* a C group's, one record's bytes */
	int32_t origins[LADX_WLOG_ATOM_MAX];
	LadxBackground background; /* a C group's, one background being edited */
	LadxRoom room;		   /* a C group's, one room being edited */
	LadxPipelineSave save;	   /* save's */
	struct LadxSpoiler *spoiler; /* the spoiler group's result, the caller's; NULL: not built */
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

/* ladx_pipeline_const of units first to first + count - 1 alone (0-based in the file); 0 when out of range. */
int ladx_pipeline_const_units(LadxEncoder *enc, LadxPipeline *p, const char *group, int first, int count);

/*
 * The bytes unit (0-based in group's constants file) writes at bank offsets
 * offset to offset + n - 1, resolved as ladx_pipeline_const does, into out;
 * nothing written. 0 when the unit does not write every one of them, has
 * no entry, or a reference cannot be read.
 */
int ladx_pipeline_const_bank(LadxPipeline *p, const char *group, int unit, uint32_t offset, uint8_t *out, size_t n);

/* The number of units of group's constants file; -1 when it has none. */
int ladx_pipeline_const_unit_count(const LadxPipeline *p, const char *group);

#endif
