#include "dungeon_build.h"

/* logic/dungeon6.py Dungeon6 at 4325207e, line for line. */

#define DUNGEON 6

int ladx_dungeon6(const LadxDungeonContext *c)
{
	/* Logic passes no raft_game_chest outside overworld=alttp. */
	const bool raft_game_chest = true;
	int i, loop[5];

	/* locations */
	int entrance = LOC("D6 Entrance");
	int entrance_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x1BB);
	int entrance_switch = ADD_KEY(LOC("D6 Entrance Switch"), SWITCH6A);
	int entrance_switch_range = ADD_KEY(LOC("D6 Entrance Switch (Range)"), SWITCH6A_RANGE);
	int first_elephant_room = LOC("D6 1st Statue & Chest Room");
	int first_elephant_room_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1CF);
	int second_elephant_room = LOC("D6 2nd Statue & Chest Room");
	int second_elephant_room_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1C9);
	int potbutton_area_switched = LOC("D6 Hidden Button Area");
	int fenced_walkway_switched = LOC("D6 Fenced Walkway");
	int dark_room = LOC("D6 Dark Room");
	int before_a_passage = LOC("D6 Dark Room Stairs Spawn");
	int after_a_passage = LOC("D6 Passage Inside Pegs");
	int pre_bracelet_room = LOC("D6 Before L2 Bracelet Room");
	int bracelet_room = LOC("D6 L2 Bracelet Room");
	int bracelet_room_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1CE);
	int after_potbutton_door = LOC("D6 After Button Door");
	int after_potbutton_door_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1C0);
	int three_wizrobe_area = LOC("D6 Wizrobe Peg Area");
	int three_wizrobe_area_switched = LOC("D6 Wizrobe Peg Area (Switched)");
	int three_wizrobe_area_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1B9);
	int three_wizrobe_area_clear = ADD_KEY(LOC("D6 Wizrobe Peg Room Clear"), D6_THREE_WIZROBE_CLEAR);
	int three_wizrobe_area_switch_midrange = ADD_KEY(LOC("D6 Wizrobe Peg Area Switch"), SWITCH6B_MIDRANGE);
	int three_wizrobe_area_switch_range = ADD_KEY(LOC("D6 Wizrobe Peg Area Switch (Range)"), SWITCH6B_RANGE);
	int star_area = LOC("D6 NW Star Area");
	int star_area_switched = LOC("D6 NW Star Area (Switched)");
	int star_area_switch = ADD_KEY(LOC("D6 NW Star Area Switch"), SWITCH6C);
	int star_area_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1B3);
	int star_area_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x1B4);
	int top_left_room = LOC("D6 NW Horsehead Room");
	int top_left_room_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1B0);
	int top_left_room_switch = ADD_KEY(LOC("D6 NW Horsehead witch"), SWITCH6D);
	int top_left_exit = LOC("D6 NW Exit Staircase");
	int rapids_island = LOC_OUTSIDE("D6 Raft Ride Island");
	int before_miniboss = LOC("D6 Before Miniboss");
	int miniboss_room = LOC("D6 Miniboss Room");
	int after_miniboss = LOC("D6 After Miniboss");
	int before_b_passage = LOC("D6 Statue Stairs Revealed");
	int after_b_passage = LOC("D6 2nd Floating Tile Fight");
	int after_b_passage_drop2 = ADD(LOC(NULL), DROPPED_KEY, 0x1C3);
	int flying_bomb_room = LOC("D6 Flying Bomb Room");
	int before_c_passage = LOC("D6 Passageway Below Torch");
	int four_wizrobe_room = LOC("D6 Four Wizrobe Room");
	int blade_trap_room = LOC("D6 Blade Trap Room");
	int blade_trap_room_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x1D7);
	int after_blade_trap = LOC("D6 Ledge After Blade");
	int after_blade_trap_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1D1);
	int south_waterway = LOC("D6 South Waterway");
	int north_waterway = LOC("D6 North Waterway");
	int north_waterway_chest9 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1BE);
	int after_c_passage = LOC("D6 Pols Room");
	int south_spark_pot_maze = LOC("D6 Before Pot Maze");
	int north_spark_pot_maze = LOC("D6 After Pot Maze");
	int top_right_room = LOC("D6 NE Horsehead Room");
	int top_right_room_switch = ADD_KEY(LOC("D6 NE Horsehead Switch"), SWITCH6E);
	int top_right_room_chest10 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1B1);
	int dodongo_room = LOC("D6 Dodongo Room");
	int north_waterway_east_ledge = LOC("D6 Outside Dodongo Room");
	int hookshot_block = LOC("D6 North Waterway Hookshot Block");
	int north_waterway_west_ledge = LOC("D6 Star Ledge");
	int pot_area = LOC("D6 Pot Owl Area");
	int pot_area_owl3 = ADD(LOC(NULL), OWL_STATUE, 0x1B6);
	int pot_area_switch = ADD_KEY(LOC("D6 Pot Owl Area Switch"), SWITCH6F);
	int pot_area_chest11 = ADD(LOC(NULL), DUNGEON_CHEST, 0x1B6);
	int vacuum_room = LOC("D6 Vacuum Room");
	int laser_turret_room = LOC("D6 Laser Turret Room");
	int pre_boss_room = LOC("D6 Room Before Boss");
	int boss_room = LOC("D6 Boss Room");
	int boss_room_drop3 = ADD(LOC(NULL), HEART_CONTAINER, 0x1BC);
	int instrument = ADD(LOC("D6 Instrument Room"), INSTRUMENT, 0x1b5);

	/* back exit */
	if (raft_game_chest) {
		int rapids_island_chest11 = ADD(LOC_OUTSIDE(NULL), CHEST, 0x06C);
		CONNECT_BACK(rapids_island, rapids_island_chest11, NONE, NO);
	}

	/* connections */
	/* entrance */
	CONNECT_BACK(entrance, entrance_switch, R->hit_switch, NO);
	CONNECT_BACK(entrance, entrance_switch_range, I(BOMB), NO);
	CONNECT_BACK(entrance, first_elephant_room, ENEMY(WIZROBE), NONE);
	CONNECT_BACK(entrance, second_elephant_room, COUNT(POWER_BRACELET, 2), NONE);
	CONNECT(dark_room, entrance, I(BOMB));
	CONNECT_BACK(first_elephant_room, first_elephant_room_chest1, NONE, NO);
	CONNECT(first_elephant_room, second_elephant_room, COUNT(POWER_BRACELET, 2));
	CONNECT_BACK(second_elephant_room, second_elephant_room_chest2, NONE, NO);
	CONNECT_BACK(dark_room, before_a_passage, ENEMY(HIDING_ZOL), NONE);
	CONNECT(before_a_passage, after_a_passage, I(FEATHER));
	CONNECT_BACK(after_a_passage, pre_bracelet_room, OR(I(SWITCH6A)), NO);
	CONNECT_BACK(pre_bracelet_room, bracelet_room, AND(ENEMY(MINI_MOLDORM), ENEMY(WIZROBE)), NO);
	CONNECT_BACK(bracelet_room, bracelet_room_chest3, NONE, NO);
	CONNECT_BACK(bracelet_room, entrance, COUNT(POWER_BRACELET, 2), NO);
	/* west */
	CONNECT_BACK(entrance, after_potbutton_door, I(POWER_BRACELET), NONE);
	CONNECT_ALL_BACK(entrance, TARGETS(fenced_walkway_switched, potbutton_area_switched), I(SWITCH6A_RANGE), NO);
	CONNECT_BACK(fenced_walkway_switched, potbutton_area_switched, I(POWER_BRACELET), NO);
	CONNECT_BACK(potbutton_area_switched, three_wizrobe_area_switched, I(POWER_BRACELET), NONE);
	CONNECT_BACK(after_potbutton_door, three_wizrobe_area_clear, OR(I(BOMB), I(MAGIC_ROD)), NO);
	CONNECT(three_wizrobe_area, after_potbutton_door, I(SWITCH6B_RANGE));
	CONNECT_BACK(three_wizrobe_area, three_wizrobe_area_switch_midrange,
		     OR(I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(HOOKSHOT)), NO);
	CONNECT_BACK(three_wizrobe_area, three_wizrobe_area_switch_range,
		     OR(I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG)), NO);
	CONNECT_BACK(three_wizrobe_area, three_wizrobe_area_switched, I(SWITCH6B_MIDRANGE),
		     OR(I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG)));
	loop[0] = three_wizrobe_area;
	loop[1] = three_wizrobe_area_switched;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], three_wizrobe_area_clear, ENEMY(WIZROBE), NO);
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], three_wizrobe_area_chest5, NONE, NO);
	loop[0] = after_potbutton_door;
	loop[1] = three_wizrobe_area_switched;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], after_potbutton_door_chest4, I(D6_THREE_WIZROBE_CLEAR), NO);
	CONNECT_BACK(three_wizrobe_area_switched, star_area_switched, I(POWER_BRACELET), NONE);
	/* northwest */
	CONNECT_BACK(star_area, three_wizrobe_area, NONE, NO);
	loop[0] = star_area;
	loop[1] = star_area_switched;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], star_area_switch, R->hit_switch, NO);
	CONNECT(star_area_switched, star_area, I(SWITCH6C));
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], star_area_chest6, NONE, NO);
	CONNECT_BACK(star_area, star_area_drop1, OR(ENEMY(WIZROBE), I(BOW)), NO);
	CONNECT_BACK(star_area, top_left_room, COUNT(POWER_BRACELET, 2), I(POWER_BRACELET));
	CONNECT_BACK(top_left_room, top_left_room_chest7, NONE, NO);
	CONNECT_BACK(top_left_room, top_left_room_switch, R->hit_switch, NO);
	CONNECT(top_left_room, top_left_exit, NONE);
	CONNECT(top_left_exit, rapids_island, NONE);
	/* miniboss */
	CONNECT(entrance, before_miniboss, FOUND(KEY6, 1));
	CONNECT(before_miniboss, miniboss_room, I(BOMB));
	CONNECT_BACK(miniboss_room, entrance, MINIBOSS(5), NO);
	CONNECT_BACK(miniboss_room, after_miniboss, MINIBOSS(5), COUNT(POWER_BRACELET, 2));
	CONNECT_BACK(after_miniboss, before_b_passage, COUNT(POWER_BRACELET, 2), NONE);
	CONNECT_BACK(after_miniboss, before_miniboss, FOUND(KEY6, 3), NO);
	/* center */
	CONNECT(before_b_passage, after_b_passage, I(FEATHER));
	CONNECT_BACK(after_b_passage, after_b_passage_drop2, NONE, NO);
	CONNECT(after_b_passage, flying_bomb_room, FOUND(KEY6, 2));
	CONNECT_BACK(flying_bomb_room, before_c_passage, COUNT(POWER_BRACELET, 2), I(POWER_BRACELET));
	CONNECT(before_c_passage, after_c_passage, I(PEGASUS_BOOTS));
	/* southeast */
	CONNECT_BACK(entrance, four_wizrobe_room, COUNT(POWER_BRACELET, 2), NO);
	CONNECT_ALL_BACK(four_wizrobe_room, TARGETS(entrance, blade_trap_room, south_waterway), ENEMY(WIZROBE), NO);
	CONNECT(south_waterway, north_waterway, OR(ENEMY(TEKTITE), I(FEATHER)));
	CONNECT_BACK(blade_trap_room, four_wizrobe_room, ENEMY(STAR), NO);
	CONNECT_BACK(blade_trap_room, after_blade_trap,
		     AND(I(FEATHER), OR(I(SWITCH6A), I(SWITCH6E), I(SWITCH6F))), NONE);
	CONNECT_ALL_BACK(after_blade_trap, TARGETS(after_blade_trap_chest8, four_wizrobe_room), NONE, NO);
	CONNECT_BACK(north_waterway, north_waterway_chest9, NONE, NO);
	loop[0] = hookshot_block;
	loop[1] = north_waterway_east_ledge;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(loop[i], north_waterway, NONE, NO);
	/* northeast */
	CONNECT_BACK(after_c_passage, south_spark_pot_maze, ENEMY(POLS_VOICE), NONE);
	CONNECT_BACK(after_c_passage, dodongo_room, ENEMY(POLS_VOICE), OR(I(FEATHER), MINIBOSS_NAMED(DODONGO)));
	CONNECT_BACK(south_spark_pot_maze, north_spark_pot_maze,
		     AND(I(POWER_BRACELET),
			 OR(I(FEATHER), AND(ENEMY(SPARK_COUNTER_CLOCKWISE), ENEMY(SPARK_CLOCKWISE)))),
		     NO);
	CONNECT(north_spark_pot_maze, top_right_room, I(POWER_BRACELET));
	CONNECT_BACK(top_right_room, top_right_room_chest10, NONE, NO);
	CONNECT_BACK(top_right_room, top_right_room_switch, R->hit_switch, NO);
	CONNECT_BACK(dodongo_room, north_waterway_east_ledge, MINIBOSS_NAMED(DODONGO), NONE);
	CONNECT_BACK(north_waterway_east_ledge, hookshot_block, I(HOOKSHOT), NO);
	CONNECT(hookshot_block, north_waterway_west_ledge, FOUND(KEY6, 3));
	CONNECT_BACK(north_waterway_west_ledge, pot_area, I(POWER_BRACELET), NO);
	CONNECT_BACK(pot_area, pot_area_chest11, I(POWER_BRACELET), NO);
	CONNECT_BACK(pot_area, pot_area_switch, R->hit_switch, NO);
	/* boss */
	CONNECT_BACK(after_b_passage, vacuum_room, NONE, NO);
	CONNECT_BACK(vacuum_room, entrance, I(FEATHER), NO);
	CONNECT_BACK(vacuum_room, laser_turret_room, NONE, NO);
	CONNECT_BACK(laser_turret_room, pre_boss_room,
		     AND(I(FEATHER), OR(I(SWORD), I(SHIELD), I(HOOKSHOT), I(BOOMERANG), ENEMY(WIZROBE))), NO);
	CONNECT_BACK(pre_boss_room, boss_room, I(NIGHTMARE_KEY6), NO);
	CONNECT_ALL_BACK(boss_room, TARGETS(boss_room_drop3, instrument), BOSS(5), NO);
	/* owl statues */
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		loop[0] = entrance;
		loop[1] = fenced_walkway_switched;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], entrance_owl1, I(STONE_BEAK6), NO);
		if (HARD_OR_ABOVE)
			CONNECT_BACK(blade_trap_room, blade_trap_room_owl2, I(STONE_BEAK6), NO);
		else
			CONNECT_BACK(blade_trap_room, blade_trap_room_owl2, AND(I(POWER_BRACELET), I(STONE_BEAK6)), NO);
		CONNECT_BACK(pot_area, pot_area_owl3, I(STONE_BEAK6), NO);
	}
	/* normal */
	if (!LOGIC_IS("casual"))
		CONNECT(entrance, entrance_switch_range, OR(I(BOMB), I(BOOMERANG)));
	/* hard */
	if (HARD_OR_ABOVE) {
		CONNECT_BACK(entrance, entrance_switch, AND(R->stun_mask_mimic, R->throw_enemy), NO);
		CONNECT(before_a_passage, after_a_passage, NONE);
		CONNECT_BACK(three_wizrobe_area, three_wizrobe_area_switch_midrange, AND(R->stun_wizrobe, R->throw_enemy),
			     NO);
		loop[0] = three_wizrobe_area;
		loop[1] = three_wizrobe_area_switched;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(loop[i], three_wizrobe_area_clear,
				     AND(OR(I(FEATHER), AND(R->stun_wizrobe, R->throw_enemy)), I(BOW)), NO);
		CONNECT_ALL_BACK(four_wizrobe_room, TARGETS(entrance, blade_trap_room, south_waterway),
				 AND(OR(I(FEATHER), AND(R->stun_wizrobe, R->throw_enemy)), I(BOW)), NO);
		CONNECT_BACK(south_waterway, north_waterway, OR(R->throw_pot, R->damage_boost), R->damage_boost);
		CONNECT_BACK(before_b_passage, after_b_passage, R->boots_dash_2d, AND(R->boots_dash_2d, R->boots_bonk));
		CONNECT_BACK(after_c_passage, south_spark_pot_maze,
			     AND(R->throw_pot, OR(I(BOW), AND(R->stun_wizrobe, R->throw_enemy))), NO);
		CONNECT_BACK(after_c_passage, dodongo_room,
			     AND(R->throw_pot, OR(I(BOW), AND(R->stun_wizrobe, R->throw_enemy))), NO);
		CONNECT(after_c_passage, before_c_passage, R->damage_boost);
		CONNECT(south_spark_pot_maze, north_spark_pot_maze, I(POWER_BRACELET));
		CONNECT_BACK(laser_turret_room, pre_boss_room,
			     AND(OR(I(FEATHER), R->diagonal_walk),
				 OR(I(SWORD), I(SHIELD), I(HOOKSHOT), I(BOOMERANG), AND(R->stun_wizrobe, R->throw_enemy),
				    ENEMY(WIZROBE))),
			     NO);
		CONNECT_BACK(pot_area, pot_area_switch, R->throw_pot, NO);
	}
	/* glitched */
	if (GLITCHED_OR_ABOVE) {
		CONNECT_BACK(first_elephant_room, second_elephant_room, AND(ENEMY(MINI_MOLDORM), R->bomb_trigger), NO);
		CONNECT(potbutton_area_switched, fenced_walkway_switched, AND(I(POWER_BRACELET), R->shaq_jump));
		CONNECT_BACK(after_potbutton_door, three_wizrobe_area, R->super_jump_feather, NO);
		CONNECT_BACK(three_wizrobe_area, star_area, AND(R->super_jump_feather, I(POWER_BRACELET)), NO);
		CONNECT_BACK(potbutton_area_switched, entrance, R->shaq_jump, NO);
		CONNECT_BACK(after_c_passage, dodongo_room, R->super_jump_feather, NO);
		CONNECT_BACK(after_miniboss, before_b_passage, AND(R->bomb_trigger, MINIBOSS(5), FOUND(KEY6, 1)), NO);
		CONNECT_BACK(flying_bomb_room, after_b_passage, OR(R->shaq_jump, R->super_jump_feather), NO);
		CONNECT_BACK(flying_bomb_room, entrance, R->super_jump_feather, NO);
		CONNECT_BACK(flying_bomb_room, fenced_walkway_switched,
			     AND(R->super_jump_feather, OR(I(SWITCH6A), I(SWITCH6E), I(SWITCH6F))), NO);
		CONNECT_BACK(north_waterway, north_waterway_east_ledge, R->super_jump, NO);
		CONNECT_BACK(north_waterway, north_waterway_west_ledge, R->super_jump_feather, NO);
	}
	/* hell */
	if (LOGIC_IS("hell")) {
		CONNECT_BACK(entrance, second_elephant_room,
			     AND(OR(R->super_jump_feather, R->boots_superhop), OR(I(SWORD), I(HOOKSHOT), I(POWER_BRACELET), I(SHOVEL)),
				 OR(R->sword_poke, R->shield_bump)),
			     NO);
		CONNECT_BACK(entrance, entrance_switch_range, OR(AND(R->stun_mask_mimic, R->throw_enemy), I(SWORD)), NO);
		CONNECT_BACK(entrance, potbutton_area_switched,
			     AND(I(SWITCH6A), OR(R->boots_superhop, AND(R->super_jump_feather, R->boots_jump))),
			     R->boots_superhop);
		CONNECT_BACK(after_potbutton_door, three_wizrobe_area, R->boots_superhop,
			     OR(R->boots_superhop, R->super_jump_boots, I(SWITCH6B_RANGE)));
		CONNECT_BACK(three_wizrobe_area, three_wizrobe_area_switch_midrange, I(SWORD), NO);
		CONNECT_BACK(three_wizrobe_area, three_wizrobe_area_switch_range,
			     OR(AND(R->stun_wizrobe, R->throw_enemy), R->sword_beam, AND(I(HOOKSHOT), I(FEATHER))), NO);
		CONNECT_BACK(three_wizrobe_area, star_area, AND(R->boots_superhop, I(POWER_BRACELET)), NO);
		CONNECT_BACK(before_b_passage, after_b_passage, R->damage_boost_special, NO);
		CONNECT_BACK(blade_trap_room, after_blade_trap, R->boots_superhop, NO);
		CONNECT_BACK(north_waterway, north_waterway_east_ledge, R->super_jump_feather, NO);
		CONNECT_BACK(dodongo_room, after_c_passage, R->boots_bonk, NO);
		CONNECT_BACK(vacuum_room, entrance, NONE, NO);
		/* connections that require overworld-only replenishable items */
		if (!OPTION(OVERWORLD, "alttp") && !OPTION(OVERWORLD, "dungeondive")) {
			CONNECT_BACK(entrance, second_elephant_room,
				     AND(OR(R->super_jump_feather, R->boots_superhop), I(TOADSTOOL2),
					 OR(R->sword_poke, R->shield_bump)),
				     NO);
			CONNECT_BACK(after_b_passage, before_b_passage, R->toadstool_bounce_2d_spikepit, NO);
		}
	}

	return entrance;
}
