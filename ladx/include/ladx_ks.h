#ifndef LADX_KS_H
#define LADX_KS_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_rom.h"
#include "ladx_wlog.h"

/*
 * The C-knowable source map (KS, tests/ladx/README.md, "Encoding rule"),
 * built from the source ROM's parsed tables as the oracle's
 * known_source.build builds it, in a caller-owned struct. Its spans keep
 * the build's order and labels; its lookups are membership, the entry
 * span of a table key, span_at, the maximal contiguous runs and find.
 */
#define LADX_KS_SPAN_MAX 8192
#define LADX_KS_FAILURE_MAX 1024
#define LADX_KS_BYTE_MAX 131072 /* KS offsets, which bounds the 4-byte windows */
#define LADX_KS_ALT_MAX 7	/* an alt key's name, without its NUL */

typedef enum LadxKsStatus {
	LADX_KS_OK = 0,
	LADX_KS_BAD_INPUT, /* a span outside the source, empty, or a bad label */
	LADX_KS_OVERFLOW,  /* a capacity exceeded: the KS is unusable */
	LADX_KS_NOT_FINISHED
} LadxKsStatus;

typedef enum LadxKsKind {
	LADX_KS_POINTER = 0, /* "<table> pointer <key>" */
	LADX_KS_BANK,	     /* "<table> bank <key>" */
	LADX_KS_ENTRY,	     /* "<table> <key>", an entry span */
	LADX_KS_BOSS_ZERO,   /* "boss zero <offset>" (index the offset) */
	LADX_KS_BOSS_ITEM    /* "boss item <room>" (index the room) */
} LadxKsKind;

/* A span's label; table indexes ROMWithTables' order (ladx_rom.h), -1 for the boss kinds. */
typedef struct LadxKsLabel {
	LadxKsKind kind;
	int table;
	uint32_t index;
	char alt[LADX_KS_ALT_MAX + 1]; /* "" for an index key */
} LadxKsLabel;

typedef struct LadxKsSpan {
	uint32_t start, stop;
	LadxKsLabel label;
} LadxKsSpan;

typedef struct LadxKsRun {
	uint32_t start, stop;
} LadxKsRun;

/* Read nothing of it but through the functions below. */
typedef struct LadxKs {
	const uint8_t *rom;
	size_t len;
	int finished;
	LadxKsStatus failed;
	LadxKsSpan span[LADX_KS_SPAN_MAX];
	uint32_t span_count;
	LadxKsLabel failure[LADX_KS_FAILURE_MAX];
	uint32_t failure_count;
	uint8_t mask[LADX_ROM_SIZE / 8];
	uint32_t byte_count;
	uint32_t by_start[LADX_KS_SPAN_MAX]; /* spans by start, the longest first on a tie */
	uint32_t reach[LADX_KS_SPAN_MAX];    /* the largest stop of by_start[0..i] */
	LadxKsRun run[LADX_KS_SPAN_MAX];
	uint32_t run_count;
	uint32_t window[LADX_KS_BYTE_MAX]; /* every 4-byte window inside a run, by value then offset */
	uint32_t window_count;
} LadxKs;

/* An empty KS over rom (at most LADX_ROM_SIZE bytes), which must outlive it. */
void ladx_ks_init(LadxKs *ks, const uint8_t *rom, size_t len);

/* Adds the span start..stop; an ENTRY label makes it its key's entry span (a later one wins). */
LadxKsStatus ladx_ks_add(LadxKs *ks, uint32_t start, uint32_t stop, const LadxKsLabel *label);

/* Records a span left out because its fields do not rebuild it. */
LadxKsStatus ladx_ks_add_failure(LadxKs *ks, const LadxKsLabel *label);

/* Builds the lookups; no add after it. */
LadxKsStatus ladx_ks_finish(LadxKs *ks);

/* init, every span of known_source.build in its order, then finish. */
LadxKsStatus ladx_ks_build(LadxKs *ks, const uint8_t *rom, size_t len, const LadxRomTables *tables);

uint32_t ladx_ks_span_count(const LadxKs *ks);
uint32_t ladx_ks_byte_count(const LadxKs *ks);
uint32_t ladx_ks_failure_count(const LadxKs *ks);

/* Each lookup below needs a finished KS; on another, it finds nothing. */
int ladx_ks_contains(const LadxKs *ks, int64_t q);

/* The entry span of table's key (an ENTRY label), 0 when none. */
int ladx_ks_entry(const LadxKs *ks, int table, LadxWlogKey key, uint32_t *start, uint32_t *stop);

/* The span holding q with the lowest start, the longest of those; 0 outside the map. */
int ladx_ks_span_at(const LadxKs *ks, int64_t q, uint32_t *start, uint32_t *stop);

uint32_t ladx_ks_run_count(const LadxKs *ks);
LadxKsRun ladx_ks_run(const LadxKs *ks, uint32_t i);

/* The lowest source offset where data's n >= 4 bytes lie inside one run, -1 when none. */
int64_t ladx_ks_find(const LadxKs *ks, const uint8_t *data, size_t n);

typedef void (*LadxKsSink)(void *ctx, const char *text, size_t len);

/*
 * The span list (tests/ladx/README.md): `ladx-ks-spans 1`, then
 * `<start> <length> <label>` per span in hex in the build's order, then
 * `failure <label>` per span left out.
 */
void ladx_ks_render_spans(const LadxKs *ks, LadxKsSink sink, void *ctx);

#endif
