#include "dungeon_build.h"

#include <string.h>

#define DUNGEON -1

static const char *const entrance_names[LADX_ENTRANCE_COUNT] = {
	"d8", "phone_d8", "fire_cave_exit", "fire_cave_entrance", "madbatter_taltal", "left_taltal_entrance",
	"obstacle_cave_entrance", "left_to_right_taltalentrance", "obstacle_cave_outside_chest",
	"obstacle_cave_exit", "papahl_entrance", "papahl_exit", "rooster_house", "bird_cave", "multichest_left",
	"multichest_right", "multichest_top", "right_taltal_connector1", "right_taltal_connector2",
	"right_taltal_connector3", "right_taltal_connector4", "right_taltal_connector5", "right_taltal_connector6",
	"right_fairy", "d7", "writes_cave_left", "writes_cave_right", "writes_house", "writes_phone", "d2",
	"moblin_cave", "photo_house", "mambo", "d4", "heartpiece_swim_cave", "raft_return_exit", "raft_house",
	"raft_return_enter", "hookshot_cave", "toadstool_exit", "forest_madbatter", "toadstool_entrance",
	"crazy_tracy", "witch", "graveyard_cave_left", "graveyard_cave_right", "d0", "castle_jump_cave",
	"castle_main_entrance", "castle_upper_left", "castle_upper_right", "castle_secret_exit",
	"castle_secret_entrance", "castle_phone", "papahl_house_left", "papahl_house_right", "dream_hut",
	"rooster_grave", "shop", "madambowwow", "kennel", "start_house", "library", "ulrira", "mabe_phone",
	"trendy_shop", "prairie_left_phone", "prairie_left_cave1", "prairie_left_cave2", "prairie_left_fairy",
	"mamu", "d3", "prairie_right_phone", "seashell_mansion", "prairie_right_cave_top",
	"prairie_right_cave_bottom", "prairie_right_cave_high", "prairie_to_animal_connector",
	"animal_to_prairie_connector", "d6", "d6_connector_exit", "d6_connector_entrance", "armos_fairy",
	"armos_maze_cave", "armos_temple", "d1", "boomerang_cave", "banana_seller", "ghost_house",
	"richard_house", "richard_maze", "prairie_low_phone", "prairie_madbatter_connector_entrance",
	"prairie_madbatter_connector_exit", "prairie_madbatter", "d5", "animal_phone", "animal_house1",
	"animal_house2", "animal_house3", "animal_house4", "animal_house5", "animal_cave", "desert_cave",
};

const char *ladx_entrance_name(int k)
{
	return k >= 0 && k < LADX_ENTRANCE_COUNT ? entrance_names[k] : NULL;
}

static int entrance_find(const char *name)
{
	int k;

	for (k = 0; k < LADX_ENTRANCE_COUNT; k++)
		if (strcmp(entrance_names[k], name) == 0)
			return k;
	return -1;
}

static LadxEntranceExterior *entrance(LadxLogic *logic, LadxWorld *w, const char *name, bool inside)
{
	int k = entrance_find(name);

	if (k < 0 || !w->entrance[2 * k].added) {
		logic->broken = true;
		return NULL;
	}
	return &w->entrance[2 * k + inside];
}

static void exterior(LadxEntranceExterior *e, int location, LadxReq requirement)
{
	e->added = true;
	e->location = location;
	e->requirement = requirement;
	e->enter_set = e->exit_set = false;
	e->one_way_enter = e->one_way_exit = ladx_req_none();
}

static void add_entrance(LadxLogic *logic, LadxWorld *w, const char *name, int outside, int inside,
			 LadxReq requirement)
{
	int k = entrance_find(name);

	if (k < 0 || w->entrance[2 * k].added) {
		logic->broken = true;
		return;
	}
	exterior(&w->entrance[2 * k], outside, requirement);
	exterior(&w->entrance[2 * k + 1], inside, ladx_req_none());
}

static void add_requirement(LadxLogic *logic, LadxWorld *w, const char *name, LadxReq requirement)
{
	LadxEntranceExterior *e = entrance(logic, w, name, false);

	if (e)
		e->requirement = LADX_OR(ladx_logic_arena_mut(logic), e->requirement, requirement);
}

static void add_one_way(LadxLogic *logic, LadxReq *slot, bool *set, LadxReq requirement)
{
	*slot = *set ? LADX_OR(ladx_logic_arena_mut(logic), *slot, requirement) : requirement;
	*set = true;
}

static void add_exit_requirement(LadxLogic *logic, LadxWorld *w, const char *name, LadxReq requirement)
{
	LadxEntranceExterior *e = entrance(logic, w, name, false);

	if (e)
		add_one_way(logic, &e->one_way_exit, &e->exit_set, requirement);
}

static void add_enter_requirement(LadxLogic *logic, LadxWorld *w, const char *name, LadxReq requirement)
{
	LadxEntranceExterior *e = entrance(logic, w, name, false);

	if (e)
		add_one_way(logic, &e->one_way_enter, &e->enter_set, requirement);
}

void ladx_world_update_indoor(LadxLogic *logic, LadxWorld *w, const char *name, int location)
{
	LadxEntranceExterior *e = entrance(logic, w, name, true);

	if (!e)
		return;
	if (e->location >= 0) {
		logic->broken = true;
		return;
	}
	e->location = location;
}

static int create_shop_sanity(const LadxDungeonContext *c)
{
	if (!OPTION(SHOPSANITY, ""))
		c->logic->broken = true;
	return -1;
}

static int connected(LadxLogic *logic, int from, int to, LadxReq req, LadxReq back)
{
	ladx_logic_connect_back(logic, from, to, req, back);
	return from;
}

#define CHAIN(from, to, req) chain1(L, (from), (to), (req))
#define CHAIN_BACK(from, to, req, back) connected(L, (from), (to), (req), (back))

static int chain1(LadxLogic *logic, int from, int to, LadxReq req)
{
	return connected(logic, from, to, req, req);
}

#define ADD_ITEM(loc, kind, room, item) ladx_logic_add_to(L, (loc), LADX_SPOT_##kind, (room), LADX_ITEM_##item)
#define ENTRANCE(name, outside, inside, req) add_entrance(L, w, (name), (outside), (inside), (req))
#define ENTRANCE_REQUIREMENT(name, req) add_requirement(L, w, (name), (req))
#define ENTRANCE_EXIT(name, req) add_exit_requirement(L, w, (name), (req))
#define ENTRANCE_ENTER(name, req) add_enter_requirement(L, w, (name), (req))
#define SHOP_SANITY(index, room) create_shop_sanity(c)
#define OWLS_OVERWORLD (OPTION(OWLSTATUES, "both") || OPTION(OWLSTATUES, "overworld"))
#define MINIBOSS_MOBLIN_CAVE 10
#define MINIBOSS_ARMOS_TEMPLE 11

