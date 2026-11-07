#include "ladx_groups.h"
#include "ladx_items.h"

#include <string.h>

#define TEXTS 0

/* hints.hint_text_ids. */
static const unsigned short text_ids[LADX_HINT_TEXT_COUNT] = {
	0x1B6, 0x1B7, 0x1B9, 0x1BA, 0x1BB, 0x1BC, 0x1BD, 0x1BE, 0x22D, 0x288, 0x280, 0x28A, 0x289,
	0x281, 0x282, 0x28C, 0x28B, 0x283, 0x28D, 0x284, 0x285, 0x28F, 0x28E, 0x291, 0x290, 0x286,
	0x293, 0x287, 0x292, 0x263, 0x267, 0x201, 0x203, 0x205, 0x207, 0x209, 0x20B,
};

static const int hint_items[] = {
	LADX_ITEM_POWER_BRACELET, LADX_ITEM_SHIELD,    LADX_ITEM_BOW,	     LADX_ITEM_HOOKSHOT,  LADX_ITEM_MAGIC_ROD,
	LADX_ITEM_PEGASUS_BOOTS,  LADX_ITEM_OCARINA,   LADX_ITEM_FEATHER,    LADX_ITEM_SHOVEL,	  LADX_ITEM_MAGIC_POWDER,
	LADX_ITEM_BOMB,		  LADX_ITEM_SWORD,     LADX_ITEM_FLIPPERS,   LADX_ITEM_TAIL_KEY,  LADX_ITEM_ANGLER_KEY,
	LADX_ITEM_FACE_KEY,	  LADX_ITEM_BIRD_KEY,  LADX_ITEM_SLIME_KEY,  LADX_ITEM_GOLD_LEAF, LADX_ITEM_BOOMERANG,
	LADX_ITEM_BOWWOW,	  LADX_ITEM_ROOSTER,   LADX_ITEM_SONG1,	     LADX_ITEM_SONG2,	  LADX_ITEM_SONG3,
};

static const char *const templates[] = {
	"{0} is at {1}",
	"If you want {0} start looking in {1}",
	"{1} holds {0}",
	"They say that {0} is at {1}",
	"You might want to look in {1} for a secret",
};

