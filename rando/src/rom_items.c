#include "rom_emit.h"

#include <assert.h>

typedef enum Slot {
	SLOT_0,
	SLOT_1,
	SLOT_2,
	SLOT_3,
	SLOT_4,
	SLOT_5,
	SLOT_6,
	SLOT_T0,
	SLOT_T1,
	SLOT_T2,
	SLOT_M0,
	SLOT_M1,
	SLOT_M2,
	SLOT_COUNT,
} Slot;

typedef struct ItemBytes {
	uint16_t present;
	uint8_t bytes[SLOT_COUNT];
} ItemBytes;

#define ADDRESSES_MAX 6

typedef struct LocationAddresses {
	uint8_t count;
	struct {
		uint8_t slot;
		uint32_t address;
	} at[ADDRESSES_MAX];
} LocationAddresses;

static const ItemBytes item_bytes[RANDO_ITEM_COUNT] = {
	[RANDO_ITEM_Nothing] = {0x0001, {0x5A}},
	[RANDO_ITEM_L1Sword] = {0x0001, {0x49}},
	[RANDO_ITEM_L1SwordAndShield] = {0x0001, {0x00}},
	[RANDO_ITEM_L2Sword] = {0x0001, {0x01}},
	[RANDO_ITEM_MasterSword] = {0x0001, {0x50}},
	[RANDO_ITEM_L3Sword] = {0x0001, {0x02}},
	[RANDO_ITEM_L4Sword] = {0x0001, {0x03}},
	[RANDO_ITEM_BlueShield] = {0x0001, {0x04}},
	[RANDO_ITEM_RedShield] = {0x0001, {0x05}},
	[RANDO_ITEM_MirrorShield] = {0x0001, {0x06}},
	[RANDO_ITEM_FireRod] = {0x0001, {0x07}},
	[RANDO_ITEM_IceRod] = {0x0001, {0x08}},
	[RANDO_ITEM_Hammer] = {0x0001, {0x09}},
	[RANDO_ITEM_Hookshot] = {0x0001, {0x0A}},
	[RANDO_ITEM_Bow] = {0x0001, {0x0B}},
	[RANDO_ITEM_Boomerang] = {0x0001, {0x0C}},
	[RANDO_ITEM_Powder] = {0x0001, {0x0D}},
	[RANDO_ITEM_Bee] = {0x0001, {0x0E}},
	[RANDO_ITEM_Bombos] = {0x1F83, {0x0F, 0x00, 0, 0, 0, 0, 0, 0x31, 0x90, 0x00, 0x31, 0x80, 0x00}},
	[RANDO_ITEM_Ether] = {0x1F83, {0x10, 0x01, 0, 0, 0, 0, 0, 0x31, 0x98, 0x00, 0x13, 0x9F, 0xF1}},
	[RANDO_ITEM_Quake] = {0x1F83, {0x11, 0x02, 0, 0, 0, 0, 0, 0x14, 0xEF, 0xC4, 0x31, 0x88, 0x00}},
	[RANDO_ITEM_Lamp] = {0x0001, {0x12}},
	[RANDO_ITEM_Shovel] = {0x0001, {0x13}},
	[RANDO_ITEM_OcarinaInactive] = {0x0001, {0x14}},
	[RANDO_ITEM_CaneOfSomaria] = {0x0001, {0x15}},
	[RANDO_ITEM_Bottle] = {0x0001, {0x16}},
	[RANDO_ITEM_PieceOfHeart] = {0x0001, {0x17}},
	[RANDO_ITEM_CaneOfByrna] = {0x0001, {0x18}},
	[RANDO_ITEM_Cape] = {0x0001, {0x19}},
	[RANDO_ITEM_MagicMirror] = {0x0001, {0x1A}},
	[RANDO_ITEM_PowerGlove] = {0x0001, {0x1B}},
	[RANDO_ITEM_TitansMitt] = {0x0001, {0x1C}},
	[RANDO_ITEM_BookOfMudora] = {0x0001, {0x1D}},
	[RANDO_ITEM_Flippers] = {0x0001, {0x1E}},
	[RANDO_ITEM_MoonPearl] = {0x0001, {0x1F}},
	[RANDO_ITEM_Crystal] = {0x0001, {0x20}},
	[RANDO_ITEM_BugCatchingNet] = {0x0001, {0x21}},
	[RANDO_ITEM_BlueMail] = {0x0001, {0x22}},
	[RANDO_ITEM_RedMail] = {0x0001, {0x23}},
	[RANDO_ITEM_Key] = {0x0001, {0x24}},
	[RANDO_ITEM_Compass] = {0x0001, {0x25}},
	[RANDO_ITEM_HeartContainerNoAnimation] = {0x0001, {0x26}},
	[RANDO_ITEM_Bomb] = {0x0001, {0x27}},
	[RANDO_ITEM_ThreeBombs] = {0x0001, {0x28}},
	[RANDO_ITEM_Mushroom] = {0x0001, {0x29}},
	[RANDO_ITEM_RedBoomerang] = {0x0001, {0x2A}},
	[RANDO_ITEM_BottleWithRedPotion] = {0x0001, {0x2B}},
	[RANDO_ITEM_BottleWithGreenPotion] = {0x0001, {0x2C}},
	[RANDO_ITEM_BottleWithBluePotion] = {0x0001, {0x2D}},
	[RANDO_ITEM_RedPotion] = {0x0001, {0x2E}},
	[RANDO_ITEM_GreenPotion] = {0x0001, {0x2F}},
	[RANDO_ITEM_BluePotion] = {0x0001, {0x30}},
	[RANDO_ITEM_TenBombs] = {0x0001, {0x31}},
	[RANDO_ITEM_BigKey] = {0x0001, {0x32}},
	[RANDO_ITEM_Map] = {0x0001, {0x33}},
	[RANDO_ITEM_OneRupee] = {0x0001, {0x34}},
	[RANDO_ITEM_FiveRupees] = {0x0001, {0x35}},
	[RANDO_ITEM_TwentyRupees] = {0x0001, {0x36}},
	[RANDO_ITEM_PendantOfCourage] = {0x007F, {0x37, 0x04, 0x38, 0x62, 0x00, 0x69, 0x37}},
	[RANDO_ITEM_PendantOfWisdom] = {0x007F, {0x38, 0x01, 0x32, 0x60, 0x00, 0x69, 0x38}},
	[RANDO_ITEM_PendantOfPower] = {0x007F, {0x39, 0x02, 0x34, 0x60, 0x00, 0x69, 0x39}},
	[RANDO_ITEM_BowAndArrows] = {0x0001, {0x3A}},
	[RANDO_ITEM_BowAndSilverArrows] = {0x0001, {0x3B}},
	[RANDO_ITEM_BottleWithBee] = {0x0001, {0x3C}},
	[RANDO_ITEM_BottleWithFairy] = {0x0001, {0x3D}},
	[RANDO_ITEM_BossHeartContainer] = {0x0001, {0x3E}},
	[RANDO_ITEM_HeartContainer] = {0x0001, {0x3F}},
	[RANDO_ITEM_OneHundredRupees] = {0x0001, {0x40}},
	[RANDO_ITEM_FiftyRupees] = {0x0001, {0x41}},
	[RANDO_ITEM_Heart] = {0x0001, {0x42}},
	[RANDO_ITEM_Arrow] = {0x0001, {0x43}},
	[RANDO_ITEM_TenArrows] = {0x0001, {0x44}},
	[RANDO_ITEM_SmallMagic] = {0x0001, {0x45}},
	[RANDO_ITEM_ThreeHundredRupees] = {0x0001, {0x46}},
	[RANDO_ITEM_TwentyRupees2] = {0x0001, {0x47}},
	[RANDO_ITEM_BottleWithGoldBee] = {0x0001, {0x48}},
	[RANDO_ITEM_OcarinaActive] = {0x0001, {0x4A}},
	[RANDO_ITEM_PegasusBoots] = {0x0001, {0x4B}},
	[RANDO_ITEM_BombUpgrade5] = {0x0001, {0x51}},
	[RANDO_ITEM_BombUpgrade10] = {0x0001, {0x52}},
	[RANDO_ITEM_BombUpgrade50] = {0x0001, {0x4C}},
	[RANDO_ITEM_ArrowUpgrade5] = {0x0001, {0x53}},
	[RANDO_ITEM_ArrowUpgrade10] = {0x0001, {0x54}},
	[RANDO_ITEM_ArrowUpgrade70] = {0x0001, {0x4D}},
	[RANDO_ITEM_HalfMagic] = {0x0001, {0x4E}},
	[RANDO_ITEM_QuarterMagic] = {0x0001, {0x4F}},
	[RANDO_ITEM_Programmable1] = {0x0001, {0x55}},
	[RANDO_ITEM_Programmable2] = {0x0001, {0x56}},
	[RANDO_ITEM_Programmable3] = {0x0001, {0x57}},
	[RANDO_ITEM_SilverArrowUpgrade] = {0x0001, {0x58}},
	[RANDO_ITEM_Rupoor] = {0x0001, {0x59}},
	[RANDO_ITEM_RedClock] = {0x0001, {0x5B}},
	[RANDO_ITEM_BlueClock] = {0x0001, {0x5C}},
	[RANDO_ITEM_GreenClock] = {0x0001, {0x5D}},
	[RANDO_ITEM_ProgressiveSword] = {0x0001, {0x5E}},
	[RANDO_ITEM_ProgressiveShield] = {0x0001, {0x5F}},
	[RANDO_ITEM_ProgressiveArmor] = {0x0001, {0x60}},
	[RANDO_ITEM_ProgressiveGlove] = {0x0001, {0x61}},
	[RANDO_ITEM_singleRNG] = {0x0001, {0x62}},
	[RANDO_ITEM_multiRNG] = {0x0001, {0x63}},
	[RANDO_ITEM_ProgressiveBow] = {0x0001, {0x64}},
	[RANDO_ITEM_ProgressiveBowAlternate] = {0x0001, {0x65}},
	[RANDO_ITEM_Triforce] = {0x0001, {0x6A}},
	[RANDO_ITEM_PowerStar] = {0x0001, {0x6B}},
	[RANDO_ITEM_TriforcePiece] = {0x0001, {0x6C}},
	[RANDO_ITEM_MapLW] = {0x0001, {0x70}},
	[RANDO_ITEM_MapDW] = {0x0001, {0x71}},
	[RANDO_ITEM_MapA2] = {0x0001, {0x72}},
	[RANDO_ITEM_MapD7] = {0x0001, {0x73}},
	[RANDO_ITEM_MapD4] = {0x0001, {0x74}},
	[RANDO_ITEM_MapP3] = {0x0001, {0x75}},
	[RANDO_ITEM_MapD5] = {0x0001, {0x76}},
	[RANDO_ITEM_MapD3] = {0x0001, {0x77}},
	[RANDO_ITEM_MapD6] = {0x0001, {0x78}},
	[RANDO_ITEM_MapD1] = {0x0001, {0x79}},
	[RANDO_ITEM_MapD2] = {0x0001, {0x7A}},
	[RANDO_ITEM_MapA1] = {0x0001, {0x7B}},
	[RANDO_ITEM_MapP2] = {0x0001, {0x7C}},
	[RANDO_ITEM_MapP1] = {0x0001, {0x7D}},
	[RANDO_ITEM_MapH1] = {0x0001, {0x7E}},
	[RANDO_ITEM_MapH2] = {0x0001, {0x7F}},
	[RANDO_ITEM_CompassA2] = {0x0001, {0x82}},
	[RANDO_ITEM_CompassD7] = {0x0001, {0x83}},
	[RANDO_ITEM_CompassD4] = {0x0001, {0x84}},
	[RANDO_ITEM_CompassP3] = {0x0001, {0x85}},
	[RANDO_ITEM_CompassD5] = {0x0001, {0x86}},
	[RANDO_ITEM_CompassD3] = {0x0001, {0x87}},
	[RANDO_ITEM_CompassD6] = {0x0001, {0x88}},
	[RANDO_ITEM_CompassD1] = {0x0001, {0x89}},
	[RANDO_ITEM_CompassD2] = {0x0001, {0x8A}},
	[RANDO_ITEM_CompassA1] = {0x0001, {0x8B}},
	[RANDO_ITEM_CompassP2] = {0x0001, {0x8C}},
	[RANDO_ITEM_CompassP1] = {0x0001, {0x8D}},
	[RANDO_ITEM_CompassH1] = {0x0001, {0x8E}},
	[RANDO_ITEM_CompassH2] = {0x0001, {0x8F}},
	[RANDO_ITEM_BigKeyA2] = {0x0001, {0x92}},
	[RANDO_ITEM_BigKeyD7] = {0x0001, {0x93}},
	[RANDO_ITEM_BigKeyD4] = {0x0001, {0x94}},
	[RANDO_ITEM_BigKeyP3] = {0x0001, {0x95}},
	[RANDO_ITEM_BigKeyD5] = {0x0001, {0x96}},
	[RANDO_ITEM_BigKeyD3] = {0x0001, {0x97}},
	[RANDO_ITEM_BigKeyD6] = {0x0001, {0x98}},
	[RANDO_ITEM_BigKeyD1] = {0x0001, {0x99}},
	[RANDO_ITEM_BigKeyD2] = {0x0001, {0x9A}},
	[RANDO_ITEM_BigKeyA1] = {0x0001, {0x9B}},
	[RANDO_ITEM_BigKeyP2] = {0x0001, {0x9C}},
	[RANDO_ITEM_BigKeyP1] = {0x0001, {0x9D}},
	[RANDO_ITEM_BigKeyH1] = {0x0001, {0x9E}},
	[RANDO_ITEM_BigKeyH2] = {0x0001, {0x9F}},
	[RANDO_ITEM_KeyH2] = {0x0001, {0xA0}},
	[RANDO_ITEM_KeyH1] = {0x0001, {0xA1}},
	[RANDO_ITEM_KeyP1] = {0x0001, {0xA2}},
	[RANDO_ITEM_KeyP2] = {0x0001, {0xA3}},
	[RANDO_ITEM_KeyA1] = {0x0001, {0xA4}},
	[RANDO_ITEM_KeyD2] = {0x0001, {0xA5}},
	[RANDO_ITEM_KeyD1] = {0x0001, {0xA6}},
	[RANDO_ITEM_KeyD6] = {0x0001, {0xA7}},
	[RANDO_ITEM_KeyD3] = {0x0001, {0xA8}},
	[RANDO_ITEM_KeyD5] = {0x0001, {0xA9}},
	[RANDO_ITEM_KeyP3] = {0x0001, {0xAA}},
	[RANDO_ITEM_KeyD4] = {0x0001, {0xAB}},
	[RANDO_ITEM_KeyD7] = {0x0001, {0xAC}},
	[RANDO_ITEM_KeyA2] = {0x0001, {0xAD}},
	[RANDO_ITEM_KeyGK] = {0x0001, {0xAF}},
	[RANDO_ITEM_Crystal1] = {0x007E, {0, 0x02, 0x34, 0x64, 0x40, 0x7F, 0x20}},
	[RANDO_ITEM_Crystal2] = {0x007E, {0, 0x10, 0x34, 0x64, 0x40, 0x79, 0x20}},
	[RANDO_ITEM_Crystal3] = {0x007E, {0, 0x40, 0x34, 0x64, 0x40, 0x6C, 0x20}},
	[RANDO_ITEM_Crystal4] = {0x007E, {0, 0x20, 0x34, 0x64, 0x40, 0x6D, 0x20}},
	[RANDO_ITEM_Crystal5] = {0x007E, {0, 0x04, 0x32, 0x64, 0x40, 0x6E, 0x20}},
	[RANDO_ITEM_Crystal6] = {0x007E, {0, 0x01, 0x32, 0x64, 0x40, 0x6F, 0x20}},
	[RANDO_ITEM_Crystal7] = {0x007E, {0, 0x08, 0x34, 0x64, 0x40, 0x7C, 0x20}},
	[RANDO_ITEM_RescueZelda] = {0x0000, {0}},
	[RANDO_ITEM_DefeatAgahnim] = {0x0000, {0}},
	[RANDO_ITEM_BigRedBomb] = {0x0000, {0}},
	[RANDO_ITEM_DefeatAgahnim2] = {0x0000, {0}},
	[RANDO_ITEM_DefeatGanon] = {0x0000, {0}},
};

