#include "ladx_groups.h"

#include <string.h>

#define BANK3E (0x3Eu * 0x4000u)
#define SEED_AT (BANK3E + 0x2F00u)
#define SETTINGS_AT (BANK3E + 0x2F20u)
#define TEXTS 0
#define MASTER_SKULL_TEXT 0x111u
#define OWL_TEXT_SLOT 0xBEu

static LadxSpan group_name(void)
{
	LadxSpan s;

	s.at = "bank3e";
	s.len = 6;
	return s;
}

static LadxWlogKey index_key(uint32_t index)
{
	LadxWlogKey k;

	k.index = index;
	k.alt.at = NULL;
	k.alt.len = 0;
	return k;
}

static int computed(LadxEncoder *enc, LadxPipeline *p, uint32_t offset, const uint8_t *bytes, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		p->origins[i] = LADX_ENC_KNOWN;
	return n == 0 || ladx_enc_bank(enc, group_name(), offset, bytes, p->origins, n) == LADX_ENC_OK;
}

int ladx_group_bank3e(LadxEncoder *enc, LadxPipeline *p)
{
	char text[LADX_SETTINGS_SHORT_MAX + 1];
	const uint8_t *seed;
	size_t n;
	int len;

	if (!p->rom || !p->randomizer || !ladx_pipeline_const(enc, p, "bank3e"))
		return 0;
	if (!ladx_enc_entry_value(enc, TEXTS, index_key(MASTER_SKULL_TEXT), p->values, p->origins, LADX_WLOG_ATOM_MAX,
				  &n) ||
	    ladx_enc_table(enc, group_name(), TEXTS, index_key(OWL_TEXT_SLOT), p->values, p->origins, n) != LADX_ENC_OK)
		return 0;
	seed = ladx_randomizer_seed(p->randomizer, &n);
	if (!computed(enc, p, SEED_AT, seed, n))
		return 0;
	if (p->settings->value[LADX_SETTING_RACE])
		return 1;
	len = ladx_settings_short_string(p->settings, text, sizeof text);
	/* LADXR's len(...).to_bytes(1, 'little') raises over 255. */
	if (len < 0 || len > 255)
		return 0;
	p->values[0] = (uint8_t)len;
	memcpy(p->values + 1, text, (size_t)len);
	return computed(enc, p, SETTINGS_AT, p->values, 1) && computed(enc, p, SETTINGS_AT + 1, p->values + 1, (size_t)len);
}
