#include "ladx_groups.h"
#include "ladx_sha1.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_music"
#define LOG_MAX 65536
#define BANK2 (2u * SYNTH_ROM_BANK)
/* The planted KS: one 4-byte bank span, outside every synthetic table. */
#define KS_AT (3u * SYNTH_ROM_BANK)

static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

/* patches/music.py _LOOPING_MUSIC at the pin, as runs of consecutive tracks. */
static const uint8_t looping[][2] = {
	{0x01, 0x0E}, {0x11, 0x19}, {0x1C, 0x1D}, {0x1F, 0x1F}, {0x21, 0x27}, {0x2F, 0x2F},
	{0x31, 0x33}, {0x37, 0x37}, {0x39, 0x3A}, {0x3C, 0x3C}, {0x3E, 0x3E}, {0x40, 0x40},
	{0x48, 0x4B}, {0x4E, 0x4E}, {0x50, 0x50}, {0x53, 0x55}, {0x57, 0x61},
};

static int is_looping(uint8_t v)
{
	size_t i;

	for (i = 0; i < sizeof looping / sizeof looping[0]; i++)
		if (v >= looping[i][0] && v <= looping[i][1])
			return 1;
	return 0;
}

/*
 * LADXR's own patches.music.randomizeMusic at the pin, rnd =
 * random.Random(b"ladx-music-<k>"), on a fake bank 0x02 of zeros: SHA-1
 * of its bytes 0x000..0x11F, then rnd.random().hex().
 */
