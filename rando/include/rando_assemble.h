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
	RANDO_ASSEMBLE_STATUS_COUNT, /* never returned */
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

typedef struct RandoAssembleWork {
	uint8_t rom[RANDO_ROM_SIZE];
	RandoGen gen;
} RandoAssembleWork;

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

/* Each step runs only when every earlier one succeeded: (1) source_size is RANDO_ROM_SOURCE_SIZE,
 * else SOURCE_SIZE; (2) the source's MD5 is source_md5, else SOURCE_MD5; (3)
 * rando_assemble_check_patch; (4) the patch applied from source into work->rom, else PATCH; (5) the
 * image's MD5 is base_md5, else BASE_MD5; (6) rando_generate on work->gen (its fault fields are left
 * as the caller set them), UNSUPPORTED or GAVE_UP; (7) rando_rom_write with heart_beep; (8)
 * rando_rom_write_cosmetic_defaults, the sprite read from the still unchanged source; (9)
 * rando_rom_update_checksum; (10) output(output_ctx, work->rom, RANDO_ROM_SIZE), false giving OUTPUT.
 * Every buffer is the caller's: nothing is allocated, kept or freed. source and patch are only read
 * and must not overlap work; output must not be NULL. *bps (may be NULL) gets the BPS module's code
 * when a BPS call ran, RANDO_BPS_OK otherwise. On any status but OK and OUTPUT, work->rom holds
 * unspecified bytes and output was not called; on OK and OUTPUT (output called once), work->gen holds
 * the complete generation and work->rom the finished image. */
RandoAssembleStatus rando_assemble(const RandoAssembleRequest *request, RandoAssembleWork *work, RandoBpsResult *bps);

#endif
