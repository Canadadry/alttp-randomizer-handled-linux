#include "rando.h"
#include "rando_rom.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_rom_credits"
#define FIXTURE_CREDIT_TABLES "oracle/credit_tables.txt"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define DATA_ADDRESS 0x181500u
#define POINTERS_ADDRESS 0x076CC0u
#define SCENES 16
#define POINTERS (SCENES + 1)
#define DATA_MAX 4096
#define CREDITS_MAX 4

typedef struct Write {
	uint32_t address;
	size_t count;
	uint8_t bytes[DATA_MAX];
} Write;

typedef struct Recorder {
	Write writes[CREDITS_MAX];
	int n;
	/* Calls before the first writeCredits one, and after the last one. */
	int before;
	int after;
	bool settings_after;
	/* The writeCredits call that returns false, counted from 1; 0 accepts every call. */
	int refuse_at;
	int calls_after_refusal;
	bool refused;
} Recorder;

/* VT's Credits scene order. */
enum {
	CASTLE,
	SANCTUARY,
	KAKARIKO,
	DESERT,
	HERA,
	HOUSE,
	ZORA,
	WITCH,
	LUMBERJACKS,
	GROVE,
	WELL,
	SMITHY,
	KAKARIKO2,
	BRIDGE,
	WOODS,
	PEDESTAL,
};

static const char *const scene_names[SCENES] = {
	"castle", "sanctuary", "kakariko", "desert",	"hera",	     "house",  "zora",	"witch",
	"lumberjacks", "grove",	   "well",     "smithy", "kakariko2", "bridge", "woods", "pedestal",
};

static bool record(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	Recorder *r = ctx;

	if (r->refused) {
		r->calls_after_refusal++;
		return true;
	}
	if (strcmp(method, "writeCredits") != 0) {
		if (r->n == 0)
			r->before++;
		else {
			r->after++;
			if (strcmp(method, "writeItem") != 0 && strcmp(method, "writeText") != 0 &&
			    strcmp(method, "writeInitialSram") != 0 && strcmp(method, "setTotalItemCount") != 0 &&
			    strcmp(method, "setHeartBeepSpeed") != 0)
				r->settings_after = true;
		}
		return true;
	}
	if (r->n < CREDITS_MAX) {
		r->writes[r->n].address = address;
		r->writes[r->n].count = count;
		if (count <= DATA_MAX)
			memcpy(r->writes[r->n].bytes, bytes, count);
	}
	r->n++;
	if (r->refuse_at == r->n) {
		r->refused = true;
		return false;
	}
	return true;
}

static RandoGen baseline, gen, before;
static Recorder rec;

static void run_refusing(int refuse_at, bool expected)
{
	RandoRomWriter writer = {record, &rec};

	memset(&rec, 0, sizeof rec);
	rec.refuse_at = refuse_at;
	CHECK(rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer) == expected);
}

static void run(void)
{
	run_refusing(0, true);
	CHECK(rec.n == 2);
	CHECK(rec.n < 1 || rec.writes[0].address == DATA_ADDRESS);
	CHECK(rec.n < 2 || (rec.writes[1].address == POINTERS_ADDRESS && rec.writes[1].count == 2 * POINTERS));
}

static unsigned pointer(int i)
{
	return rec.writes[1].bytes[2 * i] | (unsigned)rec.writes[1].bytes[2 * i + 1] << 8;
}

