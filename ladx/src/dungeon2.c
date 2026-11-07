#include "dungeon_build.h"

/* logic/dungeon2.py Dungeon2 at 4325207e, line for line. */

#define DUNGEON 2

int ladx_dungeon2(const LadxDungeonContext *c)
{
	int i;

	/* locations */
	int entrance = LOC("D2 Entrance");
	int entrance_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x136);
	int blade_room = LOC("D2 West of Torches");
	int pitbeetle_room = LOC("D2 Hardhat Beetle Room");
	int pitbeetle_room_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x12E);
	int stalfos_room = LOC("D2 East of Torches");
	int stalfos_room_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x132);
	int mimic_room = LOC("D2 Mimic Room");
	int mimic_room_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x137);
	int statue_switch_room = LOC("D2 1st Switch Room");
	int statue_switch_room_drop2 = ADD(LOC(NULL), DROPPED_KEY, 0x134);
	int statue_switch_room_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x133);
	int locked_switch_room = LOC("D2 Locked Switch Room");
	int locked_switch_room_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x138);
	int pit_peg_room = LOC("D2 Pits & Pegs Room");
	int pit_peg_room_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x139);
	int mimic_beetle_room = LOC("D2 Mimic & Beetle Area");
	int pushblock_room = LOC("D2 Pushblock Room");
	int pushblock_room_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x12F);
	int before_a_passage = LOC("D2 Pushblock Room Passage Spawn");
	int after_a_passage = LOC("D2 Before Miniboss");
	int miniboss_room = LOC("D2 Miniboss Room");
	int after_miniboss = LOC("D2 After Miniboss");
	int between_pits = LOC("D2 After Miniboss Between Pits");
	int between_pits_owl3 = ADD(LOC(NULL), OWL_STATUE, 0x129);
	int before_b_passage = LOC("D2 Blocked Staircase");
	int vacuum_area = LOC("D2 Vacuum Mouth Area");
	int vacuum_area_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x126);
	int vacuum_area_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x121);
	int boo_room = LOC("D2 Boo Buddies Room");
	int boo_room_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x120);
	int final_switch_west = LOC("D2 Left of North Switch");
	int final_switch_east = LOC("D2 Right of North Switch");
	int final_switch_east_chest9 = ADD(LOC(NULL), DUNGEON_CHEST, 0x122);
	int after_b_passage = LOC("D2 Enemy Order Room");
	int after_b_passage_chest10 = ADD(LOC(NULL), DUNGEON_CHEST, 0x127);
	int pot_pol_room_doorway = LOC("D2 20 Pots Room Doorway");
	int pot_pol_room = LOC("D2 20 Pots Room");
	int before_c_passage = LOC("D2 Boss Passageway Spawn");
	int pre_boss_room = LOC("D2 Room Before Boss");
	int pre_boss = LOC("D2 Outside Boss Door");
	int boss_room = LOC("D2 Boss Room");
	int boss_room_drop3 = ADD(LOC(NULL), HEART_CONTAINER, 0x12B);
	int instrument = ADD(LOC("D2 Instrument Room"), INSTRUMENT, 0x12a);
	int pegs[2];

	/* connections */
	/* entrance */
	CONNECT_BACK(entrance, entrance_chest1, I(POWER_BRACELET), NO);
	CONNECT_BACK(entrance, blade_room, FOUND(KEY2, 5), NO);
	CONNECT_BACK(blade_room, pitbeetle_room, ENEMY(KEESE), NO);
	CONNECT_BACK(pitbeetle_room, pitbeetle_room_chest2, OR(I(FEATHER), I(HOOKSHOT)), NO);
	CONNECT_BACK(entrance, stalfos_room, R->fire, NONE);
	CONNECT_BACK(stalfos_room, stalfos_room_drop1, AND(ENEMY(STALFOS_EVASIVE), ENEMY(STALFOS_AGGRESSIVE)), NO);
	CONNECT_BACK(stalfos_room, mimic_room, FOUND(KEY2, 5), NO);
	CONNECT_BACK(stalfos_room, statue_switch_room, NONE, R->hit_switch);
	CONNECT_BACK(mimic_room, mimic_room_chest3, ENEMY(MASKED_MIMIC_GORIYA), NO);
	CONNECT_ALL(locked_switch_room, TARGETS(statue_switch_room, locked_switch_room_chest4), R->hit_switch);
	CONNECT_BACK(locked_switch_room, pit_peg_room, R->hit_switch, NONE);
	CONNECT(pit_peg_room, mimic_beetle_room, I(FEATHER));
	pegs[0] = mimic_beetle_room;
	pegs[1] = pit_peg_room;
	for (i = 0; i < 2; i++)
		CONNECT_BACK(pegs[i], pit_peg_room_chest5, I(FEATHER), NO);
	CONNECT_BACK(mimic_beetle_room, statue_switch_room_drop2,
		     AND(ENEMY(MASKED_MIMIC_GORIYA), I(FEATHER), R->hit_switch), NO);
	CONNECT(mimic_beetle_room, pushblock_room, FOUND(KEY2, 3));
	CONNECT(pushblock_room, before_a_passage, NONE);
	/* main */
	CONNECT(before_a_passage, after_a_passage, I(FEATHER));
	CONNECT_BACK(after_a_passage, miniboss_room, NONE, NO);
	CONNECT_BACK(miniboss_room, entrance, MINIBOSS(1), NO);
	CONNECT_BACK(miniboss_room, after_miniboss, MINIBOSS(1), NONE);
	CONNECT_ALL(between_pits, TARGETS(after_miniboss, vacuum_area), I(FEATHER));
	CONNECT_ALL(before_b_passage, TARGETS(after_miniboss, between_pits), I(POWER_BRACELET));
	CONNECT(before_b_passage, after_b_passage, I(FEATHER));
	CONNECT_ALL_BACK(vacuum_area, TARGETS(vacuum_area_chest6, vacuum_area_chest7), NONE, NO);
	CONNECT_BACK(vacuum_area, entrance, I(FEATHER), NO);
	CONNECT_BACK(vacuum_area, boo_room, FOUND(KEY2, 5), NO);
	CONNECT_BACK(boo_room, boo_room_chest8, OR(R->fire, ENEMY(BOO_BUDDY)), NO);
	CONNECT(vacuum_area, final_switch_west, I(POWER_BRACELET));
	CONNECT(final_switch_west, final_switch_east, R->hit_switch);
	CONNECT_BACK(final_switch_east, final_switch_east_chest9, NONE, NO);
	CONNECT_BACK(final_switch_east, after_b_passage, R->hit_switch, NO);
	CONNECT_BACK(after_b_passage, after_b_passage_chest10,
		     AND(ENEMY(KEESE), ENEMY(MOBLIN), OR(ENEMY(POLS_VOICE), I(POWER_BRACELET))), NO);
	CONNECT_ALL_BACK(after_b_passage, TARGETS(vacuum_area, final_switch_west, final_switch_east),
			 AND(R->hit_switch, I(FEATHER), I(POWER_BRACELET)), NO);
	/* boss */
	CONNECT(after_b_passage, pot_pol_room_doorway, FOUND(KEY2, 5));
	CONNECT(pot_pol_room_doorway, pot_pol_room, I(POWER_BRACELET));
	CONNECT_BACK(pot_pol_room, before_c_passage, AND(OR(I(POWER_BRACELET), ENEMY(POLS_VOICE)), ENEMY(ZOL)), NONE);
	CONNECT_BACK(before_c_passage, pre_boss_room, I(POWER_BRACELET), NONE);
	CONNECT_BACK(pre_boss_room, pre_boss, I(FEATHER), NO);
	CONNECT_BACK(pre_boss, boss_room, I(NIGHTMARE_KEY2), NO);
	CONNECT_ALL_BACK(boss_room, TARGETS(boss_room_drop3, instrument), BOSS(1), NO);
	/* owl statues */
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		CONNECT_BACK(statue_switch_room, statue_switch_room_owl1, I(STONE_BEAK2), NO);
		CONNECT_BACK(pushblock_room, pushblock_room_owl2, I(STONE_BEAK2), NO);
		CONNECT_BACK(between_pits, between_pits_owl3, I(STONE_BEAK2), NO);
	}
	/* normal */
	if (!LOGIC_IS("casual")) {
		CONNECT_BACK(statue_switch_room, statue_switch_room_drop2, R->rear_attack, NO);
		CONNECT_BACK(pot_pol_room, before_c_passage,
			     OR(I(POWER_BRACELET), AND(ENEMY(POLS_VOICE), ENEMY(ZOL))), NONE);
	}
	/* hard */
	if (HARD_OR_ABOVE) {
		CONNECT(before_a_passage, after_a_passage, R->boots_dash_2d);
		CONNECT_BACK(before_b_passage, vacuum_area, AND(I(POWER_BRACELET), R->corner_walk), NO);
		CONNECT(final_switch_west, final_switch_east, R->tight_jump);
		CONNECT_ALL_BACK(after_b_passage, TARGETS(final_switch_west, final_switch_east),
				 AND(R->hit_switch, R->tight_jump), NO);
		CONNECT(after_b_passage, vacuum_area, AND(I(BOOMERANG), I(POWER_BRACELET)));
		CONNECT_ALL_BACK(after_b_passage, TARGETS(final_switch_west, final_switch_east),
				 AND(I(BOOMERANG), I(HOOKSHOT)), NO);
	}
	/* glitched */
	if (GLITCHED_OR_ABOVE) {
		CONNECT_BACK(boo_room, boo_room_chest8, I(SWORD), NO);
		CONNECT_BACK(after_miniboss, before_b_passage, R->super_jump_feather, NO);
		CONNECT_BACK(final_switch_east, after_b_passage, R->pit_buffer_itemless, NO);
		CONNECT_ALL_BACK(after_b_passage, TARGETS(vacuum_area, final_switch_west, final_switch_east),
				 R->super_jump_feather, NO);
		CONNECT_BACK(pot_pol_room, pot_pol_room_doorway,
			     OR(AND(R->hookshot_clip_wall, R->super_jump_feather), R->super_jump_boots), NO);
		CONNECT_BACK(before_c_passage, pre_boss_room, I(BOMB), NO);
	}
	/* hell */
	if (LOGIC_IS("hell")) {
		CONNECT_BACK(pitbeetle_room, pitbeetle_room_chest2, R->boots_bonk_pit, NO);
		CONNECT(pit_peg_room, mimic_beetle_room, OR(R->boots_bonk_pit, R->hookshot_spam_pit));
		CONNECT_BACK(mimic_beetle_room, statue_switch_room_drop2,
			     AND(ENEMY(MASKED_MIMIC_GORIYA), OR(R->boots_bonk_pit, R->hookshot_spam_pit), R->hit_switch),
			     NO);
		pegs[0] = pit_peg_room;
		pegs[1] = mimic_beetle_room;
		for (i = 0; i < 2; i++)
			CONNECT_BACK(pegs[i], pit_peg_room_chest5, OR(R->boots_bonk_pit, R->hookshot_spam_pit), NO);
		CONNECT(before_a_passage, after_a_passage, R->bracelet_bounce_2d_spikepit);
		CONNECT_ALL(between_pits, TARGETS(after_miniboss, vacuum_area), OR(R->hookshot_spam_pit, R->boots_bonk_pit));
		CONNECT_BACK(vacuum_area, before_b_passage, AND(R->pit_buffer_itemless, I(POWER_BRACELET)), NO);
		CONNECT_BACK(vacuum_area, entrance, NONE, NO);
		CONNECT(before_b_passage, after_b_passage, R->boots_bonk_2d_hell);
		CONNECT_BACK(after_b_passage, final_switch_east, AND(OR(R->boots_superhop, I(BOOMERANG)), R->boots_bonk_pit),
			     NO);
		CONNECT(final_switch_west, final_switch_east, R->pit_buffer_boots);
		CONNECT_BACK(pot_pol_room_doorway, pot_pol_room, AND(R->hookshot_clip_block), NO);
		CONNECT_BACK(before_c_passage, pre_boss_room, R->boots_jump, NO);
		CONNECT_BACK(pre_boss_room, pre_boss, OR(R->boots_bonk_pit, R->hookshot_spam_pit), NO);
		/* connections that require overworld-only replenishable items such as "TOADSTOOL2" and "MEDICINE2" */
		if (!OPTION(OVERWORLD, "alttp") && !OPTION(OVERWORLD, "dungeondive"))
			CONNECT(before_a_passage, after_a_passage, R->toadstool_bounce_2d_spikepit);
	}

	return entrance;
}
