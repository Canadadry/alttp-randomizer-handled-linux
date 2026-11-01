#include "rando_assemble.h"

#include <assert.h>
#include <string.h>

const uint8_t rando_assemble_jp10_md5[RANDO_MD5_SIZE] = {
	0x03, 0xA6, 0x39, 0x45, 0x39, 0x81, 0x91, 0x33, 0x7E, 0x89, 0x6E, 0x57, 0x71, 0xF7, 0x71, 0x73,
};

const uint8_t rando_assemble_base_md5[RANDO_MD5_SIZE] = {
	0xED, 0xC0, 0x1F, 0x3D, 0xB7, 0x98, 0xAE, 0x4D, 0xFE, 0x21, 0x10, 0x13, 0x11, 0x59, 0x8D, 0x44,
};

void rando_assemble_request_init(RandoAssembleRequest *request)
{
	assert(request != NULL);
	memset(request, 0, sizeof *request);
	request->source = NULL;
	request->patch = NULL;
	request->trace = NULL;
	request->trace_ctx = NULL;
	request->output = NULL;
	request->output_ctx = NULL;
	memcpy(request->source_md5, rando_assemble_jp10_md5, RANDO_MD5_SIZE);
	memcpy(request->base_md5, rando_assemble_base_md5, RANDO_MD5_SIZE);
	rando_options_init(&request->options);
	request->heart_beep = RANDO_HEART_BEEP_HALF;
	request->max_attempts = RANDO_ASSEMBLE_MAX_ATTEMPTS;
}

RandoAssembleStatus rando_assemble_check_patch(const uint8_t *patch, size_t patch_size, RandoBpsResult *bps)
{
	RandoBpsHeader header;
	RandoBpsResult result = rando_bps_read_header(patch, patch_size, &header);
	if (bps != NULL)
		*bps = result;
	if (result != RANDO_BPS_OK)
		return RANDO_ASSEMBLE_PATCH;
	if (header.source_size != RANDO_ROM_SOURCE_SIZE || header.target_size != RANDO_ROM_SIZE)
		return RANDO_ASSEMBLE_PATCH_SIZES;
	return RANDO_ASSEMBLE_OK;
}
