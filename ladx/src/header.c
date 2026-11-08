#include "ladx_groups.h"

#include <string.h>

#define NAME_AT 0x134u
#define CHECKSUM_AT 0x14Du
#define GLOBAL_AT 0x14Eu
#define HEADER_END 0x150u
#define CHUNK 1024u

static int write_byte(const LadxHeaderIo *io, uint32_t offset, unsigned value)
{
	uint8_t v = (uint8_t)value;

	return io->write(io->ctx, offset, &v, 1);
}

int ladx_header_fix(const LadxHeaderIo *io, size_t len, const uint8_t *name, size_t n)
{
	uint8_t buf[CHUNK];
	unsigned sum = 0;
	size_t at, k, take;

	if (len < HEADER_END)
		return 0;
	if (name) {
		memset(buf, 0, LADX_HEADER_NAME_LEN);
		memcpy(buf, name, n < LADX_HEADER_NAME_LEN ? n : LADX_HEADER_NAME_LEN);
		if (!io->write(io->ctx, NAME_AT, buf, LADX_HEADER_NAME_LEN))
			return 0;
	}
	if (!io->read(io->ctx, NAME_AT, buf, CHECKSUM_AT - NAME_AT))
		return 0;
	for (k = 0; k < CHECKSUM_AT - NAME_AT; k++)
		sum -= buf[k] + 1u;
	if (!write_byte(io, CHECKSUM_AT, sum & 0xFFu) || !write_byte(io, GLOBAL_AT, 0) ||
	    !write_byte(io, GLOBAL_AT + 1, 0))
		return 0;
	sum = 0;
	for (at = 0; at < len; at += take) {
		take = len - at < CHUNK ? len - at : CHUNK;
		if (!io->read(io->ctx, (uint32_t)at, buf, take))
			return 0;
		for (k = 0; k < take; k++)
			sum += buf[k];
		sum &= 0xFFFFu;
	}
	return write_byte(io, GLOBAL_AT, sum >> 8) && write_byte(io, GLOBAL_AT + 1, sum & 0xFFu);
}

typedef struct EncIo {
	LadxEncoder *enc;
	LadxPipeline *p;
} EncIo;

static int enc_read(void *ctx, uint32_t offset, uint8_t *out, size_t n)
{
	EncIo *e = ctx;
	int32_t origin;
	size_t i;

	for (i = 0; i < n; i++)
		if (!ladx_enc_bank_value(e->enc, offset + (uint32_t)i, &out[i], &origin))
			return 0;
	return 1;
}

static int enc_write(void *ctx, uint32_t offset, const uint8_t *bytes, size_t n)
{
	EncIo *e = ctx;
	LadxSpan group;
	size_t i;

	group.at = "header";
	group.len = 6;
	for (i = 0; i < n; i++)
		e->p->origins[i] = LADX_ENC_KNOWN;
	return ladx_enc_bank(e->enc, group, offset, bytes, e->p->origins, n) == LADX_ENC_OK;
}

int ladx_group_header(LadxEncoder *enc, LadxPipeline *p)
{
	static const uint8_t name[] = {'L', 'A', 'D', 'X', 'R'};
	LadxHeaderIo io;
	EncIo e;

	if (!p->rom)
		return 0;
	e.enc = enc;
	e.p = p;
	io.read = enc_read;
	io.write = enc_write;
	io.ctx = &e;
	return ladx_header_fix(&io, p->rom->len, name, sizeof name);
}
