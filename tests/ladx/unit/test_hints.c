#include "ladx_groups.h"
#include "ladx_items.h"
#include "ladx_sha1.h"
#include "support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_hints"

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

typedef struct Hashed {
	LadxSha1 sha;
	int writes;
} Hashed;

static int hash_write(void *ctx, unsigned text_id, const uint8_t *bytes, size_t n)
{
	Hashed *h = ctx;
	char head[32];

	sprintf(head, "%x %lu\n", text_id, (unsigned long)n);
	ladx_sha1_update(&h->sha, (const uint8_t *)head, strlen(head));
	ladx_sha1_update(&h->sha, bytes, n);
	h->writes++;
	return 1;
}

#define PLANTED_MAX 80

/*
 * Spot i of the planted list: name id "0x%03X" % (7i mod 13) (ties for the
 * stable sort), item and area cycling over these lists.
 */
static const char *const planted_items[] = {"SWORD",   "HEART_PIECE", "BOW",	 "SONG2",    "RUPEES_20",   "BOWWOW",
					    "ROOSTER", "GOLD_LEAF",   "KEY3", "FACE_KEY", "MAGIC_POWDER"};
static const char *const planted_areas[] = {"Planted Area", "Sam's Cove", "Nowhere Field"};

/*
 * LADXR's own hints.addHints on n planted spots, rnd = random.Random(b"ladx-hints-<k>"),
 * a fake ROM recording texts: per write `<id hex> <len>\n<bytes>` hashed, then rnd.random().hex().
 */
static void test_planted(size_t n, int k, const char *want, double next)
{
	static char ids[PLANTED_MAX][8];
	LadxHintSpot spots[PLANTED_MAX];
	uint8_t digest[LADX_SHA1_SIZE], seed[16];
	char hex[2 * LADX_SHA1_SIZE + 1], what[96];
	LadxOutcome raised;
	LadxRng rnd;
	Hashed h;
	size_t i;

	for (i = 0; i < n; i++) {
		sprintf(ids[i], "0x%03X", (unsigned)((i * 7) % 13));
		spots[i].name_id = ids[i];
		spots[i].item = ladx_item_find(planted_items[i % (sizeof planted_items / sizeof planted_items[0])]);
		spots[i].area = planted_areas[i % 3];
	}
	sprintf((char *)seed, "ladx-hints-%d", k);
	ladx_rng_init(&rnd, LADX_RNG_RND, NULL, NULL);
	ladx_rng_seed(&rnd, LADX_STAGE_RANDOMIZER_INIT, seed, strlen((char *)seed));
	ladx_sha1_init(&h.sha);
	h.writes = 0;
	snprintf(what, sizeof what, "%lu planted spots run", (unsigned long)n);
	check(ladx_hints_add(&rnd, spots, n, hash_write, &h, &raised) == LADX_HINTS_OK, what);
	check(h.writes == LADX_HINT_WRITE_COUNT, "43 texts written");
	ladx_sha1_final(&h.sha, digest);
	hex_of(digest, LADX_SHA1_SIZE, hex);
	snprintf(what, sizeof what, "%lu planted spots: texts as LADXR writes them", (unsigned long)n);
	if (strcmp(hex, want) != 0)
		test_fail(TEST, "%s: SHA-1 %s, expected %s", what, hex, want);
	check(ladx_rng_random(&rnd, LADX_STAGE_RANDOMIZER_INIT) == next, "as many words drawn as LADXR");
}

#define TABLES                                                                                                    \
	"texts entities rooms_overworld_top rooms_overworld_bottom rooms_indoor_a rooms_indoor_b "               \
	"rooms_color_dungeon room_sprite_data_overworld room_sprite_data_indoor background_tiles "               \
	"background_attributes"
#define LOG_MAX 65536

static LadxTables tables;
static LadxEncoder enc;
static LadxWlog wlog;
static LadxRandomizer randomizer, copy;
static LadxPipeline pipeline;
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
				      "C marin - patches.aesthetics.reduceMessageLengths(rom, rnd)\n"
				      "C hints - hints.addHints(rom, rnd, logic.iteminfo_list)\n";

/* The case seed of tests/ladx/cases/baseline. */
static const uint8_t baseline_seed[16] = {0xe8, 0x51, 0x5e, 0x70, 0x64, 0xfb, 0x5f, 0xd6,
					  0x45, 0x8e, 0x72, 0xd5, 0x61, 0x22, 0x67, 0x3b};

