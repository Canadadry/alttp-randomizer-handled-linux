#include "rando_postfill.h"

#include "postfill_draw.h"
#include "rando_pool.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* n + 1, so that an id left out (0) reads as a missing key. */
#define ALTS(n) ((n) + 1)

/* resources/lang/en/hint.php at 219fcafd, in file order. */
static const uint8_t item_alternatives[RANDO_ITEM_COUNT] = {
	[RANDO_ITEM_Nothing] = ALTS(0),
	[RANDO_ITEM_UncleSword] = ALTS(0),
	[RANDO_ITEM_L1Sword] = ALTS(0),
	[RANDO_ITEM_L1SwordAndShield] = ALTS(0),
	[RANDO_ITEM_L2Sword] = ALTS(0),
	[RANDO_ITEM_MasterSword] = ALTS(0),
	[RANDO_ITEM_L3Sword] = ALTS(0),
	[RANDO_ITEM_L4Sword] = ALTS(0),
	[RANDO_ITEM_BlueShield] = ALTS(0),
	[RANDO_ITEM_RedShield] = ALTS(0),
	[RANDO_ITEM_MirrorShield] = ALTS(0),
	[RANDO_ITEM_FireRod] = ALTS(2),
	[RANDO_ITEM_IceRod] = ALTS(2),
	[RANDO_ITEM_Hammer] = ALTS(2),
	[RANDO_ITEM_Hookshot] = ALTS(2),
	[RANDO_ITEM_Bow] = ALTS(2),
	[RANDO_ITEM_Boomerang] = ALTS(2),
	[RANDO_ITEM_Powder] = ALTS(2),
	[RANDO_ITEM_Bee] = ALTS(0),
	[RANDO_ITEM_Bombos] = ALTS(2),
	[RANDO_ITEM_Ether] = ALTS(2),
	[RANDO_ITEM_Quake] = ALTS(2),
	[RANDO_ITEM_Lamp] = ALTS(2),
	[RANDO_ITEM_Shovel] = ALTS(2),
	[RANDO_ITEM_OcarinaInactive] = ALTS(2),
	[RANDO_ITEM_CaneOfSomaria] = ALTS(2),
	[RANDO_ITEM_Bottle] = ALTS(0),
	[RANDO_ITEM_PieceOfHeart] = ALTS(0),
	[RANDO_ITEM_CaneOfByrna] = ALTS(2),
	[RANDO_ITEM_Cape] = ALTS(2),
	[RANDO_ITEM_MagicMirror] = ALTS(2),
	[RANDO_ITEM_PowerGlove] = ALTS(2),
	[RANDO_ITEM_TitansMitt] = ALTS(2),
	[RANDO_ITEM_BookOfMudora] = ALTS(2),
	[RANDO_ITEM_Flippers] = ALTS(2),
	[RANDO_ITEM_MoonPearl] = ALTS(2),
	[RANDO_ITEM_BugCatchingNet] = ALTS(2),
	[RANDO_ITEM_BlueMail] = ALTS(0),
	[RANDO_ITEM_RedMail] = ALTS(0),
	[RANDO_ITEM_Key] = ALTS(0),
	[RANDO_ITEM_Compass] = ALTS(0),
	[RANDO_ITEM_HeartContainerNoAnimation] = ALTS(0),
	[RANDO_ITEM_Bomb] = ALTS(0),
	[RANDO_ITEM_ThreeBombs] = ALTS(0),
	[RANDO_ITEM_Mushroom] = ALTS(2),
	[RANDO_ITEM_RedBoomerang] = ALTS(2),
	[RANDO_ITEM_BottleWithRedPotion] = ALTS(0),
	[RANDO_ITEM_BottleWithGreenPotion] = ALTS(0),
	[RANDO_ITEM_BottleWithBluePotion] = ALTS(0),
	[RANDO_ITEM_RedPotion] = ALTS(0),
	[RANDO_ITEM_GreenPotion] = ALTS(0),
	[RANDO_ITEM_BluePotion] = ALTS(0),
	[RANDO_ITEM_TenBombs] = ALTS(0),
	[RANDO_ITEM_BigKey] = ALTS(0),
	[RANDO_ITEM_Map] = ALTS(0),
	[RANDO_ITEM_OneRupee] = ALTS(0),
	[RANDO_ITEM_FiveRupees] = ALTS(0),
	[RANDO_ITEM_TwentyRupees] = ALTS(0),
	[RANDO_ITEM_PendantOfCourage] = ALTS(0),
	[RANDO_ITEM_PendantOfWisdom] = ALTS(0),
	[RANDO_ITEM_PendantOfPower] = ALTS(0),
	[RANDO_ITEM_BowAndArrows] = ALTS(2),
	[RANDO_ITEM_BowAndSilverArrows] = ALTS(2),
	[RANDO_ITEM_BottleWithBee] = ALTS(0),
	[RANDO_ITEM_BottleWithFairy] = ALTS(0),
	[RANDO_ITEM_BossHeartContainer] = ALTS(0),
	[RANDO_ITEM_HeartContainer] = ALTS(0),
	[RANDO_ITEM_OneHundredRupees] = ALTS(0),
	[RANDO_ITEM_FiftyRupees] = ALTS(0),
	[RANDO_ITEM_Heart] = ALTS(0),
	[RANDO_ITEM_Arrow] = ALTS(2),
	[RANDO_ITEM_TenArrows] = ALTS(0),
	[RANDO_ITEM_SmallMagic] = ALTS(0),
	[RANDO_ITEM_ThreeHundredRupees] = ALTS(0),
	[RANDO_ITEM_TwentyRupees2] = ALTS(0),
	[RANDO_ITEM_BottleWithGoldBee] = ALTS(0),
	[RANDO_ITEM_OcarinaActive] = ALTS(2),
	[RANDO_ITEM_PegasusBoots] = ALTS(2),
	[RANDO_ITEM_BombUpgrade5] = ALTS(0),
	[RANDO_ITEM_BombUpgrade10] = ALTS(0),
	[RANDO_ITEM_BombUpgrade50] = ALTS(0),
	[RANDO_ITEM_ArrowUpgrade5] = ALTS(0),
	[RANDO_ITEM_ArrowUpgrade10] = ALTS(0),
	[RANDO_ITEM_ArrowUpgrade70] = ALTS(0),
	[RANDO_ITEM_HalfMagic] = ALTS(2),
	[RANDO_ITEM_QuarterMagic] = ALTS(2),
	[RANDO_ITEM_Programmable1] = ALTS(0),
	[RANDO_ITEM_Programmable2] = ALTS(0),
	[RANDO_ITEM_Programmable3] = ALTS(0),
	[RANDO_ITEM_SilverArrowUpgrade] = ALTS(2),
	[RANDO_ITEM_Rupoor] = ALTS(0),
	[RANDO_ITEM_RedClock] = ALTS(0),
	[RANDO_ITEM_BlueClock] = ALTS(0),
	[RANDO_ITEM_GreenClock] = ALTS(0),
	[RANDO_ITEM_ProgressiveSword] = ALTS(0),
	[RANDO_ITEM_ProgressiveShield] = ALTS(0),
	[RANDO_ITEM_ProgressiveArmor] = ALTS(0),
	[RANDO_ITEM_ProgressiveGlove] = ALTS(2),
	[RANDO_ITEM_singleRNG] = ALTS(0),
	[RANDO_ITEM_multiRNG] = ALTS(0),
	[RANDO_ITEM_ProgressiveBow] = ALTS(2),
	[RANDO_ITEM_Triforce] = ALTS(0),
	[RANDO_ITEM_PowerStar] = ALTS(0),
	[RANDO_ITEM_TriforcePiece] = ALTS(0),
	[RANDO_ITEM_MapLW] = ALTS(0),
	[RANDO_ITEM_MapDW] = ALTS(0),
	[RANDO_ITEM_MapA2] = ALTS(0),
	[RANDO_ITEM_MapD7] = ALTS(0),
	[RANDO_ITEM_MapD4] = ALTS(0),
	[RANDO_ITEM_MapP3] = ALTS(0),
	[RANDO_ITEM_MapD5] = ALTS(0),
	[RANDO_ITEM_MapD3] = ALTS(0),
	[RANDO_ITEM_MapD6] = ALTS(0),
	[RANDO_ITEM_MapD1] = ALTS(0),
	[RANDO_ITEM_MapD2] = ALTS(0),
	[RANDO_ITEM_MapA1] = ALTS(0),
	[RANDO_ITEM_MapP2] = ALTS(0),
	[RANDO_ITEM_MapP1] = ALTS(0),
	[RANDO_ITEM_MapH1] = ALTS(0),
	[RANDO_ITEM_MapH2] = ALTS(0),
	[RANDO_ITEM_CompassA2] = ALTS(0),
	[RANDO_ITEM_CompassD7] = ALTS(0),
	[RANDO_ITEM_CompassD4] = ALTS(0),
	[RANDO_ITEM_CompassP3] = ALTS(0),
	[RANDO_ITEM_CompassD5] = ALTS(0),
	[RANDO_ITEM_CompassD3] = ALTS(0),
	[RANDO_ITEM_CompassD6] = ALTS(0),
	[RANDO_ITEM_CompassD1] = ALTS(0),
	[RANDO_ITEM_CompassD2] = ALTS(0),
	[RANDO_ITEM_CompassA1] = ALTS(0),
	[RANDO_ITEM_CompassP2] = ALTS(0),
	[RANDO_ITEM_CompassP1] = ALTS(0),
	[RANDO_ITEM_CompassH1] = ALTS(0),
	[RANDO_ITEM_CompassH2] = ALTS(0),
	[RANDO_ITEM_BigKeyA2] = ALTS(0),
	[RANDO_ITEM_BigKeyD7] = ALTS(0),
	[RANDO_ITEM_BigKeyD4] = ALTS(0),
	[RANDO_ITEM_BigKeyP3] = ALTS(0),
	[RANDO_ITEM_BigKeyD5] = ALTS(0),
	[RANDO_ITEM_BigKeyD3] = ALTS(0),
	[RANDO_ITEM_BigKeyD6] = ALTS(0),
	[RANDO_ITEM_BigKeyD1] = ALTS(0),
	[RANDO_ITEM_BigKeyD2] = ALTS(0),
	[RANDO_ITEM_BigKeyA1] = ALTS(0),
	[RANDO_ITEM_BigKeyP2] = ALTS(0),
	[RANDO_ITEM_BigKeyP1] = ALTS(0),
	[RANDO_ITEM_BigKeyH1] = ALTS(0),
	[RANDO_ITEM_BigKeyH2] = ALTS(0),
	[RANDO_ITEM_KeyH2] = ALTS(0),
	[RANDO_ITEM_KeyH1] = ALTS(0),
	[RANDO_ITEM_KeyP1] = ALTS(0),
	[RANDO_ITEM_KeyP2] = ALTS(0),
	[RANDO_ITEM_KeyA1] = ALTS(0),
	[RANDO_ITEM_KeyD2] = ALTS(0),
	[RANDO_ITEM_KeyD1] = ALTS(0),
	[RANDO_ITEM_KeyD6] = ALTS(0),
	[RANDO_ITEM_KeyD3] = ALTS(0),
	[RANDO_ITEM_KeyD5] = ALTS(0),
	[RANDO_ITEM_KeyP3] = ALTS(0),
	[RANDO_ITEM_KeyD4] = ALTS(0),
	[RANDO_ITEM_KeyD7] = ALTS(0),
	[RANDO_ITEM_KeyA2] = ALTS(0),
	[RANDO_ITEM_KeyGK] = ALTS(0),
	[RANDO_ITEM_Crystal1] = ALTS(0),
	[RANDO_ITEM_Crystal2] = ALTS(0),
	[RANDO_ITEM_Crystal3] = ALTS(0),
	[RANDO_ITEM_Crystal4] = ALTS(0),
	[RANDO_ITEM_Crystal5] = ALTS(0),
	[RANDO_ITEM_Crystal6] = ALTS(0),
	[RANDO_ITEM_Crystal7] = ALTS(0),
	[RANDO_ITEM_RescueZelda] = ALTS(0),
	[RANDO_ITEM_DefeatAgahnim] = ALTS(0),
	[RANDO_ITEM_BigRedBomb] = ALTS(0),
	[RANDO_ITEM_DefeatAgahnim2] = ALTS(0),
	[RANDO_ITEM_DefeatGanon] = ALTS(0),
};

