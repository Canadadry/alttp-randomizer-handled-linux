#ifndef LADX_KS_H
#define LADX_KS_H

#include <stddef.h>
#include <stdint.h>

#include "ladx_rom.h"
#include "ladx_wlog.h"

#define LADX_KS_SPAN_MAX 8192
#define LADX_KS_FAILURE_MAX 1024
#define LADX_KS_BYTE_MAX 131072
#define LADX_KS_ALT_MAX 7

typedef enum LadxKsStatus {
	LADX_KS_OK = 0,
	LADX_KS_BAD_INPUT,
	LADX_KS_OVERFLOW,
	LADX_KS_NOT_FINISHED
} LadxKsStatus;

typedef enum LadxKsKind {
	LADX_KS_POINTER = 0,
	LADX_KS_BANK,
	LADX_KS_ENTRY,
	LADX_KS_BOSS_ZERO,
	LADX_KS_BOSS_ITEM
} LadxKsKind;

typedef struct LadxKsLabel {
	LadxKsKind kind;
	int table;
	uint32_t index;
	char alt[LADX_KS_ALT_MAX + 1];
} LadxKsLabel;

typedef struct LadxKsSpan {
	uint32_t start, stop;
	LadxKsLabel label;
} LadxKsSpan;

typedef struct LadxKsRun {
	uint32_t start, stop;
} LadxKsRun;

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
	uint32_t by_start[LADX_KS_SPAN_MAX];
	uint32_t reach[LADX_KS_SPAN_MAX];
	LadxKsRun run[LADX_KS_SPAN_MAX];
	uint32_t run_count;
	uint32_t window[LADX_KS_BYTE_MAX];
	uint32_t window_count;
} LadxKs;

void ladx_ks_init(LadxKs *ks, const uint8_t *rom, size_t len);

LadxKsStatus ladx_ks_add(LadxKs *ks, uint32_t start, uint32_t stop, const LadxKsLabel *label);

LadxKsStatus ladx_ks_add_failure(LadxKs *ks, const LadxKsLabel *label);

LadxKsStatus ladx_ks_finish(LadxKs *ks);

LadxKsStatus ladx_ks_build(LadxKs *ks, const uint8_t *rom, size_t len, const LadxRomTables *tables);

uint32_t ladx_ks_span_count(const LadxKs *ks);
uint32_t ladx_ks_byte_count(const LadxKs *ks);
uint32_t ladx_ks_failure_count(const LadxKs *ks);

int ladx_ks_contains(const LadxKs *ks, int64_t q);

int ladx_ks_entry(const LadxKs *ks, int table, LadxWlogKey key, uint32_t *start, uint32_t *stop);

int ladx_ks_span_at(const LadxKs *ks, int64_t q, uint32_t *start, uint32_t *stop);

uint32_t ladx_ks_run_count(const LadxKs *ks);
LadxKsRun ladx_ks_run(const LadxKs *ks, uint32_t i);

int64_t ladx_ks_find(const LadxKs *ks, const uint8_t *data, size_t n);

typedef void (*LadxKsSink)(void *ctx, const char *text, size_t len);

void ladx_ks_render_spans(const LadxKs *ks, LadxKsSink sink, void *ctx);

#endif
