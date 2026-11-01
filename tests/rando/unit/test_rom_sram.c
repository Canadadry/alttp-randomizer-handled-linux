#include "rando.h"
#include "rando_rom.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_rom_sram"
#define SRAM_ADDRESS 0x183000u
#define SRAM_SIZE 0x500
#define ROM_SIZE (2u * 1024 * 1024)

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
	uint32_t address;
	size_t count;
	uint8_t bytes[SRAM_SIZE];
	bool refuse_sram;
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

static void test_refusal(void)
{
	RandoRomWriter writer = {record, &rec};

	memset(&rec, 0, sizeof rec);
	rec.refuse_sram = true;
	CHECK(!rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.sram_calls == 1 && rec.calls_after_refusal == 0);
}

static void test_const(void)
{
	memcpy(&before, &gen, sizeof gen);
	run(&gen);
	CHECK(memcmp(&before, &gen, sizeof gen) == 0);
}

int main(int argc, char **argv)
{
	RandoOptions options;

	test_fixtures_mode(TEST, argc, argv);
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
	test_refusal();
	test_const();
	printf(TEST ": passed\n");
	return 0;
}
