#include "ladx_text.h"

#include <stdio.h>
#include <string.h>

typedef struct LadxTextName {
	const char *key, *value;
} LadxTextName;

static const LadxTextName names[] = {
	{"SWORD", "Sword"},
	{"BOMB", "Bombs"},
	{"POWER_BRACELET", "Power Bracelet"},
	{"SHIELD", "Shield"},
	{"BOW", "Bow"},
	{"HOOKSHOT", "Hookshot"},
	{"MAGIC_ROD", "Magic Rod"},
	{"PEGASUS_BOOTS", "Pegasus Boots"},
	{"OCARINA", "Ocarina"},
	{"FEATHER", "Roc's Feather"},
	{"SHOVEL", "Shovel"},
	{"MAGIC_POWDER", "Magic Powder"},
	{"BOOMERANG", "Boomerang"},
	{"ROOSTER", "Flying Rooster"},
	{"HAMMER", "Hammer"},
	{"FLIPPERS", "Flippers"},
	{"SLIME_KEY", "Slime key"},
	{"TAIL_KEY", "Tail key"},
	{"ANGLER_KEY", "Angler key"},
	{"FACE_KEY", "Face key"},
	{"BIRD_KEY", "Bird key"},
	{"GOLD_LEAF", "Golden leaf"},
	{"RUPEE", "Rupee"},
	{"RUPEES", "Rupees"},
	{"RUPEES_50", "50 Rupees"},
	{"RUPEES_20", "20 Rupees"},
	{"RUPEES_100", "100 Rupees"},
	{"RUPEES_200", "200 Rupees"},
	{"RUPEES_500", "500 Rupees"},
	{"SEASHELL", "Secret Seashell"},
	{"KEY", "Small Key"},
	{"KEY1", "Key for Tail Cave"},
	{"KEY2", "Key for Bottle Grotto"},
	{"KEY3", "Key for Key Cavern"},
	{"KEY4", "Key for Angler's Tunnel"},
	{"KEY5", "Key for Catfish's Maw"},
	{"KEY6", "Key for Face Shrine"},
	{"KEY7", "Key for Eagle's Tower"},
	{"KEY8", "Key for Turtle Rock"},
	{"KEY0", "Key for Color Dungeon"},
	{"MAP", "Dungeon Map"},
	{"MAP1", "Map for Tail Cave"},
	{"MAP2", "Map for Bottle Grotto"},
	{"MAP3", "Map for Key Cavern"},
	{"MAP4", "Map for Angler's Tunnel"},
	{"MAP5", "Map for Catfish's Maw"},
	{"MAP6", "Map for Face Shrine"},
	{"MAP7", "Map for Eagle's Tower"},
	{"MAP8", "Map for Turtle Rock"},
	{"MAP0", "Map for Color Dungeon"},
	{"COMPASS", "Dungeon Compass"},
	{"COMPASS1", "Compass for Tail Cave"},
	{"COMPASS2", "Compass for Bottle Grotto"},
	{"COMPASS3", "Compass for Key Cavern"},
	{"COMPASS4", "Compass for Angler's Tunnel"},
	{"COMPASS5", "Compass for Catfish's Maw"},
	{"COMPASS6", "Compass for Face Shrine"},
	{"COMPASS7", "Compass for Eagle's Tower"},
	{"COMPASS8", "Compass for Turtle Rock"},
	{"COMPASS0", "Compass for Color Dungeon"},
	{"STONE_BEAK", "Stone Beak"},
	{"STONE_BEAK1", "Stone Beak for Tail Cave"},
	{"STONE_BEAK2", "Stone Beak for Bottle Grotto"},
	{"STONE_BEAK3", "Stone Beak for Key Cavern"},
	{"STONE_BEAK4", "Stone Beak for Angler's Tunnel"},
	{"STONE_BEAK5", "Stone Beak for Catfish's Maw"},
	{"STONE_BEAK6", "Stone Beak for Face Shrine"},
	{"STONE_BEAK7", "Stone Beak for Eagle's Tower"},
	{"STONE_BEAK8", "Stone Beak for Turtle Rock"},
	{"STONE_BEAK0", "Stone Beak for Color Dungeon"},
	{"NIGHTMARE_KEY", "Nightmare Key"},
	{"NIGHTMARE_KEY1", "Nightmare Key for Tail Cave"},
	{"NIGHTMARE_KEY2", "Nightmare Key for Bottle Grotto"},
	{"NIGHTMARE_KEY3", "Nightmare Key for Key Cavern"},
	{"NIGHTMARE_KEY4", "Nightmare Key for Angler's Tunnel"},
	{"NIGHTMARE_KEY5", "Nightmare Key for Catfish's Maw"},
	{"NIGHTMARE_KEY6", "Nightmare Key for Face Shrine"},
	{"NIGHTMARE_KEY7", "Nightmare Key for Eagle's Tower"},
	{"NIGHTMARE_KEY8", "Nightmare Key for Turtle Rock"},
	{"NIGHTMARE_KEY0", "Nightmare Key for Color Dungeon"},
	{"HEART_PIECE", "Piece of Heart"},
	{"BOWWOW", "Bowwow"},
	{"ARROWS_10", "10 Arrows"},
	{"SINGLE_ARROW", "Single Arrow"},
	{"MEDICINE", "Medicine"},
	{"MAX_POWDER_UPGRADE", "Magic Powder upgrade"},
	{"MAX_BOMBS_UPGRADE", "Bombs upgrade"},
	{"MAX_ARROWS_UPGRADE", "Arrows upgrade"},
	{"RED_TUNIC", "Red Tunic"},
	{"BLUE_TUNIC", "Blue Tunic"},
	{"HEART_CONTAINER", "Heart Container"},
	{"BAD_HEART_CONTAINER", "Anti-Heart Container"},
	{"TOADSTOOL", "Toadstool"},
	{"SONG1", "Ballad of the Wind Fish"},
	{"SONG2", "Manbo's Mambo"},
	{"SONG3", "Frog's Song of Soul"},
	{"INSTRUMENT1", "Full Moon Cello"},
	{"INSTRUMENT2", "Conch Horn"},
	{"INSTRUMENT3", "Sea Lily's Bell"},
	{"INSTRUMENT4", "Surf Harp"},
	{"INSTRUMENT5", "Wind Marimba"},
	{"INSTRUMENT6", "Coral Triangle"},
	{"INSTRUMENT7", "Organ of Evening Calm"},
	{"INSTRUMENT8", "Thunder Drum"},
	{"TRADING_ITEM_YOSHI_DOLL", "Yoshi Doll"},
	{"TRADING_ITEM_RIBBON", "Ribbon"},
	{"TRADING_ITEM_DOG_FOOD", "Dog Food"},
	{"TRADING_ITEM_BANANAS", "Bananas"},
	{"TRADING_ITEM_STICK", "Stick"},
	{"TRADING_ITEM_HONEYCOMB", "Honeycomb"},
	{"TRADING_ITEM_PINEAPPLE", "Pineapple"},
	{"TRADING_ITEM_HIBISCUS", "Hibiscus"},
	{"TRADING_ITEM_LETTER", "Letter"},
	{"TRADING_ITEM_BROOM", "Broom"},
	{"TRADING_ITEM_FISHING_HOOK", "Fishing Hook"},
	{"TRADING_ITEM_NECKLACE", "Necklace"},
	{"TRADING_ITEM_SCALE", "Scale"},
	{"TRADING_ITEM_MAGNIFYING_GLASS", "Magnifying Lens"},
	{"TAIL_CAVE_OPENED", "Tail Cave open"},
	{"KEY_CAVERN_OPENED", "Key Cavern open"},
	{"ANGLER_TUNNEL_OPENED", "Angler Tunnel open"},
	{"FACE_SHRINE_OPENED", "Face Shrine open"},
	{"CASTLE_GATE_OPENED", "Castle Gate open"},
	{"EAGLE_TOWER_OPENED", "Eagle Tower open"},
};