static const LocationAddresses location_addresses[RANDO_LOCATION_COUNT] = {
	[RANDO_LOC_SAHASRAHLAS_HUT_LEFT] = {1, {{SLOT_0, 0xEA82}}},
	[RANDO_LOC_SAHASRAHLAS_HUT_MIDDLE] = {1, {{SLOT_0, 0xEA85}}},
	[RANDO_LOC_SAHASRAHLAS_HUT_RIGHT] = {1, {{SLOT_0, 0xEA88}}},
	[RANDO_LOC_SAHASRAHLA] = {1, {{SLOT_0, 0x2F1FC}}},
	[RANDO_LOC_KING_ZORA] = {1, {{SLOT_0, 0xEE1C3}}},
	[RANDO_LOC_POTION_SHOP] = {1, {{SLOT_0, 0x180014}}},
	[RANDO_LOC_ZORAS_LEDGE] = {1, {{SLOT_0, 0x180149}}},
	[RANDO_LOC_WATERFALL_FAIRY_LEFT] = {1, {{SLOT_0, 0xE9B0}}},
	[RANDO_LOC_WATERFALL_FAIRY_RIGHT] = {1, {{SLOT_0, 0xE9D1}}},
	[RANDO_LOC_MASTER_SWORD_PEDESTAL] = {1, {{SLOT_0, 0x289B0}}},
	[RANDO_LOC_KINGS_TOMB] = {1, {{SLOT_0, 0xE97A}}},
	[RANDO_LOC_KAKARIKO_TAVERN] = {1, {{SLOT_0, 0xE9CE}}},
	[RANDO_LOC_CHICKEN_HOUSE] = {1, {{SLOT_0, 0xE9E9}}},
	[RANDO_LOC_KAKARIKO_WELL_TOP] = {1, {{SLOT_0, 0xEA8E}}},
	[RANDO_LOC_KAKARIKO_WELL_LEFT] = {1, {{SLOT_0, 0xEA91}}},
	[RANDO_LOC_KAKARIKO_WELL_MIDDLE] = {1, {{SLOT_0, 0xEA94}}},
	[RANDO_LOC_KAKARIKO_WELL_RIGHT] = {1, {{SLOT_0, 0xEA97}}},
	[RANDO_LOC_KAKARIKO_WELL_BOTTOM] = {1, {{SLOT_0, 0xEA9A}}},
	[RANDO_LOC_BLINDS_HIDEOUT_TOP] = {1, {{SLOT_0, 0xEB0F}}},
	[RANDO_LOC_BLINDS_HIDEOUT_LEFT] = {1, {{SLOT_0, 0xEB12}}},
	[RANDO_LOC_BLINDS_HIDEOUT_RIGHT] = {1, {{SLOT_0, 0xEB15}}},
	[RANDO_LOC_BLINDS_HIDEOUT_FAR_LEFT] = {1, {{SLOT_0, 0xEB18}}},
	[RANDO_LOC_BLINDS_HIDEOUT_FAR_RIGHT] = {1, {{SLOT_0, 0xEB1B}}},
	[RANDO_LOC_PEGASUS_ROCKS] = {1, {{SLOT_0, 0xEB3F}}},
	[RANDO_LOC_BOTTLE_MERCHANT] = {1, {{SLOT_0, 0x2EB18}}},
	[RANDO_LOC_MAGIC_BAT] = {1, {{SLOT_0, 0x180015}}},
	[RANDO_LOC_SICK_KID] = {1, {{SLOT_0, 0x339CF}}},
	[RANDO_LOC_LOST_WOODS_HIDEOUT] = {1, {{SLOT_0, 0x180000}}},
	[RANDO_LOC_LUMBERJACK_TREE] = {1, {{SLOT_0, 0x180001}}},
	[RANDO_LOC_GRAVEYARD_LEDGE] = {1, {{SLOT_0, 0x180004}}},
	[RANDO_LOC_MUSHROOM] = {1, {{SLOT_0, 0x180013}}},
	[RANDO_LOC_FLOODGATE_CHEST] = {1, {{SLOT_0, 0xE98C}}},
	[RANDO_LOC_LINKS_HOUSE] = {1, {{SLOT_0, 0xE9BC}}},
	[RANDO_LOC_AGINAHS_CAVE] = {1, {{SLOT_0, 0xE9F2}}},
	[RANDO_LOC_MINI_MOLDORM_CAVE_FAR_LEFT] = {1, {{SLOT_0, 0xEB42}}},
	[RANDO_LOC_MINI_MOLDORM_CAVE_LEFT] = {1, {{SLOT_0, 0xEB45}}},
	[RANDO_LOC_MINI_MOLDORM_CAVE_RIGHT] = {1, {{SLOT_0, 0xEB48}}},
	[RANDO_LOC_MINI_MOLDORM_CAVE_FAR_RIGHT] = {1, {{SLOT_0, 0xEB4B}}},
	[RANDO_LOC_ICE_ROD_CAVE] = {1, {{SLOT_0, 0xEB4E}}},
	[RANDO_LOC_HOBO] = {1, {{SLOT_0, 0x33E7D}}},
	[RANDO_LOC_BOMBOS_TABLET] = {1, {{SLOT_0, 0x180017}}},
	[RANDO_LOC_CAVE_45] = {1, {{SLOT_0, 0x180003}}},
	[RANDO_LOC_CHECKERBOARD_CAVE] = {1, {{SLOT_0, 0x180005}}},
	[RANDO_LOC_MINI_MOLDORM_CAVE_NPC] = {1, {{SLOT_0, 0x180010}}},
	[RANDO_LOC_LIBRARY] = {1, {{SLOT_0, 0x180012}}},
	[RANDO_LOC_MAZE_RACE] = {1, {{SLOT_0, 0x180142}}},
	[RANDO_LOC_DESERT_LEDGE] = {1, {{SLOT_0, 0x180143}}},
	[RANDO_LOC_LAKE_HYLIA_ISLAND] = {1, {{SLOT_0, 0x180144}}},
	[RANDO_LOC_SUNKEN_TREASURE] = {1, {{SLOT_0, 0x180145}}},
	[RANDO_LOC_FLUTE_SPOT] = {1, {{SLOT_0, 0x18014A}}},
	[RANDO_LOC_SANCTUARY] = {1, {{SLOT_0, 0xEA79}}},
	[RANDO_LOC_SEWERS_SECRET_ROOM_LEFT] = {1, {{SLOT_0, 0xEB5D}}},
	[RANDO_LOC_SEWERS_SECRET_ROOM_MIDDLE] = {1, {{SLOT_0, 0xEB60}}},
	[RANDO_LOC_SEWERS_SECRET_ROOM_RIGHT] = {1, {{SLOT_0, 0xEB63}}},
	[RANDO_LOC_SEWERS_DARK_CROSS] = {1, {{SLOT_0, 0xE96E}}},
	[RANDO_LOC_HYRULE_CASTLE_BOOMERANG_CHEST] = {1, {{SLOT_0, 0xE974}}},
	[RANDO_LOC_HYRULE_CASTLE_MAP_CHEST] = {1, {{SLOT_0, 0xEB0C}}},
	[RANDO_LOC_HYRULE_CASTLE_ZELDAS_CELL] = {1, {{SLOT_0, 0xEB09}}},
	[RANDO_LOC_LINKS_UNCLE] = {1, {{SLOT_0, 0x2DF45}}},
	[RANDO_LOC_SECRET_PASSAGE] = {1, {{SLOT_0, 0xE971}}},
	[RANDO_LOC_ZELDA] = {0},
	[RANDO_LOC_EASTERN_PALACE_COMPASS_CHEST] = {1, {{SLOT_0, 0xE977}}},
	[RANDO_LOC_EASTERN_PALACE_BIG_CHEST] = {1, {{SLOT_0, 0xE97D}}},
	[RANDO_LOC_EASTERN_PALACE_CANNONBALL_CHEST] = {1, {{SLOT_0, 0xE9B3}}},
	[RANDO_LOC_EASTERN_PALACE_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xE9B9}}},
	[RANDO_LOC_EASTERN_PALACE_MAP_CHEST] = {1, {{SLOT_0, 0xE9F5}}},
	[RANDO_LOC_EASTERN_PALACE_BOSS] = {1, {{SLOT_0, 0x180150}}},
	[RANDO_LOC_EASTERN_PALACE_PRIZE] = {6, {{SLOT_1, 0x1209D}, {SLOT_2, 0x53E76}, {SLOT_3, 0x53E77}, {SLOT_4, 0x180052}, {SLOT_5, 0x180070}, {SLOT_6, 0xC6FE}}},
	[RANDO_LOC_DESERT_PALACE_BIG_CHEST] = {1, {{SLOT_0, 0xE98F}}},
	[RANDO_LOC_DESERT_PALACE_MAP_CHEST] = {1, {{SLOT_0, 0xE9B6}}},
	[RANDO_LOC_DESERT_PALACE_TORCH] = {1, {{SLOT_0, 0x180160}}},
	[RANDO_LOC_DESERT_PALACE_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xE9C2}}},
	[RANDO_LOC_DESERT_PALACE_COMPASS_CHEST] = {1, {{SLOT_0, 0xE9CB}}},
	[RANDO_LOC_DESERT_PALACE_BOSS] = {1, {{SLOT_0, 0x180151}}},
	[RANDO_LOC_DESERT_PALACE_PRIZE] = {6, {{SLOT_1, 0x1209E}, {SLOT_2, 0x53E7A}, {SLOT_3, 0x53E7B}, {SLOT_4, 0x180053}, {SLOT_5, 0x180072}, {SLOT_6, 0xC6FF}}},
	[RANDO_LOC_OLD_MAN] = {1, {{SLOT_0, 0xF69FA}}},
	[RANDO_LOC_SPECTACLE_ROCK_CAVE] = {1, {{SLOT_0, 0x180002}}},
	[RANDO_LOC_ETHER_TABLET] = {1, {{SLOT_0, 0x180016}}},
	[RANDO_LOC_SPECTACLE_ROCK] = {1, {{SLOT_0, 0x180140}}},
	[RANDO_LOC_SPIRAL_CAVE] = {1, {{SLOT_0, 0xE9BF}}},
	[RANDO_LOC_MIMIC_CAVE] = {1, {{SLOT_0, 0xE9C5}}},
	[RANDO_LOC_PARADOX_CAVE_LOWER_FAR_LEFT] = {1, {{SLOT_0, 0xEB2A}}},
	[RANDO_LOC_PARADOX_CAVE_LOWER_LEFT] = {1, {{SLOT_0, 0xEB2D}}},
	[RANDO_LOC_PARADOX_CAVE_LOWER_RIGHT] = {1, {{SLOT_0, 0xEB30}}},
	[RANDO_LOC_PARADOX_CAVE_LOWER_FAR_RIGHT] = {1, {{SLOT_0, 0xEB33}}},
	[RANDO_LOC_PARADOX_CAVE_LOWER_MIDDLE] = {1, {{SLOT_0, 0xEB36}}},
	[RANDO_LOC_PARADOX_CAVE_UPPER_LEFT] = {1, {{SLOT_0, 0xEB39}}},
	[RANDO_LOC_PARADOX_CAVE_UPPER_RIGHT] = {1, {{SLOT_0, 0xEB3C}}},
	[RANDO_LOC_FLOATING_ISLAND] = {1, {{SLOT_0, 0x180141}}},
	[RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xE9E6}}},
	[RANDO_LOC_TOWER_OF_HERA_BASEMENT_CAGE] = {1, {{SLOT_0, 0x180162}}},
	[RANDO_LOC_TOWER_OF_HERA_MAP_CHEST] = {1, {{SLOT_0, 0xE9AD}}},
	[RANDO_LOC_TOWER_OF_HERA_COMPASS_CHEST] = {1, {{SLOT_0, 0xE9FB}}},
	[RANDO_LOC_TOWER_OF_HERA_BIG_CHEST] = {1, {{SLOT_0, 0xE9F8}}},
	[RANDO_LOC_TOWER_OF_HERA_BOSS] = {1, {{SLOT_0, 0x180152}}},
	[RANDO_LOC_TOWER_OF_HERA_PRIZE] = {6, {{SLOT_1, 0x120A5}, {SLOT_2, 0x53E78}, {SLOT_3, 0x53E79}, {SLOT_4, 0x18005A}, {SLOT_5, 0x180071}, {SLOT_6, 0xC706}}},
	[RANDO_LOC_CASTLE_TOWER_ROOM_03] = {1, {{SLOT_0, 0xEAB5}}},
	[RANDO_LOC_CASTLE_TOWER_DARK_MAZE] = {1, {{SLOT_0, 0xEAB2}}},
	[RANDO_LOC_AGAHNIM] = {0},
	[RANDO_LOC_SUPERBUNNY_CAVE_TOP] = {1, {{SLOT_0, 0xEA7C}}},
	[RANDO_LOC_SUPERBUNNY_CAVE_BOTTOM] = {1, {{SLOT_0, 0xEA7F}}},
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_RIGHT] = {1, {{SLOT_0, 0xEB51}}},
	[RANDO_LOC_HOOKSHOT_CAVE_TOP_LEFT] = {1, {{SLOT_0, 0xEB54}}},
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_LEFT] = {1, {{SLOT_0, 0xEB57}}},
	[RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_RIGHT] = {1, {{SLOT_0, 0xEB5A}}},
	[RANDO_LOC_SPIKE_CAVE] = {1, {{SLOT_0, 0xEA8B}}},
	[RANDO_LOC_CATFISH] = {1, {{SLOT_0, 0xEE185}}},
	[RANDO_LOC_PYRAMID] = {1, {{SLOT_0, 0x180147}}},
	[RANDO_LOC_PYRAMID_FAIRY_SWORD] = {1, {{SLOT_0, 0x180028}}},
	[RANDO_LOC_PYRAMID_FAIRY_BOW] = {1, {{SLOT_0, 0x34914}}},
	[RANDO_LOC_GANON] = {0},
	[RANDO_LOC_PYRAMID_FAIRY_LEFT] = {1, {{SLOT_0, 0xE980}}},
	[RANDO_LOC_PYRAMID_FAIRY_RIGHT] = {1, {{SLOT_0, 0xE983}}},
	[RANDO_LOC_BREWERY] = {1, {{SLOT_0, 0xE9EC}}},
	[RANDO_LOC_C_SHAPED_HOUSE] = {1, {{SLOT_0, 0xE9EF}}},
	[RANDO_LOC_CHEST_GAME] = {1, {{SLOT_0, 0xEDA8}}},
	[RANDO_LOC_HAMMER_PEGS] = {1, {{SLOT_0, 0x180006}}},
	[RANDO_LOC_BUMPER_CAVE] = {1, {{SLOT_0, 0x180146}}},
	[RANDO_LOC_BLACKSMITH] = {1, {{SLOT_0, 0x18002A}}},
	[RANDO_LOC_PURPLE_CHEST] = {1, {{SLOT_0, 0x33D68}}},
	[RANDO_LOC_HYPE_CAVE_TOP] = {1, {{SLOT_0, 0xEB1E}}},
	[RANDO_LOC_HYPE_CAVE_MIDDLE_RIGHT] = {1, {{SLOT_0, 0xEB21}}},
	[RANDO_LOC_HYPE_CAVE_MIDDLE_LEFT] = {1, {{SLOT_0, 0xEB24}}},
	[RANDO_LOC_HYPE_CAVE_BOTTOM] = {1, {{SLOT_0, 0xEB27}}},
	[RANDO_LOC_STUMPY] = {1, {{SLOT_0, 0x330C7}}},
	[RANDO_LOC_HYPE_CAVE_NPC] = {1, {{SLOT_0, 0x180011}}},
	[RANDO_LOC_DIGGING_GAME] = {1, {{SLOT_0, 0x180148}}},
	[RANDO_LOC_MIRE_SHED_LEFT] = {1, {{SLOT_0, 0xEA73}}},
	[RANDO_LOC_MIRE_SHED_RIGHT] = {1, {{SLOT_0, 0xEA76}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM] = {1, {{SLOT_0, 0xEA5B}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xEA37}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_LEDGE] = {1, {{SLOT_0, 0xEA3A}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_BRIDGE] = {1, {{SLOT_0, 0xEA3D}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_STALFOS_BASEMENT] = {1, {{SLOT_0, 0xEA49}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_MAP_CHEST] = {1, {{SLOT_0, 0xEA52}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_BIG_CHEST] = {1, {{SLOT_0, 0xEA40}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_COMPASS_CHEST] = {1, {{SLOT_0, 0xEA43}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY] = {1, {{SLOT_0, 0xEA46}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_LEFT] = {1, {{SLOT_0, 0xEA4C}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_RIGHT] = {1, {{SLOT_0, 0xEA4F}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_TOP] = {1, {{SLOT_0, 0xEA55}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_BOTTOM] = {1, {{SLOT_0, 0xEA58}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_BOSS] = {1, {{SLOT_0, 0x180153}}},
	[RANDO_LOC_PALACE_OF_DARKNESS_PRIZE] = {6, {{SLOT_1, 0x120A1}, {SLOT_2, 0x53E7C}, {SLOT_3, 0x53E7D}, {SLOT_4, 0x180056}, {SLOT_5, 0x180073}, {SLOT_6, 0xC702}}},
	[RANDO_LOC_SWAMP_PALACE_ENTRANCE] = {1, {{SLOT_0, 0xEA9D}}},
	[RANDO_LOC_SWAMP_PALACE_BIG_CHEST] = {1, {{SLOT_0, 0xE989}}},
	[RANDO_LOC_SWAMP_PALACE_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xEAA6}}},
	[RANDO_LOC_SWAMP_PALACE_MAP_CHEST] = {1, {{SLOT_0, 0xE986}}},
	[RANDO_LOC_SWAMP_PALACE_WEST_CHEST] = {1, {{SLOT_0, 0xEAA3}}},
	[RANDO_LOC_SWAMP_PALACE_COMPASS_CHEST] = {1, {{SLOT_0, 0xEAA0}}},
	[RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_LEFT] = {1, {{SLOT_0, 0xEAA9}}},
	[RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_RIGHT] = {1, {{SLOT_0, 0xEAAC}}},
	[RANDO_LOC_SWAMP_PALACE_WATERFALL_ROOM] = {1, {{SLOT_0, 0xEAAF}}},
	[RANDO_LOC_SWAMP_PALACE_BOSS] = {1, {{SLOT_0, 0x180154}}},
	[RANDO_LOC_SWAMP_PALACE_PRIZE] = {6, {{SLOT_1, 0x120A0}, {SLOT_2, 0x53E88}, {SLOT_3, 0x53E89}, {SLOT_4, 0x180055}, {SLOT_5, 0x180079}, {SLOT_6, 0xC701}}},
	[RANDO_LOC_SKULL_WOODS_BIG_CHEST] = {1, {{SLOT_0, 0xE998}}},
	[RANDO_LOC_SKULL_WOODS_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xE99E}}},
	[RANDO_LOC_SKULL_WOODS_COMPASS_CHEST] = {1, {{SLOT_0, 0xE992}}},
	[RANDO_LOC_SKULL_WOODS_MAP_CHEST] = {1, {{SLOT_0, 0xE99B}}},
	[RANDO_LOC_SKULL_WOODS_BRIDGE_ROOM] = {1, {{SLOT_0, 0xE9FE}}},
	[RANDO_LOC_SKULL_WOODS_POT_PRISON] = {1, {{SLOT_0, 0xE9A1}}},
	[RANDO_LOC_SKULL_WOODS_PINBALL_ROOM] = {1, {{SLOT_0, 0xE9C8}}},
	[RANDO_LOC_SKULL_WOODS_BOSS] = {1, {{SLOT_0, 0x180155}}},
	[RANDO_LOC_SKULL_WOODS_PRIZE] = {6, {{SLOT_1, 0x120A3}, {SLOT_2, 0x53E7E}, {SLOT_3, 0x53E7F}, {SLOT_4, 0x180058}, {SLOT_5, 0x180074}, {SLOT_6, 0xC704}}},
	[RANDO_LOC_THIEVES_TOWN_ATTIC] = {1, {{SLOT_0, 0xEA0D}}},
	[RANDO_LOC_THIEVES_TOWN_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xEA04}}},
	[RANDO_LOC_THIEVES_TOWN_MAP_CHEST] = {1, {{SLOT_0, 0xEA01}}},
	[RANDO_LOC_THIEVES_TOWN_COMPASS_CHEST] = {1, {{SLOT_0, 0xEA07}}},
	[RANDO_LOC_THIEVES_TOWN_AMBUSH_CHEST] = {1, {{SLOT_0, 0xEA0A}}},
	[RANDO_LOC_THIEVES_TOWN_BIG_CHEST] = {1, {{SLOT_0, 0xEA10}}},
	[RANDO_LOC_THIEVES_TOWN_BLINDS_CELL] = {1, {{SLOT_0, 0xEA13}}},
	[RANDO_LOC_THIEVES_TOWN_BOSS] = {1, {{SLOT_0, 0x180156}}},
	[RANDO_LOC_THIEVES_TOWN_PRIZE] = {6, {{SLOT_1, 0x120A6}, {SLOT_2, 0x53E82}, {SLOT_3, 0x53E83}, {SLOT_4, 0x18005B}, {SLOT_5, 0x180076}, {SLOT_6, 0xC707}}},
	[RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xE9A4}}},
	[RANDO_LOC_ICE_PALACE_COMPASS_CHEST] = {1, {{SLOT_0, 0xE9D4}}},
	[RANDO_LOC_ICE_PALACE_MAP_CHEST] = {1, {{SLOT_0, 0xE9DD}}},
	[RANDO_LOC_ICE_PALACE_SPIKE_ROOM] = {1, {{SLOT_0, 0xE9E0}}},
	[RANDO_LOC_ICE_PALACE_FREEZOR_CHEST] = {1, {{SLOT_0, 0xE995}}},
	[RANDO_LOC_ICE_PALACE_ICED_T_ROOM] = {1, {{SLOT_0, 0xE9E3}}},
	[RANDO_LOC_ICE_PALACE_BIG_CHEST] = {1, {{SLOT_0, 0xE9AA}}},
	[RANDO_LOC_ICE_PALACE_BOSS] = {1, {{SLOT_0, 0x180157}}},
	[RANDO_LOC_ICE_PALACE_PRIZE] = {6, {{SLOT_1, 0x120A4}, {SLOT_2, 0x53E86}, {SLOT_3, 0x53E87}, {SLOT_4, 0x180059}, {SLOT_5, 0x180078}, {SLOT_6, 0xC705}}},
	[RANDO_LOC_MISERY_MIRE_BIG_CHEST] = {1, {{SLOT_0, 0xEA67}}},
	[RANDO_LOC_MISERY_MIRE_MAIN_LOBBY] = {1, {{SLOT_0, 0xEA5E}}},
	[RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xEA6D}}},
	[RANDO_LOC_MISERY_MIRE_COMPASS_CHEST] = {1, {{SLOT_0, 0xEA64}}},
	[RANDO_LOC_MISERY_MIRE_BRIDGE_CHEST] = {1, {{SLOT_0, 0xEA61}}},
	[RANDO_LOC_MISERY_MIRE_MAP_CHEST] = {1, {{SLOT_0, 0xEA6A}}},
	[RANDO_LOC_MISERY_MIRE_SPIKE_CHEST] = {1, {{SLOT_0, 0xE9DA}}},
	[RANDO_LOC_MISERY_MIRE_BOSS] = {1, {{SLOT_0, 0x180158}}},
	[RANDO_LOC_MISERY_MIRE_PRIZE] = {6, {{SLOT_1, 0x120A2}, {SLOT_2, 0x53E84}, {SLOT_3, 0x53E85}, {SLOT_4, 0x180057}, {SLOT_5, 0x180077}, {SLOT_6, 0xC703}}},
	[RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS] = {1, {{SLOT_0, 0xEA16}}},
	[RANDO_LOC_TURTLE_ROCK_COMPASS_CHEST] = {1, {{SLOT_0, 0xEA22}}},
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_LEFT] = {1, {{SLOT_0, 0xEA1C}}},
	[RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_RIGHT] = {1, {{SLOT_0, 0xEA1F}}},
	[RANDO_LOC_TURTLE_ROCK_BIG_CHEST] = {1, {{SLOT_0, 0xEA19}}},
	[RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xEA25}}},
	[RANDO_LOC_TURTLE_ROCK_CRYSTAROLLER_ROOM] = {1, {{SLOT_0, 0xEA34}}},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_LEFT] = {1, {{SLOT_0, 0xEA31}}},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_RIGHT] = {1, {{SLOT_0, 0xEA2E}}},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT] = {1, {{SLOT_0, 0xEA2B}}},
	[RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_RIGHT] = {1, {{SLOT_0, 0xEA28}}},
	[RANDO_LOC_TURTLE_ROCK_BOSS] = {1, {{SLOT_0, 0x180159}}},
	[RANDO_LOC_TURTLE_ROCK_PRIZE] = {6, {{SLOT_1, 0x120A7}, {SLOT_2, 0x53E80}, {SLOT_3, 0x53E81}, {SLOT_4, 0x18005C}, {SLOT_5, 0x180075}, {SLOT_6, 0xC708}}},
	[RANDO_LOC_GANONS_TOWER_BOBS_TORCH] = {1, {{SLOT_0, 0x180161}}},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT] = {1, {{SLOT_0, 0xEAB8}}},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_RIGHT] = {1, {{SLOT_0, 0xEABB}}},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_LEFT] = {1, {{SLOT_0, 0xEABE}}},
	[RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_RIGHT] = {1, {{SLOT_0, 0xEAC1}}},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT] = {1, {{SLOT_0, 0xEAC4}}},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT] = {1, {{SLOT_0, 0xEAC7}}},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT] = {1, {{SLOT_0, 0xEACA}}},
	[RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT] = {1, {{SLOT_0, 0xEACD}}},
	[RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM] = {1, {{SLOT_0, 0xEAD0}}},
	[RANDO_LOC_GANONS_TOWER_MAP_CHEST] = {1, {{SLOT_0, 0xEAD3}}},
	[RANDO_LOC_GANONS_TOWER_BIG_CHEST] = {1, {{SLOT_0, 0xEAD6}}},
	[RANDO_LOC_GANONS_TOWER_HOPE_ROOM_LEFT] = {1, {{SLOT_0, 0xEAD9}}},
	[RANDO_LOC_GANONS_TOWER_HOPE_ROOM_RIGHT] = {1, {{SLOT_0, 0xEADC}}},
	[RANDO_LOC_GANONS_TOWER_BOBS_CHEST] = {1, {{SLOT_0, 0xEADF}}},
	[RANDO_LOC_GANONS_TOWER_TILE_ROOM] = {1, {{SLOT_0, 0xEAE2}}},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT] = {1, {{SLOT_0, 0xEAE5}}},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_RIGHT] = {1, {{SLOT_0, 0xEAE8}}},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_LEFT] = {1, {{SLOT_0, 0xEAEB}}},
	[RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT] = {1, {{SLOT_0, 0xEAEE}}},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_CHEST] = {1, {{SLOT_0, 0xEAF1}}},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_LEFT] = {1, {{SLOT_0, 0xEAF4}}},
	[RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_RIGHT] = {1, {{SLOT_0, 0xEAF7}}},
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_LEFT] = {1, {{SLOT_0, 0xEAFD}}},
	[RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_RIGHT] = {1, {{SLOT_0, 0xEB00}}},
	[RANDO_LOC_GANONS_TOWER_PRE_MOLDORM_CHEST] = {1, {{SLOT_0, 0xEB03}}},
	[RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST] = {1, {{SLOT_0, 0xEB06}}},
	[RANDO_LOC_AGAHNIM_2] = {0},
	[RANDO_LOC_TURTLE_ROCK_MEDALLION] = {4, {{SLOT_1, 0x180023}, {SLOT_T0, 0x5020}, {SLOT_T1, 0x50FF}, {SLOT_T2, 0x51DE}}},
	[RANDO_LOC_MISERY_MIRE_MEDALLION] = {4, {{SLOT_1, 0x180022}, {SLOT_M0, 0x4FF2}, {SLOT_M1, 0x50D1}, {SLOT_M2, 0x51B0}}},
	[RANDO_LOC_WATERFALL_BOTTLE] = {1, {{SLOT_0, 0x348FF}}},
	[RANDO_LOC_PYRAMID_BOTTLE] = {1, {{SLOT_0, 0x3493B}}},
	[RANDO_LOC_BOMB_MERCHANT] = {0},
};

