#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_endscreen"
#define OUT_MAX 8192
#define TEXT_MAX 8192
/* The planted cats' reference: 4 bytes of the synthetic ROM outside every synthetic table. */
#define REF_AT (3u * SYNTH_ROM_BANK + 0x123u)

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static LadxRomTables rom_tables;
static LadxRandomizer randomizer, copy;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static SynthRom synth;
static char out[OUT_MAX], endscreen_text[TEXT_MAX];
static size_t out_len;
static int checks, records;
static LadxRngRecord last;

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

static void trace(void *ctx, const LadxRngRecord *record)
{
	(void)ctx;
	records++;
	last = *record;
}

static LadxSpan span(const char *s)
{
	LadxSpan sp;

	sp.at = s;
	sp.len = strlen(s);
	return sp;
}

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"
#define HEADER "ladx-rom-writes 1\ntables " TABLES "\n"

static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "C endscreen - patches.endscreen.updateEndScreen(rom)\n";

static const uint8_t seed[16] = {'L', 'A', 'D', 'X', '-', 'e', 'n', 'd', 's', 'c', 'r', 'e', 'e', 'n', '-', 't'};

/* A planted constants file: the fixed unit, then cats units of cat k: k, 0xE0, then 4 ROM bytes. */
static void plant(int cats)
{
	size_t len;
	int k;

	len = (size_t)snprintf(endscreen_text, TEXT_MAX,
			       "ladx-tables 1\ngroup endscreen\ntables " TABLES "\nunit endscreen.1\nE\nB 391d 3e3f\n");
	for (k = 0; k < cats; k++)
		len += (size_t)snprintf(endscreen_text + len, TEXT_MAX - len, "unit endscreen.%d\nE\nB fc400 %02xe0 @%x+4\n",
					k + 2, k, REF_AT);
}

static void open_tables(int with_constants)
{
	LadxTablesFile order, group;
	LadxTablesError err;

	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "endscreen.txt";
	group.text = span(endscreen_text);
	if (ladx_tables_open(&tables, &order, &group, with_constants ? 1 : 0, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static LadxPipelineStatus run(int with_rom, int with_rngs, LadxPipelineError *err)
{
	static LadxSettings settings;
	int count;

	ladx_settings_defaults(&settings);
	ladx_settings_validate(&settings);
	ladx_randomize(&randomizer, &settings, seed, sizeof seed, NULL, NULL);
	ladx_randomize(&copy, &settings, seed, sizeof seed, NULL, NULL);
	randomizer.global.sink = trace;
	records = 0;
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, with_rom ? synth.rom : NULL, with_rom ? SYNTH_ROM_SIZE : 0, with_rom ? &ks : NULL,
			  with_rom ? &rom_tables : NULL, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	memset(&pipeline, 0, sizeof pipeline);
	pipeline.tables = &tables;
	pipeline.settings = &settings;
	pipeline.randomizer = &randomizer;
	pipeline.rnd = with_rngs ? &randomizer.rnd : NULL;
	pipeline.global = with_rngs ? &randomizer.global : NULL;
	pipeline.rom = with_rom ? &rom : NULL;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

static int same_generators(void)
{
	return memcmp(&copy.rnd.py, &randomizer.rnd.py, sizeof copy.rnd.py) == 0 &&
	       memcmp(&copy.global.py, &randomizer.global.py, sizeof copy.global.py) == 0;
}

/* updateEndScreen's draw against CPython 3.13: random.Random(b"LADX-endscreen-test").choice(range(24)) x6. */
static void test_draw_against_cpython(void)
{
	static const size_t want[] = {22, 5, 15, 10, 9, 15};
	LadxRng global;
	size_t i, k;

	ladx_rng_init(&global, LADX_RNG_GLOBAL, trace, NULL);
	ladx_rng_seed(&global, LADX_STAGE_RANDOMIZER_INIT, (const uint8_t *)"LADX-endscreen-test", 19);
	records = 0;
	for (i = 0; i < sizeof want / sizeof want[0]; i++)
		check(ladx_endscreen_draw(&global, &k) == LADX_RNG_OK && k == want[i], "choice(24) as CPython draws it");
	check(records == 6 && last.method == LADX_RNG_CHOICE && last.n == LADX_ENDSCREEN_CAT_COUNT &&
		      last.stage == LADX_STAGE_ENDSCREEN_UPDATE && last.gen == LADX_RNG_GLOBAL,
	      "one choice of 24 per draw, traced at patches.endscreen:updateEndScreen");
	check(strcmp(ladx_rng_stage_name(LADX_STAGE_ENDSCREEN_UPDATE), "patches.endscreen:updateEndScreen") == 0,
	      "the stage's name is LADXR's frame");
	check(ladx_rng_random(&global, LADX_STAGE_RANDOMIZER_INIT) == 0x1.a90c6611e3de0p-2,
	      "as many words drawn as CPython");
}

static void test_writes(void)
{
	char want[OUT_MAX];
	LadxPipelineError err;
	size_t k;

	plant(LADX_ENDSCREEN_CAT_COUNT);
	open_tables(1);
	check(run(1, 1, &err) == LADX_PIPELINE_OK, "the pipeline runs endscreen");
	check(ladx_endscreen_draw(&copy.global, &k) == LADX_RNG_OK, "the copy draws");
	check(same_generators(), "the group drew once from the randomizer's own global generator");
	check(records == 1 && last.stage == LADX_STAGE_ENDSCREEN_UPDATE && last.index == k, "its draw is traced");
	snprintf(want, sizeof want, HEADER "G endscreen\nB 391d 3e3f\nB fc400 %02xe0%02x%02x%02x%02x\n", (unsigned)k,
		 synth.rom[REF_AT], synth.rom[REF_AT + 1], synth.rom[REF_AT + 2], synth.rom[REF_AT + 3]);
	check(strcmp(out, want) == 0,
	      "the fixed unit, then the drawn cat's unit, its reference read from the ROM as computed bytes");
}

static void test_draw_without_rom(void)
{
	LadxPipelineError err;
	size_t k;

	plant(LADX_ENDSCREEN_CAT_COUNT);
	open_tables(1);
	check(run(0, 1, &err) == LADX_PIPELINE_OK && strcmp(out, HEADER) == 0, "without the ROM, nothing is written");
	check(ladx_endscreen_draw(&copy.global, &k) == LADX_RNG_OK && same_generators() && records == 1,
	      "without the ROM, the draw is made all the same");
}

static void test_failures(void)
{
	LadxPipelineError err;

	plant(LADX_ENDSCREEN_CAT_COUNT - 1);
	open_tables(1);
	check(run(1, 1, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "endscreen") == 0,
	      "a constants file of 23 cats is refused");
	plant(LADX_ENDSCREEN_CAT_COUNT + 1);
	open_tables(1);
	check(run(1, 1, &err) == LADX_PIPELINE_GROUP, "a constants file of 25 cats is refused");
	open_tables(0);
	check(run(1, 1, &err) == LADX_PIPELINE_GROUP, "no constants file fails");
	plant(LADX_ENDSCREEN_CAT_COUNT);
	open_tables(1);
	check(run(1, 0, &err) == LADX_PIPELINE_GROUP && run(0, 0, &err) == LADX_PIPELINE_GROUP,
	      "no generators fails, with or without the ROM");
}

int main(void)
{
	synth_rom_build(&synth, 0xE5C00001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	test_draw_against_cpython();
	test_writes();
	test_draw_without_rom();
	test_failures();
	printf("%s: the drawn cat's constants on a synthetic ROM, the draw without it (%d checks)\n", TEST, checks);
	return 0;
}
