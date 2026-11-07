#include "ladx_groups.h"
#include "ladx_sha1.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_entrances"
#define OUT_MAX 262144
#define ENTITIES 1
#define TOP 2
#define BOTTOM 3
#define INDOOR_A 4
#define INDOOR_B 5

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

/* SHA-1 of entranceInfo.py's ENTRANCE_INFO at 4325207e, one "<key> <room> <alt> <index> <target> <instrument>\n" line each (hex, "none", "all"). */
#define INFO_SHA1 "1054093465845e26f3e6ae09f42bdec052931651"

static SynthRom synth;
static LadxRomTables rom_tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxRoom room;
static LadxOutcome raised;
static char out[OUT_MAX];
static size_t out_len;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void sink(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	if (out_len + len >= OUT_MAX)
		test_fail(TEST, "log text over %d bytes", OUT_MAX);
	memcpy(out + out_len, text, len);
	out_len += len;
	out[out_len] = '\0';
}

static LadxSpan span(const char *s)
{
	LadxSpan sp;

	sp.at = s;
	sp.len = strlen(s);
	return sp;
}

/* No KS: planted entries are not matched against the synthetic ROM. */
static void begin(void)
{
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
}

static void end(void)
{
	if (ladx_enc_end(&enc) != LADX_ENC_OK)
		test_fail(TEST, "the encoder ends");
}

static void plant_key(int table, LadxWlogKey key, const uint8_t *bytes, size_t n)
{
	int32_t known[64];
	size_t i;

	for (i = 0; i < n; i++)
		known[i] = LADX_ENC_KNOWN;
	if (ladx_enc_table(&enc, span("plant"), table, key, bytes, known, n) != LADX_ENC_OK)
		test_fail(TEST, "planting an entry");
}

/* Room nr's objects (and empty entities), header 0x01 0x02. */
static void plant_room(int nr, const char *alt, const uint8_t *objects, size_t n)
{
	static const uint8_t no_entities[] = {0xFF};
	uint8_t raw[64];
	LadxWlogKey key;
	int table;

	raw[0] = 0x01;
	raw[1] = 0x02;
	memcpy(raw + 2, objects, n);
	raw[n + 2] = 0xFE;
	key.index = 0;
	key.alt.at = alt;
	key.alt.len = alt ? strlen(alt) : 0;
	if (alt) {
		table = strcmp(alt, "Alt1F5") == 0 ? INDOOR_A : TOP;
	} else {
		key.index = (uint32_t)(nr & 0xFF) - (nr < 0x100 && nr >= 0x80 ? 0x80u : 0u);
		table = nr < 0x80 ? TOP : nr < 0x100 ? BOTTOM : nr < 0x200 ? INDOOR_A : INDOOR_B;
	}
	plant_key(table, key, raw, n + 3);
	if (!alt) {
		key.index = (uint32_t)nr;
		plant_key(ENTITIES, key, no_entities, 1);
	}
}

#define WARP_IN(lo, x, y) 0xE1, 0x10, (lo), (x), (y) /* indoor B: room 0x200 + lo */
#define WARP_OUT(nr, x, y) 0xE0, 0x00, (nr), (x), (y)

/*
 * a: 0x10 -> 0x210; b: 0x0E (alt Alt0E, index 1) -> 0x21E, instrument 0x230;
 * c: 0xF4 (index "all") -> 0x2F5, instrument Alt1F5.
 */
static const LadxEntranceInfo planted_info[3] = {
	{"a", 0x10, NULL, LADX_ENTRANCE_INDEX_NONE, 0x210, -1, NULL},
	{"b", 0x0E, "Alt0E", 1, 0x21E, 0x230, NULL},
	{"c", 0xF4, NULL, LADX_ENTRANCE_INDEX_ALL, 0x2F5, -1, "Alt1F5"},
};