#define MUSIC_MAX 8

typedef struct MusicAddresses {
	uint8_t count;
	uint32_t address[MUSIC_MAX];
} MusicAddresses;

static const MusicAddresses music_addresses[RANDO_REGION_COUNT] = {
	[RANDO_REGION_EASTERN_PALACE] = {1, {0x1559A}},
	[RANDO_REGION_DESERT_PALACE] = {4, {0x1559B, 0x1559C, 0x1559D, 0x1559E}},
	[RANDO_REGION_TOWER_OF_HERA] = {3, {0x155C5, 0x1107A, 0x10B8C}},
	[RANDO_REGION_PALACE_OF_DARKNESS] = {1, {0x155B8}},
	[RANDO_REGION_SWAMP_PALACE] = {1, {0x155B7}},
	[RANDO_REGION_SKULL_WOODS] = {8, {0x155BA, 0x155BB, 0x155BC, 0x155BD, 0x15608, 0x15609, 0x1560A, 0x1560B}},
	[RANDO_REGION_THIEVES_TOWN] = {1, {0x155C6}},
	[RANDO_REGION_ICE_PALACE] = {1, {0x155BF}},
	[RANDO_REGION_MISERY_MIRE] = {1, {0x155B9}},
	[RANDO_REGION_TURTLE_ROCK] = {4, {0x155C7, 0x155A7, 0x155AA, 0x155AB}},
	[RANDO_REGION_GANONS_TOWER] = {1, {0x155C9}},
};

