#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_goal"
#define OUT_MAX (1 << 20)
#define TEXTS 0
#define ENTITIES 1
#define TOP 2
#define BOTTOM 3
#define UNITS 265
#define UNIT_BASE (0x19u * SYNTH_ROM_BANK) /* unit k (k >= 1) writes its number at UNIT_BASE + 4k */
#define SIGN_DATA (0x0Cu * SYNTH_ROM_BANK + 0x3800u)
#define SIGN_NUMBER (0x0Cu * SYNTH_ROM_BANK + 0x3900u)
#define MAZE_CODE (0x0Cu * SYNTH_ROM_BANK)

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog, want_log, project_log;
static LadxKs ks;
static LadxRomTables rom_tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static SynthRom synth;
static char out[OUT_MAX], want[OUT_MAX], got[OUT_MAX];
static size_t out_len, want_len, got_len;
static char order_text[1024], early_text[65536], goal_text[65536];
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

typedef struct Buffer {
	char *text;
	size_t *len;
} Buffer;

static void sink(void *ctx, const char *text, size_t len)
{
	Buffer *b = ctx;

	if (*b->len + len >= OUT_MAX)
		test_fail(TEST, "log text over %d bytes", OUT_MAX);
	memcpy(b->text + *b->len, text, len);
	*b->len += len;
	b->text[*b->len] = '\0';
}

static LadxSpan span(const char *s)
{
	LadxSpan sp;

	sp.at = s;
	sp.len = strlen(s);
	return sp;
}

static size_t appendf(char *buf, size_t cap, size_t len, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf + len, cap - len, fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= cap - len)
		test_fail(TEST, "planted text over %u bytes", (unsigned)cap);
	return len + (size_t)n;
}

/* Every overworld room's entities planted by an earlier table group: room 6 holds two egg entities. */
static int planted_entities(int room, uint8_t *b)
{
	int n = 0;

	if (room == 6 || room % 3 == 0) {
		b[n++] = (uint8_t)(0x10 | (room % 10));
		b[n++] = 0xDE;
	}
	b[n++] = 0x21;
	b[n++] = (uint8_t)(0x50 + room % 16);
	if (room == 6) {
		b[n++] = 0x45;
		b[n++] = 0xDE;
	}
	b[n++] = 0xFF;
	return n;
}

static void plant_files(void)
{
	size_t len;
	int room, k, i, n;
	uint8_t b[8];

	snprintf(order_text, sizeof order_text,
		 "ladx-tables-order 1\nsection generator.py generateRom\ntable early early.1 patches.early(rom)\n"
		 "C goal - patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])\n");
	len = appendf(early_text, sizeof early_text, 0, "ladx-tables 1\ngroup early\ntables " TABLES "\nunit early.1\nE\n");
	for (room = 0; room < 0x100; room++) {
		len = appendf(early_text, sizeof early_text, len, "T entities %x ", room);
		n = planted_entities(room, b);
		for (i = 0; i < n; i++)
			len = appendf(early_text, sizeof early_text, len, "%02x", b[i]);
		len = appendf(early_text, sizeof early_text, len, "\n");
	}
	len = appendf(goal_text, sizeof goal_text, 0, "ladx-tables 1\ngroup goal\ntables " TABLES "\nunit goal.1\nE\n");
	len = appendf(goal_text, sizeof goal_text, len, "B %x c0de\nB %x ", MAZE_CODE, SIGN_DATA);
	for (i = 0; i < 0x200; i++)
		len = appendf(goal_text, sizeof goal_text, len, "00");
	len = appendf(goal_text, sizeof goal_text, len, "\n");
	/* unit k + 1 writes k, big-endian, at UNIT_BASE + 4k */
	for (k = 1; k < UNITS; k++)
		len = appendf(goal_text, sizeof goal_text, len, "unit goal.%d\nE\nB %x %04x\n", k + 1,
			      UNIT_BASE + 4u * (unsigned)k, k);
}

