#include "ladx_groups.h"
#include "ladx_sha1.h"
#include "support.h"
#include "synth_rom.h"
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

#define TEST "test_marin"
#define OUT_MAX 2048

static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void hex_of(const uint8_t *bytes, size_t n, char *hex)
{
	size_t i;

	for (i = 0; i < n; i++)
		sprintf(hex + 2 * i, "%02x", bytes[i]);
	hex[2 * n] = '\0';
}

static void check_digest(LadxSha1 *sha, const char *want, const char *what)
{
	uint8_t digest[LADX_SHA1_SIZE];
	char hex[2 * LADX_SHA1_SIZE + 1];

	ladx_sha1_final(sha, digest);
	hex_of(digest, LADX_SHA1_SIZE, hex);
	if (strcmp(hex, want) != 0)
		test_fail(TEST, "%s: SHA-1 %s, expected %s", what, hex, want);
	checks++;
}

/* sha1sum of patches/marin.txt at the pin. */
static void test_lines(void)
{
	LadxSha1 sha;
	size_t i, len;
	const char *line;

	ladx_sha1_init(&sha);
	for (i = 0; i < LADX_MARIN_LINE_COUNT; i++) {
		line = ladx_marin_line(i, &len);
		check(line && len > 0 && line[len - 1] == '\n', "every line ends with its newline");
		ladx_sha1_update(&sha, (const uint8_t *)line, len);
	}
	check(ladx_marin_line(LADX_MARIN_LINE_COUNT, &len) == NULL, "no line past the last");
	check_digest(&sha, "1cd50e338412bed15aefce2bf99d20cfcc661da2", "the lines are marin.txt's bytes");
}

/*
 * LADXR's getMarinStartingText forced to line i for i = 0..479, the global
 * random seeded with b"ladx-marin-lines": per line `ok <len>\n<bytes>` or
 * `raise <type> <message>\n` hashed, then random().hex() 0x1.4aac18b26390cp-2.
 */
static void test_every_line(void)
{
	static const uint8_t seed[] = "ladx-marin-lines";
	char head[LADX_OUTCOME_TEXT_MAX + 64];
	uint8_t out[OUT_MAX];
	LadxOutcome raised;
	LadxTextStatus st;
	LadxRng global;
	LadxSha1 sha;
	size_t i, n;
	int raises = 0;

	ladx_rng_init(&global, LADX_RNG_GLOBAL, NULL, NULL);
	ladx_rng_seed(&global, LADX_STAGE_RANDOMIZER_INIT, seed, sizeof seed - 1);
	ladx_sha1_init(&sha);
	for (i = 0; i < LADX_MARIN_LINE_COUNT; i++) {
		st = ladx_marin_line_text(i, &global, out, sizeof out, &n, &raised);
		if (st == LADX_TEXT_OK) {
			sprintf(head, "ok %lu\n", (unsigned long)n);
			ladx_sha1_update(&sha, (const uint8_t *)head, strlen(head));
			ladx_sha1_update(&sha, out, n);
		} else {
			check(st == LADX_TEXT_RAISED, "a line formats or raises");
			raises++;
			snprintf(head, sizeof head, "raise %s %s\n", raised.type, raised.message);
			ladx_sha1_update(&sha, (const uint8_t *)head, strlen(head));
		}
	}
	check(raises == 2, "lines 387 and 415 raise");
	check_digest(&sha, "b4fc739db2332a243e83e731b1873ab3fcc9b9d9", "every line as LADXR formats it");
	check(ladx_rng_random(&global, LADX_STAGE_RANDOMIZER_INIT) == 0x1.4aac18b26390cp-2,
	      "the placeholders drew as many words as LADXR");
}

/* LADXR's getMarinStartingText on these lines (its open patched), global seeded with b"ladx-marin-custom". */
static void test_placeholders(void)
{
	static const uint8_t seed[] = "ladx-marin-custom";
	static const struct {
		const char *line, *hex;
	} lines[] = {
		{"<ord1,1> <ord2,2> <ord3,3> <ord11,11> <ord5,5> <ord-3,-3> <ord12,12>\n",
		 "31737420326e64203372642031317374357468202d3372642031326e64ff"},
		{"<ord9> then <rnd2,4> and <rnd-5,-1>!\n", "397468207468656e203420616e6420202d3221ff"},
		{"<rnd2,rnd> <2,rnd999> <rnd> <rnd12,> <rndx> <rnd<rnd3>>\n",
		 "3c726e64322c726e643e2020202020203c322c726e643939393e203c726e643e3c726e6431322c3e203c726e64783e203c726e64323eff"},
		{"Total <marinLinesAmount>\n", "546f74616c20343830ff"},
		{"  spaced \\n escaped \\' quote \\\\ back \\q  \n",
		 "7370616365642020202020202020202065736361706564205e2071756f7465205c206261636b205c71ff"},
	};
	uint8_t out[OUT_MAX];
	char hex[2 * OUT_MAX + 1];
	LadxOutcome raised;
	LadxRng global;
	size_t i, n;

	ladx_rng_init(&global, LADX_RNG_GLOBAL, NULL, NULL);
	ladx_rng_seed(&global, LADX_STAGE_RANDOMIZER_INIT, seed, sizeof seed - 1);
	for (i = 0; i < sizeof lines / sizeof lines[0]; i++) {
		check(ladx_marin_text(lines[i].line, strlen(lines[i].line), &global, out, sizeof out, &n, &raised) ==
			      LADX_TEXT_OK,
		      "a custom line formats");
		hex_of(out, n, hex);
		if (strcmp(hex, lines[i].hex) != 0)
			test_fail(TEST, "custom line %lu: got %s, LADXR %s", (unsigned long)i, hex, lines[i].hex);
		checks++;
	}
	check(ladx_rng_random(&global, LADX_STAGE_RANDOMIZER_INIT) == 0x1.35984e961c328p-2,
	      "the custom lines drew as many words as LADXR");
}

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"
#define LOG_MAX 65536

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static LadxRomTables rom_tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static SynthRom synth;
static char log_text[LOG_MAX];
static size_t log_len;

