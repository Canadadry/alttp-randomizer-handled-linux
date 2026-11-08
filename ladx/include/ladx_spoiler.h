#ifndef LADX_SPOILER_H
#define LADX_SPOILER_H

#include <stddef.h>

#include "ladx_encoder.h"
#include "ladx_groups.h"
#include "ladx_logic.h"
#include "ladx_outcome.h"
#include "ladx_room.h"
#include "ladx_settings.h"

/*
 * spoilerLog.py SpoilerLog at the pin (tests/ladx/README.md, "Spoiler"):
 * the finished ROM read back from an encoder's current state, the logic
 * rebuilt on the read-back world setup, the spheres counted, and
 * outputJson's text. Caller-owned, no allocation; draws nothing.
 */

#define LADX_SPOILER_SEED_HEX 32

typedef enum LadxSpoilerStatus {
	LADX_SPOILER_OK = 0,
	LADX_SPOILER_NONE,   /* race: LADXR writes no spoiler */
	LADX_SPOILER_RAISED, /* LADXR raises: the outcome filled */
	LADX_SPOILER_FAILED  /* a byte or room C cannot read, the logic not built, a state outside tier 1 */
} LadxSpoilerStatus;

typedef struct LadxSpoiler {
	LadxSettings settings;
	char seed[LADX_SPOILER_SEED_HEX + 1]; /* readHexSeed, set by build before the world setup is read */
	LadxWorldSetup ws;		       /* loadFromRom's */
	LadxLogic logic;		       /* the spoiler's own Logic */
	LadxExplorer explorer;
	int item[LADX_LOGIC_SPOT_CAP];	 /* each spot's read */
	int sphere[LADX_LOGIC_SPOT_CAP]; /* -1: no sphere (null) */
	char remaining[LADX_LOGIC_SPOT_CAP];
	char last[LADX_LOGIC_LOCATION_CAP];
	int accessible[LADX_LOGIC_SPOT_CAP], accessible_count;
	int inaccessible[LADX_LOGIC_SPOT_CAP], inaccessible_count;
	int mapping[2 * LADX_ENTRANCES_MAX]; /* readEntrances' scratch */
	int built;			     /* 1 once ladx_spoiler_build returned OK */
} LadxSpoiler;

/* entranceInfo.py's x and y (-1: None), the fields LadxEntranceInfo lacks. */
typedef struct LadxEntrancePos {
	int x, y;
} LadxEntrancePos;

/* ENTRANCE_INFO's x and y in its order; *count LADX_ENTRANCE_INFO_COUNT. */
const LadxEntrancePos *ladx_entrance_pos(size_t *count);

/*
 * SpoilerLog(settings, args, rom) on the encoder's current state (the ROM
 * given to it for source bytes): the race byte, readHexSeed, loadFromRom,
 * Logic and _loadItems. NONE, nothing built, when settings has race on.
 */
LadxSpoilerStatus ladx_spoiler_build(LadxSpoiler *sp, const LadxEncoder *enc, const LadxSettings *validated,
				     LadxRoom *room, LadxOutcome *raised);

/* Logic(settings, world_setup=ws) into sp, with the seed's text; FAILED when it does not build. */
LadxSpoilerStatus ladx_spoiler_begin(LadxSpoiler *sp, const LadxSettings *validated, const LadxWorldSetup *ws,
				     const char *seed_hex);

/* _loadItems' spheres and lists from items, one per spot of sp's logic (ladx_spoiler_begin first). */
LadxSpoilerStatus ladx_spoiler_spheres(LadxSpoiler *sp, const int *items);

/* Spot i's read(rom) of sp's logic (ladx_spoiler_begin first) into *item. */
LadxSpoilerStatus ladx_spoiler_read_item(const LadxSpoiler *sp, const LadxEncoder *enc, int spot, LadxRoom *room,
					 int *item, LadxOutcome *raised);

/* patches/enemies.py readBossMapping into mapping[9]. */
LadxSpoilerStatus ladx_spoiler_read_bosses(const LadxEncoder *enc, LadxRoom *room, int *mapping, LadxOutcome *raised);

/* readMiniBossMapping into mapping[LADX_MINIBOSS_KEY_COUNT]; FAILED for a key with no or a nightmare match. */
LadxSpoilerStatus ladx_spoiler_read_minibosses(const LadxEncoder *enc, LadxRoom *room, LadxMiniboss *mapping,
					       LadxOutcome *raised);

/*
 * patches/entrances.py readEntrances over info and pos [0..count-1]:
 * mapping[2 * count], key k < count the entrance k and count + k its
 * ":inside", each value such a key or -1 when absent. FAILED over
 * LADX_ENTRANCES_MAX or on a room that does not load.
 */
LadxSpoilerStatus ladx_spoiler_read_entrances(const LadxEncoder *enc, LadxRoom *room, const LadxEntranceInfo *info,
					      const LadxEntrancePos *pos, size_t count, int *mapping);

/*
 * The spoiler group: ladx_spoiler_build into p->spoiler (nothing when
 * NULL), LADXR's raise into p->raised. Needs p->rom.
 */
int ladx_group_spoiler(LadxEncoder *enc, LadxPipeline *p);

typedef void (*LadxSpoilerSink)(void *ctx, const char *text, size_t len);

/* outputJson's text, no trailing newline; 0, part written, on a non-ASCII name. */
int ladx_spoiler_render(const LadxSpoiler *sp, LadxSpoilerSink sink, void *ctx);

#endif