static void plant_world(void)
{
	static const uint8_t r10[] = {0x33, 0x44, WARP_IN(0x10, 0x50, 0x60)};
	static const uint8_t r0e[] = {WARP_IN(0x1E, 0x11, 0x12)};
	static const uint8_t alt0e[] = {WARP_IN(0x99, 0x01, 0x02), WARP_IN(0x1E, 0x40, 0x30)};
	static const uint8_t rf4[] = {WARP_IN(0xF5, 0x70, 0x71)};
	static const uint8_t r210[] = {WARP_OUT(0x10, 0x58, 0x20)};
	static const uint8_t r21e[] = {WARP_OUT(0x0E, 0x01, 0x01), 0x45, 0x67, WARP_OUT(0x0E, 0x02, 0x02)};
	static const uint8_t r230[] = {WARP_OUT(0x0E, 0x09, 0x09)};
	static const uint8_t r2f5[] = {WARP_OUT(0xF4, 0x22, 0x23)};
	static const uint8_t alt1f5[] = {WARP_OUT(0xF4, 0x05, 0x06)};

	begin();
	plant_room(0x10, NULL, r10, sizeof r10);
	plant_room(0x0E, NULL, r0e, sizeof r0e);
	plant_room(0, "Alt0E", alt0e, sizeof alt0e);
	plant_room(0xF4, NULL, rf4, sizeof rf4);
	plant_room(0x210, NULL, r210, sizeof r210);
	plant_room(0x21E, NULL, r21e, sizeof r21e);
	plant_room(0x230, NULL, r230, sizeof r230);
	plant_room(0x2F5, NULL, r2f5, sizeof r2f5);
	plant_room(0, "Alt1F5", alt1f5, sizeof alt1f5);
}

static void identity(int *mapping, size_t count)
{
	size_t k;

	for (k = 0; k < count; k++) {
		mapping[k] = (int)(count + k);
		mapping[count + k] = (int)k;
	}
}

static LadxEntrancesStatus change(const LadxEntranceInfo *info, size_t count, const int *mapping)
{
	memset(&raised, 0, sizeof raised);
	return ladx_entrances_change(&enc, span("entrances"), &room, info, count, mapping, &raised);
}

/* The i-th warp of a room as it stands now. */
static const LadxRoomObject *warp(int nr, const char *alt, size_t i)
{
	size_t k;

	if (ladx_room_load(&room, &enc, nr, alt) != LADX_ROOM_OK)
		test_fail(TEST, "room %x %s reloads", nr, alt ? alt : "");
	for (k = 0; k < room.object_count; k++)
		if (room.objects[k].kind == LADX_ROOM_WARP && i-- == 0)
			return &room.objects[k];
	test_fail(TEST, "room %x %s lost a warp", nr, alt ? alt : "");
	return NULL;
}

static int warp_is(const LadxRoomObject *w, int target, int x, int y)
{
	return w->room == target && w->target_x == x && w->target_y == y;
}

static void test_table(void)
{
	static char text[8192];
	const LadxEntranceInfo *info;
	uint8_t digest[20];
	char hex[41];
	size_t count, k, len = 0;

	info = ladx_entrance_info(&count);
	check(count == LADX_ENTRANCE_INFO_COUNT, "104 entrances");
	for (k = 0; k < count; k++) {
		char alt[16], index[16], instrument[16];

		snprintf(alt, sizeof alt, "%s", info[k].alt_room ? info[k].alt_room : "none");
		if (info[k].index == LADX_ENTRANCE_INDEX_NONE)
			strcpy(index, "none");
		else if (info[k].index == LADX_ENTRANCE_INDEX_ALL)
			strcpy(index, "all");
		else
			snprintf(index, sizeof index, "%x", info[k].index);
		if (info[k].instrument_alt)
			snprintf(instrument, sizeof instrument, "%s", info[k].instrument_alt);
		else if (info[k].instrument_room >= 0)
			snprintf(instrument, sizeof instrument, "%x", info[k].instrument_room);
		else
			strcpy(instrument, "none");
		len += (size_t)snprintf(text + len, sizeof text - len, "%s %x %s %s %x %s\n", info[k].name, info[k].room,
					alt, index, info[k].target, instrument);
	}
	ladx_sha1((const uint8_t *)text, len, digest);
	for (k = 0; k < 20; k++)
		snprintf(hex + 2 * k, 3, "%02x", digest[k]);
	if (strcmp(hex, INFO_SHA1) != 0)
		test_fail(TEST, "ENTRANCE_INFO digest %s, LADXR's %s", hex, INFO_SHA1);
	checks++;
}