static int hex_digit(char c)
{
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* Parses hex into out; returns its byte count, or -1 when it is not an even run of hex digits. */
static int parse_hex(const char *hex, uint8_t *out, size_t max)
{
	size_t len = strlen(hex), i;

	if (len % 2 != 0 || len / 2 > max)
		return -1;
	for (i = 0; i < len / 2; i++) {
		int hi = hex_digit(hex[2 * i]), lo = hex_digit(hex[2 * i + 1]);

		if (hi < 0 || lo < 0)
			return -1;
		out[i] = (uint8_t)(hi << 4 | lo);
	}
	return (int)(len / 2);
}

/* Whether scene's segment of the last run's data write is exactly hex. */
static bool segment_is(int scene, const char *hex)
{
	static uint8_t expected[DATA_MAX];
	int n = parse_hex(hex, expected, sizeof expected);
	unsigned from = pointer(scene), to = pointer(scene + 1);

	return n >= 0 && to >= from && to - from == (unsigned)n && to <= rec.writes[0].count &&
	       memcmp(rec.writes[0].bytes + from, expected, (size_t)n) == 0;
}

#define CHECK_SEGMENT(scene, hex) \
	do { \
		if (!segment_is(scene, hex)) \
			test_fail(TEST, "%s:%d: scene %s is not %s", __FILE__, __LINE__, scene_names[scene], hex); \
	} while (0)

static void reset(void)
{
	memcpy(&gen, &baseline, sizeof gen);
}

static void check_shape(void)
{
	int i;

	reset();
	run();
	CHECK(rec.before > 0);
	CHECK(!rec.settings_after);
	CHECK(pointer(0) == 0);
	for (i = 0; i < SCENES; i++)
		CHECK(pointer(i) <= pointer(i + 1));
	CHECK(pointer(SCENES) == rec.writes[0].count);
}

/* Every scene but except has the bytes of the saved run. */
static void check_only_changed(const Write *saved_data, const Write *saved_pointers, int except)
{
	int i;

	for (i = 0; i < SCENES; i++) {
		unsigned from, to, was_from, was_to;

		if (i == except)
			continue;
		from = pointer(i);
		to = pointer(i + 1);
		was_from = saved_pointers->bytes[2 * i] | (unsigned)saved_pointers->bytes[2 * i + 1] << 8;
		was_to = saved_pointers->bytes[2 * i + 2] | (unsigned)saved_pointers->bytes[2 * i + 3] << 8;
		CHECK(to - from == was_to - was_from);
		CHECK(to - from != was_to - was_from ||
		      memcmp(rec.writes[0].bytes + from, saved_data->bytes + was_from, to - from) == 0);
	}
}

/* "the two towers", 14 bytes: x (32 - 14) / 2 = 9 at y 19, so the header 62 69 00 1B. */
static void check_header_and_centring(void)
{
	reset();
	gen.post.credits.castle = 2;
	run();
	CHECK_SEGMENT(CASTLE, "6269001b2d211e9f2d30289f2d28301e2b2c"
			      "62e9001964756e7168619f5f5d6f706861630900198a9b94978e879f858395968e87");
	/* "sits in own pew", 15 bytes: x rounds down to 8. */
	gen.post.credits.sanctuary = 2;
	run();
	CHECK_SEGMENT(SANCTUARY,
		      "6268001d2c222d2c9f22279f2830279f291e30" "62eb00116f5d6a5f70715d6e75630b001195839085969783949b");
	/* "Your ad here": centred on its 12 bytes, x 10, encoded lower-case. */
	gen.post.credits.bridge = 2;
	run();
	CHECK_SEGMENT(BRIDGE, "626a001732282e2b9f1a1d9f211e2b1e"
			      "62e9001b60615d70649f696b716a705d656a6309001b868783968a9f8f91979096838b90");
}

/* The name's apostrophe is drawn as a `,` record (34) on y 18, at x + its index (4 + 10 = 14). */
static void check_apostrophe_record(void)
{
	reset();
	gen.post.credits.kakariko = 0;
	run();
	CHECK_SEGMENT(KAKARIKO, "6264002d2c1a211a2c2b1a251a21352c9f2128261e1c2826222720"
				"624e000134"
				"62e90019675d675d6e65676b9f706b736a630900198d838d83948b8d919f96919990");
}

static void check_comma_records(void)
{
	reset();
	/* "Venus, I'm your fire": its apostrophe at x 6 + 8 on y 18, its comma as `'` (35) at x 6 + 5 on y 20. */
	gen.post.credits.well = 4;
	run();
	CHECK_SEGMENT(WELL, "626600272f1e272e2c349f2235269f32282e2b9f1f222b1e"
			    "624e000134"
			    "628b000135"
			    "62ea001773656f64656a639f73616868630a0017998b958a8b90899f99878e8e");
	/* "Yeah, baby, she's got it": the comma record runs from the first comma to the last, spaces as 9F. */
	gen.post.credits.well = 3;
	run();
	CHECK_SEGMENT(WELL, "6264002f321e1a21349f1b1a1b32349f2c211e352c9f20282d9f222d"
			    "6253000134"
			    "6288000d359f9f9f9f9f35"
			    "62ea001773656f64656a639f73616868630a0017998b958a8b90899f99878e8e");
}

/* Scenes no rule touches keep VT's default record and x: the large titles' two lines, pedestal's
 * small_alt with `.` as 52, the large `!` as E5 and F8, the small `-` as 36. The pedestal and the
 * kid hold their vanilla items, whose texts are the defaults. */
static void check_defaults(void)
{
	reset();
	gen.post.credits.smithy_breadsmiths = false;
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_L2Sword));
	CHECK(rando_world_set_item(&gen.world, RANDO_LOC_SICK_KID, RANDO_ITEM_BugCatchingNet));
	run();
	CHECK_SEGMENT(DESERT, "6264002f2f2e252d2e2b1e2c9f2b2e251e9f2d211e9f1d1e2c1e2b2d"
			      "62e9001960616f616e709f6c5d685d5f61630900198687958794969f92838e838587");
	CHECK_SEGMENT(PEDESTAL, "626600271a271d9f2d211e9f261a2c2d1e2b9f2c30282b1d"
				"62a8001d4a433c3c474a9f383e38404552525262"
				"ec000f626b6e6172616ee5630c000f88919487988794f8");
	CHECK_SEGMENT(KAKARIKO2, "626600272d211e9f1b2e20361c1a2d1c212227209f24221d"
				 "62e90019675d675d6e65676b9f706b736a630900198d838d83948b8d919f96919990");
	CHECK_SEGMENT(SMITHY,
		      "6264002d2d211e9f1d301a2b2f1e279f2c30282b1d2c26222d212c" "62ec000f6f69657064616e75630c000f958f8b968a87949b");
}

