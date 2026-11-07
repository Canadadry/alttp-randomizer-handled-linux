#include "ladx_pipeline.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_pipeline"
#define OUT_MAX 4096

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static LadxRomTables rom_tables;
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

/* Two table units around C rows: beta twice, rommy (needs the ROM), gamma unregistered, drawy (needs the ROM, draws without). */
static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "table alpha alpha.1 patches.a.one(rom)\n"
				      "C beta - patches.b.first(rom, rnd)\n"
				      "C rommy - patches.r.one(rom)\n"
				      "C beta - patches.b.second(rom)\n"
				      "C gamma - patches.g.one(rom)\n"
				      "C drawy - patches.d.one(rom, rnd)\n"
				      "table alpha alpha.2 patches.a.two(rom)\n";

static const char *const alpha_text = "ladx-tables 1\n"
				      "group alpha\n"
				      "tables " TABLES "\n"
				      "unit alpha.1\n"
				      "E\n"
				      "B 10 0102\n"
				      "unit alpha.2 music\n"
				      "E \"\"\n"
				      "B 20 03\n";

/* alpha.2 with no entry for the default music. */
static const char *const alpha_no_entry = "ladx-tables 1\n"
					  "group alpha\n"
					  "tables " TABLES "\n"
					  "unit alpha.1\n"
					  "E\n"
					  "B 10 0102\n"
					  "unit alpha.2 music\n"
					  "E off\n"
					  "B 20 03\n";

typedef struct Calls {
	char text[128];
	int fail_beta;
	int rommy_saw_tables;
	int rommy_const; /* rommy also writes its constants file */
} Calls;

static void called(Calls *c, const char *name)
{
	size_t len = strlen(c->text);

	snprintf(c->text + len, sizeof c->text - len, "%s%s", len ? " " : "", name);
}

/* One byte at offset, a literal unlike the source byte when there is one. */
static int write_byte(LadxEncoder *e, const char *group, uint32_t offset, const LadxPipelineRom *rom)
{
	uint8_t v = rom ? (uint8_t)(rom->bytes[offset] ^ 0xffu) : 0xaa;
	int32_t origin = LADX_ENC_KNOWN;

	return ladx_enc_bank(e, span(group), offset, &v, &origin, 1) == LADX_ENC_OK;
}

static int run_beta(LadxEncoder *e, LadxPipeline *p)
{
	Calls *c = p->ctx;

	called(c, "beta");
	if (c->fail_beta == 2) {
		p->raised.kind = LADX_OUTCOME_CRASH;
		p->raised.type = "PlantedError";
		p->raised.message = "planted";
	}
	return !c->fail_beta && write_byte(e, "beta", 0x40, p->rom);
}

static int run_rommy(LadxEncoder *e, LadxPipeline *p)
{
	Calls *c = p->ctx;
	const LadxPipelineRom *rom = p->rom;

	called(c, "rommy");
	c->rommy_saw_tables = rom && rom->tables == &rom_tables && rom->ks == &ks;
	return rom && write_byte(e, "rommy", 0x50, rom) && (!c->rommy_const || ladx_pipeline_const(e, p, "rommy"));
}

static int run_drawy(LadxEncoder *e, LadxPipeline *p)
{
	called(p->ctx, "drawy");
	return p->rom && write_byte(e, "drawy", 0x58, p->rom);
}

static int draw_drawy(LadxEncoder *e, LadxPipeline *p)
{
	(void)e;
	called(p->ctx, "drawy-draws");
	return !p->rom;
}

static const LadxPipelineGroup planted[] = {
	{"rommy", 1, run_rommy, NULL},
	{"beta", 0, run_beta, NULL},
	{"drawy", 1, run_drawy, draw_drawy},
};

static int rommy_const_mode;

