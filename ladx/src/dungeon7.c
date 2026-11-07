#include "dungeon_build.h"

/* logic/dungeon7.py Dungeon7 at 4325207e, line for line. */

#define DUNGEON 7

int ladx_dungeon7(const LadxDungeonContext *c)
{
	int i, loop[3];

	/* locations */
	int entrance = LOC("D7 Entrance");
	int entrance_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x210);
	int before_a_stairs = LOC("D7 Key Locked Staircase");
	int after_a_stairs = LOC("D7 Ball Room");
	int after_a_stairs_switch = ADD_KEY(LOC("D7 Pit Switch"), SWITCH7B);
	int after_a_stairs_switch_range = ADD_KEY(LOC("D7 Pit Switch (Range)"), SWITCH7B_RANGE);
	int after_a_stairs_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x216);
	int ball_access = ADD_KEY(LOC("D7 Ball Access"), D7_BALL);
	int before_b_stairs = LOC("D7 1st Floor North Area");
	int before_b_stairs_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x204);
	int before_b_stairs_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x209);
	int before_b_stairs_switch = ADD_KEY(LOC("D7 1st Floor Switch"), SWITCH7A);
	int before_b_stairs_switch_range = ADD_KEY(LOC("D7 1st Floor Switch (From Pegs)"), SWITCH7A_RANGE);
	int after_b_stairs = LOC("D7 Upstairs Locked Staircase");
	int before_c_stairs = LOC("D7 1st Floor NW Staircase");
	int after_c_stairs = LOC("D7 North Three-of-a-Kind");
	int after_c_stairs_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x211);
	int before_d_stairs = LOC("D7 1st Floor SW Staircase");
	int after_d_stairs = LOC("D7 Hinox Area");
	int after_d_stairs_drop2 = ADD(LOC(NULL), DROPPED_KEY, 0x21B);
	int east_ledge = LOC("D7 East Ledge");
	int east_ledge_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x204);
	int west_ledge = LOC("D7 West Ledge");
	int west_ledge_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x201);
	int ne_pillar = LOC("D7 NE Pillar Area");
	int ne_pillar_fall = ADD_KEY(LOC("D7 NE Pillar Fallen"), D7_PILLAR);
	int ne_pillar_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x212);
	int se_pillar = LOC("D7 SE Pillar Area");
	int se_pillar_fall = ADD_KEY(LOC("D7 SE Pillar Fallen"), D7_PILLAR);
	int se_pillar_switch = ADD_KEY(LOC("D7 Fenced Switch"), SWITCH7C);
	int se_pillar_switch_midrange = ADD_KEY(LOC("D7 Fenced Switch (From Below)"), SWITCH7C_MIDRANGE);
	int se_pillar_switch_range = ADD_KEY(LOC("D7 Fenced Switch (From Pegs)"), SWITCH7C_RANGE);
	int nw_pillar = LOC("D7 NW Pillar Area");
	int nw_pillar_fall = ADD_KEY(LOC("D7 NW Pillar Fallen"), D7_PILLAR);
	int sw_pillar = LOC("D7 SW Pillar Area");
	int sw_pillar_fall = ADD_KEY(LOC("D7 SW Pillar Fallen"), D7_PILLAR);
	int sw_pillar_toak_clear = ADD_KEY(LOC("D7 South Three-of-a-Kind Clear"), D7_TOAK_CLEAR);
	int sw_pillar_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x21C);
	int tile_room = LOC("D7 Floating Tile Fight");
	int spike_corridor = LOC("D7 Corridor Between Pillars");
	int bombwall_corridor = LOC("D7 Bombable Wall Corridor");
	int bombwall_pit = LOC("D7 Near Bombable Wall Owl");
	int bombwall_pit_owl3 = ADD(LOC(NULL), OWL_STATUE, 0x21C);
	int keylock_ledge = LOC("D7 Key Locked Ledge");
	int pegs_before_ball = LOC("D7 On Pegs Around Chest");
	int pegs_before_ball_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x21A);
	int before_e_stairs = LOC("D7 Staircase by Fenced Switch");
	int after_e_stairs = LOC("D7 3rd Floor Staircase");
	int pre_cut_floor3 = LOC("D7 3rd Floor (Before Cutscene)");
	int post_cut_floor3 = LOC("D7 3rd Floor (After Cutscene)");
	int miniboss = LOC("D7 Miniboss Room");
	int after_miniboss = LOC("D7 After Miniboss Room");
	int after_miniboss_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x224);
	int pre_cut_boss_backdoor = LOC("D7 Boss Backdoor Room (Before Cutscene)");
	int post_cut_boss_backdoor = LOC("D7 Boss Backdoor Room (After Cutscene)");
	int pre_cut_conveyor_area = LOC("D7 Conveyor Horseheads Area (Before Cutscene)");
	int post_cut_conveyor_area = LOC("D7 After Boss Door");
	int conveyor_area_chest9 = ADD(LOC(NULL), DUNGEON_CHEST, 0x220);
	int pre_boss_stairs = LOC("D7 Before Boss");
	int boss_room = LOC("D7 Boss Room");
	int boss_room_drop3 = ADD_KEY(ADD(LOC(NULL), HEART_CONTAINER, 0x223), D7_BOSS_CLEAR);
	int instrument = ADD(LOC("D7 Instrument Room"), INSTRUMENT, 0x22c);

	/* connections */
	/* floor 1 */
	CONNECT_BACK(entrance, entrance_drop1, ENEMY(LIKE_LIKE), NO);
	CONNECT(entrance, before_a_stairs, FOUND(KEY7, 1));
	CONNECT_BACK(before_b_stairs, before_b_stairs_chest1, NONE, NO);
	CONNECT_BACK(before_b_stairs, before_b_stairs_switch, R->hit_switch, NO);
	CONNECT_BACK(before_b_stairs, before_b_stairs_switch_range,
		     OR(I(BOOMERANG), I(BOW), I(BOMB), I(HOOKSHOT), I(MAGIC_ROD)), NO);
	CONNECT_BACK(before_b_stairs, before_c_stairs, AND(I(SWITCH7A_RANGE), I(FEATHER)), NO);
	CONNECT_BACK(before_b_stairs, before_d_stairs, I(SWITCH7A), NO);
	CONNECT_BACK(before_b_stairs, entrance, I(SWITCH7A), NO);
	CONNECT_ALL_BACK(west_ledge, TARGETS(entrance, before_b_stairs, west_ledge_chest4), NONE, NO);
	CONNECT_ALL_BACK(east_ledge, TARGETS(entrance, before_a_stairs, before_b_stairs, east_ledge_chest3), NONE, NO);
	/* connect floor 1 & 2 */
	CONNECT(before_a_stairs, after_a_stairs, NONE);
	CONNECT(before_b_stairs, after_b_stairs, NONE);
	CONNECT(before_c_stairs, after_c_stairs, NONE);
	CONNECT(before_d_stairs, after_d_stairs, NONE);
	CONNECT_ALL_BACK(ne_pillar, TARGETS(entrance, before_b_stairs), NONE, NO);
	CONNECT_ALL_BACK(se_pillar, TARGETS(entrance, before_b_stairs), NONE, NO);
	CONNECT_BACK(spike_corridor, before_b_stairs, NONE, NO);
	CONNECT_BACK(nw_pillar, before_b_stairs, NONE, NO);
	loop[0] = bombwall_pit;
	loop[1] = sw_pillar;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], entrance, NONE, NO);
	CONNECT_ALL_BACK(tile_room, TARGETS(before_b_stairs, west_ledge), NONE, NO);
	CONNECT_ALL_BACK(after_d_stairs, TARGETS(entrance, west_ledge), NONE, NO);
	CONNECT_BACK(pegs_before_ball, east_ledge, NONE, NO);
	/* floor 2 north */
	CONNECT_BACK(after_a_stairs, ball_access, I(POWER_BRACELET), NO);
	CONNECT_BACK(after_a_stairs, ne_pillar, I(POWER_BRACELET), NONE);
	CONNECT_BACK(after_a_stairs, pegs_before_ball, OR(I(SWITCH7A), I(SWITCH7B_RANGE)), NO);
	CONNECT_BACK(after_b_stairs, ne_pillar, I(SWITCH7A), OR(I(SWITCH7B_RANGE), I(SWITCH7C)));
	CONNECT_BACK(ne_pillar, ne_pillar_chest5, I(POWER_BRACELET), NO);
	CONNECT_BACK(ne_pillar, ne_pillar_fall, I(D7_BALL), NO);
	CONNECT(ne_pillar, se_pillar, I(FEATHER));
	CONNECT_BACK(ne_pillar, spike_corridor, OR(I(SWITCH7A), I(SWITCH7B_RANGE)), OR(I(SWITCH7C), I(SWITCH7C_MIDRANGE)));
	CONNECT_BACK(se_pillar, se_pillar_fall, I(D7_BALL), NO);
	CONNECT(se_pillar, se_pillar_switch, R->hit_switch);
	CONNECT_BACK(se_pillar, spike_corridor, OR(I(FEATHER), I(SWITCH7C)),
		     OR(I(SWITCH7A), I(SWITCH7B_RANGE), I(FEATHER)));
	CONNECT_BACK(se_pillar, before_c_stairs, I(FEATHER), NO);
	CONNECT(spike_corridor, after_c_stairs, I(FEATHER));
	CONNECT_BACK(spike_corridor, before_c_stairs, OR(I(SWITCH7A), I(SWITCH7B_RANGE)), NO);
	CONNECT_BACK(after_c_stairs, after_c_stairs_chest2, ENEMY(THREE_OF_A_KIND), NO);
	CONNECT_BACK(nw_pillar, spike_corridor, I(FEATHER), NO);
	CONNECT_BACK(nw_pillar, nw_pillar_fall, I(D7_BALL), NO);
	CONNECT(nw_pillar, tile_room, NONE);
	/* floor 2 south */
	CONNECT_BACK(after_d_stairs, tile_room, NONE, NO);
	CONNECT_BACK(after_d_stairs, after_d_stairs_drop2, MINIBOSS_NAMED(HINOX), NO);
	CONNECT_BACK(after_d_stairs, sw_pillar_toak_clear, ENEMY(THREE_OF_A_KIND), NO);
	CONNECT_BACK(after_d_stairs, se_pillar_switch_midrange, OR(I(BOOMERANG), I(BOW), I(BOMB), I(MAGIC_ROD)), NO);
	CONNECT_BACK(after_d_stairs, se_pillar_switch_range, OR(I(BOOMERANG), I(BOW), I(BOMB)), NO);
	CONNECT_BACK(after_d_stairs, pegs_before_ball, I(SWITCH7C_RANGE), NONE);
	CONNECT(after_d_stairs, keylock_ledge, FOUND(KEY7, 3));
	CONNECT_ALL_BACK(keylock_ledge, TARGETS(se_pillar, pegs_before_ball, after_d_stairs), NONE, NO);
	CONNECT_BACK(pegs_before_ball, after_a_stairs_switch, R->hit_switch, NO);
	CONNECT_ALL_BACK(pegs_before_ball, TARGETS(after_a_stairs, pegs_before_ball_chest7, after_d_stairs), NONE, NO);
	/* floor 2 center */
	CONNECT_ALL(bombwall_corridor, TARGETS(bombwall_pit, nw_pillar), I(BOMB));
	CONNECT_BACK(bombwall_corridor, after_d_stairs, NONE, NO);
	CONNECT_BACK(bombwall_pit, sw_pillar, AND(I(HOOKSHOT), I(D7_TOAK_CLEAR)), NO);
	CONNECT_BACK(sw_pillar, sw_pillar_chest6, I(D7_TOAK_CLEAR), NO);
	CONNECT_BACK(sw_pillar, sw_pillar_fall, I(D7_BALL), NO);
	/* connect floor 2 & 3 */
	CONNECT(after_d_stairs, before_e_stairs, NONE);
	CONNECT(before_e_stairs, after_e_stairs, NONE);
	CONNECT_BACK(after_e_stairs, pre_cut_floor3, NONE, NO);
	CONNECT_BACK(after_e_stairs, post_cut_floor3, FOUND(D7_PILLAR, 4), NO);
	/* floor 3 before cutscene */
	CONNECT_BACK(pre_cut_floor3, miniboss, NONE, NO);
	loop[0] = pre_cut_conveyor_area;
	loop[1] = post_cut_conveyor_area;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], before_c_stairs, NONE, NO);
	CONNECT_BACK(miniboss, entrance, MINIBOSS(6), NO);
	CONNECT_ALL_BACK(miniboss, TARGETS(pre_cut_floor3, after_miniboss), MINIBOSS(6), NONE);
	CONNECT_BACK(after_miniboss, after_miniboss_chest8, NONE, NO);
	CONNECT(pre_cut_floor3, pre_cut_boss_backdoor, FOUND(KEY7, 3));
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], conveyor_area_chest9, I(POWER_BRACELET), NO);
	/* floor 3 after cutscene */
	CONNECT(post_cut_floor3, post_cut_conveyor_area, I(NIGHTMARE_KEY7));
	CONNECT_BACK(post_cut_conveyor_area, pre_boss_stairs, I(HOOKSHOT), NONE);
	CONNECT(pre_boss_stairs, boss_room, NONE);
	CONNECT_BACK(boss_room, boss_room_drop3, BOSS(6), NO);
	CONNECT_BACK(boss_room, instrument, I(D7_BOSS_CLEAR), NO);
	/* key logic patch */
	if (OPTION(DUNGEON_KEYS, ""))
		ladx_logic_force_first_item(L, entrance_drop1, LADX_ITEM_KEY7);
	/* owl statues */
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		CONNECT_BACK(after_a_stairs, after_a_stairs_owl1, I(STONE_BEAK7), NO);
		CONNECT_BACK(before_b_stairs, before_b_stairs_owl2, I(STONE_BEAK7), NO);
		CONNECT_BACK(bombwall_pit, bombwall_pit_owl3, I(STONE_BEAK7), NO);
	}
	/* normal */
	if (!LOGIC_IS("casual")) {
		CONNECT(after_a_stairs, ne_pillar, NONE);
		CONNECT_BACK(after_a_stairs, after_a_stairs_switch_range, I(BOOMERANG), NO);
		CONNECT_BACK(before_b_stairs, before_c_stairs, I(SWITCH7A), NO);
	}
	/* hard */
	if (HARD_OR_ABOVE) {
		CONNECT_BACK(after_c_stairs, after_c_stairs_chest2, I(BOMB), NO);
		CONNECT(after_c_stairs, spike_corridor, R->damage_boost);
		CONNECT_BACK(after_d_stairs, se_pillar_switch_midrange, AND(I(FEATHER), I(SWORD)), NO);
		CONNECT_BACK(after_d_stairs, se_pillar_switch_range, I(MAGIC_ROD), NO);
		CONNECT_BACK(after_d_stairs, sw_pillar_toak_clear, I(D7_BALL), NO);
		loop[0] = after_d_stairs;
		loop[1] = bombwall_pit;
		loop[2] = sw_pillar;
		for (i = 0; i < 3; i++)
			CONNECT_BACK(loop[i], sw_pillar_toak_clear, I(BOMB), NO);
	}
	/* glitched */
	if (GLITCHED_OR_ABOVE) {
		CONNECT_BACK(ne_pillar, se_pillar, AND(R->boots_bonk, R->pit_buffer_itemless), NO);
		CONNECT_BACK(ne_pillar, ne_pillar_fall, AND(R->bomb_trigger, ENEMY(HIDING_ZOL)), NO);
		CONNECT_BACK(se_pillar, se_pillar_fall, AND(R->bomb_trigger, ENEMY(HIDING_ZOL)), NO);
		CONNECT_ALL_BACK(spike_corridor, TARGETS(nw_pillar_fall, ne_pillar_fall, se_pillar_fall),
				 AND(R->bomb_trigger, ENEMY(HIDING_ZOL)), NO);
		CONNECT_BACK(nw_pillar, nw_pillar_fall, R->bomb_trigger, NO);
		CONNECT_BACK(sw_pillar, sw_pillar_fall, R->bomb_trigger, NO);
		CONNECT_ALL_BACK(entrance, TARGETS(before_b_stairs, before_c_stairs), R->super_jump_sword, NO);
		CONNECT_ALL_BACK(before_b_stairs, TARGETS(entrance, before_c_stairs, east_ledge), R->super_jump_feather, NO);
		CONNECT_ALL_BACK(before_b_stairs, TARGETS(west_ledge, east_ledge), I(PEGASUS_BOOTS), NO);
		CONNECT_BACK(spike_corridor, nw_pillar, OR(R->shaq_jump, R->super_jump_feather), NO);
		CONNECT_BACK(west_ledge, before_d_stairs, R->boots_jump, NO);
		CONNECT_ALL_BACK(after_d_stairs, TARGETS(bombwall_pit, sw_pillar), R->sideways_block_push, NO);
		CONNECT_BACK(after_d_stairs, se_pillar, R->super_jump_feather, NO);
		CONNECT_BACK(after_d_stairs, bombwall_corridor, R->shaq_jump, NO);
		CONNECT_BACK(post_cut_floor3, pre_boss_stairs, R->super_jump_feather, NO);
		CONNECT_BACK(pre_cut_boss_backdoor, pre_cut_conveyor_area, AND(R->hookshot_clip_wall, R->super_jump_feather),
			     NO);
		CONNECT_BACK(post_cut_boss_backdoor, post_cut_conveyor_area,
			     AND(R->hookshot_clip_wall, R->super_jump_feather), NO);
	}

	/* hell */
	if (LOGIC_IS("hell")) {
		CONNECT_BACK(entrance, before_a_stairs, OR(R->boots_superbump, R->boots_superpoke),
			     AND(OR(R->super_jump_boots, R->zoomerang), R->zoomerang_shovel));
		CONNECT_BACK(entrance, west_ledge, R->super_bump, NO);
		CONNECT_BACK(entrance, east_ledge, AND(OR(R->super_jump_boots, R->zoomerang), R->shield_bump), NO);
		CONNECT_ALL(entrance, TARGETS(before_b_stairs, before_c_stairs), R->super_jump_feather);
		CONNECT_BACK(entrance, before_b_stairs, R->boots_superhop, YES);
		CONNECT_BACK(before_b_stairs, before_b_stairs_switch_range, R->sword_beam, NO);
		CONNECT_BACK(before_b_stairs, east_ledge, R->boots_superhop, NO);
		CONNECT_ALL_BACK(before_c_stairs, TARGETS(before_b_stairs, west_ledge), R->super_bump, NO);
		CONNECT(ne_pillar, se_pillar, OR(R->pit_buffer_itemless, R->hookshot_spam_pit));
		CONNECT_BACK(se_pillar, spike_corridor, AND(I(BOOMERANG), R->hookshot_clip_block), NO);
		CONNECT_BACK(spike_corridor, ne_pillar, R->pit_buffer_boots,
			     OR(R->boots_superbump, AND(R->pit_buffer_itemless, R->super_jump_feather)));
		CONNECT_BACK(ne_pillar, after_b_stairs, R->super_bump, NO);
		CONNECT_BACK(spike_corridor, se_pillar, R->boots_bonk_pit, NO);
		CONNECT_BACK(after_d_stairs, after_d_stairs_drop2, I(D7_BALL), NO);
		CONNECT_BACK(after_d_stairs, sw_pillar, R->super_jump_boots, NO);
		CONNECT_BACK(after_d_stairs, se_pillar, R->boots_superhop, NO);
		CONNECT_BACK(after_d_stairs, se_pillar_switch_midrange,
			     OR(AND(R->boots_bonk, I(SWORD)), AND(R->super_jump_feather, I(HOOKSHOT)), R->sword_beam), NO);
		CONNECT_BACK(after_d_stairs, se_pillar_switch_range, R->sword_beam, NO);
		CONNECT_BACK(after_d_stairs, keylock_ledge, AND(R->boots_superhop, R->shield_bump), NO);
		loop[0] = bombwall_pit;
		loop[1] = sw_pillar;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], after_d_stairs, R->zoomerang_shovel, NO);
		CONNECT(bombwall_pit, sw_pillar, R->pit_buffer_boots);
		CONNECT_BACK(sw_pillar, sw_pillar_toak_clear, NONE, NO);
		CONNECT_BACK(post_cut_floor3, pre_boss_stairs, R->boots_superhop, NO);
		CONNECT_BACK(pre_cut_floor3, pre_cut_conveyor_area,
			     OR(R->super_bump, R->super_poke, R->boots_superbump, R->boots_superpoke), NO);
		CONNECT_BACK(post_cut_floor3, post_cut_conveyor_area,
			     OR(R->super_bump, R->super_poke, R->boots_superbump, R->boots_superpoke), NO);
	}

	return entrance;
}
