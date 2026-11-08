#include "ladx_groups.h"
#include "support.h"
#include "synth_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_header"
#define VECTORS "oracle/header_vectors.txt"
#define SIZE 0x100000u
#define FIX_MAX (256 * 1024)
#define TEXT_MAX 4096

static uint8_t image[SIZE];
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

typedef struct ArrayIo {
	uint8_t *bytes;
	size_t len;
	char writes[TEXT_MAX];
	size_t used;
	int fail_read_at, fail_write; /* -1: never; else the read offset, the write count */
	int write_count;
} ArrayIo;

static int array_read(void *ctx, uint32_t offset, uint8_t *out, size_t n)
{
	ArrayIo *a = ctx;

	if (offset + n > a->len || (a->fail_read_at >= 0 && (uint32_t)a->fail_read_at >= offset &&
				    (uint32_t)a->fail_read_at < offset + n))
		return 0;
	memcpy(out, a->bytes + offset, n);
	return 1;
}

static int array_write(void *ctx, uint32_t offset, const uint8_t *bytes, size_t n)
{
	ArrayIo *a = ctx;
	size_t i;

	if (a->fail_write >= 0 && a->write_count == a->fail_write)
		return 0;
	a->write_count++;
	if (offset + n > a->len)
		test_fail(TEST, "a write past the image");
	memcpy(a->bytes + offset, bytes, n);
	a->used += (size_t)snprintf(a->writes + a->used, TEXT_MAX - a->used, "W %x ", (unsigned)offset);
	for (i = 0; i < n; i++)
		a->used += (size_t)snprintf(a->writes + a->used, TEXT_MAX - a->used, "%02x", bytes[i]);
	a->used += (size_t)snprintf(a->writes + a->used, TEXT_MAX - a->used, "\n");
	return 1;
}

static int fix(ArrayIo *a, const char *name, size_t len)
{
	LadxHeaderIo io;

	io.read = array_read;
	io.write = array_write;
	io.ctx = a;
	return ladx_header_fix(&io, len, (const uint8_t *)name, name ? strlen(name) : 0);
}

static void array_init(ArrayIo *a)
{
	memset(a, 0, sizeof *a);
	a->bytes = image;
	a->len = SIZE;
	a->fail_read_at = -1;
	a->fail_write = -1;
}

static uint8_t header_checksum(void)
{
	unsigned sum = 0, i;

	for (i = 0x134; i < 0x14D; i++)
		sum += image[i] + 1u;
	return (uint8_t)(0u - sum);
}

static unsigned global_sum(void)
{
	unsigned long sum = 0;
	size_t i;

	for (i = 0; i < SIZE; i++)
		sum += image[i];
	return (unsigned)(sum & 0xFFFF);
}

static void test_zero_image(void)
{
	static ArrayIo a;

	memset(image, 0, SIZE);
	array_init(&a);
	check(fix(&a, "LADXR", SIZE), "a zero image is fixed");
	check(strcmp(a.writes, "W 134 4c4144585200000000000000000000\nW 14d 6c\nW 14e 00\nW 14f 00\nW 14e 01\n"
			       "W 14f e7\n") == 0,
	      "the name, the header checksum, the zeroing and the global checksum, in LADXR's order");
	check(header_checksum() == image[0x14D], "0x14D: minus the sum of each header byte plus one");
}

static void test_no_name(void)
{
	static ArrayIo a;

	memset(image, 0, SIZE);
	memcpy(image + 0x134, "QRS", 3);
	array_init(&a);
	check(fix(&a, NULL, SIZE), "no name: fixed");
	check(strncmp(a.writes, "W 14d ", 6) == 0 && memcmp(image + 0x134, "QRS", 3) == 0,
	      "no name: the title is kept and not written");
}

static void test_name_cut_and_padded(void)
{
	static ArrayIo a;

	memset(image, 0x33, SIZE);
	array_init(&a);
	check(fix(&a, "ABCDEFGHIJKLMNOPQRST", SIZE) && memcmp(image + 0x134, "ABCDEFGHIJKLMNO", 15) == 0 &&
		      image[0x143] == 0x33,
	      "a long name is cut to 15 bytes");
	memset(image, 0x33, SIZE);
	array_init(&a);
	check(fix(&a, "AB", SIZE) && image[0x135] == 'B' && image[0x136] == 0 && image[0x142] == 0 &&
		      image[0x143] == 0x33,
	      "a short name is padded with zeros to 15 bytes");
}

static void test_checksums_read_the_current_bytes(void)
{
	static ArrayIo a;
	unsigned want;

	memset(image, 0xFF, SIZE);
	image[0x14D] = 0x12;
	image[0x14E] = 0x34;
	image[0x14F] = 0x56;
	image[SIZE - 1] = 0x01;
	array_init(&a);
	check(fix(&a, "LADXR", SIZE), "a 0xFF image is fixed");
	check(image[0x14D] == header_checksum(), "the header checksum over the name just written");
	want = (global_sum() - image[0x14E] - image[0x14F]) & 0xFFFF;
	check(image[0x14E] == want >> 8 && image[0x14F] == (want & 0xFF),
	      "the global sum: every byte, the new 0x14D counted, 0x14E and 0x14F zeroed first, mod 0x10000");
	memset(image, 0, SIZE);
	image[0x8122] = 0x05;
	image[0x8123] = 0x40;
	array_init(&a);
	check(fix(&a, "LADXR", 0x8123), "a shorter length is fixed");
	want = (global_sum() - 0x40 - image[0x14E] - image[0x14F]) & 0xFFFF;
	check(image[0x14E] == want >> 8 && image[0x14F] == (want & 0xFF), "a shorter length sums its bytes only");
}

