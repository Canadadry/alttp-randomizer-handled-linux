#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_title"
#define OUT_MAX 65536
#define BG_TILES 9
#define BG_ATTRIBUTES 10

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static LadxRomTables rom_tables;
static LadxRandomizer randomizer;
static LadxPipeline pipeline;
static LadxPipelineRom rom;
static SynthRom synth;
static LadxBackground bg;
static char out[OUT_MAX];
static size_t out_len;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static size_t from_hex(const char *hex, uint8_t *bytes)
{
	size_t n = strlen(hex) / 2, i;
	unsigned v;

	for (i = 0; i < n; i++) {
		sscanf(hex + 2 * i, "%2x", &v);
		bytes[i] = (uint8_t)v;
	}
	return n;
}

static void test_encode_text(void)
{
	static const uint8_t want[] = {0x00, 0x19, 0x1a, 0x3e, 0x70, 0x79, 0x3e, 0x3f, 0x39, 0x42, 0x3c, 0x3d, 0x7e, 0x7e};
	uint8_t got[sizeof want];

	ladx_title_encode_text("AZaz09z-.:?!# ", sizeof want, got);
	check(memcmp(got, want, sizeof want) == 0, "_encode's letters, digits, CHAR_MAP and its default 0x7E");
}

/* python3: int(t, 16), else int(hashlib.md5(t.encode('ascii', 'replace')).hexdigest(), 16); then % 112 ** 5 */
static void test_seed_number(void)
{
	check(ladx_title_seed_number("0123456789ABCDEFFEDCBA9876543210", 32) == 9946018320ull, "a hex seed");
	check(ladx_title_seed_number("0123456789abcdeffedcba9876543210", 32) == 9946018320ull, "lower-case hex");
	check(ladx_title_seed_number("hello world", 11) == 13150899651ull, "a non-hex text: its MD5");
	check(ladx_title_seed_number("", 0) == 17097572990ull, "the empty text: its MD5");
	check(ladx_title_seed_number("a\x80", 2) == ladx_title_seed_number("a?", 2) &&
		      ladx_title_seed_number("a\xff", 2) == ladx_title_seed_number("a?", 2) &&
		      ladx_title_seed_number("a?", 2) == 13878934235ull,
	      "a byte over 0x7F hashed as '?'");
}

/*
 * Synthetic backgrounds run through LADXR's own setRomInfo (a fake ROM
 * object holding only these four entries): its stored bytes.
 */
static const char *const bg_in[4] = {
	"98000301020304" "99e04551" "9a010211121300", "9a00820a0b0c" "9820c47f00",
	"98014e03" "9a00030102030400", "98400506060606060600",
};

static const struct {
	const char *text;
	const char *bg_out[4]; /* tiles 3, tiles 4, attributes 3, attributes 4 */
} setrominfo[] = {
	{"0123456789ABCDEFFEDCBA9876543210",
	 {"98000301020304980a09a0a2d4d6a0a2c4c6bcbe98290a7fa1a3d5d7a1a3c5c7bdbf98490aa1a0a1a0a1a0a1a0a1a0a198694aa299e04551"
	  "9a010f707172737475767778790001020304059a210f0504030201007978777675747372717000",
	  "980a09a0a2d4d6a0a2c4c6bcbe9820007f98290a7fa1a3d5d7a1a3c5c7bdbf9840007f98490aa1a0a1a0a1a0a1a0a1a0a19860007f98694a"
	  "a29880007f98a0007f9a00100a707172737475767778790001020304059a20100b05040302010079787776757473727170"
	  "9a40000c00",
	  "98011203030303030303030308080b0b0b0b09090c0c982a0908080b0b0b0b09090c0c9a001001000000000000000000000000000000"
	  "009a214f0000",
	  "980a0908080b0b0b0b09090c0c982a0908080b0b0b0b09090c0c984045069a014f009a214f0000"}},
	{"Race",
	 {"98000301020304980a09a0a2d4d6a0a2c4c6bcbe98290a7fa1a3d5d7a1a3c5c7bdbf98490aa1a0a1a0a1a0a1a0a1a0a198694aa299e04551"
	  "9a0103111a1c1e00",
	  "980a09a0a2d4d6a0a2c4c6bcbe9820007f98290a7fa1a3d5d7a1a3c5c7bdbf9840007f98490aa1a0a1a0a1a0a1a0a1a0a19860007f98694a"
	  "a29880007f98a0007f9a00040a111a1c1e9a20000b9a40000c00",
	  "98011203030303030303030308080b0b0b0b09090c0c982a0908080b0b0b0b09090c0c9a0004010000000000",
	  "980a0908080b0b0b0b09090c0c982a0908080b0b0b0b09090c0c984045069a01430000"}},
	{"hello world",
	 {"98000301020304980a09a4a6b0b2b4b6c0c2d4d698290a7fa5a7b1b3b5b7c1c3d5d798490aa1a0a1a0a1a0a1a0a1a0a198694aa299e04551"
	  "9a010a211e2525287e30282b251d00",
	  "980a09a4a6b0b2b4b6c0c2d4d69820007f98290a7fa5a7b1b3b5b7c1c3d5d79840007f98490aa1a0a1a0a1a0a1a0a1a0a19860007f98694a"
	  "a29880007f98a0007f9a000b0a211e2525287e30282b251d9a20000b9a40000c00",
	  "9801120303030303030303030f0f0d0d0c0c0c0c0d0d982a090f0f0d0d0c0c0c0c0d0d9a000b01000000000000000000000000",
	  "980a090f0f0d0d0c0c0c0c0d0d982a090f0f0d0d0c0c0c0c0d0d984045069a014a0000"}},
};

