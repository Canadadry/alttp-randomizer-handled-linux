#ifndef LADX_LOGIC_INTERNAL_H
#define LADX_LOGIC_INTERNAL_H

#include "ladx_logic.h"
#include "ladx_logic_builder.h"
#include "ladx_settings.h"

#include <stdbool.h>

/* options.<key> == "<name>"; an unknown name is never equal. */
bool ladx_setting_is(const LadxSettings *settings, LadxSettingId id, const char *name);
/* A boolean setting's truth. */
bool ladx_setting_on(const LadxSettings *settings, LadxSettingId id);

/* items[0].forced_item = item: the location's first spot. */
void ladx_logic_force_first_item(LadxLogic *logic, int location, int item);
/* Location.add(<Kind>(room, arg)), as ladx_logic_add_item_info; returns the
 * location, as add returns self. */
int ladx_logic_add_to(LadxLogic *logic, int location, LadxSpotKind kind, int room, int arg);
/* Location.connect(others, req, back=back) on a tuple: each target's both
 * edges in turn, req and back the same nodes for all. */
void ladx_logic_connect_all(LadxLogic *logic, int from, int n, const int *targets, LadxReq req, LadxReq back);

/* requirements.py RequirementsSettings.enemy_requirements' keys, in its order;
 * the last three exist only under the settings that add them. */
typedef enum LadxEnemy {
	LADX_ENEMY_HARDHAT_BEETLE,
	LADX_ENEMY_MINI_MOLDORM,
	LADX_ENEMY_KEESE,
	LADX_ENEMY_STALFOS_AGGRESSIVE,
	LADX_ENEMY_STALFOS_EVASIVE,
	LADX_ENEMY_SPIKED_BEETLE,
	LADX_ENEMY_THREE_OF_A_KIND,
	LADX_ENEMY_MASKED_MIMIC_GORIYA,
	LADX_ENEMY_BOO_BUDDY,
	LADX_ENEMY_MOBLIN,
	LADX_ENEMY_MOBLIN_SWORD,
	LADX_ENEMY_POLS_VOICE,
	LADX_ENEMY_ZOL,
	LADX_ENEMY_HIDING_ZOL,
	LADX_ENEMY_GEL,
	LADX_ENEMY_BOUNCING_BOMBITE,
	LADX_ENEMY_TIMER_BOMBITE,
	LADX_ENEMY_PAIRODD,
	LADX_ENEMY_IRON_MASK,
	LADX_ENEMY_MASTER_STALFOS,
	LADX_ENEMY_STAR,
	LADX_ENEMY_WIZROBE,
	LADX_ENEMY_LIKE_LIKE,
	LADX_ENEMY_VIRE,
	LADX_ENEMY_SNAKE,
	LADX_ENEMY_GIBDO,
	LADX_ENEMY_MIMIC,
	LADX_ENEMY_COLOR_GHOUL_GREEN,
	LADX_ENEMY_COLOR_GHOUL_RED,
	LADX_ENEMY_COLOR_GHOUL_BLUE,
	LADX_ENEMY_COLOR_SHELL_GREEN,
	LADX_ENEMY_COLOR_SHELL_RED,
	LADX_ENEMY_COLOR_SHELL_BLUE,
	LADX_ENEMY_URCHIN,
	LADX_ENEMY_GIANT_GOPONGA_FLOWER,
	LADX_ENEMY_GOPONGA_FLOWER,
	LADX_ENEMY_MAD_BOMBER,
	LADX_ENEMY_CROW,
	LADX_ENEMY_KNIGHT,
	LADX_ENEMY_DESERT_LANMOLA,
	LADX_ENEMY_TURTLE_ROCK_HEAD,
	LADX_ENEMY_SHADOW_BLOB,
	LADX_ENEMY_SHADOW_AGAHNIM,
	LADX_ENEMY_SHADOW_MOLDORM,
	LADX_ENEMY_SHADOW_GANON,
	LADX_ENEMY_SHADOW_LANMOLA,
	LADX_ENEMY_SHADOW_DETHL,
	LADX_ENEMY_ARMOS_STATUE,
	LADX_ENEMY_BOMBER,
	LADX_ENEMY_BUZZ_BLOB,
	LADX_ENEMY_HIDING_GHINI,
	LADX_ENEMY_GHINI,
	LADX_ENEMY_GIANT_GHINI,
	LADX_ENEMY_LEEVER,
	LADX_ENEMY_DOG,
	LADX_ENEMY_CUCCO,
	LADX_ENEMY_OCTOROK,
	LADX_ENEMY_PINCER,
	LADX_ENEMY_FISH,
	LADX_ENEMY_POKEY,
	LADX_ENEMY_SAND_CRAB,
	LADX_ENEMY_BUSH_CRAWLER,
	LADX_ENEMY_ROCK_CRAWLER,
	LADX_ENEMY_TEKTITE,
	LADX_ENEMY_WINGED_OCTOROK,
	LADX_ENEMY_ZORA,
	LADX_ENEMY_ANTI_KIRBY,
	LADX_ENEMY_BLOOPER,
	LADX_ENEMY_FLYING_HOPPER_BOMBS,
	LADX_ENEMY_HOPPER,
	LADX_ENEMY_CHEEP_CHEEP_HORIZONTAL,
	LADX_ENEMY_CHEEP_CHEEP_VERTICAL,
	LADX_ENEMY_CHEEP_CHEEP_JUMPING,
	LADX_ENEMY_GOOMBA,
	LADX_ENEMY_PEAHAT,
	LADX_ENEMY_PIRANHA_PLANT,
	LADX_ENEMY_SPARK_COUNTER_CLOCKWISE,
	LADX_ENEMY_SPARK_CLOCKWISE,
	LADX_ENEMY_ANTI_FAIRY,
	LADX_ENEMY_WATER_TEKTITE,
	LADX_ENEMY_TRACTOR_DEVICE,
	LADX_ENEMY_TRACTOR_DEVICE_REVERSE,
	LADX_ENEMY_ZOMBIE,
	LADX_ENEMY_WINGED_BONE_PUTTER,
	LADX_ENEMY_BONE_PUTTER,
	LADX_ENEMY_FINAL_NIGHTMARE,
	LADX_ENEMY_COUNT
} LadxEnemy;

