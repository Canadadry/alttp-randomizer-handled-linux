#ifndef RANDO_BPS_H
#define RANDO_BPS_H

#include <stddef.h>
#include <stdint.h>

typedef enum RandoBpsResult {
	RANDO_BPS_OK,
	RANDO_BPS_NOT_BPS,
	RANDO_BPS_MALFORMED,
	RANDO_BPS_PATCH_CRC,
	RANDO_BPS_SOURCE_SIZE,
	RANDO_BPS_SOURCE_CRC,
	RANDO_BPS_TARGET_TOO_SMALL,
	RANDO_BPS_TARGET_CRC,
	RANDO_BPS_RESULT_COUNT, /* never returned */
} RandoBpsResult;

/* Offsets are into the patch; the metadata is not parsed. The actions run from actions_offset up
 * to patch_size - 12. */
typedef struct RandoBpsHeader {
	size_t source_size;
	size_t target_size;
	size_t metadata_offset;
	size_t metadata_size;
	size_t actions_offset;
	uint32_t source_crc;
	uint32_t target_crc;
	uint32_t patch_crc;
} RandoBpsHeader;

/* patch is only read. Checks the magic, the header numbers, that the metadata ends before the
 * 12-byte footer, and the patch CRC32. Fills *out only on RANDO_BPS_OK. */
RandoBpsResult rando_bps_read_header(const uint8_t *patch, size_t patch_size, RandoBpsHeader *out);

/* Every buffer is the caller's; patch and source are only read; target must not overlap either.
 * Sets *target_size to the declared target size on RANDO_BPS_OK, to 0 on any failure; on failure
 * target[0..target_capacity) may be partly written and must be discarded. Bytes past the declared
 * target size are never written. */
RandoBpsResult rando_bps_apply(const uint8_t *patch, size_t patch_size, const uint8_t *source, size_t source_size,
			       uint8_t *target, size_t target_capacity, size_t *target_size);

#endif