static const uint8_t location_alternatives[RANDO_LOCATION_COUNT] = {
	[RANDO_LOC_AGAHNIM_2] = ALTS(0),
	[RANDO_LOC_AGAHNIM] = ALTS(0),
	[RANDO_LOC_AGINAHS_CAVE] = ALTS(2),
	[RANDO_LOC_BLACKSMITH] = ALTS(2),
	[RANDO_LOC_BLINDS_HIDEOUT_FAR_LEFT] = ALTS(0),
	[RANDO_LOC_BLINDS_HIDEOUT_FAR_RIGHT] = ALTS(0),
	[RANDO_LOC_BLINDS_HIDEOUT_LEFT] = ALTS(0),
	[RANDO_LOC_BLINDS_HIDEOUT_RIGHT] = ALTS(0),
	[RANDO_LOC_BLINDS_HIDEOUT_TOP] = ALTS(2),
	[RANDO_LOC_BOMBOS_TABLET] = ALTS(2),
	[RANDO_LOC_BOTTLE_MERCHANT] = ALTS(2),
	[RANDO_LOC_BREWERY] = ALTS(2),
	[RANDO_LOC_BUMPER_CAVE] = ALTS(2),
	[RANDO_LOC_C_SHAPED_HOUSE] = ALTS(0),
	[RANDO_LOC_CASTLE_TOWER_DARK_MAZE] = ALTS(2),
	[RANDO_LOC_CASTLE_TOWER_ROOM_03] = ALTS(0),
	[RANDO_LOC_CATFISH] = ALTS(4),
	[RANDO_LOC_CAVE_45] = ALTS(0),
	[RANDO_LOC_CHECKERBOARD_CAVE] = ALTS(2),
	[RANDO_LOC_CHEST_GAME] = ALTS(0),
	[RANDO_LOC_CHICKEN_HOUSE] = ALTS(2),
	[RANDO_LOC_DESERT_LEDGE] = ALTS(0),
	[RANDO_LOC_DESERT_PALACE_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_DESERT_PALACE_BIG_KEY_CHEST] = ALTS(0),
	[RANDO_LOC_DESERT_PALACE_COMPASS_CHEST] = ALTS(0),
	[RANDO_LOC_DESERT_PALACE_BOSS] = ALTS(2),
	[RANDO_LOC_DESERT_PALACE_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_DESERT_PALACE_PRIZE] = ALTS(0),
	[RANDO_LOC_DESERT_PALACE_TORCH] = ALTS(3),
	[RANDO_LOC_DIGGING_GAME] = ALTS(0),
	[RANDO_LOC_EASTERN_PALACE_BOSS] = ALTS(2),
	[RANDO_LOC_EASTERN_PALACE_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_EASTERN_PALACE_BIG_KEY_CHEST] = ALTS(0),
	[RANDO_LOC_EASTERN_PALACE_CANNONBALL_CHEST] = ALTS(0),
	[RANDO_LOC_EASTERN_PALACE_COMPASS_CHEST] = ALTS(0),
	[RANDO_LOC_EASTERN_PALACE_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_EASTERN_PALACE_PRIZE] = ALTS(0),
	[RANDO_LOC_ETHER_TABLET] = ALTS(2),
	[RANDO_LOC_FLOATING_ISLAND] = ALTS(2),
	[RANDO_LOC_FLOODGATE_CHEST] = ALTS(0),
	[RANDO_LOC_FLUTE_SPOT] = ALTS(2),
	[RANDO_LOC_GANON] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_CHEST] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_LEFT] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_RIGHT] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_BOBS_CHEST] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_BOBS_TORCH] = ALTS(3),
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_LEFT] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_RIGHT] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_LEFT] = ALTS(3),
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_RIGHT] = ALTS(3),
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT] = ALTS(3),
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_RIGHT] = ALTS(3),
	[RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM] = ALTS(3),
	[RANDO_LOC_GANONS_TOWER_HOPE_ROOM_LEFT] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_HOPE_ROOM_RIGHT] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_MAP_CHEST] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_LEFT] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_RIGHT] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST] = ALTS(2),
	[RANDO_LOC_GANONS_TOWER_PRE_MOLDORM_CHEST] = ALTS(0),
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT] = ALTS(4),
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT] = ALTS(4),
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT] = ALTS(4),
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT] = ALTS(4),
	[RANDO_LOC_GANONS_TOWER_TILE_ROOM] = ALTS(2),
	[RANDO_LOC_GRAVEYARD_LEDGE] = ALTS(2),
	[RANDO_LOC_HAMMER_PEGS] = ALTS(2),
	[RANDO_LOC_HOBO] = ALTS(2),
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_LEFT] = ALTS(3),
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_RIGHT] = ALTS(2),
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_LEFT] = ALTS(3),
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_RIGHT] = ALTS(3),
	[RANDO_LOC_HYPE_CAVE_BOTTOM] = ALTS(2),
	[RANDO_LOC_HYPE_CAVE_MIDDLE_LEFT] = ALTS(2),
	[RANDO_LOC_HYPE_CAVE_MIDDLE_RIGHT] = ALTS(2),
	[RANDO_LOC_HYPE_CAVE_NPC] = ALTS(3),
	[RANDO_LOC_HYPE_CAVE_TOP] = ALTS(2),
	[RANDO_LOC_HYRULE_CASTLE_BOOMERANG_CHEST] = ALTS(0),
	[RANDO_LOC_HYRULE_CASTLE_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_HYRULE_CASTLE_ZELDAS_CELL] = ALTS(0),
	[RANDO_LOC_ICE_PALACE_BIG_CHEST] = ALTS(2),
	[RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST] = ALTS(4),
	[RANDO_LOC_ICE_PALACE_COMPASS_CHEST] = ALTS(0),
	[RANDO_LOC_ICE_PALACE_FREEZOR_CHEST] = ALTS(2),
	[RANDO_LOC_ICE_PALACE_ICED_T_ROOM] = ALTS(2),
	[RANDO_LOC_ICE_PALACE_BOSS] = ALTS(5),
	[RANDO_LOC_ICE_PALACE_MAP_CHEST] = ALTS(4),
	[RANDO_LOC_ICE_PALACE_PRIZE] = ALTS(0),
	[RANDO_LOC_ICE_PALACE_SPIKE_ROOM] = ALTS(2),
	[RANDO_LOC_ICE_ROD_CAVE] = ALTS(2),
	[RANDO_LOC_KAKARIKO_TAVERN] = ALTS(0),
	[RANDO_LOC_KAKARIKO_WELL_BOTTOM] = ALTS(0),
	[RANDO_LOC_KAKARIKO_WELL_LEFT] = ALTS(0),
	[RANDO_LOC_KAKARIKO_WELL_MIDDLE] = ALTS(0),
	[RANDO_LOC_KAKARIKO_WELL_RIGHT] = ALTS(0),
	[RANDO_LOC_KAKARIKO_WELL_TOP] = ALTS(2),
	[RANDO_LOC_KING_ZORA] = ALTS(5),
	[RANDO_LOC_KINGS_TOMB] = ALTS(2),
	[RANDO_LOC_LAKE_HYLIA_ISLAND] = ALTS(0),
	[RANDO_LOC_LIBRARY] = ALTS(2),
	[RANDO_LOC_LINKS_HOUSE] = ALTS(0),
	[RANDO_LOC_LINKS_UNCLE] = ALTS(2),
	[RANDO_LOC_LOST_WOODS_HIDEOUT] = ALTS(0),
	[RANDO_LOC_LUMBERJACK_TREE] = ALTS(2),
	[RANDO_LOC_MAGIC_BAT] = ALTS(2),
	[RANDO_LOC_MASTER_SWORD_PEDESTAL] = ALTS(2),
	[RANDO_LOC_MAZE_RACE] = ALTS(0),
	[RANDO_LOC_MIMIC_CAVE] = ALTS(2),
	[RANDO_LOC_MINI_MOLDORM_CAVE_FAR_LEFT] = ALTS(2),
	[RANDO_LOC_MINI_MOLDORM_CAVE_FAR_RIGHT] = ALTS(2),
	[RANDO_LOC_MINI_MOLDORM_CAVE_LEFT] = ALTS(2),
	[RANDO_LOC_MINI_MOLDORM_CAVE_NPC] = ALTS(3),
	[RANDO_LOC_MINI_MOLDORM_CAVE_RIGHT] = ALTS(2),
	[RANDO_LOC_MIRE_SHED_LEFT] = ALTS(0),
	[RANDO_LOC_MIRE_SHED_RIGHT] = ALTS(0),
	[RANDO_LOC_MISERY_MIRE_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST] = ALTS(2),
	[RANDO_LOC_MISERY_MIRE_BRIDGE_CHEST] = ALTS(0),
	[RANDO_LOC_MISERY_MIRE_COMPASS_CHEST] = ALTS(2),
	[RANDO_LOC_MISERY_MIRE_MAIN_LOBBY] = ALTS(0),
	[RANDO_LOC_MISERY_MIRE_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_MISERY_MIRE_PRIZE] = ALTS(0),
	[RANDO_LOC_MISERY_MIRE_SPIKE_CHEST] = ALTS(0),
	[RANDO_LOC_MISERY_MIRE_BOSS] = ALTS(3),
	[RANDO_LOC_MISERY_MIRE_MEDALLION] = ALTS(0),
	[RANDO_LOC_MUSHROOM] = ALTS(2),
	[RANDO_LOC_OLD_MAN] = ALTS(3),
	[RANDO_LOC_PALACE_OF_DARKNESS_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_COMPASS_CHEST] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_LEFT] = ALTS(2),
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_RIGHT] = ALTS(2),
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_BOTTOM] = ALTS(2),
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_TOP] = ALTS(2),
	[RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_BOSS] = ALTS(4),
	[RANDO_LOC_PALACE_OF_DARKNESS_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_PRIZE] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_STALFOS_BASEMENT] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_BRIDGE] = ALTS(0),
	[RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_LEDGE] = ALTS(0),
	[RANDO_LOC_PARADOX_CAVE_LOWER_FAR_LEFT] = ALTS(0),
	[RANDO_LOC_PARADOX_CAVE_LOWER_FAR_RIGHT] = ALTS(0),
	[RANDO_LOC_PARADOX_CAVE_LOWER_LEFT] = ALTS(0),
	[RANDO_LOC_PARADOX_CAVE_LOWER_MIDDLE] = ALTS(0),
	[RANDO_LOC_PARADOX_CAVE_LOWER_RIGHT] = ALTS(0),
	[RANDO_LOC_PARADOX_CAVE_UPPER_LEFT] = ALTS(2),
	[RANDO_LOC_PARADOX_CAVE_UPPER_RIGHT] = ALTS(2),
	[RANDO_LOC_PEGASUS_ROCKS] = ALTS(2),
	[RANDO_LOC_POTION_SHOP] = ALTS(2),
	[RANDO_LOC_PURPLE_CHEST] = ALTS(0),
	[RANDO_LOC_PYRAMID_BOTTLE] = ALTS(0),
	[RANDO_LOC_PYRAMID_FAIRY_BOW] = ALTS(2),
	[RANDO_LOC_PYRAMID_FAIRY_LEFT] = ALTS(2),
	[RANDO_LOC_PYRAMID_FAIRY_RIGHT] = ALTS(2),
	[RANDO_LOC_PYRAMID_FAIRY_SWORD] = ALTS(2),
	[RANDO_LOC_PYRAMID] = ALTS(0),
	[RANDO_LOC_SAHASRAHLA] = ALTS(3),
	[RANDO_LOC_SAHASRAHLAS_HUT_LEFT] = ALTS(0),
	[RANDO_LOC_SAHASRAHLAS_HUT_MIDDLE] = ALTS(0),
	[RANDO_LOC_SAHASRAHLAS_HUT_RIGHT] = ALTS(0),
	[RANDO_LOC_SANCTUARY] = ALTS(0),
	[RANDO_LOC_SECRET_PASSAGE] = ALTS(0),
	[RANDO_LOC_SEWERS_DARK_CROSS] = ALTS(2),
	[RANDO_LOC_SEWERS_SECRET_ROOM_LEFT] = ALTS(0),
	[RANDO_LOC_SEWERS_SECRET_ROOM_MIDDLE] = ALTS(0),
	[RANDO_LOC_SEWERS_SECRET_ROOM_RIGHT] = ALTS(0),
	[RANDO_LOC_SICK_KID] = ALTS(3),
	[RANDO_LOC_SKULL_WOODS_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_SKULL_WOODS_BIG_KEY_CHEST] = ALTS(0),
	[RANDO_LOC_SKULL_WOODS_BRIDGE_ROOM] = ALTS(0),
	[RANDO_LOC_SKULL_WOODS_COMPASS_CHEST] = ALTS(0),
	[RANDO_LOC_SKULL_WOODS_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_SKULL_WOODS_BOSS] = ALTS(2),
	[RANDO_LOC_SKULL_WOODS_PINBALL_ROOM] = ALTS(0),
	[RANDO_LOC_SKULL_WOODS_POT_PRISON] = ALTS(0),
	[RANDO_LOC_SKULL_WOODS_PRIZE] = ALTS(0),
	[RANDO_LOC_SPECTACLE_ROCK_CAVE] = ALTS(2),
	[RANDO_LOC_SPECTACLE_ROCK] = ALTS(2),
	[RANDO_LOC_SPIKE_CAVE] = ALTS(3),
	[RANDO_LOC_SPIRAL_CAVE] = ALTS(0),
	[RANDO_LOC_STUMPY] = ALTS(2),
	[RANDO_LOC_SUNKEN_TREASURE] = ALTS(0),
	[RANDO_LOC_SUPERBUNNY_CAVE_BOTTOM] = ALTS(0),
	[RANDO_LOC_SUPERBUNNY_CAVE_TOP] = ALTS(0),
	[RANDO_LOC_SWAMP_PALACE_BOSS] = ALTS(4),
	[RANDO_LOC_SWAMP_PALACE_BIG_CHEST] = ALTS(3),
	[RANDO_LOC_SWAMP_PALACE_BIG_KEY_CHEST] = ALTS(3),
	[RANDO_LOC_SWAMP_PALACE_COMPASS_CHEST] = ALTS(2),
	[RANDO_LOC_SWAMP_PALACE_ENTRANCE] = ALTS(0),
	[RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_LEFT] = ALTS(4),
	[RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_RIGHT] = ALTS(4),
	[RANDO_LOC_SWAMP_PALACE_MAP_CHEST] = ALTS(2),
	[RANDO_LOC_SWAMP_PALACE_PRIZE] = ALTS(0),
	[RANDO_LOC_SWAMP_PALACE_WATERFALL_ROOM] = ALTS(3),
	[RANDO_LOC_SWAMP_PALACE_WEST_CHEST] = ALTS(3),
	[RANDO_LOC_THIEVES_TOWN_AMBUSH_CHEST] = ALTS(0),
	[RANDO_LOC_THIEVES_TOWN_ATTIC] = ALTS(0),
	[RANDO_LOC_THIEVES_TOWN_BIG_CHEST] = ALTS(2),
	[RANDO_LOC_THIEVES_TOWN_BIG_KEY_CHEST] = ALTS(0),
	[RANDO_LOC_THIEVES_TOWN_BOSS] = ALTS(2),
	[RANDO_LOC_THIEVES_TOWN_BLINDS_CELL] = ALTS(0),
	[RANDO_LOC_THIEVES_TOWN_COMPASS_CHEST] = ALTS(0),
	[RANDO_LOC_THIEVES_TOWN_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_THIEVES_TOWN_PRIZE] = ALTS(0),
	[RANDO_LOC_TOWER_OF_HERA_BASEMENT_CAGE] = ALTS(2),
	[RANDO_LOC_TOWER_OF_HERA_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST] = ALTS(2),
	[RANDO_LOC_TOWER_OF_HERA_COMPASS_CHEST] = ALTS(0),
	[RANDO_LOC_TOWER_OF_HERA_MAP_CHEST] = ALTS(0),
	[RANDO_LOC_TOWER_OF_HERA_BOSS] = ALTS(2),
	[RANDO_LOC_TOWER_OF_HERA_PRIZE] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_BIG_CHEST] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_COMPASS_CHEST] = ALTS(2),
	[RANDO_LOC_TURTLE_ROCK_CRYSTAROLLER_ROOM] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_LEFT] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_RIGHT] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_RIGHT] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_PRIZE] = ALTS(0),
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_LEFT] = ALTS(2),
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_RIGHT] = ALTS(2),
	[RANDO_LOC_TURTLE_ROCK_BOSS] = ALTS(3),
	[RANDO_LOC_TURTLE_ROCK_MEDALLION] = ALTS(2),
	[RANDO_LOC_WATERFALL_BOTTLE] = ALTS(0),
	[RANDO_LOC_WATERFALL_FAIRY_LEFT] = ALTS(3),
	[RANDO_LOC_WATERFALL_FAIRY_RIGHT] = ALTS(3),
	[RANDO_LOC_ZELDA] = ALTS(0),
	[RANDO_LOC_ZORAS_LEDGE] = ALTS(0),
};