typedef struct Pick {
	int scene;
	uint8_t *field;
	uint8_t index;
	const char *hex;
} Pick;

/* Index 0 and the last index of each pick. */
static void check_picks(void)
{
	RandoCredits *c = &gen.post.credits;
	const Pick picks[] = {
		{CASTLE, &c->castle, 0,
		 "6265002b2d211e9f2b1e2d2e2b279f281f9f2d211e9f24222720"
		 "62e9001964756e7168619f5f5d6f706861630900198a9b94978e879f858395968e87"},
		{CASTLE, &c->castle, RANDO_CREDITS_CASTLE - 1,
		 "6269001b2d211e9f2d30289f2d28301e2b2c"
		 "62e9001964756e7168619f5f5d6f706861630900198a9b94978e879f858395968e87"},
		{SANCTUARY, &c->sanctuary, 0,
		 "6268001f2d211e9f2528321a259f292b221e2c2d62eb00116f5d6a5f70715d6e75630b001195839085969783949b"},
		{SANCTUARY, &c->sanctuary, RANDO_CREDITS_SANCTUARY - 1,
		 "626c000f211e1a259f292533" "62eb00116f5d6a5f70715d6e75630b001195839085969783949b"},
		{KAKARIKO, &c->kakariko, 0,
		 "6264002d2c1a211a2c2b1a251a21352c9f2128261e1c2826222720624e000134"
		 "62e90019675d675d6e65676b9f706b736a630900198d838d83948b8d919f96919990"},
		{KAKARIKO, &c->kakariko, RANDO_CREDITS_KAKARIKO - 1,
		 "6264002d2c1a2c211a2b1a25251a352c9f2128261e1c2826222720624e000134"
		 "62e90019675d675d6e65676b9f706b736a630900198d838d83948b8d919f96919990"},
		{LUMBERJACKS, &c->lumberjacks, 0,
		 "6268001f2d3022279f252e261b1e2b231a1c242c"
		 "62e9001b736b6b606f69616ad96f9f6471706309001b99919186958f8790ec959f8a9796"},
		{LUMBERJACKS, &c->lumberjacks, RANDO_CREDITS_LUMBERJACKS - 1,
		 "626c000f1d282c9f1a311e2c"
		 "62e9001b736b6b606f69616ad96f9f6471706309001b99919186958f8790ec959f8a9796"},
		{BRIDGE, &c->bridge, 0,
		 "6268001f2d211e9f25282c2d9f28251d9f261a27"
		 "62e9001b60615d70649f696b716a705d656a6309001b868783968a9f8f91979096838b90"},
		{BRIDGE, &c->bridge, RANDO_CREDITS_BRIDGE - 1,
		 "626a001732282e2b9f1a1d9f211e2b1e"
		 "62e9001b60615d70649f696b716a705d656a6309001b868783968a9f8f91979096838b90"},
		{WOODS, &c->woods, 0,
		 "6268001f2d211e9f1f282b1e2c2d9f2d21221e1f" "62eb0013686b6f709f736b6b606f630b00138e9195969f9991918695"},
		{WOODS, &c->woods, RANDO_CREDITS_WOODS - 1,
		 "6268001d1f25322227209f2f2e252d2e2b1e2c" "62eb0013686b6f709f736b6b606f630b00138e9195969f9991918695"},
		{WELL, &c->well, 0,
		 "6264002d2f1e272e2c379f2a2e1e1e279f281f9f1f1a1e2b221e2c"
		 "62ea001773656f64656a639f73616868630a0017998b958a8b90899f99878e8e"},
		{WELL, &c->well, RANDO_CREDITS_WELL - 1,
		 "626600252f1e272e2c9f1c2b1e2c1c1e272d9f1b1e1a26"
		 "62ea001773656f64656a639f73616868630a0017998b958a8b90899f99878e8e"},
	};
	size_t i;

	for (i = 0; i < sizeof picks / sizeof picks[0]; i++) {
		reset();
		*picks[i].field = picks[i].index;
		run();
		if (!segment_is(picks[i].scene, picks[i].hex))
			test_fail(TEST, "pick %s %u is not %s", scene_names[picks[i].scene], picks[i].index,
				  picks[i].hex);
	}
}

