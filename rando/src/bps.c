#include "rando_bps.h"
#include "rando_crc32.h"

#include <assert.h>
#include <string.h>

#define MAGIC_SIZE 4u
#define FOOTER_SIZE 12u
#define MIN_PATCH_SIZE (MAGIC_SIZE + 3u + FOOTER_SIZE)

static uint32_t read_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

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

static int move_offset(const uint8_t *patch, size_t *pos, size_t end, size_t *offset)
{
	size_t d, delta;

	if (!decode_number(patch, pos, end, &d))
		return 0;
	delta = d >> 1;
	if (d & 1) {
		if (delta > *offset)
			return 0;
		*offset -= delta;
	} else {
		if (delta > SIZE_MAX - *offset)
			return 0;
		*offset += delta;
	}
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

RandoBpsResult rando_bps_apply(const uint8_t *patch, size_t patch_size, const uint8_t *source, size_t source_size,
			       uint8_t *target, size_t target_capacity, size_t *target_size)
{
	RandoBpsHeader h;
	RandoBpsResult r;
	size_t pos, end, out = 0, source_offset = 0, target_offset = 0;

	assert(target_size != NULL && (source != NULL || source_size == 0) && (target != NULL || target_capacity == 0));
	*target_size = 0;
	r = rando_bps_read_header(patch, patch_size, &h);
	if (r != RANDO_BPS_OK)
		return r;
	if (source_size != h.source_size)
		return RANDO_BPS_SOURCE_SIZE;
	if (rando_crc32(source, source_size) != h.source_crc)
		return RANDO_BPS_SOURCE_CRC;
	if (h.target_size > target_capacity)
		return RANDO_BPS_TARGET_TOO_SMALL;
	pos = h.actions_offset;
	end = patch_size - FOOTER_SIZE;
	while (pos < end) {
		size_t n, length;
		if (!decode_number(patch, &pos, end, &n))
			return RANDO_BPS_MALFORMED;
		length = (n >> 2) + 1;
		if (length > h.target_size - out)
			return RANDO_BPS_MALFORMED;
		switch (n & 3) {
		case 0:
			if (out > source_size || length > source_size - out)
				return RANDO_BPS_MALFORMED;
			memcpy(target + out, source + out, length);
			break;
		case 1:
			if (length > end - pos)
				return RANDO_BPS_MALFORMED;
			memcpy(target + out, patch + pos, length);
			pos += length;
			break;
		case 2:
			if (!move_offset(patch, &pos, end, &source_offset) || source_offset > source_size ||
			    length > source_size - source_offset)
				return RANDO_BPS_MALFORMED;
			memcpy(target + out, source + source_offset, length);
			source_offset += length;
			break;
		default: {
			size_t i;
			if (!move_offset(patch, &pos, end, &target_offset) || target_offset >= out)
				return RANDO_BPS_MALFORMED;
			for (i = 0; i < length; i++)
				target[out + i] = target[target_offset + i];
			target_offset += length;
			break;
		}
		}
		out += length;
	}
	if (out != h.target_size)
		return RANDO_BPS_MALFORMED;
	if (rando_crc32(target, out) != h.target_crc)
		return RANDO_BPS_TARGET_CRC;
	*target_size = out;
	return RANDO_BPS_OK;
}
