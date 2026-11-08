#include "ladx_assemble.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_assemble"
#define OUT_MAX 4096

static LadxAssembler assembler;
static LadxTables tables;
static SynthRom synth;
static uint8_t base[LADX_ROM_SIZE];
static uint8_t out[LADX_ROM_SIZE];
static uint8_t base_sha1[LADX_SHA1_SIZE];
static char log_text[OUT_MAX];
static size_t log_len;
static int checks, draws;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void log_sink(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	if (log_len + len >= OUT_MAX)
		test_fail(TEST, "log text over %d bytes", OUT_MAX);
	memcpy(log_text + log_len, text, len);
	log_len += len;
	log_text[log_len] = '\0';
}

static void trace_sink(void *ctx, const LadxRngRecord *record)
{
	(void)ctx;
	(void)record;
	draws++;
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

static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "table alpha alpha.1 patches.a.one(rom)\n"
				      "C beta - patches.b.one(rom, rnd)\n"
				      "table alpha alpha.2 patches.a.two(rom)\n"
				      "C gamma - patches.g.one(rom)\n";

static const char *const alpha_text = "ladx-tables 1\n"
				      "group alpha\n"
				      "tables " TABLES "\n"
				      "unit alpha.1\n"
				      "E\n"
				      "B 10 0102\n"
				      "unit alpha.2 music\n"
				      "E \"\"\n"
				      "B 20 03 @30+2\n";

static const char *const alpha_no_entry = "ladx-tables 1\n"
					  "group alpha\n"
					  "tables " TABLES "\n"
					  "unit alpha.1\n"
					  "E\n"
					  "B 10 0102\n"
					  "unit alpha.2 music\n"
					  "E off\n"
					  "B 20 03\n";

typedef struct Seen {
	char order[64];
	uint8_t beta_at_10, beta_at_20, gamma_at_20;
	int fail; /* 1: gamma fails, 2: gamma raises */
} Seen;

static Seen seen;

static void called(const char *name)
{
	size_t len = strlen(seen.order);

	snprintf(seen.order + len, sizeof seen.order - len, "%s%s", len ? " " : "", name);
}

static uint8_t now(LadxEncoder *e, uint32_t offset)
{
	uint8_t v = 0;
	int32_t origin;

	if (!ladx_enc_bank_value(e, offset, &v, &origin))
		test_fail(TEST, "a planted group reads offset %x", (unsigned)offset);
	return v;
}

static int put(LadxEncoder *e, const char *group, uint32_t offset, uint8_t v)
{
	int32_t origin = LADX_ENC_KNOWN;

	return ladx_enc_bank(e, span(group), offset, &v, &origin, 1) == LADX_ENC_OK;
}

static int run_beta(LadxEncoder *e, LadxPipeline *p)
{
	called("beta");
	seen.beta_at_10 = now(e, 0x10);
	seen.beta_at_20 = now(e, 0x20);
	return p->rom && put(e, "beta", 0x40, 0x44);
}

static int run_gamma(LadxEncoder *e, LadxPipeline *p)
{
	called("gamma");
	seen.gamma_at_20 = now(e, 0x20);
	if (seen.fail == 2) {
		p->raised.kind = LADX_OUTCOME_CRASH;
		p->raised.type = "PlantedError";
		p->raised.message = "planted";
	}
	return !seen.fail && put(e, "gamma", 0x14E, 0x99);
}

static const LadxPipelineGroup planted[] = {
	{"beta", 1, run_beta, NULL},
	{"gamma", 1, run_gamma, NULL},
};

static void open_tables(const char *alpha)
{
	LadxTablesFile order, group;
	LadxTablesError err;

	order.name = "order.txt";
	order.text = span(order_text);
	group.name = "alpha.txt";
	group.text = span(alpha);
	if (ladx_tables_open(&tables, &order, &group, 1, &err) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic tables: %s line %d", ladx_tables_status_name(err.status), err.line);
}

static LadxSettings settings;
static uint8_t seed[16];
static LadxAssembleInput input;

static void reset(void)
{
	open_tables(alpha_text);
	ladx_settings_defaults(&settings);
	ladx_settings_validate(&settings);
	memset(seed, 0x5E, sizeof seed);
	memcpy(base, synth.rom, LADX_ROM_SIZE);
	ladx_sha1(base, LADX_ROM_SIZE, base_sha1);
	memset(&input, 0, sizeof input);
	input.base = base;
	input.base_len = LADX_ROM_SIZE;
	input.base_sha1 = base_sha1;
	input.tables = &tables;
	input.settings = &settings;
	input.seed = seed;
	input.seed_len = sizeof seed;
	input.trace = trace_sink;
	input.log = log_sink;
	input.groups = planted;
	input.group_count = 2;
	memset(&seen, 0, sizeof seen);
	memset(out, 0xA5, sizeof out);
	log_len = 0;
	log_text[0] = '\0';
	draws = 0;
}

static int out_untouched(void)
{
	size_t i;

	for (i = 0; i < sizeof out; i++)
		if (out[i] != 0xA5)
			return 0;
	return 1;
}

/* rom.py fixHeader's formula, written out here: the header checksum and the global sum. */
static void fixed_header(const uint8_t *image, uint8_t want[3])
{
	unsigned sum = 0, global = 0;
	size_t i;

	for (i = 0x134; i < 0x14D; i++)
		sum -= image[i] + 1u;
	want[0] = (uint8_t)sum;
	for (i = 0; i < LADX_ROM_SIZE; i++)
		global += i == 0x14D ? want[0] : (i == 0x14E || i == 0x14F) ? 0 : image[i];
	want[1] = (uint8_t)(global >> 8);
	want[2] = (uint8_t)global;
}

static void test_ok(void)
{
	static const char want_log[] = "ladx-rom-writes 1\ntables " TABLES "\n"
				       "G alpha\nB 10 0102\nG beta\nB 40 44\nG alpha\nB 20 03 @30+2\nG gamma\nB 14e 99\n";
	static const char name[15] = "LADXR";
	uint8_t header[3];
	size_t i;
	int others = 1;

	reset();
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_OK, "a synthetic base with its own digest assembles");
	check(strcmp(seen.order, "beta gamma") == 0, "the C groups run once each, in order.txt's order");
	check(seen.beta_at_10 == 0x01, "a C group sees the table entry of a row before it");
	check(seen.beta_at_20 == base[0x20], "a C group does not see the table entry of a row after it");
	check(seen.gamma_at_20 == 0x03, "the later C group sees the later table entry");
	check(strcmp(log_text, want_log) == 0, "the log sink receives the write log in order.txt's order");
	check(draws > 0, "the trace sink receives the fill's draws");
	check(out[0x10] == 0x01 && out[0x11] == 0x02 && out[0x40] == 0x44 && out[0x20] == 0x03,
	      "the image holds the table entries' and the groups' bytes");
	check(out[0x21] == base[0x30] && out[0x22] == base[0x31], "a table reference is read from the base");
	check(memcmp(out + 0x134, name, sizeof name) == 0, "the header's name is LADXR, zero-padded");
	fixed_header(out, header);
	check(out[0x14D] == header[0] && out[0x14E] == header[1] && out[0x14F] == header[2],
	      "the header fix runs on the image last: a group's write to 0x14E is overwritten by the global sum");
	for (i = 0; i < LADX_ROM_SIZE; i++) {
		if (i == 0x10 || i == 0x11 || i == 0x20 || i == 0x21 || i == 0x22 || i == 0x40 || (i >= 0x134 && i < 0x150))
			continue;
		others &= out[i] == base[i];
	}
	check(others, "every other byte is the base's");
	check(!assembler.spoiler.built, "no spoiler without the spoiler group");
}

static void test_base_refused(void)
{
	uint8_t digest[LADX_SHA1_SIZE];

	reset();
	input.base_len = LADX_ROM_SIZE - 1;
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_WRONG_BASE && out_untouched() &&
		      !seen.order[0] && draws == 0,
	      "a base of 1 MiB less one byte is refused before any work, the output untouched");
	reset();
	input.base_sha1 = NULL;
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_WRONG_BASE && out_untouched() && draws == 0,
	      "without an expected digest, a base other than the LADX ROM is refused");
	ladx_sha1(base, LADX_ROM_SIZE, digest);
	check(memcmp(assembler.base_digest, digest, sizeof digest) == 0, "the refused base's SHA-1 is reported");
	reset();
	base[0x31DD] = 0x34;
	base[0x31DE] = 0x12;
	ladx_sha1(base, LADX_ROM_SIZE, base_sha1);
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_BAD_BASE && out_untouched() && draws == 0,
	      "a base whose tables do not parse is refused before the fill");
}