int rando_hint_item_alternatives(RandoItemId item)
{
	assert(item < RANDO_ITEM_COUNT);
	return item_alternatives[item] - 1;
}

int rando_hint_location_alternatives(RandoLocationId location)
{
	assert(location < RANDO_LOCATION_COUNT);
	return location_alternatives[location] - 1;
}

/* An alternative index, or 0 with no draw for a plain string or a missing key. */
static uint8_t pick_name(RandoPrng *prng, int alternatives)
{
	return alternatives > 0 ? rando_pick_first(prng, alternatives) : 0;
}

bool rando_location_hint(const RandoWorld *world, RandoLocationId location, RandoPrng *prng, RandoLocationHint *out)
{
	RandoItemId item = rando_world_item(world, location);
	const RandoItemInfo *info;

	if (item == RANDO_ITEM_NONE)
		return false;
	info = rando_item_info(item);
	if (world->config.rom_generic_keys && info->item_class == RANDO_ITEM_CLASS_KEY)
		item = RANDO_ITEM_KeyGK;
	else if (info->item_class == RANDO_ITEM_CLASS_ALIAS)
		item = info->alias_target;
	out->location = location;
	out->item = item;
	out->item_name = pick_name(prng, rando_hint_item_alternatives(item));
	out->location_name = pick_name(prng, rando_hint_location_alternatives(location));
	return true;
}

