#include "rom_credits_data.h"
#include "rom_emit.h"

#include <assert.h>
#include <string.h>

#define TEXT_MAX 32
#define SCENE_RECORDS_MAX 3
#define POINTERS (SCENE_COUNT + 1)
/* Per scene at most 108 bytes of small, 36 of small_alt and 72 of large records. */
#define DATA_MAX (SCENE_COUNT * (108 + 36 + 72))

#define DATA_ADDRESS 0x181500u
#define POINTERS_ADDRESS 0x076CC0u

typedef enum Scene {
	SCENE_CASTLE,
	SCENE_SANCTUARY,
	SCENE_KAKARIKO,
	SCENE_DESERT,
	SCENE_HERA,
	SCENE_HOUSE,
	SCENE_ZORA,
	SCENE_WITCH,
	SCENE_LUMBERJACKS,
	SCENE_GROVE,
	SCENE_WELL,
	SCENE_SMITHY,
	SCENE_KAKARIKO2,
	SCENE_BRIDGE,
	SCENE_WOODS,
	SCENE_PEDESTAL,
	SCENE_COUNT
} Scene;

typedef enum RecordType { RECORD_NONE, RECORD_SMALL, RECORD_SMALL_ALT, RECORD_LARGE } RecordType;

typedef struct Record {
	RecordType type;
	unsigned x;
	unsigned y;
	const char *text;
} Record;

/* Credits::$scenes at 219fcafd; RECORD_NONE ends a scene of fewer than three records. */
static const Record scenes[SCENE_COUNT][SCENE_RECORDS_MAX] = {
	{{RECORD_SMALL, 5, 19, "The return of the King"}, {RECORD_LARGE, 9, 23, "Hyrule Castle"}},
	{{RECORD_SMALL, 8, 19, "The loyal priest"}, {RECORD_LARGE, 11, 23, "Sanctuary"}},
	{{RECORD_SMALL, 4, 19, "Sahasralah's Homecoming"}, {RECORD_LARGE, 9, 23, "Kakariko Town"}},
	{{RECORD_SMALL, 4, 19, "vultures rule the desert"}, {RECORD_LARGE, 9, 23, "Desert Palace"}},
	{{RECORD_SMALL, 4, 19, "the bully makes a friend"}, {RECORD_LARGE, 9, 23, "Mountain Tower"}},
	{{RECORD_SMALL, 6, 19, "your uncle recovers"}, {RECORD_LARGE, 11, 23, "Your House"}},
	{{RECORD_SMALL, 6, 19, "finger webs for sale"}, {RECORD_LARGE, 8, 23, "Zora's Waterfall"}},
	{{RECORD_SMALL, 4, 19, "the witch and assistant"}, {RECORD_LARGE, 11, 23, "Magic Shop"}},
	{{RECORD_SMALL, 8, 19, "twin lumberjacks"}, {RECORD_LARGE, 9, 23, "Woodsmen's Hut"}},
	{{RECORD_SMALL, 4, 19, "ocarina boy plays again"}, {RECORD_LARGE, 9, 23, "Haunted Grove"}},
	{{RECORD_SMALL, 4, 19, "venus, queen of faeries"}, {RECORD_LARGE, 10, 23, "Wishing Well"}},
	{{RECORD_SMALL, 4, 19, "the dwarven swordsmiths"}, {RECORD_LARGE, 12, 23, "Smithery"}},
	{{RECORD_SMALL, 6, 19, "the bug-catching kid"}, {RECORD_LARGE, 9, 23, "Kakariko Town"}},
	{{RECORD_SMALL, 8, 19, "the lost old man"}, {RECORD_LARGE, 9, 23, "Death Mountain"}},
	{{RECORD_SMALL, 8, 19, "the forest thief"}, {RECORD_LARGE, 11, 23, "Lost Woods"}},
	{{RECORD_SMALL, 6, 19, "and the master sword"},
	 {RECORD_SMALL_ALT, 8, 21, "sleeps again..."},
	 {RECORD_LARGE, 12, 23, "Forever!"}},
};

/* Each scene's line 0 as setCredit left it: VT's default record, or a text and its centred x. */
typedef struct Line {
	char text[TEXT_MAX + 1];
	unsigned x;
} Line;

typedef struct Buffer {
	uint8_t bytes[DATA_MAX];
	size_t n;
} Buffer;

static void set_line(Line *line, const char *text)
{
	size_t len = strlen(text);

	if (len > TEXT_MAX)
		len = TEXT_MAX;
	memcpy(line->text, text, len);
	line->text[len] = '\0';
	line->x = len >= TEXT_MAX ? 0 : (unsigned)(TEXT_MAX - len) / 2;
}

static void set_default(Line *line, Scene scene)
{
	set_line(line, scenes[scene][0].text);
	line->x = scenes[scene][0].x;
}