/* The coin changes the smithy segment only. */
static void check_coin(void)
{
	static Write data, pointers;

	reset();
	gen.post.credits.smithy_breadsmiths = false;
	run();
	data = rec.writes[0];
	pointers = rec.writes[1];
	gen.post.credits.smithy_breadsmiths = true;
	run();
	CHECK_SEGMENT(SMITHY, "6264002d2d211e9f1d301a2b2f1e279f1b2b1e1a1d2c26222d212c"
			      "62ec000f6f69657064616e75630c000f958f8b968a87949b");
	check_only_changed(&data, &pointers, SMITHY);
}

/* Places item at location (asserting setItem accepts it), then writes. */
static void run_with(RandoLocationId location, RandoItemId item)
{
	CHECK(rando_world_set_item(&gen.world, location, item));
	run();
}

/* Witch's raw-name case 'Hammer': "stop...   hammer time", 21 bytes, x 5. */
static void check_override_item_entry(void)
{
	reset();
	run_with(RANDO_LOC_POTION_SHOP, RANDO_ITEM_Hammer);
	CHECK_SEGMENT(WITCH, "626500292c2d28293737379f9f9f211a26261e2b9f2d22261e"
			     "62eb0013695d63655f9f6f646b6c630b00138f83898b859f958a9192");
}

#define ZORA_LARGE "62e8001f766b6e5dd96f9f735d70616e625d68686308001f9c919483ec959f998396879488838e8e"
#define PEDESTAL_REST \
	"62a8001d4a433c3c474a9f383e38404552525262ec000f626b6e6172616ee5630c000f88919487988794f8"
#define HOUSE_LARGE "62eb0013756b716e9f646b716f61630b00139b9197949f8a91979587"
#define GROVE_LARGE "62e90019645d716a7061609f636e6b7261630900198a8397909687869f8994919887"

/* An empty King Zora reads as Nothing: "Nothing, so stupid", 18 bytes at x 7, lower-cased, with
 * its comma record. */
static void check_override_empty(void)
{
	const char *nothing = "6267002327282d21222720349f2c289f2c2d2e29221d628e000135" ZORA_LARGE;

	reset();
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_NONE);
	CHECK_SEGMENT(ZORA, nothing);
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_Nothing);
	CHECK_SEGMENT(ZORA, nothing);
}

/* The class switch comes before the raw names: Key and BigKey share one text, Map and Compass
 * have their own, whatever the dungeon. */
