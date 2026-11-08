#ifndef LADX_SPOILER_H
#define LADX_SPOILER_H

#include <stddef.h>

#include "ladx_encoder.h"
#include "ladx_groups.h"
#include "ladx_logic.h"
#include "ladx_outcome.h"
#include "ladx_room.h"
#include "ladx_settings.h"

#define LADX_SPOILER_SEED_HEX 32

typedef enum LadxSpoilerStatus {
	LADX_SPOILER_OK = 0,
	LADX_SPOILER_NONE,
	LADX_SPOILER_RAISED,
	LADX_SPOILER_FAILED
} LadxSpoilerStatus;

typedef struct LadxSpoiler {
	LadxSettings settings;
	char seed[LADX_SPOILER_SEED_HEX + 1];
	LadxWorldSetup ws;
	LadxLogic logic;
	LadxExplorer explorer;
	int item[LADX_LOGIC_SPOT_CAP];
	int sphere[LADX_LOGIC_SPOT_CAP];
	char remaining[LADX_LOGIC_SPOT_CAP];
	char last[LADX_LOGIC_LOCATION_CAP];
	int accessible[LADX_LOGIC_SPOT_CAP], accessible_count;
	int inaccessible[LADX_LOGIC_SPOT_CAP], inaccessible_count;
	int mapping[2 * LADX_ENTRANCES_MAX];
	int built;
} LadxSpoiler;

typedef struct LadxEntrancePos {
	int x, y;
} LadxEntrancePos;

const LadxEntrancePos *ladx_entrance_pos(size_t *count);

LadxSpoilerStatus ladx_spoiler_build(LadxSpoiler *sp, const LadxEncoder *enc, const LadxSettings *validated,
				     LadxRoom *room, LadxOutcome *raised);

LadxSpoilerStatus ladx_spoiler_begin(LadxSpoiler *sp, const LadxSettings *validated, const LadxWorldSetup *ws,
				     const char *seed_hex);

LadxSpoilerStatus ladx_spoiler_spheres(LadxSpoiler *sp, const int *items);

LadxSpoilerStatus ladx_spoiler_read_item(const LadxSpoiler *sp, const LadxEncoder *enc, int spot, LadxRoom *room,
					 int *item, LadxOutcome *raised);

LadxSpoilerStatus ladx_spoiler_read_bosses(const LadxEncoder *enc, LadxRoom *room, int *mapping, LadxOutcome *raised);

LadxSpoilerStatus ladx_spoiler_read_minibosses(const LadxEncoder *enc, LadxRoom *room, LadxMiniboss *mapping,
					       LadxOutcome *raised);

LadxSpoilerStatus ladx_spoiler_read_entrances(const LadxEncoder *enc, LadxRoom *room, const LadxEntranceInfo *info,
					      const LadxEntrancePos *pos, size_t count, int *mapping);

int ladx_group_spoiler(LadxEncoder *enc, LadxPipeline *p);

typedef void (*LadxSpoilerSink)(void *ctx, const char *text, size_t len);

int ladx_spoiler_render(const LadxSpoiler *sp, LadxSpoilerSink sink, void *ctx);

#endif
