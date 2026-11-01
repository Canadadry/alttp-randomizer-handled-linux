#include "rando.h"
#include "rando_case.h"
#include "rando_rom.h"
#include "rando_text.h"
#include "support.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_rom_text"
#define FIXTURE_TEXT_DEFAULT "oracle/text_default.txt"
#define FIXTURE_TEXT_TABLES "oracle/text_tables.txt"
#define ORACLE_WATCHDOG 1000u
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
		n = rando_text_seed(&gen, (RandoTextId)id, entry, NULL);
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

static RandoGen edited;

/* The seed's text of id in g is utf8 through the encoder; the encoder is proven on its own above. */
static void expect_seed(const char *what, const RandoGen *g, RandoTextId id, const char *utf8, bool pause)
{
	uint8_t want[RANDO_TEXT_ENTRY_MAX], got[RANDO_TEXT_ENTRY_MAX];
	size_t nwant, ngot;
	bool set = false;

	nwant = rando_text_encode(utf8, pause, want);
	ngot = rando_text_seed(g, id, got, &set);
	if (!set)
		test_fail(TEST, "%s: %s not seed-set", what, rando_text_name(id));
	if (ngot != nwant || memcmp(got, want, nwant) != 0)
		test_fail(TEST, "%s: %s differs", what, rando_text_name(id));
}

static void expect_unset(const char *what, const RandoGen *g, RandoTextId id)
{
	uint8_t want[RANDO_TEXT_ENTRY_MAX], got[RANDO_TEXT_ENTRY_MAX];
	size_t nwant, ngot;
	bool set = true;

	nwant = rando_text_default(id, want);
	ngot = rando_text_seed(g, id, got, &set);
	if (set)
		test_fail(TEST, "%s: %s seed-set", what, rando_text_name(id));
	if (ngot != nwant || memcmp(got, want, nwant) != 0)
		test_fail(TEST, "%s: %s is not its default", what, rando_text_name(id));
}

/* First and last alternative of each string file, typed from VT's strings/<file>.txt. */
static void test_alternatives(void)
{
	memcpy(&edited, &gen, sizeof gen);
	expect_unset("no setter", &edited, RANDO_TEXT_set_cursor);
	expect_unset("no setter", &edited, RANDO_TEXT_uncle_dying_sewer);
	edited.post.texts.uncle = 0;
	expect_seed("uncle first", &edited, RANDO_TEXT_uncle_leaving_text, "We're out of\nWeetabix. To\nthe store!", true);
	edited.post.texts.uncle = RANDO_STRINGS_UNCLE - 1;
	expect_seed("uncle last", &edited, RANDO_TEXT_uncle_leaving_text, "Off to buy us\na second bed!\nLater!", true);
	edited.post.texts.blind = 0;
	expect_seed("blind first", &edited, RANDO_TEXT_blind_by_the_light, "I hate insect\npuns, they\nreally bug me.", true);
	edited.post.texts.blind = RANDO_STRINGS_BLIND - 1;
	expect_seed("blind last", &edited, RANDO_TEXT_blind_by_the_light, "Ice Trinexx is\npolar-icing", true);
	edited.post.texts.tavern_man = 0;
	expect_seed("tavern first", &edited, RANDO_TEXT_kakariko_tavern_fisherman,
		    "What do you\ncall a blind\ndinosaur?\na doyouthink-\nhesaurus.", true);
	edited.post.texts.tavern_man = RANDO_STRINGS_TAVERN_MAN - 1;
	expect_seed("tavern last", &edited, RANDO_TEXT_kakariko_tavern_fisherman,
		    "Hardhat Beetle\nwon't\nLet It Be?\nTell it to Get\nBack or give\nit a Ticket to\nRide down\na hole!", true);
	edited.post.texts.ganon_1 = 0;
	expect_seed("ganon_1 first", &edited, RANDO_TEXT_ganon_fall_in,
		    "Start your day\nsmiling with a\ndelicious\nwhole grain\nbreakfast\ncreated for\nyour incredible\ninsides.",
		    true);
	edited.post.texts.ganon_1 = RANDO_STRINGS_GANON_1 - 1;
	expect_seed("ganon_1 last", &edited, RANDO_TEXT_ganon_fall_in,
		    "@:\nHello there\n\n...\n\n\nGanon:\nGeneral Kenobi\n", true);
	edited.post.texts.triforce = 0;
	expect_seed("triforce first", &edited, RANDO_TEXT_end_triforce, "{NOBORDER}\n\n     G G", true);
	edited.post.texts.triforce = RANDO_STRINGS_TRIFORCE - 1;
	expect_seed("triforce last", &edited, RANDO_TEXT_end_triforce, "{NOBORDER}\nBreakfast\nis served!", true);
	expect_seed("fixed", &edited, RANDO_TEXT_ganon_phase_3_alt, "Got wax in\nyour ears?\nI cannot die!", true);
}