static void test_against_ladxr(int k, const char *want, double next)
{
	uint8_t table[LADX_MUSIC_TABLE_SIZE], digest[LADX_SHA1_SIZE], seed[32];
	char hex[2 * LADX_SHA1_SIZE + 1], what[64];
	LadxSha1 sha;
	LadxRng rnd;
	size_t i;
	int tracks = 1;

	sprintf((char *)seed, "ladx-music-%d", k);
	ladx_rng_init(&rnd, LADX_RNG_RND, NULL, NULL);
	ladx_rng_seed(&rnd, LADX_STAGE_RANDOMIZER_INIT, seed, strlen((char *)seed));
	memset(table, 0, sizeof table);
	check(ladx_music_randomize(&rnd, table) == LADX_RNG_OK, "randomizeMusic draws");
	ladx_sha1_init(&sha);
	ladx_sha1_update(&sha, table, sizeof table);
	ladx_sha1_final(&sha, digest);
	for (i = 0; i < LADX_SHA1_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
	snprintf(what, sizeof what, "ladx-music-%d: the table as LADXR writes it", k);
	if (strcmp(hex, want) != 0)
		test_fail(TEST, "%s: SHA-1 %s, expected %s", what, hex, want);
	for (i = 0; i < sizeof table; i++)
		tracks &= is_looping(table[i]);
	check(tracks, "every byte of the table is a looping track");
	check(ladx_rng_random(&rnd, LADX_STAGE_RANDOMIZER_INIT) == next, "as many words drawn as LADXR");
}

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"
#define HEADER "ladx-rom-writes 1\ntables " TABLES "\n"

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxKs ks;
static LadxRomTables rom_tables;
static LadxRandomizer randomizer, copy;
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
				      "C music - patches.music.randomizeMusic(rom, rnd)\n";

/* The case seed of tests/ladx/cases/baseline. */
static const uint8_t baseline_seed[16] = {0xe8, 0x51, 0x5e, 0x70, 0x64, 0xfb, 0x5f, 0xd6,
					  0x45, 0x8e, 0x72, 0xd5, 0x61, 0x22, 0x67, 0x3b};

static LadxPipelineStatus run(const char *music, int with_rom, int with_rngs, LadxPipelineError *err)
{
	static LadxSettings settings;
	LadxTablesFile order;
	LadxTablesError terr;
	char assignment[32];
	int count;

	order.name = "order.txt";
	order.text = span(order_text);
	if (ladx_tables_open(&tables, &order, NULL, 0, &terr) != LADX_TABLES_OK)
		test_fail(TEST, "synthetic order: %s line %d", ladx_tables_status_name(terr.status), terr.line);
	ladx_settings_defaults(&settings);
	snprintf(assignment, sizeof assignment, "music=%s", music);
	if (ladx_settings_set(&settings, assignment) != LADX_SETTINGS_OK)
		test_fail(TEST, "cannot set %s", assignment);
	ladx_settings_validate(&settings);
	if (ladx_randomize(&randomizer, &settings, baseline_seed, 16, NULL, NULL) != LADX_RANDOMIZE_OK ||
	    !ladx_randomizer_placement(&randomizer))
		test_fail(TEST, "the baseline seed fills");
	ladx_randomize(&copy, &settings, baseline_seed, 16, NULL, NULL);
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

static int same_generators(void)
{
	return memcmp(&copy.rnd.py, &randomizer.rnd.py, sizeof copy.rnd.py) == 0 &&
	       memcmp(&copy.global.py, &randomizer.global.py, sizeof copy.global.py) == 0;
}

/*
 * The log's B records resolved against the synthetic ROM into bank 0x02's
 * first 0x120 bytes, *covered counting the bytes they write there, *copied
 * those copied from KS_AT; 0 on a token or offset outside them.
 */
static int resolve(uint8_t *table, int *covered, int *copied)
{
	const char *line = strstr(log_text, "\nG music\n"), *t;
	unsigned long at, q, n, i;
	char *end;
	unsigned v;

	*covered = *copied = 0;
	if (!line)
		return 0;
	while ((line = strstr(line, "\nB ")) != NULL) {
		at = strtoul(line + 3, &end, 16);
		for (t = end; *t == ' ';) {
			t++;
			if (*t == '=') {
				n = strtoul(t + 1, &end, 16);
				for (i = 0; i < n; i++, at++) {
					if (at < BANK2 || at >= BANK2 + LADX_MUSIC_TABLE_SIZE)
						return 0;
					table[at - BANK2] = synth.rom[at];
					(*covered)++;
				}
			} else if (*t == '@') {
				q = strtoul(t + 1, &end, 16);
				n = strtoul(end + 1, &end, 16);
				for (i = 0; i < n; i++, at++) {
					if (at < BANK2 || at >= BANK2 + LADX_MUSIC_TABLE_SIZE)
						return 0;
					table[at - BANK2] = synth.rom[q + i];
					(*covered)++;
					*copied += q + i >= KS_AT && q + i < KS_AT + 4;
				}
			} else {
				for (end = (char *)t; *end && *end != ' ' && *end != '\n'; end += 2, at++) {
					if (at < BANK2 || at >= BANK2 + LADX_MUSIC_TABLE_SIZE || sscanf(end, "%2x", &v) != 1)
						return 0;
					table[at - BANK2] = (uint8_t)v;
					(*covered)++;
				}
			}
			t = end;
		}
		line = t;
	}
	return 1;
}

static void test_group(void)
{
	uint8_t want[LADX_MUSIC_TABLE_SIZE], got[LADX_MUSIC_TABLE_SIZE];
	LadxPipelineError err;
	int covered, copies;

	check(run("random", 1, 1, &err) == LADX_PIPELINE_OK, "music=random: the pipeline runs music");
	check(strncmp(log_text, HEADER "G music\n", strlen(HEADER "G music\n")) == 0, "one music segment");
	check(ladx_music_randomize(&copy.rnd, want) == LADX_RNG_OK, "the copy draws");
	check(same_generators(), "the group drew from the randomizer's own rnd");
	memset(got, 0, sizeof got);
	check(resolve(got, &covered, &copies), "every record inside bank 0x02 0x000..0x11F");
	check(covered == LADX_MUSIC_TABLE_SIZE, "every byte of the table written once");
	check(memcmp(got, want, sizeof want) == 0, "the records give randomizeMusic's table");
	check(copies >= 8 && copies % 4 == 0,
	      "each drawn run equal to the planted KS run (East Tal Tal's two rows at least) is its copy");
}

static void test_skips_and_failures(void)
{
	static const char *const others[] = {"", "off", "shifted"};
	LadxPipelineError err;
	size_t i;

	for (i = 0; i < sizeof others / sizeof others[0]; i++) {
		check(run(others[i], 1, 1, &err) == LADX_PIPELINE_OK && !strstr(log_text, "G music"),
		      "another music: nothing written");
		check(same_generators(), "another music: nothing drawn");
		check(run(others[i], 0, 1, &err) == LADX_PIPELINE_OK && same_generators(),
		      "another music without the ROM: nothing drawn");
	}
	check(run("random", 0, 1, &err) == LADX_PIPELINE_OK && !strstr(log_text, "G music"),
	      "without the ROM, music writes nothing");
	{
		uint8_t table[LADX_MUSIC_TABLE_SIZE];

		ladx_randomize(&copy, pipeline.settings, baseline_seed, 16, NULL, NULL);
		ladx_music_randomize(&copy.rnd, table);
		check(same_generators(), "without the ROM, music still draws as with it");
	}
	check(run("random", 0, 0, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "music") == 0,
	      "without the ROM or the generators, music fails");
	check(run("random", 1, 0, &err) == LADX_PIPELINE_GROUP && strcmp(err.group, "music") == 0,
	      "without the generators, music fails");
}

int main(void)
{
	uint8_t table[LADX_MUSIC_TABLE_SIZE];
	LadxPipelineError err;
	LadxKsLabel label;

	test_against_ladxr(1, "10d4861daee8cef3ee66274c8f956fd78985aa8d", 0x1.ef1813f931884p-1);
	test_against_ladxr(2, "9f26cddb85bf71f06a0e0eaea94b3121060e2bbb", 0x1.ebef8fa9e2ef4p-2);
	test_against_ladxr(3, "c34035f18ef1c52d8650b3edfe69eddc2c6498da", 0x1.b9147dd3dab66p-2);
	synth_rom_build(&synth, 0x3A12E001u);
	/*
	 * East Tal Tal's rows (rooms 0x07..0x0D, 0x17..0x1D) are 7-byte runs:
	 * the only KS span holds its track 4 times, so KS-match copies at
	 * least one 4-byte run of each. Bank 0x02 made 0xFF, no track.
	 */
	run("random", 0, 1, &err);
	ladx_music_randomize(&copy.rnd, table);
	memset(synth.rom + BANK2, 0xFF, LADX_MUSIC_TABLE_SIZE);
	memset(synth.rom + KS_AT, table[0x07], 4);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK)
		test_fail(TEST, "the synthetic ROM parses");
	label.kind = LADX_KS_BANK;
	label.table = 0;
	label.index = 0;
	label.alt[0] = '\0';
	ladx_ks_init(&ks, synth.rom, SYNTH_ROM_SIZE);
	if (ladx_ks_add(&ks, KS_AT, KS_AT + 4, &label) != LADX_KS_OK || ladx_ks_finish(&ks) != LADX_KS_OK)
		test_fail(TEST, "the planted KS builds");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	test_group();
	test_skips_and_failures();
	printf("%s: randomizeMusic against LADXR, the group on a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
