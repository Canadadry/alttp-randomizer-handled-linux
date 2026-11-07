#include "ladx_room.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_room"
#define OUT_MAX 65536
#define ENTITIES 1
#define TOP 2
#define BOTTOM 3
#define INDOOR_A 4
#define INDOOR_B 5
#define COLOR 6

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"
#define HEADER "ladx-rom-writes 1\ntables " TABLES "\n"

static SynthRom synth;
static LadxRomTables rom_tables;
static LadxKs ks;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxRoom room;
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

static int no_ks; /* planted entries: no KS, so no fragment rule on them */

static void begin(int with_rom)
{
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, with_rom ? synth.rom : NULL, with_rom ? SYNTH_ROM_SIZE : 0, with_rom && !no_ks ? &ks : NULL,
			  &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
}

static void end(void)
{
	if (ladx_enc_end(&enc) != LADX_ENC_OK)
		test_fail(TEST, "the encoder ends");
}

static const SynthEntry *entry(int table, uint32_t index, const char *alt)
{
	int i;

	for (i = 0; i < synth.table_count[table]; i++) {
		const SynthEntry *e = &synth.entry[synth.table_first[table] + i];

		if (alt ? (e->alt && strcmp(e->alt, alt) == 0) : (!e->alt && e->index == index))
			return e;
	}
	return NULL;
}

/* An entity entry RoomEditor parses: bytes, its 0xFF on a pair's first byte. */
static int entities_ok(uint32_t room_nr)
{
	const SynthEntry *e = entry(ENTITIES, room_nr, NULL);
	uint32_t i;

	if (!e || e->integer)
		return 0;
	for (i = e->start; i < e->stop && synth.rom[i] != 0xFF; i += 2)
		;
	return i == e->stop - 1;
}

static void objects_of(uint32_t room_nr, int *table, uint32_t *index)
{
	if (room_nr < 0x80) {
		*table = TOP;
		*index = room_nr;
	} else if (room_nr < 0x100) {
		*table = BOTTOM;
		*index = room_nr - 0x80;
	} else if (room_nr < 0x200) {
		*table = INDOOR_A;
		*index = room_nr - 0x100;
	} else if (room_nr < 0x300) {
		*table = INDOOR_B;
		*index = room_nr - 0x200;
	} else {
		*table = COLOR;
		*index = room_nr - 0x300;
	}
}

static int32_t overlay_at(uint32_t room_nr)
{
	if (room_nr < 0xCC)
		return (int32_t)(0x26 * SYNTH_ROM_BANK + room_nr * 80);
	if (room_nr < 0x100)
		return (int32_t)(0x27 * SYNTH_ROM_BANK + (room_nr - 0xCC) * 80);
	return -1;
}

/* Each room loaded and stored unchanged: its overlay kept, its entries copies of themselves. */
static void test_round_trip(void)
{
	static char want[OUT_MAX];
	uint32_t room_nr;
	int stored = 0, refused = 0;

	for (room_nr = 0; room_nr < 0x320; room_nr++) {
		int table;
		uint32_t index;
		const SynthEntry *objects, *entities = entry(ENTITIES, room_nr, NULL);
		LadxRoomStatus st;
		size_t len;

		objects_of(room_nr, &table, &index);
		objects = entry(table, index, NULL);
		if (!objects || !entities)
			continue;
		begin(1);
		st = ladx_room_load(&room, &enc, (int)room_nr, NULL);
		if (!entities_ok(room_nr) || objects->integer) {
			if (st != LADX_ROOM_MALFORMED)
				test_fail(TEST, "room %x: an integer entry or a misplaced 0xFF is refused", room_nr);
			refused++;
			continue;
		}
		if (st != LADX_ROOM_OK)
			test_fail(TEST, "room %x loads: status %d", room_nr, (int)st);
		checks++;
		if (!objects->entry_rebuilds || !entities->entry_rebuilds)
			continue;
		check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_OK, "the room stores back");
		end();
		len = (size_t)snprintf(want, sizeof want, HEADER "G test\n");
		if (overlay_at(room_nr) >= 0)
			len += (size_t)snprintf(want + len, sizeof want - len, "B %x =50\n",
						(unsigned)overlay_at(room_nr));
		len += (size_t)snprintf(want + len, sizeof want - len, "T entities %x @%x+%x\n", room_nr,
					entities->start, entities->stop - entities->start);
		snprintf(want + len, sizeof want - len, "T %s %x @%x+%x\n", synth_table_names[table], index,
			 objects->start, objects->stop - objects->start);
		if (strcmp(out, want) != 0)
			test_fail(TEST, "room %x stored unchanged:\n%s\nwanted\n%s", room_nr, out, want);
		stored++;
	}
	check(stored > 100 && refused > 0, "most rooms round-trip, the planted ones are refused");
}


