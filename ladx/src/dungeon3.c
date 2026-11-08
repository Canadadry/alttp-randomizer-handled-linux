#include "dungeon_build.h"

#define DUNGEON 3

int ladx_dungeon3(const LadxDungeonContext *c)
{
	int entrance = LOC("D3 Entrance");
	int entrance_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x153);
	int after_pot_door = LOC("D3 After Pot Door");
	int after_pot_door_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x151);
	int slime_room = LOC("D3 Slime Room");
	int slime_room_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x14F);
	int swordstalfos_room = LOC("D3 Stalfos Pedestal Room");
	int swordstalfos_room_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x150);
	int before_a_stairs = LOC("D3 Near 1st Staircase");
	int before_a_stairs_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x14E);
	int before_a_stairs_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x14C);
	int west_hallway = LOC("D3 Long Hallway");
	int west_hallway_clear = ADD_KEY(LOC("D3 Hallway Gel Defeated"), D3_GEL_CLEAR);
	int after_a_stairs = LOC("D3 Key Door Crossroads");
	int north_4way = LOC("D3 North Key Room");
	int north_4way_drop3 = ADD(LOC(NULL), DROPPED_KEY, 0x154);
	int north_4way_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x154);
	int north_4way_switch = ADD_KEY(LOC("D3 North Key Room Switch"), SWITCH3);
	int south_4way = LOC("D3 South Key Room");
	int south_4way_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x158);
	int west_4way = LOC("D3 West Key Room");
	int west_4way_drop2 = ADD(LOC(NULL), DROPPED_KEY, 0x155);
	int before_b_stairs = LOC("D3 East Key Room");
	int after_b_stairs = LOC("D3 Main Area");
	int after_b_stairs_drop4 = ADD(LOC(NULL), DROPPED_KEY, 0x14D);
	int after_b_stairs_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x147);
	int miniboss_room = LOC("D3 Miniboss Room");
	int after_miniboss_room = LOC("D3 Miniboss Reward Room");
	int after_miniboss_room_chest10 = ADD(LOC(NULL), DUNGEON_CHEST, 0x146);
	int two_pairodd_room = LOC("D3 Two Zol, Two Pairodd Room");
	int two_pairodd_room_drop5 = ADD(LOC(NULL), DROPPED_KEY, 0x148);
	int two_zol_stalfos_room = LOC("D3 Two Zol, Stalfos Room");
	int two_zol_stalfos_room_clear =
		ADD_KEY(ADD_KEY(LOC("D3 Two Zols & Stalfos Defeated"), D3_ZOLS_CLEAR), D3_STALFOS_CLEAR);
	int fenced_walkway = LOC("D3 Fenced Walkway");
	int fenced_walkway_owl3 = ADD(LOC("Flying Bomb Owl"), OWL_STATUE, 0x140);
	int fenced_walkway_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x144);
	int north_bombwall = ADD_KEY(LOC("North Bombable Wall Open"), D3_BOMBWALL);
	int timer_bombite_room = LOC("D3 Timer Bombite Room");
	int three_zol_stalfos_room = LOC("D3 Three Zol, Stalfos Room");
	int three_zol_stalfos_room_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x142);
	int bouncing_bombite_room = LOC("D3 Bouncing Bombite Room");
	int bouncing_bombite_room_drop_6 = ADD(LOC(NULL), DROPPED_KEY, 0x141);
	int big_pit_room = LOC("D3 Bombable Wall Room");
	int ledge_pre_pit = LOC("D3 East Ledge Before Pit");
	int ledge_post_pit = LOC("D3 East Ledge After Pit");
	int ledge_post_pit_chest_9 = ADD(LOC(NULL), DUNGEON_CHEST, 0x147);
	int towards_boss1 = LOC("D3 Boss Path 1");
	int towards_boss2 = LOC("D3 Boss Path 2");
	int towards_boss3 = LOC("D3 Boss Path 3");
	int before_c_passage = LOC("D3 Boss Path 4");
	int after_c_passage = LOC("D3 Three Pairodd Room");
	int conveyor_room = LOC("D3 Room Before Boss");
	int conveyor_room_drop7 = ADD(LOC(NULL), DROPPED_KEY, 0x15B);
	int boss_room = LOC("D3 Boss Room");
	int boss_room_drop8 = ADD(LOC(NULL), HEART_CONTAINER, 0x15A);
	int instrument = ADD(LOC("D3 Instrument Room"), INSTRUMENT, 0x159);

	CONNECT_BACK(entrance, entrance_chest1, I(PEGASUS_BOOTS), NO);
	CONNECT_BACK(entrance, after_pot_door, I(POWER_BRACELET), NONE);
	CONNECT_BACK(after_pot_door, after_pot_door_chest2,
		     AND(ENEMY(GEL), ENEMY(MOBLIN_SWORD), ENEMY(BOUNCING_BOMBITE)), NO);
	CONNECT(after_pot_door, west_hallway, I(PEGASUS_BOOTS));
	CONNECT_ALL_BACK(slime_room, TARGETS(after_pot_door, swordstalfos_room, before_a_stairs), ENEMY(HIDING_ZOL),
			 NONE);
	CONNECT_BACK(slime_room, slime_room_chest3, NONE, NO);
	CONNECT_BACK(west_hallway, west_hallway_clear, ENEMY(GEL), NO);
	CONNECT_BACK(west_hallway, before_a_stairs, NONE, NO);
	CONNECT_BACK(before_a_stairs, before_a_stairs_chest5, AND(I(D3_GEL_CLEAR), ENEMY(STALFOS_EVASIVE)), NO);
	CONNECT_BACK(swordstalfos_room, swordstalfos_room_chest4, I(SWITCH3), NO);
	CONNECT_BACK(before_a_stairs, before_a_stairs_chest6, I(SWITCH3), NO);
	CONNECT(before_a_stairs, after_a_stairs, NONE);
	CONNECT_ALL_BACK(after_a_stairs, TARGETS(north_4way, south_4way, west_4way), FOUND(KEY3, 8), NO);
	CONNECT_BACK(north_4way, north_4way_drop3, AND(ENEMY(STALFOS_AGGRESSIVE), ENEMY(MOBLIN)), NO);
	CONNECT_BACK(north_4way, north_4way_switch, R->hit_switch, NO);
	CONNECT_BACK(south_4way, south_4way_drop1,
		     AND(ENEMY(HIDING_ZOL), ENEMY(MOBLIN), OR(ENEMY(PAIRODD), I(BOOMERANG))), NO);
	CONNECT_BACK(west_4way, west_4way_drop2, AND(ENEMY(HIDING_ZOL), OR(ENEMY(PAIRODD), I(BOOMERANG))), NO);
	CONNECT(after_a_stairs, before_b_stairs, FOUND(KEY3, 4));
	CONNECT(before_b_stairs, after_b_stairs, NONE);
	CONNECT_BACK(after_b_stairs, after_b_stairs_drop4, ENEMY(HIDING_ZOL), NO);
	CONNECT_BACK(after_b_stairs, two_pairodd_room, AND(ENEMY(ZOL), ENEMY(GEL)), NONE);
	CONNECT_BACK(after_b_stairs, miniboss_room, AND(ENEMY(ZOL), ENEMY(GEL)), NONE);
	CONNECT_BACK(miniboss_room, entrance, MINIBOSS(2), NO);
	CONNECT_BACK(miniboss_room, after_miniboss_room, MINIBOSS(2), NONE);
	CONNECT_ALL_BACK(after_miniboss_room, TARGETS(after_miniboss_room_chest10, after_b_stairs), NONE, NO);
	CONNECT_BACK(two_pairodd_room, two_pairodd_room_drop5, AND(ENEMY(HIDING_ZOL), ENEMY(PAIRODD)), NO);
	CONNECT_BACK(two_pairodd_room, two_zol_stalfos_room, NONE, NO);
	CONNECT_BACK(two_zol_stalfos_room, two_zol_stalfos_room_clear, AND(ENEMY(ZOL), ENEMY(STALFOS_AGGRESSIVE)), NO);
	CONNECT_BACK(two_zol_stalfos_room, fenced_walkway, NONE, NO);
	CONNECT_BACK(fenced_walkway, fenced_walkway_chest7, AND(I(D3_ZOLS_CLEAR), I(D3_STALFOS_CLEAR)), NO);
	CONNECT_BACK(fenced_walkway, north_bombwall, I(BOMB), NO);
	CONNECT_ALL_BACK(timer_bombite_room, TARGETS(after_b_stairs, three_zol_stalfos_room),
			 AND(ENEMY(HIDING_ZOL), ENEMY(TIMER_BOMBITE)), NONE);
	CONNECT_BACK(three_zol_stalfos_room, three_zol_stalfos_room_chest8, NONE, NO);
	CONNECT_BACK(three_zol_stalfos_room, north_bombwall, I(BOMB), NO);
	CONNECT(three_zol_stalfos_room, bouncing_bombite_room, I(D3_BOMBWALL));
	CONNECT_BACK(bouncing_bombite_room, bouncing_bombite_room_drop_6, ENEMY(BOUNCING_BOMBITE), NO);
	CONNECT(after_b_stairs, big_pit_room, I(BOMB));
	CONNECT(big_pit_room, ledge_pre_pit, AND(I(FEATHER), I(PEGASUS_BOOTS)));
	CONNECT_ALL_BACK(ledge_pre_pit, TARGETS(after_b_stairs, big_pit_room), NONE, NO);
	CONNECT_BACK(ledge_pre_pit, ledge_post_pit, OR(I(HOOKSHOT), I(FEATHER)), NO);
	CONNECT_ALL_BACK(ledge_post_pit, TARGETS(after_b_stairs, ledge_post_pit_chest_9), NONE, NO);
	CONNECT_BACK(after_b_stairs, towards_boss1, FOUND(KEY3, 5), FOUND(KEY3, 4));
	CONNECT_BACK(towards_boss1, towards_boss2, FOUND(KEY3, 6), FOUND(KEY3, 3));
	CONNECT_BACK(towards_boss2, towards_boss3, FOUND(KEY3, 7), FOUND(KEY3, 2));
	CONNECT_BACK(towards_boss3, before_c_passage, FOUND(KEY3, 8), FOUND(KEY3, 1));
	CONNECT(before_c_passage, after_c_passage, AND(I(FEATHER), I(PEGASUS_BOOTS)));
	CONNECT_BACK(after_c_passage, conveyor_room, ENEMY(PAIRODD), NO);
	CONNECT_BACK(conveyor_room, conveyor_room_drop7, ENEMY(KEESE), NO);
	CONNECT_BACK(conveyor_room, boss_room, I(NIGHTMARE_KEY3), NO);
	CONNECT_ALL_BACK(boss_room, TARGETS(boss_room_drop8, instrument), BOSS(2), NO);
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		CONNECT_BACK(north_4way, north_4way_owl1, I(STONE_BEAK3), NO);
		CONNECT_BACK(after_b_stairs, after_b_stairs_owl2, I(STONE_BEAK3), NO);
		CONNECT_BACK(fenced_walkway, fenced_walkway_owl3, I(STONE_BEAK3), NO);
	}
	if (OPTION(DUNGEON_KEYS, "")) {
		CONNECT_BACK(after_a_stairs, west_4way, FOUND(KEY3, 1), NO);
		ladx_logic_force_first_item(L, west_4way_drop2, LADX_ITEM_KEY3);
		CONNECT_BACK(after_a_stairs, south_4way, FOUND(KEY3, 1), NO);
		ladx_logic_force_first_item(L, south_4way_drop1, LADX_ITEM_KEY3);
	}
	if (HARD_OR_ABOVE) {
		CONNECT_BACK(entrance, entrance_chest1, R->hookshot_over_pit, NO);
		CONNECT_BACK(north_4way, north_4way_switch, R->throw_pot, NO);
		CONNECT_BACK(south_4way, south_4way_drop1, R->throw_pot, NO);
		CONNECT_BACK(north_4way, north_4way_drop3, R->throw_pot, NO);
		CONNECT_BACK(fenced_walkway, bouncing_bombite_room_drop_6, I(BOOMERANG), NO);
		CONNECT_BACK(fenced_walkway, two_zol_stalfos_room_clear, OR(I(BOOMERANG), I(BOMB), I(BOW)), NO);
		CONNECT_BACK(fenced_walkway, north_bombwall,
			     OR(AND(I(FEATHER), OR(I(SWORD), I(MAGIC_POWDER))), I(BOW), I(MAGIC_ROD), I(BOOMERANG)), NO);
	}
	if (GLITCHED_OR_ABOVE) {
		CONNECT_BACK(before_a_stairs, west_hallway,
			     OR(AND(I(SWITCH3), R->super_jump_feather), R->hookshot_clip_block), NO);
		CONNECT_ALL_BACK(west_hallway, TARGETS(before_a_stairs, before_a_stairs_chest6),
				 OR(R->super_jump_feather, R->shaq_jump), NO);
		CONNECT_BACK(swordstalfos_room, swordstalfos_room_chest4, R->super_jump_feather, NO);
		CONNECT_BACK(big_pit_room, ledge_pre_pit, AND(R->corner_walk, R->super_jump_feather), NO);
		CONNECT_BACK(towards_boss1, miniboss_room, R->super_jump_feather, NO);
		CONNECT_BACK(towards_boss2, after_miniboss_room, R->super_jump_feather, NO);
		CONNECT_ALL_BACK(towards_boss3, TARGETS(after_miniboss_room, after_b_stairs), R->super_jump_feather, NO);
	}
	if (LOGIC_IS("hell")) {
		CONNECT_BACK(entrance, entrance_chest1, I(SWORD), NO);
		CONNECT_ALL_BACK(west_hallway, TARGETS(before_a_stairs, before_a_stairs_chest6), R->boots_superhop, NO);
		CONNECT_BACK(swordstalfos_room, swordstalfos_room_chest4, OR(R->boots_superhop, R->hookshot_clip_block),
			     NO);
		CONNECT_ALL_BACK(before_a_stairs, TARGETS(west_hallway, before_a_stairs_chest6),
				 OR(R->boots_superbump, AND(R->shaq_jump, R->super_bump)), NO);
		CONNECT_BACK(west_4way, west_4way_drop2, R->shield_bump, NO);
		CONNECT_BACK(south_4way, south_4way_drop1, R->shield_bump, NO);
		CONNECT_BACK(after_b_stairs, fenced_walkway, R->super_bump, NO);
		CONNECT_BACK(fenced_walkway, north_bombwall,
			     OR(R->sword_beam, AND(R->boots_bonk, OR(I(SWORD), I(MAGIC_POWDER)))), NO);
		CONNECT_BACK(after_b_stairs, ledge_pre_pit, AND(R->super_jump_feather, R->shield_bump), NO);
		CONNECT(big_pit_room, ledge_pre_pit, R->pit_buffer_boots);
		CONNECT_BACK(after_b_stairs, towards_boss3, R->super_bump, NO);
		CONNECT_BACK(before_c_passage, after_c_passage, R->bracelet_bounce_2d_spikepit,
			     OR(R->bracelet_bounce_2d_spikepit, R->boots_bonk_2d_hell));
		if (!OPTION(OVERWORLD, "alttp") && !OPTION(OVERWORLD, "dungeondive"))
			CONNECT_BACK(before_c_passage, after_c_passage,
				     OR(R->boots_bonk_2d_spikepit, R->toadstool_bounce_2d_spikepit),
				     R->toadstool_bounce_2d_spikepit);
	}

	return entrance;
}
