#include "rando_playthrough.h"
#include "rando_spoiler.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_playthrough"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define TEXT_MAX (128 * 1024)

static RandoWorld world, before;
static RandoOptions options;
static RandoPlaythrough playthrough;
static char spoiler[TEXT_MAX], block[TEXT_MAX];

/* fast_ganon with no crystal: Ganon needs RescueZelda and DefeatAgahnim (North East Dark World),
 * MoonPearl, sword level 2 and a Lamp. */
static void fast_ganon_world(void)
{
	RandoConfig config;

	rando_options_init(&options);
	CHECK(rando_options_set(&options, "goal", "fast_ganon"));
	CHECK(rando_options_set(&options, "crystals_ganon", "0"));
	rando_config_from_options(&options, 0, 7, &config);
	rando_world_init(&world, RANDO_VARIANT_OPEN, &config);
	rando_world_add_pre_collected(&world, RANDO_ITEM_BossHeartContainer);
	rando_world_add_pre_collected(&world, RANDO_ITEM_RescueZelda);
	rando_world_add_pre_collected(&world, RANDO_ITEM_DefeatAgahnim);
	rando_world_add_pre_collected(&world, RANDO_ITEM_MoonPearl);
	rando_world_add_pre_collected(&world, RANDO_ITEM_UncleSword);
	rando_world_add_pre_collected(&world, RANDO_ITEM_ProgressiveSword);
	CHECK(rando_world_set_item(&world, RANDO_LOC_GANON, RANDO_ITEM_Triforce));
}

/* The playthrough block of the full spoiler, as run_e2e.sh cuts a section. */
static void write_playthrough(void)
{
	FILE *f = tmpfile();
	const char *p;
	size_t n;

	before = world;
	rando_playthrough(&world, &playthrough);
	CHECK(memcmp(&before, &world, sizeof world) == 0);
	CHECK(f != NULL);
	CHECK(rando_spoiler_write_json(f, &world, &options, &playthrough));
	rewind(f);
	n = fread(spoiler, 1, TEXT_MAX - 1, f);
	fclose(f);
	spoiler[n] = '\0';
	p = strstr(spoiler, "\n    \"playthrough\": {\n");
	CHECK(p != NULL);
	p++;
	n = (size_t)(strstr(p, "\n    }") + 6 - p);
	memcpy(block, p, n);
	block[n] = '\0';
}

static void expect(const char *what, const char *expected)
{
	if (strcmp(block, expected) != 0)
		test_fail(TEST, "%s\n%s\nexpected\n%s", what, block, expected);
}

/* Slot 0 lists the pre-collected items with raw names, minus health, upgrades and events; the last,
 * empty round decrements longest_item_chain; regions_visited counts slot 0's Equipped. A junk item
 * and an item the win does not need never appear. */
static void check_pre_collected_win(void)
{
	fast_ganon_world();
	rando_world_add_pre_collected(&world, RANDO_ITEM_Lamp);
	CHECK(rando_world_set_item(&world, RANDO_LOC_LINKS_HOUSE, RANDO_ITEM_Heart));
	CHECK(rando_world_set_item(&world, RANDO_LOC_SAHASRAHLAS_HUT_LEFT, RANDO_ITEM_Hammer));
	write_playthrough();
	expect("pre-collected win", "    \"playthrough\": {\n"
				    "        \"0\": {\n"
				    "            \"Equipped\": {\n"
				    "                \"Equipment Slot 1\": \"MoonPearl:1\",\n"
				    "                \"Equipment Slot 2\": \"UncleSword:1\",\n"
				    "                \"Equipment Slot 3\": \"ProgressiveSword:1\",\n"
				    "                \"Equipment Slot 4\": \"Lamp:1\"\n"
				    "            }\n"
				    "        },\n"
				    "        \"1\": {\n"
				    "            \"Dark World\": {\n"
				    "                \"Ganon:1\": \"Triforce:1\"\n"
				    "            }\n"
				    "        },\n"
				    "        \"longest_item_chain\": 1,\n"
				    "        \"regions_visited\": 2\n"
				    "    }");
}

