#ifndef RANDO_CONFIG_H
#define RANDO_CONFIG_H

#include "rando_item_ids.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum RandoItemPlacement {
	RANDO_PLACEMENT_UNSET,
	RANDO_PLACEMENT_BASIC,
	RANDO_PLACEMENT_ADVANCED,
} RandoItemPlacement;

typedef enum RandoWeapons {
	RANDO_WEAPONS_UNSET,
	RANDO_WEAPONS_RANDOMIZED,
	RANDO_WEAPONS_ASSURED,
	RANDO_WEAPONS_VANILLA,
	RANDO_WEAPONS_SWORDLESS,
} RandoWeapons;

typedef enum RandoState {
	RANDO_STATE_UNSET,
	RANDO_STATE_STANDARD,
	RANDO_STATE_OPEN,
	RANDO_STATE_INVERTED,
	RANDO_STATE_RETRO,
} RandoState;

typedef enum RandoLogic {
	RANDO_LOGIC_NO_GLITCHES,
	RANDO_LOGIC_OVERWORLD_GLITCHES,
	RANDO_LOGIC_HYBRID_MAJOR_GLITCHES,
	RANDO_LOGIC_MAJOR_GLITCHES,
	RANDO_LOGIC_NO_LOGIC,
} RandoLogic;

typedef enum RandoGoal {
	RANDO_GOAL_GANON,
	RANDO_GOAL_FAST_GANON,
	RANDO_GOAL_DUNGEONS,
	RANDO_GOAL_PEDESTAL,
	RANDO_GOAL_TRIFORCE_HUNT,
	RANDO_GOAL_GANONHUNT,
	RANDO_GOAL_COMPLETIONIST,
} RandoGoal;

typedef enum RandoAccessibility {
	RANDO_ACCESSIBILITY_UNSET,
	RANDO_ACCESSIBILITY_ITEMS,
	RANDO_ACCESSIBILITY_LOCATIONS,
	RANDO_ACCESSIBILITY_NONE,
} RandoAccessibility;

typedef enum RandoDungeonItems {
	RANDO_DUNGEON_ITEMS_STANDARD,
	RANDO_DUNGEON_ITEMS_MC,
	RANDO_DUNGEON_ITEMS_MCS,
	RANDO_DUNGEON_ITEMS_FULL,
} RandoDungeonItems;

typedef enum RandoEnemyHealth {
	RANDO_ENEMY_HEALTH_DEFAULT,
	RANDO_ENEMY_HEALTH_EASY,
	RANDO_ENEMY_HEALTH_HARD,
	RANDO_ENEMY_HEALTH_EXPERT,
} RandoEnemyHealth;

typedef enum RandoItemPool {
	RANDO_ITEM_POOL_NORMAL,
	RANDO_ITEM_POOL_HARD,
	RANDO_ITEM_POOL_EXPERT,
	RANDO_ITEM_POOL_CROWD_CONTROL,
} RandoItemPool;

typedef enum RandoItemFunctionality {
	RANDO_ITEM_FUNCTIONALITY_NORMAL,
	RANDO_ITEM_FUNCTIONALITY_HARD,
	RANDO_ITEM_FUNCTIONALITY_EXPERT,
} RandoItemFunctionality;

#define RANDO_CONFIG_NULL (-1)

typedef struct RandoConfig {
	RandoState mode_state;
	RandoWeapons mode_weapons;
	RandoItemPlacement item_placement;
	RandoLogic logic;
	RandoGoal goal;
	RandoAccessibility accessibility;
	RandoDungeonItems dungeon_items;
	RandoEnemyHealth enemizer_enemy_health;
	RandoItemPool item_pool;
	RandoItemFunctionality item_functionality;
	int crystals_ganon;
	int crystals_tower;
	int item_goal_required;
	int item_require_lamp;
	int item_overflow_count_shield;
	int item_overflow_count_bow;
	int item_overflow_count_sword;
	int item_overflow_count_armor;
	int item_overflow_count_boss_heart_container;
	int item_overflow_count_piece_of_heart;
	RandoItemId item_overflow_replacement_shield;
	int rom_bottle_fill_magic;
	int rom_bottle_fill_health;
	int rom_cape_magic_usage_normal;
	int rom_cape_magic_usage_half;
	int rom_cape_magic_usage_quarter;
	int rom_powdered_sprite_fairy_prize;
	int rom_stun_items;
	int rom_free_item_text;
	int rom_free_item_menu;
	int16_t item_count[RANDO_ITEM_COUNT];
	bool ignore_can_kill_escape_things;
	bool shops_hard_mode;
	bool region_wild_keys;
	bool region_wild_big_keys;
	bool region_wild_maps;
	bool region_wild_compasses;
	bool region_boss_normal_location;
	bool region_cant_take_damage;
	bool region_force_skull_woods_key;
	bool region_require_better_bow;
	bool region_require_better_sword;
	bool region_swords_in_pool;
	bool region_pyramid_bow_upgrade;
	bool region_boss_hearts_in_pool;
	RandoItemId region_bosses_have_item;
	bool region_take_anys;
	bool rom_catchable_fairies;
	bool rom_cane_of_byrna_invulnerability;
	bool rom_silvers_only_at_ganon;
	bool rom_no_farie_drops;
	bool rom_dungeon_count_pickup;
	bool rom_rupee_bow;
	bool rom_generic_keys;
	bool rom_map_on_pickup;
	bool rom_vanilla_keys;
	bool rom_vanilla_big_keys;
	bool rom_vanilla_maps;
	bool rom_vanilla_compasses;
	bool can_super_bunny;
	bool can_fake_flipper;
	bool can_super_speed;
	bool can_boots_clip;
	bool can_mirror_clip;
	bool can_water_walk;
	bool can_dungeon_revive;
	bool can_mirror_wrap;
	bool can_owyba;
	bool can_one_frame_clip_ow;
	bool can_one_frame_clip_uw;
	bool can_transition_wrapped;
	bool can_bunny_surf;
	bool can_bunny_revive;
	bool custom_prize_packs;
	bool spoil_hints;
} RandoConfig;

void rando_config_init(RandoConfig *config);

#endif
