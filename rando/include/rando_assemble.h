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

/* rando_gen's watchdog: every golden wins at attempt 0. */
#define RANDO_ASSEMBLE_MAX_ATTEMPTS 1000u

typedef enum RandoAssembleStatus {
	RANDO_ASSEMBLE_OK,
	RANDO_ASSEMBLE_SOURCE_SIZE, /* source is not RANDO_ROM_SOURCE_SIZE bytes */
	RANDO_ASSEMBLE_SOURCE_MD5,  /* not the expected JP ROM */
	RANDO_ASSEMBLE_PATCH,       /* the BPS module refused it: see *bps */
	RANDO_ASSEMBLE_PATCH_SIZES, /* a valid BPS declaring other sizes */
	RANDO_ASSEMBLE_BASE_MD5,    /* patched image is not the expected base build */
	RANDO_ASSEMBLE_UNSUPPORTED,
	RANDO_ASSEMBLE_GAVE_UP,
	RANDO_ASSEMBLE_OUTPUT, /* the output function returned false */
} RandoAssembleStatus;

/* Returning false reports a failed write. rom is only valid during the call. */
typedef bool (*RandoAssembleOutputFn)(void *ctx, const uint8_t *rom, size_t size);

typedef struct RandoAssembleRequest {
	const uint8_t *source; /* the JP ROM, header stripped */
	size_t source_size;
	const uint8_t *patch; /* the base BPS */
	size_t patch_size;
	uint8_t source_md5[RANDO_MD5_SIZE];
	uint8_t base_md5[RANDO_MD5_SIZE];
	RandoOptions options; /* hints included */
	RandoHeartBeep heart_beep;
	uint32_t seed;
	uint32_t max_attempts; /* rando_generate's; 0 unbounded */
	RandoTraceFn trace;    /* may be NULL */
	void *trace_ctx;
	RandoAssembleOutputFn output;
	void *output_ctx;
} RandoAssembleRequest;

/* The JP 1.0 ROM (03a63945...) and VT's Rom::HASH of the base-patched image (edc01f3d...). */
extern const uint8_t rando_assemble_jp10_md5[RANDO_MD5_SIZE];
extern const uint8_t rando_assemble_base_md5[RANDO_MD5_SIZE];

/* The one place the two digests above enter a request. Also sets VT's default options (hints on),
 * heart beep half, max_attempts RANDO_ASSEMBLE_MAX_ATTEMPTS; every pointer NULL, sizes and seed 0. */
void rando_assemble_request_init(RandoAssembleRequest *request);

/* patch is only read. RANDO_ASSEMBLE_PATCH when rando_bps_read_header refuses it, else
 * RANDO_ASSEMBLE_PATCH_SIZES unless it declares RANDO_ROM_SOURCE_SIZE -> RANDO_ROM_SIZE. *bps (may
 * be NULL) gets rando_bps_read_header's code. */
RandoAssembleStatus rando_assemble_check_patch(const uint8_t *patch, size_t patch_size, RandoBpsResult *bps);

#endif