static void test_failures(void)
{
	static ArrayIo a;
	int k;

	memset(image, 0, SIZE);
	array_init(&a);
	check(!fix(&a, "LADXR", 0x14F), "a length short of 0x150 fails");
	array_init(&a);
	a.fail_read_at = 0x140;
	check(!fix(&a, "LADXR", SIZE), "a failing header read fails");
	array_init(&a);
	a.fail_read_at = 0x80000;
	check(!fix(&a, "LADXR", SIZE), "a failing bank read fails");
	for (k = 0; k < 6; k++) {
		array_init(&a);
		a.fail_write = k;
		check(!fix(&a, "LADXR", SIZE) && a.write_count == k, "a failing write fails, nothing written after it");
	}
}

#define OUT_MAX 65536
#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"

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

/* early writes 0x0144 (inside the header checksum's bytes) and bank 0x21's 0x1000: both counted. */
static const char *const order_text = "ladx-tables-order 1\n"
				      "section generator.py generateRom\n"
				      "table early early.1 patches.early(rom)\n"
				      "section rom.py ROM.fixHeader\n"
				      "C header - checksum = 0\n";
static const char *const early_text = "ladx-tables 1\n"
				      "group early\n"
				      "tables " TABLES "\n"
				      "unit early.1\n"
				      "E\n"
				      "B 144 a5\n"
				      "B 85000 3c77\n";

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

