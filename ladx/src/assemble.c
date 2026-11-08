#include "ladx_assemble.h"

#include <string.h>

static void discard(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	(void)text;
	(void)len;
}

static int hex_value(char c)
{
	return c <= '9' ? c - '0' : c - 'a' + 10;
}

static int base_matches(LadxAssembler *a, const LadxAssembleInput *in)
{
	uint8_t want[LADX_SHA1_SIZE];
	LadxSha1 sha;
	int i;

	ladx_sha1_init(&sha);
	ladx_sha1_update(&sha, in->base, in->base_len);
	ladx_sha1_final(&sha, a->base_digest);
	if (in->base_len != LADX_ROM_SIZE)
		return 0;
	if (in->base_sha1)
		return memcmp(a->base_digest, in->base_sha1, LADX_SHA1_SIZE) == 0;
	for (i = 0; i < LADX_SHA1_SIZE; i++)
		want[i] = (uint8_t)(hex_value(LADX_ROM_SHA1_HEX[2 * i]) << 4 | hex_value(LADX_ROM_SHA1_HEX[2 * i + 1]));
	return memcmp(a->base_digest, want, LADX_SHA1_SIZE) == 0;
}

typedef struct Image {
	uint8_t *bytes;
} Image;

static int image_read(void *ctx, uint32_t offset, uint8_t *out, size_t n)
{
	memcpy(out, ((Image *)ctx)->bytes + offset, n);
	return 1;
}

static int image_write(void *ctx, uint32_t offset, const uint8_t *bytes, size_t n)
{
	memcpy(((Image *)ctx)->bytes + offset, bytes, n);
	return 1;
}

static LadxAssembleStatus pipeline(LadxAssembler *a, const LadxAssembleInput *in)
{
	LadxPipeline *p = &a->pipeline;

	if (ladx_wlog_begin(&a->log, ladx_tables_names(in->tables), in->log ? in->log : discard, in->log_ctx) !=
		    LADX_WLOG_OK ||
	    ladx_enc_init(&a->encoder, in->base, in->base_len, &a->ks, &a->rom_tables, &a->log) != LADX_ENC_OK) {
		memset(&a->pipeline_error, 0, sizeof a->pipeline_error);
		a->pipeline_error.status = LADX_PIPELINE_ENCODER;
		a->pipeline_error.encoder = LADX_ENC_BAD_INPUT;
		return LADX_ASSEMBLE_PIPELINE;
	}
	a->rom.bytes = in->base;
	a->rom.len = in->base_len;
	a->rom.tables = &a->rom_tables;
	a->rom.ks = &a->ks;
	p->tables = in->tables;
	p->settings = in->settings;
	p->spot_count = ladx_tables_spot_count(ladx_randomizer_placement(&a->randomizer));
	p->randomizer = &a->randomizer;
	p->rnd = &a->randomizer.rnd;
	p->global = &a->randomizer.global;
	p->rom = &a->rom;
	if (in->groups) {
		p->groups = in->groups;
		p->group_count = in->group_count;
	} else {
		p->groups = ladx_pipeline_groups(&p->group_count);
	}
	p->ctx = NULL;
	a->spoiler.built = 0;
	p->spoiler = &a->spoiler;
	return ladx_pipeline_run(p, &a->encoder, &a->pipeline_error) == LADX_PIPELINE_OK ? LADX_ASSEMBLE_OK
											  : LADX_ASSEMBLE_PIPELINE;
}

LadxAssembleStatus ladx_assemble(LadxAssembler *a, const LadxAssembleInput *in, uint8_t *out)
{
	static const uint8_t name[] = {'L', 'A', 'D', 'X', 'R'};
	LadxSettingPair finding;
	LadxRandomizeStatus rs;
	LadxAssembleStatus st;
	LadxHeaderIo io;
	Image image;
	uint32_t i;
	int32_t origin;

	if (!a || !in || !out || !in->base || !in->tables || !in->settings || (!in->seed && in->seed_len) ||
	    (out < in->base + in->base_len && in->base < out + LADX_ROM_SIZE))
		return LADX_ASSEMBLE_BAD_INPUT;
	a->spoiler.built = 0;
	if (!base_matches(a, in))
		return LADX_ASSEMBLE_WRONG_BASE;
	if (ladx_rom_tables_parse(in->base, in->base_len, &a->rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&a->ks, in->base, in->base_len, &a->rom_tables) != LADX_KS_OK)
		return LADX_ASSEMBLE_BAD_BASE;
	if (!ladx_settings_supported(in->settings, &finding))
		return LADX_ASSEMBLE_UNSUPPORTED;
	rs = ladx_randomize(&a->randomizer, in->settings, in->seed, in->seed_len, in->trace, in->trace_ctx);
	if (rs == LADX_RANDOMIZE_UNSUPPORTED)
		return LADX_ASSEMBLE_UNSUPPORTED;
	if (rs != LADX_RANDOMIZE_OK)
		return LADX_ASSEMBLE_BROKEN;
	if (!ladx_randomizer_placement(&a->randomizer))
		return LADX_ASSEMBLE_OUTCOME;
	if ((st = pipeline(a, in)) != LADX_ASSEMBLE_OK)
		return st;
	for (i = 0; i < LADX_ROM_SIZE; i++)
		if (!ladx_enc_bank_value(&a->encoder, i, &out[i], &origin))
			return LADX_ASSEMBLE_IMAGE;
	image.bytes = out;
	io.read = image_read;
	io.write = image_write;
	io.ctx = &image;
	return ladx_header_fix(&io, LADX_ROM_SIZE, name, sizeof name) ? LADX_ASSEMBLE_OK : LADX_ASSEMBLE_IMAGE;
}

const char *ladx_assemble_status_name(LadxAssembleStatus status)
{
	static const char *const names[] = {"ok",	   "bad input", "wrong base", "bad base", "unsupported",
					    "broken", "outcome",   "pipeline",   "image"};

	return (unsigned)status < sizeof names / sizeof names[0] ? names[status] : "unknown";
}