static void plant(int table, uint32_t index, const uint8_t *bytes, size_t n)
{
	int32_t known[64];
	LadxWlogKey key;
	size_t i;

	for (i = 0; i < n; i++)
		known[i] = LADX_ENC_KNOWN;
	key.index = index;
	key.alt.at = NULL;
	key.alt.len = 0;
	if (ladx_enc_table(&enc, span("plant"), table, key, bytes, known, n) != LADX_ENC_OK)
		test_fail(TEST, "planting an entry");
}

static void append_hex(char *want, size_t cap, const uint8_t *b, size_t n)
{
	size_t len = strlen(want), i;

	for (i = 0; i < n; i++)
		len += (size_t)snprintf(want + len, cap - len, "%02x", b[i]);
	snprintf(want + len, cap - len, "\n");
}

/* Room 5: one object of each kind and two entities, planted by an earlier group. */
static const uint8_t objects5[] = {0x11, 0x22, 0x21, 0x30, 0x83, 0x14, 0x40, 0xC2, 0x37, 0x50,
				   0xE1, 0x10, 0x55, 0x60, 0x70, 0xE0, 0x07, 0x66, 0x10, 0x20, 0xFE};
static const uint8_t entities5[] = {0x32, 0xDE, 0x65, 0x4C, 0xFF};

static void load_planted(void)
{
	no_ks = 1;
	begin(1);
	no_ks = 0;
	plant(ENTITIES, 5, entities5, sizeof entities5);
	plant(TOP, 5, objects5, sizeof objects5);
	check(ladx_room_load(&room, &enc, 5, NULL) == LADX_ROOM_OK, "the planted room loads");
}

static void test_parse(void)
{
	const LadxRoomObject *o = room.objects;

	load_planted();
	check(room.animation_id == 0x11 && room.floor_object == 0x22 && room.object_count == 5, "header and objects");
	check(o[0].kind == LADX_ROOM_SINGLE && o[0].x == 1 && o[0].y == 2 && o[0].type_id == 0x30, "a single object");
	check(o[1].kind == LADX_ROOM_HORIZONTAL && o[1].x == 4 && o[1].y == 1 && o[1].count == 3 &&
		      o[1].type_id == 0x40,
	      "a horizontal object");
	check(o[2].kind == LADX_ROOM_VERTICAL && o[2].x == 7 && o[2].y == 3 && o[2].count == 2 && o[2].type_id == 0x50,
	      "a vertical object");
	check(o[3].kind == LADX_ROOM_WARP && o[3].warp_type == 1 && o[3].map_nr == 0x10 && o[3].room == 0x255 &&
		      o[3].target_x == 0x60 && o[3].target_y == 0x70,
	      "an indoor B warp, its room offset by its map");
	check(o[4].kind == LADX_ROOM_WARP && o[4].warp_type == 0 && o[4].room == 0x66, "an overworld warp");
	check(room.entity_count == 2 && room.entities[0].x == 2 && room.entities[0].y == 3 &&
		      room.entities[0].type_id == 0xDE && room.entities[1].type_id == 0x4C,
	      "the entities");
	check(room.has_overlay && ladx_room_has_entity(&room, 0x4C) && !ladx_room_has_entity(&room, 0x4D) &&
		      ladx_room_has_object(&room, 0x50) && !ladx_room_has_object(&room, 0x66),
	      "has entity and object, a warp's room no type");
}