/* A key (non-wild) is replayed but not listed; its round is reused. The Lamp behind it is kept by
 * the bubbling pass: without it Ganon, in a higher sphere, is out of reach. */
static void check_key_and_bubbling(void)
{
	fast_ganon_world();
	rando_world_add_pre_collected(&world, RANDO_ITEM_BookOfMudora);
	CHECK(rando_world_set_item(&world, RANDO_LOC_DESERT_PALACE_MAP_CHEST, RANDO_ITEM_KeyP2));
	CHECK(rando_world_set_item(&world, RANDO_LOC_DESERT_PALACE_COMPASS_CHEST, RANDO_ITEM_Lamp));
	write_playthrough();
	expect("key and bubbling", "    \"playthrough\": {\n"
				   "        \"0\": {\n"
				   "            \"Equipped\": {\n"
				   "                \"Equipment Slot 1\": \"MoonPearl:1\",\n"
				   "                \"Equipment Slot 2\": \"UncleSword:1\",\n"
				   "                \"Equipment Slot 3\": \"ProgressiveSword:1\",\n"
				   "                \"Equipment Slot 4\": \"BookOfMudora:1\"\n"
				   "            }\n"
				   "        },\n"
				   "        \"1\": {\n"
				   "            \"Desert Palace\": {\n"
				   "                \"Desert Palace - Compass Chest:1\": \"Lamp:1\"\n"
				   "            }\n"
				   "        },\n"
				   "        \"2\": {\n"
				   "            \"Dark World\": {\n"
				   "                \"Ganon:1\": \"Triforce:1\"\n"
				   "            }\n"
				   "        },\n"
				   "        \"longest_item_chain\": 2,\n"
				   "        \"regions_visited\": 3\n"
				   "    }");
}

/* A round made only of a Trade location is dropped and decrements longest_item_chain. */
static void check_trade_round(void)
{
	fast_ganon_world();
	rando_world_add_pre_collected(&world, RANDO_ITEM_Crystal5);
	rando_world_add_pre_collected(&world, RANDO_ITEM_Crystal6);
	rando_world_add_pre_collected(&world, RANDO_ITEM_Hammer);
	rando_world_add_pre_collected(&world, RANDO_ITEM_BowAndArrows);
	CHECK(rando_world_set_item(&world, RANDO_LOC_PYRAMID_FAIRY_BOW, RANDO_ITEM_Lamp));
	write_playthrough();
	expect("trade round", "    \"playthrough\": {\n"
			      "        \"0\": {\n"
			      "            \"Equipped\": {\n"
			      "                \"Equipment Slot 1\": \"MoonPearl:1\",\n"
			      "                \"Equipment Slot 2\": \"UncleSword:1\",\n"
			      "                \"Equipment Slot 3\": \"ProgressiveSword:1\",\n"
			      "                \"Equipment Slot 4\": \"Crystal5:1\",\n"
			      "                \"Equipment Slot 5\": \"Crystal6:1\",\n"
			      "                \"Equipment Slot 6\": \"Hammer:1\",\n"
			      "                \"Equipment Slot 7\": \"BowAndArrows:1\"\n"
			      "            }\n"
			      "        },\n"
			      "        \"2\": {\n"
			      "            \"Dark World\": {\n"
			      "                \"Ganon:1\": \"Triforce:1\"\n"
			      "            }\n"
			      "        },\n"
			      "        \"longest_item_chain\": 1,\n"
			      "        \"regions_visited\": 2\n"
			      "    }");
}

int main(void)
{
	check_pre_collected_win();
	check_key_and_bubbling();
	check_trade_round();
	printf(TEST ": hand-written cases passed\n");
	return 0;
}