static void sink(void *ctx, const char *text, size_t len)
{
	(void)ctx;
	if (log_len + len >= LOG_MAX)
		test_fail(TEST, "log text over %d bytes", LOG_MAX);
	memcpy(log_text + log_len, text, len);
	log_len += len;
	log_text[log_len] = '\0';
}

static LadxSpan span(const char *s)
{
	LadxSpan sp;

	sp.at = s;
	sp.len = strlen(s);
	return sp;
}

static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "C marin - patches.aesthetics.reduceMessageLengths(rom, rnd)\n";

/* The case seed of tests/ladx/cases/baseline. */
static const uint8_t baseline_seed[16] = {0xe8, 0x51, 0x5e, 0x70, 0x64, 0xfb, 0x5f, 0xd6,
					  0x45, 0x8e, 0x72, 0xd5, 0x61, 0x22, 0x67, 0x3b};

/*
 * derive("ladx-marin-refusal", 166), the first such seed on which LADXR's
 * baseline run fills and then raises in getMarinStartingText (line 387).
 */
static const uint8_t refusal_seed[16] = {0x2b, 0xa6, 0x80, 0xf9, 0xda, 0x28, 0xd3, 0x6d,
					 0x27, 0x47, 0xc7, 0x01, 0x7b, 0x6a, 0x6f, 0x94};

