#include "ladx_groups.h"

#include <string.h>

#define TEXTS 0
#define KEEP_FIRST 0x217u
#define KEEP_LAST 0x21Au

static LadxSpan group_name(void)
{
	LadxSpan s;

	s.at = "notext";
	s.len = 6;
	return s;
}

int ladx_group_notext(LadxEncoder *enc, LadxPipeline *p)
{
	const char *mode;
	LadxWlogKey key;
	uint32_t index, pointer;
	size_t n;
	int kind;

	if (!p->rom)
		return 0;
	mode = ladx_setting_value_name(LADX_SETTING_TEXTMODE, p->settings->value[LADX_SETTING_TEXTMODE]);
	if (!mode || strcmp(mode, "none") != 0)
		return 1;
	key.alt.at = NULL;
	key.alt.len = 0;
	for (index = 0; index < p->rom->tables->table[TEXTS].count; index++) {
		if (index >= KEEP_FIRST && index <= KEEP_LAST)
			continue;
		key.index = index;
		kind = ladx_enc_entry_pointer(enc, TEXTS, key, &pointer);
		if (kind < 0)
			return 0;
		if (kind == 1)
			continue;
		if (!ladx_enc_entry_value(enc, TEXTS, key, p->values, p->origins, LADX_WLOG_ATOM_MAX, &n))
			return 0;
		if (n > 0) {
			p->values[0] = p->values[n - 1];
			p->origins[0] = p->origins[n - 1];
			n = 1;
		}
		if (ladx_enc_table(enc, group_name(), TEXTS, key, p->values, p->origins, n) != LADX_ENC_OK)
			return 0;
	}
	return 1;
}