RandoCollectionHint rando_collection_hint(const RandoWorld *world, const RandoLocationId *locations, int n,
					  RandoPrng *prng, RandoLocationHint *out)
{
	RandoLocationId prime = RANDO_LOCATION_NONE;
	int i, items = 0;

	for (i = 0; i < n; i++)
		if (rando_world_item(world, locations[i]) != RANDO_ITEM_NONE && items++ == 0)
			prime = locations[i];
	if (items == 0)
		return RANDO_COLLECTION_HINT_NONE;
	if (items == 1) {
		rando_location_hint(world, prime, prng, out);
		return RANDO_COLLECTION_HINT_ONE;
	}
	out->location = prime;
	out->item = RANDO_ITEM_NONE;
	out->item_name = 0;
	out->location_name = 0;
	return RANDO_COLLECTION_HINT_MANY;
}

/* HintService's $locations; RANDO_LOCATION_NONE marks a pair pick. */
static const RandoLocationId hint_picks[] = {
	RANDO_LOC_SAHASRAHLA,
	RANDO_LOC_MIMIC_CAVE,
	RANDO_LOC_CATFISH,
	RANDO_LOC_GRAVEYARD_LEDGE,
	RANDO_LOC_PURPLE_CHEST,
	RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST,
	RANDO_LOC_SWAMP_PALACE_BIG_CHEST,
	RANDO_LOCATION_NONE,
	RANDO_LOCATION_NONE,
	RANDO_LOCATION_NONE,
};