static void test_edit_against_ladxr(void)
{
	uint8_t in[256], want[512], got[512];
	size_t in_n, want_n, got_n, c, k;
	uint64_t number;
	char what[96];

	for (c = 0; c < sizeof setrominfo / sizeof setrominfo[0]; c++) {
		/* LADXR's race text keeps the hex seed's number. */
		number = ladx_title_seed_number(c == 1 ? setrominfo[0].text : setrominfo[c].text,
						strlen(c == 1 ? setrominfo[0].text : setrominfo[c].text));
		for (k = 0; k < 4; k++) {
			in_n = from_hex(bg_in[k], in);
			want_n = from_hex(setrominfo[c].bg_out[k], want);
			sprintf(what, "%s: background %u", setrominfo[c].text, (unsigned)k);
			check(ladx_background_decode(&bg, in, in_n), what);
			check(ladx_title_edit(&bg, k >= 2, setrominfo[c].text, strlen(setrominfo[c].text), number), what);
			got_n = ladx_background_encode(&bg, got, sizeof got);
			check(got_n == want_n && memcmp(got, want, got_n) == 0, what);
		}
	}
	check(!ladx_title_edit(&bg, 0, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0", 65, 0),
	      "a text over 64 bytes is refused");
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

static const char *const order_text =
	"ladx-tables-order 1\n"
	"section generator.py generateRom\n"
	"C title - patches.titleScreen.setRomInfo(rom, binascii.hexlify(seed).decode(\"ascii\").upper(), settings)\n";

static const uint8_t seed[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
				 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};

static LadxPipelineStatus run(const char *assignment, int with_rom, LadxPipelineError *err)
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

static size_t resolve(const char *tokens, uint8_t *bytes, size_t cap)
{
	size_t n = 0;
	const char *t = tokens;
	unsigned long q, len, i;
	char *end;

	while (*t && *t != '\n') {
		if (*t == '@') {
			q = strtoul(t + 1, &end, 16);
			if (*end != '+')
				return 0;
			len = strtoul(end + 1, &end, 16);
			for (i = 0; i < len && n < cap; i++)
				bytes[n++] = synth.rom[q + i];
		} else {
			for (end = (char *)t; *end && *end != ' ' && *end != '\n' && n < cap; end += 2) {
				unsigned v;

				sscanf(end, "%2x", &v);
				bytes[n++] = (uint8_t)v;
			}
		}
		t = end;
		if (*t == ' ')
			t++;
	}
	return n;
}

static size_t expected_entry(int table, uint32_t index, const char *text, uint64_t number, uint8_t *bytes,
			     size_t cap)
{
	const SynthEntry *e = &synth.entry[synth.table_first[table] + index];

	if (!ladx_background_decode(&bg, synth.rom + e->start, e->stop - e->start) ||
	    !ladx_title_edit(&bg, table == BG_ATTRIBUTES, text, strlen(text), number))
		test_fail(TEST, "the synthetic background %d %u does not edit", table, index);
	return ladx_background_encode(&bg, bytes, cap);
}

static void check_log(const char *what, const char *text, const char *flag)
{
	static const struct {
		int table;
		uint32_t index;
		const char *prefix;
	} records[4] = {
		{BG_TILES, 3, "T background_tiles 3 "},
		{BG_TILES, 4, "T background_tiles 4 "},
		{BG_ATTRIBUTES, 3, "T background_attributes 3 "},
		{BG_ATTRIBUTES, 4, "T background_attributes 4 "},
	};
	static uint8_t want[8192], got[8192];
	char head[512];
	const char *line;
	uint64_t number = ladx_title_seed_number("0123456789ABCDEFFEDCBA9876543210", 32);
	size_t want_n, got_n, k;

	sprintf(head, HEADER "G title\nB 7 %s\n", flag);
	check(strncmp(out, head, strlen(head)) == 0, what);
	line = out + strlen(head);
	for (k = 0; k < 4; k++) {
		check(strncmp(line, records[k].prefix, strlen(records[k].prefix)) == 0, what);
		want_n = expected_entry(records[k].table, records[k].index, text, number, want, sizeof want);
		got_n = resolve(line + strlen(records[k].prefix), got, sizeof got);
		check(want_n > 0 && got_n == want_n && memcmp(got, want, want_n) == 0, what);
		line = strchr(line, '\n') + 1;
	}
	check(*line == '\0', what);
}

static void test_group(void)
{
	LadxPipelineError err;

	check(run(NULL, 1, &err) == LADX_PIPELINE_OK, "defaults: the pipeline runs title");
	check_log("defaults: 0x52 at bank 0 offset 7, the hex seed on backgrounds 3 and 4",
		  "0123456789ABCDEFFEDCBA9876543210", "52");
	check(run("race=1", 1, &err) == LADX_PIPELINE_OK, "race: the pipeline runs title");
	check_log("race: 0x01, \"Race\" and the hex seed's pattern", "Race", "01");
	check(run(NULL, 0, &err) == LADX_PIPELINE_OK && strcmp(out, HEADER) == 0, "without the ROM, title is skipped");
}

int main(void)
{
	test_encode_text();
	test_seed_number();
	test_edit_against_ladxr();
	synth_rom_build(&synth, 0x7171E001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	test_group();
	printf("%s: _encode, the seed number, setRomInfo's edits against LADXR on synthetic backgrounds, the group "
	       "on a synthetic ROM (%d checks)\n",
	       TEST, checks);
	return 0;
}
