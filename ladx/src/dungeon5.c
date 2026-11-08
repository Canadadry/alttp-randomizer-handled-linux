#include "dungeon_build.h"

#define DUNGEON 5

int ladx_dungeon5(const LadxDungeonContext *c)
{
	int entrance = LOC("D5 Entrance");
	int entrance_ledge = LOC("D5 Entrance Ledge");
	int entrance_ledge_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1A0);
	int before_a_passage = LOC("D5 Room West of Entrance");
	int before_a_passage_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x19E);
	int after_a_passage = LOC("D5 Crystal Room Entrance");
	int crystal_room = LOC("D5 Crystal Room");
	int crystal_room_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x181);
	int crystal_puddle_room = LOC("D5 Crystal River Area");
	int crystal_puddle_room_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x19A);
	int messy_room = LOC("D5 Messy Room");
	int messy_room_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x19B);
	int statue_room = LOC("D5 Fireball Statue Room");
	int after_blade_trap = LOC("D5 After Blade Trap");
	int pot_column_room = LOC("D5 Pot Column Room");
	int pot_column_room_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x197);
	int before_miniboss = LOC("D5 Before Miniboss");
	int miniboss_room = LOC("D5 Miniboss Room");
	int before_c_passage = LOC("D5 After Miniboss");
	int after_c_passage = LOC("D5 Before Boss Keyblock");
	int after_boss_keyblock = LOC("D5 After Boss Keyblock");
	int ms_1_room = LOC("D5 Master Stalfos 1");
	int ms_1_victory = ADD_KEY(LOC("D5 Master Stalfos 1 Victory"), MS1_KILL);
	int before_d_passage = LOC("D5 South of Crossroads");
	int before_d_passage_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x196);
	int after_d_passage = LOC("D5 Fireball Torches Room");
	int star_room = LOC("D5 Star Owl Room");
	int star_room_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x18A);
	int spark_hallway = LOC("D5 Spark Hallway");
	int north_crossroads = LOC("D5 North of Crossroads");
	int single_tile_ledge = LOC("D5 Hookshot Tile");
	int middle_ledge = LOC("D5 1st Ledge");
	int middle_ledge_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x18E);
	int north_ledge = LOC("D5 North Ledge");
	int north_ledge_chest_7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x188);
	int east_ledge = LOC("D5 East Ledge");
	int east_ledge_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x18F);
	int ms_2_room = LOC("D5 Master Stalfos 2");
	int ms_2_victory = ADD_KEY(LOC("D5 Master Stalfos 2 Victory"), MS2_KILL);
	int west_crossroads = LOC("D5 West of Crossroads");
	int west_crossroads_zol_clear = ADD_KEY(LOC("D5 Zol Above Water Defeated"), D5_ZOL_CLEAR);
	int before_b_passage = LOC("D5 Water Passage Entrance");
	int after_b_passage = LOC("D5 Bridge Room Entrance");
	int boss_key_room = LOC("D5 Bridge Room");
	int boss_key_room_chest10 = ADD(LOC(NULL), DUNGEON_CHEST, 0x186);
	int ms_3_room = LOC("D5 Master Stalfos 3");
	int ms_3_victory = ADD_KEY(LOC("D5 Master Stalfos 3 Victory"), MS3_KILL);
	int pot_locked_room = LOC("D5 Pot Locked Room");
	int pot_locked_room_chest9 = ADD(LOC(NULL), DUNGEON_CHEST, 0x183);
	int ms_4_room = LOC("D5 Master Stalfos 4");
	int ms_4_room_drop2 = ADD(LOC("D5 Master Stalfos 4 Victory"), HOOKSHOT_DROP, -1);
	int pre_boss_room = LOC("D5 Before Boss");
	int boss_room = LOC("D5 Boss Room");
	int boss_room_drop3 = ADD(LOC(NULL), HEART_CONTAINER, 0x185);
	int instrument = ADD(LOC("D5 Instrument Room"), INSTRUMENT, 0x182);

	CONNECT_BACK(entrance, entrance_ledge, I(HOOKSHOT), NO);
	CONNECT_BACK(entrance_ledge, entrance_ledge_chest1, NONE, NO);
	CONNECT_BACK(entrance, before_a_passage, AND(ENEMY(KEESE), ENEMY(IRON_MASK)), ENEMY(IRON_MASK));
	CONNECT(entrance, crystal_puddle_room, FOUND(KEY5, 1));
	CONNECT_BACK(before_a_passage, before_a_passage_chest2, NONE, NO);
	CONNECT_BACK(crystal_puddle_room, messy_room, ENEMY(IRON_MASK), NO);
	CONNECT_BACK(crystal_puddle_room, statue_room, ENEMY(IRON_MASK), NONE);
	CONNECT_BACK(crystal_puddle_room, ms_1_room, AND(ENEMY(STALFOS_AGGRESSIVE), ENEMY(STALFOS_EVASIVE)), NO);
	CONNECT_BACK(messy_room, messy_room_chest3, ENEMY(IRON_MASK), NO);
	CONNECT_ALL_BACK(ms_1_room, TARGETS(ms_1_victory, crystal_puddle_room, before_d_passage), ENEMY(MASTER_STALFOS),
			 NO);
	CONNECT_BACK(before_d_passage, ms_1_room, NONE, NO);
	CONNECT_BACK(before_d_passage, before_d_passage_chest5, NONE, NO);
	CONNECT(before_d_passage, north_crossroads, I(FEATHER));
	CONNECT(before_d_passage, after_d_passage, I(FEATHER));
	CONNECT_ALL(before_d_passage, TARGETS(west_crossroads, ms_2_room), NONE);
	CONNECT_BACK(west_crossroads, west_crossroads_zol_clear, ENEMY(HIDING_ZOL), NO);
	CONNECT_BACK(ms_2_room, ms_2_victory, ENEMY(MASTER_STALFOS), NO);
	CONNECT_BACK(north_crossroads, single_tile_ledge, I(HOOKSHOT), NO);
	CONNECT(north_crossroads, single_tile_ledge, I(FEATHER));
	CONNECT(single_tile_ledge, middle_ledge, AND(I(FEATHER), I(PEGASUS_BOOTS)));
	CONNECT_BACK(single_tile_ledge, middle_ledge, I(HOOKSHOT), NO);
	CONNECT_BACK(middle_ledge, middle_ledge_chest6, NONE, NO);
	CONNECT(middle_ledge, east_ledge, I(HOOKSHOT));
	CONNECT_BACK(east_ledge, east_ledge_chest8, NONE, NO);
	CONNECT_BACK(middle_ledge, north_ledge, I(HOOKSHOT), NO);
	CONNECT_BACK(north_ledge, north_ledge_chest_7, NONE, NO);
	CONNECT(north_ledge, north_crossroads, I(HOOKSHOT));
	CONNECT_BACK(north_crossroads, west_crossroads, I(POWER_BRACELET), NO);
	CONNECT_BACK(north_crossroads, ms_3_room, AND(I(D5_ZOL_CLEAR), ENEMY(HIDING_ZOL)), NO);
	CONNECT_BACK(ms_3_room, ms_3_victory, AND(I(MS1_KILL), I(MS2_KILL), ENEMY(MASTER_STALFOS)), NO);
	CONNECT_BACK(ms_3_room, pot_locked_room, I(POWER_BRACELET), NO);
	CONNECT_BACK(pot_locked_room, pot_locked_room_chest9, NONE, NO);
	CONNECT(before_a_passage, after_a_passage, I(FEATHER));
	CONNECT_BACK(after_a_passage, crystal_room, I(SWORD), NO);
	CONNECT_BACK(crystal_room, crystal_room_drop1, I(SWORD), NO);
	CONNECT_BACK(crystal_room, ms_4_room, NONE, NO);
	CONNECT_BACK(ms_4_room, ms_4_room_drop2, AND(I(MS3_KILL), ENEMY(MASTER_STALFOS)), NO);
	CONNECT(west_crossroads, before_b_passage, I(FLIPPERS));
	CONNECT(before_b_passage, after_b_passage, I(FLIPPERS));
	CONNECT_BACK(after_b_passage, boss_key_room, I(HOOKSHOT), NO);
	CONNECT_BACK(boss_key_room, boss_key_room_chest10, NONE, NO);
	CONNECT_ALL_BACK(statue_room, TARGETS(after_blade_trap, before_miniboss), I(HOOKSHOT), NO);
	CONNECT_BACK(after_blade_trap, pot_column_room, I(HOOKSHOT), NONE);
	CONNECT_BACK(pot_column_room, pot_column_room_chest4, NONE, NO);
	CONNECT(before_miniboss, miniboss_room, FOUND(KEY5, 2));
	CONNECT_ALL_BACK(miniboss_room, TARGETS(entrance, before_c_passage), MINIBOSS(4), NO);
	CONNECT_BACK(before_c_passage, before_miniboss, I(POWER_BRACELET), NO);
	CONNECT_ALL_BACK(star_room, TARGETS(after_d_passage, spark_hallway), ENEMY(STAR), NONE);
	CONNECT_BACK(spark_hallway, after_c_passage, NONE, NO);
	CONNECT_BACK(before_c_passage, after_c_passage, AND(I(HOOKSHOT), I(FEATHER)), I(FEATHER));
	CONNECT(after_c_passage, after_boss_keyblock, FOUND(KEY5, 3));
	CONNECT(after_boss_keyblock, pre_boss_room, I(HOOKSHOT));
	CONNECT_BACK(pre_boss_room, boss_room, I(NIGHTMARE_KEY5), NO);
	CONNECT_ALL_BACK(boss_room, TARGETS(boss_room_drop3, instrument), BOSS(4), NO);
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		CONNECT(crystal_puddle_room, crystal_puddle_room_owl1, I(STONE_BEAK5));
		CONNECT(star_room, star_room_owl2, I(STONE_BEAK5));
	}
	if (HARD_OR_ABOVE) {
		CONNECT_BACK(statue_room, after_blade_trap, I(FEATHER), NO);
		CONNECT_BACK(after_blade_trap, pot_column_room, R->tight_jump, NO);
		CONNECT_BACK(before_miniboss, statue_room, I(HOOKSHOT), NO);
		CONNECT_BACK(after_a_passage, before_a_passage, R->boots_bonk, NO);
		CONNECT(before_d_passage, after_d_passage, R->boots_bonk);
		CONNECT_BACK(before_c_passage, after_c_passage, AND(R->boots_bonk, I(HOOKSHOT)), R->boots_bonk);
		CONNECT(spark_hallway, after_boss_keyblock, R->tight_jump);
		CONNECT_BACK(spark_hallway, pre_boss_room, R->boots_jump, NO);
		CONNECT_ALL_BACK(before_d_passage, TARGETS(north_crossroads, single_tile_ledge), R->hookshot_over_pit, NO);
		CONNECT(single_tile_ledge, north_crossroads, R->diagonal_walk);
		CONNECT_BACK(north_ledge, middle_ledge, I(HOOKSHOT), NO);
		CONNECT(middle_ledge, north_crossroads, R->tight_jump);
		CONNECT_BACK(after_b_passage, boss_key_room, R->boots_jump, NO);
		CONNECT_BACK(north_crossroads, west_crossroads_zol_clear, OR(I(SWORD), I(BOMB)), NO);
	}
	if (GLITCHED_OR_ABOVE) {
		CONNECT_BACK(entrance, entrance_ledge, R->pit_buffer, NO);
		CONNECT_BACK(before_miniboss, before_c_passage, R->hookshot_clip_wall, NO);
		CONNECT_BACK(before_miniboss, before_c_passage, AND(R->sideways_block_push, I(POWER_BRACELET)), NO);
		CONNECT(north_crossroads, north_ledge, R->pit_buffer);
		CONNECT(middle_ledge, east_ledge, R->pit_buffer);
		CONNECT_BACK(after_c_passage, spark_hallway, R->super_jump_boots, NO);
	}
	if (LOGIC_IS("hell")) {
		CONNECT_BACK(entrance, entrance_ledge, R->pit_buffer_boots, NO);
		CONNECT_BACK(before_a_passage, after_a_passage, R->boots_bonk_2d_hell, NO);
		CONNECT_BACK(statue_room, after_blade_trap, R->pit_buffer_boots, NO);
		CONNECT_BACK(after_blade_trap, pot_column_room, R->pit_buffer_itemless, NO);
		CONNECT_BACK(before_miniboss, statue_room, R->boots_jump, NO);
		CONNECT_BACK(statue_room, before_miniboss, AND(R->boots_jump, R->pit_buffer_boots), NO);
		CONNECT_BACK(before_miniboss, before_c_passage, AND(FOUND(KEY5, 3), R->super_bump), NO);
		CONNECT_BACK(before_c_passage, after_c_passage, R->boots_jump, NO);
		CONNECT_BACK(after_c_passage, spark_hallway, OR(R->super_jump_sword, R->zoomerang), NO);
		CONNECT(before_d_passage, after_d_passage, NONE);
		CONNECT_BACK(before_d_passage, north_crossroads, R->boots_bonk_pit, NO);
		CONNECT(north_crossroads, north_ledge, R->pit_buffer_boots);
		CONNECT_BACK(north_ledge, middle_ledge, R->pit_buffer_itemless, NO);
		CONNECT(middle_ledge, east_ledge, R->pit_buffer_boots);
		CONNECT_BACK(ms_3_room, pot_locked_room, AND(R->super_jump_boots, R->zoomerang_shovel), NO);
		CONNECT_BACK(after_b_passage, boss_key_room, R->pit_buffer_itemless, NO);
		CONNECT_BACK(after_boss_keyblock, spark_hallway, R->pit_buffer_itemless, NO);
		CONNECT_BACK(spark_hallway, pre_boss_room, AND(R->super_jump_feather, R->tight_jump), NO);
	}

	return entrance;
}