static void test_warp_maps(void)
{
	static const struct {
		uint8_t type, map;
		int room;
	} warps[] = {{2, 0xFF, 0x377}, {1, 0x05, 0x177}, {3, 0x06, 0x277}, {1, 0x19, 0x277}, {1, 0x1A, 0x177}};
	uint8_t raw[8] = {0, 0, 0, 0, 0x77, 1, 2, 0xFE};
	size_t i;

	for (i = 0; i < sizeof warps / sizeof warps[0]; i++) {
		raw[2] = (uint8_t)(0xE0 | warps[i].type);
		raw[3] = warps[i].map;
		begin(1);
		plant(TOP, 5, raw, sizeof raw);
		check(ladx_room_load(&room, &enc, 5, NULL) == LADX_ROOM_OK && room.objects[0].room == warps[i].room,
		      "a warp's room by its map: colour dungeon, indoor A, indoor B");
	}
}

/* LADXR's helpers, then store: entries with no origin, the overlay's edited bytes literals. */
static void test_edits_and_store(void)
{
	static char want[OUT_MAX];
	uint8_t objects[sizeof objects5], overlay[80];
	const uint8_t entities[] = {0x11, 0x99, 0x65, 0x4C, 0x02, 0x33, 0xFF};
	int32_t at = overlay_at(5);
	size_t i;

	load_planted();
	check(ladx_room_insert_entity(&room, 0, 1, 1, 0x99) == LADX_ROOM_OK, "insert an entity first");
	ladx_room_remove_entities(&room, 0xDE);
	check(ladx_room_add_entity(&room, 2, 0, 0x33) == LADX_ROOM_OK, "add an entity last");
	check(ladx_room_change_object(&room, 1, 2, 0x31) == LADX_ROOM_OK, "change an object");
	check(ladx_room_move_object(&room, 4, 1, 5, 1) == LADX_ROOM_OK, "move an object");
	ladx_room_remove_object(&room, 7, 3);
	check(room.object_count == 4, "remove an object, the warps kept");
	memcpy(objects, objects5, sizeof objects5);
	objects[3] = 0x31;
	objects[5] = 0x15;
	memmove(objects + 7, objects + 10, sizeof objects5 - 10);
	for (i = 0; i < 80; i++)
		overlay[i] = synth.rom[at + (int32_t)i];
	overlay[21] = 0x31;
	overlay[14] = 0x22;
	overlay[15] = 0x40;
	check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_OK, "store");
	end();
	snprintf(want, sizeof want, HEADER "G plant\nT entities 5 ");
	append_hex(want, sizeof want, entities5, sizeof entities5);
	strcat(want, "T rooms_overworld_top 5 ");
	append_hex(want, sizeof want, objects5, sizeof objects5);
	snprintf(want + strlen(want), sizeof want - strlen(want), "G test\nB %x =e ", (unsigned)at);
	append_hex(want, sizeof want, overlay + 14, 2);
	want[strlen(want) - 1] = '\0';
	strcat(want, " =5 ");
	append_hex(want, sizeof want, overlay + 21, 1);
	want[strlen(want) - 1] = '\0';
	strcat(want, " =3a\nT entities 5 ");
	append_hex(want, sizeof want, entities, sizeof entities);
	strcat(want, "T rooms_overworld_top 5 ");
	append_hex(want, sizeof want, objects, sizeof objects5 - 3);
	if (strcmp(out, want) != 0)
		test_fail(TEST, "edits stored:\n%s\nwanted\n%s", out, want);
	checks++;
}