/* Identity: every room re-stored, a second warp to the same room rewritten, the last way out taken. */
static void test_identity(void)
{
	int mapping[6];
	const char *seg;

	plant_world();
	identity(mapping, 3);
	check(change(planted_info, 3, mapping) == LADX_ENTRANCES_OK, "identity runs");
	check(warp_is(warp(0x10, NULL, 0), 0x210, 0x50, 0x60), "a's warp kept");
	check(warp_is(warp(0x0E, NULL, 0), 0x21E, 0x40, 0x30), "b's room takes its alt room's index-1 warp");
	check(warp_is(warp(0, "Alt0E", 0), 0x299, 0x01, 0x02), "a warp to another room kept");
	check(warp_is(warp(0, "Alt0E", 1), 0x21E, 0x40, 0x30), "the alt room's index warp kept");
	check(warp_is(warp(0x21E, NULL, 0), 0x0E, 0x02, 0x02) && warp_is(warp(0x21E, NULL, 1), 0x0E, 0x02, 0x02),
	      "both ways out made the last one");
	check(warp_is(warp(0x230, NULL, 0), 0x0E, 0x02, 0x02), "the instrument room's way out too");
	check(warp_is(warp(0, "Alt1F5", 0), 0xF4, 0x22, 0x23), "the alt instrument room takes c's way out");
	check(warp_is(warp(0xF4, NULL, 0), 0x2F5, 0x70, 0x71), "index all is warp 0");
	end();
	check((seg = strstr(out, "G entrances\n")) != NULL, "a segment of the group");
	check(strstr(seg, "T entities 10 ") && strstr(seg, "T entities e ") && strstr(seg, "T entities f4 ") &&
		      strstr(seg, "T entities 210 ") && strstr(seg, "T entities 21e ") &&
		      strstr(seg, "T entities 230 ") && strstr(seg, "T entities 2f5 "),
	      "index rooms' entities stored");
	check(strstr(seg, "T rooms_overworld_top Alt0E ") && strstr(seg, "T rooms_indoor_a Alt1F5 ") &&
		      strstr(seg, "T rooms_overworld_top 10 ") && strstr(seg, "T rooms_overworld_top e ") &&
		      strstr(seg, "T rooms_overworld_bottom 74 ") && strstr(seg, "T rooms_indoor_b 10 ") &&
		      strstr(seg, "T rooms_indoor_b 1e ") && strstr(seg, "T rooms_indoor_a 30 ") == NULL &&
		      strstr(seg, "T rooms_indoor_b 30 ") && strstr(seg, "T rooms_indoor_b f5 "),
	      "every alt and index room stored, none other");
	check(strstr(seg, "B ") != NULL, "the overlays stored");
}

/* Two doors of one room into one house: the later key's change to that room wins, for both doors. */
static void test_override(void)
{
	static const LadxEntranceInfo info[2] = {
		{"left", 0x20, NULL, 0, 0x220, -1, NULL},
		{"right", 0x20, NULL, 1, 0x220, -1, NULL},
	};
	static const uint8_t r20[] = {WARP_IN(0x20, 0x10, 0x11), WARP_IN(0x20, 0x30, 0x31)};
	static const uint8_t r220[] = {WARP_OUT(0x20, 0x44, 0x45)};
	int mapping[4];

	begin();
	plant_room(0x20, NULL, r20, sizeof r20);
	plant_room(0x220, NULL, r220, sizeof r220);
	identity(mapping, 2);
	check(change(info, 2, mapping) == LADX_ENTRANCES_OK, "two doors run");
	check(warp_is(warp(0x20, NULL, 0), 0x220, 0x30, 0x31) && warp_is(warp(0x20, NULL, 1), 0x220, 0x30, 0x31),
	      "both doors made the later key's warp");
}

