#ifndef RANDO_WORLD_H
#define RANDO_WORLD_H

#include "rando_bosses.h"
#include "rando_config.h"
#include "rando_items.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum RandoVariant {
	RANDO_VARIANT_STANDARD,
	RANDO_VARIANT_OPEN,
	RANDO_VARIANT_INVERTED,
	RANDO_VARIANT_RETRO,
	RANDO_VARIANT_COUNT,
} RandoVariant;

/* VT's World order. */
typedef enum RandoRegionId {
	RANDO_REGION_NORTH_EAST_LIGHT_WORLD,
	RANDO_REGION_NORTH_WEST_LIGHT_WORLD,
	RANDO_REGION_SOUTH_LIGHT_WORLD,
	RANDO_REGION_ESCAPE,
	RANDO_REGION_EASTERN_PALACE,
	RANDO_REGION_DESERT_PALACE,
	RANDO_REGION_WEST_DEATH_MOUNTAIN,
	RANDO_REGION_EAST_DEATH_MOUNTAIN,
	RANDO_REGION_TOWER_OF_HERA,
	RANDO_REGION_HYRULE_CASTLE_TOWER,
	RANDO_REGION_EAST_DARK_WORLD_DEATH_MOUNTAIN,
	RANDO_REGION_WEST_DARK_WORLD_DEATH_MOUNTAIN,
	RANDO_REGION_NORTH_EAST_DARK_WORLD,
	RANDO_REGION_NORTH_WEST_DARK_WORLD,
	RANDO_REGION_SOUTH_DARK_WORLD,
	RANDO_REGION_MIRE,
	RANDO_REGION_PALACE_OF_DARKNESS,
	RANDO_REGION_SWAMP_PALACE,
	RANDO_REGION_SKULL_WOODS,
	RANDO_REGION_THIEVES_TOWN,
	RANDO_REGION_ICE_PALACE,
	RANDO_REGION_MISERY_MIRE,
	RANDO_REGION_TURTLE_ROCK,
	RANDO_REGION_GANONS_TOWER,
	RANDO_REGION_MEDALLIONS,
	RANDO_REGION_FOUNTAINS,
	RANDO_REGION_COUNT,
	RANDO_REGION_NONE = RANDO_REGION_COUNT,
} RandoRegionId;