static void check_override_classes(void)
{
	reset();
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_KeyD1);
	CHECK_SEGMENT(ZORA, "626600271a1d2f1a271c1e261e272d9f1f282b9f2c1a251e" ZORA_LARGE);
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_BigKeyD3);
	CHECK_SEGMENT(ZORA, "626600271a1d2f1a271c1e261e272d9f1f282b9f2c1a251e" ZORA_LARGE);
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_MapD1);
	CHECK_SEGMENT(ZORA, "626700232d211e9f30282b251d9f1f282b9f2c1a251e" ZORA_LARGE);
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_CompassH2);
	CHECK_SEGMENT(ZORA, "626700211b1e1a2b2227202c9f1f282b9f2c1a251e" ZORA_LARGE);
	/* The witch's "keys, keys, keys": one comma record from the first comma to the last. */
	run_with(RANDO_LOC_POTION_SHOP, RANDO_ITEM_Key);
	CHECK_SEGMENT(WITCH, "6268001f241e322c349f241e322c349f241e322c628c000d359f9f9f9f9f35"
			     "62eb0013695d63655f9f6f646b6c630b00138f83898b859f958a9192");
	/* The sick kid's key text, from a scene other than Zora's. */
	run_with(RANDO_LOC_SICK_KID, RANDO_ITEM_BigKeyP1);
	CHECK_SEGMENT(KAKARIKO2, "626600252d211e9f241e32362128251d2227209f24221d"
				 "62e90019675d675d6e65676b9f706b736a630900198d838d83948b8d919f96919990");
}

/* An alias reads as its target: UncleSword as ProgressiveSword ("your uncle recovers", where
 * UncleSword itself would fall to "your uncle does nothing"), ShopKey as a Key. */
static void check_override_alias(void)
{
	reset();
	run_with(RANDO_LOC_LINKS_UNCLE, RANDO_ITEM_UncleSword);
	CHECK_SEGMENT(HOUSE, "6266002532282e2b9f2e271c251e9f2b1e1c282f1e2b2c" HOUSE_LARGE);
	run_with(RANDO_LOC_LINKS_UNCLE, RANDO_ITEM_ShopKey);
	CHECK_SEGMENT(HOUSE, "6265002b32282e2b9f2e271c251e9f29221c242c9f25281c242c" HOUSE_LARGE);
}

/* Pedestal's raw-name check for BigKeyA2 comes before its class switch; MasterSword is stored as
 * L2Sword. */
static void check_override_pedestal(void)
{
	reset();
	run_with(RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_BigKeyA2);
	CHECK_SEGMENT(PEDESTAL, "62660027251a9f241e329f281f9f1e2f22252c9f1b1a271e" PEDESTAL_REST);
	run_with(RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_BigKeyA1);
	CHECK_SEGMENT(PEDESTAL, "626a00151a271d9f2d211e9f241e32" PEDESTAL_REST);
	run_with(RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword);
	CHECK_SEGMENT(PEDESTAL, "626600271a271d9f2d211e9f261a2c2d1e2b9f2c30282b1d" PEDESTAL_REST);
	/* Zora has no BigKeyA2 case: the class text. */
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_BigKeyA2);
	CHECK_SEGMENT(ZORA, "626600271a1d2f1a271c1e261e272d9f1f282b9f2c1a251e" ZORA_LARGE);
}

/* The grove's texts: "body-building boy has gold again" fills 32 bytes at x 0; "'splosion boy
 * explodes again" (28, x 2) has its apostrophe record at x 2 + 0, y 18. */
static void check_override_grove(void)
{
	reset();
	run_with(RANDO_LOC_FLUTE_SPOT, RANDO_ITEM_TitansMitt);
	CHECK_SEGMENT(GROVE, "6260003f1b281d32361b2e22251d2227209f1b28329f211a2c9f2028251d9f1a201a2227" GROVE_LARGE);
	run_with(RANDO_LOC_FLUTE_SPOT, RANDO_ITEM_Bomb);
	CHECK_SEGMENT(GROVE, "62620037352c2925282c2228279f1b28329f1e312925281d1e2c9f1a201a2227"
			     "6242000134" GROVE_LARGE);
}

/* Under rom.genericKeys, writeItem writes KeyGK, but the credits read the location's own key. */
static void check_override_generic_keys(void)
{
	reset();
	gen.world.config.rom_generic_keys = true;
	run_with(RANDO_LOC_KING_ZORA, RANDO_ITEM_KeyD1);
	CHECK_SEGMENT(ZORA, "626600271a1d2f1a271c1e261e272d9f1f282b9f2c1a251e" ZORA_LARGE);
}

