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

RandoAssembleStatus rando_assemble(const RandoAssembleRequest *request, RandoAssembleWork *work, RandoBpsResult *bps)
{
	uint8_t digest[RANDO_MD5_SIZE];
	RandoBpsResult code = RANDO_BPS_OK;
	RandoAssembleStatus status;
	RandoRomBuffer rom;
	RandoRomWriter writer;
	size_t target_size;
	bool ok;

	assert(request != NULL && work != NULL && request->output != NULL);
	if (bps != NULL)
		*bps = RANDO_BPS_OK;
	if (request->source_size != RANDO_ROM_SOURCE_SIZE)
		return RANDO_ASSEMBLE_SOURCE_SIZE;
	assert(request->source != NULL);
	rando_md5(request->source, request->source_size, digest);
	if (memcmp(digest, request->source_md5, RANDO_MD5_SIZE) != 0)
		return RANDO_ASSEMBLE_SOURCE_MD5;

	status = rando_assemble_check_patch(request->patch, request->patch_size, &code);
	if (bps != NULL)
		*bps = code;
	if (status != RANDO_ASSEMBLE_OK)
		return status;
	code = rando_bps_apply(request->patch, request->patch_size, request->source, request->source_size, work->rom,
			       RANDO_ROM_SIZE, &target_size);
	if (bps != NULL)
		*bps = code;
	if (code != RANDO_BPS_OK)
		return RANDO_ASSEMBLE_PATCH;
	assert(target_size == RANDO_ROM_SIZE);
	rando_md5(work->rom, RANDO_ROM_SIZE, digest);
	if (memcmp(digest, request->base_md5, RANDO_MD5_SIZE) != 0)
		return RANDO_ASSEMBLE_BASE_MD5;

	switch (rando_generate(&work->gen, &request->options, request->seed, request->max_attempts, request->trace,
			       request->trace_ctx)) {
	case RANDO_COMPLETE:
		break;
	case RANDO_UNSUPPORTED:
		return RANDO_ASSEMBLE_UNSUPPORTED;
	default:
		return RANDO_ASSEMBLE_GAVE_UP;
	}

	rom.bytes = work->rom;
	rom.size = RANDO_ROM_SIZE;
	writer.write = rando_rom_buffer_write;
	writer.ctx = &rom;
	/* Every write is below 2 MiB; nothing can refuse it. */
	ok = rando_rom_write(&work->gen, request->heart_beep, &writer);
	assert(ok);
	ok = rando_rom_write_cosmetic_defaults(&rom, request->source, request->source_size);
	assert(ok);
	ok = rando_rom_update_checksum(&rom);
	assert(ok);
	(void)ok;
	if (!request->output(request->output_ctx, work->rom, RANDO_ROM_SIZE))
		return RANDO_ASSEMBLE_OUTPUT;
	return RANDO_ASSEMBLE_OK;
}