/* Swaps item, wherever the world holds it, with what location holds. */
static void move_item(RandoGen *g, RandoItemId item, RandoLocationId location)
{
	RandoItemId there = rando_world_item(&g->world, location);
	int l;

	for (l = 0; l < RANDO_LOCATION_COUNT; l++)
		if (rando_world_item(&g->world, (RandoLocationId)l) == item) {
			CHECK(rando_world_set_item(&g->world, (RandoLocationId)l, there));
			CHECK(rando_world_set_item(&g->world, location, item));
			return;
		}
	test_fail(TEST, "no location holds the item");
}

static void test_regions(void)
{
	memcpy(&edited, &gen, sizeof gen);
	move_item(&edited, RANDO_ITEM_PendantOfCourage, RANDO_LOC_PALACE_OF_DARKNESS_PRIZE);
	move_item(&edited, RANDO_ITEM_Crystal5, RANDO_LOC_TOWER_OF_HERA_PRIZE);
	move_item(&edited, RANDO_ITEM_Crystal6, RANDO_LOC_EASTERN_PALACE_PRIZE);
	expect_seed("pendant region", &edited, RANDO_TEXT_sahasrahla_bring_courage,
		    "Want something\nfor free? Go\nearn the green\npendant in\nDark Palace\nand I'll give\nyou something.",
		    true);
	expect_seed("crystal regions", &edited, RANDO_TEXT_bomb_shop,
		    "bring me the\ncrystals from\nTower Of Hera\nand\nEastern Palace\nso I can make\na big bomb!", true);
}

static void test_bow_hint(void)
{
	static const char *const light = "Did you find\nthe arrows in\nLight World";
	static const char *const tower = "Did you find\nthe arrows in\nMy tower?";
	RandoTexts *t = &edited.post.texts;

	memcpy(&edited, &gen, sizeof gen);
	t->bow_hint = RANDO_BOW_HINT_ALTERNATE;
	t->bow_locations[0] = RANDO_LOC_GANONS_TOWER_BOBS_TORCH;
	t->bow_locations[1] = RANDO_LOC_SAHASRAHLA;
	expect_seed("first bow in the tower", &edited, RANDO_TEXT_ganon_phase_3_no_silvers, tower, true);
	expect_seed("second bow", &edited, RANDO_TEXT_ganon_phase_3_no_silvers_alt, light, true);
	t->bow_locations[0] = RANDO_LOC_SAHASRAHLA;
	t->bow_locations[1] = RANDO_LOC_GANONS_TOWER_BOBS_TORCH;
	expect_seed("first bow", &edited, RANDO_TEXT_ganon_phase_3_no_silvers, light, true);
	expect_seed("second bow in the tower", &edited, RANDO_TEXT_ganon_phase_3_no_silvers_alt, tower, true);

	t->bow_hint = RANDO_BOW_HINT_SILVERS;
	t->silvers_location = RANDO_LOC_GANONS_TOWER_BOBS_TORCH;
	expect_seed("silvers in the tower", &edited, RANDO_TEXT_ganon_phase_3_no_silvers, tower, true);
	expect_seed("silvers in the tower", &edited, RANDO_TEXT_ganon_phase_3_no_silvers_alt, tower, true);
	t->silvers_location = RANDO_LOC_SAHASRAHLA;
	expect_seed("silvers", &edited, RANDO_TEXT_ganon_phase_3_no_silvers, light, true);
	expect_seed("silvers", &edited, RANDO_TEXT_ganon_phase_3_no_silvers_alt, light, true);

	t->bow_hint = RANDO_BOW_HINT_FAKE;
	t->fake_silvers = 0;
	expect_seed("fake first", &edited, RANDO_TEXT_ganon_phase_3_no_silvers,
		    "Did you find\nthe arrows on\nPlanet Zebes?", true);
	expect_seed("fake first", &edited, RANDO_TEXT_ganon_phase_3_no_silvers_alt,
		    "Did you find\nthe arrows on\nPlanet Zebes?", true);
	t->fake_silvers = RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS - 1;
	expect_seed("fake last", &edited, RANDO_TEXT_ganon_phase_3_no_silvers,
		    "No, painting\narrows silver\nwon't work.", true);
	edited.world.config.item_pool = RANDO_ITEM_POOL_CROWD_CONTROL;
	expect_seed("crowd control", &edited, RANDO_TEXT_ganon_phase_3_no_silvers,
		    "Chat said no\nto Silvers.\nIt's over Hero", true);
	expect_seed("crowd control", &edited, RANDO_TEXT_ganon_phase_3_no_silvers_alt,
		    "Chat said no\nto Silvers.\nIt's over Hero", true);
}

static void test_tower_sign(void)
{
	memcpy(&edited, &gen, sizeof gen);
	edited.world.config.crystals_tower = 7;
	expect_unset("seven crystals", &edited, RANDO_TEXT_sign_ganons_tower);
	edited.world.config.crystals_tower = 1;
	expect_seed("one crystal", &edited, RANDO_TEXT_sign_ganons_tower, "You need 1 crystal to enter.", true);
	edited.world.config.crystals_tower = 0;
	expect_seed("no crystal", &edited, RANDO_TEXT_sign_ganons_tower, "You need 0 crystals to enter.", true);
	edited.world.config.crystals_tower = 6;
	expect_seed("six crystals", &edited, RANDO_TEXT_sign_ganons_tower, "You need 6 crystals to enter.", true);
}