/* Standard's World::getLocations() order, then the locations only Inverted has. */
typedef enum RandoLocationId {
	RANDO_LOC_SAHASRAHLAS_HUT_LEFT,
	RANDO_LOC_SAHASRAHLAS_HUT_MIDDLE,
	RANDO_LOC_SAHASRAHLAS_HUT_RIGHT,
	RANDO_LOC_SAHASRAHLA,
	RANDO_LOC_KING_ZORA,
	RANDO_LOC_POTION_SHOP,
	RANDO_LOC_ZORAS_LEDGE,
	RANDO_LOC_WATERFALL_FAIRY_LEFT,
	RANDO_LOC_WATERFALL_FAIRY_RIGHT,
	RANDO_LOC_MASTER_SWORD_PEDESTAL,
	RANDO_LOC_KINGS_TOMB,
	RANDO_LOC_KAKARIKO_TAVERN,
	RANDO_LOC_CHICKEN_HOUSE,
	RANDO_LOC_KAKARIKO_WELL_TOP,
	RANDO_LOC_KAKARIKO_WELL_LEFT,
	RANDO_LOC_KAKARIKO_WELL_MIDDLE,
	RANDO_LOC_KAKARIKO_WELL_RIGHT,
	RANDO_LOC_KAKARIKO_WELL_BOTTOM,
	RANDO_LOC_BLINDS_HIDEOUT_TOP,
	RANDO_LOC_BLINDS_HIDEOUT_LEFT,
	RANDO_LOC_BLINDS_HIDEOUT_RIGHT,
	RANDO_LOC_BLINDS_HIDEOUT_FAR_LEFT,
	RANDO_LOC_BLINDS_HIDEOUT_FAR_RIGHT,
	RANDO_LOC_PEGASUS_ROCKS,
	RANDO_LOC_BOTTLE_MERCHANT,
	RANDO_LOC_MAGIC_BAT,
	RANDO_LOC_SICK_KID,
	RANDO_LOC_LOST_WOODS_HIDEOUT,
	RANDO_LOC_LUMBERJACK_TREE,
	RANDO_LOC_GRAVEYARD_LEDGE,
	RANDO_LOC_MUSHROOM,
	RANDO_LOC_FLOODGATE_CHEST,
	RANDO_LOC_LINKS_HOUSE,
	RANDO_LOC_AGINAHS_CAVE,
	RANDO_LOC_MINI_MOLDORM_CAVE_FAR_LEFT,
	RANDO_LOC_MINI_MOLDORM_CAVE_LEFT,
	RANDO_LOC_MINI_MOLDORM_CAVE_RIGHT,
	RANDO_LOC_MINI_MOLDORM_CAVE_FAR_RIGHT,
	RANDO_LOC_ICE_ROD_CAVE,
	RANDO_LOC_HOBO,
	RANDO_LOC_BOMBOS_TABLET,
	RANDO_LOC_CAVE_45,
	RANDO_LOC_CHECKERBOARD_CAVE,
	RANDO_LOC_MINI_MOLDORM_CAVE_NPC,
	RANDO_LOC_LIBRARY,
	RANDO_LOC_MAZE_RACE,
	RANDO_LOC_DESERT_LEDGE,
	RANDO_LOC_LAKE_HYLIA_ISLAND,
	RANDO_LOC_SUNKEN_TREASURE,
	RANDO_LOC_FLUTE_SPOT,
	RANDO_LOC_SANCTUARY,
	RANDO_LOC_SEWERS_SECRET_ROOM_LEFT,
	RANDO_LOC_SEWERS_SECRET_ROOM_MIDDLE,
	RANDO_LOC_SEWERS_SECRET_ROOM_RIGHT,
	RANDO_LOC_SEWERS_DARK_CROSS,
	RANDO_LOC_HYRULE_CASTLE_BOOMERANG_CHEST,
	RANDO_LOC_HYRULE_CASTLE_MAP_CHEST,
	RANDO_LOC_HYRULE_CASTLE_ZELDAS_CELL,
	RANDO_LOC_LINKS_UNCLE,
	RANDO_LOC_SECRET_PASSAGE,
	RANDO_LOC_ZELDA,
	RANDO_LOC_EASTERN_PALACE_COMPASS_CHEST,
	RANDO_LOC_EASTERN_PALACE_BIG_CHEST,
	RANDO_LOC_EASTERN_PALACE_CANNONBALL_CHEST,
	RANDO_LOC_EASTERN_PALACE_BIG_KEY_CHEST,
	RANDO_LOC_EASTERN_PALACE_MAP_CHEST,
	RANDO_LOC_EASTERN_PALACE_BOSS,
	RANDO_LOC_EASTERN_PALACE_PRIZE,
	RANDO_LOC_DESERT_PALACE_BIG_CHEST,
	RANDO_LOC_DESERT_PALACE_MAP_CHEST,
	RANDO_LOC_DESERT_PALACE_TORCH,
	RANDO_LOC_DESERT_PALACE_BIG_KEY_CHEST,
	RANDO_LOC_DESERT_PALACE_COMPASS_CHEST,
	RANDO_LOC_DESERT_PALACE_BOSS,
	RANDO_LOC_DESERT_PALACE_PRIZE,
	RANDO_LOC_OLD_MAN,
	RANDO_LOC_SPECTACLE_ROCK_CAVE,
	RANDO_LOC_ETHER_TABLET,
	RANDO_LOC_SPECTACLE_ROCK,
	RANDO_LOC_SPIRAL_CAVE,
	RANDO_LOC_MIMIC_CAVE,
	RANDO_LOC_PARADOX_CAVE_LOWER_FAR_LEFT,
	RANDO_LOC_PARADOX_CAVE_LOWER_LEFT,
	RANDO_LOC_PARADOX_CAVE_LOWER_RIGHT,
	RANDO_LOC_PARADOX_CAVE_LOWER_FAR_RIGHT,
	RANDO_LOC_PARADOX_CAVE_LOWER_MIDDLE,
	RANDO_LOC_PARADOX_CAVE_UPPER_LEFT,
	RANDO_LOC_PARADOX_CAVE_UPPER_RIGHT,
	RANDO_LOC_FLOATING_ISLAND,
	RANDO_LOC_TOWER_OF_HERA_BIG_KEY_CHEST,
	RANDO_LOC_TOWER_OF_HERA_BASEMENT_CAGE,
	RANDO_LOC_TOWER_OF_HERA_MAP_CHEST,
	RANDO_LOC_TOWER_OF_HERA_COMPASS_CHEST,
	RANDO_LOC_TOWER_OF_HERA_BIG_CHEST,
	RANDO_LOC_TOWER_OF_HERA_BOSS,
	RANDO_LOC_TOWER_OF_HERA_PRIZE,
	RANDO_LOC_CASTLE_TOWER_ROOM_03,
	RANDO_LOC_CASTLE_TOWER_DARK_MAZE,
	RANDO_LOC_AGAHNIM,
	RANDO_LOC_SUPERBUNNY_CAVE_TOP,
	RANDO_LOC_SUPERBUNNY_CAVE_BOTTOM,
	RANDO_LOC_HOOKSHOT_CAVE_TOP_RIGHT,
	RANDO_LOC_HOOKSHOT_CAVE_TOP_LEFT,
	RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_LEFT,
	RANDO_LOC_HOOKSHOT_CAVE_BOTTOM_RIGHT,
	RANDO_LOC_SPIKE_CAVE,
	RANDO_LOC_CATFISH,
	RANDO_LOC_PYRAMID,
	RANDO_LOC_PYRAMID_FAIRY_SWORD,
	RANDO_LOC_PYRAMID_FAIRY_BOW,
	RANDO_LOC_GANON,
	RANDO_LOC_PYRAMID_FAIRY_LEFT,
	RANDO_LOC_PYRAMID_FAIRY_RIGHT,
	RANDO_LOC_BREWERY,
	RANDO_LOC_C_SHAPED_HOUSE,
	RANDO_LOC_CHEST_GAME,
	RANDO_LOC_HAMMER_PEGS,
	RANDO_LOC_BUMPER_CAVE,
	RANDO_LOC_BLACKSMITH,
	RANDO_LOC_PURPLE_CHEST,
	RANDO_LOC_HYPE_CAVE_TOP,
	RANDO_LOC_HYPE_CAVE_MIDDLE_RIGHT,
	RANDO_LOC_HYPE_CAVE_MIDDLE_LEFT,
	RANDO_LOC_HYPE_CAVE_BOTTOM,
	RANDO_LOC_STUMPY,
	RANDO_LOC_HYPE_CAVE_NPC,
	RANDO_LOC_DIGGING_GAME,
	RANDO_LOC_MIRE_SHED_LEFT,
	RANDO_LOC_MIRE_SHED_RIGHT,
	RANDO_LOC_PALACE_OF_DARKNESS_SHOOTER_ROOM,
	RANDO_LOC_PALACE_OF_DARKNESS_BIG_KEY_CHEST,
	RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_LEDGE,
	RANDO_LOC_PALACE_OF_DARKNESS_THE_ARENA_BRIDGE,
	RANDO_LOC_PALACE_OF_DARKNESS_STALFOS_BASEMENT,
	RANDO_LOC_PALACE_OF_DARKNESS_MAP_CHEST,
	RANDO_LOC_PALACE_OF_DARKNESS_BIG_CHEST,
	RANDO_LOC_PALACE_OF_DARKNESS_COMPASS_CHEST,
	RANDO_LOC_PALACE_OF_DARKNESS_HARMLESS_HELLWAY,
	RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_LEFT,
	RANDO_LOC_PALACE_OF_DARKNESS_DARK_BASEMENT_RIGHT,
	RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_TOP,
	RANDO_LOC_PALACE_OF_DARKNESS_DARK_MAZE_BOTTOM,
	RANDO_LOC_PALACE_OF_DARKNESS_BOSS,
	RANDO_LOC_PALACE_OF_DARKNESS_PRIZE,
	RANDO_LOC_SWAMP_PALACE_ENTRANCE,
	RANDO_LOC_SWAMP_PALACE_BIG_CHEST,
	RANDO_LOC_SWAMP_PALACE_BIG_KEY_CHEST,
	RANDO_LOC_SWAMP_PALACE_MAP_CHEST,
	RANDO_LOC_SWAMP_PALACE_WEST_CHEST,
	RANDO_LOC_SWAMP_PALACE_COMPASS_CHEST,
	RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_LEFT,
	RANDO_LOC_SWAMP_PALACE_FLOODED_ROOM_RIGHT,
	RANDO_LOC_SWAMP_PALACE_WATERFALL_ROOM,
	RANDO_LOC_SWAMP_PALACE_BOSS,
	RANDO_LOC_SWAMP_PALACE_PRIZE,
	RANDO_LOC_SKULL_WOODS_BIG_CHEST,
	RANDO_LOC_SKULL_WOODS_BIG_KEY_CHEST,
	RANDO_LOC_SKULL_WOODS_COMPASS_CHEST,
	RANDO_LOC_SKULL_WOODS_MAP_CHEST,
	RANDO_LOC_SKULL_WOODS_BRIDGE_ROOM,
	RANDO_LOC_SKULL_WOODS_POT_PRISON,
	RANDO_LOC_SKULL_WOODS_PINBALL_ROOM,
	RANDO_LOC_SKULL_WOODS_BOSS,
	RANDO_LOC_SKULL_WOODS_PRIZE,
	RANDO_LOC_THIEVES_TOWN_ATTIC,
	RANDO_LOC_THIEVES_TOWN_BIG_KEY_CHEST,
	RANDO_LOC_THIEVES_TOWN_MAP_CHEST,
	RANDO_LOC_THIEVES_TOWN_COMPASS_CHEST,
	RANDO_LOC_THIEVES_TOWN_AMBUSH_CHEST,
	RANDO_LOC_THIEVES_TOWN_BIG_CHEST,
	RANDO_LOC_THIEVES_TOWN_BLINDS_CELL,
	RANDO_LOC_THIEVES_TOWN_BOSS,
	RANDO_LOC_THIEVES_TOWN_PRIZE,
	RANDO_LOC_ICE_PALACE_BIG_KEY_CHEST,
	RANDO_LOC_ICE_PALACE_COMPASS_CHEST,
	RANDO_LOC_ICE_PALACE_MAP_CHEST,
	RANDO_LOC_ICE_PALACE_SPIKE_ROOM,
	RANDO_LOC_ICE_PALACE_FREEZOR_CHEST,
	RANDO_LOC_ICE_PALACE_ICED_T_ROOM,
	RANDO_LOC_ICE_PALACE_BIG_CHEST,
	RANDO_LOC_ICE_PALACE_BOSS,
	RANDO_LOC_ICE_PALACE_PRIZE,
	RANDO_LOC_MISERY_MIRE_BIG_CHEST,
	RANDO_LOC_MISERY_MIRE_MAIN_LOBBY,
	RANDO_LOC_MISERY_MIRE_BIG_KEY_CHEST,
	RANDO_LOC_MISERY_MIRE_COMPASS_CHEST,
	RANDO_LOC_MISERY_MIRE_BRIDGE_CHEST,
	RANDO_LOC_MISERY_MIRE_MAP_CHEST,
	RANDO_LOC_MISERY_MIRE_SPIKE_CHEST,
	RANDO_LOC_MISERY_MIRE_BOSS,
	RANDO_LOC_MISERY_MIRE_PRIZE,
	RANDO_LOC_TURTLE_ROCK_CHAIN_CHOMPS,
	RANDO_LOC_TURTLE_ROCK_COMPASS_CHEST,
	RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_LEFT,
	RANDO_LOC_TURTLE_ROCK_ROLLER_ROOM_RIGHT,
	RANDO_LOC_TURTLE_ROCK_BIG_CHEST,
	RANDO_LOC_TURTLE_ROCK_BIG_KEY_CHEST,
	RANDO_LOC_TURTLE_ROCK_CRYSTAROLLER_ROOM,
	RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_LEFT,
	RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_BOTTOM_RIGHT,
	RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_LEFT,
	RANDO_LOC_TURTLE_ROCK_EYE_BRIDGE_TOP_RIGHT,
	RANDO_LOC_TURTLE_ROCK_BOSS,
	RANDO_LOC_TURTLE_ROCK_PRIZE,
	RANDO_LOC_GANONS_TOWER_BOBS_TORCH,
	RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_LEFT,
	RANDO_LOC_GANONS_TOWER_DMS_ROOM_TOP_RIGHT,
	RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_LEFT,
	RANDO_LOC_GANONS_TOWER_DMS_ROOM_BOTTOM_RIGHT,
	RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_LEFT,
	RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_TOP_RIGHT,
	RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_LEFT,
	RANDO_LOC_GANONS_TOWER_RANDOMIZER_ROOM_BOTTOM_RIGHT,
	RANDO_LOC_GANONS_TOWER_FIRESNAKE_ROOM,
	RANDO_LOC_GANONS_TOWER_MAP_CHEST,
	RANDO_LOC_GANONS_TOWER_BIG_CHEST,
	RANDO_LOC_GANONS_TOWER_HOPE_ROOM_LEFT,
	RANDO_LOC_GANONS_TOWER_HOPE_ROOM_RIGHT,
	RANDO_LOC_GANONS_TOWER_BOBS_CHEST,
	RANDO_LOC_GANONS_TOWER_TILE_ROOM,
	RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_LEFT,
	RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_TOP_RIGHT,
	RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_LEFT,
	RANDO_LOC_GANONS_TOWER_COMPASS_ROOM_BOTTOM_RIGHT,
	RANDO_LOC_GANONS_TOWER_BIG_KEY_CHEST,
	RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_LEFT,
	RANDO_LOC_GANONS_TOWER_BIG_KEY_ROOM_RIGHT,
	RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_LEFT,
	RANDO_LOC_GANONS_TOWER_MINI_HELMASAUR_ROOM_RIGHT,
	RANDO_LOC_GANONS_TOWER_PRE_MOLDORM_CHEST,
	RANDO_LOC_GANONS_TOWER_MOLDORM_CHEST,
	RANDO_LOC_AGAHNIM_2,
	RANDO_LOC_TURTLE_ROCK_MEDALLION,
	RANDO_LOC_MISERY_MIRE_MEDALLION,
	RANDO_LOC_WATERFALL_BOTTLE,
	RANDO_LOC_PYRAMID_BOTTLE,
	RANDO_LOC_BOMB_MERCHANT,
	RANDO_LOCATION_COUNT,
	RANDO_LOCATION_NONE = RANDO_LOCATION_COUNT,
} RandoLocationId;