static void test_update_overlay(void)
{
	size_t i;
	int ok = 1;

	load_planted();
	check(ladx_room_update_overlay(&room, 0) == LADX_ROOM_OK, "update the overlay");
	for (i = 0; i < 80; i++) {
		int want = i == 21 ? 0x30 : (i >= 14 && i < 17) ? 0x40 : (i == 37 || i == 47) ? 0x50 : 0x22;

		ok &= room.overlay[i] == want && room.overlay_origin[i] == LADX_ENC_KNOWN;
	}
	check(ok, "updateOverlay: the floor, then each object, the warps skipped");
	load_planted();
	room.overlay[0] = 0x99;
	check(ladx_room_update_overlay(&room, 1) == LADX_ROOM_OK && room.overlay[0] == 0x99 && room.overlay[21] == 0x30,
	      "updateOverlay(preserve_floor) keeps the floor");
	room.objects[0].x = 70;
	check(ladx_room_update_overlay(&room, 1) == LADX_ROOM_RANGE, "an object outside the overlay raises");
	room.objects[0].x = -30;
	check(ladx_room_update_overlay(&room, 1) == LADX_ROOM_OK && room.overlay[70] == 0x30,
	      "a negative index wraps as Python's");
}

/* A source room's floor carries its origin into the overlay. */
static void test_floor_origin(void)
{
	static char want[OUT_MAX];
	const SynthEntry *objects = entry(TOP, 0x10, NULL);
	size_t len, i;

	begin(1);
	check(ladx_room_load(&room, &enc, 0x10, NULL) == LADX_ROOM_OK, "room 0x10 loads");
	room.object_count = 0;
	check(ladx_room_update_overlay(&room, 0) == LADX_ROOM_OK, "update an emptied overlay");
	len = (size_t)snprintf(want, sizeof want, "B %x", (unsigned)overlay_at(0x10));
	for (i = 0; i < 80; i++)
		len += (size_t)snprintf(want + len, sizeof want - len, " @%x+1", objects->start + 1);
	check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_OK, "store");
	end();
	check(strstr(out, want) != NULL, "the floor's source offset carried to every overlay byte");
}

static void test_alt_rooms(void)
{
	static char want[OUT_MAX];
	const SynthEntry *e = entry(TOP, 0, "Alt06");
	const SynthEntry *a = entry(INDOOR_A, 0, "Alt1F5");

	begin(1);
	check(ladx_room_load(&room, &enc, 0, "Alt06") == LADX_ROOM_OK && room.has_overlay && room.entity_count == 0,
	      "an alt room of the top table loads with its overlay");
	check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_OK, "store the alt room");
	end();
	snprintf(want, sizeof want, HEADER "G test\nB %x =50\nT rooms_overworld_top Alt06 @%x+%x\n",
		 0x27 * SYNTH_ROM_BANK + 0x1040, e->start, e->stop - e->start);
	if (strcmp(out, want) != 0)
		test_fail(TEST, "alt room stored:\n%s\nwanted\n%s", out, want);
	begin(1);
	check(ladx_room_load(&room, &enc, 0, "Alt1F5") == LADX_ROOM_OK && !room.has_overlay,
	      "an indoor alt room has no overlay");
	check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_OK, "store it");
	end();
	snprintf(want, sizeof want, HEADER "G test\nT rooms_indoor_a Alt1F5 @%x+%x\n", a->start, a->stop - a->start);
	check(strcmp(out, want) == 0, "an indoor alt room stores its objects alone");
	begin(1);
	check(ladx_room_load(&room, &enc, 0, "Alt99") == LADX_ROOM_BAD_ROOM, "an unknown alt room is refused");
	check(ladx_room_load(&room, &enc, 0x320, NULL) == LADX_ROOM_BAD_ROOM, "a room over 0x31F is refused");
	check(ladx_room_load(&room, &enc, -1, NULL) == LADX_ROOM_BAD_ROOM, "a negative room is refused");
}