/* rommy: NULL for no constants file. */
static void open_with(const char *alpha, const char *rommy)
{
	LadxTablesFile order, group[2];
	LadxTablesError err;

	order.name = "order.txt";
	order.text = span(order_text);
	group[0].name = "alpha.txt";
	group[0].text = span(alpha);
	group[1].name = "rommy.txt";
	group[1].text = span(rommy ? rommy : "");
	if (ladx_tables_open(&tables, &order, group, rommy ? 2 : 1, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static void open_tables(const char *alpha)
{
	open_with(alpha, NULL);
}

static LadxPipelineStatus run(const LadxPipelineRom *rom, Calls *calls, LadxPipelineError *err)
{
	static LadxSettings settings;
	static LadxPipeline p;

	ladx_settings_defaults(&settings);
	ladx_settings_validate(&settings);
	out_len = 0;
	out[0] = '\0';
	memset(calls, 0, sizeof *calls);
	calls->rommy_const = rommy_const_mode;
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK)
		test_fail(TEST, "log begins");
	if (ladx_enc_init(&enc, rom ? rom->bytes : NULL, rom ? rom->len : 0, rom ? rom->ks : NULL,
			  rom ? rom->tables : NULL, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "encoder initialises");
	p.tables = &tables;
	p.settings = &settings;
	p.spot_count = 0;
	p.randomizer = NULL;
	p.rom = rom;
	p.groups = planted;
	p.group_count = 3;
	p.ctx = calls;
	return ladx_pipeline_run(&p, &enc, err);
}

static void test_without_rom(void)
{
	Calls calls;
	LadxPipelineError err;

	open_tables(alpha_text);
	check(run(NULL, &calls, &err) == LADX_PIPELINE_OK, "without a ROM, the walk succeeds");
	check(strcmp(calls.text, "beta drawy-draws") == 0,
	      "without a ROM, the ROM-free group runs, once, and a ROM group's draws alone, never its run");
	check(strcmp(out, HEADER "G alpha\nB 10 0102\nG beta\nB 40 aa\nG alpha\nB 20 03\n") == 0,
	      "without a ROM, table entries and the group's write in order.txt's order");
}

static void test_with_rom(void)
{
	char want[OUT_MAX];
	Calls calls;
	LadxPipelineError err;
	LadxPipelineRom rom;

	synth_rom_build(&synth, 0x5EED0002u);
	check(ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) == LADX_ROM_PARSE_OK,
	      "the synthetic ROM parses");
	check(ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) == LADX_KS_OK, "KS builds");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	open_tables(alpha_text);
	check(run(&rom, &calls, &err) == LADX_PIPELINE_OK, "with a ROM, the walk succeeds");
	check(strcmp(calls.text, "beta rommy drawy") == 0,
	      "with a ROM, every group runs once, at its first C row, never a draws-only run");
	check(calls.rommy_saw_tables, "the ROM group receives the parsed ROM");
	snprintf(want, sizeof want, HEADER "G alpha\nB 10 0102\nG beta\nB 40 %02x\nG rommy\nB 50 %02x\nG drawy\nB 58 %02x\nG alpha\nB 20 03\n",
		 synth.rom[0x40] ^ 0xffu, synth.rom[0x50] ^ 0xffu, synth.rom[0x58] ^ 0xffu);
	check(strcmp(out, want) == 0, "with a ROM, the ROM group's segment between beta's and alpha's");
}

/* rommy's constants: a fixed unit (a keep, a copy, a literal) and one keyed by music. */
static const char *const rommy_text = "ladx-tables 1\n"
				      "group rommy\n"
				      "tables " TABLES "\n"
				      "unit rommy.1\n"
				      "E\n"
				      "B 60 =1 @100+2 7f\n"
				      "unit rommy.2 music\n"
				      "E \"\"\n"
				      "T texts 3 41 @200+2\n"
				      "T texts 4 p1234\n"
				      "E off\n"
				      "B 70 01\n";

static void use_synth_rom(LadxPipelineRom *rom)
{
	synth_rom_build(&synth, 0x5EED0002u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom->bytes = synth.rom;
	rom->len = SYNTH_ROM_SIZE;
	rom->tables = &rom_tables;
	rom->ks = &ks;
}

static void test_const(void)
{
	char want[OUT_MAX];
	Calls calls;
	LadxPipelineError err;
	LadxPipelineRom rom;

	use_synth_rom(&rom);
	rommy_const_mode = 1;
	open_with(alpha_text, rommy_text);
	check(run(&rom, &calls, &err) == LADX_PIPELINE_OK, "a group writes its constants file");
	snprintf(want, sizeof want,
		 HEADER "G alpha\nB 10 0102\nG beta\nB 40 %02x\nG rommy\nB 50 %02x\nB 60 %02x%02x%02x7f\n"
			"T texts 3 41%02x%02x\nT texts 4 p1234\nG drawy\nB 58 %02x\nG alpha\nB 20 03\n",
		 synth.rom[0x40] ^ 0xffu, synth.rom[0x50] ^ 0xffu, synth.rom[0x60], synth.rom[0x100], synth.rom[0x101],
		 synth.rom[0x200], synth.rom[0x201], synth.rom[0x58] ^ 0xffu);
	check(strcmp(out, want) == 0, "constants: values, the keyed unit's entry for the key, computed literals");

	open_with(alpha_text, "ladx-tables 1\ngroup rommy\ntables " TABLES "\nunit rommy.1\nE\nB 60 @fffff+2\n");
	check(run(&rom, &calls, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "rommy") == 0,
	      "a reference beyond the ROM fails the group");
	open_with(alpha_text, "ladx-tables 1\ngroup rommy\ntables " TABLES "\nunit rommy.1 music\nE off\nB 60 01\n");
	check(run(&rom, &calls, &err) == LADX_PIPELINE_GROUP, "a unit with no entry for the key fails the group");
	open_tables(alpha_text);
	check(run(&rom, &calls, &err) == LADX_PIPELINE_GROUP, "a group without a constants file fails");
	rommy_const_mode = 0;
}

static void test_errors(void)
{
	Calls calls;
	LadxPipelineError err;
	int count = -1;

	open_tables(alpha_no_entry);
	check(run(NULL, &calls, &err) == LADX_PIPELINE_TABLES && err.status == LADX_PIPELINE_TABLES &&
		      err.tables.status == LADX_TABLES_NO_ENTRY && strcmp(err.tables.unit, "alpha.2") == 0,
	      "a unit with no entry for the key is a tables error");
	open_tables(alpha_text);
	memset(&calls, 0, sizeof calls);
	{
		static LadxSettings settings;
		static LadxPipeline p;

		ladx_settings_defaults(&settings);
		ladx_settings_validate(&settings);
		out_len = 0;
		ladx_wlog_begin(&wlog, span(TABLES), sink, NULL);
		ladx_enc_init(&enc, NULL, 0, NULL, NULL, &wlog);
		calls.fail_beta = 1;
		p.tables = &tables;
		p.settings = &settings;
		p.spot_count = 0;
		p.randomizer = NULL;
		p.rom = NULL;
		p.groups = planted;
		p.group_count = 3;
		p.ctx = &calls;
		check(ladx_pipeline_run(&p, &enc, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "beta") == 0 &&
			      !err.outcome,
		      "a group's failure stops the walk, naming the group");
		calls.fail_beta = 2;
		out_len = 0;
		ladx_wlog_begin(&wlog, span(TABLES), sink, NULL);
		ladx_enc_init(&enc, NULL, 0, NULL, NULL, &wlog);
		check(ladx_pipeline_run(&p, &enc, &err) == LADX_PIPELINE_RAISED && strcmp(err.group, "beta") == 0 &&
			      err.outcome == &p.raised && strcmp(err.outcome->type, "PlantedError") == 0,
		      "a group that sets raised stops the walk with LADXR's outcome");
		calls.fail_beta = 1;
		out_len = 0;
		ladx_wlog_begin(&wlog, span(TABLES), sink, NULL);
		ladx_enc_init(&enc, NULL, 0, NULL, NULL, &wlog);
		check(ladx_pipeline_run(&p, &enc, &err) == LADX_PIPELINE_GROUP && !err.outcome,
		      "raised is cleared before each run");
	}
	{
		const LadxPipelineGroup *g = ladx_pipeline_groups(&count);

		check(count == 4 && strcmp(g[0].name, "bank3e") == 0 && g[0].needs_rom && !g[0].draw &&
			      strcmp(g[1].name, "marin") == 0 && g[1].needs_rom && g[1].draw &&
			      strcmp(g[2].name, "hints") == 0 && !g[2].needs_rom && !g[2].draw &&
			      strcmp(g[3].name, "title") == 0 && g[3].needs_rom && !g[3].draw,
		      "the library registers bank3e, marin, hints then title, in LADXR's order, hints alone "
		      "ROM-free, marin drawing without the ROM");
	}
}

int main(void)
{
	test_without_rom();
	test_with_rom();
	test_const();
	test_errors();
	printf("%s: %d checks ok\n", TEST, checks);
	return 0;
}