/* VT's World::getShops() order. */
typedef enum RandoShopId {
	RANDO_SHOP_LONG_FAIRY_CAVE,
	RANDO_SHOP_LAKE_HYLIA_FAIRY,
	RANDO_SHOP_LIGHT_WORLD_KAKARIKO_SHOP,
	RANDO_SHOP_FORTUNE_TELLER_LIGHT,
	RANDO_SHOP_BUSH_COVERED_HOUSE,
	RANDO_SHOP_LOST_WOODS_GAMBLE,
	RANDO_SHOP_LUMBERJACK_HOUSE,
	RANDO_SHOP_SNITCH_LADY_EAST,
	RANDO_SHOP_SNITCH_LADY_WEST,
	RANDO_SHOP_BOMB_HUT,
	RANDO_SHOP_LIGHT_WORLD_LAKE_HYLIA_SHOP,
	RANDO_SHOP_CAPACITY_UPGRADE,
	RANDO_SHOP_20_RUPEE_CAVE,
	RANDO_SHOP_50_RUPEE_CAVE,
	RANDO_SHOP_BONK_FAIRY_LIGHT,
	RANDO_SHOP_DESERT_FAIRY,
	RANDO_SHOP_GOOD_BEE_CAVE,
	RANDO_SHOP_LAKE_HYLIA_FORTUNE_TELLER,
	RANDO_SHOP_LIGHT_HYPE_FAIRY,
	RANDO_SHOP_KAKARIKO_GAMBLE_GAME,
	RANDO_SHOP_LIGHT_WORLD_DEATH_MOUNTAIN_SHOP,
	RANDO_SHOP_HOOKSHOT_FAIRY,
	RANDO_SHOP_DARK_WORLD_DEATH_MOUNTAIN_SHOP,
	RANDO_SHOP_DARK_DEATH_MOUNTAIN_FAIRY,
	RANDO_SHOP_DARK_WORLD_POTION_SHOP,
	RANDO_SHOP_DARK_LAKE_HYLIA_FAIRY,
	RANDO_SHOP_EAST_DARK_WORLD_HINT,
	RANDO_SHOP_PALACE_OF_DARKNESS_HINT,
	RANDO_SHOP_DARK_WORLD_FOREST_SHOP,
	RANDO_SHOP_DARK_WORLD_LUMBERJACK_HUT_SHOP,
	RANDO_SHOP_DARK_WORLD_OUTCASTS_SHOP,
	RANDO_SHOP_DARK_SANCTUARY_HINT,
	RANDO_SHOP_FORTUNE_TELLER_DARK,
	RANDO_SHOP_DARK_WORLD_LAKE_HYLIA_SHOP,
	RANDO_SHOP_ARCHERY_GAME,
	RANDO_SHOP_BONK_FAIRY_DARK,
	RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_FAIRY,
	RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_HINT,
	RANDO_SHOP_DARK_LAKE_HYLIA_LEDGE_SPIKE_CAVE,
	RANDO_SHOP_DARK_DESERT_FAIRY,
	RANDO_SHOP_DARK_DESERT_HINT,
	RANDO_SHOP_COUNT,
	RANDO_SHOP_NONE = RANDO_SHOP_COUNT,
} RandoShopId;