/* store(rom, new_room_nr), and the optional bytes RoomEditor stores. */
static void test_store_elsewhere(void)
{
	static char want[OUT_MAX];
	begin(1);
	check(ladx_room_load(&room, &enc, 0x10, NULL) == LADX_ROOM_OK, "room 0x10 loads");
	room.tileset_index = 0x1F3;
	room.palette_index = 0x07;
	room.attribset_bank = 0x2A;
	room.attribset_addr = 0x6543;
	check(ladx_room_store(&room, &enc, span("test"), 0x11, NULL) == LADX_ROOM_OK, "store as room 0x11");
	end();
	snprintf(want, sizeof want, HEADER "G test\nB %x 4365\nB %x 2a\nB %x 07\nB %x @%x+50\nB %x f3\nT entities 11 ",
		 0x1A * SYNTH_ROM_BANK + 0x1E76 + 0x22, 0x1A * SYNTH_ROM_BANK + 0x2487, 0x21 * SYNTH_ROM_BANK + 0x0300,
		 (unsigned)overlay_at(0x11), (unsigned)overlay_at(0x10), 0x3F * SYNTH_ROM_BANK + 0x3F11);
	if (strncmp(out, want, strlen(want)) != 0 || !strstr(out, "\nT rooms_overworld_top 11 "))
		test_fail(TEST, "stored elsewhere:\n%s\nwanted it to start with\n%s", out, want);
	checks++;
	begin(1);
	check(ladx_room_load(&room, &enc, 0x10, NULL) == LADX_ROOM_OK, "room 0x10 loads");
	room.tileset_index = 0x44;
	room.palette_index = 0x07;
	check(ladx_room_store(&room, &enc, span("test"), 0x120, NULL) == LADX_ROOM_OK, "store as indoor room 0x120");
	end();
	snprintf(want, sizeof want, HEADER "G test\nB %x 44\nT entities 120 ", 0x20 * SYNTH_ROM_BANK + 0x2EB3 + 0x20);
	check(strncmp(out, want, strlen(want)) == 0 && strstr(out, "\nT rooms_indoor_a 20 "),
	      "an indoor room stores its tileset alone, and no overlay");
}

static void test_refusals(void)
{
	const uint8_t no_end[] = {0x01, 0x02, 0x21, 0x30};
	const uint8_t short_warp[] = {0x01, 0x02, 0xE1, 0x10, 0xFE};
	const uint8_t odd_entities[] = {0x12, 0x34, 0xFF, 0x00};
	const uint8_t trailing[] = {0x01, 0x02, 0xFE, 0x00};

	begin(1);
	plant(TOP, 5, no_end, sizeof no_end);
	check(ladx_room_load(&room, &enc, 5, NULL) == LADX_ROOM_MALFORMED, "objects without 0xFE are refused");
	begin(1);
	plant(TOP, 5, short_warp, sizeof short_warp);
	check(ladx_room_load(&room, &enc, 5, NULL) == LADX_ROOM_MALFORMED, "a warp past the entry is refused");
	begin(1);
	plant(TOP, 5, trailing, sizeof trailing);
	check(ladx_room_load(&room, &enc, 5, NULL) == LADX_ROOM_MALFORMED, "bytes after 0xFE are refused");
	begin(1);
	plant(ENTITIES, 5, odd_entities, sizeof odd_entities);
	check(ladx_room_load(&room, &enc, 5, NULL) == LADX_ROOM_MALFORMED, "bytes after the entities' 0xFF are refused");
	begin(0);
	check(ladx_room_load(&room, &enc, 0x10, NULL) == LADX_ROOM_NO_ROM, "a source entry needs the ROM");
	load_planted();
	room.entities[0].x = 0x100;
	check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_RANGE, "an entity out of its byte");
	load_planted();
	room.objects[1].count = 0x100;
	check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_RANGE, "a count out of its byte");
	load_planted();
	room.has_overlay = 0;
	check(ladx_room_store(&room, &enc, span("test"), -1, NULL) == LADX_ROOM_RANGE, "an overlay to store and none");
}

int main(void)
{
	synth_rom_build(&synth, 0x7E470001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	test_round_trip();
	test_parse();
	test_warp_maps();
	test_edits_and_store();
	test_update_overlay();
	test_floor_origin();
	test_alt_rooms();
	test_store_elsewhere();
	test_refusals();
	printf("%s: RoomEditor's load, edits and store on a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
