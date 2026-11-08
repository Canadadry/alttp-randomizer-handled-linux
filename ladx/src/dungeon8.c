#include "dungeon_build.h"

#define DUNGEON 8

int ladx_dungeon8(const LadxDungeonContext *c)
{
	const int back_entrance_heartpiece = 0x000;
	int i, loop[5];

	int entrance = LOC("D8 Entrance");
	int lava_beamos_room = LOC("D8 Lava Beamos Room");
	int miniboss1_room = LOC("D8 Hinox Room");
	int sw_zamboni_area = LOC("D8 Zamboni Intersection");
	int sw_zamboni_area_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x24D);
	int sw_zamboni_area_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x246);
	int spark_pit_room = LOC("D8 Southwest Spark & Pit Room");
	int spark_pit_room_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x255);
	int miniboss2_room = LOC("D8 Rolling Bones Room");
	int sw_vire_room = LOC("D8 Southwest Vire Room");
	int sw_vire_room_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x24C);
	int vacuum_room = LOC("D8 Vacuum Room");
	int vacuum_room_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x25C);
	int spark_pot_room = LOC("D8 Sparks Hidden Button Room");
	int slime_trap_room = LOC("D8 Lava Chest Room");
	int slime_trap_room_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x259);
	int zamboni_pit_west = LOC("D8 West of Chasm Zamboni");
	int zamboni_pit_east = LOC("D8 East of Chasm Zamboni");
	int zamboni_pit_east_drop2 = ADD(LOC(NULL), DROPPED_KEY, 0x25A);
	int miniboss3_room = LOC("D8 Smasher Room");
	int mimic_room = LOC("D8 Mimic Room");
	int before_a_passage = LOC("D8 Mimic Passageway Spawned");
	int after_a_passage = LOC("D8 Beamos Hidden Button Room");
	int after_a_passage_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x253);
	int pot_pit_room = LOC("D8 Pots & Pits Room");
	int pot_pit_room_doorway = LOC("D8 Pots & Pits Room Door");
	int pot_pit_room_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x25F);
	int after_e_passage = LOC("D8 Pots & Pits Room Stairs");
	int before_e_passage = LOC("D8 Staircase Below Three Peahats");
	int pre_center_zamboni = LOC("D8 North of Entrance");
	int before_b_passage = LOC("D8 Staircase by Center Zamboni");
	int after_b_passage = LOC("D8 Before Cueball");
	int before_c_passage = LOC("D8 Before Passage to Boss");
	int switch_room = LOC("D8 Switch Room");
	int switch_room_switch = ADD_KEY(LOC("D8 Switch"), SWITCH8);
	int pushblock_room = LOC("D8 Pushblock Chest Area");
	int pushblock_room_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x24F);
	int bombwall_corridor = LOC("D8 Bombwall Corridor");
	int pre_center_keyblock = LOC("D8 Before Central Keyblock");
	int loop_ledge = LOC("D8 Useless Ledge");
	int peahat_area = LOC("D8 Peahat Area");
	int heart_vire = LOC("D8 Floating Heart & Vire Area");
	int before_g_passage = LOC("D8 Blade Room");
	int hidden_arrow_room = LOC("Hidden Arrow Room");
	int hidden_arrow_room_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x245);
	int dark_east_zol = LOC("D8 Dark East (Zol Side)");
	int dark_east_spark = LOC("D8 Dark East (Spark Side)");
	int dark_center = LOC("D8 Dark Center");
	int dark_center_torches = LOC("D8 Dark Between Torches");
	int dark_center_pre_keyblock = LOC("D8 Dark CBefore Keyblock");
	int dark_west = LOC("D8 Dark West");
	int before_d_passage = LOC("D8 Dark Room Staircase");
	int after_d_passage = LOC("D8 Staircase by Blaino");
	int miniboss_room = LOC("D8 Blaino Room");
	int miniboss_cubby = LOC("D8 Before Pegs After Blaino");
	int rod_ledge = LOC("D8 Blaino Reward Ledge");
	int rod_ledge_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x237);
	int dodongo_area = LOC("D8 Dodongo Area");
	int dodongo_area_drop3 = ADD(LOC(NULL), DROPPED_KEY, 0x23E);
	int pre_lava_ledge = LOC("D8 Ledge West of Dodongos");
	int lava_ledge = LOC("D8 Lava Ledge");
	int lava_ledge_chest9 = ADD(LOC(NULL), DUNGEON_CHEST, 0x235);
	int slime_corridor = LOC("D8 Corridor by Lava Ledge Chest");
	int after_g_passage = LOC("D8 North Refill Room");
	int after_f_stairs = LOC("D8 Ledge Above Dodongos");
	int after_f_stairs_chest10 = ADD(LOC(NULL), DUNGEON_CHEST, 0x23D);
	int before_f_stairs = LOC("D8 Northwest Area");
	int taltal_passage = LOC_OUTSIDE("D8 Tal Tal Connector");
	int before_f_stairs_owl3 = ADD(LOC(NULL), OWL_STATUE, 0x241);
	int miniboss4_cubby = LOC("D8 Cubby Before Cueball");
	int before_f_stairs_chest11 = ADD(LOC(NULL), DUNGEON_CHEST, 0x240);
	int before_f_stairs_drop4 = ADD(LOC(NULL), DROPPED_KEY, 0x241);
	int ledge_west_boss = LOC("D8 Ledge by NW Exit");
	int ledge_west_boss_chest12 = ADD(LOC(NULL), DUNGEON_CHEST, 0x23A);
	int miniboss4_room = LOC("D8 Cueball Room");
	int nw_zamboni_room = LOC("D8 Two Torch Zamboni Puzzle");
	int nw_zamboni_room_chest13 = ADD(LOC(NULL), DUNGEON_CHEST, 0x232);
	int after_c_passage = LOC("Outside Boss Door");
	int boss_room = LOC("D8 Boss Room");
	int boss_room_drop5 = ADD(LOC(NULL), HEART_CONTAINER, 0x234);
	int instrument = ADD(LOC("D8 Instrument Room"), INSTRUMENT, 0x230);

	CONNECT_ALL(taltal_passage, TARGETS(before_f_stairs, after_f_stairs), NONE);
	{
		int taltal_chest = ADD(LOC_OUTSIDE(NULL), HEART_PIECE, back_entrance_heartpiece);
		CONNECT_BACK(taltal_passage, taltal_chest, NONE, NO);
	}

	CONNECT_ALL_BACK(entrance, TARGETS(lava_beamos_room, spark_pot_room), ENEMY(VIRE), NO);
	CONNECT_BACK(lava_beamos_room, entrance, NONE, NO);
	CONNECT_BACK(lava_beamos_room, miniboss1_room, ENEMY(SNAKE), NONE);
	CONNECT_BACK(miniboss1_room, sw_zamboni_area, MINIBOSS_NAMED(HINOX), NONE);
	CONNECT_ALL_BACK(sw_zamboni_area, TARGETS(sw_zamboni_area_chest1, miniboss2_room), NONE, NO);
	CONNECT_BACK(sw_zamboni_area, sw_zamboni_area_chest2, I(MAGIC_ROD), NO);
	CONNECT_BACK(sw_zamboni_area, spark_pit_room, OR(I(HOOKSHOT), I(FEATHER)), NO);
	CONNECT_BACK(spark_pit_room, spark_pit_room_chest3, NONE, NO);
	CONNECT_ALL_BACK(miniboss2_room, TARGETS(sw_vire_room, vacuum_room, sw_zamboni_area),
			 MINIBOSS_NAMED(ROLLING_BONES), NO);
	CONNECT_BACK(sw_vire_room, sw_vire_room_drop1, R->attack_hookshot_no_bomb, NO);
	CONNECT_ALL_BACK(vacuum_room, TARGETS(vacuum_room_chest4, entrance), NONE, NO);
	CONNECT_ALL_BACK(spark_pot_room, TARGETS(entrance, slime_trap_room, mimic_room), I(POWER_BRACELET), NO);
	CONNECT_ALL_BACK(slime_trap_room, TARGETS(slime_trap_room_chest5, spark_pot_room), NONE, NO);
	CONNECT_BACK(slime_trap_room, zamboni_pit_west, I(FEATHER), OR(I(FEATHER), I(HOOKSHOT)));
	CONNECT_BACK(mimic_room, spark_pot_room, NONE, NO);
	CONNECT_BACK(mimic_room, before_a_passage, ENEMY(MIMIC), NONE);
	CONNECT(before_a_passage, after_a_passage, I(FEATHER));
	CONNECT_ALL_BACK(zamboni_pit_east, TARGETS(zamboni_pit_east_drop2, zamboni_pit_west, miniboss3_room), NONE, NO);
	CONNECT_ALL(switch_room, TARGETS(zamboni_pit_east, before_c_passage), I(BOMB));
	CONNECT_BACK(switch_room, switch_room_switch, R->hit_switch, NO);
	CONNECT_ALL_BACK(miniboss3_room, TARGETS(zamboni_pit_east, after_a_passage, pot_pit_room_doorway),
			 MINIBOSS_NAMED(SMASHER), NO);
	CONNECT_BACK(after_a_passage, miniboss3_room, I(POWER_BRACELET), NO);
	CONNECT_BACK(pot_pit_room_doorway, miniboss3_room, AND(OR(I(FEATHER), I(POWER_BRACELET)), ENEMY(SNAKE)), NO);
	CONNECT(pot_pit_room_doorway, pot_pit_room, OR(I(FEATHER), I(POWER_BRACELET)));
	CONNECT_BACK(pot_pit_room, pot_pit_room_chest6, NONE, NO);
	CONNECT(pot_pit_room, after_e_passage, I(POWER_BRACELET));
	CONNECT(entrance, pre_center_zamboni, I(FEATHER));
	CONNECT_ALL_BACK(pre_center_zamboni,
			 TARGETS(bombwall_corridor, before_e_passage, pre_center_keyblock, loop_ledge, before_c_passage,
				 before_b_passage),
			 NONE, NO);
	CONNECT_BACK(before_b_passage, spark_pot_room, NONE, NO);
	CONNECT_BACK(before_c_passage, slime_trap_room, NONE, NO);
	CONNECT_BACK(bombwall_corridor, pre_center_zamboni, I(FEATHER), NO);
	CONNECT(bombwall_corridor, before_e_passage, I(BOMB));
	CONNECT_BACK(bombwall_corridor, pushblock_room, NONE, NO);
	CONNECT_BACK(pushblock_room, pushblock_room_chest7, NONE, NO);
	CONNECT_BACK(pre_center_keyblock, loop_ledge, I(FEATHER), NO);
	CONNECT_BACK(loop_ledge, before_c_passage, NONE, NO);
	CONNECT_ALL(pre_center_keyblock, TARGETS(heart_vire, peahat_area), FOUND(KEY8, 1));
	CONNECT_BACK(peahat_area, before_e_passage, NONE, NO);
	CONNECT(before_e_passage, after_e_passage, I(FEATHER));
	CONNECT(heart_vire, before_g_passage, FOUND(KEY8, 2));
	CONNECT_BACK(before_g_passage, hidden_arrow_room, NONE, NO);
	CONNECT(before_g_passage, after_g_passage, AND(I(FEATHER), I(HOOKSHOT)));
	CONNECT_BACK(hidden_arrow_room, dodongo_area, ENEMY(HIDING_ZOL), NONE);
	CONNECT(hidden_arrow_room, dark_east_spark, I(BOMB));
	CONNECT(dark_east_spark, dark_east_zol, I(FEATHER));
	CONNECT(dark_east_zol, dark_center, FOUND(KEY8, 4));
	CONNECT_ALL_BACK(peahat_area, TARGETS(dark_center, dark_west), AND(I(BOW), I(BOMB), I(FEATHER)),
			 AND(I(BOMB), I(FEATHER)));
	CONNECT_BACK(dark_center, dark_west, ENEMY(SNAKE), AND(ENEMY(SNAKE), ENEMY(PEAHAT)));
	CONNECT(dark_west, dark_center_torches, FOUND(KEY8, 3));
	loop[0] = dark_center_torches;
	loop[1] = dark_center_pre_keyblock;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], dark_center, NONE, NO);
	CONNECT_BACK(dark_west, before_f_stairs, AND(I(BOMB), I(FEATHER)), AND(I(BOW), I(BOMB), I(FEATHER)));
	CONNECT(dark_center_torches, dark_center_pre_keyblock, I(HOOKSHOT));
	CONNECT(dark_center_pre_keyblock, before_d_passage, FOUND(KEY8, 7));
	CONNECT(before_d_passage, after_d_passage, I(FEATHER));
	CONNECT(after_d_passage, miniboss_room, NONE);
	CONNECT_ALL_BACK(miniboss_room, TARGETS(miniboss_cubby, entrance), MINIBOSS(7), NO);
	CONNECT_BACK(miniboss_cubby, rod_ledge, I(SWITCH8), NO);
	CONNECT_BACK(rod_ledge, rod_ledge_chest8, NONE, NO);
	CONNECT_BACK(dodongo_area, dodongo_area_drop3, ENEMY(GIBDO), NO);
	CONNECT(dodongo_area, pre_lava_ledge, I(FEATHER));
	CONNECT_ALL_BACK(pre_lava_ledge, TARGETS(slime_corridor, after_g_passage), NONE, NO);
	CONNECT_BACK(pre_lava_ledge, lava_ledge, I(HOOKSHOT), NO);
	CONNECT_ALL_BACK(lava_ledge, TARGETS(lava_ledge_chest9, after_g_passage), NONE, NO);
	CONNECT_BACK(after_f_stairs, after_f_stairs_chest10, MINIBOSS_NAMED(DODONGO), NO);
	CONNECT_BACK(after_f_stairs, dodongo_area, NONE, NO);
	CONNECT(slime_corridor, before_f_stairs, FOUND(KEY8, 4));
	CONNECT_ALL_BACK(before_f_stairs, TARGETS(before_f_stairs_chest11, sw_zamboni_area), NONE, NO);
	CONNECT_BACK(before_f_stairs, before_f_stairs_drop4, I(BOW), NO);
	CONNECT_BACK(before_f_stairs, miniboss4_cubby, AND(ENEMY(VIRE), AND(ENEMY(SNAKE))), NONE);
	CONNECT_BACK(before_f_stairs, peahat_area, I(FEATHER), NO);
	CONNECT_BACK(miniboss4_cubby, ledge_west_boss, I(HOOKSHOT), NO);
	CONNECT_BACK(ledge_west_boss, ledge_west_boss_chest12, NONE, NO);
	CONNECT_BACK(before_b_passage, after_b_passage, AND(I(FEATHER), I(MAGIC_ROD)), I(MAGIC_ROD));
	CONNECT_BACK(after_b_passage, miniboss4_cubby, NONE, NO);
	CONNECT_BACK(after_b_passage, miniboss4_room, FOUND(KEY8, 7), NO);
	CONNECT_BACK(miniboss4_room, nw_zamboni_room, AND(I(FEATHER), MINIBOSS_NAMED(CUE_BALL)), NO);
	CONNECT_BACK(nw_zamboni_room, nw_zamboni_room_chest13, NONE, NO);
	CONNECT_BACK(before_c_passage, after_c_passage, AND(I(FEATHER), I(MAGIC_ROD)), I(MAGIC_ROD));
	CONNECT_BACK(after_c_passage, boss_room, I(NIGHTMARE_KEY8), NO);
	CONNECT_BACK(boss_room, boss_room_drop5, BOSS(7), NO);
	CONNECT_BACK(boss_room, instrument, AND(I(FEATHER), BOSS(7)), NO);
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		CONNECT(after_a_passage, after_a_passage_owl1, I(STONE_BEAK8));
		CONNECT(hidden_arrow_room, hidden_arrow_room_owl2, I(STONE_BEAK8));
		CONNECT(before_f_stairs, before_f_stairs_owl3, I(STONE_BEAK8));
	}
	if (HARD_OR_ABOVE) {
		CONNECT_BACK(zamboni_pit_west, zamboni_pit_east, R->tight_jump, NO);
		CONNECT_BACK(after_a_passage, before_a_passage, R->damage_boost, NO);
		CONNECT(pot_pit_room_doorway, pot_pit_room, R->boots_bonk_pit);
		CONNECT_BACK(pot_pit_room_doorway, miniboss3_room, OR(R->throw_pot, AND(I(FEATHER), R->shield_bump)), NO);
		CONNECT(peahat_area, heart_vire, R->tight_jump);
		CONNECT_ALL_BACK(peahat_area, TARGETS(dark_center, dark_west), AND(I(BOMB), I(FEATHER)), NO);
		CONNECT_BACK(dodongo_area, dodongo_area_drop3, OR(I(HOOKSHOT), I(MAGIC_ROD)), NO);
		CONNECT_BACK(before_f_stairs, dark_west, AND(I(BOMB), I(FEATHER)), NO);
		CONNECT_BACK(before_f_stairs, miniboss4_cubby, R->throw_pot, NO);
		CONNECT_BACK(before_f_stairs, sw_vire_room_drop1, AND(MINIBOSS_NAMED(ROLLING_BONES), ENEMY(VIRE)), NO);
	}
	if (GLITCHED_OR_ABOVE) {
		CONNECT_BACK(sw_zamboni_area, spark_pit_room, AND(R->pit_buffer_itemless, R->diagonal_walk), NO);
		CONNECT_BACK(after_e_passage, pot_pit_room, AND(R->hookshot_clip_wall, R->super_jump_feather), NO);
		CONNECT_BACK(before_e_passage, peahat_area, R->sideways_block_push, NO);
		CONNECT_BACK(before_e_passage, bombwall_corridor, R->super_jump_feather, NO);
		loop[0] = before_e_passage;
		loop[1] = pre_center_keyblock;
		loop[2] = loop_ledge;
		loop[3] = before_c_passage;
		loop[4] = before_b_passage;
		for (i = 0; i < 5; i++)
			CONNECT_BACK(loop[i], pre_center_zamboni, R->jesus_jump, NO);
		CONNECT_BACK(peahat_area, before_f_stairs, R->jesus_jump, NO);
		CONNECT_BACK(before_c_passage, loop_ledge, R->super_jump_feather, NO);
		CONNECT_ALL_BACK(loop_ledge, TARGETS(heart_vire, pre_center_keyblock), R->super_jump_feather, NO);
		CONNECT_ALL_BACK(pre_center_keyblock, TARGETS(loop_ledge, heart_vire), R->super_jump_feather, NO);
		CONNECT_ALL_BACK(dark_center, TARGETS(dark_center_torches, dark_center_pre_keyblock), R->super_jump_feather,
				 NO);
		CONNECT_BACK(dark_center, dark_east_zol, R->hookshot_clip_block, NO);
		CONNECT_BACK(dark_center_torches, before_d_passage, R->bookshot, NO);
		CONNECT_BACK(miniboss_cubby, rod_ledge, R->super_jump_feather, NO);
		CONNECT_ALL(slime_corridor, TARGETS(before_f_stairs, after_g_passage), R->jesus_jump);
		CONNECT(miniboss4_cubby, after_b_passage, R->super_jump_feather);
		loop[0] = after_g_passage;
		loop[1] = slime_corridor;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], lava_ledge, AND(R->jesus_jump, R->super_jump_feather), NO);
		CONNECT_BACK(dodongo_area, after_f_stairs, R->super_jump_feather, NO);
		CONNECT_BACK(dodongo_area, after_f_stairs_chest10, AND(MINIBOSS_NAMED(DODONGO), R->super_jump_feather), NO);
		CONNECT_BACK(before_f_stairs, after_c_passage, R->super_jump_feather, NO);
	}
	if (LOGIC_IS("hell")) {
		CONNECT_ALL_BACK(entrance, TARGETS(lava_beamos_room, pre_center_zamboni),
				 AND(R->jesus_buffer, R->lava_swim_sword), NO);
		CONNECT_BACK(entrance, lava_beamos_room, R->jesus_jump, NO);
		CONNECT_BACK(before_a_passage, after_a_passage, OR(R->bracelet_bounce_2d_spikepit, R->boots_bonk_2d_hell), NO);
		CONNECT_BACK(after_e_passage, pot_pit_room,
			     OR(R->shaq_jump, AND(R->hookshot_clip_wall, R->hookshot_clip_block, R->hookshot_over_pit)),
			     R->zoomerang);
		CONNECT_BACK(pot_pit_room_doorway, pot_pit_room, R->pit_buffer_itemless, R->hookshot_spam_pit);
		CONNECT_BACK(pot_pit_room_doorway, miniboss3_room,
			     OR(AND(R->hookshot_spam_pit, ENEMY(SNAKE)), AND(R->boots_bonk_pit, R->shield_bump)), NO);
		CONNECT_BACK(after_e_passage, before_e_passage, R->boots_bonk, NO);
		CONNECT_BACK(zamboni_pit_west, slime_trap_room, AND(R->boots_bonk, R->jesus_buffer), R->boots_bonk_lava);
		CONNECT_BACK(zamboni_pit_west, zamboni_pit_east, AND(R->hookshot_spam_pit, R->hookshot_over_pit), NO);
		CONNECT_BACK(before_f_stairs, peahat_area, AND(R->boots_bonk, R->jesus_buffer), NO);
		CONNECT_BACK(peahat_area, before_f_stairs, AND(R->jesus_buffer, R->lava_swim), NO);
		loop[0] = before_e_passage;
		loop[1] = pre_center_keyblock;
		loop[2] = loop_ledge;
		loop[3] = before_c_passage;
		loop[4] = before_b_passage;
		for (i = 0; i < 5; i++)
			CONNECT_BACK(loop[i], pre_center_zamboni, R->jesus_buffer, NO);
		CONNECT_BACK(before_b_passage, loop_ledge, I(HOOKSHOT), NO);
		CONNECT_BACK(loop_ledge, heart_vire, R->hookshot_clip_block, NO);
		CONNECT_BACK(before_g_passage, after_g_passage, AND(I(HOOKSHOT), R->damage_boost_special, I(TOADSTOOL2)), NO);
		CONNECT_BACK(before_f_stairs, dark_west, AND(I(BOMB), R->jesus_buffer, R->lava_swim), NO);
		loop[0] = dark_west;
		loop[1] = dark_center;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], peahat_area, AND(I(BOMB), R->jesus_buffer), NO);
		CONNECT_BACK(dark_west, before_f_stairs, AND(I(BOMB), R->hookshot_wrap, R->jesus_buffer), NO);
		CONNECT(dark_east_zol, dark_east_spark, OR(R->hookshot_spam_pit, R->boots_bonk));
		CONNECT_BACK(dark_center, before_d_passage, AND(R->super_jump_feather, R->shield_bump), NO);
		CONNECT_BACK(dark_center_pre_keyblock, before_d_passage, R->zoomerang_shovel, NO);
		CONNECT(before_d_passage, after_d_passage, R->boots_bonk_2d_hell);
		CONNECT_BACK(dodongo_area, dodongo_area_drop3, AND(I(FEATHER), I(SHIELD)), NO);
		CONNECT(dodongo_area, pre_lava_ledge, R->boots_bonk);
		CONNECT(pre_lava_ledge, lava_ledge, R->super_jump_feather);
		CONNECT(slime_corridor, before_f_stairs, AND(R->jesus_buffer, R->lava_swim));
		CONNECT_BACK(before_b_passage, after_b_passage, AND(R->boots_bonk_2d_hell, I(MAGIC_ROD)), NO);
		CONNECT_BACK(before_f_stairs, ledge_west_boss, R->super_bump, NO);
		CONNECT_BACK(miniboss4_room, nw_zamboni_room, AND(R->boots_bonk, R->jesus_buffer, MINIBOSS_NAMED(CUE_BALL)),
			     NO);
		CONNECT_BACK(before_c_passage, after_c_passage, AND(R->boots_bonk_2d_hell, I(MAGIC_ROD)), NO);
		CONNECT_BACK(after_c_passage, before_f_stairs, AND(R->jesus_jump, R->zoomerang), NO);
		CONNECT_BACK(boss_room, instrument, AND(R->boots_bonk_lava, BOSS(7)), NO);
		if (!OPTION(OVERWORLD, "alttp") && !OPTION(OVERWORLD, "dungeondive"))
			CONNECT_BACK(before_a_passage, after_a_passage, R->toadstool_bounce_2d_spikepit, NO);
	}

	return entrance;
}
