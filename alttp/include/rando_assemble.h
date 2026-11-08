#ifndef RANDO_ASSEMBLE_H
#define RANDO_ASSEMBLE_H

#include "rando.h"
#include "rando_bps.h"
#include "rando_md5.h"
#include "rando_options.h"
#include "rando_rom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RANDO_ASSEMBLE_MAX_ATTEMPTS 1000u

typedef enum RandoAssembleStatus {
	RANDO_ASSEMBLE_OK,
	RANDO_ASSEMBLE_SOURCE_SIZE,
	RANDO_ASSEMBLE_SOURCE_MD5,
	RANDO_ASSEMBLE_PATCH,
	RANDO_ASSEMBLE_PATCH_SIZES,
	RANDO_ASSEMBLE_BASE_MD5,
	RANDO_ASSEMBLE_UNSUPPORTED,
	RANDO_ASSEMBLE_GAVE_UP,
	RANDO_ASSEMBLE_OUTPUT,
	RANDO_ASSEMBLE_STATUS_COUNT,
} RandoAssembleStatus;

typedef bool (*RandoAssembleOutputFn)(void *ctx, const uint8_t *rom, size_t size);

typedef struct RandoAssembleRequest {
	const uint8_t *source;
	size_t source_size;
	const uint8_t *patch;
	size_t patch_size;
	uint8_t source_md5[RANDO_MD5_SIZE];
	uint8_t base_md5[RANDO_MD5_SIZE];
	RandoOptions options;
	RandoHeartBeep heart_beep;
	uint32_t seed;
	uint32_t max_attempts;
	RandoTraceFn trace;
	void *trace_ctx;
	RandoAssembleOutputFn output;
	void *output_ctx;
} RandoAssembleRequest;

typedef struct RandoAssembleWork {
	uint8_t rom[RANDO_ROM_SIZE];
	RandoGen gen;
} RandoAssembleWork;

extern const uint8_t rando_assemble_jp10_md5[RANDO_MD5_SIZE];
extern const uint8_t rando_assemble_base_md5[RANDO_MD5_SIZE];

void rando_assemble_request_init(RandoAssembleRequest *request);

RandoAssembleStatus rando_assemble_check_patch(const uint8_t *patch, size_t patch_size, RandoBpsResult *bps);

RandoAssembleStatus rando_assemble(const RandoAssembleRequest *request, RandoAssembleWork *work, RandoBpsResult *bps);

#endif