#define HERA_BASEMENT_KEY_FANFARE 0x4E3BB

#define BLACKSMITH_SWORDS_OUT_OF_POOL 0x3355C

static const char METHOD[] = "writeItem";

static const ItemBytes alternate_bow = {0x0001, {0x65}};

static RandoItemId item_written(const RandoWorld *world, RandoLocationId location)
{
	RandoItemId item = rando_world_item(world, location);

	return item == RANDO_ITEM_NONE ? RANDO_ITEM_Nothing : item;
}

static bool region_item(RandoRegionId region, RandoItemId item)
{
	const RandoRegionInfo *info = rando_region_info(region);
	int i;

	for (i = 0; i < info->nitems; i++)
		if (info->items[i] == item)
			return true;
	return false;
}

static bool escape_exception(RandoLocationId location, RandoItemId item, RandoItemId h2)
{
	return (location == RANDO_LOC_SECRET_PASSAGE || location == RANDO_LOC_LINKS_UNCLE) && item == h2;
}

static RandoItemId substituted(const RandoConfig *config, RandoRegionId region, RandoLocationId location,
			       RandoItemId item)
{
	RandoItemClass item_class = rando_item_info(item)->item_class;

	if (config->rom_vanilla_keys && item_class == RANDO_ITEM_CLASS_KEY && region_item(region, item) &&
	    !escape_exception(location, item, RANDO_ITEM_KeyH2))
		item = RANDO_ITEM_Key;
	if (config->rom_vanilla_big_keys && item_class == RANDO_ITEM_CLASS_BIG_KEY && region_item(region, item))
		item = RANDO_ITEM_BigKey;
	if (config->rom_vanilla_maps && item_class == RANDO_ITEM_CLASS_MAP && region_item(region, item) &&
	    !escape_exception(location, item, RANDO_ITEM_MapH2))
		item = RANDO_ITEM_Map;
	if (config->rom_vanilla_compasses && item_class == RANDO_ITEM_CLASS_COMPASS && region_item(region, item))
		item = RANDO_ITEM_Compass;
	if (config->rom_generic_keys && item_class == RANDO_ITEM_CLASS_KEY)
		item = RANDO_ITEM_KeyGK;
	return item;
}