/* One value per concrete VT location class, plus the bases Location (shop slots) and Prize. */
typedef enum RandoLocationClass {
	RANDO_LOCATION_CLASS_LOCATION,
	RANDO_LOCATION_CLASS_CHEST,
	RANDO_LOCATION_CLASS_BIG_CHEST,
	RANDO_LOCATION_CLASS_DASH,
	RANDO_LOCATION_CLASS_DIG,
	RANDO_LOCATION_CLASS_DIG_HAUNTED_GROVE,
	RANDO_LOCATION_CLASS_DROP,
	RANDO_LOCATION_CLASS_DROP_BOMBOS,
	RANDO_LOCATION_CLASS_DROP_ETHER,
	RANDO_LOCATION_CLASS_FOUNTAIN,
	RANDO_LOCATION_CLASS_MEDALLION,
	RANDO_LOCATION_CLASS_NPC,
	RANDO_LOCATION_CLASS_NPC_BUG_CATCHING_KID,
	RANDO_LOCATION_CLASS_NPC_UNCLE,
	RANDO_LOCATION_CLASS_NPC_WITCH,
	RANDO_LOCATION_CLASS_NPC_ZORA,
	RANDO_LOCATION_CLASS_PEDESTAL,
	RANDO_LOCATION_CLASS_PRIZE,
	RANDO_LOCATION_CLASS_PRIZE_CRYSTAL,
	RANDO_LOCATION_CLASS_PRIZE_EVENT,
	RANDO_LOCATION_CLASS_PRIZE_PENDANT,
	RANDO_LOCATION_CLASS_STANDING,
	RANDO_LOCATION_CLASS_STANDING_HERA_BASEMENT,
	RANDO_LOCATION_CLASS_TRADE,
	RANDO_LOCATION_CLASS_COUNT,
} RandoLocationClass;