/* Changing one override location changes only its scene's segment. */
static void check_override_isolation(void)
{
	static Write data, pointers;

	reset();
	run_with(RANDO_LOC_FLUTE_SPOT, RANDO_ITEM_Nothing);
	data = rec.writes[0];
	pointers = rec.writes[1];
	run_with(RANDO_LOC_FLUTE_SPOT, RANDO_ITEM_TitansMitt);
	check_only_changed(&data, &pointers, GROVE);
}

static void check_refusal(void)
{
	reset();
	run_refusing(1, false);
	CHECK(rec.n == 1 && rec.calls_after_refusal == 0);
	run_refusing(2, false);
	CHECK(rec.n == 2 && rec.calls_after_refusal == 0);
}

static void check_const(void)
{
	reset();
	memcpy(&before, &gen, sizeof gen);
	run();
	CHECK(memcmp(&before, &gen, sizeof gen) == 0);
}

static int scene_of(const Fixture *fx, const char *name)
{
	int i;

	for (i = 0; i < SCENES; i++)
		if (strcmp(scene_names[i], name) == 0)
			return i;
	fixture_fail(fx, "unknown scene");
	return -1;
}

static uint8_t *pick_field(const Fixture *fx, int scene, unsigned *count)
{
	RandoCredits *c = &gen.post.credits;

	switch (scene) {
	case CASTLE:
		*count = RANDO_CREDITS_CASTLE;
		return &c->castle;
	case SANCTUARY:
		*count = RANDO_CREDITS_SANCTUARY;
		return &c->sanctuary;
	case KAKARIKO:
		*count = RANDO_CREDITS_KAKARIKO;
		return &c->kakariko;
	case LUMBERJACKS:
		*count = RANDO_CREDITS_LUMBERJACKS;
		return &c->lumberjacks;
	case BRIDGE:
		*count = RANDO_CREDITS_BRIDGE;
		return &c->bridge;
	case WOODS:
		*count = RANDO_CREDITS_WOODS;
		return &c->woods;
	case WELL:
		*count = RANDO_CREDITS_WELL;
		return &c->well;
	default:
		fixture_fail(fx, "no pick for this scene");
		return NULL;
	}
}

/* randomizeCredits sets the picked scenes and the item loop the override scenes, so only these
 * keep their default bytes. */
static bool default_untouched(int scene)
{
	return scene == DESERT || scene == HERA || scene == SMITHY;
}

static bool is_pick(int scene)
{
	return scene == CASTLE || scene == SANCTUARY || scene == KAKARIKO || scene == LUMBERJACKS ||
	       scene == BRIDGE || scene == WOODS || scene == WELL;
}

/* The one location of each override scene's VT class, or RANDO_LOCATION_NONE. */
static RandoLocationId override_location(int scene)
{
	switch (scene) {
	case ZORA:
		return RANDO_LOC_KING_ZORA;
	case WITCH:
		return RANDO_LOC_POTION_SHOP;
	case PEDESTAL:
		return RANDO_LOC_MASTER_SWORD_PEDESTAL;
	case KAKARIKO2:
		return RANDO_LOC_SICK_KID;
	case GROVE:
		return RANDO_LOC_FLUTE_SPOT;
	case HOUSE:
		return RANDO_LOC_LINKS_UNCLE;
	default:
		return RANDO_LOCATION_NONE;
	}
}

#define WORDS_MAX 4

/* Splits the space-separated record in place; returns its word count, failing beyond WORDS_MAX. */
static int split_words(const Fixture *fx, char *line, char *words[WORDS_MAX])
{
	int n = 0;
	char *p = line;

	for (;;) {
		if (n == WORDS_MAX)
			fixture_fail(fx, "too many words");
		words[n++] = p;
		p = strchr(p, ' ');
		if (!p)
			return n;
		*p++ = '\0';
	}
}

/* credit_tables.txt: `default <scene> <hex>`, `pick <scene> <index> <hex>`, `item <scene> <raw name>
 * <hex>`. Every pick's index is replayed, and the counts match RANDO_CREDITS_*; every C item has an
 * item record in each override scene. */