static uint8_t drawn_music(const RandoPrizeMusic *prize_music, RandoLocationId location)
{
	int i;

	for (i = 0; i < prize_music->count; i++)
		if (prize_music->locations[i] == location)
			return prize_music->music[i];
	assert(!"prize location without drawn music");
	return 0x16;
}

static void write_music(RomOut *out, const RandoGen *gen, RandoRegionId region, RandoLocationId location)
{
	const RandoWorld *world = &gen->world;
	const MusicAddresses *music = &music_addresses[region];
	uint8_t byte;
	int i;

	if (music->count == 0)
		return;
	if (world->config.rom_map_on_pickup)
		byte = drawn_music(&gen->post.prize_music, location);
	else if (rando_item_info(rando_world_item(world, location))->item_class == RANDO_ITEM_CLASS_PENDANT)
		byte = 0x11;
	else
		byte = 0x16;
	for (i = 0; i < music->count; i++)
		rando_rom_emit(out, music->address[i], &byte, 1, METHOD);
}

static void write_location(RomOut *out, const RandoGen *gen, RandoRegionId region, RandoLocationId location)
{
	const RandoWorld *world = &gen->world;
	const LocationAddresses *addresses = &location_addresses[location];
	RandoItemId item = item_written(world, location);
	const ItemBytes *bytes;
	bool prize = rando_location_is_a(rando_location_info(location)->location_class, RANDO_LOCATION_CLASS_PRIZE);
	int i;

	/* VT's Prize::setItem(Nothing) throws before any write. */
	assert(!prize || rando_world_has_item(world, location, RANDO_ITEM_NONE));
	item = substituted(&world->config, region, location, item);
	if (rando_item_info(item)->item_class == RANDO_ITEM_CLASS_ALIAS)
		item = rando_item_info(item)->alias_target;
	bytes = location == gen->post.texts.alternate_bow ? &alternate_bow : &item_bytes[item];
	for (i = 0; i < addresses->count; i++) {
		uint32_t address = addresses->at[i].address;
		Slot slot = (Slot)addresses->at[i].slot;

		if (location == RANDO_LOC_BLACKSMITH && !world->config.region_swords_in_pool)
			address = BLACKSMITH_SWORDS_OUT_OF_POOL;
		if (bytes->present & (1u << slot))
			rando_rom_emit(out, address, &bytes->bytes[slot], 1, METHOD);
	}
	if (prize)
		write_music(out, gen, region, location);
	if (rando_location_is_a(rando_location_info(location)->location_class, RANDO_LOCATION_CLASS_STANDING_HERA_BASEMENT)) {
		RandoItemId own = rando_world_item(world, location);
		uint8_t fanfare = own == RANDO_ITEM_Key || own == RANDO_ITEM_KeyP3 ? 0xE4 : 0xEB;

		rando_rom_emit(out, HERA_BASEMENT_KEY_FANFARE, &fanfare, 1, METHOD);
	}
}

uint8_t rando_rom_item_byte(RandoItemId item)
{
	assert(item < RANDO_ITEM_COUNT);
	if (rando_item_info(item)->item_class == RANDO_ITEM_CLASS_ALIAS)
		item = rando_item_info(item)->alias_target;
	assert(item_bytes[item].present & (1u << SLOT_0));
	return item_bytes[item].bytes[SLOT_0];
}

void rando_rom_write_items(RomOut *out, const RandoGen *gen)
{
	const RandoWorld *world = &gen->world;
	int r, i, pass;

	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		int n = rando_layout_region_location_count(world->variant, (RandoRegionId)r);

		for (pass = 0; pass < 2; pass++)
			for (i = 0; i < n; i++) {
				RandoLocationId location = rando_layout_region_location(world->variant, (RandoRegionId)r, i);
				bool empty = !rando_world_has_item(world, location, RANDO_ITEM_NONE);

				if (empty == (pass == 1))
					write_location(out, gen, (RandoRegionId)r, location);
			}
	}
}