static void expect_goal(RandoGoal goal, int crystals, int required, const char *sign, const char *fall_in)
{
	memcpy(&edited, &gen, sizeof gen);
	edited.world.config.goal = goal;
	edited.world.config.crystals_ganon = crystals;
	edited.world.config.item_goal_required = required;
	expect_seed(sign, &edited, RANDO_TEXT_sign_ganon, sign, true);
	expect_seed(sign, &edited, RANDO_TEXT_ganon_fall_in_alt, fall_in, true);
	if (goal != RANDO_GOAL_TRIFORCE_HUNT)
		expect_unset(sign, &edited, RANDO_TEXT_murahdahla);
}

static void test_goals(void)
{
	static const char *const dingus = "You think you\nare ready to\nface me?\n\nI will not die\n\nunless you\ncomplete your\n"
					  "goals. Dingus!";

	expect_goal(RANDO_GOAL_GANON, 7, 0,
		    "To beat Ganon you must collect 7 Crystals and defeat his minion at the top of his tower.", dingus);
	expect_goal(RANDO_GOAL_GANON, 1, 0,
		    "To beat Ganon you must collect 1 Crystal and defeat his minion at the top of his tower.", dingus);
	expect_goal(RANDO_GOAL_FAST_GANON, 1, 0, "You need 1 Crystal to beat Ganon.", dingus);
	expect_goal(RANDO_GOAL_FAST_GANON, 0, 0, "You need 0 Crystals to beat Ganon.", dingus);
	expect_goal(RANDO_GOAL_GANONHUNT, 1, 25, "To beat Ganon you must collect 25 Triforce Pieces.", dingus);
	expect_goal(RANDO_GOAL_PEDESTAL, 7, 0, "You need to get to the pedestal... Dingus!",
		    "You cannot\nkill me. You\nshould go for\nyour real goal\nIt's on the\npedestal.\n\nYou dingus!\n");
	expect_goal(RANDO_GOAL_DUNGEONS, 7, 0, "You need to defeat all of Ganon's bosses.", dingus);
	expect_goal(RANDO_GOAL_COMPLETIONIST, 7, 0, "You need to collect EVERY item and defeat EVERY boss.", dingus);
	expect_goal(RANDO_GOAL_TRIFORCE_HUNT, 7, 20, "Go find the Triforce pieces... Dingus!",
		    "So you thought\nyou could come\nhere and beat\nme? I have\nhidden the\nTriforce\npieces well.\n"
		    "Without them,\nyou can't win!");
	expect_seed("murahdahla", &edited, RANDO_TEXT_murahdahla,
		    "Hello @. I\nam Murahdahla, brother of\nSahasrahla and Aginah. Behold the power of\ninvisibility.\n\n\n\n"
		    "\xE2\x80\xA6 \xE2\x80\xA6 \xE2\x80\xA6\n\nWait! you can see me? I knew I should have\n"
		    "hidden in  a hollow tree. If you bring\n20 triforce pieces, I can reassemble it.",
		    true);
}

static void test_hints(void)
{
	RandoHints *h = &edited.post.hints;
	int tile;

	memcpy(&edited, &gen, sizeof gen);
	edited.world.config.spoil_hints = false;
	memset(h, 0, sizeof *h);
	expect_seed("hints off", &edited, RANDO_TEXT_sign_north_of_links_house, "Randomizer v31\n\n>    -veetorp", true);
	expect_unset("hints off", &edited, RANDO_TEXT_telepathic_tile_eastern_palace);
	expect_unset("hints off", &edited, RANDO_TEXT_telepathic_tile_south_east_darkworld_cave);

	edited.world.config.spoil_hints = true;
	h->on = true;
	for (tile = 0; tile < RANDO_TILE_COUNT; tile++)
		h->tiles[tile].kind = RANDO_TILE_HINT_JOKE;
	h->tiles[RANDO_TILE_SOUTH_EAST_DARKWORLD_CAVE].joke = RANDO_STRINGS_HINT - 1;
	expect_unset("hints on", &edited, RANDO_TEXT_sign_north_of_links_house);
	expect_seed("joke first", &edited, RANDO_TEXT_telepathic_tile_eastern_palace,
		    "It\xE2\x80\x99s a secret\nto everybody.\n            >", true);
	expect_seed("joke last", &edited, RANDO_TEXT_telepathic_tile_south_east_darkworld_cave,
		    "The hobo's\nfire sheds\nno light.  >", true);
	expect_unset("no tile", &edited, RANDO_TEXT_telepathic_tile_under_ganon);
}