typedef enum RandoBossSlot {
	RANDO_BOSS_SLOT_BOSS,
	RANDO_BOSS_SLOT_TOP,
	RANDO_BOSS_SLOT_MIDDLE,
	RANDO_BOSS_SLOT_BOTTOM,
	RANDO_BOSS_SLOT_COUNT,
} RandoBossSlot;

#define RANDO_SHOP_SLOTS 3

/* VT's Sprite table order: a drop's ROM sprite byte is 0xD7 + its id. */
typedef enum RandoDrop {
	RANDO_DROP_NONE,
	RANDO_DROP_HEART,
	RANDO_DROP_RUPEE_GREEN,
	RANDO_DROP_RUPEE_BLUE,
	RANDO_DROP_RUPEE_RED,
	RANDO_DROP_BOMB_REFILL_1,
	RANDO_DROP_BOMB_REFILL_4,
	RANDO_DROP_BOMB_REFILL_8,
	RANDO_DROP_MAGIC_REFILL_SMALL,
	RANDO_DROP_MAGIC_REFILL_FULL,
	RANDO_DROP_ARROW_REFILL_5,
	RANDO_DROP_ARROW_REFILL_10,
	RANDO_DROP_FAIRY,
	RANDO_DROP_COUNT,
} RandoDrop;

