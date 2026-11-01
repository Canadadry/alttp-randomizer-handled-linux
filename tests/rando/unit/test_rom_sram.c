#include "rando.h"
#include "rando_case.h"
#include "rando_rom.h"
#include "support.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_rom_sram"
#define SRAM_ADDRESS 0x183000u
#define SRAM_SIZE 0x500
#define COUNT_ADDRESS 0x180196u
#define ROM_SIZE (2u * 1024 * 1024)
#define ORACLE_WATCHDOG 1000u
#define ROM_WRITES_MAX (256u * 1024)
#define SRAM_LINES_MAX 32

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

/* Expected bytes come from VT's InitialSram at 219fcafd, never from the C table. */
#define CONSTRUCTOR "20D=F0,20F=F0,379=68,401=FF,402=FF"
#define DEFAULT_EQUIPMENT "36C=18,36D=18,370=0A,371=1E"
#define OPEN_MODE "29B=20,3C5=02,3C6=14,3C8=01"

typedef struct Recorder {
	int calls;
	int sram_calls;
	int text_calls;
	bool sram_after_text;
	bool last_was_sram;
	uint32_t address;
	size_t count;
	uint8_t bytes[SRAM_SIZE];
	int count_calls;
	bool count_after_sram;
	bool only_heart_beep_after_count;
	uint32_t count_address;
	size_t count_size;
	uint8_t count_bytes[2];
	bool refuse_sram;
	bool refuse_count;
	int calls_after_refusal;
	bool refused;
} Recorder;

static bool record(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	Recorder *r = ctx;

	if (r->refused) {
		r->calls_after_refusal++;
		return true;
	}
	r->calls++;
	if (r->count_calls > 0 && strcmp(method, "setHeartBeepSpeed") != 0)
		r->only_heart_beep_after_count = false;
	if (strcmp(method, "setTotalItemCount") == 0) {
		r->count_calls++;
		r->count_after_sram = r->sram_calls == 1 && r->last_was_sram;
		r->count_address = address;
		r->count_size = count;
		if (count <= sizeof r->count_bytes)
			memcpy(r->count_bytes, bytes, count);
		if (r->refuse_count) {
			r->refused = true;
			return false;
		}
	}
	r->last_was_sram = strcmp(method, "writeInitialSram") == 0;
	if (strcmp(method, "writeText") == 0) {
		r->text_calls++;
		if (r->sram_calls > 0)
			r->sram_after_text = false;
	}
	if (strcmp(method, "writeInitialSram") != 0)
		return true;
	r->sram_calls++;
	r->sram_after_text = r->text_calls == 1;
	r->address = address;
	r->count = count;
	if (count <= sizeof r->bytes)
		memcpy(r->bytes, bytes, count);
	if (r->refuse_sram) {
		r->refused = true;
		return false;
	}
	return true;
}

static RandoGen gen, edited, before;
static Recorder rec;

