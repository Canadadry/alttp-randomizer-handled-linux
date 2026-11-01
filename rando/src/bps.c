#include "rando_bps.h"
#include "rando_crc32.h"

#include <assert.h>

#define MAGIC_SIZE 4u
#define FOOTER_SIZE 12u
#define MIN_PATCH_SIZE (MAGIC_SIZE + 3u + FOOTER_SIZE)

static uint32_t read_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* VT's decodeBPS, reading patch[*pos..end). Returns 0 on a number unterminated before end, or
 * above SIZE_MAX, or overflowing 64 bits while decoded. */
static int decode_number(const uint8_t *patch, size_t *pos, size_t end, size_t *out)
{
	uint64_t number = 0, shift = 1;

	for (;;) {
		uint8_t x;
		if (*pos >= end)
			return 0;
		x = patch[(*pos)++];
		if ((x & 0x7F) != 0 && shift > (UINT64_MAX - number) / (x & 0x7F))
			return 0;
		number += (uint64_t)(x & 0x7F) * shift;
		if (x & 0x80)
			break;
		if (shift > UINT64_MAX >> 7)
			return 0;
		shift <<= 7;
		if (number > UINT64_MAX - shift)
			return 0;
		number += shift;
	}
	if (number > SIZE_MAX)
		return 0;
	*out = (size_t)number;
	return 1;
}

RandoBpsResult rando_bps_read_header(const uint8_t *patch, size_t patch_size, RandoBpsHeader *out)
{
	RandoBpsHeader h;
	size_t pos = MAGIC_SIZE, end;

	assert(patch != NULL && out != NULL);
	if (patch_size < MAGIC_SIZE || read_u32(patch) != 0x31535042u)
		return RANDO_BPS_NOT_BPS;
	if (patch_size < MIN_PATCH_SIZE)
		return RANDO_BPS_MALFORMED;
	end = patch_size - FOOTER_SIZE;
	if (!decode_number(patch, &pos, end, &h.source_size) || !decode_number(patch, &pos, end, &h.target_size) ||
	    !decode_number(patch, &pos, end, &h.metadata_size))
		return RANDO_BPS_MALFORMED;
	if (h.metadata_size > end - pos)
		return RANDO_BPS_MALFORMED;
	h.metadata_offset = pos;
	h.actions_offset = pos + h.metadata_size;
	h.source_crc = read_u32(patch + end);
	h.target_crc = read_u32(patch + end + 4);
	h.patch_crc = read_u32(patch + end + 8);
	if (h.patch_crc != rando_crc32(patch, patch_size - 4))
		return RANDO_BPS_PATCH_CRC;
	*out = h;
	return RANDO_BPS_OK;
}
