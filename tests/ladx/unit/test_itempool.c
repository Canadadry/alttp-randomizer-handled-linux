#include "ladx_itempool.h"
#include "ladx_randomizer.h"
#include "ladx_run_text.h"
#include "ladx_settings.h"
#include "support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_itempool"
#define POOLS "oracle/item_pools.txt"

static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static const char *dict_text(const LadxItemPool *pool)
{
	static char text[8192];
	size_t len = 0;
	int i, item, count;

	text[0] = '\0';
	for (i = 0; ladx_itempool_entry(pool, i, &item, &count); i++)
		len += (size_t)snprintf(text + len, sizeof text - len, i ? " %s=%d" : "%s=%d", ladx_item_name(item),
					count);
	return text;
}

static void check_dict(const LadxItemPool *pool, const char *want, const char *what)
{
	checks++;
	if (strcmp(dict_text(pool), want) != 0)
		test_fail(TEST, "%s: got \"%s\", want \"%s\"", what, dict_text(pool), want);
}

static void test_dict(void)
{
	static LadxItemPool p;
	int item, count;

	ladx_itempool_clear(&p);
	check(ladx_itempool_size(&p) == 0 && !ladx_itempool_entry(&p, 0, &item, &count), "a cleared pool is empty");
	ladx_itempool_add(&p, LADX_ITEM_SWORD, 2);
	ladx_itempool_add(&p, LADX_ITEM_BOMB, 1);
	ladx_itempool_add(&p, LADX_ITEM_SWORD, 1);
	check_dict(&p, "SWORD=3 BOMB=1", "add keeps a key's first position");

	ladx_itempool_add(&p, LADX_ITEM_FEATHER, 0);
	check_dict(&p, "SWORD=3 BOMB=1 FEATHER=0", "add of zero inserts the key at zero");
	check(ladx_itempool_contains(&p, LADX_ITEM_FEATHER), "a zero count is a key");

	ladx_itempool_remove(&p, LADX_ITEM_SWORD, 2);
	check_dict(&p, "SWORD=1 BOMB=1 FEATHER=0", "remove above zero keeps the key");
	ladx_itempool_remove(&p, LADX_ITEM_SWORD, 1);
	check_dict(&p, "BOMB=1 FEATHER=0", "remove to exactly zero deletes the key");
	ladx_itempool_add(&p, LADX_ITEM_SWORD, 1);
	check_dict(&p, "BOMB=1 FEATHER=0 SWORD=1", "a deleted key re-added goes to the end");

	ladx_itempool_remove(&p, LADX_ITEM_BOW, 1);
	check_dict(&p, "BOMB=1 FEATHER=0 SWORD=1 BOW=-1", "remove of an absent key inserts it negative");
	ladx_itempool_remove(&p, LADX_ITEM_BOMB, 3);
	check_dict(&p, "BOMB=-2 FEATHER=0 SWORD=1 BOW=-1", "remove below zero keeps the key");
	ladx_itempool_remove(&p, LADX_ITEM_HOOKSHOT, 0);
	check_dict(&p, "BOMB=-2 FEATHER=0 SWORD=1 BOW=-1", "remove of zero of an absent key leaves it absent");
	ladx_itempool_remove(&p, LADX_ITEM_FEATHER, 0);
	check_dict(&p, "BOMB=-2 SWORD=1 BOW=-1", "remove of zero of a zero key deletes it");

	ladx_itempool_set(&p, LADX_ITEM_BOMB, 5);
	ladx_itempool_set(&p, LADX_ITEM_SHIELD, 0);
	check_dict(&p, "BOMB=5 SWORD=1 BOW=-1 SHIELD=0", "set keeps a key's position, inserts an absent one");
	ladx_itempool_del(&p, LADX_ITEM_SWORD);
	ladx_itempool_del(&p, LADX_ITEM_SWORD);
	check_dict(&p, "BOMB=5 BOW=-1 SHIELD=0", "del removes a key, twice is nothing");
	check(ladx_itempool_get(&p, LADX_ITEM_SWORD) == 0 && ladx_itempool_get(&p, LADX_ITEM_BOW) == -1,
	      "get gives 0 for an absent key, the count otherwise");

	check(!ladx_itempool_add(&p, LADX_ITEM_COUNT, 1) && !ladx_itempool_set(&p, -1, 1) &&
		      !ladx_itempool_remove(&p, LADX_ITEM_NONE, 1) && !ladx_itempool_del(&p, LADX_ITEM_COUNT),
	      "an item outside the table is refused");
	check_dict(&p, "BOMB=5 BOW=-1 SHIELD=0", "a refused item leaves the pool untouched");
}

