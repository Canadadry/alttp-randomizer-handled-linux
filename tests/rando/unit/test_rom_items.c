#include "rando.h"
#include "rando_rom.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_rom_items"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define WRITES_MAX 1024

typedef struct Write {
	uint32_t address;
	uint8_t byte;
	size_t count;
	const char *method;
} Write;

typedef struct Recorder {
	Write writes[WRITES_MAX];
	int n;
	/* The call that returns false, counted from 1; 0 accepts every call. */
	int refuse_at;
	int calls;
} Recorder;

static bool record(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	Recorder *r = ctx;

	r->calls++;
	if (r->n < WRITES_MAX) {
		r->writes[r->n].address = address;
		r->writes[r->n].byte = bytes[0];
		r->writes[r->n].count = count;
		r->writes[r->n].method = method;
		r->n++;
	}
	return r->refuse_at == 0 || r->calls < r->refuse_at;
}

static RandoGen gen, before;
static Recorder rec;

static const RandoItemId pendants[3] = {RANDO_ITEM_PendantOfCourage, RANDO_ITEM_PendantOfWisdom,
					RANDO_ITEM_PendantOfPower};
static const RandoItemId crystals[7] = {RANDO_ITEM_Crystal1, RANDO_ITEM_Crystal2, RANDO_ITEM_Crystal3,
					RANDO_ITEM_Crystal4, RANDO_ITEM_Crystal5, RANDO_ITEM_Crystal6,
					RANDO_ITEM_Crystal7};
static const RandoLocationId pendant_prizes[3] = {RANDO_LOC_EASTERN_PALACE_PRIZE, RANDO_LOC_DESERT_PALACE_PRIZE,
						 RANDO_LOC_TOWER_OF_HERA_PRIZE};
static const RandoLocationId crystal_prizes[7] = {
	RANDO_LOC_PALACE_OF_DARKNESS_PRIZE, RANDO_LOC_SWAMP_PALACE_PRIZE, RANDO_LOC_SKULL_WOODS_PRIZE,
	RANDO_LOC_THIEVES_TOWN_PRIZE,	    RANDO_LOC_ICE_PALACE_PRIZE,	  RANDO_LOC_MISERY_MIRE_PRIZE,
	RANDO_LOC_TURTLE_ROCK_PRIZE,
};

/* An Open world with only its prizes placed (VT's writeItem refuses an empty prize): pendants on the
 * light world dungeons, crystals on the dark world ones, in order. */
static void build(RandoGen *g, void (*tweak)(RandoConfig *))
{
	RandoConfig config;
	int i;

	memset(g, 0, sizeof *g);
	rando_config_init(&config);
	if (tweak)
		tweak(&config);
	rando_world_init(&g->world, RANDO_VARIANT_OPEN, &config);
	rando_postfill_clear(&g->post);
	for (i = 0; i < 3; i++)
		CHECK(rando_world_set_item(&g->world, pendant_prizes[i], pendants[i]));
	for (i = 0; i < 7; i++)
		CHECK(rando_world_set_item(&g->world, crystal_prizes[i], crystals[i]));
}

static void run(void)
{
	RandoRomWriter writer = {record, &rec};

	memset(&rec, 0, sizeof rec);
	CHECK(rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
}

/* The index of the only write at address, or -1 when there is none. */
static int find(uint32_t address)
{
	int i, at = -1;

	for (i = 0; i < rec.n; i++)
		if (rec.writes[i].address == address) {
			CHECK(at < 0);
			at = i;
		}
	return at;
}

static uint8_t byte_at(uint32_t address)
{
	int i = find(address);

	CHECK(i >= 0);
	return i >= 0 ? rec.writes[i].byte : 0;
}

/* Every write is one byte tagged writeItem. */
static void check_tags(void)
{
	int i;

	for (i = 0; i < rec.n; i++)
		CHECK(rec.writes[i].count == 1 && strcmp(rec.writes[i].method, "writeItem") == 0);
}

static void check_chest(void)
{
	build(&gen, NULL);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_SAHASRAHLAS_HUT_LEFT, RANDO_ITEM_Hammer));
	run();
	check_tags();
	CHECK(byte_at(0xEA82) == 0x09);
}

/* Regions in RandoRegionId order; within one, the empty locations after every filled one, as Nothing. */
static void check_walk_order(void)
{
	int left, middle, right, sanctuary;

	build(&gen, NULL);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_SAHASRAHLAS_HUT_MIDDLE, RANDO_ITEM_Hammer));
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_SAHASRAHLAS_HUT_RIGHT, RANDO_ITEM_Lamp));
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_SANCTUARY, RANDO_ITEM_FireRod));
	run();
	left = find(0xEA82);
	middle = find(0xEA85);
	right = find(0xEA88);
	sanctuary = find(0xEA79);
	CHECK(middle >= 0 && right == middle + 1);
	CHECK(left > right && left < sanctuary);
	CHECK(rec.writes[left].byte == 0x5A);
	CHECK(rec.writes[sanctuary].byte == 0x07);
}

