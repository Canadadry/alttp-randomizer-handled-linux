#include "ladx_groups.h"
#include "marin_lines.h"

#include <stdio.h>
#include <string.h>

#define TEXTS 0
#define DIGITS_MAX 18

const char *ladx_marin_line(size_t i, size_t *len)
{
	if (i >= LADX_MARIN_LINE_COUNT)
		return NULL;
	*len = strlen(ladx_marin_lines[i]);
	return ladx_marin_lines[i];
}

typedef struct Str {
	char at[LADX_TEXT_MAX];
	size_t len;
} Str;

static int ascii_space(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int append(Str *s, const char *bytes, size_t n)
{
	if (s->len + n > LADX_TEXT_MAX)
		return 0;
	memcpy(s->at + s->len, bytes, n);
	s->len += n;
	return 1;
}

/* bytes.strip() then decode("unicode_escape"): the simple escapes only. */
static LadxTextStatus decode(const char *line, size_t n, Str *s)
{
	static const char simple[] = "\\\\''\"\"a\ab\bf\fn\nr\rt\tv\v";
	const char *e;
	size_t i;
	char c;

	while (n > 0 && ascii_space(line[n - 1]))
		n--;
	while (n > 0 && ascii_space(*line)) {
		line++;
		n--;
	}
	s->len = 0;
	for (i = 0; i < n; i++) {
		c = line[i];
		if (c == '\\') {
			if (++i == n)
				return LADX_TEXT_UNSUPPORTED;
			if (line[i] == '\n')
				continue;
			for (e = simple; *e && *e != line[i]; e += 2)
				;
			if (*e)
				c = e[1];
			else if (strchr("01234567xuUN", line[i]))
				return LADX_TEXT_UNSUPPORTED;
			else if (!append(s, "\\", 1))
				return LADX_TEXT_OVERFLOW;
			else
				c = line[i];
		}
		if (!append(s, &c, 1))
			return LADX_TEXT_OVERFLOW;
	}
	return LADX_TEXT_OK;
}

static LadxTextStatus replace_amount(Str *s)
{
	static const char tag[] = "<marinLinesAmount>";
	char amount[8];
	Str out;
	size_t i, k = sizeof tag - 1;

	sprintf(amount, "%d", LADX_MARIN_LINE_COUNT);
	out.len = 0;
	for (i = 0; i < s->len;) {
		if (i + k <= s->len && memcmp(s->at + i, tag, k) == 0) {
			if (!append(&out, amount, strlen(amount)))
				return LADX_TEXT_OVERFLOW;
			i += k;
		} else if (!append(&out, s->at + i++, 1)) {
			return LADX_TEXT_OVERFLOW;
		}
	}
	*s = out;
	return LADX_TEXT_OK;
}

/* -?\d+ at *i: 1 with *value and *i past it; 0 for no match; -1 over DIGITS_MAX digits. */
static int number(const Str *s, size_t *i, long long *value)
{
	size_t at = *i, digits = 0;
	int negative = 0;
	long long v = 0;

	if (at < s->len && s->at[at] == '-') {
		negative = 1;
		at++;
	}
	while (at < s->len && s->at[at] >= '0' && s->at[at] <= '9') {
		if (++digits > DIGITS_MAX)
			return -1;
		v = v * 10 + (s->at[at++] - '0');
	}
	if (!digits)
		return 0;
	*value = negative ? -v : v;
	*i = at;
	return 1;
}

/* <kind(?:(-?\d+),)?(-?\d+)> at i: 1 with its bounds and end, 0 for none, -1 unsupported. */
static int placeholder(const Str *s, size_t i, const char *kind, long long *lo, long long *hi, size_t *end)
{
	size_t k = strlen(kind), at = i + k;
	int m;

	if (i + k > s->len || memcmp(s->at + i, kind, k) != 0)
		return 0;
	if ((m = number(s, &at, lo)) <= 0)
		return m;
	if (at < s->len && s->at[at] == ',') {
		at++;
		if ((m = number(s, &at, hi)) < 0)
			return m;
		if (m && at < s->len && s->at[at] == '>') {
			*end = at + 1;
			return 1;
		}
		return 0;
	}
	if (at < s->len && s->at[at] == '>') {
		*hi = *lo;
		*lo = 1;
		*end = at + 1;
		return 1;
	}
	return 0;
}

/* re.sub of the <rnd...> or <ord...> pattern, each match utils.randomNumber or randomOrdinal(min, max + 1). */
static LadxTextStatus substitute(Str *s, const char *kind, int ordinal, LadxRng *global)
{
	char text[32];
	long long lo, hi;
	int64_t value;
	size_t i, end;
	Str out;
	int m;

	out.len = 0;
	for (i = 0; i < s->len;) {
		m = placeholder(s, i, kind, &lo, &hi, &end);
		if (m < 0)
			return LADX_TEXT_UNSUPPORTED;
		if (!m) {
			if (!append(&out, s->at + i++, 1))
				return LADX_TEXT_OVERFLOW;
			continue;
		}
		if (ladx_rng_randrange(global, LADX_STAGE_UTILS_RANDOM_NUMBER, lo, hi + 1, &value) != LADX_RNG_OK)
			return LADX_TEXT_UNSUPPORTED;
		sprintf(text, "%lld", (long long)value);
		if (ordinal) {
			char last = text[strlen(text) - 1];

			strcat(text, last == '1' ? "st" : last == '2' ? "nd" : last == '3' ? "rd" : "th");
		}
		if (!append(&out, text, strlen(text)))
			return LADX_TEXT_OVERFLOW;
		i = end;
	}
	*s = out;
	return LADX_TEXT_OK;
}

LadxTextStatus ladx_marin_line_text(size_t index, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
				    LadxOutcome *raised)
{
	const char *line;
	size_t len;

	if (!(line = ladx_marin_line(index, &len)))
		return LADX_TEXT_UNSUPPORTED;
	return ladx_marin_text(line, len, global, out, cap, n, raised);
}

LadxTextStatus ladx_marin_text(const char *line, size_t len, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
			       LadxOutcome *raised)
{
	Str s;
	LadxTextStatus st;

	if ((st = decode(line, len, &s)) != LADX_TEXT_OK || (st = replace_amount(&s)) != LADX_TEXT_OK ||
	    (st = substitute(&s, "<rnd", 0, global)) != LADX_TEXT_OK ||
	    (st = substitute(&s, "<ord", 1, global)) != LADX_TEXT_OK)
		return st;
	return ladx_format_text(s.at, s.len, 0, NULL, out, cap, n, raised);
}

LadxTextStatus ladx_marin_starting_text(LadxRng *rnd, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
					LadxOutcome *raised)
{
	size_t index;

	if (ladx_rng_choice(rnd, LADX_STAGE_MARIN_STARTING_TEXT, LADX_MARIN_LINE_COUNT, &index) != LADX_RNG_OK)
		return LADX_TEXT_UNSUPPORTED;
	return ladx_marin_line_text(index, global, out, cap, n, raised);
}

static LadxSpan group_name(void)
{
	LadxSpan s;

	s.at = "marin";
	s.len = 5;
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

/* reduceMessageLengths after text 0x01, in its order; a NULL text is a copy of copy_of's current value. */
static const struct {
	uint16_t index;
	const char *text;
	int center;
	const char *ask;
} shortened[] = {
	{0xEA, "You've got a Guardian Acorn!", 0, NULL},
	{0xEB, NULL, 0, NULL},
	{0xEC, NULL, 0, NULL},
	{0x08, "You got a Piece of Power!", 0, NULL},
	{0xEF, "You found a {SEASHELL}!", 0, NULL},
	{0xA7, "You've got the {COMPASS}!", 0, NULL},
	{0x07, "You need the {NIGHTMARE_KEY}!", 0, NULL},
	{0x8C, "You need a {KEY}!", 0, NULL},
	{0x09, "Ahhh... It has  the Sleepy {TOADSTOOL}, it does! We'll mix it up something in a jiffy, we will!", 0, NULL},
	{0x0A, "The last thing I kin remember was bitin' into a big juicy {TOADSTOOL}... Then, I had the darndest dream... I was a raccoon! Yeah, sounds strange, but it sure was fun!", 0, NULL},
	{0x0F, "You pick the {TOADSTOOL}... As you hold it over your head, a mellow aroma flows into your nostrils.", 0, NULL},
	{0x13, "You've learned the ^{SONG1}!^ This song will always remain in your heart!", 0, NULL},
	{0x18, "Will you give me 28 {RUPEES} for my secret?", 0, "Give Don't"},
	{0x19, "How about it? 42 {RUPEES} for my little secret...", 0, "Give Don't"},
	{0x1E, "...You're so cute! I'll give you a 7 {RUPEE} discount!", 0, NULL},
	{0x2D, "{ARROWS_10}\n10 {RUPEES}!", 0, "Buy  Don't"},
	{0x32, "{SHIELD}\n20 {RUPEES}!", 0, "Buy  Don't"},
	{0x33, "Ten {BOMB}\n10 {RUPEES}", 0, "Buy  Don't"},
	{0x3D, "It's a {SHIELD}! There is space for your name!", 0, NULL},
	{0x42, "It's 30 {RUPEES}! You can play the game three more times with this!", 0, NULL},
	{0x44, "You got a {TRADING_ITEM_YOSHI_DOLL}! Recently, he seems to be showing up in many games!", 0, NULL},
	{0x45, "How about some fishing, little buddy? I'll only charge you 10 {RUPEES}...", 0, "Fish Not Now"},
	{0x4B, "Wow! Nice Fish! It's a lunker!! I'll give you a 20 {RUPEE} prize! Try again?", 0, "Cast Not Now"},
	{0x4E, "You're short of {RUPEES}? Don't worry about it. You just come back when you have more money, little buddy.", 0, NULL},
	{0x4F, "You've got a {HEART_PIECE}! Press SELECT on the Subscreen to see.", 0, NULL},
	{0x8E, "Well, it's an {OCARINA}, but you don't know how  to play it...", 0, NULL},
	{0x90, "You found the {POWER_BRACELET}! At last, you can pick up pots and stones!", 0, NULL},
	{0x91, "You got your {SHIELD} back! Press the button and repel enemies with it!", 0, NULL},
	{0x93, "You've got the {HOOKSHOT}! Its chain stretches long when you use it!", 0, NULL},
	{0x94, "You've got the {MAGIC_ROD}! Now you can burn things! Burn it! Burn, baby burn!", 0, NULL},
	{0x95, "You've got the {PEGASUS_BOOTS}! If you hold down the Button, you can dash!", 0, NULL},
	{0x96, "You've got the {OCARINA}! You should learn to play many songs!", 0, NULL},
	{0x97, "You've got the {FEATHER}! It feels like your body is a  lot lighter!", 0, NULL},
	{0x98, "You've got a {SHOVEL}! Now you can feel the joy of digging!", 0, NULL},
	{0x99, "You've got some {MAGIC_POWDER}! Try sprinkling it on a variety of things!", 0, NULL},
	{0x9B, "You found your {SWORD}!  It must be yours because it has your name engraved on it!", 0, NULL},
	{0x9C, "You've got the {FLIPPERS}! If you press the B Button while you swim, you can dive underwater!", 0, NULL},
	{0x9D, "You've got the {TRADING_ITEM_MAGNIFYING_GLASS}! This will reveal many things you couldn't see before!", 0, NULL},
	{0x9E, "You've got a new {SWORD}! You should put your name on it right away!", 0, NULL},
	{0x9F, "You've got a new {SWORD}! You should put your name on it right away!", 0, NULL},
	{0xA0, "You found the {MEDICINE}! You should apply this and see what happens!", 0, NULL},
	{0xA1, "You've got the {TAIL_KEY}! Now you can open the Tail Cave gate!", 0, NULL},
	{0xA2, "You've got the {SLIME_KEY}! Now you can open the gate in Ukuku Prairie!", 0, NULL},
	{0xA3, "You've got the {ANGLER_KEY}!", 0, NULL},
	{0xA4, "You've got the {FACE_KEY}!", 0, NULL},
	{0xA5, "You've got the {BIRD_KEY}!", 0, NULL},
	{0xA6, "At last, you got a {MAP}! Press the START Button to look at it!", 0, NULL},
	{0xA8, "You found a {STONE_BEAK}! Let's find the owl statue that belongs to it.", 0, NULL},
	{0xA9, "You've got the {NIGHTMARE_KEY}! Now you can open the door to the Nightmare's Lair!", 0, NULL},
	{0xAA, "You got a {KEY}! You can open a locked door.", 0, NULL},
	{0xAB, "You got 20 {RUPEES}! JOY!", 1, NULL},
	{0xAC, "You got 50 {RUPEES}! Very Nice!", 1, NULL},
	{0xAD, "You got 100 {RUPEES}! You're Happy!", 1, NULL},
	{0xAE, "You got 200 {RUPEES}! You're Ecstatic!", 1, NULL},
	{0xDC, "Ribbit! Ribbit! I'm Mamu, on vocals! But I don't need to tell you that, do I? Everybody knows me! Want to hang out and listen to us jam? For 300 {RUPEES}, we'll let you listen to a previously unreleased cut! What do you do?", 0, "Pay Leave"},
	{0xE8, "You've found a {GOLD_LEAF}! Press START to see how many you've collected!", 0, NULL},
	{0xED, "You've got the Mirror {SHIELD}! You can now turn back the beams you couldn't block before!", 0, NULL},
	{0xEE, "You've got a more Powerful {POWER_BRACELET}! Now you can almost lift a whale!", 0, NULL},
	{0xF0, "Want to go on a raft ride for a hundred {RUPEES}?", 0, "Yes No Way"},
};

#define COPY_OF 0xEAu

static int write_text(LadxEncoder *enc, LadxPipeline *p, uint32_t index, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		p->origins[i] = LADX_ENC_KNOWN;
	return ladx_enc_table(enc, group_name(), TEXTS, index_key(index), p->values, p->origins, n) == LADX_ENC_OK;
}

/* A raised text ends the group with LADXR's outcome; any other failure is C's. */
static int formatted(LadxTextStatus st, LadxPipeline *p, const LadxOutcome *raised)
{
	if (st == LADX_TEXT_RAISED) {
		p->raised = *raised;
		p->raised.message = p->raised.text;
	}
	return st == LADX_TEXT_OK;
}

int ladx_group_marin(LadxEncoder *enc, LadxPipeline *p)
{
	LadxOutcome raised;
	size_t i, n;

	if (!p->rom || !p->rnd || !p->global)
		return 0;
	if (!formatted(ladx_marin_starting_text(p->rnd, p->global, p->values, LADX_WLOG_ATOM_MAX, &n, &raised), p,
		       &raised) ||
	    !write_text(enc, p, 0x01, n))
		return 0;
	for (i = 0; i < sizeof shortened / sizeof shortened[0]; i++) {
		if (!shortened[i].text) {
			if (!ladx_enc_entry_value(enc, TEXTS, index_key(COPY_OF), p->values, p->origins,
						  LADX_WLOG_ATOM_MAX, &n) ||
			    ladx_enc_table(enc, group_name(), TEXTS, index_key(shortened[i].index), p->values, p->origins,
					   n) != LADX_ENC_OK)
				return 0;
			continue;
		}
		if (!formatted(ladx_format_text(shortened[i].text, strlen(shortened[i].text), shortened[i].center,
						shortened[i].ask, p->values, LADX_WLOG_ATOM_MAX, &n, &raised),
			       p, &raised) ||
		    !write_text(enc, p, shortened[i].index, n))
			return 0;
	}
	return 1;
}

int ladx_group_marin_draws(LadxEncoder *enc, LadxPipeline *p)
{
	LadxOutcome raised;
	size_t n;

	(void)enc;
	if (!p->rnd || !p->global)
		return 0;
	return formatted(ladx_marin_starting_text(p->rnd, p->global, p->values, LADX_WLOG_ATOM_MAX, &n, &raised), p,
			 &raised);
}
