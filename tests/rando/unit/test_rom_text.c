#include "rando.h"
#include "rando_rom.h"
#include "rando_text.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_rom_text"
#define FIXTURE_TEXT_DEFAULT "oracle/text_default.txt"
#define TEXT_ADDRESS 0x0E0000u
#define ROM_SIZE (2u * 1024 * 1024)

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

/* Expected bytes come from VT's Dialog at 219fcafd run under PHP 8.3, never from the C encoder. */
static void expect(const char *what, const char *utf8, bool pause, const char *hex)
{
	uint8_t want[RANDO_TEXT_ENTRY_MAX], got[RANDO_TEXT_ENTRY_MAX];
	size_t nwant = 0, ngot, i;
	unsigned byte;

	while (hex[2 * nwant] != '\0') {
		if (sscanf(hex + 2 * nwant, "%2x", &byte) != 1)
			test_fail(TEST, "%s: bad expected hex", what);
		want[nwant++] = (uint8_t)byte;
	}
	ngot = rando_text_encode(utf8, pause, got);
	if (ngot != nwant || memcmp(got, want, nwant) != 0) {
		fprintf(stderr, "%s: got ", what);
		for (i = 0; i < ngot; i++)
			fprintf(stderr, "%02X", got[i]);
		fprintf(stderr, "\n");
		test_fail(TEST, "%s: expected %s", what, hex);
	}
}

static void test_layout(void)
{
	expect("one line", "Hi", true,
	       "FBB1D8");
	expect("empty string", "", true,
	       "FB");
	expect("second line F8", "a\nb", true,
	       "FBD0F8D1");
	expect("third line F9", "a\nb\nc", true,
	       "FBD0F8D1F9D2");
	expect("fourth line F6, FA after the third", "a\nb\nc\nd\ne", true,
	       "FBD0F8D1F9D2FAF6D3F6D4");
	expect("FA after every third line", "a\nb\nc\nd\ne\nf\ng", true,
	       "FBD0F8D1F9D2FAF6D3F6D4F6D5FAF6D6");
	expect("no FA with pause false", "a\nb\nc\nd\ne\nf\ng", false,
	       "FBD0F8D1F9D2F6D3F6D4F6D5F6D6");
	expect("trailing command lowers line_count once more", "a\nb\nc\n{CHOICE}", true,
	       "FBD0F8D1F9D2FE68");
	expect("trailing command after a fourth line", "a\nb\nc\nd\n{CHOICE}", true,
	       "FBD0F8D1F9D2FAF6D3FE68");
	expect("command line takes no row", "a\nb\nc\n{PAUSE3}\nd", true,
	       "FBD0F8D1F9D2FAFE7803F6D3");
	expect("empty line keeps its row marker", "a\n\nb", true,
	       "FBD0F8F9D1");
}

