#include "rando_config.h"

#include <string.h>

void rando_config_init(RandoConfig *config)
{
	int i;

	memset(config, 0, sizeof *config);
	config->mode_state = RANDO_STATE_UNSET;
	config->mode_weapons = RANDO_WEAPONS_UNSET;
	config->item_placement = RANDO_PLACEMENT_UNSET;
	config->logic = RANDO_LOGIC_NO_GLITCHES;
	config->goal = RANDO_GOAL_GANON;
	config->accessibility = RANDO_ACCESSIBILITY_UNSET;
	config->dungeon_items = RANDO_DUNGEON_ITEMS_STANDARD;
	config->enemizer_enemy_health = RANDO_ENEMY_HEALTH_DEFAULT;
	config->item_pool = RANDO_ITEM_POOL_NORMAL;
	config->item_functionality = RANDO_ITEM_FUNCTIONALITY_NORMAL;
	config->crystals_ganon = 7;
	config->crystals_tower = 7;
	config->item_goal_required = RANDO_CONFIG_NULL;
	config->item_require_lamp = 1;
	config->item_overflow_count_shield = 3;
	config->item_overflow_replacement_shield = RANDO_ITEM_TwentyRupees2;
	config->rom_bottle_fill_magic = 0x80;
	config->region_boss_normal_location = true;
	config->region_swords_in_pool = true;
	config->region_boss_hearts_in_pool = true;
	config->region_bosses_have_item = RANDO_ITEM_NONE;
	config->rom_catchable_fairies = true;
	for (i = 0; i < RANDO_ITEM_COUNT; i++)
		config->item_count[i] = RANDO_CONFIG_NULL;
}