/* The string keys boss_requirements and miniboss_requirements share. */
typedef enum LadxNightmare {
	LADX_NIGHTMARE_GEL,
	LADX_NIGHTMARE_AGAHNIM,
	LADX_NIGHTMARE_MOLDORM,
	LADX_NIGHTMARE_GANON,
	LADX_NIGHTMARE_LANMOLA,
	LADX_NIGHTMARE_COUNT
} LadxNightmare;

/* requirements.py RequirementsSettings, its attributes by name. */
typedef struct LadxRequirementsSettings {
	LadxReq bush, pit_bush, attack, attack_hookshot, hit_switch, hit_switch_color;
	LadxReq attack_hookshot_powder, attack_no_bomb, attack_hookshot_no_bomb, attack_no_boomerang;
	LadxReq attack_skeleton, attack_gibdos, stun_wizrobe, stun_mask_mimic, rear_attack;
	LadxReq rear_attack_range, fire, sword_beam, push_hardhat, shuffled_magnifier;
	LadxReq throw_pot, throw_enemy, midair_turn, running_turn, tight_jump, shield_bump, sword_poke;
	LadxReq super_jump, super_jump_boots, super_jump_feather, super_jump_sword, super_jump_rooster;
	LadxReq shaq_jump, super_bump, super_poke, boots_superhop, boots_superbump, boots_superpoke;
	LadxReq boots_roosterhop, jesus_jump, jesus_rooster, jesus_buffer, jesus_buffer_hookshot;
	LadxReq jesus_buffer_itemless, damage_boost_special, damage_boost, diagonal_walk, corner_walk;
	LadxReq sideways_block_push, wall_clip, pit_buffer_itemless, pit_buffer, pit_buffer_boots;
	LadxReq boots_jump, boots_bonk, boots_bonk_pit, boots_bonk_lava, boots_dash_2d, boots_bonk_2d_hell;
	LadxReq boots_bonk_2d_spikepit, bounce_2d_spikepit, bracelet_bounce_2d_spikepit;
	LadxReq toadstool_bounce_2d_spikepit, hookshot_spam_pit, hookshot_clip, hookshot_clip_block;
	LadxReq hookshot_over_pit, hookshot_clip_wall, hookshot_jump, hookshot_wrap, bookshot;
	LadxReq bomb_trigger, text_clip, zoomerang, zoomerang_shovel, lava_swim, lava_swim_sword;
	LadxReq boss_requirements[9];
	LadxReq boss_nightmare[LADX_NIGHTMARE_COUNT];
	LadxReq miniboss_requirements[LADX_MINIBOSS_COUNT];
	LadxReq miniboss_nightmare[LADX_NIGHTMARE_COUNT];
	LadxReq enemy_requirements[LADX_ENEMY_COUNT];
	bool enemy_present[LADX_ENEMY_COUNT];
} LadxRequirementsSettings;