static void push(Buffer *buf, uint8_t byte)
{
	assert(buf->n < DATA_MAX);
	buf->bytes[buf->n++] = byte;
}

static void push_header(Buffer *buf, unsigned x, unsigned y, size_t len)
{
	uint32_t v;

	assert(len > 0);
	v = (uint32_t)(0x6000u | (y >> 5 << 11) | ((y & 0x1Fu) << 5) | (x >> 5 << 10) | (x & 0x1Fu)) << 16 |
	    (uint32_t)(len * 2 - 1);
	push(buf, (uint8_t)(v >> 24));
	push(buf, (uint8_t)(v >> 16));
	push(buf, (uint8_t)(v >> 8));
	push(buf, (uint8_t)v);
}

static char lower(char c)
{
	return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static bool is_letter(char c)
{
	return c >= 'a' && c <= 'z';
}

/* Credits::charToCreditsHex, on a lower-cased byte. */
static uint8_t small_byte(char c)
{
	if (is_letter(c))
		return (uint8_t)(c - 0x47);
	switch (c) {
	case ',':
		return 0x34;
	case '.':
		return 0x37;
	case '-':
		return 0x36;
	case '\'':
		return 0x35;
	default:
		return 0x9F;
	}
}

static uint8_t small_alt_byte(char c)
{
	if (is_letter(c))
		return (uint8_t)(c - 0x29);
	return c == '.' ? 0x52 : 0x9F;
}

static uint8_t large_top_byte(char c)
{
	if (is_letter(c))
		return (uint8_t)(c - 0x4);
	switch (c) {
	case '\'':
		return 0xD9;
	case '!':
		return 0xE5;
	case '_':
		return 0xDE;
	default:
		return 0x9F;
	}
}

static uint8_t large_bottom_byte(char c)
{
	if (is_letter(c))
		return (uint8_t)(c + 0x22);
	switch (c) {
	case '\'':
		return 0xEC;
	case '!':
		return 0xF8;
	case '_':
		return 0xF1;
	default:
		return 0x9F;
	}
}

static void push_converted(Buffer *buf, unsigned x, unsigned y, const char *text, size_t len,
			   uint8_t (*convert)(char))
{
	size_t i;

	push_header(buf, x, y, len);
	for (i = 0; i < len; i++)
		push(buf, convert(lower(text[i])));
}

/* getSmallConverted's apostrophe and comma records: x is offset by strpos on the untrimmed mask. */
static void push_mark(Buffer *buf, const Line *line, unsigned y, char mark, char shown)
{
	char mask[TEXT_MAX + 1];
	size_t len = strlen(line->text), i, first = len, last = 0;

	for (i = 0; i < len; i++) {
		mask[i] = line->text[i] == mark ? shown : ' ';
		if (line->text[i] == mark) {
			if (first == len)
				first = i;
			last = i;
		}
	}
	if (first == len)
		return;
	push_converted(buf, line->x + (unsigned)first, y, mask + first, last - first + 1, small_byte);
}

static void push_small(Buffer *buf, const Line *line, unsigned y)
{
	push_converted(buf, line->x, y, line->text, strlen(line->text), small_byte);
	push_mark(buf, line, y - 1, '\'', ',');
	push_mark(buf, line, y + 1, ',', '\'');
}

static void push_record(Buffer *buf, const Record *rec, const Line *line)
{
	size_t len = rec->type == RECORD_NONE ? 0 : strlen(rec->text);

	switch (rec->type) {
	case RECORD_SMALL:
		push_small(buf, line, rec->y);
		break;
	case RECORD_SMALL_ALT:
		push_converted(buf, rec->x, rec->y, rec->text, len, small_alt_byte);
		break;
	case RECORD_LARGE:
		push_converted(buf, rec->x, rec->y, rec->text, len, large_top_byte);
		push_converted(buf, rec->x, rec->y + 1, rec->text, len, large_bottom_byte);
		break;
	case RECORD_NONE:
		break;
	}
}

static const char *pick(const char *const *alternatives, unsigned count, unsigned index)
{
	assert(index < count);
	(void)count;
	return alternatives[index];
}

/* randomizeCredits' setCredit calls; World::writeToRom replays them before the item loop. */
static void set_picks(Line lines[SCENE_COUNT], const RandoCredits *c)
{
	char homecoming[TEXT_MAX + 1 + sizeof "'s homecoming"];
	const char *name = pick(rando_credits_kakariko, RANDO_CREDITS_KAKARIKO, c->kakariko);

	set_line(&lines[SCENE_CASTLE], pick(rando_credits_castle, RANDO_CREDITS_CASTLE, c->castle));
	set_line(&lines[SCENE_SANCTUARY], pick(rando_credits_sanctuary, RANDO_CREDITS_SANCTUARY, c->sanctuary));
	assert(strlen(name) <= TEXT_MAX);
	strcpy(homecoming, name);
	strcat(homecoming, "'s homecoming");
	set_line(&lines[SCENE_KAKARIKO], homecoming);
	set_line(&lines[SCENE_LUMBERJACKS],
		 pick(rando_credits_lumberjacks, RANDO_CREDITS_LUMBERJACKS, c->lumberjacks));
	if (c->smithy_breadsmiths)
		set_line(&lines[SCENE_SMITHY], "the dwarven breadsmiths");
	set_line(&lines[SCENE_BRIDGE], pick(rando_credits_bridge, RANDO_CREDITS_BRIDGE, c->bridge));
	set_line(&lines[SCENE_WOODS], pick(rando_credits_woods, RANDO_CREDITS_WOODS, c->woods));
	set_line(&lines[SCENE_WELL], pick(rando_credits_well, RANDO_CREDITS_WELL, c->well));
}

typedef struct Override {
	Scene scene;
	RandoLocationClass location_class;
	const RandoCreditsOverride *table;
} Override;

/* The six writeItem overrides, in no particular order: their scenes are not the picks'. */
static const Override overrides[] = {
	{SCENE_ZORA, RANDO_LOCATION_CLASS_NPC_ZORA, &rando_credits_zora},
	{SCENE_WITCH, RANDO_LOCATION_CLASS_NPC_WITCH, &rando_credits_witch},
	{SCENE_PEDESTAL, RANDO_LOCATION_CLASS_PEDESTAL, &rando_credits_pedestal},
	{SCENE_KAKARIKO2, RANDO_LOCATION_CLASS_NPC_BUG_CATCHING_KID, &rando_credits_kakariko2},
	{SCENE_GROVE, RANDO_LOCATION_CLASS_DIG_HAUNTED_GROVE, &rando_credits_grove},
	{SCENE_HOUSE, RANDO_LOCATION_CLASS_NPC_UNCLE, &rando_credits_house},
};

/* getItemCreditsText reads the location's own item, never writeItem's substituted key. */
static const char *override_text(const RandoCreditsOverride *table, RandoItemId item)
{
	const RandoItemInfo *info;

	if (item == RANDO_ITEM_NONE)
		item = RANDO_ITEM_Nothing;
	info = rando_item_info(item);
	if (info->item_class == RANDO_ITEM_CLASS_ALIAS) {
		item = info->alias_target;
		info = rando_item_info(item);
	}
	if (item == RANDO_ITEM_BigKeyA2 && table->big_key_a2)
		return table->big_key_a2;
	switch (info->item_class) {
	case RANDO_ITEM_CLASS_KEY:
	case RANDO_ITEM_CLASS_BIG_KEY:
		return table->key;
	case RANDO_ITEM_CLASS_MAP:
		return table->map;
	case RANDO_ITEM_CLASS_COMPASS:
		return table->compass;
	default:
		break;
	}
	return table->items[item] ? table->items[item] : table->fallback;
}

/* World::writeToRom's item loop over the world's locations. */
static void set_overrides(Line lines[SCENE_COUNT], const RandoWorld *world)
{
	int n = rando_layout_location_count(world->variant), i;
	size_t o;

	for (i = 0; i < n; i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);
		RandoLocationClass location_class = rando_location_info(location)->location_class;

		for (o = 0; o < sizeof overrides / sizeof overrides[0]; o++)
			if (overrides[o].location_class == location_class)
				set_line(&lines[overrides[o].scene],
					 override_text(overrides[o].table, rando_world_item(world, location)));
	}
}

void rando_rom_write_credits(RomOut *out, const RandoGen *gen)
{
	Buffer buf;
	Line lines[SCENE_COUNT];
	uint8_t pointers[2 * POINTERS];
	int s, r;

	for (s = 0; s < SCENE_COUNT; s++)
		set_default(&lines[s], (Scene)s);
	set_picks(lines, &gen->post.credits);
	set_overrides(lines, &gen->world);

	buf.n = 0;
	for (s = 0; s < SCENE_COUNT; s++) {
		pointers[2 * s] = (uint8_t)(buf.n & 0xFF);
		pointers[2 * s + 1] = (uint8_t)(buf.n >> 8);
		for (r = 0; r < SCENE_RECORDS_MAX; r++)
			push_record(&buf, &scenes[s][r], &lines[s]);
	}
	pointers[2 * SCENE_COUNT] = (uint8_t)(buf.n & 0xFF);
	pointers[2 * SCENE_COUNT + 1] = (uint8_t)(buf.n >> 8);
	rando_rom_emit(out, DATA_ADDRESS, buf.bytes, buf.n, "writeCredits");
	rando_rom_emit(out, POINTERS_ADDRESS, pointers, sizeof pointers, "writeCredits");
}