/* VT's World::getPrizePacks() order. */
typedef enum RandoPrizePack {
	RANDO_PRIZE_PACK_0,
	RANDO_PRIZE_PACK_1,
	RANDO_PRIZE_PACK_2,
	RANDO_PRIZE_PACK_3,
	RANDO_PRIZE_PACK_4,
	RANDO_PRIZE_PACK_5,
	RANDO_PRIZE_PACK_6,
	RANDO_PRIZE_PACK_PULL,
	RANDO_PRIZE_PACK_CRAB,
	RANDO_PRIZE_PACK_STUN,
	RANDO_PRIZE_PACK_FISH,
	RANDO_PRIZE_PACK_COUNT,
} RandoPrizePack;

#define RANDO_PRIZE_PACK_SLOTS 8

#define RANDO_PRE_COLLECTED_MAX 16

typedef struct RandoLocationInfo {
	const char *name;
	RandoLocationClass location_class;
	RandoItemId constructor_item;
} RandoLocationInfo;

typedef struct RandoRegionInfo {
	const char *key;
	const char *spoiler_name;
	const uint8_t *items;
	uint8_t nitems;
	RandoShopId first_shop;
	uint8_t nshops;
	uint8_t nboss_slots;
	RandoBossId default_bosses[RANDO_BOSS_SLOT_COUNT];
} RandoRegionInfo;

typedef enum RandoShopKind {
	RANDO_SHOP_KIND_SHOP,
	RANDO_SHOP_KIND_TAKE_ANY,
	RANDO_SHOP_KIND_UPGRADE,
} RandoShopKind;