static void run(const RandoGen *g)
{
	RandoRomWriter writer = {record, &rec};

	memset(&rec, 0, sizeof rec);
	rec.only_heart_beep_after_count = true;
	CHECK(rando_rom_write(g, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.sram_calls == 1 && rec.count == SRAM_SIZE);
}

/* Parses "OFS=VV,..." into a table of zeros; exits on a malformed list. */
static void parse_changes(const char *list, uint8_t table[SRAM_SIZE])
{
	const char *p = list;

	memset(table, 0, SRAM_SIZE);
	while (*p != '\0') {
		char *end;
		unsigned long ofs = strtoul(p, &end, 16), val;

		if (*end != '=' || ofs >= SRAM_SIZE)
			test_fail(TEST, "malformed change list: %s", list);
		val = strtoul(end + 1, &end, 16);
		if ((*end != ',' && *end != '\0') || val > 0xFF)
			test_fail(TEST, "malformed change list: %s", list);
		table[ofs] = (uint8_t)val;
		p = *end == ',' ? end + 1 : end;
	}
}

/* g's table holds exactly the non-zero bytes of the comma-joined lists, every other byte 0. */
static void expect(const char *what, const RandoGen *g, const char *a, const char *b, const char *c)
{
	const char *lists[3];
	uint8_t want[SRAM_SIZE], part[SRAM_SIZE];
	int i, k;

	lists[0] = a;
	lists[1] = b;
	lists[2] = c;
	memset(want, 0, sizeof want);
	for (k = 0; k < 3; k++) {
		if (lists[k] == NULL)
			continue;
		parse_changes(lists[k], part);
		for (i = 0; i < SRAM_SIZE; i++)
			if (part[i] != 0)
				want[i] = part[i];
	}
	run(g);
	for (i = 0; i < SRAM_SIZE; i++)
		if (rec.bytes[i] != want[i])
			test_fail(TEST, "%s: offset %03X is %02X, VT gives %02X", what, i, rec.bytes[i], want[i]);
}

static void test_shape(void)
{
	run(&gen);
	CHECK(rec.sram_after_text);
	CHECK(rec.address == SRAM_ADDRESS);
	CHECK(rec.address <= ROM_SIZE && rec.count <= ROM_SIZE - rec.address);
	CHECK(rec.count_calls == 1 && rec.count_after_sram && rec.only_heart_beep_after_count);
	CHECK(rec.count_address == COUNT_ADDRESS && rec.count_size == 2);
	CHECK(rec.count_address <= ROM_SIZE && rec.count_size <= ROM_SIZE - rec.count_address);
}

static void test_baseline(void)
{
	CHECK(gen.world.config.mode_state == RANDO_STATE_OPEN);
	expect("baseline", &gen, CONSTRUCTOR, OPEN_MODE, DEFAULT_EQUIPMENT);
}

static void test_states(void)
{
	static const RandoState open_states[] = {RANDO_STATE_OPEN, RANDO_STATE_RETRO, RANDO_STATE_INVERTED};
	size_t i;

	for (i = 0; i < sizeof open_states / sizeof open_states[0]; i++) {
		memcpy(&edited, &gen, sizeof gen);
		edited.world.config.mode_state = open_states[i];
		expect("open state", &edited, CONSTRUCTOR, OPEN_MODE, DEFAULT_EQUIPMENT);
	}
	memcpy(&edited, &gen, sizeof gen);
	edited.world.config.mode_state = RANDO_STATE_STANDARD;
	expect("standard", &edited, CONSTRUCTOR, DEFAULT_EQUIPMENT, NULL);
}

static void test_goals(void)
{
	static const RandoGoal pyramid[] = {RANDO_GOAL_FAST_GANON, RANDO_GOAL_GANONHUNT};
	static const RandoGoal closed[] = {RANDO_GOAL_GANON, RANDO_GOAL_DUNGEONS, RANDO_GOAL_PEDESTAL,
					   RANDO_GOAL_TRIFORCE_HUNT, RANDO_GOAL_COMPLETIONIST};
	size_t i;

	for (i = 0; i < sizeof pyramid / sizeof pyramid[0]; i++) {
		memcpy(&edited, &gen, sizeof gen);
		edited.world.config.goal = pyramid[i];
		expect("pyramid goal", &edited, CONSTRUCTOR "," OPEN_MODE, DEFAULT_EQUIPMENT, "2DB=20");
	}
	for (i = 0; i < sizeof closed / sizeof closed[0]; i++) {
		memcpy(&edited, &gen, sizeof gen);
		edited.world.config.goal = closed[i];
		expect("other goal", &edited, CONSTRUCTOR, OPEN_MODE, DEFAULT_EQUIPMENT);
	}
}

/* edited is the baseline with exactly items[0..n) pre-collected, in that order. */
static void pre_collect(const RandoItemId *items, size_t n)
{
	size_t i;

	memcpy(&edited, &gen, sizeof gen);
	rando_items_clear(&edited.world.pre_collected);
	edited.world.npre_collected = 0;
	for (i = 0; i < n; i++)
		rando_world_add_pre_collected(&edited.world, items[i]);
}

#define PRE_COLLECT(...) \
	do { \
		static const RandoItemId items_[] = {__VA_ARGS__}; \
		pre_collect(items_, sizeof items_ / sizeof items_[0]); \
	} while (0)

#define DEFAULT_ITEMS \
	RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer, RANDO_ITEM_BossHeartContainer, \
		RANDO_ITEM_BombUpgrade10, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade10

static void test_boots(void)
{
	PRE_COLLECT(DEFAULT_ITEMS, RANDO_ITEM_PegasusBoots);
	expect("boots", &edited, "20D=F0,20F=F0,355=01,379=6C,401=FF,402=FF", OPEN_MODE, DEFAULT_EQUIPMENT);
}

static void test_swords(void)
{
	PRE_COLLECT(DEFAULT_ITEMS, RANDO_ITEM_UncleSword);
	expect("uncle sword", &edited, CONSTRUCTOR "," OPEN_MODE, DEFAULT_EQUIPMENT, "359=01,417=01");
	PRE_COLLECT(DEFAULT_ITEMS, RANDO_ITEM_ProgressiveSword, RANDO_ITEM_ProgressiveSword);
	expect("two swords", &edited, CONSTRUCTOR "," OPEN_MODE, DEFAULT_EQUIPMENT, "359=02,417=02");
	PRE_COLLECT(DEFAULT_ITEMS, RANDO_ITEM_ProgressiveSword, RANDO_ITEM_ProgressiveSword, RANDO_ITEM_UncleSword,
		    RANDO_ITEM_ProgressiveSword, RANDO_ITEM_ProgressiveSword);
	expect("five swords", &edited, CONSTRUCTOR "," OPEN_MODE, DEFAULT_EQUIPMENT, "359=04,417=04");
}

static void test_hearts(void)
{
	PRE_COLLECT(RANDO_ITEM_RescueZelda);
	expect("no heart", &edited, CONSTRUCTOR, OPEN_MODE, "36C=18,36D=18");
	PRE_COLLECT(RANDO_ITEM_BossHeartContainer);
	expect("one heart", &edited, CONSTRUCTOR, OPEN_MODE, "36C=08,36D=08");
	PRE_COLLECT(RANDO_ITEM_BossHeartContainer, RANDO_ITEM_HeartContainer, RANDO_ITEM_HeartContainerNoAnimation,
		    RANDO_ITEM_BossHeartContainer);
	expect("four hearts", &edited, CONSTRUCTOR, OPEN_MODE, "36C=20,36D=20");
}

static void test_capacities(void)
{
	PRE_COLLECT(RANDO_ITEM_BombUpgrade10, RANDO_ITEM_BombUpgrade10, RANDO_ITEM_ArrowUpgrade10,
		    RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade10);
	expect("capacities", &edited, CONSTRUCTOR, OPEN_MODE, "36C=18,36D=18,370=14,371=1E");
}

static void test_swordless(void)
{
	PRE_COLLECT(DEFAULT_ITEMS, RANDO_ITEM_UncleSword);
	edited.world.config.mode_weapons = RANDO_WEAPONS_SWORDLESS;
	expect("swordless", &edited, CONSTRUCTOR "," OPEN_MODE, DEFAULT_EQUIPMENT, "061=80,093=80,359=FF");
}

static void test_timer(void)
{
	int i;

	run(&gen);
	for (i = 0x454; i <= 0x457; i++)
		CHECK(rec.bytes[i] == 0);
}

static void test_ganons_tower(void)
{
	memcpy(&edited, &gen, sizeof gen);
	edited.options.crystals_tower = RANDO_CRYSTALS_RANDOM;
	edited.world.config.crystals_tower = 0;
	expect("drawn 0", &edited, CONSTRUCTOR "," OPEN_MODE, DEFAULT_EQUIPMENT, "2C3=20");
	edited.options.crystals_tower = RANDO_CRYSTALS_0;
	expect("chosen 0", &edited, CONSTRUCTOR, OPEN_MODE, DEFAULT_EQUIPMENT);
	edited.options.crystals_tower = RANDO_CRYSTALS_RANDOM;
	edited.world.config.crystals_tower = 3;
	expect("drawn 3", &edited, CONSTRUCTOR, OPEN_MODE, DEFAULT_EQUIPMENT);
}

/* VT's World::getTotalItemCount, counted by hand on every state: 216. */
static void expect_total(const char *what, const RandoGen *g)
{
	run(g);
	if (rec.count_bytes[0] != 0xD8 || rec.count_bytes[1] != 0x00)
		test_fail(TEST, "%s: total item count is %02X%02X, VT gives D800", what, rec.count_bytes[0],
			  rec.count_bytes[1]);
}

static void test_total_item_count(void)
{
	static const RandoOptionState states[] = {RANDO_OPTION_STATE_OPEN, RANDO_OPTION_STATE_STANDARD,
						  RANDO_OPTION_STATE_INVERTED, RANDO_OPTION_STATE_RETRO};
	static const RandoVariant variants[] = {RANDO_VARIANT_OPEN, RANDO_VARIANT_STANDARD, RANDO_VARIANT_INVERTED,
						RANDO_VARIANT_RETRO};
	RandoOptions options;
	RandoLocationId from = rando_location_find("Link's House"), to = rando_location_find("Sahasrahla");
	RandoItemId moved;
	size_t i;

	for (i = 0; i < sizeof states / sizeof states[0]; i++) {
		rando_options_init(&options);
		options.state = states[i];
		CHECK(rando_generate(&edited, &options, 1, 0, NULL, NULL) == RANDO_COMPLETE);
		CHECK(edited.world.variant == variants[i]);
		expect_total("state", &edited);
	}
	CHECK(from != RANDO_LOCATION_NONE && to != RANDO_LOCATION_NONE);
	memcpy(&edited, &gen, sizeof gen);
	moved = rando_world_item(&edited.world, from);
	CHECK(rando_world_set_item(&edited.world, to, moved));
	CHECK(rando_world_set_item(&edited.world, from, RANDO_ITEM_NONE));
	expect_total("item moved", &edited);
}

static void test_refusal(void)
{
	RandoRomWriter writer = {record, &rec};

	memset(&rec, 0, sizeof rec);
	rec.refuse_sram = true;
	CHECK(!rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.sram_calls == 1 && rec.count_calls == 0 && rec.calls_after_refusal == 0);
	memset(&rec, 0, sizeof rec);
	rec.refuse_count = true;
	CHECK(!rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.count_calls == 1 && rec.calls_after_refusal == 0);
}

static void test_const(void)
{
	memcpy(&before, &gen, sizeof gen);
	run(&gen);
	CHECK(memcmp(&before, &gen, sizeof gen) == 0);
}

/* The InitialSram methods the C ports; a golden `sram` line naming another one fails. */
static const char *const ported_methods[] = {
	"preOpenCastleGate",   "preOpenGanonsTower",   "preOpenPyramid",	"setProgressIndicator",
	"setProgressFlags",    "setStartingEntrance",  "setSwordlessCurtains", "setStartingEquipment",
	"setStartingTimer",
};

typedef struct SramLine {
	char method[64];
	char changes[FIXTURE_LINE_MAX];
	uint8_t table[SRAM_SIZE];
	bool changed[SRAM_SIZE];
} SramLine;

static SramLine sram_lines[SRAM_LINES_MAX];

static bool is_ported(const char *method)
{
	size_t i;

	for (i = 0; i < sizeof ported_methods / sizeof ported_methods[0]; i++)
		if (strcmp(method, ported_methods[i]) == 0)
			return true;
	return false;
}

/* The golden's `sram` lines, in order; returns their count. */
static int read_sram_lines(const char *path)
{
	Fixture fx;
	int n = 0;

	fixture_open(&fx, path);
	while (fixture_next(&fx)) {
		SramLine *line;
		char *method, *changes, *p;

		if (strncmp(fx.record, "sram ", 5) != 0)
			continue;
		if (n == SRAM_LINES_MAX)
			fixture_fail(&fx, "too many sram lines");
		line = &sram_lines[n++];
		method = fx.record + 5;
		changes = strchr(method, ' ');
		if (changes == NULL || (size_t)(changes - method) >= sizeof line->method)
			fixture_fail(&fx, "malformed sram line");
		*changes++ = '\0';
		if (!is_ported(method))
			fixture_fail(&fx, "an InitialSram method the C does not port");
		strcpy(line->method, method);
		strcpy(line->changes, changes);
		memset(line->changed, 0, sizeof line->changed);
		if (strcmp(changes, "-") == 0) {
			memset(line->table, 0, sizeof line->table);
			continue;
		}
		parse_changes(changes, line->table);
		for (p = changes; p != NULL; p = strchr(p, ',') != NULL ? strchr(p, ',') + 1 : NULL)
			line->changed[strtoul(p, NULL, 16)] = true;
	}
	fixture_close(&fx);
	return n;
}

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* Fills out with the bytes of the golden write tagged method, at address; fails when it is missing,
 * repeated, elsewhere or not count bytes long. */
static void golden_write(const char *path, char *text, const char *method, uint32_t address, uint8_t *out,
			 size_t count)
{
	char *line;
	int found = 0;

	for (line = strtok(text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
		char *hex = strchr(line, ' '), *tag;
		size_t i;

		if (hex == NULL || (tag = strchr(hex + 1, ' ')) == NULL || strcmp(tag + 1, method) != 0)
			continue;
		found++;
		if (strtoul(line, NULL, 16) != address || (size_t)(tag - hex - 1) != 2 * count)
			test_fail(TEST, "%s: the %s write is not %zu bytes at %06X", path, method, count,
				  (unsigned)address);
		for (i = 0; i < count; i++) {
			int hi = hex_nibble(hex[1 + 2 * i]), lo = hex_nibble(hex[2 + 2 * i]);

			if (hi < 0 || lo < 0)
				test_fail(TEST, "%s: malformed %s bytes", path, method);
			out[i] = (uint8_t)(hi << 4 | lo);
		}
	}
	if (found != 1)
		test_fail(TEST, "%s: %d %s writes, expected 1", path, found, method);
}

/* Reads the file into text, whole; strtok then cuts it, so it is read again per method. */
static void read_rom_writes(const char *path, char *text, size_t size)
{
	FILE *f = fopen(path, "rb");
	size_t n;

	if (f == NULL)
		test_fail(TEST, "cannot open %s", path);
	n = fread(text, 1, size - 1, f);
	if (!feof(f) || ferror(f))
		test_fail(TEST, "%s: larger than %zu bytes or unreadable", path, size - 1);
	fclose(f);
	text[n] = '\0';
}

/* The C writeInitialSram and setTotalItemCount writes of the case's generation, against its golden's:
 * each differing offset is named with both bytes and the golden sram line(s) that changed it. */
static void check_golden_case(const char *dir, const char *name)
{
	static char text[ROM_WRITES_MAX];
	uint8_t golden[SRAM_SIZE], count[2];
	char path[512], error[RANDO_CASE_ERROR_MAX];
	RandoCase c;
	uint32_t cap;
	int nlines, ndiffs = 0, i, k;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	if (!rando_case_read(path, &c, error))
		test_fail(TEST, "%s", error);
	cap = !c.has_attempts ? ORACLE_WATCHDOG : c.attempts == UINT32_MAX ? 0 : c.attempts + 1;
	if (rando_generate(&edited, &c.options, c.seed, cap, NULL, NULL) != RANDO_COMPLETE)
		test_fail(TEST, "%s: the generation does not complete", path);
	run(&edited);
	snprintf(path, sizeof path, "%s/%s/rom_writes.txt", dir, name);
	read_rom_writes(path, text, sizeof text);
	golden_write(path, text, "writeInitialSram", SRAM_ADDRESS, golden, sizeof golden);
	read_rom_writes(path, text, sizeof text);
	golden_write(path, text, "setTotalItemCount", COUNT_ADDRESS, count, sizeof count);
	snprintf(path, sizeof path, "%s/%s/rom_buffers.txt", dir, name);
	nlines = read_sram_lines(path);
	for (i = 0; i < SRAM_SIZE; i++) {
		bool named = false;

		if (rec.bytes[i] == golden[i])
			continue;
		ndiffs++;
		fprintf(stderr, "%s/%s: offset %03X is %02X in C, %02X in the golden, set by", dir, name, i,
			rec.bytes[i], golden[i]);
		for (k = 0; k < nlines; k++)
			if (sram_lines[k].changed[i]) {
				fprintf(stderr, " `sram %s %s`", sram_lines[k].method, sram_lines[k].changes);
				named = true;
			}
		fprintf(stderr, "%s\n", named ? "" : " the constructor");
	}
	if (ndiffs > 0)
		test_fail(TEST, "%s/%s: %d initial SRAM bytes differ", dir, name, ndiffs);
	if (memcmp(rec.count_bytes, count, sizeof count) != 0)
		test_fail(TEST, "%s/%s: total item count is %02X%02X in C, %02X%02X in the golden", dir, name,
			  rec.count_bytes[0], rec.count_bytes[1], count[0], count[1]);
}

static int check_golden_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *entry;
	int n = 0;

	if (d == NULL)
		test_fail(TEST, "cannot open %s/", dir);
	while ((entry = readdir(d)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		check_golden_case(dir, entry->d_name);
		n++;
	}
	closedir(d);
	return n;
}

static void check_goldens(void)
{
	int n = check_golden_dir("cases") + check_golden_dir("combos");

	printf(TEST ": %d goldens' initial SRAM tables and total item counts matched\n", n);
}

int main(int argc, char **argv)
{
	RandoOptions options;
	int fixtures = test_fixtures_mode(TEST, argc, argv);
	rando_options_init(&options);
	CHECK(rando_generate(&gen, &options, 1, 0, NULL, NULL) == RANDO_COMPLETE);
	test_shape();
	test_baseline();
	test_states();
	test_goals();
	test_boots();
	test_swords();
	test_hearts();
	test_capacities();
	test_swordless();
	test_timer();
	test_ganons_tower();
	test_total_item_count();
	test_refusal();
	test_const();
	printf(TEST ": passed\n");
	if (fixtures)
		check_goldens();
	return 0;
}