#define HINT_PICKS ((int)(sizeof hint_picks / sizeof hint_picks[0]))
#define HINT_PICK_ROUNDS 5
#define HINT_ITEMS_MAX 4

static bool hintable_location(RandoLocationId location)
{
	RandoLocationClass c = rando_location_info(location)->location_class;

	return !rando_location_is_a(c, RANDO_LOCATION_CLASS_MEDALLION)
		&& !rando_location_is_a(c, RANDO_LOCATION_CLASS_FOUNTAIN)
		&& !rando_location_is_a(c, RANDO_LOCATION_CLASS_PRIZE)
		&& !rando_location_is_a(c, RANDO_LOCATION_CLASS_TRADE);
}

static bool dungeon_item(const RandoWorld *world, RandoItemClass c)
{
	return c == RANDO_ITEM_CLASS_KEY || c == RANDO_ITEM_CLASS_MAP || c == RANDO_ITEM_CLASS_COMPASS
		|| (!world->config.region_wild_big_keys && c == RANDO_ITEM_CLASS_BIG_KEY);
}

static bool hintable_item(const RandoWorld *world, RandoItemId item)
{
	RandoItemClass c = rando_item_info(item)->item_class;

	return c != RANDO_ITEM_CLASS_SHIELD && !dungeon_item(world, c) && c != RANDO_ITEM_CLASS_BOTTLE
		&& c != RANDO_ITEM_CLASS_SWORD && item != RANDO_ITEM_TenBombs && item != RANDO_ITEM_HalfMagic
		&& item != RANDO_ITEM_BugCatchingNet && item != RANDO_ITEM_Powder && item != RANDO_ITEM_Mushroom;
}