static LadxPipelineStatus run(int with_rom, LadxPipelineError *err)
{
	static const uint8_t seed[16] = {'L', 'A', 'D', 'X', '-', 'h', 'e', 'a', 'd', 'e', 'r', '-', 't', 'e', 's', 't'};
	static LadxSettings settings;
	int count;

	ladx_settings_defaults(&settings);
	ladx_settings_validate(&settings);
	ladx_randomize(&randomizer, &settings, seed, sizeof seed, NULL, NULL);
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
	pipeline.rom = with_rom ? &rom : NULL;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

static void test_group(void)
{
	static ArrayIo a;
	LadxPipelineError err;
	uint8_t v;
	int32_t origin;
	uint32_t i;
	int same = 1;

	check(run(1, &err) == LADX_PIPELINE_OK, "with the ROM, the pipeline runs header");
	memcpy(image, synth.rom, SIZE);
	image[0x144] = 0xA5;
	image[0x85000] = 0x3C;
	image[0x85001] = 0x77;
	array_init(&a);
	check(fix(&a, "LADXR", SIZE), "the expected header");
	for (i = 0x134; i < 0x150; i++)
		same = same && ladx_enc_bank_value(&enc, i, &v, &origin) && v == image[i];
	check(same, "the name and both checksums over the bytes as the earlier groups left them");
	check(strstr(out, "\nG header\nB 134 ") != NULL && strstr(out, "\nB 14d ") != NULL,
	      "the header segment: the name's record, then 0x14D..0x14F's");
	check(run(0, &err) == LADX_PIPELINE_OK && strstr(out, "G header") == NULL, "without the ROM, header is skipped");
}

static void test_registry(void)
{
	const LadxPipelineGroup *groups;
	int count;

	groups = ladx_pipeline_groups(&count);
	check(count > 2 && strcmp(groups[count - 1].name, "header") == 0 && strcmp(groups[count - 2].name, "spoiler") == 0 &&
		      strcmp(groups[count - 3].name, "save") == 0,
	      "header registered last, after save and spoiler");
	check(groups[count - 1].needs_rom && groups[count - 1].draw == NULL, "a rom-group with no draw");
	memset(&pipeline, 0, sizeof pipeline);
	check(!ladx_group_header(&enc, &pipeline), "no ROM: the group fails");
}

static char fixture[FIX_MAX];

static unsigned long hex(const char *s, const char *what, const char *name)
{
	char *end;
	unsigned long v = strtoul(s, &end, 16);

	if (end == s || *end)
		test_fail(TEST, "%s: %s: bad %s `%s`", VECTORS, name, what, s);
	return v;
}

static size_t hex_bytes(const char *s, uint8_t *out, size_t cap, const char *name)
{
	size_t n = strlen(s) / 2, k;

	if (strlen(s) % 2 || n > cap)
		test_fail(TEST, "%s: %s: bad bytes `%s`", VECTORS, name, s);
	for (k = 0; k < n; k++) {
		char pair[3] = {s[2 * k], s[2 * k + 1], '\0'};

		out[k] = (uint8_t)hex(pair, "byte", name);
	}
	return n;
}

static void replay_vectors(void)
{
	static ArrayIo a;
	static char want[TEXT_MAX];
	static uint8_t bytes[64];
	FILE *file = fopen(VECTORS, "rb");
	const char *name = "";
	char *at, *line;
	char title[64];
	size_t len, n = 0;
	int count = 0, in = 0, has_name = 0;

	if (!file)
		test_fail(TEST, "cannot open %s", VECTORS);
	len = fread(fixture, 1, FIX_MAX - 1, file);
	fclose(file);
	if (len == FIX_MAX - 1)
		test_fail(TEST, "%s: over %d bytes", VECTORS, FIX_MAX - 2);
	fixture[len] = '\0';
	for (at = fixture; at && *at; at = line) {
		char *row = at;

		line = strchr(at, '\n');
		if (line)
			*line++ = '\0';
		if (row[0] == '#' || row[0] == '\0' || (!in && count == 0 && strcmp(row, "ladx-header-vectors 1") == 0))
			continue;
		if (strncmp(row, "V ", 2) == 0) {
			name = row + 2;
			memset(image, 0, SIZE);
			want[0] = '\0';
			in = 1;
			has_name = -1;
			continue;
		}
		if (!in)
			test_fail(TEST, "%s: a line outside a vector: %s", VECTORS, row);
		if (strcmp(row, "end") == 0) {
			LadxHeaderIo io;

			if (has_name < 0)
				test_fail(TEST, "%s: %s: no name line", VECTORS, name);
			array_init(&a);
			io.read = array_read;
			io.write = array_write;
			io.ctx = &a;
			if (!ladx_header_fix(&io, SIZE, has_name ? (const uint8_t *)title : NULL, has_name ? n : 0))
				test_fail(TEST, "%s: fixHeader failed", name);
			if (strcmp(a.writes, want) != 0)
				test_fail(TEST, "%s: writes\n%s\nwant\n%s", name, a.writes, want);
			count++;
			in = 0;
		} else if (strncmp(row, "F ", 2) == 0) {
			char *f1 = strchr(row + 2, ' '), *f2 = f1 ? strchr(f1 + 1, ' ') : NULL;
			unsigned long offset, length;

			if (!f2)
				test_fail(TEST, "%s: %s: bad F line", VECTORS, name);
			*f1 = *f2 = '\0';
			offset = hex(row + 2, "offset", name);
			length = hex(f1 + 1, "length", name);
			if (offset + length > SIZE)
				test_fail(TEST, "%s: %s: a fill past the image", VECTORS, name);
			memset(image + offset, (int)hex(f2 + 1, "byte", name), length);
		} else if (strncmp(row, "P ", 2) == 0) {
			char *f1 = strchr(row + 2, ' ');
			unsigned long offset;
			size_t k;

			if (!f1)
				test_fail(TEST, "%s: %s: bad P line", VECTORS, name);
			*f1 = '\0';
			offset = hex(row + 2, "offset", name);
			k = hex_bytes(f1 + 1, bytes, sizeof bytes, name);
			if (offset + k > SIZE)
				test_fail(TEST, "%s: %s: a plant past the image", VECTORS, name);
			memcpy(image + offset, bytes, k);
		} else if (strncmp(row, "name ", 5) == 0) {
			has_name = strcmp(row + 5, "-") != 0;
			n = has_name ? hex_bytes(row + 5, (uint8_t *)title, sizeof title, name) : 0;
		} else if (strncmp(row, "W ", 2) == 0) {
			if (strlen(want) + strlen(row) + 2 > TEXT_MAX)
				test_fail(TEST, "%s: %s: too many writes", VECTORS, name);
			strcat(want, row);
			strcat(want, "\n");
		} else {
			test_fail(TEST, "%s: %s: unknown line %s", VECTORS, name, row);
		}
	}
	if (in || count == 0)
		test_fail(TEST, "%s: %d vectors, one unfinished: %d", VECTORS, count, in);
	printf("%s: %d vectors of %s replayed write for write\n", TEST, count, VECTORS);
}

int main(int argc, char **argv)
{
	if (test_fixtures_mode(TEST, argc, argv)) {
		replay_vectors();
		return 0;
	}
	test_zero_image();
	test_no_name();
	test_name_cut_and_padded();
	test_checksums_read_the_current_bytes();
	test_failures();
	synth_rom_build(&synth, 0x4EAD0001u);
	if (ladx_rom_tables_parse(synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_ROM_PARSE_OK ||
	    ladx_ks_build(&ks, synth.rom, SYNTH_ROM_SIZE, &rom_tables) != LADX_KS_OK)
		test_fail(TEST, "the synthetic ROM parses and gives KS");
	rom.bytes = synth.rom;
	rom.len = SYNTH_ROM_SIZE;
	rom.tables = &rom_tables;
	rom.ks = &ks;
	open_tables();
	test_registry();
	test_group();
	printf("%s: ROM.fixHeader's name and checksums, alone and as the group on a synthetic ROM (%d checks)\n", TEST, checks);
	return 0;
}