static const struct {
	const char *tag;
	uint8_t code;
} characters[] = {
	{"'", 0x5E},
	{"<agrave>", 0x80},
	{"<acirc>", 0x81},
	{"<auml>", 0x82},
	{"<egrave>", 0x83},
	{"<eacute>", 0x84},
	{"<ecirc>", 0x85},
	{"<ugrave>", 0x86},
	{"<ucirc>", 0x87},
	{"<uuml>", 0x88},
	{"<ocirc>", 0x89},
	{"<ouml>", 0x8A},
	{"<ccedil>", 0x8B},
	{"<icirc>", 0x8C},
	{"<bigA>", 0xD0},
	{"<bigB>", 0xD1},
	{"<bigC>", 0xD2},
	{"<bigD>", 0xD3},
	{"<bigE>", 0xD4},
	{"<bigF>", 0xD5},
	{"<foot>", 0xDA},
	{"<master>", 0xDC},
	{"<link>", 0xDD},
	{"<marin>", 0xDE},
	{"<tarkin>", 0xDF},
	{"<yoshi>", 0xE0},
	{"<ribbon>", 0xE1},
	{"<dogfood>", 0xE2},
	{"<bananas>", 0xE3},
	{"<stick>", 0xE4},
	{"<honeycomb>", 0xE5},
	{"<pineapple>", 0xE6},
	{"<hibiscus>", 0xE7},
	{"<broom>", 0xE8},
	{"<hook>", 0xE9},
	{"<necklace>", 0xEA},
	{"<scale>", 0xEB},
	{"<glass>", 0xEC},
	{"<letter>", 0xED},
	{"<dpad>", 0xEE},
	{"<arrowU>", 0xF0},
	{"<arrowD>", 0xF1},
	{"<arrowL>", 0xF2},
	{"<arrowR>", 0xF3},
};