void ladx_overworld(const LadxDungeonContext *c, LadxWorld *w)
{
	int i;

	for (i = 0; i < 2 * LADX_ENTRANCE_COUNT; i++)
		w->entrance[i].added = false;

	int mabe_village = LOC("Mabe Village");
	CONNECT(ADD(LOC(NULL), HEART_PIECE, 0x2A4), mabe_village, R->bush);
	CONNECT(ADD(LOC(NULL), FISHING_MINIGAME, -1), mabe_village, AND(R->bush, FOUND(RUPEES, 50)));
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0A3), mabe_village, R->bush);
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0D2), mabe_village, I(PEGASUS_BOOTS));
	int marin_song = ADD(LOC(NULL), SONG, 0x092);
	CONNECT_BACK(mabe_village, marin_song, I(OCARINA), NO);
	CONNECT(ADD_ITEM(LOC(NULL), KEY_HOLE, 0x0D3, TAIL_CAVE_OPENED), mabe_village, I(TAIL_KEY));
	int rooster_cave = LOC("Rooster Cave");
	CONNECT(ADD(LOC(NULL), DROPPED_KEY, 0x1E4), rooster_cave, AND(I(OCARINA), I(SONG3)));

	int papahl_house = LOC("Papahl House");
	int mamasha_trade = ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2A6, TRADING_ITEM_RIBBON);
	CONNECT(papahl_house, mamasha_trade, I(TRADING_ITEM_YOSHI_DOLL));

	int trendy_shop = LOC("Trendy Shop");
	CONNECT(trendy_shop, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2A0, TRADING_ITEM_YOSHI_DOLL),
		FOUND(RUPEES, 50));
	int outside_trendy = LOC(NULL);
	CONNECT(outside_trendy, mabe_village, R->bush);

	ENTRANCE("papahl_house_left", mabe_village, papahl_house, NONE);
	ENTRANCE("papahl_house_right", mabe_village, papahl_house, NONE);
	ENTRANCE("rooster_grave", mabe_village, rooster_cave, COUNT(POWER_BRACELET, 2));
	ENTRANCE_EXIT("rooster_grave", NONE);
	ENTRANCE("madambowwow", mabe_village, -1, NONE);
	ENTRANCE("ulrira", mabe_village, -1, NONE);
	ENTRANCE("mabe_phone", mabe_village, SHOP_SANITY(1, 0x2CB), NONE);
	ENTRANCE("library", mabe_village, -1, NONE);
	ENTRANCE("trendy_shop", mabe_village, trendy_shop, R->bush);
	ENTRANCE("d1", mabe_village, -1, I(TAIL_CAVE_OPENED));
	ENTRANCE_EXIT("d1", NONE);

	int start_house = ADD(LOC("Start House"), START_ITEM, -1);
	ENTRANCE("start_house", mabe_village, start_house, NONE);

	int shop = LOC("Shop");
	if (OPTION(SHOPSANITY, "")) {
		CONNECT(ADD_INDEX(LOC(NULL), SHOP_ITEM, 0), shop, FOUND(RUPEES, 250));
		CONNECT(ADD_INDEX(LOC(NULL), SHOP_ITEM, 1), shop, FOUND(RUPEES, 1000 + 250));
	} else {
		CONNECT(ADD_INDEX(LOC(NULL), SHOP_ITEM, 0), shop, FOUND(RUPEES, 100));
		CONNECT(ADD_INDEX(LOC(NULL), SHOP_ITEM, 1), shop, FOUND(RUPEES, 200));
	}
	ENTRANCE("shop", mabe_village, shop, NONE);

	int dream_hut = LOC("Dream Hut");
	int dream_hut_right = CHAIN(ADD(LOC(NULL), CHEST, 0x2BF), dream_hut, ENEMY(MIMIC));
	if (!LOGIC_IS("casual"))
		CONNECT(dream_hut_right, dream_hut, OR(I(BOOMERANG), I(HOOKSHOT), I(FEATHER)));
	int dream_hut_left = CHAIN(ADD(LOC(NULL), CHEST, 0x2BE), dream_hut_right, I(PEGASUS_BOOTS));
	int outside_dream_hut = LOC(NULL);
	CONNECT(outside_dream_hut, mabe_village, I(POWER_BRACELET));
	ENTRANCE("dream_hut", outside_dream_hut, dream_hut, NONE);

	int kennel = CHAIN(LOC("Kennel"), ADD(LOC(NULL), SEASHELL, 0x2B2), I(SHOVEL));
	CONNECT(kennel, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2B2, TRADING_ITEM_DOG_FOOD),
		I(TRADING_ITEM_RIBBON));
	ENTRANCE("kennel", mabe_village, kennel, NONE);

	int sword_beach = CHAIN(ADD(LOC("Sword Beach"), BEACH_SWORD, -1), mabe_village,
				OR(R->bush, I(SHIELD), ENEMY(URCHIN)));
	int banana_seller = LOC("Banana Seller");
	CONNECT(banana_seller, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2FE, TRADING_ITEM_BANANAS),
		I(TRADING_ITEM_DOG_FOOD));
	int outside_banana_seller = LOC(NULL);
	CONNECT(outside_banana_seller, sword_beach, R->bush);
	ENTRANCE("banana_seller", outside_banana_seller, banana_seller, NONE);
	int boomerang_cave = LOC("Boomerang Cave");
	if (OPTION(BOOMERANG, "trade"))
		CONNECT(ADD(LOC(NULL), BOOMERANG_GUY, -1), boomerang_cave,
			AND(R->shuffled_magnifier, OR(I(BOOMERANG), I(HOOKSHOT), I(MAGIC_ROD), I(PEGASUS_BOOTS),
						      I(FEATHER), I(SHOVEL))));
	else if (OPTION(BOOMERANG, "gift"))
		CONNECT(ADD(LOC(NULL), BOOMERANG_GUY, -1), boomerang_cave, R->shuffled_magnifier);
	ENTRANCE("boomerang_cave", sword_beach, boomerang_cave, I(BOMB));
	ENTRANCE_EXIT("boomerang_cave", NONE);

	int sword_beach_to_ghost_hut =
		CHAIN(ADD(LOC("Sword Beach to Ghost House"), CHEST, 0x0E5), sword_beach, I(POWER_BRACELET));
	int ghost_hut_outside = CHAIN(LOC("Outside Ghost House"), sword_beach_to_ghost_hut, I(POWER_BRACELET));
	int ghost_hut_inside = CHAIN(LOC("Ghost House"), ADD(LOC(NULL), SEASHELL, 0x1E3), I(POWER_BRACELET));
	ENTRANCE("ghost_house", ghost_hut_outside, ghost_hut_inside, NONE);

	int forest = CHAIN(LOC("Forest"), mabe_village, R->bush);
	CONNECT(ADD(LOC(NULL), CHEST, 0x071), forest, I(POWER_BRACELET));
	int forest_heartpiece = ADD(LOC("Forest Heart Piece"), HEART_PIECE, 0x044);
	CONNECT_BACK(forest, forest_heartpiece, OR(I(BOOMERANG), I(FEATHER), I(HOOKSHOT), I(ROOSTER)), NO);

	int witch_hut = CHAIN(LOC("Witch's Hut"), ADD(LOC(NULL), WITCH, -1), I(TOADSTOOL));
	ENTRANCE("witch", forest, witch_hut, NONE);
	int crazy_tracy_hut = CHAIN(LOC("Outside Crazy Tracy's House"), forest, I(POWER_BRACELET));
	int crazy_tracy_hut_inside = LOC("Crazy Tracy's House");
	CONNECT(ADD_KEY(LOC("Medicine Purchase"), MEDICINE2), crazy_tracy_hut_inside, FOUND(RUPEES, 50));
	ENTRANCE("crazy_tracy", crazy_tracy_hut, crazy_tracy_hut_inside, NONE);
	CONNECT_BACK(start_house, crazy_tracy_hut, AND(I(OCARINA), I(SONG2)), NO);

	int forest_madbatter = LOC("Forest Mad Batter");
	CONNECT(ADD(LOC(NULL), MAD_BATTER, 0x1E1), forest_madbatter, I(MAGIC_POWDER));
	ENTRANCE("forest_madbatter", forest, forest_madbatter, I(POWER_BRACELET));
	ENTRANCE_EXIT("forest_madbatter", NONE);

	int forest_cave = LOC("Forest Cave");
	int forest_cave_crystal_chest = CHAIN(ADD(LOC(NULL), CHEST, 0x2BD), forest_cave, I(SWORD));
	int log_cave_heartpiece = CHAIN(ADD(LOC(NULL), HEART_PIECE, 0x2AB), forest_cave, I(POWER_BRACELET));
	int forest_toadstool = ADD(LOC(NULL), TOADSTOOL, -1);
	CONNECT(ADD_KEY(LOC("Backup Toadstool"), TOADSTOOL2), forest_toadstool, I(TOADSTOOL));
	ENTRANCE("toadstool_entrance", forest, forest_cave, NONE);
	ENTRANCE("toadstool_exit", forest_toadstool, forest_cave, NONE);

	int hookshot_cave = LOC("Hookshot Cave");
	int hookshot_cave_chest = CHAIN(ADD(LOC(NULL), CHEST, 0x2B3), hookshot_cave, OR(I(HOOKSHOT), I(ROOSTER)));
	int outside_hookshot_cave = LOC(NULL);
	CONNECT(outside_hookshot_cave, forest, I(POWER_BRACELET));
	ENTRANCE("hookshot_cave", outside_hookshot_cave, hookshot_cave, NONE);

	int swamp = CHAIN(LOC("Swamp"), forest, AND(OR(I(MAGIC_POWDER), I(FEATHER), I(ROOSTER)), R->bush));
	CONNECT_BACK(swamp, forest, R->bush, NO);
	CONNECT(swamp, forest_toadstool, OR(I(FEATHER), I(ROOSTER)));
	int swamp_chest = CHAIN(ADD(LOC("Swamp Chest"), CHEST, 0x034), swamp, ENEMY(GIANT_GOPONGA_FLOWER));
	int outside_d2 = LOC(NULL);
	CONNECT(swamp, outside_d2, ENEMY(GOPONGA_FLOWER));
	ENTRANCE("d2", outside_d2, -1, NONE);
	CONNECT(ADD(LOC(NULL), CHEST, 0x041), swamp, R->bush);
	ENTRANCE("writes_phone", swamp, SHOP_SANITY(2, 0x29B), NONE);

	int writes_hut_outside = CHAIN(LOC("Outside Write's House"), swamp, OR(I(FEATHER), I(ROOSTER)));
	int writes_house = LOC("Write's House");
	CONNECT(writes_house, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2a8, TRADING_ITEM_BROOM),
		I(TRADING_ITEM_LETTER));
	ENTRANCE("writes_house", writes_hut_outside, writes_house, NONE);
	if (OWLS_OVERWORLD)
		ADD(writes_hut_outside, OWL_STATUE, 0x11);
	int writes_cave = LOC("Write's Cave");
	int writes_cave_left_chest =
		CHAIN(ADD(LOC(NULL), CHEST, 0x2AE), writes_cave, OR(I(FEATHER), I(ROOSTER), I(HOOKSHOT)));
	CONNECT(ADD(LOC(NULL), CHEST, 0x2AF), writes_cave, I(POWER_BRACELET));
	ENTRANCE("writes_cave_left", writes_hut_outside, writes_cave, NONE);
	ENTRANCE("writes_cave_right", writes_hut_outside, writes_cave, NONE);

	int graveyard = CHAIN(LOC("Graveyard"), forest, OR(I(FEATHER), I(ROOSTER), I(POWER_BRACELET)));
	if (OWLS_OVERWORLD)
		ADD(graveyard, OWL_STATUE, 0x035);
	ENTRANCE("photo_house", graveyard, -1, NONE);
	ENTRANCE("d0", graveyard, -1, I(POWER_BRACELET));
	ENTRANCE_EXIT("d0", NONE);
	int ghost_grave = CHAIN(LOC("Ghost Grave"), forest, I(POWER_BRACELET));
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x074), ghost_grave, AND(R->bush, I(SHOVEL)));
	CONNECT_BACK(graveyard, forest_heartpiece, OR(I(BOOMERANG), I(HOOKSHOT)), NO);

	int graveyard_cave_left = LOC("Graveyard Cave West");
	int graveyard_cave_right = CHAIN(LOC("Graveyard Cave East"), graveyard_cave_left, OR(I(FEATHER), I(ROOSTER)));
	int graveyard_heartpiece =
		CHAIN(ADD(LOC(NULL), HEART_PIECE, 0x2DF), graveyard_cave_right,
		      OR(AND(I(BOMB), OR(I(HOOKSHOT), I(PEGASUS_BOOTS)), I(FEATHER)), I(ROOSTER)));
	int outside_graveyard_left = LOC("Inside Ghost Grave Rock Circle");
	CONNECT(ghost_grave, outside_graveyard_left, I(POWER_BRACELET));
	ENTRANCE("graveyard_cave_left", outside_graveyard_left, graveyard_cave_left, NONE);
	ENTRANCE("graveyard_cave_right", graveyard, graveyard_cave_right, NONE);
	int moblin_cave = CHAIN(LOC("Moblin Cave"), ADD(LOC(NULL), CHEST, 0x2E2),
				AND(ENEMY(MOBLIN_SWORD), ENEMY(MOBLIN), MINIBOSS(MINIBOSS_MOBLIN_CAVE)));
	ENTRANCE("moblin_cave", graveyard, moblin_cave, NONE);

	int ukuku_prairie =
		CHAIN(CHAIN(LOC("Ukuku Prairie"), mabe_village, I(POWER_BRACELET)), graveyard, I(POWER_BRACELET));
	CONNECT(ukuku_prairie, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x07B, TRADING_ITEM_STICK),
		I(TRADING_ITEM_BANANAS));
	CONNECT(ukuku_prairie, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x087, TRADING_ITEM_HONEYCOMB),
		I(TRADING_ITEM_STICK));
	CONNECT(ukuku_prairie, ADD_ITEM(LOC(NULL), KEY_HOLE, 0x0B5, KEY_CAVERN_OPENED), I(SLIME_KEY));
	ENTRANCE("prairie_left_phone", ukuku_prairie, SHOP_SANITY(3, 0x2B4), NONE);
	ENTRANCE("prairie_right_phone", ukuku_prairie, SHOP_SANITY(4, 0x29C), NONE);
	ENTRANCE("prairie_left_cave1", ukuku_prairie, ADD(LOC(NULL), CHEST, 0x2CD), NONE);
	ENTRANCE("prairie_left_fairy", ukuku_prairie, -1, I(BOMB));
	ENTRANCE_EXIT("prairie_left_fairy", NONE);

	int prairie_left_cave2 = LOC("Boots 'n' Bomb Cave");
	CONNECT(ADD(LOC(NULL), CHEST, 0x2F4), prairie_left_cave2, I(PEGASUS_BOOTS));
	CONNECT(ADD(LOC(NULL), HEART_PIECE, 0x2E5), prairie_left_cave2, AND(I(BOMB), I(PEGASUS_BOOTS)));
	ENTRANCE("prairie_left_cave2", ukuku_prairie, prairie_left_cave2, I(BOMB));
	ENTRANCE_EXIT("prairie_left_cave2", NONE);

	int mamu = CHAIN(LOC("Mamu"), ADD(LOC(NULL), SONG, 0x2FB), AND(I(OCARINA), FOUND(RUPEES, 1300)));
	ENTRANCE("mamu", ukuku_prairie, mamu,
		 AND(OR(AND(I(FEATHER), I(PEGASUS_BOOTS)), I(ROOSTER)), OR(I(HOOKSHOT), I(ROOSTER)),
		     I(POWER_BRACELET)));

	int dungeon3_entrance = CHAIN(LOC("Outside D3"), ukuku_prairie, OR(I(FEATHER), I(ROOSTER), I(FLIPPERS)));
	ENTRANCE("d3", dungeon3_entrance, -1, I(KEY_CAVERN_OPENED));
	ENTRANCE_EXIT("d3", NONE);
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0A5), dungeon3_entrance, I(SHOVEL));
	CONNECT_BACK(dungeon3_entrance, ukuku_prairie, NONE, NO);

	int prairie_island_seashell =
		CHAIN(ADD(LOC(NULL), SEASHELL, 0x0A6), ukuku_prairie, AND(I(FLIPPERS), R->bush));
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x08B), ukuku_prairie, R->bush);
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0A4), ukuku_prairie, I(PEGASUS_BOOTS));
	ENTRANCE("castle_jump_cave", ukuku_prairie, ADD(LOC(NULL), CHEST, 0x1FD), I(ROOSTER));
	if (!ladx_setting_on(c->options, LADX_SETTING_ROOSTER))
		ENTRANCE_REQUIREMENT("castle_jump_cave", AND(I(FEATHER), I(PEGASUS_BOOTS)));
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0B9), ukuku_prairie, I(POWER_BRACELET));

	int left_bay_area = LOC("Western Bay");
	CONNECT(left_bay_area, ghost_hut_outside, OR(AND(I(FEATHER), I(PEGASUS_BOOTS)), I(ROOSTER)));
	ENTRANCE("prairie_low_phone", left_bay_area, SHOP_SANITY(5, 0x29D), NONE);

	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0E9), left_bay_area, R->bush);
	int tiny_island =
		CHAIN(ADD(LOC(NULL), SEASHELL, 0x0F8), left_bay_area, AND(OR(I(FLIPPERS), I(ROOSTER)), R->bush));

	int prairie_plateau = LOC("Bay Plateau");
	if (OWLS_OVERWORLD)
		ADD(prairie_plateau, OWL_STATUE, 0x0A8);
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0A8), prairie_plateau, I(SHOVEL));

	int prairie_cave = LOC("Bay Cliff Cave");
	int prairie_cave_secret_exit =
		CHAIN_BACK(LOC("Bay Cliff to Plateau"), prairie_cave, OR(I(FEATHER), I(ROOSTER)), NO);
	CONNECT_BACK(prairie_cave, prairie_cave_secret_exit, AND(I(BOMB), OR(I(FEATHER), I(ROOSTER))), NO);
	ENTRANCE("prairie_right_cave_top", ukuku_prairie, prairie_cave, NONE);
	ENTRANCE("prairie_right_cave_bottom", left_bay_area, prairie_cave, NONE);
	ENTRANCE("prairie_right_cave_high", prairie_plateau, prairie_cave_secret_exit, NONE);

	int bay_madbatter_connector_entrance = LOC("Bay Batter Tunnel Entrance");
	int bay_madbatter_connector_exit =
		CHAIN(LOC("Bay Batter Tunnel Exit"), bay_madbatter_connector_entrance, I(FLIPPERS));
	int bay_madbatter_connector_outside = LOC("Outside Bay Batter");
	int bay_madbatter = CHAIN(LOC("Bay Batter"), ADD(LOC(NULL), MAD_BATTER, 0x1E0), I(MAGIC_POWDER));
	int outside_bay_madbatter_entrance = LOC(NULL);
	CONNECT(left_bay_area, outside_bay_madbatter_entrance,
		AND(OR(I(FEATHER), I(ROOSTER)), OR(I(SWORD), I(MAGIC_ROD), I(BOOMERANG))));
	CONNECT_BACK(outside_bay_madbatter_entrance, left_bay_area, AND(OR(I(FEATHER), I(ROOSTER)), R->bush), NO);
	ENTRANCE("prairie_madbatter_connector_entrance", outside_bay_madbatter_entrance,
		 bay_madbatter_connector_entrance, NONE);
	ENTRANCE("prairie_madbatter_connector_exit", bay_madbatter_connector_outside, bay_madbatter_connector_exit,
		 NONE);
	ENTRANCE("prairie_madbatter", bay_madbatter_connector_outside, bay_madbatter, NONE);

	int seashell_mansion = LOC("Seashell Mansion");
	if (!OPTION(GOAL, "seashells")) {
		CONNECT(ADD_INDEX(LOC(NULL), SEASHELL_MANSION_BONUS, 0), seashell_mansion, COUNT(SEASHELL, 5));
		CONNECT(ADD_INDEX(LOC(NULL), SEASHELL_MANSION_BONUS, 1), seashell_mansion, COUNT(SEASHELL, 10));
		CONNECT(ADD(LOC(NULL), SEASHELL_MANSION, 0x2E9), seashell_mansion, COUNT(SEASHELL, 20));
	} else {
		ADD(seashell_mansion, DROPPED_KEY, 0x2E9);
	}
	ENTRANCE("seashell_mansion", ukuku_prairie, seashell_mansion, NONE);

	int bay_water = LOC("Bay Water");
	CONNECT(bay_water, ukuku_prairie, I(FLIPPERS));
	CONNECT(bay_water, left_bay_area, I(FLIPPERS));
	int fisher_under_bridge = ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2F5, TRADING_ITEM_NECKLACE);
	CONNECT(fisher_under_bridge, bay_water, AND(I(TRADING_ITEM_FISHING_HOOK), I(FEATHER), I(FLIPPERS)));
	CONNECT(bay_water, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x0C9, TRADING_ITEM_SCALE),
		AND(I(TRADING_ITEM_NECKLACE), I(FLIPPERS)));
	int d5_entrance = CHAIN(LOC("Outside D5"), bay_water, I(FLIPPERS));
	ENTRANCE("d5", d5_entrance, -1, NONE);

	int richard_house = LOC("Richard's Villa");
	int richard_cave = CHAIN(LOC("Richard's Cave"), richard_house, COUNT(GOLD_LEAF, 5));
	CONNECT_BACK(richard_cave, richard_house, NONE, NO);
	int richard_cave_chest =
		CHAIN(ADD(LOC(NULL), CHEST, 0x2C8), richard_cave, OR(I(FEATHER), I(HOOKSHOT), I(ROOSTER)));
	int richard_maze = LOC("Pothole Maze");
	ENTRANCE("richard_house", ukuku_prairie, richard_house, NONE);
	ENTRANCE("richard_maze", richard_maze, richard_cave, NONE);
	int slime_key_area = CHAIN(LOC("Near Slime Key"), richard_maze,
				   OR(I(SWORD), AND(I(MAGIC_POWDER), I(MAX_POWDER_UPGRADE)), I(MAGIC_ROD),
				      I(POWER_BRACELET), I(BOOMERANG)));
	if (OWLS_OVERWORLD)
		CONNECT(ADD(LOC(NULL), OWL_STATUE, 0x0C6), slime_key_area, NONE);
	CONNECT(ADD(LOC(NULL), SLIME_KEY, -1), slime_key_area, I(SHOVEL));

	int next_to_castle = LOC("Next to Kanalet");
	if (ladx_setting_on(c->options, LADX_SETTING_TRADEQUEST))
		CONNECT_BACK(ukuku_prairie, next_to_castle, I(TRADING_ITEM_BANANAS), NO);
	else
		CONNECT(next_to_castle, ukuku_prairie, NONE);
	CONNECT(next_to_castle, ukuku_prairie, I(FLIPPERS));
	ENTRANCE("castle_phone", next_to_castle, SHOP_SANITY(6, 0x2CC), NONE);
	int castle_secret_entrance_left = LOC("Kanalet Tunnel West");
	int castle_secret_entrance_right = CHAIN(LOC("Kanalet Tunnel East"), castle_secret_entrance_left, I(FEATHER));
	int castle_courtyard = LOC("Kanalet Courtyard");
	int castle_frontdoor = CHAIN(LOC("Kanalet Front Door"), castle_courtyard, R->bush);
	CONNECT(castle_frontdoor, ukuku_prairie, I(CASTLE_GATE_OPENED));
	ENTRANCE("castle_secret_entrance", next_to_castle, castle_secret_entrance_right, R->pit_bush);
	ENTRANCE_EXIT("castle_secret_entrance", NONE);
	ENTRANCE("castle_secret_exit", castle_courtyard, castle_secret_entrance_left, NONE);

	CONNECT(ADD(LOC(NULL), HEART_PIECE, 0x078), bay_water, I(FLIPPERS));
	int castle_inside = LOC("Inside Kanalet");
	CONNECT(ADD_ITEM(LOC(NULL), KEY_HOLE, 0x2C3, CASTLE_GATE_OPENED), castle_inside, NONE);
	int castle_top_outside = LOC("Outside Top of Kanalet");
	int castle_top_inside = LOC("Inside Top of Kanalet");
	ENTRANCE("castle_main_entrance", castle_frontdoor, castle_inside, NONE);
	ENTRANCE("castle_upper_left", castle_top_outside, castle_inside, NONE);
	ENTRANCE("castle_upper_right", castle_top_outside, castle_top_inside, NONE);
	CONNECT(ADD(LOC(NULL), GOLD_LEAF, 0x05A), castle_courtyard, ENEMY(MAD_BOMBER));
	int crow_gold_leaf =
		CHAIN(ADD(LOC(NULL), GOLD_LEAF, 0x058), castle_courtyard, AND(I(POWER_BRACELET), ENEMY(CROW)));
	CONNECT(ADD(LOC(NULL), GOLD_LEAF, 0x2D2), castle_inside, AND(ENEMY(MOBLIN), ENEMY(ZOL)));
	CONNECT(ADD(LOC(NULL), GOLD_LEAF, 0x2C5), castle_inside, AND(I(BOMB), ENEMY(MOBLIN_SWORD)));
	int kanalet_chain_trooper = ADD(LOC(NULL), GOLD_LEAF, 0x2C6);
	CONNECT_BACK(castle_top_inside, kanalet_chain_trooper, AND(I(POWER_BRACELET), ENEMY(KNIGHT)), NO);

	int animal_village = LOC("Animal Village");
	CONNECT(animal_village, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x0CD, TRADING_ITEM_FISHING_HOOK),
		I(TRADING_ITEM_BROOM));
	CONNECT_BACK(animal_village, marin_song, I(OCARINA), NO);
	int cookhouse = LOC("Bear Chef's House");
	CONNECT(cookhouse, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2D7, TRADING_ITEM_PINEAPPLE),
		I(TRADING_ITEM_HONEYCOMB));
	int goathouse = LOC("Goat's House");
	CONNECT(goathouse, ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x2D9, TRADING_ITEM_LETTER),
		I(TRADING_ITEM_HIBISCUS));
	int mermaid_statue = LOC("Inside Mermaid Statue");
	CONNECT(mermaid_statue, animal_village, AND(I(TRADING_ITEM_SCALE), I(HOOKSHOT)));
	ADD_ITEM(mermaid_statue, TRADE_SEQUENCE_ITEM, 0x297, TRADING_ITEM_MAGNIFYING_GLASS);
	ENTRANCE("animal_phone", animal_village, SHOP_SANITY(7, 0x2E3), NONE);
	ENTRANCE("animal_house1", animal_village, -1, NONE);
	ENTRANCE("animal_house2", animal_village, -1, NONE);
	ENTRANCE("animal_house3", animal_village, goathouse, NONE);
	ENTRANCE("animal_house4", animal_village, -1, NONE);
	ENTRANCE("animal_house5", animal_village, cookhouse, NONE);
	CONNECT(animal_village, bay_water, I(FLIPPERS));
	CONNECT(animal_village, ukuku_prairie, OR(I(HOOKSHOT), I(ROOSTER)));
	int animal_village_connector_left = LOC("Under the River West");
	int animal_village_connector_right =
		CHAIN(LOC("Under the River East"), animal_village_connector_left, I(PEGASUS_BOOTS));
	ENTRANCE("prairie_to_animal_connector", ukuku_prairie, animal_village_connector_left, R->pit_bush);
	ENTRANCE_EXIT("prairie_to_animal_connector", NONE);
	ENTRANCE("animal_to_prairie_connector", animal_village, animal_village_connector_right, NONE);
	if (OWLS_OVERWORLD)
		ADD(animal_village, OWL_STATUE, 0x0DA);
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0DA), animal_village, I(SHOVEL));
	int desert = CHAIN(LOC("Desert"), animal_village, R->bush);
	if (OWLS_OVERWORLD)
		ADD(desert, OWL_STATUE, 0x0CF);
	CONNECT(ADD(LOC(NULL), ANGLER_KEY, -1), desert, ENEMY(DESERT_LANMOLA));

	int animal_village_bombcave = LOC("Bomb Arrow Cave");
	ENTRANCE("animal_cave", desert, animal_village_bombcave, I(BOMB));
	ENTRANCE_EXIT("animal_cave", NONE);
	int animal_village_bombcave_heartpiece =
		CHAIN(ADD(LOC(NULL), HEART_PIECE, 0x2E6), animal_village_bombcave,
		      OR(AND(I(BOMB), I(FEATHER), I(HOOKSHOT)), I(ROOSTER)));

	int desert_cave = LOC("Desert Cave");
	ENTRANCE("desert_cave", desert, desert_cave, NONE);
	CONNECT_BACK(desert, desert_cave, NONE, NO);

	CONNECT(ADD(LOC(NULL), HEART_PIECE, 0x1E8), desert_cave, I(BOMB));
	CONNECT(ADD(LOC(NULL), SEASHELL, 0x0FF), desert, I(POWER_BRACELET));

	int armos_maze = CHAIN(LOC("Armos Maze"), animal_village, I(POWER_BRACELET));
	int armos_temple = LOC("Southern Shrine");
	CONNECT(ADD(LOC(NULL), FACE_KEY, -1), armos_temple, MINIBOSS(MINIBOSS_ARMOS_TEMPLE));
	if (OWLS_OVERWORLD)
		ADD(armos_maze, OWL_STATUE, 0x08F);
	int outside_armos_cave =
		CHAIN(LOC("Outside Armos Maze Cave"), armos_maze,
		      OR(ENEMY(ARMOS_STATUE), I(SWORD), I(BOOMERANG), I(HOOKSHOT), I(SHIELD)));
	int outside_armos_temple =
		CHAIN(LOC("Outside Southern Shrine"), armos_maze,
		      OR(ENEMY(ARMOS_STATUE), I(SWORD), I(BOOMERANG), I(HOOKSHOT), I(SHIELD)));
	ENTRANCE("armos_maze_cave", outside_armos_cave, ADD(LOC(NULL), CHEST, 0x2FC), NONE);
	ENTRANCE("armos_temple", outside_armos_temple, armos_temple, NONE);

	int armos_fairy_entrance =
		CHAIN(CHAIN(LOC("Outside D6 Fairy"), bay_water, I(FLIPPERS)), animal_village, I(POWER_BRACELET));
	ENTRANCE("armos_fairy", armos_fairy_entrance, -1, I(BOMB));
	ENTRANCE_EXIT("armos_fairy", NONE);

	int d6_connector_left = LOC("To D6 West");
	int d6_connector_right =
		CHAIN(LOC("To D6 East"), d6_connector_left,
		      OR(AND(I(HOOKSHOT), OR(I(FLIPPERS), AND(I(FEATHER), I(PEGASUS_BOOTS)))), I(ROOSTER)));
	int d6_entrance = LOC("Outside D6");
	CONNECT_BACK(d6_entrance, bay_water, I(FLIPPERS), NO);
	CONNECT(d6_entrance, ADD_ITEM(LOC(NULL), KEY_HOLE, 0x08C, FACE_SHRINE_OPENED), I(FACE_KEY));
	int d6_armos_island = CHAIN(LOC("Armos Island"), bay_water, I(FLIPPERS));
	ENTRANCE("d6_connector_entrance", d6_armos_island, d6_connector_right, NONE);
	ENTRANCE("d6_connector_exit", d6_entrance, d6_connector_left, NONE);
	ENTRANCE("d6", d6_entrance, -1, I(FACE_SHRINE_OPENED));
	ENTRANCE_EXIT("d6", NONE);

	int windfish_egg =
		CHAIN(CHAIN(LOC("Outside the Egg"), swamp, I(POWER_BRACELET)), graveyard, I(POWER_BRACELET));
	CONNECT_BACK(windfish_egg, graveyard, NONE, NO);

	int obstacle_cave_entrance = LOC("Mountain Access Entrance");
	int obstacle_cave_inside = CHAIN(LOC("Mountain Access Inside"), obstacle_cave_entrance, I(SWORD));
	CONNECT_BACK(obstacle_cave_inside, obstacle_cave_entrance, I(FEATHER), NO);
	int obstacle_cave_inside_chest =
		CHAIN(ADD(LOC(NULL), CHEST, 0x2BB), obstacle_cave_inside, OR(I(HOOKSHOT), I(ROOSTER)));
	int obstacle_cave_exit =
		CHAIN(LOC("Mountain Access Exit"), obstacle_cave_inside, OR(I(PEGASUS_BOOTS), I(ROOSTER)));

	int outside_obstacle_cave = LOC(NULL);
	CONNECT(windfish_egg, outside_obstacle_cave, I(POWER_BRACELET));

	int lower_right_taltal = LOC("Lower Tal Tal");
	ENTRANCE("obstacle_cave_entrance", outside_obstacle_cave, obstacle_cave_entrance, NONE);
	ENTRANCE("obstacle_cave_outside_chest", ADD(LOC(NULL), CHEST, 0x018), obstacle_cave_inside, NONE);
	ENTRANCE("obstacle_cave_exit", lower_right_taltal, obstacle_cave_exit, NONE);

	int papahl_cave = ADD(LOC(NULL), CHEST, 0x28A);
	int papahl = CHAIN_BACK(LOC("Papahl's Ledge"), lower_right_taltal, NONE, NO);
	int hibiscus_item = ADD_ITEM(LOC(NULL), TRADE_SEQUENCE_ITEM, 0x019, TRADING_ITEM_HIBISCUS);
	CONNECT_BACK(papahl, hibiscus_item, I(TRADING_ITEM_PINEAPPLE), NO);
	ENTRANCE("papahl_entrance", lower_right_taltal, papahl_cave, NONE);
	ENTRANCE("papahl_exit", papahl, papahl_cave, NONE);

	int below_right_taltal = CHAIN(LOC("Near D4 Keyhole"), windfish_egg, I(POWER_BRACELET));
	int angler_tunnel_keyhole =
		CHAIN(LOC("D4 Keyhole"), ADD_ITEM(LOC(NULL), KEY_HOLE, 0x02B, ANGLER_TUNNEL_OPENED), I(ANGLER_KEY));

	int d4_entrance_locked = CHAIN(LOC("Outside D4 Closed"), below_right_taltal, I(FLIPPERS));
	int d4_entrance_unlocked = CHAIN(LOC("Outside D4 Open"), d4_entrance_locked, I(ANGLER_TUNNEL_OPENED));

	CONNECT_BACK(below_right_taltal, angler_tunnel_keyhole, NONE, NO);
	CONNECT(below_right_taltal, bay_water, I(FLIPPERS));
	CONNECT(below_right_taltal, next_to_castle, I(ROOSTER));
	CONNECT_BACK(lower_right_taltal, d4_entrance_locked, I(FLIPPERS), NO);
	CONNECT_BACK(lower_right_taltal, d4_entrance_unlocked, I(ANGLER_TUNNEL_OPENED), NO);

	int heartpiece_swim_cave = CHAIN(LOC("Damp Cave"), ADD(LOC(NULL), HEART_PIECE, 0x1F2), I(FLIPPERS));
	int outside_swim_cave = LOC(NULL);
	CONNECT(below_right_taltal, outside_swim_cave, I(FLIPPERS));
	ENTRANCE("heartpiece_swim_cave", outside_swim_cave, heartpiece_swim_cave, NONE);
	ENTRANCE("d4", d4_entrance_locked, -1, I(ANGLER_TUNNEL_OPENED));
	ENTRANCE_EXIT("d4", I(FLIPPERS));
	CONNECT_BACK(d4_entrance_unlocked, below_right_taltal, NONE, NO);
	int outside_mambo = CHAIN(LOC("Outside Manbo"), d4_entrance_locked, I(FLIPPERS));
	int inside_mambo = LOC("Manbo's Cave");
	int mambo = CHAIN(ADD(LOC(NULL), SONG, 0x2FD), inside_mambo, AND(I(OCARINA), I(FLIPPERS)));
	ENTRANCE("mambo", outside_mambo, inside_mambo, NONE);

	int raft_house = LOC("Raft House");
	CONNECT(ADD_KEY(LOC("Raft Purchase"), RAFT), raft_house, FOUND(RUPEES, 500));
	int raft_return_upper = LOC("Raft Return North");
	int raft_return_lower = CHAIN_BACK(LOC("Raft Return South"), raft_return_upper, NONE, NO);
	int outside_raft_house = CHAIN_BACK(CHAIN(LOC("Outside Raft House"), below_right_taltal, I(HOOKSHOT)),
					    below_right_taltal, I(FLIPPERS), NO);
	int raft_game = LOC(NULL);
	CONNECT(raft_game, outside_raft_house, I(RAFT));
	ADD(raft_game, CHEST, 0x05C);
	ADD(raft_game, CHEST, 0x05D);
	int raft_exit = LOC("Raft Exit");
	if (!LOGIC_IS("casual")) {
		CONNECT_BACK(raft_game, raft_exit, NONE, NO);
		CONNECT_BACK(raft_game, armos_fairy_entrance, NONE, NO);
	}
	ENTRANCE("raft_return_exit", outside_raft_house, raft_return_upper, NONE);
	ENTRANCE("raft_return_enter", raft_exit, raft_return_lower, NONE);
	CONNECT(raft_exit, armos_fairy_entrance, I(FLIPPERS));
	ENTRANCE("raft_house", outside_raft_house, raft_house, NONE);
	if (OWLS_OVERWORLD)
		ADD(raft_game, OWL_STATUE, 0x5D);

	int middle_right_taltal = CHAIN(LOC("Between Waterfall and Mountain Stairs"), lower_right_taltal,
					OR(I(FLIPPERS), I(ROOSTER), I(ANGLER_TUNNEL_OPENED)));
	int outside_rooster_house = CHAIN(LOC("Outside Rooster House"), middle_right_taltal, OR(I(FLIPPERS), I(ROOSTER)));
	ENTRANCE("rooster_house", outside_rooster_house, -1, NONE);
	int bird_cave = LOC("Bird Cave");
	int bird_key = ADD(LOC(NULL), BIRD_KEY, -1);
	CONNECT(bird_cave, bird_key, I(ROOSTER));
	if (!ladx_setting_on(c->options, LADX_SETTING_ROOSTER))
		CONNECT(bird_cave, bird_key, AND(I(FEATHER), COUNT(POWER_BRACELET, 2)));
	if (!LOGIC_IS("casual"))
		CONNECT_BACK(bird_cave, lower_right_taltal, NONE, NO);
	ENTRANCE("bird_cave", outside_rooster_house, bird_cave, NONE);
	int bridge_seashell = CHAIN(ADD(LOC(NULL), SEASHELL, 0x00C), outside_rooster_house,
				    AND(OR(I(FEATHER), I(ROOSTER)), I(POWER_BRACELET)));

	int multichest_cave = LOC("5 Chest Game Tunnel");
	int multichest_cave_secret = CHAIN_BACK(LOC("To 5 Chest Game"), multichest_cave, NONE, NO);
	CONNECT_BACK(multichest_cave, multichest_cave_secret, I(BOMB), NO);
	int water_cave_hole = LOC("Near Hole to Damp Cave");
	if (!LOGIC_IS("casual"))
		CONNECT_BACK(water_cave_hole, heartpiece_swim_cave, I(FLIPPERS), NO);
	int multichest_outside = ADD(LOC(NULL), CHEST, 0x01D);
	int outside_multichest_left = LOC(NULL);
	CONNECT(lower_right_taltal, outside_multichest_left, OR(I(FLIPPERS), I(ROOSTER)));
	ENTRANCE("multichest_left", outside_multichest_left, multichest_cave, NONE);
	ENTRANCE("multichest_right", water_cave_hole, multichest_cave, NONE);
	ENTRANCE("multichest_top", multichest_outside, multichest_cave_secret, NONE);
	if (OWLS_OVERWORLD)
		ADD(water_cave_hole, OWL_STATUE, 0x1E);

	int right_taltal_connector1 = LOC("Outer Rainbow Cave");
	int right_taltal_connector_outside1 = LOC("Rainbow Ledge");
	int right_taltal_connector2 = LOC("Inner Rainbow Cave Entrance");
	int right_taltal_connector3 = LOC("Inner Rainbow Cave Exit");
	CONNECT_BACK(right_taltal_connector2, right_taltal_connector3, AND(OR(I(FEATHER), I(ROOSTER)), I(HOOKSHOT)),
		     NO);
	int right_taltal_connector_outside2 = LOC("After Rainbow Caves");
	int right_taltal_connector4 = LOC("Path to D7");
	int d7_platau = LOC("D7 Plateau");
	int d7_tower = LOC("D7 Tower");
	CONNECT(d7_platau, ADD_ITEM(LOC(NULL), KEY_HOLE, 0x00E, EAGLE_TOWER_OPENED), AND(I(POWER_BRACELET), I(BIRD_KEY)));
	CONNECT_BACK(d7_platau, d7_tower, I(EAGLE_TOWER_OPENED), NO);
	CONNECT_BACK(d7_tower, d7_platau, NONE, NO);
	ENTRANCE("right_taltal_connector1", water_cave_hole, right_taltal_connector1, NONE);
	ENTRANCE("right_taltal_connector2", right_taltal_connector_outside1, right_taltal_connector1, NONE);
	ENTRANCE("right_taltal_connector3", right_taltal_connector_outside1, right_taltal_connector2, NONE);
	ENTRANCE("right_taltal_connector4", right_taltal_connector_outside2, right_taltal_connector3, NONE);
	ENTRANCE("right_taltal_connector5", right_taltal_connector_outside2, right_taltal_connector4, NONE);
	ENTRANCE("right_taltal_connector6", d7_platau, right_taltal_connector4, NONE);
	ENTRANCE("right_fairy", right_taltal_connector_outside2, -1, I(BOMB));
	ENTRANCE_EXIT("right_fairy", NONE);
	ENTRANCE("d7", d7_tower, -1, NONE);
	if (!LOGIC_IS("casual")) {
		CONNECT_BACK(d7_platau, heartpiece_swim_cave, I(FLIPPERS), NO);
		CONNECT_BACK(d7_platau, right_taltal_connector_outside1, NONE, NO);
	}

	int mountain_bridge_staircase =
		CHAIN(LOC("Bridge Towards D8"), outside_rooster_house, OR(I(HOOKSHOT), I(ROOSTER)));
	if (!LOGIC_IS("casual"))
		CONNECT_BACK(mountain_bridge_staircase, windfish_egg, NONE, NO);

	int left_right_connector_cave_entrance = LOC("Path to West Tal Tal Entrance");
	int left_right_connector_cave_exit = LOC("Path to West Tal Tal Exit");
	CONNECT_BACK(left_right_connector_cave_entrance, left_right_connector_cave_exit, OR(I(HOOKSHOT), I(ROOSTER)),
		     NO);
	int taltal_boulder_zone = LOC("Falling Rocks");
	ENTRANCE("left_to_right_taltalentrance", mountain_bridge_staircase, left_right_connector_cave_entrance,
		 R->pit_bush);
	ENTRANCE_EXIT("left_to_right_taltalentrance", NONE);
	ENTRANCE("left_taltal_entrance", taltal_boulder_zone, left_right_connector_cave_exit, NONE);
	int mountain_heartpiece = ADD(LOC(NULL), HEART_PIECE, 0x2BA);
	CONNECT_BACK(left_right_connector_cave_entrance, mountain_heartpiece, I(BOMB), NO);

	ADD(taltal_boulder_zone, CHEST, 0x004);
	int taltal_madbatter = CHAIN(LOC("Mountain Mad Batter"), ADD(LOC(NULL), MAD_BATTER, 0x1E2), I(MAGIC_POWDER));
	ENTRANCE("madbatter_taltal", taltal_boulder_zone, taltal_madbatter, I(POWER_BRACELET));
	ENTRANCE_EXIT("madbatter_taltal", NONE);

	int outside_fire_cave = LOC("Outside Fire Cave");
	if (!LOGIC_IS("casual"))
		CONNECT_BACK(outside_fire_cave, writes_hut_outside, NONE, NO);
	CONNECT_BACK(taltal_boulder_zone, outside_fire_cave, NONE, NO);
	int fire_cave_bottom = LOC("Fire Cave South");
	int fire_cave_top = CHAIN(LOC("Fire Cave North"), fire_cave_bottom, COUNT(SHIELD, 2));
	ENTRANCE("fire_cave_entrance", outside_fire_cave, fire_cave_bottom, I(BOMB));
	ENTRANCE_EXIT("fire_cave_entrance", NONE);

	int d8_entrance = LOC("Outside D8");
	if (!LOGIC_IS("casual")) {
		CONNECT_BACK(d8_entrance, writes_hut_outside, NONE, NO);
		CONNECT_BACK(d8_entrance, outside_fire_cave, NONE, NO);
	}
	ENTRANCE("fire_cave_exit", d8_entrance, fire_cave_top, NONE);
	ENTRANCE("phone_d8", d8_entrance, SHOP_SANITY(8, 0x299), NONE);
	ENTRANCE("d8", d8_entrance, -1, ENEMY(TURTLE_ROCK_HEAD));
	ENTRANCE_EXIT("d8", NONE);

	int nightmare = LOC("Nightmare");
	int windfish = CHAIN(LOC("Windfish"), nightmare, AND(ENEMY(FINAL_NIGHTMARE)));

	if (HARD_OR_ABOVE) {
		CONNECT(hookshot_cave, hookshot_cave_chest, R->boots_jump);
		CONNECT_BACK(graveyard_cave_left, graveyard_cave_right, R->hookshot_over_pit, NO);
		CONNECT(swamp_chest, swamp, R->wall_clip);
		CONNECT_BACK(swamp, outside_d2, AND(R->wall_clip, I(POWER_BRACELET)), NO);
		CONNECT_BACK(outside_d2, swamp, R->wall_clip, NO);
		CONNECT_BACK(swamp, writes_hut_outside, R->hookshot_over_pit, NO);
		CONNECT(graveyard_heartpiece, graveyard_cave_right, I(FEATHER));
		CONNECT(graveyard_heartpiece, graveyard_cave_right, AND(R->wall_clip, OR(I(HOOKSHOT), I(BOOMERANG))));

		ENTRANCE_REQUIREMENT("mamu", AND(R->wall_clip, I(FEATHER), I(POWER_BRACELET)));
		CONNECT(left_bay_area, outside_bay_madbatter_entrance,
			AND(OR(I(FEATHER), I(ROOSTER)), OR(I(MAGIC_POWDER), I(BOMB))));
		CONNECT(crow_gold_leaf, castle_courtyard, I(POWER_BRACELET));
		CONNECT_BACK(castle_inside, kanalet_chain_trooper, I(BOOMERANG), NO);
		CONNECT(animal_village_bombcave_heartpiece, animal_village_bombcave, R->boots_jump);
		CONNECT(animal_village_bombcave_heartpiece, animal_village_bombcave,
			AND(I(BOMB), I(FEATHER), I(BOOMERANG)));

		CONNECT(armos_maze, outside_armos_cave, NONE);
		CONNECT(armos_maze, outside_armos_temple, NONE);
		CONNECT(d6_connector_left, d6_connector_right, AND(OR(I(FLIPPERS), I(PEGASUS_BOOTS)), I(FEATHER)));
		CONNECT_BACK(obstacle_cave_exit, obstacle_cave_inside, AND(I(FEATHER), R->hookshot_over_pit), NO);
		CONNECT_BACK(d4_entrance_unlocked, angler_tunnel_keyhole, NONE, NO);
		if (!ladx_setting_on(c->options, LADX_SETTING_ROOSTER))
			CONNECT(bird_key, bird_cave, COUNT(POWER_BRACELET, 2));
		CONNECT_BACK(right_taltal_connector2, right_taltal_connector3, I(ROOSTER), NO);
		CONNECT_BACK(fire_cave_bottom, fire_cave_top, AND(R->damage_boost_special, I(PEGASUS_BOOTS)), NO);
	}

	if (GLITCHED_OR_ABOVE) {
		CONNECT(papahl_house, mamasha_trade, R->bomb_trigger);
		CONNECT_BACK(mabe_village, outside_dream_hut, R->hookshot_clip, NO);
		CONNECT(dream_hut_right, dream_hut_left, R->super_jump_feather);
		CONNECT(forest, swamp, R->bomb_trigger);
		CONNECT_BACK(forest, forest_heartpiece, R->bomb_trigger, NO);
		CONNECT_BACK(forest, outside_hookshot_cave, R->hookshot_clip, NO);
		CONNECT_BACK(swamp, forest_toadstool, R->pit_buffer_itemless, NO);
		CONNECT_BACK(writes_hut_outside, swamp, R->pit_buffer_itemless, NO);
		CONNECT_BACK(graveyard, forest_heartpiece, R->pit_buffer_itemless, NO);
		CONNECT_BACK(graveyard, forest, NONE, NO);
		CONNECT(log_cave_heartpiece, forest_cave, R->super_jump_feather);
		CONNECT(log_cave_heartpiece, forest_cave, R->bomb_trigger);
		CONNECT(log_cave_heartpiece, forest_cave, AND(R->sideways_block_push, R->hookshot_clip));
		CONNECT_BACK(graveyard_cave_left, graveyard_heartpiece, R->bomb_trigger, NO);
		CONNECT(graveyard_heartpiece, graveyard_cave_right, R->sideways_block_push);

		CONNECT(prairie_island_seashell, ukuku_prairie, AND(R->jesus_jump, R->bush));
		ENTRANCE_REQUIREMENT("castle_jump_cave", R->pit_buffer);
		CONNECT(left_bay_area, ghost_hut_outside, R->pit_buffer);
		CONNECT(tiny_island, left_bay_area, AND(R->jesus_jump, R->bush));
		CONNECT_BACK(bay_madbatter_connector_exit, bay_madbatter_connector_entrance, R->jesus_jump, NO);
		CONNECT(left_bay_area, outside_bay_madbatter_entrance, AND(R->pit_buffer, I(POWER_BRACELET)));

		CONNECT_BACK(ukuku_prairie, richard_maze,
			     AND(R->pit_buffer_itemless, OR(AND(I(MAGIC_POWDER), I(MAX_POWDER_UPGRADE)), I(BOMB),
							    I(BOOMERANG), I(MAGIC_ROD), I(SWORD))),
			     NO);
		CONNECT_BACK(richard_maze, ukuku_prairie,
			     AND(R->pit_buffer_itemless,
				 OR(I(MAGIC_POWDER), I(BOMB), I(BOOMERANG), I(MAGIC_ROD), I(SWORD))),
			     NO);
		CONNECT(fisher_under_bridge, bay_water, AND(R->bomb_trigger, AND(I(FEATHER), I(FLIPPERS))));
		CONNECT(animal_village, ukuku_prairie, R->jesus_jump);
		CONNECT(below_right_taltal, next_to_castle, R->jesus_jump);
		CONNECT(animal_village_bombcave_heartpiece, animal_village_bombcave,
			AND(R->bomb_trigger, OR(I(HOOKSHOT), I(FEATHER), R->boots_bonk_pit)));
		CONNECT(animal_village_bombcave_heartpiece, animal_village_bombcave, R->pit_buffer);

		CONNECT_BACK(d6_entrance, ukuku_prairie, R->jesus_jump, NO);
		CONNECT_BACK(d6_entrance, armos_fairy_entrance, R->jesus_jump, NO);
		CONNECT(d6_connector_left, d6_connector_right, R->jesus_jump);
		CONNECT_BACK(armos_fairy_entrance, d6_armos_island, R->jesus_jump, NO);
		CONNECT(armos_fairy_entrance, raft_exit, R->jesus_jump);
		CONNECT_BACK(windfish_egg, outside_obstacle_cave, R->hookshot_clip, NO);
		CONNECT(obstacle_cave_inside_chest, obstacle_cave_inside, R->pit_buffer);
		CONNECT(obstacle_cave_exit, obstacle_cave_inside, R->pit_buffer);
		CONNECT_BACK(lower_right_taltal, hibiscus_item, AND(I(TRADING_ITEM_PINEAPPLE), R->bomb_trigger), NO);
		CONNECT_BACK(lower_right_taltal, d4_entrance_locked, R->jesus_jump, NO);
		ENTRANCE_EXIT("d4", NONE);
		CONNECT_BACK(d4_entrance_locked, angler_tunnel_keyhole, R->jesus_jump, NO);
		CONNECT_BACK(below_right_taltal, d4_entrance_locked, OR(R->super_jump_boots, I(HOOKSHOT)), NO);

		CONNECT(below_right_taltal, outside_swim_cave, R->jesus_jump);
		CONNECT(outside_mambo, d4_entrance_locked, R->jesus_jump);
		CONNECT_BACK(outside_raft_house, below_right_taltal, R->jesus_jump, NO);

		CONNECT(lower_right_taltal, outside_multichest_left, R->jesus_jump);
		CONNECT(outside_rooster_house, lower_right_taltal, R->jesus_jump);
		CONNECT_BACK(d7_platau, water_cave_hole, NONE, NO);
		CONNECT_BACK(water_cave_hole, heartpiece_swim_cave, R->jesus_jump, NO);
		CONNECT(mountain_bridge_staircase, outside_rooster_house, AND(R->boots_jump, R->pit_buffer));
		CONNECT(bird_key, bird_cave, R->hookshot_jump);
		CONNECT_BACK(right_taltal_connector2, right_taltal_connector3, OR(R->pit_buffer, I(ROOSTER)), NO);
		CONNECT_BACK(left_right_connector_cave_exit, left_right_connector_cave_entrance,
			     AND(I(HOOKSHOT), R->super_jump_feather), NO);
		CONNECT_BACK(obstacle_cave_inside, mountain_heartpiece, R->bomb_trigger, NO);
		ENTRANCE_REQUIREMENT("d8", OR(R->bomb_trigger, AND(I(OCARINA), I(SONG3))));
	}

	if (LOGIC_IS("hell")) {
		CONNECT_BACK(outside_dream_hut, mabe_village, R->zoomerang_shovel, NO);
		CONNECT(dream_hut_right, dream_hut, NONE);
		CONNECT_BACK(outside_hookshot_cave, forest, R->zoomerang_shovel, NO);
		CONNECT(swamp, forest_toadstool, R->damage_boost);
		CONNECT(swamp, forest, AND(R->bush, OR(R->boots_bonk_pit, R->hookshot_spam_pit)));
		CONNECT_BACK(forest, forest_heartpiece, R->boots_bonk_pit, NO);
		CONNECT(forest_cave_crystal_chest, forest_cave,
			AND(OR(R->boots_roosterhop, R->super_jump_feather), R->hookshot_clip_block, R->sideways_block_push));
		CONNECT(log_cave_heartpiece, forest_cave, I(BOOMERANG));
		CONNECT(log_cave_heartpiece, forest_cave, OR(R->super_jump_rooster, R->boots_roosterhop));
		CONNECT(writes_hut_outside, swamp, R->damage_boost);
		CONNECT(writes_cave_left_chest, writes_cave, R->damage_boost);
		CONNECT_BACK(graveyard, crazy_tracy_hut, R->hookshot_spam_pit, NO);
		CONNECT_BACK(crazy_tracy_hut, graveyard, R->zoomerang_shovel, NO);
		CONNECT(graveyard, forest, OR(R->boots_bonk_pit, R->hookshot_spam_pit));
		CONNECT(graveyard_cave_left, graveyard_cave_right, R->hookshot_spam_pit);
		CONNECT_BACK(graveyard_cave_right, graveyard_cave_left, OR(R->damage_boost, R->boots_bonk_pit), NO);
		CONNECT_BACK(outside_graveyard_left, ghost_grave,
			     AND(I(BOOMERANG), OR(I(FEATHER), I(PEGASUS_BOOTS)), R->hookshot_clip_block), NO);
		CONNECT(ghost_grave, outside_graveyard_left, R->zoomerang_shovel);

		ENTRANCE_ENTER("mamu", AND(R->pit_buffer_itemless, R->pit_buffer_boots, I(POWER_BRACELET)));
		ENTRANCE_REQUIREMENT("castle_jump_cave", R->pit_buffer_boots);
		CONNECT(prairie_cave_secret_exit, prairie_cave, AND(I(BOMB), OR(R->boots_bonk_pit, R->hookshot_spam_pit)));
		CONNECT(richard_cave_chest, richard_cave, R->damage_boost);
		CONNECT(richard_cave_chest, richard_cave, R->boots_bonk);
		CONNECT_BACK(ukuku_prairie, dungeon3_entrance, AND(R->jesus_buffer, R->jesus_buffer_hookshot), NO);
		CONNECT(castle_secret_entrance_right, castle_secret_entrance_left,
			OR(R->boots_bonk_2d_spikepit, R->bracelet_bounce_2d_spikepit, R->toadstool_bounce_2d_spikepit));
		CONNECT(left_bay_area, ghost_hut_outside, R->pit_buffer_boots);
		CONNECT_BACK(left_bay_area, ukuku_prairie, R->hookshot_clip_block, NO);
		CONNECT(tiny_island, left_bay_area, AND(R->jesus_buffer, R->boots_bonk_pit, R->bush));
		CONNECT(left_bay_area, outside_bay_madbatter_entrance,
			AND(R->pit_buffer_boots, OR(I(MAGIC_POWDER), I(SWORD), I(MAGIC_ROD), I(BOOMERANG))));
		CONNECT(left_bay_area, outside_bay_madbatter_entrance,
			AND(R->pit_buffer, R->hookshot_spam_pit, R->bush));
		CONNECT_BACK(outside_bay_madbatter_entrance, left_bay_area, AND(R->pit_buffer_boots, R->bush), NO);

		CONNECT(left_bay_area, bay_water, OR(R->jesus_jump, R->jesus_rooster));
		CONNECT(animal_village, bay_water, OR(R->jesus_jump, R->jesus_rooster));
		CONNECT_BACK(ukuku_prairie, bay_water, OR(R->jesus_jump, R->jesus_rooster), NO);
		CONNECT(bay_water, d5_entrance, OR(R->jesus_jump, R->jesus_rooster));
		CONNECT(prairie_island_seashell, ukuku_prairie,
			OR(AND(R->jesus_rooster, R->bush), AND(R->jesus_buffer_hookshot, I(BOOMERANG))));
		CONNECT_BACK(bay_madbatter_connector_exit, bay_madbatter_connector_entrance, R->jesus_rooster, NO);
		CONNECT(fisher_under_bridge, bay_water, AND(I(TRADING_ITEM_FISHING_HOOK), I(FLIPPERS)));

		CONNECT_BACK(below_right_taltal, next_to_castle, R->jesus_buffer, NO);
		CONNECT(crow_gold_leaf, castle_courtyard, I(BOMB));
		CONNECT(mermaid_statue, animal_village, AND(I(TRADING_ITEM_SCALE), R->super_jump_feather));
		CONNECT(animal_village_connector_right, animal_village_connector_left, R->shaq_jump);
		CONNECT_BACK(animal_village_connector_left, animal_village_connector_right, R->hookshot_clip_block, NO);
		CONNECT(animal_village_bombcave_heartpiece, animal_village_bombcave, R->pit_buffer_boots);

		CONNECT(d6_armos_island, ukuku_prairie, OR(R->jesus_jump, R->jesus_rooster));
		CONNECT_BACK(armos_fairy_entrance, d6_armos_island, R->jesus_buffer, NO);
		CONNECT(d6_connector_right, d6_connector_left, R->pit_buffer_boots);

		CONNECT_BACK(d6_entrance, ukuku_prairie, R->jesus_rooster, NO);
		CONNECT_BACK(d6_entrance, armos_fairy_entrance, R->jesus_rooster, NO);
		CONNECT_BACK(armos_fairy_entrance, d6_armos_island, R->jesus_rooster, NO);
		CONNECT(armos_fairy_entrance, raft_exit, R->jesus_rooster);

		CONNECT(obstacle_cave_entrance, obstacle_cave_inside, OR(R->hookshot_clip_block, R->shaq_jump));
		CONNECT(obstacle_cave_entrance, obstacle_cave_inside, R->boots_roosterhop);
		CONNECT(d4_entrance_locked, below_right_taltal, OR(R->jesus_jump, R->jesus_rooster));
		CONNECT_BACK(d4_entrance_locked, angler_tunnel_keyhole, OR(I(ROOSTER), R->jesus_buffer), NO);
		CONNECT_BACK(below_right_taltal, d4_entrance_locked, AND(R->shaq_jump, R->super_jump_feather), NO);
		CONNECT_BACK(lower_right_taltal, angler_tunnel_keyhole, OR(I(ROOSTER), AND(R->jesus_buffer, R->midair_turn)),
			     NO);
		CONNECT_BACK(middle_right_taltal, angler_tunnel_keyhole,
			     OR(I(ROOSTER), AND(R->jesus_buffer, R->midair_turn)), NO);

		CONNECT_BACK(lower_right_taltal, below_right_taltal, OR(R->jesus_jump, R->jesus_rooster), NO);
		CONNECT(below_right_taltal, outside_swim_cave, R->jesus_rooster);
		CONNECT(outside_mambo, d4_entrance_locked, OR(R->jesus_rooster, R->jesus_jump));
		if (!OPTION(HARDMODE, "oracle"))
			CONNECT(mambo, inside_mambo, AND(I(OCARINA), R->bomb_trigger));
		CONNECT_BACK(outside_raft_house, below_right_taltal, R->jesus_rooster, NO);
		CONNECT(lower_right_taltal, outside_multichest_left, R->jesus_rooster);
		CONNECT_BACK(outside_rooster_house, below_right_taltal, R->jesus_rooster, NO);
		CONNECT_BACK(outside_rooster_house, middle_right_taltal, R->jesus_buffer, NO);

		if (OPTION(ENTRANCESHUFFLE, "default") || OPTION(ENTRANCESHUFFLE, "simple"))
			CONNECT_BACK(below_right_taltal, raft_game,
				     AND(OR(R->jesus_jump, R->jesus_rooster), R->attack_hookshot_powder), NO);
		CONNECT(outside_raft_house, below_right_taltal, AND(R->super_jump, I(PEGASUS_BOOTS)));
		CONNECT(bridge_seashell, outside_rooster_house,
			AND(OR(R->hookshot_spam_pit, R->boots_bonk_pit), I(POWER_BRACELET)));
		CONNECT(bird_key, bird_cave, AND(R->boots_jump, R->pit_buffer));
		CONNECT_BACK(right_taltal_connector2, right_taltal_connector3, R->pit_buffer_itemless, NO);
		CONNECT_BACK(right_taltal_connector3, right_taltal_connector2, AND(R->zoomerang, I(HOOKSHOT)), NO);
		CONNECT_BACK(water_cave_hole, heartpiece_swim_cave, R->jesus_buffer_itemless, NO);
		CONNECT(mountain_bridge_staircase, outside_rooster_house, R->pit_buffer_boots);
		CONNECT_BACK(left_right_connector_cave_entrance, left_right_connector_cave_exit,
			     AND(R->boots_jump, R->pit_buffer), NO);
		CONNECT_BACK(left_right_connector_cave_exit, left_right_connector_cave_entrance,
			     AND(I(ROOSTER), OR(R->boots_roosterhop, R->super_jump_rooster)), NO);

		CONNECT(windfish, nightmare,
			AND(ENEMY(SHADOW_BLOB), ENEMY(SHADOW_AGAHNIM), ENEMY(SHADOW_MOLDORM), ENEMY(SHADOW_GANON),
			    ENEMY(SHADOW_LANMOLA), AND(I(OCARINA), OR(I(SONG1), I(SONG3)))));
	}

	w->start = start_house;
	w->egg = windfish_egg;
	w->nightmare = nightmare;
	w->windfish = windfish;
}