/* A swap of a and c, as WorldSetup._swap leaves the mapping. */
static void test_swap(void)
{
	int mapping[6];

	plant_world();
	identity(mapping, 3);
	mapping[0] = 3 + 2;
	mapping[2] = 3 + 0;
	mapping[3 + 0] = 2;
	mapping[3 + 2] = 0;
	check(change(planted_info, 3, mapping) == LADX_ENTRANCES_OK, "a swap runs");
	check(warp_is(warp(0x10, NULL, 0), 0x2F5, 0x70, 0x71), "a's door leads into c");
	check(warp_is(warp(0xF4, NULL, 0), 0x210, 0x50, 0x60), "c's door leads into a");
	check(warp_is(warp(0x210, NULL, 0), 0xF4, 0x22, 0x23), "a's inside leads out of c");
	check(warp_is(warp(0x2F5, NULL, 0), 0x10, 0x58, 0x20), "c's inside leads out of a");
	check(warp_is(warp(0, "Alt1F5", 0), 0x10, 0x58, 0x20), "c's instrument room too");
	check(warp(0x10, NULL, 0)->warp_type == 1 && warp(0x10, NULL, 0)->map_nr == 0x10, "the copy's type and map");
	end();
}

static void test_raises(void)
{
	static const uint8_t bad10[] = {0xE1, 0x10};
	static const uint8_t rf3[] = {WARP_IN(0xF5, 0x70, 0x71)};
	LadxEntranceInfo info[3];
	int mapping[6];

	identity(mapping, 3);
	memcpy(info, planted_info, sizeof info);
	info[0].target = 0x211;
	plant_world();
	check(change(info, 3, mapping) == LADX_ENTRANCES_RAISED && raised.kind == LADX_OUTCOME_CRASH &&
		      strcmp(raised.type, "AssertionError") == 0 && strcmp(raised.message, "a != 210") == 0,
	      "a target mismatch raises LADXR's AssertionError");
	memcpy(info, planted_info, sizeof info);
	info[1].index = 2;
	plant_world();
	check(change(info, 3, mapping) == LADX_ENTRANCES_RAISED && strcmp(raised.type, "IndexError") == 0 &&
		      strcmp(raised.message, "list index out of range") == 0,
	      "a missing warp index raises IndexError");
	memcpy(info, planted_info, sizeof info);
	info[2].room = 0xF3;
	plant_world();
	plant_room(0xF3, NULL, rf3, sizeof rf3);
	check(change(info, 3, mapping) == LADX_ENTRANCES_RAISED && strcmp(raised.type, "AssertionError") == 0 &&
		      strcmp(raised.message, "Missing warp to outdoor on c") == 0,
	      "no way out raises LADXR's AssertionError");
	plant_world();
	plant_room(0x10, NULL, bad10, sizeof bad10);
	check(change(planted_info, 3, mapping) == LADX_ENTRANCES_FAILED, "a malformed room fails");
	plant_world();
	mapping[1] = 6;
	check(change(planted_info, 3, mapping) == LADX_ENTRANCES_FAILED, "a target key out of range fails");
}

static void test_registry(void)
{
	const LadxPipelineGroup *groups;
	int count, g, items = -1, entrances = -1;

	groups = ladx_pipeline_groups(&count);
	for (g = 0; g < count; g++) {
		if (strcmp(groups[g].name, "items") == 0)
			items = g;
		if (strcmp(groups[g].name, "entrances") == 0)
			entrances = g;
	}
	check(entrances == items + 1 && items >= 0, "entrances registered right after items, LADXR's order");
	check(groups[entrances].needs_rom && groups[entrances].draw == NULL, "a rom-group with no draw");
}

int main(void)
{
	synth_rom_build(&synth, 0x7E470001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM parses");
	test_table();
	test_identity();
	test_override();
	test_swap();
	test_raises();
	test_registry();
	printf("%s: changeEntrances on planted rooms of a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
