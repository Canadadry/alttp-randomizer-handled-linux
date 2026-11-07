#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_bank3e"
#define OUT_MAX 4096

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

static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "C bank3e - patches.bank3e.addBank3E(rom, seed, settings)\n";

/* A planted constants file: a fixed unit and one keyed by HARD_MODE. */
static const char *const bank3e_text = "ladx-tables 1\n"
				       "group bank3e\n"
				       "tables " TABLES "\n"
				       "unit bank3e.1\n"
				       "E\n"
				       "B 8 0102\n"
				       "unit bank3e.2 asm:HARD_MODE\n"
				       "E 0\n"
				       "B f8000 aa\n"
				       "E 1\n"
				       "B f8000 bb\n";

static const uint8_t seed[16] = {'L', 'A', 'D', 'X', '-', 'b', 'a', 'n', 'k', '3', 'e', '-', 't', 'e', 's', 't'};

static void open_tables(int with_constants)
{
	LadxTablesFile order, group;
	LadxTablesError err;

	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "bank3e.txt";
	group.text = span(bank3e_text);
	if (ladx_tables_open(&tables, &order, &group, with_constants ? 1 : 0, &err) != LADX_TABLES_OK)
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

/* The expected log: the planted constant, the seed, the settings string unless race, 0xBE copied. */
static void expected(char *want, size_t cap, uint8_t keyed, const LadxSettings *s)
{
	const SynthEntry *skull = &synth.entry[synth.table_first[0] + 0x111];
	char text[LADX_SETTINGS_SHORT_MAX + 1], hex[2 * LADX_SETTINGS_SHORT_MAX + 8];
	size_t len = 0, i;
	int n;

	len += (size_t)snprintf(want + len, cap - len, HEADER "G bank3e\nB 8 0102\nB f8000 %02x\nB faf00 ", keyed);
	for (i = 0; i < sizeof seed; i++)
		len += (size_t)snprintf(want + len, cap - len, "%02x", seed[i]);
	if (!s->value[LADX_SETTING_RACE]) {
		n = ladx_settings_short_string(s, text, sizeof text);
		snprintf(hex, sizeof hex, "%02x", n);
		for (i = 0; i < (size_t)n; i++)
			snprintf(hex + 2 + 2 * i, sizeof hex - 2 - 2 * i, "%02x", (unsigned char)text[i]);
		len += (size_t)snprintf(want + len, cap - len, "\nB faf20 %s", hex);
	}
	snprintf(want + len, cap - len, "\nT texts be @%x+%x\n", skull->start, skull->stop - skull->start);
}

static void test_writes(void)
{
	char want[OUT_MAX];
	LadxPipelineError err;

	open_tables(1);
	check(run(NULL, 1, &err) == LADX_PIPELINE_OK, "defaults: the pipeline runs bank3e");
	expected(want, sizeof want, 0xaa, pipeline.settings);
	check(strcmp(out, want) == 0, "defaults: constants for HARD_MODE 0, seed, settings string, 0xBE carried");
	check(out[strlen(HEADER "G bank3e\nB 8 0102\nB f8000 aa\nB faf00 ") + 32] == '\n',
	      "the seed's 16 bytes, no more");

	check(run("hardmode=hero", 1, &err) == LADX_PIPELINE_OK, "hard mode: the pipeline runs bank3e");
	expected(want, sizeof want, 0xbb, pipeline.settings);
	check(strcmp(out, want) == 0, "hard mode: the HARD_MODE 1 entry, the settings string with hardmode");

	check(run("race=1", 1, &err) == LADX_PIPELINE_OK, "race: the pipeline runs bank3e");
	expected(want, sizeof want, 0xaa, pipeline.settings);
	check(strcmp(out, want) == 0 && !strstr(out, "faf20"), "race: no settings string");
}

static void test_skips_and_failures(void)
{
	LadxPipelineError err;

	open_tables(1);
	check(run(NULL, 0, &err) == LADX_PIPELINE_OK && strcmp(out, HEADER) == 0, "without the ROM, bank3e is skipped");
	open_tables(0);
	check(run(NULL, 1, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "bank3e") == 0,
	      "without its constants file, bank3e fails");
}

int main(void)
{
	synth_rom_build(&synth, 0xB3E00001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	test_writes();
	test_skips_and_failures();
	printf("%s: constants, seed, settings string and the 0xBE copy on a synthetic ROM (%d checks)\n", TEST,
	       checks);
	return 0;
}