static void tile_location(RandoHints *hints, uint8_t tile, const RandoLocationHint *hint)
{
	hints->tiles[tile].kind = RANDO_TILE_HINT_LOCATION;
	hints->tiles[tile].location = *hint;
}

/* Pops a tile for location's hint, as VT does only when the hint is not null. */
static void pop_tile_hint(const RandoWorld *world, RandoLocationId location, RandoPrng *prng, RandoHints *hints,
			  uint8_t *tiles, int *ntiles)
{
	RandoLocationHint hint;

	if (location == RANDO_LOCATION_NONE || !rando_location_hint(world, location, prng, &hint))
		return;
	assert(*ntiles > 0);
	tile_location(hints, tiles[--*ntiles], &hint);
}

/* The item's location for a hint, drawn by random() among those VT keeps; RANDO_LOCATION_NONE, no draw,
 * when none is left. */
static RandoLocationId hint_item_location(const RandoWorld *world, RandoItemId item, RandoLocationId alternate_bow,
					  RandoPrng *prng)
{
	uint8_t ids[RANDO_LOCATION_COUNT];
	int i, n, kept = 0;

	n = rando_locations_with(world, item, ids);
	/* setTexts' alternate bow ([0x65]) is not == to Item::get('ProgressiveBow'). */
	for (i = 0; i < n; i++)
		if (hintable_location((RandoLocationId)ids[i])
		    && !(item == RANDO_ITEM_ProgressiveBow && ids[i] == alternate_bow))
			ids[kept++] = ids[i];
	if (kept == 0)
		return RANDO_LOCATION_NONE;
	return (RandoLocationId)ids[rando_random_int(prng, 0, kept - 1)];
}

