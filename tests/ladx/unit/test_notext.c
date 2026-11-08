#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_notext"
#define OUT_MAX 65536
#define TEXTS 0

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static LadxRomTables rom_tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static SynthRom synth;
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

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"
#define HEADER "ladx-rom-writes 1\ntables " TABLES "\n"

/* A planted table group before notext: text 0x10 a literal, text 0x11 an integer pointer. */
static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "table early early.1 patches.early(rom)\n"
				      "C notext - patches.aesthetics.noText(rom)\n";
static const char *const early_text = "ladx-tables 1\n"
				      "group early\n"
				      "tables " TABLES "\n"
				      "unit early.1\n"
				      "E\n"
				      "T texts 10 a1b2c3\n"
				      "T texts 11 p1234\n";
#define EARLY_HEADER HEADER "G early\nT texts 10 a1b2c3\nT texts 11 p1234\n"

static const uint8_t seed[16] = {'L', 'A', 'D', 'X', '-', 'n', 'o', 't', 'e', 'x', 't', '-', 't', 'e', 's', 't'};

static void open_tables(void)
{
	LadxTablesFile order, group;
	LadxTablesError err;

	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "early.txt";
	group.text = span(early_text);
	if (ladx_tables_open(&tables, &order, &group, 1, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static LadxPipelineStatus run(const char *assignment, int with_rom, LadxPipelineError *err)
{
	static LadxSettings settings;
	int count;

	ladx_settings_defaults(&settings);
	if (assignment && ladx_settings_set(&settings, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
	ladx_settings_validate(&settings);
	ladx_randomize(&randomizer, &settings, seed, sizeof seed, NULL, NULL);
	out_len = 0;
	out[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, with_rom ? synth.rom : NULL, with_rom ? SYNTH_ROM_SIZE : 0, with_rom ? &ks : NULL,
			  with_rom ? &rom_tables : NULL, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	pipeline.tables = &tables;
	pipeline.settings = &settings;
	pipeline.spot_count = 0;
	pipeline.randomizer = &randomizer;
	pipeline.rom = with_rom ? &rom : NULL;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	pipeline.ctx = NULL;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

/* noText's log: every text but 0x217..0x21A and the integers cut to its last byte, a source copy. */
static void expected(char *want, size_t cap)
{
	size_t len = (size_t)snprintf(want, cap, EARLY_HEADER "G notext\n");
	int i;

	for (i = 0; i < synth.table_count[TEXTS]; i++) {
		const SynthEntry *e = &synth.entry[synth.table_first[TEXTS] + i];

		if (e->alt || (e->index >= 0x217 && e->index <= 0x21A) || e->index == 0x11 || e->integer)
			continue;
		if (e->index == 0x10)
			len += (size_t)snprintf(want + len, cap - len, "T texts 10 c3\n");
		else if (e->stop > e->start)
			len += (size_t)snprintf(want + len, cap - len, "T texts %x @%x+1\n", e->index, e->stop - 1);
		else
			len += (size_t)snprintf(want + len, cap - len, "T texts %x\n", e->index);
		if (len >= cap)
			test_fail(TEST, "expected log over %u bytes", (unsigned)cap);
	}
}

static void test_cut(void)
{
	static char want[OUT_MAX];
	LadxPipelineError err;

	check(run("textmode=none", 1, &err) == LADX_PIPELINE_OK, "textmode none: the pipeline runs notext");
	expected(want, sizeof want);
	check(strcmp(out, want) == 0, "textmode none: every text cut to its last byte, its origin carried");
}

static void test_other_modes(void)
{
	static const char *const modes[] = {NULL, "textmode=fast", "textmode=default", "textmode=notextbox"};
	LadxPipelineError err;
	size_t i;

	for (i = 0; i < sizeof modes / sizeof modes[0]; i++)
		check(run(modes[i], 1, &err) == LADX_PIPELINE_OK && strcmp(out, EARLY_HEADER) == 0,
		      "another textmode: notext writes nothing");
}

static void test_without_rom(void)
{
	LadxPipelineError err;

	check(run("textmode=none", 0, &err) == LADX_PIPELINE_OK && strcmp(out, EARLY_HEADER) == 0,
	      "without the ROM, notext is skipped");
}

int main(void)
{
	synth_rom_build(&synth, 0x7E470001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	open_tables();
	test_cut();
	test_other_modes();
	test_without_rom();
	printf("%s: texts cut to their terminator on a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