static void check_alias(void)
{
	build(&gen, NULL);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_LINKS_UNCLE, RANDO_ITEM_UncleSword));
	run();
	CHECK(byte_at(0x2DF45) == 0x5E);
}

/* Agahnim's event has no address: Hera's last write (its empty boss drop) comes right before Castle
 * Tower's first chest. Agahnim 2's event writes only Ganon's Tower's music. */
static void check_events(void)
{
	build(&gen, NULL);
	run();
	CHECK(find(0x180152) >= 0 && find(0xEAB5) == find(0x180152) + 1);
	CHECK(byte_at(0x155C9) == 0x16);
}

static void check_bytes_at(int at, const uint32_t *addresses, const uint8_t *bytes, int n)
{
	int i;

	CHECK(at >= 0 && at + n <= rec.n);
	for (i = 0; i < n && at >= 0 && at + i < rec.n; i++)
		CHECK(rec.writes[at + i].address == addresses[i] && rec.writes[at + i].byte == bytes[i]);
}

/* Keys 1 to 6 of the prize, then the region's music: 0x11 after a pendant, 0x16 after a crystal. */
static void check_prizes(void)
{
	static const uint32_t desert[10] = {0x1209E, 0x53E7A, 0x53E7B, 0x180053, 0x180072, 0xC6FF,
					    0x1559B, 0x1559C, 0x1559D, 0x1559E};
	static const uint8_t wisdom[10] = {0x01, 0x32, 0x60, 0x00, 0x69, 0x38, 0x11, 0x11, 0x11, 0x11};
	static const uint32_t darkness[7] = {0x120A1, 0x53E7C, 0x53E7D, 0x180056, 0x180073, 0xC702, 0x155B8};
	static const uint8_t crystal1[7] = {0x02, 0x34, 0x64, 0x40, 0x7F, 0x20, 0x16};

	build(&gen, NULL);
	run();
	check_tags();
	check_bytes_at(find(0x1209E), desert, wisdom, 10);
	check_bytes_at(find(0x120A1), darkness, crystal1, 7);
}

static void check_medallions(void)
{
	static const uint32_t turtle[4] = {0x180023, 0x5020, 0x50FF, 0x51DE};
	static const uint8_t quake[4] = {0x02, 0x14, 0xEF, 0xC4};
	static const uint32_t mire[4] = {0x180022, 0x4FF2, 0x50D1, 0x51B0};
	static const uint8_t ether[4] = {0x01, 0x13, 0x9F, 0xF1};

	build(&gen, NULL);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_TURTLE_ROCK_MEDALLION, RANDO_ITEM_Quake));
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_MISERY_MIRE_MEDALLION, RANDO_ITEM_Ether));
	run();
	check_bytes_at(find(0x180023), turtle, quake, 4);
	check_bytes_at(find(0x180022), mire, ether, 4);
}

static void swords_out_of_pool(RandoConfig *config)
{
	config->region_swords_in_pool = false;
}

static void check_blacksmith(void)
{
	build(&gen, NULL);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_BLACKSMITH, RANDO_ITEM_Hammer));
	run();
	CHECK(byte_at(0x18002A) == 0x09);
	CHECK(find(0x3355C) < 0);

	build(&gen, swords_out_of_pool);
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_BLACKSMITH, RANDO_ITEM_Hammer));
	run();
	CHECK(byte_at(0x3355C) == 0x09);
	CHECK(find(0x18002A) < 0);
}

/* Handed over by step 5: nothing is written after a refused write. */
static void check_refusal(void)
{
	RandoRomWriter writer = {record, &rec};

	build(&gen, NULL);
	memset(&rec, 0, sizeof rec);
	rec.refuse_at = 3;
	CHECK(!rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.calls == 3);
}

static void check_gen_unchanged(void)
{
	build(&gen, NULL);
	CHECK(!rando_world_has_item(&gen.world, RANDO_LOC_SAHASRAHLAS_HUT_LEFT, RANDO_ITEM_NONE));
	memcpy(&before, &gen, sizeof gen);
	run();
	CHECK(memcmp(&before, &gen, sizeof gen) == 0);
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	check_chest();
	check_walk_order();
	check_alias();
	check_events();
	check_prizes();
	check_medallions();
	check_blacksmith();
	check_refusal();
	check_gen_unchanged();
	printf(TEST ": passed\n");
	return 0;
}