static void set_location_tile(RandoGen *g, RandoTile tile, RandoItemId item, int item_name, RandoLocationId location,
			      int location_name)
{
	RandoTileHint *hint = &g->post.hints.tiles[tile];

	hint->kind = RANDO_TILE_HINT_LOCATION;
	hint->location.item = item;
	hint->location.item_name = (uint8_t)item_name;
	hint->location.location = location;
	hint->location.location_name = (uint8_t)location_name;
}

/* Names typed from VT's resources/lang/en/hint.php. */
static void test_location_hints(void)
{
	int tile;

	memcpy(&edited, &gen, sizeof gen);
	edited.world.config.spoil_hints = true;
	edited.post.hints.on = true;
	for (tile = 0; tile < RANDO_TILE_COUNT; tile++)
		edited.post.hints.tiles[tile].kind = RANDO_TILE_HINT_JOKE;
	set_location_tile(&edited, RANDO_TILE_CASTLE_TOWER, RANDO_ITEM_FireRod, 0, RANDO_LOC_SAHASRAHLA, 1);
	expect_seed("ucfirst, both arrays", &edited, RANDO_TEXT_telepathic_tile_castle_tower,
		    "The Fire Rod is held by the Kakariko village elder", true);
	set_location_tile(&edited, RANDO_TILE_CASTLE_TOWER, RANDO_ITEM_FireRod, 1, RANDO_LOC_ETHER_TABLET, 0);
	expect_seed("second item name", &edited, RANDO_TEXT_telepathic_tile_castle_tower, "A unique item is on a Tablet",
		    true);
	set_location_tile(&edited, RANDO_TILE_TURTLE_ROCK, RANDO_ITEM_KeyGK, 0, RANDO_LOC_ETHER_TABLET, 1);
	expect_seed("plain item name", &edited, RANDO_TEXT_telepathic_tile_turtle_rock,
		    "A Generic Key requires the book", true);
	set_location_tile(&edited, RANDO_TILE_TURTLE_ROCK, RANDO_ITEM_ProgressiveBowAlternate, 0, RANDO_LOC_BOMB_MERCHANT, 0);
	expect_seed("missing keys", &edited, RANDO_TEXT_telepathic_tile_turtle_rock,
		    "Hint.item.ProgressiveBowAlternate hint.location.Bomb Merchant", true);
}

static void expect_override(const char *what, RandoLocationId location, RandoItemId item, RandoTextId id,
			    const char *utf8)
{
	CHECK(rando_world_set_item(&edited.world, location, item));
	expect_seed(what, &edited, id, utf8, true);
}

/* Texts typed from VT's Drop\Ether, Drop\Bombos and Pedestal at 219fcafd. */
static void test_item_texts(void)
{
	memcpy(&edited, &gen, sizeof gen);
	edited.world.config.rom_generic_keys = false;
	expect_override("ether", RANDO_LOC_ETHER_TABLET, RANDO_ITEM_Bombos, RANDO_TEXT_tablet_ether_book,
			"Burn, baby,\nburn! Fear my\nring of fire!");
	expect_override("ether alias", RANDO_LOC_ETHER_TABLET, RANDO_ITEM_ShopArrow, RANDO_TEXT_tablet_ether_book,
			"a lonely arrow\nsits here.");
	expect_override("ether default", RANDO_LOC_ETHER_TABLET, RANDO_ITEM_Crystal1, RANDO_TEXT_tablet_ether_book,
			"Don't waste\nyour time!");
	expect_override("ether empty", RANDO_LOC_ETHER_TABLET, RANDO_ITEM_NONE, RANDO_TEXT_tablet_ether_book,
			"Don't waste\nyour time!");
	expect_override("bombos", RANDO_LOC_BOMBOS_TABLET, RANDO_ITEM_KeyD1, RANDO_TEXT_tablet_bombos_book,
			"A small key\nthat steals\nlight");
	expect_override("pedestal", RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_BigKeyD4,
			RANDO_TEXT_mastersword_pedestal_translated, "The Big Key\nof rogues");
	expect_override("pedestal master sword", RANDO_LOC_MASTER_SWORD_PEDESTAL, RANDO_ITEM_MasterSword,
			RANDO_TEXT_mastersword_pedestal_translated, "I thought this\nwas meant to\nbe randomized?");
	edited.world.config.rom_generic_keys = true;
	expect_override("generic key", RANDO_LOC_BOMBOS_TABLET, RANDO_ITEM_KeyD1, RANDO_TEXT_tablet_bombos_book,
			"A small key\nto the Kingdom");
	expect_override("generic big key", RANDO_LOC_BOMBOS_TABLET, RANDO_ITEM_BigKeyD1, RANDO_TEXT_tablet_bombos_book,
			"Hammeryump\nwith this\nbig key");
}