typedef struct RandoShopInfo {
	const char *name;
	RandoRegionId region;
	uint8_t nslots;
	RandoItemId default_items[RANDO_SHOP_SLOTS];
	RandoShopKind kind;
	uint16_t default_prices[RANDO_SHOP_SLOTS];
} RandoShopInfo;

/* Ids, never pointers: reinitialized by value with rando_world_init. Item and boss ids are stored
 * as uint8_t. */
typedef struct RandoWorld {
	RandoConfig config;
	RandoVariant variant;
	uint8_t location_items[RANDO_LOCATION_COUNT];
	uint8_t shop_items[RANDO_SHOP_COUNT][RANDO_SHOP_SLOTS];
	bool shop_active[RANDO_SHOP_COUNT];
	uint16_t shop_prices[RANDO_SHOP_COUNT][RANDO_SHOP_SLOTS];
	uint8_t bosses[RANDO_REGION_COUNT][RANDO_BOSS_SLOT_COUNT];
	uint8_t prize_pack_drops[RANDO_PRIZE_PACK_COUNT][RANDO_PRIZE_PACK_SLOTS];
	RandoItems pre_collected;
	uint8_t pre_collected_order[RANDO_PRE_COLLECTED_MAX];
	uint8_t npre_collected;
	bool collected[RANDO_LOCATION_COUNT];
	bool shop_collected[RANDO_SHOP_COUNT][RANDO_SHOP_SLOTS];
	/* VT computes the collectable set once and never again, even after reset_collected. */
	bool collectable_computed;
	bool collectable[RANDO_LOCATION_COUNT];
	uint8_t collectable_shop_items[RANDO_SHOP_COUNT][RANDO_SHOP_SLOTS];
} RandoWorld;

/* Each asserts its id is below the count. */
const RandoLocationInfo *rando_location_info(RandoLocationId id);
const RandoRegionInfo *rando_region_info(RandoRegionId id);
const RandoShopInfo *rando_shop_info(RandoShopId id);
/* The VT region class the variant uses in that slot, e.g. "Region\Open\HyruleCastleEscape". */
const char *rando_region_vt_class(RandoVariant variant, RandoRegionId id);

/* The variant's world layout: its locations in VT's World::getLocations() order, and per region its
 * locations in VT order and its prize location (RANDO_LOCATION_NONE when it has none). A location's
 * region is RANDO_REGION_NONE when the variant's world does not have it. A prize location may lie in
 * another region: Inverted North East Dark World keeps VT's removed Ganon. Each asserts its arguments
 * are in range. */
int rando_layout_location_count(RandoVariant variant);
RandoLocationId rando_layout_location(RandoVariant variant, int i);
RandoRegionId rando_layout_location_region(RandoVariant variant, RandoLocationId location);
int rando_layout_region_location_count(RandoVariant variant, RandoRegionId region);
RandoLocationId rando_layout_region_location(RandoVariant variant, RandoRegionId region, int i);
RandoLocationId rando_layout_region_prize(RandoVariant variant, RandoRegionId region);
#ifdef RANDO_FAULTS
/* The location's index in its region's list; 0 where the variant's world lacks it. */
int rando_layout_location_slot(RandoVariant variant, RandoLocationId location);
/* The slots the region's rule table declares, -1 when it has none; the location id it means by slot. */
int rando_region_rule_slot_count(RandoVariant variant, RandoRegionId region);
RandoLocationId rando_region_rule_slot_location(RandoVariant variant, RandoRegionId region, int slot);
#endif

/* Each returns its NONE sentinel when the name is unknown; regions are found by world key. */
RandoLocationId rando_location_find(const char *name);
RandoRegionId rando_region_find(const char *key);
RandoShopId rando_shop_find(const char *name);

/* Asserts the pack is below the count. */
int rando_prize_pack_slots(RandoPrizePack pack);

/* VT's instanceof between location classes. */
bool rando_location_is_a(RandoLocationClass location_class, RandoLocationClass base);

/* World::factory(variant, config) with an empty pre-collected collection. */
void rando_world_init(RandoWorld *world, RandoVariant variant, const RandoConfig *config);

/* World::copy: everything by value, with no collected state and the collectable cache reset. */
void rando_world_copy(RandoWorld *dst, const RandoWorld *src);

/* World::addPreCollectedItem: appends to the multiset and to the insertion order. */
void rando_world_add_pre_collected(RandoWorld *world, RandoItemId item);
int rando_world_pre_collected_count(const RandoWorld *world);
/* Asserts i is below the count. */
RandoItemId rando_world_pre_collected_item(const RandoWorld *world, int i);