static void check_credit_tables_fixture(void)
{
	static uint8_t seen[SCENES][64];
	Fixture fx;
	static uint8_t seen_items[SCENES][RANDO_ITEM_COUNT];
	int defaults = 0, picks = 0, items = 0, scene, nwords;
	char *word[WORDS_MAX];

	fixture_open(&fx, FIXTURE_CREDIT_TABLES);
	while (fixture_next(&fx)) {
		if (fx.nfields != 1)
			fixture_fail(&fx, "a tab in a space-separated record");
		nwords = split_words(&fx, fx.field[0], word);
		if (strcmp(word[0], "default") == 0) {
			if (nwords != 3)
				fixture_fail(&fx, "malformed default");
			scene = scene_of(&fx, word[1]);
			if (!default_untouched(scene))
				continue;
			reset();
			gen.post.credits.smithy_breadsmiths = false;
			run();
			if (!segment_is(scene, word[2]))
				fixture_fail(&fx, "default segment differs");
			defaults++;
		} else if (strcmp(word[0], "pick") == 0) {
			unsigned count, index;
			char *end;
			uint8_t *field;

			if (nwords != 4)
				fixture_fail(&fx, "malformed pick");
			scene = scene_of(&fx, word[1]);
			index = (unsigned)strtoul(word[2], &end, 10);
			if (*end != '\0')
				fixture_fail(&fx, "malformed index");
			reset();
			if (scene == SMITHY) {
				if (index != 0)
					fixture_fail(&fx, "smithy has the one index 0");
				gen.post.credits.smithy_breadsmiths = true;
				count = 1;
			} else {
				field = pick_field(&fx, scene, &count);
				if (index >= count)
					fixture_fail(&fx, "index beyond RANDO_CREDITS_*");
				*field = (uint8_t)index;
			}
			seen[scene][index] = 1;
			run();
			if (!segment_is(scene, word[3]))
				fixture_fail(&fx, "pick segment differs");
			picks++;
		} else if (strcmp(word[0], "item") == 0) {
			RandoLocationId location;
			RandoItemId item;

			if (nwords != 4)
				fixture_fail(&fx, "malformed item");
			scene = scene_of(&fx, word[1]);
			location = override_location(scene);
			if (location == RANDO_LOCATION_NONE)
				fixture_fail(&fx, "item record for a scene with no override");
			item = rando_item_find(word[2]);
			if (item == RANDO_ITEM_NONE)
				fixture_fail(&fx, "an item C does not know");
			reset();
			if (!rando_world_set_item(&gen.world, location, item))
				fixture_fail(&fx, "setItem refused an item VT accepted");
			seen_items[scene][item] = 1;
			run();
			if (!segment_is(scene, word[3]))
				fixture_fail(&fx, "item segment differs");
			items++;
		} else {
			fixture_fail(&fx, "unknown record");
		}
	}
	fixture_close(&fx);
	for (scene = 0; scene < SCENES; scene++) {
		unsigned count = 0, i;

		if (scene == SMITHY)
			count = 1;
		else if (is_pick(scene))
			pick_field(NULL, scene, &count);
		for (i = 0; i < count; i++)
			if (!seen[scene][i])
				test_fail(TEST, "no pick record for %s %u", scene_names[scene], i);
		if (override_location(scene) == RANDO_LOCATION_NONE)
			continue;
		for (i = 0; i < RANDO_ITEM_COUNT; i++)
			if (!seen_items[scene][i])
				test_fail(TEST, "no item record for %s %s", scene_names[scene],
					  rando_item_info((RandoItemId)i)->name);
	}
	printf(TEST ": %d default, %d pick and %d item credit_tables records matched\n", defaults, picks, items);
}

int main(int argc, char **argv)
{
	RandoOptions options;

	int fixtures = test_fixtures_mode(TEST, argc, argv);

	rando_options_init(&options);
	CHECK(rando_generate(&baseline, &options, 1, 0, NULL, NULL) == RANDO_COMPLETE);
	check_shape();
	check_header_and_centring();
	check_apostrophe_record();
	check_comma_records();
	check_defaults();
	check_picks();
	check_coin();
	check_override_item_entry();
	check_override_empty();
	check_override_classes();
	check_override_alias();
	check_override_pedestal();
	check_override_grove();
	check_override_generic_keys();
	check_override_isolation();
	check_refusal();
	check_const();
	printf(TEST ": passed\n");
	if (fixtures)
		check_credit_tables_fixture();
	return 0;
}
