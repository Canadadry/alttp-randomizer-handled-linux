#include "dungeon_build.h"

/* logic/dungeon1.py Dungeon1 at 4325207e, line for line. */

#define DUNGEON 1

int ladx_dungeon1(const LadxDungeonContext *c)
{
	/* locations */
	int entrance = LOC("D1 Entrance");
	int entrance_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x116);
	int entrance_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x115);
	int entrance_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x113);
	int entrance_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x114);
	int cracked_pit_room = LOC("D1 Cracked Pit Room");
	int main_room = LOC("D1 Main Area");
	int main_room_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x10E);
	int main_room_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x10D);
	int pre_keyblock = LOC("D1 Before Key Block");
	int fenced_walkway = LOC("D1 Fenced Area");
	int fenced_walkway_chest6 = ADD(LOC(NULL), DUNGEON_CHEST, 0x108);
	int bombable_room = LOC("Bombable Wall Room");
	int bombable_room_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x10C);
	int north_room = LOC("D1 North Room");
	int north_room_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x104);
	int northwest_room = LOC("D1 Spiked Beetle Room");
	int northwest_room_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x103);
	int before_a_passage = LOC("D1 Passage Spawn");
	int after_a_passage = LOC("D1 Feather Room");
	int after_a_passage_chest8 = ADD(LOC(NULL), DUNGEON_CHEST, 0x11D);
	int east_room = LOC("D1 East Area");
	int east_room_chest7 = ADD(LOC(NULL), DUNGEON_CHEST, 0x10A);
	int east_room_owl3 = ADD(LOC(NULL), OWL_STATUE, 0x10A);
	int miniboss_room = LOC("D1 Miniboss");
	int fourblade_room = LOC("D1 After Miniboss");
	int boss_room = LOC("D1 Boss Room");
	int boss_basement = LOC("D1 Boss Basement");
	int boss_room_drop2 = ADD(LOC(NULL), HEART_CONTAINER, 0x106);
	int instrument = ADD(LOC("D1 Instrument Room"), INSTRUMENT, 0x102);

	/* connections */
	/* entrance */
	CONNECT_BACK(entrance, entrance_drop1, OR(ENEMY(HARDHAT_BEETLE), R->push_hardhat), NO);
	CONNECT_ALL_BACK(entrance, TARGETS(entrance_chest1, entrance_chest2), NONE, NO);
	CONNECT_BACK(entrance, entrance_chest3, AND(ENEMY(STALFOS_EVASIVE), ENEMY(KEESE)), NO);
	CONNECT_BACK(entrance, main_room, NONE, NO);
	CONNECT_BACK(entrance, cracked_pit_room, NONE, ENEMY(KEESE));
	/* main area */
	CONNECT_BACK(main_room, main_room_chest4, NONE, NO);
	CONNECT_BACK(main_room, main_room_chest5, OR(ENEMY(MINI_MOLDORM), I(SHIELD)), NO);
	CONNECT(main_room, pre_keyblock, I(FEATHER));
	CONNECT(main_room, north_room, FOUND(KEY1, 3));
	CONNECT(main_room, east_room, FOUND(KEY1, 3));
	CONNECT(pre_keyblock, fenced_walkway, FOUND(KEY1, 3));
	CONNECT_BACK(fenced_walkway, fenced_walkway_chest6, NONE, NO);
	CONNECT(main_room, bombable_room, I(BOMB));
	CONNECT_BACK(bombable_room, bombable_room_chest7, NONE, NO);
	/* northwest */
	CONNECT(north_room, northwest_room, NONE);
	CONNECT_BACK(northwest_room, before_a_passage, ENEMY(SPIKED_BEETLE), NONE);
	CONNECT(before_a_passage, after_a_passage, NONE);
	CONNECT_BACK(after_a_passage, after_a_passage_chest8, NONE, NO);
	/* boss */
	CONNECT_BACK(east_room, east_room_chest7, ENEMY(THREE_OF_A_KIND), NO);
	CONNECT_BACK(east_room, miniboss_room, I(FEATHER), AND(I(FEATHER), MINIBOSS(0)));
	CONNECT_BACK(miniboss_room, entrance, MINIBOSS(0), NO);
	CONNECT_BACK(miniboss_room, fourblade_room, MINIBOSS(0), NONE);
	CONNECT_BACK(fourblade_room, boss_room, I(NIGHTMARE_KEY1), BOSS(0));
	CONNECT_BACK(boss_room, boss_basement, NONE, NO);
	CONNECT(boss_basement, fourblade_room, NONE);
	CONNECT_ALL_BACK(boss_room, TARGETS(boss_room_drop2, instrument), BOSS(0), NO);
	/* owl statues */
	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		CONNECT_BACK(north_room, north_room_owl1, I(STONE_BEAK1), NO);
		CONNECT_BACK(northwest_room, northwest_room_owl2, I(STONE_BEAK1), NO);
		CONNECT_BACK(east_room, east_room_owl3, I(STONE_BEAK1), NO);
	}
	/* good boy */
	if (!OPTION(BOWWOW, "normal"))
		CONNECT_BACK(cracked_pit_room, main_room, AND(ENEMY(KEESE), I(FEATHER)), I(FEATHER));
	else
		CONNECT_BACK(cracked_pit_room, main_room, AND(ENEMY(KEESE), I(SWORD)), I(SWORD));
	/* normal */
	if (!LOGIC_IS("casual")) {
		CONNECT_BACK(entrance, entrance_chest3, AND(OR(ENEMY(STALFOS_EVASIVE), R->shield_bump), ENEMY(KEESE)),
			     NO);
		CONNECT_BACK(northwest_room, before_a_passage, OR(ENEMY(SPIKED_BEETLE), R->shield_bump), NONE);
	}
	/* hard */
	if (HARD_OR_ABOVE) {
		CONNECT_BACK(entrance, entrance_chest3, ENEMY(KEESE), NO);
		if (!OPTION(BOWWOW, "normal"))
			CONNECT_BACK(cracked_pit_room, main_room, R->boots_bonk, NO);
	}
	/* glitched */
	if (GLITCHED_OR_ABOVE) {
		if (!OPTION(BOWWOW, "normal"))
			CONNECT_BACK(main_room, cracked_pit_room, R->pit_buffer_itemless, NO);
		CONNECT(main_room, fenced_walkway, AND(R->wall_clip, R->super_jump_feather));
		CONNECT_BACK(east_room, miniboss_room, OR(R->damage_boost, R->pit_buffer_itemless),
			     AND(R->pit_buffer_itemless, MINIBOSS(0)));
	}
	/* hell */
	if (LOGIC_IS("hell")) {
		CONNECT(main_room, pre_keyblock, R->boots_bonk);
		CONNECT_BACK(main_room, pre_keyblock, R->damage_boost, NO);
		CONNECT_BACK(northwest_room, before_a_passage, I(SWORD), NONE);
	}

	return entrance;
}