static const char *const useless[][2] = {
	{"Egg", "Mt. Tamaranch"},   {"Marin", "Mabe Village"},	  {"Marin", "Mabe Village"},
	{"Witch", "Koholint Prairie"}, {"Mermaid", "Martha's Bay"}, {"Nothing", "Tabahl Wasteland"},
	{"Animals", "Animal Village"}, {"Sand", "Yarna Desert"},
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static int is_hint_item(int item)
{
	size_t i;

	for (i = 0; i < COUNT(hint_items); i++)
		if (hint_items[i] == item)
			return 1;
	return 0;
}

/* template.format(a, b) for LADXR's templates, whose only fields are {0} and {1}. */
static int format_template(const char *t, const char *a, const char *b, char *out, size_t *len)
{
	size_t at = 0, m;
	const char *v;

	while (*t) {
		if (t[0] == '{' && (t[1] == '0' || t[1] == '1') && t[2] == '}') {
			v = t[1] == '0' ? a : b;
			t += 3;
		} else {
			if (at >= LADX_TEXT_MAX)
				return 0;
			out[at++] = *t++;
			continue;
		}
		m = strlen(v);
		if (at + m > LADX_TEXT_MAX)
			return 0;
		memcpy(out + at, v, m);
		at += m;
	}
	*len = at;
	return 1;
}

static LadxHintsStatus put_text(unsigned id, const char *s, size_t n, const char *ask, LadxHintWrite write,
				void *ctx, LadxOutcome *raised)
{
	uint8_t out[LADX_TEXT_MAX];
	size_t len;

	switch (ladx_format_text(s, n, 0, ask, out, sizeof out, &len, raised)) {
	case LADX_TEXT_OK:
		return write(ctx, id, out, len) ? LADX_HINTS_OK : LADX_HINTS_FAILED;
	case LADX_TEXT_RAISED:
		return LADX_HINTS_RAISED;
	default:
		return LADX_HINTS_FAILED;
	}
}

LadxHintsStatus ladx_hints_add(LadxRng *rnd, const LadxHintSpot *spots, size_t count, LadxHintWrite write,
			       void *ctx, LadxOutcome *raised)
{
	static const char book[] = "Read this book?";
	int sorted[LADX_LOGIC_SPOT_CAP];
	unsigned short ids[LADX_HINT_TEXT_COUNT];
	char item[LADX_TEXT_MAX], text[LADX_TEXT_MAX];
	size_t n = 0, i, j, t, u, len;
	const char *name;
	int64_t pick;
	LadxHintsStatus st;

	memset(raised, 0, sizeof *raised);
	if (count > LADX_LOGIC_SPOT_CAP)
		return LADX_HINTS_FAILED;
	/* sorted(filter(...), key=nameId): stable, so ties keep iteminfo_list order. */
	for (i = 0; i < count; i++) {
		if (!is_hint_item(spots[i].item))
			continue;
		for (j = n; j > 0 && strcmp(spots[sorted[j - 1]].name_id, spots[i].name_id) > 0; j--)
			sorted[j] = sorted[j - 1];
		sorted[j] = (int)i;
		n++;
	}
	memcpy(ids, text_ids, sizeof ids);
	if (ladx_rng_shuffle(rnd, LADX_STAGE_HINTS_ADD_HINTS, ids, LADX_HINT_TEXT_COUNT, sizeof ids[0]) != LADX_RNG_OK)
		return LADX_HINTS_FAILED;
	for (i = 0; i < LADX_HINT_TEXT_COUNT; i++) {
		const char *a, *b;

		if (n > 0) {
			if (ladx_rng_randint(rnd, LADX_STAGE_HINTS_ADD_HINTS, 0, (int64_t)n - 1, &pick) != LADX_RNG_OK)
				return LADX_HINTS_FAILED;
			j = (size_t)sorted[pick];
			memmove(sorted + pick, sorted + pick + 1, (n - 1 - (size_t)pick) * sizeof sorted[0]);
			n--;
			if (ladx_rng_choice(rnd, LADX_STAGE_HINTS_ADD_HINTS, COUNT(templates), &t) != LADX_RNG_OK ||
			    !(name = ladx_item_name(spots[j].item)) || strlen(name) + 3 > sizeof item)
				return LADX_HINTS_FAILED;
			item[0] = '{';
			strcpy(item + 1, name);
			strcat(item, "}");
			a = item;
			b = spots[j].area;
		} else {
			/* format's receiver, rnd.choice(hints), is evaluated before its arguments. */
			if (ladx_rng_choice(rnd, LADX_STAGE_HINTS_ADD_HINTS, COUNT(templates), &t) != LADX_RNG_OK ||
			    ladx_rng_choice(rnd, LADX_STAGE_HINTS_ADD_HINTS, COUNT(useless), &u) != LADX_RNG_OK)
				return LADX_HINTS_FAILED;
			a = useless[u][0];
			b = useless[u][1];
		}
		if (!format_template(templates[t], a, b, text, &len))
			return LADX_HINTS_FAILED;
		if ((st = put_text(ids[i], text, len, NULL, write, ctx, raised)) != LADX_HINTS_OK)
			return st;
	}
	for (i = 0x200; i < 0x20C; i += 2)
		if ((st = put_text((unsigned)i, book, sizeof book - 1, "YES  NO", write, ctx, raised)) != LADX_HINTS_OK)
			return st;
	return LADX_HINTS_OK;
}

typedef struct GroupWrite {
	LadxEncoder *enc;
	LadxPipeline *p;
} GroupWrite;

static int write_text(void *ctx, unsigned text_id, const uint8_t *bytes, size_t n)
{
	GroupWrite *w = ctx;
	LadxWlogKey key;
	LadxSpan group;
	size_t i;

	if (n > LADX_WLOG_ATOM_MAX)
		return 0;
	for (i = 0; i < n; i++)
		w->p->origins[i] = LADX_ENC_KNOWN;
	key.index = text_id;
	key.alt.at = NULL;
	key.alt.len = 0;
	group.at = "hints";
	group.len = 5;
	return ladx_enc_table(w->enc, group, TEXTS, key, bytes, w->p->origins, n) == LADX_ENC_OK;
}

int ladx_group_hints(LadxEncoder *enc, LadxPipeline *p)
{
	LadxHintSpot spots[LADX_LOGIC_SPOT_CAP];
	const LadxLogic *logic;
	const LadxSpot *spot;
	LadxOutcome raised;
	GroupWrite w;
	int i, count;

	if (!p->rnd || !p->randomizer || !(logic = ladx_randomizer_placement(p->randomizer)))
		return 0;
	count = ladx_logic_spot_count(logic);
	for (i = 0; i < count; i++) {
		spot = ladx_logic_spot(logic, i);
		spots[i].name_id = spot->name_id;
		spots[i].item = spot->item;
		spots[i].area = ladx_spot_metadata_area(spot);
	}
	w.enc = enc;
	w.p = p;
	switch (ladx_hints_add(p->rnd, spots, (size_t)count, write_text, &w, &raised)) {
	case LADX_HINTS_OK:
		return 1;
	case LADX_HINTS_RAISED:
		p->raised = raised;
		p->raised.message = p->raised.text;
		return 0;
	default:
		return 0;
	}
}