static LadxPipelineStatus run(int with_rngs, LadxPipelineError *err)
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
	if (ladx_randomize(&randomizer, &settings, baseline_seed, 16, NULL, NULL) != LADX_RANDOMIZE_OK ||
	    !ladx_randomizer_placement(&randomizer))
		test_fail(TEST, "the baseline run fills");
	log_len = 0;
	log_text[0] = '\0';
	if (ladx_wlog_begin(&wlog, span(TABLES), sink, NULL) != LADX_WLOG_OK ||
	    ladx_enc_init(&enc, NULL, 0, NULL, NULL, &wlog) != LADX_ENC_OK)
		test_fail(TEST, "log and encoder begin");
	memset(&pipeline, 0, sizeof pipeline);
	pipeline.tables = &tables;
	pipeline.settings = &settings;
	pipeline.randomizer = &randomizer;
	pipeline.rnd = with_rngs ? &randomizer.rnd : NULL;
	pipeline.global = with_rngs ? &randomizer.global : NULL;
	pipeline.groups = ladx_pipeline_groups(&count);
	pipeline.group_count = count;
	return ladx_pipeline_run(&pipeline, &enc, err);
}

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

typedef struct Expected {
	int count, mismatches;
} Expected;

static int expect_write(void *ctx, unsigned text_id, const uint8_t *bytes, size_t n)
{
	static char want[2 * 2048 + 1], got[2 * 2048 + 1];
	Expected *e = ctx;

	hex_of(bytes, n, want);
	if (strcmp(record(text_id, got, sizeof got), want) != 0)
		e->mismatches++;
	e->count++;
	return 1;
}

static void test_group(void)
{
	static LadxHintSpot spots[LADX_LOGIC_SPOT_CAP];
	static const char *const head = "ladx-rom-writes 1\ntables " TABLES "\nG hints\n";
	uint8_t bytes[2048], book[2048];
	char want[2 * 2048 + 1], got[2 * 2048 + 1];
	const LadxLogic *logic;
	LadxPipelineError err;
	LadxOutcome raised;
	Expected e;
	const char *at;
	size_t n, book_n;
	int i, count, records = 0, books = 0;

	check(run(1, &err) == LADX_PIPELINE_OK, "baseline without the ROM: the pipeline runs hints");
	check(strncmp(log_text, head, strlen(head)) == 0, "one hints segment, marin writing nothing without the ROM");
	for (at = log_text; (at = strstr(at, "\nT texts ")) != NULL; at++)
		records++;
	check(records == LADX_HINT_WRITE_COUNT, "43 texts written");
	check(ladx_format_text("Read this book?", 15, 0, "YES  NO", book, sizeof book, &book_n, &raised) == LADX_TEXT_OK,
	      "the book text formats");
	hex_of(book, book_n, want);
	for (i = 0x200; i < 0x20C; i += 2)
		books += strcmp(record((unsigned)i, got, sizeof got), want) == 0;
	check(books == 6, "books 0x200 to 0x20A ask YES  NO");

	ladx_randomize(&copy, pipeline.settings, baseline_seed, 16, NULL, NULL);
	ladx_marin_starting_text(&copy.rnd, &copy.global, bytes, sizeof bytes, &n, &raised);
	logic = ladx_randomizer_placement(&copy);
	count = ladx_logic_spot_count(logic);
	for (i = 0; i < count; i++) {
		spots[i].name_id = ladx_logic_spot(logic, i)->name_id;
		spots[i].item = ladx_logic_spot(logic, i)->item;
		spots[i].area = ladx_spot_metadata_area(ladx_logic_spot(logic, i));
	}
	memset(&e, 0, sizeof e);
	check(ladx_hints_add(&copy.rnd, spots, (size_t)count, expect_write, &e, &raised) == LADX_HINTS_OK,
	      "the hints function runs on the copy's placement");
	check(e.count == LADX_HINT_WRITE_COUNT && e.mismatches == 0,
	      "every text of the log is the hints function's on the placed spots, after marin's draws");
	check(memcmp(&copy.rnd.py, &randomizer.rnd.py, sizeof copy.rnd.py) == 0 &&
		      memcmp(&copy.global.py, &randomizer.global.py, sizeof copy.global.py) == 0,
	      "the group drew from the randomizer's own rnd, after marin's");
	{
		unsigned prev = 0, id;
		int sorted = 1;

		for (at = log_text; (at = strstr(at, "\nT texts ")) != NULL; at++) {
			id = (unsigned)strtoul(at + 9, NULL, 16);
			sorted &= id > prev;
			prev = id;
		}
		check(sorted, "the segment's net texts by id, as the encoder ends a segment");
	}
}

static void test_failures(void)
{
	LadxPipelineError err;

	check(run(0, &err) == LADX_PIPELINE_GROUP, "without the generators, the walk fails");
}

int main(void)
{
	test_planted(80, 1, "a37598e338052386a4a5520dbae759914d29d35f", 0x1.e12efe4af186fp-1);
	test_planted(10, 2, "b082b778ab7bb7436a7353efc378258f256bdba0", 0x1.e2b3107c0304ap-2);
	test_group();
	test_failures();
	printf("%s: addHints on planted spots, the group on the baseline seed (%d checks)\n", TEST, checks);
	return 0;
}