static void test_inverted(void)
{
	static const RandoTextId ids[] = {
		RANDO_TEXT_sign_path_to_death_mountain, RANDO_TEXT_sign_bumper_cave, RANDO_TEXT_sign_east_of_bomb_shop,
		RANDO_TEXT_sign_east_of_links_house, RANDO_TEXT_kiki_leaving_screen, RANDO_TEXT_dark_sanctuary,
		RANDO_TEXT_dark_sanctuary_yes, RANDO_TEXT_dark_sanctuary_no, RANDO_TEXT_menu_start_2,
		RANDO_TEXT_menu_start_3, RANDO_TEXT_intro_main,
	};
	size_t i;

	memcpy(&edited, &gen, sizeof gen);
	CHECK(edited.world.config.mode_state != RANDO_STATE_INVERTED);
	for (i = 0; i < sizeof ids / sizeof ids[0]; i++)
		expect_unset("not inverted", &edited, ids[i]);
	edited.world.config.mode_state = RANDO_STATE_INVERTED;
	expect_seed("inverted", &edited, RANDO_TEXT_sign_path_to_death_mountain,
		    "\xE2\x86\x92 Bumper Cave\nYou need Cape and Mirror, but not Hookshot", true);
	expect_seed("inverted", &edited, RANDO_TEXT_sign_bumper_cave, "Cave to lost, old man.\nGood luck.", true);
	expect_seed("inverted", &edited, RANDO_TEXT_sign_east_of_bomb_shop, "\n\xE2\x86\x90 Your House", true);
	expect_seed("inverted", &edited, RANDO_TEXT_sign_east_of_links_house, "\n\xE2\x86\x90 Bomb Shoppe", true);
	expect_seed("inverted", &edited, RANDO_TEXT_kiki_leaving_screen, "{NOTEXT}", false);
	expect_seed("inverted", &edited, RANDO_TEXT_dark_sanctuary, "{NOTEXT}", false);
	expect_seed("inverted", &edited, RANDO_TEXT_dark_sanctuary_yes, "{NOTEXT}", false);
	expect_seed("inverted", &edited, RANDO_TEXT_dark_sanctuary_no,
		    "If you want that healing you're gonna need 20 rupees.", true);
	expect_seed("inverted", &edited, RANDO_TEXT_menu_start_2,
		    "{MENU}\n{SPEED0}\n\xE2\x89\xA5@'s House\n Dark Chapel\n{CHOICE3}", false);
	expect_seed("inverted", &edited, RANDO_TEXT_menu_start_3,
		    "{MENU}\n{SPEED0}\n\xE2\x89\xA5@'s House\n Dark Chapel\n Dark Mountain\n{CHOICE2}", false);
	expect_seed("inverted", &edited, RANDO_TEXT_intro_main,
		    "{INTRO}\n Episode  III\n{PAUSE3}\n A Link to\n   the Past\n"
		    "{PAUSE3}\nInverted\n  Randomizer\n{PAUSE3}\nAfter mostly disregarding what happened in the first two games,\n"
		    "{PAUSE3}\nLink has been transported to the Dark World\n{PAUSE3}\nWhile he was slumbering,\n"
		    "{PAUSE3}\nWhatever will happen?\n{PAUSE3}\n{CHANGEPIC}\nGanon has moved around all the items in Hyrule.\n"
		    "{PAUSE7}\nYou will have to find all the items necessary to beat Ganon.\n"
		    "{PAUSE7}\nThis is your chance to be a hero.\n{PAUSE3}\n{CHANGEPIC}\n"
		    "You must get the 7 crystals to beat Ganon.\n{PAUSE9}\n{CHANGEPIC}",
		    false);
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

/* Splits a space-separated record in place; returns the field count, at most max. */
static int split_spaces(char *line, char **field, int max)
{
	int n = 0;

	while (*line != '\0' && n < max) {
		field[n++] = line;
		line = strchr(line, ' ');
		if (line == NULL)
			break;
		*line++ = '\0';
	}
	return n;
}

/* Whether hex is exactly bytes[0..n). */
static bool hex_equals(const char *hex, const uint8_t *bytes, size_t n)
{
	size_t i;

	if (strlen(hex) != 2 * n)
		return false;
	for (i = 0; i < n; i++) {
		int hi = hex_value(hex[2 * i]), lo = hex_value(hex[2 * i + 1]);

		if (hi < 0 || lo < 0 || bytes[i] != (uint8_t)(hi << 4 | lo))
			return false;
	}
	return true;
}

static void print_hex(const char *label, const uint8_t *bytes, size_t n)
{
	size_t i;

	fprintf(stderr, "%s ", label);
	for (i = 0; i < n; i++)
		fprintf(stderr, "%02X", bytes[i]);
	fprintf(stderr, "\n");
}

typedef struct PickFile {
	const char *file;
	RandoTextId id;
	int count;
	int seen;
} PickFile;

/* Sets the choice that makes pick's file give its index-th alternative. */
static void apply_pick(RandoGen *g, const char *file, int index)
{
	if (strcmp(file, "uncle") == 0) {
		g->post.texts.uncle = (uint8_t)index;
	} else if (strcmp(file, "blind") == 0) {
		g->post.texts.blind = (uint8_t)index;
	} else if (strcmp(file, "tavern_man") == 0) {
		g->post.texts.tavern_man = (uint8_t)index;
	} else if (strcmp(file, "ganon_1") == 0) {
		g->post.texts.ganon_1 = (uint8_t)index;
	} else if (strcmp(file, "triforce") == 0) {
		g->post.texts.triforce = (uint8_t)index;
	} else if (strcmp(file, "ganon_phase_3_no_silvers") == 0) {
		g->post.texts.bow_hint = RANDO_BOW_HINT_FAKE;
		g->post.texts.fake_silvers = (uint8_t)index;
		g->world.config.item_pool = RANDO_ITEM_POOL_NORMAL;
	} else {
		int tile;

		g->post.hints.on = true;
		for (tile = 0; tile < RANDO_TILE_COUNT; tile++)
			g->post.hints.tiles[tile].kind = RANDO_TILE_HINT_JOKE;
		g->post.hints.tiles[RANDO_TILE_EASTERN_PALACE].joke = (uint8_t)index;
	}
}

/* Decodes hex into a NUL-terminated string; false when malformed or too long. */
static bool hex_string(const char *hex, char *out, size_t size)
{
	size_t n = strlen(hex), i;

	if (n % 2 != 0 || n / 2 >= size)
		return false;
	for (i = 0; i < n / 2; i++) {
		int hi = hex_value(hex[2 * i]), lo = hex_value(hex[2 * i + 1]);

		if (hi < 0 || lo < 0)
			return false;
		out[i] = (char)(hi << 4 | lo);
	}
	out[n / 2] = '\0';
	return true;
}

/* rando_text_seed's text of g's castle tower tile against VT's getHint string, through the encoder. */
static bool tile_matches(const RandoGen *g, const char *hint)
{
	uint8_t want[RANDO_TEXT_ENTRY_MAX], got[RANDO_TEXT_ENTRY_MAX];
	size_t nwant = rando_text_encode(hint, true, want), ngot;
	bool set;

	ngot = rando_text_seed(g, RANDO_TEXT_telepathic_tile_castle_tower, got, &set);
	return set && ngot == nwant && memcmp(got, want, ngot) == 0;
}

static RandoGen hinted;

/* hint_item <raw name> <index> <HEX>, the raw name holding spaces: named with Bomb Merchant, which
 * hint.php lacks (checked below), so no name is cut at 19 characters. */
static void replay_hint_item(const Fixture *fx, char *record, int *item_seen)
{
	char name[256], hint[512];
	char *hex = strrchr(record, ' '), *index_field;
	RandoItemId item;
	int index;

	if (hex == NULL)
		fixture_fail(fx, "malformed hint_item");
	*hex++ = '\0';
	index_field = strrchr(record, ' ');
	if (index_field == NULL || index_field == record)
		fixture_fail(fx, "malformed hint_item");
	*index_field++ = '\0';
	item = rando_item_find(record);
	if (item == RANDO_ITEM_NONE)
		return;
	index = atoi(index_field);
	if (index != item_seen[item] || !hex_string(hex, name, sizeof name))
		fixture_fail(fx, "hint_item index out of order or malformed name");
	item_seen[item]++;
	set_location_tile(&hinted, RANDO_TILE_CASTLE_TOWER, item, index, RANDO_LOC_BOMB_MERCHANT, 0);
	snprintf(hint, sizeof hint, "%s hint.location.Bomb Merchant", name);
	if (hint[0] >= 'a' && hint[0] <= 'z')
		hint[0] = (char)(hint[0] - 'a' + 'A');
	if (!tile_matches(&hinted, hint))
		fixture_fail(fx, "hint_item name differs");
}

/* hint_location <index> <HEX> <name>, named with KeyGK's plain "a Generic Key". */
static void replay_hint_location(const Fixture *fx, char *record, int *location_seen)
{
	char name[256], hint[512];
	char *hex = strchr(record, ' '), *location_name;
	RandoLocationId location;
	int index;

	location_name = hex == NULL ? NULL : strchr(hex + 1, ' ');
	if (location_name == NULL)
		fixture_fail(fx, "malformed hint_location");
	*hex++ = '\0';
	*location_name++ = '\0';
	location = rando_location_find(location_name);
	if (location == RANDO_LOCATION_NONE)
		fixture_fail(fx, "a location the C world does not know");
	index = atoi(record);
	if (index != location_seen[location] || !hex_string(hex, name, sizeof name))
		fixture_fail(fx, "hint_location index out of order or malformed name");
	location_seen[location]++;
	set_location_tile(&hinted, RANDO_TILE_CASTLE_TOWER, RANDO_ITEM_KeyGK, 0, location, index);
	snprintf(hint, sizeof hint, "A Generic Key %s", name);
	if (!tile_matches(&hinted, hint))
		fixture_fail(fx, "hint_location name differs");
}

/* A count of 0 is a plain string, its one record at index 0. */
static bool count_matches(int alternatives, int seen)
{
	return alternatives == RANDO_HINT_MISSING ? seen == 0 : seen == (alternatives == 0 ? 1 : alternatives);
}

typedef struct ItemTextId {
	RandoTextId id;
	RandoLocationId location;
	int seen;
} ItemTextId;

/* item <id> <raw name> <HEX>: the item set on the id's one location, rom.genericKeys off. */
static void replay_item(const Fixture *fx, char **field, ItemTextId *ids, size_t nids)
{
	uint8_t got[RANDO_TEXT_ENTRY_MAX];
	RandoItemId item = rando_item_find(field[2]);
	size_t i, ngot;
	bool set;

	for (i = 0; i < nids && rando_text_find(field[1]) != ids[i].id; i++)
		;
	if (i == nids)
		fixture_fail(fx, "unknown item text id");
	if (item == RANDO_ITEM_NONE)
		fixture_fail(fx, "an item the C world does not know");
	memcpy(&edited, &gen, sizeof gen);
	edited.world.config.rom_generic_keys = false;
	if (!rando_world_set_item(&edited.world, ids[i].location, item))
		fixture_fail(fx, "the C world refuses the item");
	ngot = rando_text_seed(&edited, ids[i].id, got, &set);
	if (!set || !hex_equals(field[3], got, ngot)) {
		print_hex("C", got, ngot);
		fixture_fail(fx, "item text bytes differ");
	}
	ids[i].seen++;
}

static void check_text_tables_fixture(void)
{
	PickFile files[] = {
		{"uncle", RANDO_TEXT_uncle_leaving_text, RANDO_STRINGS_UNCLE, 0},
		{"blind", RANDO_TEXT_blind_by_the_light, RANDO_STRINGS_BLIND, 0},
		{"tavern_man", RANDO_TEXT_kakariko_tavern_fisherman, RANDO_STRINGS_TAVERN_MAN, 0},
		{"ganon_1", RANDO_TEXT_ganon_fall_in, RANDO_STRINGS_GANON_1, 0},
		{"ganon_phase_3_no_silvers", RANDO_TEXT_ganon_phase_3_no_silvers, RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS, 0},
		{"triforce", RANDO_TEXT_end_triforce, RANDO_STRINGS_TRIFORCE, 0},
		{"hint", RANDO_TEXT_telepathic_tile_eastern_palace, RANDO_STRINGS_HINT, 0},
	};
	const size_t nfiles = sizeof files / sizeof files[0];
	ItemTextId ids[] = {
		{RANDO_TEXT_tablet_ether_book, RANDO_LOC_ETHER_TABLET, 0},
		{RANDO_TEXT_tablet_bombos_book, RANDO_LOC_BOMBOS_TABLET, 0},
		{RANDO_TEXT_mastersword_pedestal_translated, RANDO_LOC_MASTER_SWORD_PEDESTAL, 0},
	};
	const size_t nids = sizeof ids / sizeof ids[0];
	static int item_seen[RANDO_ITEM_COUNT], location_seen[RANDO_LOCATION_COUNT];
	Fixture fx;
	uint8_t got[RANDO_TEXT_ENTRY_MAX];
	int picks = 0, names = 0, i;
	size_t f;

	memcpy(&hinted, &gen, sizeof gen);
	hinted.post.hints.on = true;
	for (i = 0; i < RANDO_TILE_COUNT; i++)
		hinted.post.hints.tiles[i].kind = RANDO_TILE_HINT_JOKE;
	fixture_open(&fx, FIXTURE_TEXT_TABLES);
	while (fixture_next(&fx)) {
		char *field[6];
		int n, index;
		size_t ngot;
		bool set;

		if (fx.nfields != 1)
			fixture_fail(&fx, "malformed record");
		if (strncmp(fx.field[0], "hint_item ", 10) == 0) {
			replay_hint_item(&fx, fx.field[0] + 10, item_seen);
			names++;
			continue;
		}
		if (strncmp(fx.field[0], "hint_location ", 14) == 0) {
			replay_hint_location(&fx, fx.field[0] + 14, location_seen);
			names++;
			continue;
		}
		n = split_spaces(fx.field[0], field, 6);
		if (n == 4 && strcmp(field[0], "item") == 0) {
			replay_item(&fx, field, ids, nids);
			continue;
		}
		if (n < 1 || strcmp(field[0], "pick") != 0)
			fixture_fail(&fx, "unknown record");
		if (n != 5)
			fixture_fail(&fx, "malformed pick");
		for (f = 0; f < nfiles && strcmp(files[f].file, field[2]) != 0; f++)
			;
		if (f == nfiles)
			fixture_fail(&fx, "unknown string file");
		if (rando_text_find(field[1]) != files[f].id)
			fixture_fail(&fx, "pick id is not its file's");
		index = atoi(field[3]);
		if (index != files[f].seen || index >= files[f].count)
			fixture_fail(&fx, "pick index out of order or past the file's count");
		files[f].seen++;
		memcpy(&edited, &gen, sizeof gen);
		apply_pick(&edited, files[f].file, index);
		ngot = rando_text_seed(&edited, files[f].id, got, &set);
		if (!set || !hex_equals(field[4], got, ngot)) {
			print_hex("C", got, ngot);
			fixture_fail(&fx, "pick bytes differ");
		}
		picks++;
	}
	fixture_close(&fx);
	for (f = 0; f < nfiles; f++)
		if (files[f].seen != files[f].count)
			test_fail(TEST, "%s: %d pick records, expected %d", files[f].file, files[f].seen, files[f].count);
	for (f = 0; f < nids; f++)
		if (ids[f].seen != RANDO_ITEM_COUNT)
			test_fail(TEST, "%s: %d item records, expected %d", rando_text_name(ids[f].id), ids[f].seen,
				  RANDO_ITEM_COUNT);
	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		if (!count_matches(rando_hint_item_alternatives((RandoItemId)i), item_seen[i]))
			test_fail(TEST, "hint.item.%s: %d hint_item records, the C count disagrees",
				  rando_item_info((RandoItemId)i)->name, item_seen[i]);
	for (i = 0; i < RANDO_LOCATION_COUNT; i++)
		if (!count_matches(rando_hint_location_alternatives((RandoLocationId)i), location_seen[i]))
			test_fail(TEST, "hint.location.%s: %d hint_location records, the C count disagrees",
				  rando_location_info((RandoLocationId)i)->name, location_seen[i]);
	printf(TEST ": %d text_tables pick, %d item and %d hint name records matched\n", picks,
	       (int)(nids * RANDO_ITEM_COUNT), names);
}

/* The golden's last `text` line per id, against the C seed texts of the case's generation: the same ids,
 * the same bytes. */
static void check_golden_case(const char *dir, const char *name)
{
	static char golden[RANDO_TEXT_COUNT][2 * RANDO_TEXT_ENTRY_MAX + 1];
	bool in_golden[RANDO_TEXT_COUNT] = {false};
	char path[512], error[RANDO_CASE_ERROR_MAX];
	uint8_t got[RANDO_TEXT_ENTRY_MAX];
	RandoCase c;
	Fixture fx;
	uint32_t cap;
	size_t i;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	if (!rando_case_read(path, &c, error))
		test_fail(TEST, "%s", error);
	cap = !c.has_attempts ? ORACLE_WATCHDOG : c.attempts == UINT32_MAX ? 0 : c.attempts + 1;
	if (rando_generate(&edited, &c.options, c.seed, cap, NULL, NULL) != RANDO_COMPLETE)
		test_fail(TEST, "%s: the generation does not complete", path);
	snprintf(path, sizeof path, "%s/%s/rom_buffers.txt", dir, name);
	fixture_open(&fx, path);
	while (fixture_next(&fx)) {
		char *field[3];
		RandoTextId id;

		if (strncmp(fx.field[0], "text ", 5) != 0)
			continue;
		if (fx.nfields != 1 || split_spaces(fx.field[0], field, 3) != 3)
			fixture_fail(&fx, "malformed text line");
		id = rando_text_find(field[1]);
		if (id == RANDO_TEXT_NONE)
			fixture_fail(&fx, "unknown text id");
		if (strlen(field[2]) >= sizeof golden[id])
			fixture_fail(&fx, "text longer than an entry");
		strcpy(golden[id], field[2]);
		in_golden[id] = true;
	}
	fixture_close(&fx);
	for (i = 0; i < RANDO_TEXT_COUNT; i++) {
		RandoTextId id = (RandoTextId)i;
		bool set;
		size_t n = rando_text_seed(&edited, id, got, &set);

		if (set != in_golden[id])
			test_fail(TEST, "%s/%s: %s is %sset by the C but %sin the golden", dir, name, rando_text_name(id),
				  set ? "" : "not ", in_golden[id] ? "" : "not ");
		if (set && !hex_equals(golden[id], got, n)) {
			print_hex("C     ", got, n);
			fprintf(stderr, "golden %s\n", golden[id]);
			test_fail(TEST, "%s/%s: %s differs", dir, name, rando_text_name(id));
		}
	}
}

static int check_golden_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *entry;
	int n = 0;

	if (d == NULL)
		test_fail(TEST, "cannot open %s/", dir);
	while ((entry = readdir(d)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		check_golden_case(dir, entry->d_name);
		n++;
	}
	closedir(d);
	return n;
}

static void check_goldens(void)
{
	int n = check_golden_dir("cases") + check_golden_dir("combos");

	printf(TEST ": %d goldens' text lines matched for every id\n", n);
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
	test_alternatives();
	test_regions();
	test_bow_hint();
	test_tower_sign();
	test_goals();
	test_hints();
	test_location_hints();
	test_item_texts();
	test_inverted();
	printf(TEST ": passed\n");
	if (fixtures) {
		check_text_default_fixture();
		check_text_tables_fixture();
		check_goldens();
	}
	return 0;
}
