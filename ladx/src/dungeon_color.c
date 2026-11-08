#include "dungeon_build.h"

#define DUNGEON 0

int ladx_dungeon_color(const LadxDungeonContext *c)
{
	int entrance = LOC("D0 Entrance");
	int main_room = LOC("D0 Main Area");
	int main_room_chest1 = ADD(LOC(NULL), DUNGEON_CHEST, 0x30F);
	int main_room_chest2 = ADD(LOC(NULL), DUNGEON_CHEST, 0x311);
	int camo_pit_room = LOC("D0 Camo Pit Room");
	int bone_putter_room = LOC("D0 Bone Putter Room");
	int bone_putter_room_chest3 = ADD(LOC(NULL), DUNGEON_CHEST, 0x314);
	int main_room_drop1 = ADD(LOC(NULL), DROPPED_KEY, 0x308);
	int main_room_owl1 = ADD(LOC(NULL), OWL_STATUE, 0x30F);
	int main_room_owl2 = ADD(LOC(NULL), OWL_STATUE, 0x308);
	int ne_miniboss = LOC("D0 NE Miniboss");
	int ne_miniboss_chest4 = ADD(LOC(NULL), DUNGEON_CHEST, 0x302);
	int center_miniboss = LOC("D0 Center Miniboss");
	int button_room = LOC("D0 Hidden Button Room");
	int button_north = LOC("D0 Bullshit Room");
	int north_room_chest5 = ADD(LOC(NULL), DUNGEON_CHEST, 0x306);
	int north_room_drop2 = ADD(LOC(NULL), DROPPED_KEY, 0x307);
	int button_west = LOC("D0 3x3 Puzzle Room");
	int west_room_owl3 = ADD(LOC(NULL), OWL_STATUE, 0x30A);
	int fourzol_room = LOC("D0 Four Zol Room");
	int switch_room = LOC("D0 Room Before Boss");
	int pre_boss = LOC("D0 Outside Boss Door");
	int boss_room = LOC("D0 Boss Room");
	int fairy_rewards = ADD_INDEX(ADD_INDEX(LOC("D0 Fairy Room"), TUNIC_FAIRY, 0), TUNIC_FAIRY, 1);

	if (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "dungeon")) {
		CONNECT_BACK(main_room, main_room_owl1, I(STONE_BEAK0), NO);
		CONNECT_BACK(main_room, main_room_owl2, I(STONE_BEAK0), NO);
		CONNECT_BACK(button_west, west_room_owl3, I(STONE_BEAK0), NO);
	}

	CONNECT_BACK(entrance, main_room, AND(ENEMY(COLOR_GHOUL_GREEN), ENEMY(COLOR_GHOUL_RED)), NO);
	CONNECT_ALL_BACK(main_room, TARGETS(main_room_chest1, main_room_drop1), R->hit_switch_color, NO);
	CONNECT_BACK(main_room, main_room_chest2, AND(I(POWER_BRACELET), R->attack_hookshot), NO);
	CONNECT_BACK(main_room, camo_pit_room, NONE, NO);
	CONNECT_BACK(main_room, ne_miniboss, FOUND(KEY0, 3), NO);
	CONNECT_BACK(main_room, center_miniboss, FOUND(KEY0, 2), NO);
	CONNECT_BACK(camo_pit_room, bone_putter_room, AND(ENEMY(COLOR_GHOUL_GREEN), ENEMY(COLOR_GHOUL_RED)), NO);
	CONNECT_ALL_BACK(bone_putter_room, TARGETS(main_room, bone_putter_room_chest3), NONE, NO);
	CONNECT_BACK(ne_miniboss, ne_miniboss_chest4, MINIBOSS(MAPPING_C2), NO);
	CONNECT_BACK(center_miniboss, button_room, MINIBOSS(MAPPING_C1), NO);
	CONNECT_ALL_BACK(button_room, TARGETS(button_north, button_west), I(POWER_BRACELET), NO);
	CONNECT_BACK(button_north, north_room_chest5, AND(ENEMY(ZOL), ENEMY(HIDING_ZOL)), NO);
	CONNECT_BACK(button_north, north_room_drop2, AND(R->attack_hookshot, I(POWER_BRACELET)), NO);
	CONNECT_BACK(button_west, fourzol_room, R->hit_switch_color, NO);
	CONNECT_BACK(fourzol_room, switch_room, FOUND(KEY0, 3), NO);
	CONNECT_BACK(switch_room, pre_boss, OR(R->hit_switch, AND(I(PEGASUS_BOOTS), I(FEATHER))), NO);
	CONNECT_BACK(pre_boss, boss_room, I(NIGHTMARE_KEY0), NO);
	CONNECT_BACK(boss_room, fairy_rewards, BOSS(8), NO);

	if (HARD_OR_ABOVE) {
		CONNECT_BACK(entrance, main_room, R->throw_pot, NO);
		CONNECT_BACK(main_room, main_room_chest2, R->attack_hookshot_no_bomb, NO);
		CONNECT_BACK(camo_pit_room, bone_putter_room, R->throw_pot, NO);
		CONNECT_BACK(button_north, north_room_chest5, R->throw_pot, NO);
		CONNECT_BACK(switch_room, pre_boss, R->tight_jump, NO);
	}

	if (LOGIC_IS("hell")) {
		CONNECT_BACK(main_room, main_room_chest2, OR(I(BOMB), R->shield_bump), NO);
		CONNECT_BACK(button_north, north_room_drop2, OR(I(BOMB), R->shield_bump), NO);
	}

	return entrance;
}