/* RequirementsSettings(options), its nodes in logic's arena. */
void ladx_requirements_settings(LadxRequirementsSettings *r, LadxLogic *logic, const LadxSettings *options);

/* What a DungeonN(options, world_setup, r) reads, and the logic it builds in. */
typedef struct LadxDungeonContext {
	LadxLogic *logic;
	const LadxSettings *options;
	const LadxWorldSetup *world_setup;
	const LadxRequirementsSettings *r;
} LadxDungeonContext;

/* r.enemy_requirements[key]; a key the settings did not add breaks the
 * logic (Python's KeyError). */
LadxReq ladx_dungeon_enemy(const LadxDungeonContext *c, LadxEnemy enemy);

/* DungeonN(...).entrance: its handle, -1 once the logic is broken. */
int ladx_dungeon1(const LadxDungeonContext *c);
int ladx_dungeon2(const LadxDungeonContext *c);
int ladx_dungeon3(const LadxDungeonContext *c);
int ladx_dungeon4(const LadxDungeonContext *c);
int ladx_dungeon5(const LadxDungeonContext *c);
int ladx_dungeon6(const LadxDungeonContext *c);
int ladx_dungeon7(const LadxDungeonContext *c);
int ladx_dungeon8(const LadxDungeonContext *c);
/* DungeonColor(...).entrance. */
int ladx_dungeon_color(const LadxDungeonContext *c);

/* entranceInfo.py ENTRANCE_INFO's keys, in its order. */
#define LADX_ENTRANCE_COUNT 104

/* overworld.py EntranceExterior; Python's None is location -1 and a NONE
 * requirement, "UNSET" a false *_set flag. */
typedef struct LadxEntranceExterior {
	bool added;
	int location;
	LadxReq requirement;
	bool enter_set, exit_set;
	LadxReq one_way_enter, one_way_exit;
} LadxEntranceExterior;

/* overworld.py World: entrances[2 k] is ENTRANCE_INFO key k, [2 k + 1]
 * its "<key>:inside"; locations are creation ids. */
typedef struct LadxWorld {
	LadxEntranceExterior entrance[2 * LADX_ENTRANCE_COUNT];
	int start, egg, nightmare, windfish;
} LadxWorld;

/* ENTRANCE_INFO's key k; NULL out of range. */
const char *ladx_entrance_name(int k);
/* World(options, world_setup, r). */
void ladx_overworld(const LadxDungeonContext *c, LadxWorld *w);
/* World.updateIndoorLocation(name, location): "d0" to "d8". */
void ladx_world_update_indoor(LadxLogic *logic, LadxWorld *w, const char *name, int location);

#endif
