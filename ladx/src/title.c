#include "ladx_groups.h"
#include "ladx_md5.h"

#include <string.h>

#define FLAG_AT 0x0007u
#define BG_TILES 9
#define BG_ATTRIBUTES 10
#define TEXT_MAX 64
#define LINE 16

static LadxSpan group_name(void)
{
	LadxSpan s;

	s.at = "title";
	s.len = 5;
	return s;
}

static uint8_t encode_char(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (uint8_t)(c - 'A');
	if (c >= 'a' && c <= 'y')
		return (uint8_t)(c - 'a' + 26);
	if (c >= '0' && c <= '9')
		return (uint8_t)(c - '0' + 0x70);
	switch (c) {
	case 'z':
		return 0x3E;
	case '-':
		return 0x3F;
	case '.':
		return 0x39;
	case ':':
		return 0x42;
	case '?':
		return 0x3C;
	case '!':
		return 0x3D;
	default:
		return 0x7E;
	}
}

void ladx_title_encode_text(const char *text, size_t n, uint8_t *out)
{
	size_t i;

	for (i = 0; i < n; i++)
		out[i] = encode_char(text[i]);
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

uint64_t ladx_title_seed_number(const char *text, size_t n)
{
	uint8_t digest[LADX_MD5_SIZE], c;
	uint64_t r = 0;
	LadxMd5 md5;
	size_t i;

	for (i = 0; i < n && hex_digit(text[i]) >= 0; i++)
		;
	if (n > 0 && i == n) {
		for (i = 0; i < n; i++)
			r = (r * 16u + (uint64_t)hex_digit(text[i])) % LADX_TITLE_SEED_MODULUS;
		return r;
	}
	ladx_md5_init(&md5);
	for (i = 0; i < n; i++) {
		c = (uint8_t)text[i];
		if (c > 0x7F)
			c = '?';
		ladx_md5_update(&md5, &c, 1);
	}
	ladx_md5_final(&md5, digest);
	for (i = 0; i < LADX_MD5_SIZE; i++)
		r = (r * 256u + digest[i]) % LADX_TITLE_SEED_MODULUS;
	return r;
}

int ladx_title_edit(LadxBackground *bg, int attributes, const char *text, size_t n, uint64_t seed_number)
{
	uint8_t line[TEXT_MAX];
	uint32_t i, tile, pal;
	uint64_t sn = seed_number;

	if (n > TEXT_MAX)
		return 0;
	ladx_title_encode_text(text, n, line);
	for (i = 0; i < n; i++)
		ladx_background_set(bg, (i < LINE ? 0x9A01u : 0x9A21u - LINE) + i, attributes ? 0x00 : line[i]);
	if (!attributes)
		for (i = 0x09; i < 0x14; i++) {
			ladx_background_set(bg, 0x9820u + i, 0x7F);
			ladx_background_set(bg, 0x9840u + i, (uint8_t)(0xA0 + i % 2));
			ladx_background_set(bg, 0x9860u + i, 0xA2);
		}
	for (i = 0x0A; i < 0x14; i += 2) {
		tile = 0xA0u + (uint32_t)(sn % 14u) * 4u;
		sn /= 14u;
		pal = (uint32_t)(sn % 8u);
		sn /= 8u;
		ladx_background_set(bg, 0x9800u + i, (uint8_t)(attributes ? 0x08u | pal : tile));
		ladx_background_set(bg, 0x9820u + i, (uint8_t)(attributes ? 0x08u | pal : tile + 1));
		ladx_background_set(bg, 0x9801u + i, (uint8_t)(attributes ? 0x08u | pal : tile + 2));
		ladx_background_set(bg, 0x9821u + i, (uint8_t)(attributes ? 0x08u | pal : tile + 3));
	}
	return 1;
}

static LadxWlogKey index_key(uint32_t index)
{
	LadxWlogKey k;

	k.index = index;
	k.alt.at = NULL;
	k.alt.len = 0;
	return k;
}

static int store(LadxEncoder *enc, LadxPipeline *p, int table, uint32_t index, const char *text, size_t n,
		 uint64_t number)
{
	size_t len, i;

	if (!ladx_enc_entry_value(enc, table, index_key(index), p->values, p->origins, LADX_WLOG_ATOM_MAX, &len) ||
	    !ladx_background_decode(&p->background, p->values, len) ||
	    !ladx_title_edit(&p->background, table == BG_ATTRIBUTES, text, n, number) ||
	    (len = ladx_background_encode(&p->background, p->values, LADX_WLOG_ATOM_MAX)) == 0)
		return 0;
	for (i = 0; i < len; i++)
		p->origins[i] = LADX_ENC_KNOWN;
	return ladx_enc_table(enc, group_name(), table, index_key(index), p->values, p->origins, len) == LADX_ENC_OK;
}

int ladx_group_title(LadxEncoder *enc, LadxPipeline *p)
{
	static const char hex[] = "0123456789ABCDEF";
	char text[2 * LADX_RANDOMIZER_SEED_MAX];
	const uint8_t *seed;
	size_t n, i;
	uint64_t number;
	uint32_t index;
	uint8_t flag;
	int race;

	if (!p->rom || !p->randomizer)
		return 0;
	seed = ladx_randomizer_seed(p->randomizer, &n);
	for (i = 0; i < n; i++) {
		text[2 * i] = hex[seed[i] >> 4];
		text[2 * i + 1] = hex[seed[i] & 0xF];
	}
	n *= 2;
	number = ladx_title_seed_number(text, n);
	race = p->settings->value[LADX_SETTING_RACE] != 0;
	if (race) {
		memcpy(text, "Race", 4);
		n = 4;
	}
	flag = race ? 0x01 : 0x52;
	p->origins[0] = LADX_ENC_KNOWN;
	if (ladx_enc_bank(enc, group_name(), FLAG_AT, &flag, p->origins, 1) != LADX_ENC_OK)
		return 0;
	for (index = 3; index <= 4; index++)
		if (!store(enc, p, BG_TILES, index, text, n, number) ||
		    !store(enc, p, BG_ATTRIBUTES, index, text, n, number))
			return 0;
	return 1;
}