RandoItemId rando_world_item(const RandoWorld *world, RandoLocationId location);
RandoBossId rando_world_boss(const RandoWorld *world, RandoRegionId region, RandoBossSlot slot);
/* Ignores nboss_slots, which describes the constructor defaults only. */
void rando_world_set_boss(RandoWorld *world, RandoRegionId region, RandoBossSlot slot, RandoBossId boss);
RandoItemId rando_world_shop_item(const RandoWorld *world, RandoShopId shop, int slot);
bool rando_world_shop_active(const RandoWorld *world, RandoShopId shop);
void rando_world_shop_set_active(RandoWorld *world, RandoShopId shop, bool active);
/* Asserts the slot holds an item. */
int rando_world_shop_price(const RandoWorld *world, RandoShopId shop, int slot);
void rando_world_shop_clear(RandoWorld *world, RandoShopId shop);
/* Replaces what the slot held; the item may not be RANDO_ITEM_NONE. */
void rando_world_shop_add(RandoWorld *world, RandoShopId shop, int slot, RandoItemId item, int price);
/* Both assert the slot is below the pack's slot count. */
RandoDrop rando_world_drop(const RandoWorld *world, RandoPrizePack pack, int slot);
/* The drop may not be RANDO_DROP_NONE. */
void rando_world_set_drop(RandoWorld *world, RandoPrizePack pack, int slot, RandoDrop drop);

/* False where VT's setItem throws (a refused item class); the location is then unchanged, and the
 * caller fails the attempt as VT's retry loop does. RANDO_ITEM_NONE empties the location. */
bool rando_world_set_item(RandoWorld *world, RandoLocationId location, RandoItemId item);
/* RANDO_ITEM_NONE asks whether the location holds anything. */
bool rando_world_has_item(const RandoWorld *world, RandoLocationId location, RandoItemId item);

bool rando_world_can_access(const RandoWorld *world, RandoLocationId location, const RandoItems *items);
/* VT's canAccess($items, $locations): the location belongs to world, and its rules read the items
 * VT reads from $locations in locations. */
bool rando_world_can_access_with(const RandoWorld *world, const RandoWorld *locations, RandoLocationId location,
				 const RandoItems *items);
/* Any slot of the shop: its region's canEnter and the shop's own requirement. */
bool rando_world_can_access_shop(const RandoWorld *world, RandoShopId shop, const RandoItems *items);
/* Asserts the location's class accepts the item; the location's item is unchanged on return. */
bool rando_world_can_fill(RandoWorld *world, RandoLocationId location, RandoItemId item,
			  const RandoItems *items, bool check_access);
/* Keeps the item only when can_fill succeeds. */
bool rando_world_fill(RandoWorld *world, RandoLocationId location, RandoItemId item, const RandoItems *items);

bool rando_world_region_can_enter(const RandoWorld *world, RandoRegionId region, const RandoItems *items);
bool rando_world_region_can_complete(const RandoWorld *world, RandoRegionId region, const RandoItems *items);
bool rando_world_region_can_fill(const RandoWorld *world, RandoRegionId region, RandoItemId item);

/* World::collectItems: start may be NULL; out may not alias start. */
void rando_world_collect_items(RandoWorld *world, const RandoItems *start, RandoItems *out);
/* World's win_condition closure on items as given, with no collection. */
bool rando_world_win_condition(const RandoWorld *world, const RandoItems *items);
/* World::collectOtherItems: found (which may not alias collected) gets only the items found; the
 * pre-collected items are not merged. The fill and is_winnable must use this one: collect_items
 * would count the pre-collected items, already in their assumed items, twice. */
void rando_world_collect_other_items(RandoWorld *world, const RandoItems *collected, RandoItems *found);
/* World::checkWinCondition: collects from collected (may be NULL) without resetting the locations
 * already collected, so a location collected before adds nothing. */
bool rando_world_check_win_condition(RandoWorld *world, const RandoItems *collected);
void rando_world_reset_collected(RandoWorld *world);
bool rando_world_collected(const RandoWorld *world, RandoLocationId location);
bool rando_world_shop_collected(const RandoWorld *world, RandoShopId shop, int slot);
/* getCollectedLocationsCount: collected locations plus collected shop slots. */
int rando_world_collected_count(const RandoWorld *world);
/* Resets the collected state, then merges collectOtherItems into items until the collected count
 * stops changing; items grows in place. */
void rando_world_collect_until_stable(RandoWorld *world, RandoItems *items);
/* WorldCollection::isWinnable for this world alone; changes the collected state only. */
bool rando_world_is_winnable(RandoWorld *world);

#endif