/* The planted tables, goal.txt cut to its first units units. */
static void open_tables(int units)
{
	static char text[65536];
	LadxTablesFile order, groups[2];
	LadxTablesError err;
	char head[32], *cut;

	strcpy(text, goal_text);
	snprintf(head, sizeof head, "unit goal.%d\n", units + 1);
	if ((cut = strstr(text, head)) != NULL)
		*cut = '\0';
	order.name = "order.txt";
	order.text = span(order_text);
	groups[0].name = "early.txt";
	groups[0].text = span(early_text);
	groups[1].name = "goal.txt";
	groups[1].text = span(text);
	if (ladx_tables_open(&tables, &order, groups, 2, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static void randomize(const char *const *assignments, uint32_t seed_number)
{
	static LadxSettings settings;
	uint8_t seed[16];
	int i;

	ladx_settings_defaults(&settings);
	for (i = 0; assignments[i]; i++)
		if (ladx_settings_set(&settings, assignments[i]) != LADX_SETTINGS_OK)
			test_fail(TEST, "cannot set %s", assignments[i]);
	ladx_settings_validate(&settings);
	memset(seed, 0x5A, sizeof seed);
	seed[0] = (uint8_t)seed_number;
	seed[1] = (uint8_t)(seed_number >> 8);
	ladx_randomize(&randomizer, &settings, seed, sizeof seed, NULL, NULL);
	pipeline.settings = &settings;
}

/* The pipeline with the ROM, its encoder without KS: no ROM matching, so every computed byte is a literal. */
static LadxPipelineStatus run(LadxPipelineError *err)
{
	static Buffer b = {out, &out_len};
	int count;

	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, &b) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, synth.rom, SYNTH_ROM_SIZE, NULL, &rom_tables, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	pipeline.tables = &tables;
	pipeline.spot_count = 0;
	pipeline.randomizer = &randomizer;
	pipeline.rom = &rom;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	pipeline.ctx = NULL;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

/* The model's goal segment: net bank atoms and entries, rendered by a LadxWlog. */
static LadxAtom net_bank[SYNTH_ROM_SIZE];
static uint8_t net_set[SYNTH_ROM_SIZE];
static struct {
	int table;
	uint32_t index;
	LadxAtom atoms[512];
	size_t n;
} net_entry[64];
static size_t net_entries;

static void model_begin(void)
{
	memset(net_set, 0, sizeof net_set);
	net_entries = 0;
}

static void model_literal(uint32_t offset, int value)
{
	net_bank[offset] = LADX_ATOM_LITERAL(value);
	net_set[offset] = 1;
}

static void model_keep(uint32_t offset, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		net_bank[offset + i] = (LadxAtom)(offset + i);
		net_set[offset + i] = 1;
	}
}

static void model_entry(int table, uint32_t index, const uint8_t *bytes, size_t n)
{
	size_t i, e;

	for (e = 0; e < net_entries; e++)
		if (net_entry[e].table == table && net_entry[e].index == index)
			break;
	if (e == net_entries)
		net_entries++;
	net_entry[e].table = table;
	net_entry[e].index = index;
	net_entry[e].n = n;
	for (i = 0; i < n; i++)
		net_entry[e].atoms[i] = LADX_ATOM_LITERAL(bytes[i]);
}

static void model_text(const char *text)
{
	uint8_t bytes[512];
	size_t n;
	LadxOutcome raised;

	if (ladx_format_text(text, strlen(text), 0, NULL, bytes, sizeof bytes, &n, &raised) != LADX_TEXT_OK)
		test_fail(TEST, "formatText of %s", text);
	model_entry(TEXTS, 0x1A3, bytes, n);
}

static void model_unit(int k)
{
	model_literal(UNIT_BASE + 4u * (unsigned)k, k >> 8);
	model_literal(UNIT_BASE + 4u * (unsigned)k + 1, k & 0xFF);
}

/* Renders the model and the run's goal projection; they must be equal. */
static void compare(const char *what)
{
	static Buffer wb = {want, &want_len}, gb = {got, &got_len};
	LadxWlogKey key;
	uint32_t p, start;
	size_t e;
	int line;

	want_len = got_len = 0;
	want[0] = got[0] = '\0';
	if (ladx_wlog_begin(&want_log, span(TABLES), sink, &wb) != LADX_WLOG_OK)
		test_fail(TEST, "model log");
	for (p = 0; p < SYNTH_ROM_SIZE; p++) {
		if (!net_set[p])
			continue;
		for (start = p; p < SYNTH_ROM_SIZE && net_set[p]; p++)
			;
		if (ladx_wlog_bank(&want_log, span("goal"), start, net_bank + start, p - start) != LADX_WLOG_OK)
			test_fail(TEST, "model bank record");
	}
	key.alt.at = NULL;
	key.alt.len = 0;
	for (e = 0; e < net_entries; e++) {
		key.index = net_entry[e].index;
		if (ladx_wlog_table(&want_log, span("goal"), net_entry[e].table, key, net_entry[e].atoms,
				    net_entry[e].n) != LADX_WLOG_OK)
			test_fail(TEST, "model table record");
	}
	if (ladx_wlog_end(&want_log) != LADX_WLOG_OK ||
	    ladx_wlog_project(&project_log, span(out), span("goal"), sink, &gb, &line) != LADX_WLOG_OK)
		test_fail(TEST, "%s: rendering or projecting", what);
	if (strcmp(got, want) != 0)
		test_fail(TEST, "%s: goal's projection\n%s\nwanted\n%s", what, got, want);
	checks++;
}

static const LadxWorldSetup *world(void)
{
	const LadxWorldSetup *ws = ladx_randomizer_world_setup(&randomizer);

	if (!ws)
		test_fail(TEST, "no world setup");
	return ws;
}

static void test_counts(void)
{
	static const char *const instruments[][3] = {
		{"goal=instruments", "goalcount=0", NULL}, {"goal=instruments", "goalcount=4", NULL},
		{"goal=instruments", "goalcount=7", NULL}, {"goal=vanilla", NULL, NULL},
	};
	static const int counts[] = {0, 4, 7, 8};
	LadxPipelineError err;
	char text[64];
	size_t i;

	for (i = 0; i < sizeof counts / sizeof counts[0]; i++) {
		randomize(instruments[i], 1);
		check(world()->goal_count == counts[i], "the world setup's count");
		check(run(&err) == LADX_PIPELINE_OK, "the pipeline runs goal");
		model_begin();
		snprintf(text, sizeof text, "You need %d instruments", counts[i]);
		model_text(text);
		if (counts[i] < 8)
			model_unit(2 + counts[i]);
		compare("a count: its text, then its unit (none for 8)");
	}
}

/* goal=open-4 draws -1 on some seeds: the instruments goal with the count -1, unit 1. */
static void test_count_minus_one(void)
{
	static const char *const open4[] = {"goal=open-4", NULL};
	LadxPipelineError err;
	uint32_t s;

	for (s = 0; s < 200; s++) {
		randomize(open4, s);
		if (world()->goal == LADX_GOAL_INSTRUMENTS && world()->goal_count == -1)
			break;
	}
	check(s < 200, "a seed of open-4 draws -1");
	check(run(&err) == LADX_PIPELINE_OK, "the pipeline runs goal");
	model_begin();
	model_text("You need -1 instruments");
	model_unit(1);
	compare("the count -1: Python's %d text, unit 1");
}

static void test_open_and_seashells(void)
{
	static const char *const open[] = {"goal=open", NULL}, *const shells[] = {"goal=seashells", NULL};
	LadxPipelineError err;

	randomize(open, 1);
	check(run(&err) == LADX_PIPELINE_OK && !strstr(out, "G goal"), "open: goal writes nothing");
	randomize(shells, 1);
	check(run(&err) == LADX_PIPELINE_OK && !strstr(out, "G goal"), "seashells: goal writes nothing");
}

static void test_specific(void)
{
	static const char *const counts[][3] = {
		{"goal=specific", "goalcount=1", NULL}, {"goal=specific", "goalcount=3", NULL},
		{"goal=specific", "goalcount=7", NULL}, {"goal=specific", "goalcount=random", NULL},
	};
	LadxPipelineError err;
	char text[256];
	size_t i, len;
	uint32_t s;

	for (i = 0; i < sizeof counts / sizeof counts[0]; i++)
		for (s = 0; s < 3; s++) {
			const char *d;
			unsigned mask = 0;

			randomize(counts[i], s);
			check(run(&err) == LADX_PIPELINE_OK, "the pipeline runs goal");
			len = (size_t)snprintf(text, sizeof text, "You need:");
			for (d = world()->goal_instruments; *d; d++) {
				len += (size_t)snprintf(text + len, sizeof text - len, "\n{INSTRUMENT%c}", *d);
				mask |= 1u << (*d - '1');
			}
			check(world()->goal == LADX_GOAL_SPECIFIC && mask, "a specific goal");
			model_begin();
			model_text(text);
			model_unit(9 + (int)mask);
			compare("specific: the text in drawn order, the unit by the sorted set");
		}
}

static void model_room(int room, const LadxRoomEntity *entities, size_t count)
{
	const LadxRomEntry *objects = ladx_rom_entry(&rom_tables, room < 0x80 ? TOP : BOTTOM,
						     (uint32_t)(room < 0x80 ? room : room - 0x80), NULL);
	uint8_t b[512];
	size_t i, n = 0;

	for (i = 0; i < count; i++) {
		b[n++] = (uint8_t)(entities[i].x | entities[i].y << 4);
		b[n++] = (uint8_t)entities[i].type_id;
	}
	b[n++] = 0xFF;
	model_entry(ENTITIES, (uint32_t)room, b, n);
	model_entry(room < 0x80 ? TOP : BOTTOM, (uint32_t)(room < 0x80 ? room : room - 0x80), synth.rom + objects->start,
		    objects->stop - objects->start);
	model_keep(room < 0xCC ? 0x26u * SYNTH_ROM_BANK + (uint32_t)room * 80
			       : 0x27u * SYNTH_ROM_BANK + (uint32_t)(room - 0xCC) * 80,
		   80);
}

/* The planted entities of room as a list, with what earlier signs of the model inserted. */
static LadxRoomEntity maze_entities[0x100][16];
static size_t maze_counts[0x100];

static void test_maze(void)
{
	static const char *const maze[] = {"goal=maze", NULL};
	LadxPipelineError err;
	const LadxWorldSetup *ws;
	uint32_t s;
	int room, idx, i, n, k;
	uint8_t b[8];

	for (s = 0; s < 50; s++) {
		randomize(maze, s);
		if (ladx_randomizer_world_setup(&randomizer) && world()->sign_count == LADX_SIGN_MAZE_SIGNS)
			break;
	}
	check(s < 50, "a maze seed");
	ws = world();
	check(run(&err) == LADX_PIPELINE_OK, "the pipeline runs the maze");
	model_begin();
	model_literal(MAZE_CODE, 0xC0);
	model_literal(MAZE_CODE + 1, 0xDE);
	for (i = 0; i < 0x200; i++)
		model_literal(SIGN_DATA + (uint32_t)i, 0);
	for (room = 0; room < 0x100; room++) {
		n = planted_entities(room, b);
		maze_counts[room] = 0;
		for (i = 0; i + 1 < n; i += 2) {
			maze_entities[room][maze_counts[room]].x = b[i] & 0x0F;
			maze_entities[room][maze_counts[room]].y = b[i] >> 4;
			maze_entities[room][maze_counts[room]++].type_id = b[i + 1];
		}
	}
	for (idx = 0; idx < ws->sign_count; idx++) {
		int x = ws->signs[idx].x, y = ws->signs[idx].y;

		room = x / 10 | (y / 8) << 4;
		memmove(maze_entities[room] + 1, maze_entities[room], maze_counts[room] * sizeof maze_entities[0][0]);
		maze_entities[room][0].x = x % 10;
		maze_entities[room][0].y = y % 8;
		maze_entities[room][0].type_id = 0x4C;
		maze_counts[room]++;
		model_room(room, maze_entities[room], maze_counts[room]);
		model_literal(SIGN_DATA + (uint32_t)room, idx < ws->sign_count - 1 ? ws->directions[idx] : 0x40);
		model_literal(SIGN_NUMBER + (uint32_t)room, idx);
	}
	for (i = 0, k = 0; i < (int)maze_counts[6]; i++)
		if (maze_entities[6][i].type_id != 0xDE)
			maze_entities[6][k++] = maze_entities[6][i];
	check(k + 2 == (int)maze_counts[6] && maze_entities[6][0].type_id == 0x4C,
	      "room 6 holds the last sign and loses both egg entities");
	model_room(6, maze_entities[6], (size_t)k);
	compare("the maze: its unit, each sign's room and table bytes, room 6 without 0xDE");
}

static void test_refusals(void)
{
	static const char *const count[] = {"goal=instruments", NULL};
	LadxPipelineError err;

	open_tables(UNITS - 1);
	randomize(count, 1);
	check(run(&err) == LADX_PIPELINE_GROUP && strcmp(err.group, "goal") == 0,
	      "a constants file without 265 units fails the group");
	open_tables(UNITS);
	pipeline.rom = NULL;
}

int main(void)
{
	synth_rom_build(&synth, 0x7E470002u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	plant_files();
	open_tables(UNITS);
	test_counts();
	test_count_minus_one();
	test_open_and_seashells();
	test_specific();
	test_maze();
	test_refusals();
	printf("%s: the goal group on a synthetic ROM and planted constants (%d checks)\n", TEST, checks);
	return 0;
}