static int rupee_draws[2];

static void count_rupee_draws(void *ctx, const LadxRngRecord *record)
{
	(void)ctx;
	if (record->stage == LADX_STAGE_ITEMPOOL_RANDOMIZE_RUPEES)
		rupee_draws[record->n == 5 ? 0 : 1]++;
}

static LadxRandomizer r;

/*
 * itempool=pain, seed 000000a2, as LADXR ran it (oracle_run.py): RUPEES_20
 * used up after two picks, so two of __randomizeRupees's seven rupee picks
 * are re-draws on the weights it never updates.
 */
static void test_rupee_redraw(void)
{
	static const uint8_t seed[] = {0x00, 0x00, 0x00, 0xa2};
	const LadxItemPool *pool;
	uint64_t after = 0;
	LadxSettings s;

	ladx_settings_defaults(&s);
	ladx_settings_set(&s, "itempool=pain");
	ladx_settings_validate(&s);
	rupee_draws[0] = rupee_draws[1] = 0;
	check(ladx_randomize(&r, &s, seed, sizeof seed, count_rupee_draws, NULL) == LADX_RANDOMIZE_OK,
	      "pain 000000a2: the run is OK");
	pool = ladx_randomizer_pool(&r, NULL, &after);
	check(pool != NULL, "pain 000000a2: the pool is built");
	check(rupee_draws[0] == 5 && rupee_draws[1] == 7, "pain 000000a2: 5 new items, 7 rupee picks");
	check(after == 26, "pain 000000a2: words_after 26");
	check(!ladx_itempool_contains(pool, LADX_ITEM_RUPEES_20) && ladx_itempool_get(pool, LADX_ITEM_RUPEES_50) == 16,
	      "pain 000000a2: RUPEES_20 deleted, RUPEES_50=16");
	check(ladx_itempool_get(pool, LADX_ITEM_BOMB) == 3 && ladx_itempool_get(pool, LADX_ITEM_MAGIC_POWDER) == 2 &&
		      ladx_itempool_get(pool, LADX_ITEM_SINGLE_ARROW) == 1 &&
		      ladx_itempool_get(pool, LADX_ITEM_ARROWS_10) == 1,
	      "pain 000000a2: BOMB=3 MAGIC_POWDER=2 SINGLE_ARROW=1 ARROWS_10=1");
}

#define TEXT_MAX 8192
#define EXPECT_MAX 8

static char line[TEXT_MAX];
static long line_no;

static int next_line(FILE *f)
{
	size_t len;

	do {
		if (!fgets(line, sizeof line, f))
			return 0;
		line_no++;
		len = strlen(line);
		if (len == 0 || line[len - 1] != '\n')
			test_fail(TEST, "%s line %ld: too long or unterminated", POOLS, line_no);
		line[len - 1] = '\0';
	} while (line[0] == '#');
	return 1;
}

static size_t seed_of(const char *hex, uint8_t *seed, size_t cap)
{
	size_t n = strlen(hex) / 2, i;
	unsigned v;

	if (strlen(hex) % 2 || n > cap)
		test_fail(TEST, "%s line %ld: bad seed %s", POOLS, line_no, hex);
	for (i = 0; i < n; i++) {
		if (sscanf(hex + 2 * i, "%2x", &v) != 1)
			test_fail(TEST, "%s line %ld: bad seed %s", POOLS, line_no, hex);
		seed[i] = (uint8_t)v;
	}
	return n;
}

static char got[EXPECT_MAX][TEXT_MAX];