static void test_settings_and_fill(void)
{
	static const uint8_t error_seed[16] = {0x3a, 0xd4, 0x84, 0xd5, 0xf6, 0xd7, 0x79, 0x5a,
					       0xd8, 0x89, 0x3b, 0x6c, 0x53, 0x6d, 0xa4, 0xad};

	reset();
	check(ladx_settings_set(&settings, "overworld=dungeondive") == LADX_SETTINGS_OK, "a tier-3 value sets");
	ladx_settings_validate(&settings);
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_UNSUPPORTED && out_untouched() && draws == 0,
	      "unsupported settings are refused before the fill");
	reset();
	memcpy(seed, error_seed, sizeof seed);
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_OUTCOME && out_untouched() && !seen.order[0],
	      "a seed whose fill LADXR refuses ends with its outcome, no group run");
	check(ladx_randomizer_outcome(&assembler.randomizer)->kind == LADX_OUTCOME_ERROR,
	      "the fill's outcome is LADXR's error");
}

static void test_pipeline_errors(void)
{
	reset();
	seen.fail = 1;
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_PIPELINE && out_untouched() &&
		      assembler.pipeline_error.status == LADX_PIPELINE_GROUP &&
		      strcmp(assembler.pipeline_error.group, "gamma") == 0,
	      "a failing group stops the run, named, the output untouched");
	reset();
	seen.fail = 2;
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_PIPELINE && out_untouched() &&
		      assembler.pipeline_error.status == LADX_PIPELINE_RAISED &&
		      strcmp(assembler.pipeline_error.outcome->type, "PlantedError") == 0,
	      "a group raising stops the run with LADXR's outcome");
	reset();
	open_tables(alpha_no_entry);
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_PIPELINE && out_untouched() &&
		      assembler.pipeline_error.status == LADX_PIPELINE_TABLES &&
		      assembler.pipeline_error.tables.status == LADX_TABLES_NO_ENTRY,
	      "a unit without an entry for the settings stops the run");
}

static void test_bad_input(void)
{
	reset();
	check(ladx_assemble(&assembler, &input, base + 1) == LADX_ASSEMBLE_BAD_INPUT, "an output overlapping the base");
	check(ladx_assemble(&assembler, &input, NULL) == LADX_ASSEMBLE_BAD_INPUT, "no output");
	check(ladx_assemble(&assembler, NULL, out) == LADX_ASSEMBLE_BAD_INPUT, "no input");
	input.tables = NULL;
	check(ladx_assemble(&assembler, &input, out) == LADX_ASSEMBLE_BAD_INPUT && out_untouched(), "no tables");
	check(strcmp(ladx_assemble_status_name(LADX_ASSEMBLE_WRONG_BASE), "wrong base") == 0, "status names");
}

int main(void)
{
	synth_rom_build(&synth, 0xA55E0001u);
	test_ok();
	test_base_refused();
	test_settings_and_fill();
	test_pipeline_errors();
	test_bad_input();
	printf("%s: %d checks passed\n", TEST, checks);
	return 0;
}
