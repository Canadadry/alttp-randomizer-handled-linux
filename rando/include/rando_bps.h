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
	RANDO_BPS_RESULT_COUNT,
} RandoBpsResult;

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

RandoBpsResult rando_bps_read_header(const uint8_t *patch, size_t patch_size, RandoBpsHeader *out);

RandoBpsResult rando_bps_apply(const uint8_t *patch, size_t patch_size, const uint8_t *source, size_t source_size,
			       uint8_t *target, size_t target_capacity, size_t *target_size);

#endif