static void test_commands(void)
{
	expect("unknown command gives nothing and no row", "a\n{FOO}\nb", true,
	       "FBD0F8D1");
	expect("NOTEXT returns at once", "a\n{NOTEXT}\nb", true,
	       "FBFE6E00FE6B04");
	expect("command trimmed before matching", "{CHOICE} \nA", true,
	       "FBFE68AA");
	expect("INTRO pads the first three rows only", "{INTRO}\nab\nc\nd\ne", false,
	       "FBFE6E00FE7707FC03FE6B02FE67D0D1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF8D2FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF9D3FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF6D4");
	expect("brace inside a line is no command", "x{CHOICE}", true,
	       "FBE7FFACB1B8B2ACAEFF");
	expect("{SPEED0}", "{SPEED0}\nA", true,
	       "FBFC00AA");
	expect("{SPEED2}", "{SPEED2}\nA", true,
	       "FBFC02AA");
	expect("{SPEED6}", "{SPEED6}\nA", true,
	       "FBFC06AA");
	expect("{PAUSE1}", "{PAUSE1}\nA", true,
	       "FBFE7801AA");
	expect("{PAUSE3}", "{PAUSE3}\nA", true,
	       "FBFE7803AA");
	expect("{PAUSE5}", "{PAUSE5}\nA", true,
	       "FBFE7805AA");
	expect("{PAUSE7}", "{PAUSE7}\nA", true,
	       "FBFE7807AA");
	expect("{PAUSE9}", "{PAUSE9}\nA", true,
	       "FBFE7809AA");
	expect("{INPUT}", "{INPUT}\nA", true,
	       "FBFAAA");
	expect("{CHOICE}", "{CHOICE}\nA", true,
	       "FBFE68AA");
	expect("{ITEMSELECT}", "{ITEMSELECT}\nA", true,
	       "FBFE69AA");
	expect("{CHOICE2}", "{CHOICE2}\nA", true,
	       "FBFE71AA");
	expect("{CHOICE3}", "{CHOICE3}\nA", true,
	       "FBFE72AA");
	expect("{C:GREEN}", "{C:GREEN}\nA", true,
	       "FBFE7707AA");
	expect("{C:YELLOW}", "{C:YELLOW}\nA", true,
	       "FBFE7702AA");
	expect("{HARP}", "{HARP}\nA", true,
	       "FBFE792DAA");
	expect("{MENU}", "{MENU}\nA", true,
	       "FBFE6D00AA");
	expect("{BOTTOM}", "{BOTTOM}\nA", true,
	       "FBFE6D01AA");
	expect("{NOBORDER}", "{NOBORDER}\nA", true,
	       "FBFE6B02AA");
	expect("{CHANGEPIC}", "{CHANGEPIC}\nA", true,
	       "FBFE67FE67AA");
	expect("{CHANGEMUSIC}", "{CHANGEMUSIC}\nA", true,
	       "FBFE67AA");
	expect("{INTRO}", "{INTRO}\nA", true,
	       "FBFE6E00FE7707FC03FE6B02FE67AAFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
	expect("{NOTEXT}", "{NOTEXT}\nA", true,
	       "FBFE6E00FE6B04");
	expect("{IBOX}", "{IBOX}\nA", true,
	       "FBFE6B02FE7707FC03F7AA");
}

static void test_wrapping(void)
{
	expect("single-byte breaks at the last space within 19", "aaaa bbbb cccc dddd eeee", true,
	       "FBD0D0D0D0FFD1D1D1D1FFD2D2D2D2FFD3D3D3D3F8D4D4D4D4");
	expect("single-byte breaks back at the last space", "aaaa bbbb cccc ddddd eeee", true,
	       "FBD0D0D0D0FFD1D1D1D1FFD2D2D2D2F8D3D3D3D3D3FFD4D4D4D4");
	expect("multibyte last line trimmed on both sides", "abcdefghijklmnopqrs  t…", true,
	       "FBD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2F8E39F");
	expect("a 25-character word stays whole, then is cut to 19", "abcdefghijklmnopqrstuvwxy z", true,
	       "FBD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2F8E9");
	expect("single-byte keeps a trailing space", "aaaaaaaaaaaaaaaaaa  bbb", true,
	       "FBD0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0FFF8D1D1D1");
	expect("single-byte keeps a leading space", "aaaaaaaaaaaaaaaaaaa  bbb", true,
	       "FBD0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0F8FFD1D1D1");
	expect("short single-byte line not trimmed", "ab ", true,
	       "FBD0D1FF");
	expect("short multibyte line rtrimmed", "ab… ", true,
	       "FBD0D19F");
	expect("multibyte re-packed by character count", "aaaa bbbb cccc dddd… eeee", true,
	       "FBD0D0D0D0FFD1D1D1D1FFD2D2D2D2F8D3D3D3D39FFFD4D4D4D4");
	expect("multibyte long word on its own line", "x abcdefghijklmnopqrstu… yz", true,
	       "FBE7F8D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2F9E8E9");
	expect("multibyte keeps inner double space", "aaaa  bbbb cccc ddd≥ eeee", true,
	       "FBD0D0D0D0FFFFD1D1D1D1FFD2D2D2D2F8D3D3D399FFD4D4D4D4");
	expect("wrapping is per source line", "aaaa bbbb cccc dddd eeee\nf", true,
	       "FBD0D0D0D0FFD1D1D1D1FFD2D2D2D2FFD3D3D3D3F8D4D4D4D4F9D5");
}

static void test_characters(void)
{
	expect("digits", "0123456789", true,
	       "FBA0A1A2A3A4A5A6A7A8A9");
	expect("capitals and lower case", "AZaz", true,
	       "FBAAC3D0E9");
	expect("punctuation", "?!,-.~～:", true,
	       "FBC6C7C8C9CDCECEEA");
	expect("quotes, name, face", "'’@>", true,
	       "FB9D9DFE6A9B9C");
	expect("hylian", "%^=", true,
	       "FBFD10FD11FD12");
	expect("arrows", "↑↓→←", true,
	       "FBFD13FD14FD15FD16");
	expect("hearts", "¼½¾♥", true,
	       "FBE5E7E6E7E8E9EAEB");
	expect("variables", "ᚋᚌᚍᚎ", true,
	       "FBFE6C00FE6C01FE6C02FE6C03");
	expect("kana", "あんアォ", true,
	       "FB000F509F");
	expect("cursor and ellipsis", "≥…", true,
	       "FB999F");
	expect("unknown characters give FF", "a{é", true,
	       "FBD0FFFF");
}

static void test_cut(void)
{
	static char text[200 * 20];
	static const char head[] = "FBD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2F8D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2F9";
	static const char tail[] = "E0E1E2F6D0D1D2D3D4D5D6D7D8D9DADB";
	uint8_t got[RANDO_TEXT_ENTRY_MAX];
	char hex[2 * RANDO_TEXT_ENTRY_MAX + 1];
	size_t n, i;

	for (i = 0; i < 200; i++)
		memcpy(text + 20 * i, i < 199 ? "abcdefghijklmnopqrs\n" : "abcdefghijklmnopqrs", 20);
	n = rando_text_encode(text, true, got);
	if (n != RANDO_TEXT_ENTRY_MAX)
		test_fail(TEST, "cut: %zu bytes, expected %d", n, RANDO_TEXT_ENTRY_MAX);
	for (i = 0; i < n; i++)
		sprintf(hex + 2 * i, "%02X", got[i]);
	if (strncmp(hex, head, strlen(head)) != 0 || strcmp(hex + 2 * n - strlen(tail), tail) != 0)
		test_fail(TEST, "cut: got %.80s...%s", hex, hex + 2 * n - strlen(tail));
}

static void test_ids(void)
{
	int id;

	CHECK(RANDO_TEXT_COUNT == 408);
	for (id = 0; id < RANDO_TEXT_COUNT; id++)
		CHECK(rando_text_find(rando_text_name((RandoTextId)id)) == (RandoTextId)id);
	CHECK(strcmp(rando_text_name(RANDO_TEXT_set_cursor), "set_cursor") == 0);
	CHECK(strcmp(rando_text_name(RANDO_TEXT_terminator), "terminator") == 0);
	CHECK(rando_text_find("no_such_text") == RANDO_TEXT_NONE);
	CHECK(rando_text_find("") == RANDO_TEXT_NONE);
}

/* Two of VT's raw entries, an id removeUnwanted() blanks, and an en() string through the encoder. */
static void test_defaults(void)
{
	static const uint8_t terminator[] = {0xFF, 0xFF};
	static const uint8_t choice_1_2[] = {0xFB, 0xFC, 0x00, 0xF7, 0x99, 0xF8, 0xFF, 0xFE, 0x72};
	static const uint8_t notext[] = {0xFB, 0xFE, 0x6E, 0x00, 0xFE, 0x6B, 0x04};
	uint8_t got[RANDO_TEXT_ENTRY_MAX], want[RANDO_TEXT_ENTRY_MAX];
	size_t n;

	n = rando_text_default(RANDO_TEXT_terminator, got);
	CHECK(n == sizeof terminator && memcmp(got, terminator, n) == 0);
	n = rando_text_default(RANDO_TEXT_choice_1_2, got);
	CHECK(n == sizeof choice_1_2 && memcmp(got, choice_1_2, n) == 0);
	/* removeUnwanted() blanks it. */
	n = rando_text_default(RANDO_TEXT_item_get_lamp, got);
	CHECK(n == sizeof notext && memcmp(got, notext, n) == 0);
	n = rando_text_default(RANDO_TEXT_uncle_leaving_text, got);
	CHECK(n == rando_text_encode("I'm just going out for a pack of smokes.", true, want) &&
	      memcmp(got, want, n) == 0);
}

typedef struct Recorder {
	int calls;
	int text_calls;
	int credits_calls;
	int text_at;
	bool text_after_credits;
	uint32_t address;
	size_t count;
	uint8_t bytes[RANDO_TEXT_TABLE_MAX];
	bool refuse_text;
	int calls_after_refusal;
	bool refused;
} Recorder;

static bool record(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	Recorder *r = ctx;

	if (r->refused) {
		r->calls_after_refusal++;
		return true;
	}
	r->calls++;
	if (strcmp(method, "writeCredits") == 0) {
		r->credits_calls++;
		if (r->text_calls > 0)
			r->text_after_credits = false;
	}
	if (strcmp(method, "writeText") != 0)
		return true;
	r->text_calls++;
	r->text_at = r->calls;
	r->text_after_credits = r->credits_calls == 2;
	r->address = address;
	r->count = count;
	if (count <= sizeof r->bytes)
		memcpy(r->bytes, bytes, count);
	if (r->refuse_text) {
		r->refused = true;
		return false;
	}
	return true;
}

static RandoGen gen, before;
static Recorder rec;

static void test_group_shape(void)
{
	static uint8_t table[RANDO_TEXT_TABLE_MAX];
	RandoRomWriter writer = {record, &rec};
	uint8_t entry[RANDO_TEXT_ENTRY_MAX];
	size_t total = 0, n;
	int id;

	memset(&rec, 0, sizeof rec);
	CHECK(rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.text_calls == 1);
	CHECK(rec.text_after_credits);
	CHECK(rec.address == TEXT_ADDRESS);
	CHECK(rec.address <= ROM_SIZE && rec.count <= ROM_SIZE - rec.address);
	for (id = 0; id < RANDO_TEXT_COUNT; id++) {
		n = rando_text_default((RandoTextId)id, entry);
		CHECK(total + n <= sizeof table);
		memcpy(table + total, entry, n);
		total += n;
	}
	CHECK(rec.count == total && memcmp(rec.bytes, table, total) == 0);
}

static void test_refusal(void)
{
	RandoRomWriter writer = {record, &rec};

	memset(&rec, 0, sizeof rec);
	rec.refuse_text = true;
	CHECK(!rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(rec.text_calls == 1 && rec.calls_after_refusal == 0);
}

static void test_const(void)
{
	RandoRomWriter writer = {record, &rec};

	memcpy(&before, &gen, sizeof gen);
	memset(&rec, 0, sizeof rec);
	CHECK(rando_rom_write(&gen, RANDO_HEART_BEEP_NORMAL, &writer));
	CHECK(memcmp(&before, &gen, sizeof gen) == 0);
}

static int hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static void check_text_default_fixture(void)
{
	Fixture fx;
	uint8_t got[RANDO_TEXT_ENTRY_MAX];
	int id = 0;

	fixture_open(&fx, FIXTURE_TEXT_DEFAULT);
	while (fixture_next(&fx)) {
		char *hex;
		size_t n, i;

		hex = fx.nfields == 1 ? strchr(fx.field[0], ' ') : NULL;
		if (hex == NULL || strchr(hex + 1, ' ') != NULL)
			fixture_fail(&fx, "malformed default");
		*hex++ = '\0';
		if (id >= RANDO_TEXT_COUNT)
			fixture_fail(&fx, "more ids than RANDO_TEXT_COUNT");
		if (strcmp(fx.field[0], rando_text_name((RandoTextId)id)) != 0)
			fixture_fail(&fx, "id out of table order");
		n = rando_text_default((RandoTextId)id, got);
		if (strlen(hex) != 2 * n)
			fixture_fail(&fx, "default length differs");
		for (i = 0; i < n; i++) {
			int hi = hex_value(hex[2 * i]), lo = hex_value(hex[2 * i + 1]);

			if (hi < 0 || lo < 0)
				fixture_fail(&fx, "malformed hex");
			if (got[i] != (uint8_t)(hi << 4 | lo))
				fixture_fail(&fx, "default bytes differ");
		}
		id++;
	}
	fixture_close(&fx);
	if (id != RANDO_TEXT_COUNT)
		test_fail(TEST, "%d text_default records, expected %d", id, RANDO_TEXT_COUNT);
	printf(TEST ": %d text_default records matched\n", id);
}

int main(int argc, char **argv)
{
	RandoOptions options;
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	test_layout();
	test_commands();
	test_wrapping();
	test_characters();
	test_cut();
	test_ids();
	test_defaults();
	rando_options_init(&options);
	CHECK(rando_generate(&gen, &options, 1, 0, NULL, NULL) == RANDO_COMPLETE);
	test_group_shape();
	test_refusal();
	test_const();
	printf(TEST ": passed\n");
	if (fixtures)
		check_text_default_fixture();
	return 0;
}