static LadxPipelineStatus run(const uint8_t seed[16], int with_rom, int with_rngs, LadxPipelineError *err)
{
	static LadxSettings settings;
	LadxTablesFile order;
	LadxTablesError terr;
	int count;

	order.name = "order.txt";
	order.text = span(order_text);
	if (ladx_tables_open(&tables, &order, NULL, 0, &terr) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic order: %s line %d", ladx_tables_status_name(terr.status), terr.line);
	ladx_settings_defaults(&settings);
	ladx_settings_validate(&settings);
	if (ladx_randomize(&randomizer, &settings, seed, 16, NULL, NULL) != LADX_RANDOMIZE_OK ||
	    !ladx_randomizer_placement(&randomizer))
		test_fail(TEST, "the baseline run fills");
	log_len = 0;
	log_text[0] = '\0';
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

static const unsigned texts[] = {
	0x01, 0x07, 0x08, 0x09, 0x0A, 0x0F, 0x13, 0x18, 0x19, 0x1E, 0x2D, 0x32, 0x33, 0x3D, 0x42, 0x44, 0x45, 0x4B,
	0x4E, 0x4F, 0x8C, 0x8E, 0x90, 0x91, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xDC, 0xE8, 0xEA,
	0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0,
};

/* The hex of the log's T texts record for key, "" when absent. */
static const char *record(unsigned key, char *buf, size_t cap)
{
	char head[32];
	const char *at, *end;

	snprintf(head, sizeof head, "\nT texts %x ", key);
	buf[0] = '\0';
	if (!(at = strstr(log_text, head)))
		return buf;
	at += strlen(head);
	end = strchr(at, '\n');
	snprintf(buf, cap, "%.*s", (int)(end - at), at);
	return buf;
}

static void expect_text(unsigned key, const char *text, int center, const char *ask)
{
	uint8_t bytes[OUT_MAX];
	char want[2 * OUT_MAX + 1], got[2 * OUT_MAX + 1], what[64];
	LadxOutcome raised;
	size_t n;

	if (ladx_format_text(text, strlen(text), center, ask, bytes, sizeof bytes, &n, &raised) != LADX_TEXT_OK)
		test_fail(TEST, "format %s", text);
	hex_of(bytes, n, want);
	snprintf(what, sizeof what, "text 0x%02X as LADXR formats it", key);
	check(strcmp(record(key, got, sizeof got), want) == 0, what);
}

static void test_group(void)
{
	static LadxRandomizer copy;
	uint8_t bytes[OUT_MAX];
	char want[2 * OUT_MAX + 1], got[2 * OUT_MAX + 1];
	LadxPipelineError err;
	LadxOutcome raised;
	const char *at;
	size_t i, n;
	int records = 0;

	check(run(baseline_seed, 1, 1, &err) == LADX_PIPELINE_OK, "baseline: the pipeline runs marin");
	ladx_randomize(&copy, pipeline.settings, baseline_seed, 16, NULL, NULL);
	check(ladx_marin_starting_text(&copy.rnd, &copy.global, bytes, sizeof bytes, &n, &raised) == LADX_TEXT_OK,
	      "baseline: the starting text formats");
	hex_of(bytes, n, want);
	check(strcmp(record(0x01, got, sizeof got), want) == 0, "text 0x01 is Marin's starting text");
	check(memcmp(&copy.rnd.py, &randomizer.rnd.py, sizeof copy.rnd.py) == 0 &&
		      memcmp(&copy.global.py, &randomizer.global.py, sizeof copy.global.py) == 0,
	      "the group drew from the randomizer's own generators");
	expect_text(0xEA, "You've got a Guardian Acorn!", 0, NULL);
	expect_text(0xEB, "You've got a Guardian Acorn!", 0, NULL);
	expect_text(0xEC, "You've got a Guardian Acorn!", 0, NULL);
	expect_text(0x2D, "{ARROWS_10}\n10 {RUPEES}!", 0, "Buy  Don't");
	expect_text(0xAE, "You got 200 {RUPEES}! You're Ecstatic!", 1, NULL);
	expect_text(0xF0, "Want to go on a raft ride for a hundred {RUPEES}?", 0, "Yes No Way");
	check(strncmp(log_text, "ladx-rom-writes 1\ntables " TABLES "\nG marin\n", strlen("ladx-rom-writes 1\ntables " TABLES "\nG marin\n")) == 0,
	      "one marin segment");
	for (at = log_text; (at = strstr(at, "\nT texts ")) != NULL; at++)
		records++;
	check(records == (int)(sizeof texts / sizeof texts[0]), "60 texts written");
	for (i = 0; i < sizeof texts / sizeof texts[0]; i++)
		check(record(texts[i], got, sizeof got)[0] != '\0', "each of reduceMessageLengths' texts");
}

static void test_skips_and_failures(void)
{
	LadxPipelineError err;

	static LadxRandomizer copy;
	uint8_t bytes[OUT_MAX];
	LadxOutcome raised;
	size_t n;

	check(run(baseline_seed, 0, 1, &err) == LADX_PIPELINE_OK && !strstr(log_text, "G marin"),
	      "without the ROM, marin writes nothing");
	ladx_randomize(&copy, pipeline.settings, baseline_seed, 16, NULL, NULL);
	ladx_marin_starting_text(&copy.rnd, &copy.global, bytes, sizeof bytes, &n, &raised);
	check(memcmp(&copy.rnd.py, &randomizer.rnd.py, sizeof copy.rnd.py) == 0 &&
		      memcmp(&copy.global.py, &randomizer.global.py, sizeof copy.global.py) == 0,
	      "without the ROM, marin still draws as with it");
	check(run(baseline_seed, 0, 0, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "marin") == 0,
	      "without the ROM or the generators, marin fails");
	check(run(baseline_seed, 1, 0, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "marin") == 0,
	      "without the generators, marin fails");
}

/* LADXR at the pin on this seed: outcome crash UnicodeEncodeError <message>, no ROM written. */
static void test_refusal(void)
{
	LadxPipelineError err;

	check(run(refusal_seed, 1, 1, &err) == LADX_PIPELINE_RAISED && strcmp(err.group, "marin") == 0,
	      "the refusal seed raises in marin");
	check(err.outcome && err.outcome->kind == LADX_OUTCOME_CRASH &&
		      strcmp(err.outcome->type, "UnicodeEncodeError") == 0 &&
		      strcmp(err.outcome->message, "'ascii' codec can't encode characters in position 54-55: "
						   "ordinal not in range(128)") == 0,
	      "LADXR's outcome: crash UnicodeEncodeError and its message");
	check(run(refusal_seed, 0, 1, &err) == LADX_PIPELINE_RAISED && strcmp(err.group, "marin") == 0 &&
		      strcmp(err.outcome->type, "UnicodeEncodeError") == 0,
	      "the refusal seed raises in marin without the ROM too");
}

int main(void)
{
	test_lines();
	test_every_line();
	test_placeholders();
	synth_rom_build(&synth, 0x3A12E001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	test_group();
	test_skips_and_failures();
	test_refusal();
	printf("%s: marin.txt's lines, every line as LADXR formats it, the group on a synthetic ROM, the refusal seed (%d checks)\n", TEST, checks);
	return 0;
}
