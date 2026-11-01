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
 * small_alt with `.` as 52, the large `!` as E5 and F8, the small `-` as 36. */
static void check_defaults(void)
{
	reset();
	gen.post.credits.smithy_breadsmiths = false;
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

/* randomizeCredits always sets the picked scenes, so their default bytes are never written. */
static bool default_untouched(int scene)
{
	return scene != CASTLE && scene != SANCTUARY && scene != KAKARIKO && scene != LUMBERJACKS &&
	       scene != BRIDGE && scene != WOODS && scene != WELL;
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

/* credit_tables.txt: `default <scene> <hex>`, `pick <scene> <index> <hex>`; the item records are
 * chunk 3's. Every pick's index is replayed, and the counts match RANDO_CREDITS_*. */
static void check_credit_tables_fixture(void)
{
	static uint8_t seen[SCENES][64];
	Fixture fx;
	int defaults = 0, picks = 0, scene, nwords;
	char *word[WORDS_MAX];

	fixture_open(&fx, FIXTURE_CREDIT_TABLES);
	while (fixture_next(&fx)) {
		if (fx.nfields != 1)
			fixture_fail(&fx, "a tab in a space-separated record");
		if (strncmp(fx.field[0], "item ", 5) == 0)
			continue;
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
		} else {
			fixture_fail(&fx, "unknown record");
		}
	}
	fixture_close(&fx);
	for (scene = 0; scene < SCENES; scene++) {
		unsigned count = 0, i;

		if (scene == SMITHY)
			count = 1;
		else if (!default_untouched(scene))
			pick_field(NULL, scene, &count);
		for (i = 0; i < count; i++)
			if (!seen[scene][i])
				test_fail(TEST, "no pick record for %s %u", scene_names[scene], i);
	}
	printf(TEST ": %d default and %d pick credit_tables records matched\n", defaults, picks);
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
	check_refusal();
	check_const();
	printf(TEST ": passed\n");
	if (fixtures)
		check_credit_tables_fixture();
	return 0;
}
