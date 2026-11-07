#include "dungeon_build.h"

/* logic/dungeon4.py Dungeon4 at 4325207e, line for line. */

#define DUNGEON 4

int ladx_dungeon4(const LadxDungeonContext *c)
{
	int i, loop[2];

	/* locations */
	int entrance = LOC("D4 Entrance");
	int entrance_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x179);
	int entrance_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x16A);
	int spark_beetle_room = LOC("D4 Room East of Entrance");
	int spark_beetle_room_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x178);
	int crystal_room = LOC("D4 Crystal Room");
	int crystal_room_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x17B);
	int east_crossroads = LOC("D4 East of Crossroads");
	int east_crossroads_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x171);
	int east_crossroads_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x165);
	int west_statue_room = LOC("D4 Statue Room SW");
	int north_statue_room = LOC("D4 Statue Room NE");
	int south_crossroads = LOC("D4 South of Crossroads");
	int south_crossroads_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x175);
	int north_crossroads = LOC("D4 North of Crossroads");
	int before_miniboss = LOC("D4 Before Miniboss");
	int before_miniboss_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x16F);
	int before_miniboss_zol_clear = ADD_KEY(LOC("D4 Pit Drop Spawned"), D4_PITKEY);
	int before_miniboss_pit = LOC("D4 Pit Before Miniboss");
	int before_miniboss_stairs = LOC("D4 Stairs Before Miniboss");
	int sidescroller_pit = LOC("D4 Sidescroller Pit Entrance");
	int sidescroller_stairs = LOC("D4 Sidescroller Ladder Entrance");
	int sidescroller_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x169);
	int before_miniboss_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x16E);
	int south_tile_puzzle = LOC("D4 Water Tektite Area");
	int south_tile_puzzle_chest9 = ADD(LOC(NULL), DUNGEON_CHEST, 0x16D);
	int south_tile_puzzle_chest10 = ADD(LOC(NULL), DUNGEON_CHEST, 0x168);
	int miniboss_room = LOC("D4 Miniboss Room");
	int after_miniboss = LOC("D4 After Miniboss");
	int flippers_room = LOC("D4 Fireball Statue Room");
	int north_tile_puzzle = LOC("D4 North Tile Puzzle");
	int tile_puzzle_clue = ADD_KEY(LOC("D4 Tile Puzzle Cluee"), D4_PUZZLE_CLUE);
	int flippers_room_chest11 = ADD(LOC(NULL), DUNGEON_CHEST, 0x160);
	int before_a_passage = LOC("D4 Puzzle Stairs Spawn");
	int after_a_passage = LOC("D4 Boss Key Ledge");
	int after_a_passage_chest12 = ADD(LOC(NULL), DUNGEON_CHEST, 0x176);
	int before_b_passage = LOC("D4 Before Boss Passageway");
	int outside_b_passage_keyblock = LOC("D4 Flying Heart Room");
	int outside_b_passage_shallows = LOC("D4 Shallow Water By Keyblock");
	int after_b_passage = LOC("D4 Before Boss Door");
	int pre_boss = LOC("D4 Before Boss Staircase");
	int boss_room = LOC("D4 Boss Room");
	int boss_room_drop2 = ADD_KEY(ADD(LOC(NULL), HEART_CONTAINER, 0x166), D4_BOSS_CLEAR);
	int instrument = ADD(LOC("D4 Instrument Room"), INSTRUMENT, 0x162);

	/* connections */
	/* entrance */
	CONNECT_ALL_BACK(entrance, TARGETS(entrance_chest1, entrance_chest2), NONE, NO);
	CONNECT(entrance, spark_beetle_room, AND(ENEMY(SPIKED_BEETLE), ENEMY(ZOL)));
	CONNECT_BACK(spark_beetle_room, spark_beetle_room_chest3, NONE, NO);
	CONNECT_BACK(spark_beetle_room, crystal_room, AND(ENEMY(SPIKED_BEETLE), ENEMY(ZOL)), NO);
	CONNECT_BACK(crystal_room, crystal_room_chest4, I(SWORD), NO);
	/* crossroads */
	CONNECT(entrance, east_crossroads, AND(I(FEATHER), I(PEGASUS_BOOTS)));
	CONNECT_BACK(east_crossroads, east_crossroads_chest5, I(BOMB), NO);
	CONNECT_BACK(east_crossroads, east_crossroads_chest6, OR(I(BOMB), I(FLIPPERS)), NO);
	CONNECT_BACK(spark_beetle_room, west_statue_room, FOUND(KEY4, 1), FOUND(KEY4, 5));
	CONNECT(west_statue_room, north_statue_room, OR(I(FEATHER), I(FLIPPERS)));
	CONNECT_BACK(north_statue_room, south_crossroads, FOUND(KEY4, 2), FOUND(KEY4, 4));
	CONNECT_BACK(south_crossroads, south_crossroads_chest7, I(FLIPPERS), NO);
	CONNECT(south_crossroads, north_crossroads, AND(I(FEATHER), I(PEGASUS_BOOTS)));
	CONNECT(north_crossroads, before_miniboss, FOUND(KEY4, 3));
	/* miniboss */
	CONNECT_BACK(before_miniboss, before_miniboss_zol_clear, ENEMY(ZOL), NO);
	CONNECT_BACK(before_miniboss, outside_b_passage_shallows,
		     OR(I(FLIPPERS), I(HOOKSHOT), AND(I(FEATHER), I(PEGASUS_BOOTS))),
		     OR(I(FLIPPERS), AND(I(FEATHER), I(PEGASUS_BOOTS))));
	CONNECT_BACK(before_miniboss, before_miniboss_chest8, I(FLIPPERS), NO);
	CONNECT(before_miniboss, south_tile_puzzle, OR(I(FEATHER), I(FLIPPERS)));
	CONNECT(before_miniboss, north_tile_puzzle, I(FLIPPERS));
	CONNECT_BACK(before_miniboss, miniboss_room, FOUND(KEY4, 5), NO);
	CONNECT(before_miniboss, before_miniboss_stairs, NONE);
	CONNECT(before_miniboss_stairs, sidescroller_stairs, NONE);
	CONNECT_BACK(sidescroller_stairs, sidescroller_drop1, AND(I(D4_PITKEY), I(FLIPPERS)), NO);
	CONNECT_BACK(before_miniboss_pit, sidescroller_pit, NONE, NO);
	CONNECT_BACK(sidescroller_pit, sidescroller_stairs, I(FLIPPERS), NO);
	CONNECT_ALL_BACK(miniboss_room, TARGETS(entrance, before_miniboss, after_miniboss), MINIBOSS(3), NO);
	CONNECT_ALL_BACK(after_miniboss, TARGETS(miniboss_room, flippers_room), I(POWER_BRACELET), NO);
	CONNECT_ALL_BACK(flippers_room, TARGETS(after_miniboss, north_tile_puzzle), ENEMY(ZOL), NO);
	CONNECT_BACK(flippers_room, flippers_room_chest11, NONE, NO);
	CONNECT_ALL_BACK(north_tile_puzzle, TARGETS(tile_puzzle_clue, flippers_room, south_tile_puzzle), NONE, NO);
	/* west */
	CONNECT_ALL_BACK(south_tile_puzzle, TARGETS(south_tile_puzzle_chest9, south_tile_puzzle_chest10), NONE, NO);
	CONNECT_BACK(south_tile_puzzle, before_a_passage,
		     AND(I(D4_PUZZLE_CLUE), OR(AND(I(FEATHER), I(PEGASUS_BOOTS)), I(FLIPPERS))), NO);
	CONNECT(before_a_passage, after_a_passage, I(FEATHER));
	CONNECT_ALL_BACK(after_a_passage, TARGETS(south_tile_puzzle, after_a_passage_chest12), NONE, NO);
	CONNECT(south_tile_puzzle, outside_b_passage_keyblock, I(FLIPPERS));
	CONNECT(outside_b_passage_keyblock, before_b_passage, FOUND(KEY4, 5));
	CONNECT(before_b_passage, after_b_passage,
		AND(I(FLIPPERS), AND(ENEMY(CHEEP_CHEEP_HORIZONTAL), ENEMY(CHEEP_CHEEP_VERTICAL))));
	/* boss */
	CONNECT_BACK(after_b_passage, pre_boss, I(NIGHTMARE_KEY4), NONE);
	CONNECT(pre_boss, boss_room, NONE);
	CONNECT_BACK(boss_room, boss_room_drop2, BOSS(3), NO);
	CONNECT_BACK(pre_boss, instrument, I(D4_BOSS_CLEAR), NO);
	/* owl statues */
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon"))
		CONNECT(before_miniboss, before_miniboss_owl1, I(STONE_BEAK4));
	/* normal */
	if (!LOGIC_IS("casual")) {
		CONNECT_ALL_BACK(after_miniboss, TARGETS(miniboss_room, flippers_room), NONE, NO);
		CONNECT(before_b_passage, after_b_passage, I(FLIPPERS));
		CONNECT_BACK(south_tile_puzzle, before_a_passage, OR(AND(I(FEATHER), I(PEGASUS_BOOTS)), I(FLIPPERS)), NO);
	}
	/* hard */
	if (HARD_OR_ABOVE) {
		loop[0] = entrance;
		loop[1] = east_crossroads;
		for (i = 0; i < 2; i++)
			CONNECT_ALL(loop[i], TARGETS(north_crossroads, south_crossroads), R->tight_jump);
		CONNECT_BACK(east_crossroads, east_crossroads_chest6, R->tight_jump, NO);
		CONNECT_BACK(south_crossroads, south_crossroads_chest7, R->tight_jump, NO);
		CONNECT_BACK(before_miniboss, before_miniboss_zol_clear, NONE, NO);
		CONNECT(before_miniboss, before_miniboss_chest8, R->tight_jump);
		loop[0] = before_miniboss;
		loop[1] = sidescroller_stairs;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], sidescroller_drop1, AND(I(D4_PITKEY), I(BOOMERANG)), NO);
		CONNECT_BACK(sidescroller_pit, sidescroller_drop1, I(D4_PITKEY), NO);
		CONNECT_BACK(south_tile_puzzle, before_a_passage, R->tight_jump, NO);
		CONNECT(south_tile_puzzle, after_b_passage, R->tight_jump);
		CONNECT_BACK(after_a_passage, before_a_passage, NONE, NO);
	}
	/* glitched */
	if (GLITCHED_OR_ABOVE) {
		CONNECT_BACK(entrance, west_statue_room, R->super_jump_feather, NO);
		CONNECT_BACK(east_crossroads, east_crossroads_chest5, AND(R->sideways_block_push, I(FLIPPERS)), NO);
		CONNECT_BACK(before_miniboss, north_crossroads, OR(R->hookshot_clip, R->super_jump_feather), NO);
		CONNECT_BACK(before_miniboss, before_miniboss_pit, R->super_jump_feather, NO);
		CONNECT(before_miniboss, north_tile_puzzle, R->jesus_jump);
		CONNECT(before_miniboss, outside_b_passage_shallows, R->jesus_jump);
		CONNECT(south_tile_puzzle, outside_b_passage_keyblock, R->jesus_jump);
		CONNECT_BACK(south_tile_puzzle, after_a_passage, AND(I(POWER_BRACELET), R->super_poke), NO);
		CONNECT_BACK(outside_b_passage_shallows, before_b_passage, R->shaq_jump,
			     AND(R->hookshot_clip_wall, R->super_jump_feather));
	}
	/* hell */
	if (LOGIC_IS("hell")) {
		CONNECT_BACK(entrance, after_a_passage, R->super_bump, NO);
		loop[0] = south_tile_puzzle;
		loop[1] = before_miniboss;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], entrance, OR(R->super_poke, R->super_bump), NO);
		loop[0] = entrance;
		loop[1] = east_crossroads;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], north_crossroads, AND(R->pit_buffer_boots, R->hookshot_spam_pit),
				     AND(R->pit_buffer_itemless, R->hookshot_spam_pit));
		CONNECT_BACK(north_crossroads, south_crossroads, R->pit_buffer_itemless, NO);
		loop[0] = entrance;
		loop[1] = east_crossroads;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], south_crossroads,
				     AND(R->pit_buffer_itemless, OR(R->pit_buffer_boots, R->hookshot_spam_pit)),
				     AND(OR(I(BOMB), I(BOW)), R->hookshot_clip_block));
		CONNECT_BACK(east_crossroads, east_crossroads_chest5,
			     AND(R->sideways_block_push, OR(R->jesus_buffer, R->jesus_jump)), NO);
		CONNECT(west_statue_room, north_statue_room, R->boots_bonk);
		CONNECT_BACK(south_crossroads, south_crossroads_chest7, AND(R->jesus_buffer_hookshot, R->hookshot_clip),
			     NO);
		CONNECT_BACK(north_crossroads, before_miniboss,
			     OR(AND(OR(R->shaq_jump, I(BOMB), I(BOW)), R->hookshot_clip_block),
				AND(R->shaq_jump, R->super_bump)),
			     NO);
		CONNECT_BACK(before_miniboss, north_tile_puzzle, AND(R->jesus_buffer, R->jesus_buffer_hookshot), NO);
		loop[0] = before_miniboss;
		loop[1] = outside_b_passage_shallows;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], south_tile_puzzle, AND(R->jesus_buffer, R->jesus_buffer_hookshot), NO);
		CONNECT_BACK(south_tile_puzzle, north_tile_puzzle, AND(R->damage_boost_special, R->hookshot_clip_block), NO);
		CONNECT_BACK(outside_b_passage_shallows, before_b_passage, R->hookshot_clip_block, NO);
		CONNECT_BACK(outside_b_passage_shallows, before_miniboss_chest8, AND(R->boots_bonk, R->jesus_buffer), NO);
		CONNECT_BACK(outside_b_passage_keyblock, south_tile_puzzle, R->jesus_buffer, NO);
		CONNECT_BACK(south_tile_puzzle, after_a_passage, OR(R->super_poke, AND(I(POWER_BRACELET), R->super_bump)),
			     NO);
		CONNECT_BACK(before_a_passage, after_a_passage, R->boots_bonk_2d_hell, NO);
		CONNECT(south_tile_puzzle, after_b_passage, R->pit_buffer_boots);
	}

	return entrance;
}