typedef struct Out {
	uint8_t *at;
	size_t cap, len;
	int over;
} Out;

static void put(Out *o, uint8_t c)
{
	if (o->len >= o->cap)
		o->over = 1;
	else
		o->at[o->len++] = c;
}

static void put_spaces(Out *o, long n)
{
	for (; n > 0; n--)
		put(o, ' ');
}

static int is_space(uint8_t c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static void rstrip(Out *o)
{
	while (o->len > 0 && is_space(o->at[o->len - 1]))
		o->len--;
}

static LadxTextStatus crash(LadxOutcome *raised, const char *type)
{
	raised->kind = LADX_OUTCOME_CRASH;
	raised->type = type;
	raised->message = raised->text;
	return LADX_TEXT_RAISED;
}

static LadxTextStatus format_names(const char *in, size_t n, char *buf, size_t *len, LadxOutcome *raised)
{
	size_t i = 0, k, at = 0, j, m;

	while (i < n) {
		const char *value = NULL;

		if (in[i] == '}') {
			if (i + 1 >= n || in[i + 1] != '}')
				return LADX_TEXT_UNSUPPORTED;
			value = "}";
			i += 2;
		} else if (in[i] == '{' && i + 1 < n && in[i + 1] == '{') {
			value = "{";
			i += 2;
		} else if (in[i] == '{') {
			for (j = i + 1; j < n && in[j] != '}'; j++)
				if (strchr("{!:.[]'\\", in[j]) || (unsigned char)in[j] < 0x20 || (unsigned char)in[j] > 0x7E)
					return LADX_TEXT_UNSUPPORTED;
			m = j - i - 1;
			if (j == n || m == 0 || strspn(in + i + 1, "0123456789") >= m)
				return LADX_TEXT_UNSUPPORTED;
			for (k = 0; k < sizeof names / sizeof names[0]; k++)
				if (strlen(names[k].key) == m && memcmp(names[k].key, in + i + 1, m) == 0)
					value = names[k].value;
			if (!value) {
				snprintf(raised->text, sizeof raised->text, "'%.*s'", (int)m, in + i + 1);
				return crash(raised, "KeyError");
			}
			i = j + 1;
		} else {
			if (at >= LADX_TEXT_MAX)
				return LADX_TEXT_OVERFLOW;
			buf[at++] = in[i++];
			continue;
		}
		m = strlen(value);
		if (at + m > LADX_TEXT_MAX)
			return LADX_TEXT_OVERFLOW;
		memcpy(buf + at, value, m);
		at += m;
	}
	*len = at;
	return LADX_TEXT_OK;
}

static LadxTextStatus check_ascii(const char *s, size_t n, LadxOutcome *raised)
{
	size_t i, j;

	for (i = 0; i < n && (unsigned char)s[i] < 0x80; i++)
		;
	if (i == n)
		return LADX_TEXT_OK;
	for (j = i; j < n && (unsigned char)s[j] >= 0x80; j++)
		;
	if (j - i == 1)
		snprintf(raised->text, sizeof raised->text,
			 "'ascii' codec can't encode character '\\x%02x' in position %lu: ordinal not in range(128)",
			 (unsigned char)s[i], (unsigned long)i);
	else
		snprintf(raised->text, sizeof raised->text,
			 "'ascii' codec can't encode characters in position %lu-%lu: ordinal not in range(128)",
			 (unsigned long)i, (unsigned long)(j - 1));
	return crash(raised, "UnicodeEncodeError");
}

/* bytes.replace of every tag, in _CHARACTERS' order; each code is one byte, so in place. */
static size_t replace_characters(char *s, size_t n)
{
	size_t c, i, at, len;

	for (c = 0; c < sizeof characters / sizeof characters[0]; c++) {
		len = strlen(characters[c].tag);
		for (i = 0, at = 0; i < n;) {
			if (i + len <= n && memcmp(s + i, characters[c].tag, len) == 0) {
				s[at++] = (char)characters[c].code;
				i += len;
			} else {
				s[at++] = s[i++];
			}
		}
		n = at;
	}
	return n;
}

static void pad_line(Out *o, size_t start, int center)
{
	long padding = 16 - (long)(o->len - start), left;

	if (padding <= 0)
		return;
	if (!center) {
		put_spaces(o, padding);
		return;
	}
	left = padding / 2;
	put_spaces(o, padding);
	if (o->over)
		return;
	memmove(o->at + start + left, o->at + start, o->len - (size_t)padding - start);
	memset(o->at + start, ' ', (size_t)left);
}

LadxTextStatus ladx_format_text(const char *in, size_t n, int center, const char *ask, uint8_t *out, size_t cap,
				size_t *out_len, LadxOutcome *raised)
{
	char s[LADX_TEXT_MAX];
	size_t len, i, word, line_start;
	LadxTextStatus st;
	Out o;

	memset(raised, 0, sizeof *raised);
	if ((st = format_names(in, n, s, &len, raised)) != LADX_TEXT_OK || (st = check_ascii(s, len, raised)) != LADX_TEXT_OK)
		return st;
	len = replace_characters(s, len);
	o.at = out;
	o.cap = cap;
	o.len = 0;
	o.over = 0;
	line_start = 0;
	for (i = 0; i <= len;) {
		size_t end = i, rl = 0;

		while (end < len && s[end] != '\n')
			end++;
		line_start = o.len;
		for (word = i; word <= end;) {
			size_t w = word;

			while (w < end && s[w] != ' ')
				w++;
			if (rl + 1 + (w - word) > 16) {
				pad_line(&o, line_start, center);
				line_start = o.len;
				rl = 0;
			} else if (rl) {
				put(&o, ' ');
				rl++;
			}
			rl += w - word;
			for (; word < w; word++)
				put(&o, (uint8_t)s[word]);
			word = w + 1;
		}
		if (rl)
			pad_line(&o, line_start, center);
		i = end + 1;
	}
	if (ask) {
		rstrip(&o);
		while (o.len % 16 != 0 && !o.over)
			put(&o, ' ');
		put_spaces(&o, 4);
		for (i = 0; ask[i]; i++)
			put(&o, (uint8_t)ask[i]);
		put(&o, 0xFE);
	} else {
		for (i = 0; i < o.len; i++)
			if (o.at[i] == '_')
				o.at[i] = ' ';
		rstrip(&o);
		put(&o, 0xFF);
	}
	if (o.over)
		return LADX_TEXT_OVERFLOW;
	*out_len = o.len;
	return LADX_TEXT_OK;
}