void rando_apply_hints(const RandoWorld *world, RandoPrng *prng, const uint8_t *advancement, int n,
		       RandoPostFill *post)
{
	RandoHints *hints = &post->hints;
	uint8_t tiles[RANDO_TILE_COUNT], picks_order[HINT_PICKS], picks[HINT_PICKS];
	uint8_t hintables[RANDO_POOL_CAPACITY], ids[RANDO_LOCATION_COUNT], chosen[RANDO_LOCATION_COUNT + HINT_ITEMS_MAX];
	RandoLocationId hinted[HINT_ITEMS_MAX];
	int i, j, ntiles = RANDO_TILE_COUNT, npicks = HINT_PICKS, nhintables = 0, nhinted = 0, nids = 0, nchosen;
	int free_tiles;

	assert(n >= 0 && n <= (int)sizeof hintables);
	memset(hints, 0, sizeof *hints);
	hints->on = world->config.spoil_hints;
	if (!hints->on)
		return;

	for (i = 0; i < RANDO_TILE_COUNT; i++)
		tiles[i] = (uint8_t)i;
	rando_fy_shuffle(prng, tiles, RANDO_TILE_COUNT, 1);
	for (i = 0; i < HINT_PICKS; i++)
		picks_order[i] = (uint8_t)i;
	rando_fy_shuffle(prng, picks_order, HINT_PICKS, 1);

	if (world->config.region_wild_big_keys)
		pop_tile_hint(world, rando_first_with(world, RANDO_ITEM_BigKeyA2), prng, hints, tiles, &ntiles);
	pop_tile_hint(world, rando_first_with(world, RANDO_ITEM_PegasusBoots), prng, hints, tiles, &ntiles);

	for (i = 0; i < HINT_PICKS; i++)
		picks[i] = (uint8_t)i;
	for (i = 0; i < HINT_PICK_ROUNDS; i++) {
		rando_fy_shuffle(prng, picks, (size_t)npicks, 1);
		/* A pair pick matches nothing: VT filters getName(), which has a ":<world id>" suffix. */
		pop_tile_hint(world, hint_picks[picks_order[picks[--npicks]]], prng, hints, tiles, &ntiles);
	}

	for (i = 0; i < n; i++)
		if (hintable_item(world, (RandoItemId)advancement[i]))
			hintables[nhintables++] = advancement[i];
	rando_fy_shuffle(prng, hintables, (size_t)nhintables, 1);
	for (i = 0; i < nhintables && i < HINT_ITEMS_MAX && i < ntiles; i++) {
		RandoLocationId location =
			hint_item_location(world, (RandoItemId)hintables[i], post->texts.alternate_bow, prng);

		if (location != RANDO_LOCATION_NONE)
			hinted[nhinted++] = location;
	}

	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);
		RandoItemId item = rando_world_item(world, location);

		if (item != RANDO_ITEM_NONE && hintable_location(location)
		    && !dungeon_item(world, rando_item_info(item)->item_class))
			ids[nids++] = (uint8_t)location;
	}
	free_tiles = ntiles - nhinted;
	nchosen = rando_random_collection(prng, ids, nids,
					  (int)rando_random_int(prng, free_tiles / 2 - 1, free_tiles - 1), chosen);
	/* LocationCollection keys by name: a hinted location already drawn keeps its first position. */
	for (i = 0; i < nhinted; i++) {
		for (j = 0; j < nchosen && chosen[j] != hinted[i]; j++)
			;
		if (j == nchosen)
			chosen[nchosen++] = (uint8_t)hinted[i];
	}

	for (i = 0; i < ntiles; i++) {
		RandoLocationHint hint;

		if (nchosen > 0 && rando_location_hint(world, (RandoLocationId)chosen[--nchosen], prng, &hint)) {
			tile_location(hints, tiles[i], &hint);
		} else {
			hints->tiles[tiles[i]].kind = RANDO_TILE_HINT_JOKE;
			hints->tiles[tiles[i]].joke = rando_pick_first(prng, RANDO_STRINGS_HINT);
		}
	}
}