static int c_lines(void)
{
	const LadxItemPool *pool, *read;
	uint64_t before, after;
	int n = 0;

	pool = ladx_randomizer_pool(&r, &before, &after);
	read = ladx_randomizer_read_pool(&r);
	if (pool) {
		ladx_run_text_words_line(got[n++], TEXT_MAX, "words_before", before);
		ladx_run_text_pool_line(got[n++], TEXT_MAX, "pool", pool);
		ladx_run_text_words_line(got[n++], TEXT_MAX, "words_after", after);
	}
	if (read)
		ladx_run_text_pool_line(got[n++], TEXT_MAX, "placer_pool", read);
	else if (ladx_run_text_outcome_line(got[n], TEXT_MAX, ladx_randomizer_outcome(&r)) > 0) {
		memmove(got[n], got[n] + strlen("outcome "), strlen(got[n]) - strlen("outcome ") + 1);
		n++;
	}
	return n;
}

static void replay_pools(void)
{
	static char want[EXPECT_MAX][TEXT_MAX];
	char name[256], hex[80], short_want[LADX_SETTINGS_SHORT_MAX + 3], short_got[LADX_SETTINGS_SHORT_MAX + 1];
	uint8_t seed[64];
	size_t seed_len;
	LadxSettings s;
	int runs = 0, crashes = 0, have, n, i;
	FILE *f = fopen(POOLS, "r");

	if (!f)
		test_fail(TEST, "cannot open %s", POOLS);
	have = next_line(f);
	while (have) {
		long at = line_no;

		if (sscanf(line, "case %255s %79s %97s", name, hex, short_want) != 3)
			test_fail(TEST, "%s line %ld: want a case line", POOLS, line_no);
		seed_len = seed_of(hex, seed, sizeof seed);
		ladx_settings_defaults(&s);
		if (strcmp(name, "baseline") != 0) {
			size_t len = strlen(name);

			if (len >= 3 && strcmp(name + len - 3, "=\"\"") == 0)
				name[len - 2] = '\0';
			if (ladx_settings_set(&s, name) != LADX_SETTINGS_OK)
				test_fail(TEST, "%s line %ld: setting %s refused", POOLS, at, name);
		}
		ladx_settings_validate(&s);
		ladx_settings_short_string(&s, short_got, sizeof short_got);
		if (strcmp(short_want, "\"\"") == 0)
			short_want[0] = '\0';
		if (strcmp(short_got, short_want) != 0)
			test_fail(TEST, "%s line %ld: short string %s, want %s", POOLS, at, short_got, short_want);
		n = 0;
		while ((have = next_line(f)) && strncmp(line, "case ", 5) != 0) {
			if (n == EXPECT_MAX)
				test_fail(TEST, "%s line %ld: too many lines in a case", POOLS, line_no);
			strcpy(want[n++], line);
		}
		if (ladx_randomize(&r, &s, seed, seed_len, NULL, NULL) != LADX_RANDOMIZE_OK)
			test_fail(TEST, "%s line %ld: %s %s: the run is not OK", POOLS, at, name, hex);
		if (c_lines() != n)
			test_fail(TEST, "%s line %ld: %s %s: %d lines, want %d (first: %s)", POOLS, at, name, hex, c_lines(),
				  n, n ? want[0] : "-");
		for (i = 0; i < n; i++)
			if (strcmp(got[i], want[i]) != 0)
				test_fail(TEST, "%s line %ld: %s %s: line %d\n  got  %s\n  want %s", POOLS, at, name, hex,
					  i + 1, got[i], want[i]);
		if (n == 0)
			test_fail(TEST, "%s line %ld: a case with no line", POOLS, at);
		crashes += strncmp(want[n - 1], "crash ", 6) == 0 || strncmp(want[n - 1], "error ", 6) == 0;
		runs++;
	}
	fclose(f);
	printf("%s: %d runs of %s replayed line-identical (%d crashes)\n", TEST, runs, POOLS, crashes);
}

int main(int argc, char **argv)
{
	int fixtures = test_fixtures_mode(TEST, argc, argv);

	test_dict();
	test_rupee_redraw();
	printf("%s: dict semantics and a rupee re-draw ok (%d checks)\n", TEST, checks);
	if (fixtures)
		replay_pools();
	return 0;
}
