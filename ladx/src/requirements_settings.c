#include "logic_internal.h"

#define A arena
#define I(name) ladx_req_item(LADX_ITEM_##name)
#define AND(...) LADX_AND(A, __VA_ARGS__)
#define OR(...) LADX_OR(A, __VA_ARGS__)
#define COUNT(item, n) ladx_req_count(A, LADX_ITEM_##item, (n))
#define B(value) ladx_req_bool(value)
#define IS(key, name) ladx_setting_is(options, LADX_SETTING_##key, name)
#define ENEMY(name) r->enemy_requirements[LADX_ENEMY_##name]
#define MINIBOSS(name) r->miniboss_requirements[LADX_MINIBOSS_##name]

static void enemies(LadxRequirementsSettings *r, LadxReqArena *arena)
{
	ENEMY(HARDHAT_BEETLE) = I(BOMB);
	ENEMY(MINI_MOLDORM) = r->attack_hookshot_powder;
	ENEMY(KEESE) = r->attack_hookshot_powder;
	ENEMY(STALFOS_AGGRESSIVE) = r->attack_skeleton;
	ENEMY(STALFOS_EVASIVE) = r->attack_skeleton;
	ENEMY(SPIKED_BEETLE) = AND(I(SHIELD), r->attack_hookshot_powder);
	ENEMY(THREE_OF_A_KIND) = OR(r->attack_hookshot_no_bomb, I(SHIELD));
	ENEMY(MASKED_MIMIC_GORIYA) = OR(r->rear_attack, r->rear_attack_range);
	ENEMY(BOO_BUDDY) = OR(I(BOW), I(MAGIC_ROD));
	ENEMY(MOBLIN) = r->attack_hookshot_powder;
	ENEMY(MOBLIN_SWORD) = r->attack_hookshot_powder;
	ENEMY(POLS_VOICE) = OR(I(BOMB), I(MAGIC_ROD), AND(I(OCARINA), I(SONG1)));
	ENEMY(ZOL) = r->attack_hookshot_powder;
	ENEMY(HIDING_ZOL) = r->attack_hookshot_powder;
	ENEMY(GEL) = r->attack_hookshot_powder;
	ENEMY(BOUNCING_BOMBITE) = r->attack_hookshot_powder;
	ENEMY(TIMER_BOMBITE) = OR(I(SWORD), I(BOMB), I(SHIELD));
	ENEMY(PAIRODD) = r->attack_no_boomerang;
	ENEMY(IRON_MASK) = r->attack_hookshot_powder;
	ENEMY(MASTER_STALFOS) = AND(I(SWORD), I(BOMB));
	ENEMY(STAR) = r->attack_hookshot_powder;
	ENEMY(WIZROBE) = OR(I(BOMB), I(MAGIC_ROD));
	ENEMY(LIKE_LIKE) = r->attack_hookshot_powder;
	ENEMY(VIRE) = r->attack_hookshot_no_bomb;
	ENEMY(SNAKE) = r->attack_hookshot_powder;
	ENEMY(GIBDO) = r->attack_gibdos;
	ENEMY(MIMIC) = I(SWORD);
	ENEMY(COLOR_GHOUL_GREEN) = r->attack_hookshot_powder;
	ENEMY(COLOR_GHOUL_RED) = r->attack_hookshot_powder;
	ENEMY(COLOR_GHOUL_BLUE) = r->attack_hookshot_powder;
	ENEMY(COLOR_SHELL_GREEN) = AND(r->attack_hookshot, I(POWER_BRACELET));
	ENEMY(COLOR_SHELL_RED) = AND(r->attack_hookshot, I(POWER_BRACELET));
	ENEMY(COLOR_SHELL_BLUE) = AND(r->attack_hookshot, I(POWER_BRACELET));
	ENEMY(URCHIN) = r->attack_hookshot;
	ENEMY(GIANT_GOPONGA_FLOWER) = OR(I(BOWWOW), I(HOOKSHOT), I(MAGIC_ROD), I(BOOMERANG));
	ENEMY(GOPONGA_FLOWER) = OR(I(BOWWOW), I(HOOKSHOT), I(MAGIC_ROD), I(BOOMERANG));
	ENEMY(MAD_BOMBER) = OR(I(SWORD), I(BOW), I(MAGIC_ROD));
	ENEMY(CROW) = r->attack_hookshot_no_bomb;
	ENEMY(KNIGHT) = r->attack_hookshot;
	ENEMY(DESERT_LANMOLA) = r->attack_hookshot_no_bomb;
	ENEMY(TURTLE_ROCK_HEAD) = AND(I(OCARINA), I(SONG3), I(SWORD));
	ENEMY(SHADOW_BLOB) = I(MAGIC_POWDER);
	ENEMY(SHADOW_AGAHNIM) = OR(I(SWORD), I(SHOVEL));
	ENEMY(SHADOW_MOLDORM) = I(SWORD);
	ENEMY(SHADOW_GANON) = I(SWORD);
	ENEMY(SHADOW_LANMOLA) = r->attack_hookshot_no_bomb;
	ENEMY(SHADOW_DETHL) = OR(I(BOW), I(BOOMERANG));

	ENEMY(ARMOS_STATUE) = OR(I(BOMB), I(BOW), I(MAGIC_ROD));
	ENEMY(BOMBER) = OR(I(SWORD), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(MAGIC_POWDER));
	ENEMY(BUZZ_BLOB) = OR(I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG));
	ENEMY(HIDING_GHINI) = r->attack_hookshot_no_bomb;
	ENEMY(GHINI) = r->attack_hookshot_no_bomb;
	ENEMY(GIANT_GHINI) = r->attack_hookshot_no_bomb;
	ENEMY(LEEVER) = r->attack_hookshot_powder;
	ENEMY(DOG) = r->fire;
	ENEMY(CUCCO) = r->fire;
	ENEMY(OCTOROK) = r->attack_hookshot_powder;
	ENEMY(PINCER) = r->attack_hookshot;
	ENEMY(FISH) = r->attack_hookshot_powder;
	ENEMY(POKEY) = r->attack_hookshot_powder;
	ENEMY(SAND_CRAB) = r->attack_hookshot_powder;
	ENEMY(BUSH_CRAWLER) = AND(r->bush, r->attack_hookshot_powder);
	ENEMY(ROCK_CRAWLER) = AND(I(POWER_BRACELET), r->attack_hookshot_powder);
	ENEMY(TEKTITE) = r->attack_hookshot_powder;
	ENEMY(WINGED_OCTOROK) = r->attack_hookshot_powder;
	ENEMY(ZORA) = OR(I(SWORD), I(BOOMERANG));
	ENEMY(ANTI_KIRBY) = OR(I(BOMB), I(MAGIC_ROD), I(BOOMERANG));
	ENEMY(BLOOPER) = r->attack_hookshot_powder;
	ENEMY(FLYING_HOPPER_BOMBS) = OR(I(SWORD), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(MAGIC_POWDER));
	ENEMY(HOPPER) = OR(I(SWORD), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(MAGIC_POWDER));
	ENEMY(CHEEP_CHEEP_HORIZONTAL) = r->attack_hookshot_powder;
	ENEMY(CHEEP_CHEEP_VERTICAL) = r->attack_hookshot_powder;
	ENEMY(CHEEP_CHEEP_JUMPING) = r->attack_hookshot_powder;
	ENEMY(GOOMBA) = OR(I(FEATHER), r->attack_hookshot_powder);
	ENEMY(PEAHAT) = r->attack_hookshot_powder;
	ENEMY(PIRANHA_PLANT) = r->attack_hookshot_powder;
	ENEMY(SPARK_COUNTER_CLOCKWISE) = I(BOOMERANG);
	ENEMY(SPARK_CLOCKWISE) = I(BOOMERANG);
	ENEMY(ANTI_FAIRY) = r->fire;
	ENEMY(WATER_TEKTITE) = r->attack_hookshot_powder;
	ENEMY(TRACTOR_DEVICE) = I(SWORD);
	ENEMY(TRACTOR_DEVICE_REVERSE) = I(SWORD);
	ENEMY(ZOMBIE) = r->attack_hookshot_powder;
}

void ladx_requirements_settings(LadxRequirementsSettings *r, LadxLogic *logic, const LadxSettings *options)
{
	LadxReqArena *arena = ladx_logic_arena_mut(logic);
	bool hard = IS(LOGIC, "hard") || IS(LOGIC, "glitched") || IS(LOGIC, "hell");
	int i;

	r->bush = OR(I(SWORD), I(MAGIC_POWDER), I(MAGIC_ROD), I(POWER_BRACELET), I(BOOMERANG));
	r->pit_bush = OR(I(SWORD), I(MAGIC_POWDER), I(MAGIC_ROD), I(BOOMERANG), I(BOMB));
	r->attack = OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG));
	r->attack_hookshot = OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(HOOKSHOT));
	r->hit_switch = OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(HOOKSHOT));
	r->hit_switch_color = OR(I(SWORD), I(SHIELD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(HOOKSHOT));
	r->attack_hookshot_powder =
		OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(HOOKSHOT), I(MAGIC_POWDER));
	r->attack_no_bomb = OR(I(SWORD), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(HOOKSHOT));
	r->attack_hookshot_no_bomb = OR(I(SWORD), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(HOOKSHOT));
	r->attack_no_boomerang = OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(HOOKSHOT));
	r->attack_skeleton = OR(I(SWORD), I(BOMB), I(BOW), I(BOOMERANG), I(HOOKSHOT));
	r->attack_gibdos = OR(I(SWORD), I(BOMB), I(BOW), I(BOOMERANG), AND(I(MAGIC_ROD), I(HOOKSHOT)));
	r->stun_wizrobe = OR(I(BOOMERANG), I(MAGIC_POWDER), I(HOOKSHOT));
	r->stun_mask_mimic = OR(I(BOOMERANG), I(HOOKSHOT));
	r->rear_attack = OR(I(SWORD), I(BOMB));
	r->rear_attack_range = OR(I(MAGIC_ROD), I(BOW));
	r->fire = OR(I(MAGIC_POWDER), I(MAGIC_ROD));
	r->sword_beam = AND(COUNT(SWORD, 2), B(!IS(HPMODE, "5hit") || IS(HARDMODE, "ohko")));
	r->push_hardhat = OR(I(SHIELD), I(SWORD), I(HOOKSHOT), I(BOOMERANG));
	r->shuffled_magnifier = I(TRADING_ITEM_MAGNIFYING_GLASS);
	r->throw_pot = I(POWER_BRACELET);
	r->throw_enemy = I(POWER_BRACELET);
	r->midair_turn = OR(I(SWORD), I(BOW), I(MAGIC_ROD));
	r->running_turn = OR(I(BOW), I(MAGIC_ROD));
	r->tight_jump = I(FEATHER);
	r->shield_bump = I(SHIELD);
	r->sword_poke = I(SWORD);
	r->super_jump = AND(I(FEATHER), r->midair_turn);
	r->super_jump_boots = AND(I(PEGASUS_BOOTS), I(FEATHER), r->midair_turn);
	r->super_jump_feather = I(FEATHER);
	r->super_jump_sword = AND(I(FEATHER), I(SWORD));
	r->super_jump_rooster = AND(I(ROOSTER), r->midair_turn);
	r->shaq_jump = I(FEATHER);
	r->super_bump = AND(r->super_jump_feather, I(SHIELD));
	r->super_poke = AND(r->super_jump_feather, I(SWORD));
	r->boots_superhop = AND(I(PEGASUS_BOOTS), r->running_turn);
	r->boots_superbump = AND(r->boots_superhop, I(SHIELD));
	r->boots_superpoke = AND(r->boots_superhop, I(SWORD));
	r->boots_roosterhop = AND(I(PEGASUS_BOOTS), I(ROOSTER));
	r->jesus_jump = I(FEATHER);
	r->jesus_rooster = AND(I(ROOSTER), B(!IS(HARDMODE, "oracle")));
	r->jesus_buffer = I(PEGASUS_BOOTS);
	r->jesus_buffer_hookshot = I(HOOKSHOT);
	r->jesus_buffer_itemless = B(true);
	r->damage_boost_special = B(!IS(HPMODE, "5hit") && IS(HARDMODE, "none"));
	r->damage_boost = B(IS(BOWWOW, "normal") && !IS(HPMODE, "5hit") && IS(HARDMODE, "none"));
	r->diagonal_walk = B(true);
	r->corner_walk = B(true);
	r->sideways_block_push = B(true);
	r->wall_clip = B(true);
	r->pit_buffer_itemless = B(true);
	r->pit_buffer = I(FEATHER);
	r->pit_buffer_boots = OR(I(PEGASUS_BOOTS), I(FEATHER));
	r->boots_jump = AND(I(PEGASUS_BOOTS), I(FEATHER));
	r->boots_bonk = I(PEGASUS_BOOTS);
	r->boots_bonk_pit = I(PEGASUS_BOOTS);
	r->boots_bonk_lava = AND(I(PEGASUS_BOOTS), B(!IS(HPMODE, "5hit")));
	r->boots_dash_2d = I(PEGASUS_BOOTS);
	r->boots_bonk_2d_hell = I(PEGASUS_BOOTS);
	r->boots_bonk_2d_spikepit = AND(r->damage_boost_special, I(PEGASUS_BOOTS), I(MEDICINE2));
	r->bounce_2d_spikepit = r->damage_boost_special;
	r->bracelet_bounce_2d_spikepit = AND(r->damage_boost_special, I(POWER_BRACELET));
	r->toadstool_bounce_2d_spikepit = AND(r->damage_boost_special, I(TOADSTOOL2));
	r->hookshot_spam_pit = I(HOOKSHOT);
	r->hookshot_clip = AND(I(HOOKSHOT), B(!ladx_setting_on(options, LADX_SETTING_SUPERWEAPONS)));
	r->hookshot_clip_block = I(HOOKSHOT);
	r->hookshot_over_pit = I(HOOKSHOT);
	r->hookshot_clip_wall = I(HOOKSHOT);
	r->hookshot_jump = AND(I(HOOKSHOT), I(FEATHER));
	r->hookshot_wrap = I(HOOKSHOT);
	r->bookshot = AND(I(FEATHER), I(HOOKSHOT));
	r->bomb_trigger = I(BOMB);
	r->text_clip = B(false);
	r->zoomerang = AND(I(PEGASUS_BOOTS), I(FEATHER), I(BOOMERANG));
	r->zoomerang_shovel = AND(I(PEGASUS_BOOTS), I(FEATHER), I(BOOMERANG), I(SHOVEL));
	r->lava_swim = AND(I(FLIPPERS));
	r->lava_swim_sword = AND(I(FLIPPERS), I(SWORD));

	r->boss_requirements[0] = I(SWORD);
	r->boss_requirements[1] = AND(OR(I(SWORD), I(MAGIC_ROD)), I(POWER_BRACELET));
	r->boss_requirements[2] = AND(I(PEGASUS_BOOTS), I(SWORD));
	r->boss_requirements[3] = AND(I(FLIPPERS), OR(I(SWORD), I(MAGIC_ROD), I(BOW)));
	r->boss_requirements[4] = AND(I(HOOKSHOT), I(SWORD));
	r->boss_requirements[5] = I(BOMB);
	r->boss_requirements[6] = AND(OR(I(MAGIC_ROD), I(SWORD), I(HOOKSHOT)), COUNT(SHIELD, 2));
	r->boss_requirements[7] = I(MAGIC_ROD);
	r->boss_requirements[8] = r->attack_hookshot_no_bomb;
	r->boss_nightmare[LADX_NIGHTMARE_GEL] = I(MAGIC_POWDER);
	r->boss_nightmare[LADX_NIGHTMARE_AGAHNIM] = OR(I(SWORD), I(SHOVEL));
	r->boss_nightmare[LADX_NIGHTMARE_MOLDORM] = I(SWORD);
	r->boss_nightmare[LADX_NIGHTMARE_GANON] = AND(I(SWORD), I(PEGASUS_BOOTS));
	r->boss_nightmare[LADX_NIGHTMARE_LANMOLA] = OR(I(SWORD), I(BOW));

	MINIBOSS(ROLLING_BONES) = r->attack_hookshot;
	MINIBOSS(HINOX) = r->attack_hookshot;
	MINIBOSS(DODONGO) = I(BOMB);
	MINIBOSS(CUE_BALL) = I(SWORD);
	MINIBOSS(GHOMA) = OR(I(BOW), I(HOOKSHOT), I(MAGIC_ROD), I(BOOMERANG));
	MINIBOSS(SMASHER) = I(POWER_BRACELET);
	MINIBOSS(GRIM_CREEPER) = r->attack_hookshot_no_bomb;
	MINIBOSS(BLAINO) = I(SWORD);
	MINIBOSS(AVALAUNCH) = r->attack_hookshot;
	MINIBOSS(GIANT_BUZZ_BLOB) = I(MAGIC_POWDER);
	MINIBOSS(MOBLIN_KING) = I(SWORD);
	MINIBOSS(ARMOS_KNIGHT) = OR(I(BOW), I(MAGIC_ROD), I(SWORD));
	r->miniboss_nightmare[LADX_NIGHTMARE_GEL] = I(MAGIC_POWDER);
	r->miniboss_nightmare[LADX_NIGHTMARE_AGAHNIM] = OR(I(SWORD), I(SHOVEL));
	r->miniboss_nightmare[LADX_NIGHTMARE_MOLDORM] = I(SWORD);
	r->miniboss_nightmare[LADX_NIGHTMARE_GANON] = AND(I(SWORD), I(PEGASUS_BOOTS));
	r->miniboss_nightmare[LADX_NIGHTMARE_LANMOLA] = OR(I(SWORD), I(BOW));

	for (i = 0; i < LADX_ENEMY_COUNT; i++) {
		r->enemy_requirements[i] = ladx_req_none();
		r->enemy_present[i] = i < LADX_ENEMY_WINGED_BONE_PUTTER;
	}
	enemies(r, arena);

	if (!ladx_setting_on(options, LADX_SETTING_TRADEQUEST))
		r->shuffled_magnifier = B(true);
	if (IS(HARDMODE, "ohko"))
		MINIBOSS(ROLLING_BONES) = OR(I(BOW), I(MAGIC_ROD), I(BOOMERANG), AND(I(FEATHER), r->attack_hookshot));
	if (!IS(BOWWOW, "normal")) {
		ladx_req_remove(A, r->bush, I(SWORD));
		ladx_req_remove(A, r->pit_bush, I(SWORD));
		ladx_req_remove(A, r->hit_switch, I(SWORD));
		ladx_req_remove(A, r->hit_switch_color, I(SWORD));
		ladx_req_add(A, r->bush, COUNT(SWORD, 2));
		ladx_req_add(A, r->pit_bush, COUNT(SWORD, 2));
		ladx_req_add(A, r->hit_switch, COUNT(SWORD, 2));
		ladx_req_add(A, r->hit_switch_color, COUNT(SWORD, 2));
	}
	if (IS(LOGIC, "casual")) {
		ladx_req_remove(A, r->bush, I(MAGIC_POWDER));
		ladx_req_remove(A, r->attack_hookshot_powder, I(MAGIC_POWDER));
		ladx_req_remove(A, r->attack, I(BOMB));
		ladx_req_remove(A, r->attack_hookshot, I(BOMB));
		ladx_req_remove(A, r->attack_hookshot_powder, I(BOMB));
		ladx_req_remove(A, r->attack_no_boomerang, I(BOMB));
		ladx_req_remove(A, r->attack_skeleton, I(BOMB));
		ladx_req_remove(A, r->attack_gibdos, I(BOMB));
	}

	if (hard) {
		r->boss_requirements[1] = AND(OR(I(SWORD), I(MAGIC_ROD), I(BOMB)), I(POWER_BRACELET));
		r->boss_requirements[3] = AND(I(FLIPPERS), OR(I(SWORD), I(MAGIC_ROD), I(BOW), I(BOMB)));
		r->boss_requirements[6] = OR(I(MAGIC_ROD), AND(I(BOMB), I(BOW)), r->sword_beam,
					     AND(OR(I(SWORD), I(HOOKSHOT), I(BOW)), I(SHIELD)));
		ENEMY(POLS_VOICE) = OR(I(BOMB), I(MAGIC_ROD), AND(I(OCARINA), I(SONG1)),
				       AND(r->stun_wizrobe, r->throw_enemy, I(BOW)));
		ENEMY(WIZROBE) = OR(I(BOMB), I(MAGIC_ROD), AND(r->stun_wizrobe, r->throw_enemy, I(BOW)));
		ENEMY(THREE_OF_A_KIND) = OR(r->attack_hookshot, I(SHIELD));
		ENEMY(VIRE) = r->attack_hookshot_powder;
		ENEMY(GOPONGA_FLOWER) = OR(I(BOWWOW), I(HOOKSHOT), I(MAGIC_ROD), I(BOOMERANG), COUNT(SWORD, 2));
		ENEMY(GIANT_GOPONGA_FLOWER) =
			OR(I(BOWWOW), I(HOOKSHOT), I(MAGIC_ROD), I(BOOMERANG), COUNT(SWORD, 2));
		ENEMY(DESERT_LANMOLA) = r->attack_hookshot;
		ENEMY(SHADOW_LANMOLA) = r->attack_hookshot;
		ENEMY(BOMBER) = OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(MAGIC_POWDER));
		ENEMY(GHINI) = r->attack_hookshot;
		ENEMY(GIANT_GHINI) = r->attack_hookshot;
		ENEMY(WINGED_BONE_PUTTER) =
			OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(MAGIC_POWDER));
		r->enemy_present[LADX_ENEMY_WINGED_BONE_PUTTER] = true;
		ENEMY(BONE_PUTTER) = OR(I(SWORD), I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), I(MAGIC_POWDER));
		r->enemy_present[LADX_ENEMY_BONE_PUTTER] = true;
		ENEMY(BUZZ_BLOB) = OR(OR(I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG)), AND(I(HOOKSHOT), I(SWORD)));
	}

	if (IS(LOGIC, "glitched") || IS(LOGIC, "hell"))
		r->boss_requirements[6] = OR(I(MAGIC_ROD), I(BOMB), I(BOW), I(HOOKSHOT), AND(I(SWORD), I(SHIELD)));

	if (IS(LOGIC, "hell")) {
		r->boss_requirements[7] = OR(I(MAGIC_ROD), r->sword_beam);
		MINIBOSS(GHOMA) = OR(I(BOW), I(HOOKSHOT), I(MAGIC_ROD), I(BOOMERANG),
				     AND(I(OCARINA), I(BOMB), OR(I(SONG1), I(SONG3))));
		MINIBOSS(GIANT_BUZZ_BLOB) = OR(I(MAGIC_POWDER), r->sword_beam);
		ENEMY(MASTER_STALFOS) = I(SWORD);
		r->boss_requirements[6] =
			OR(I(MAGIC_ROD), I(BOMB), I(BOW), I(HOOKSHOT), r->sword_beam, AND(I(SWORD), I(SHIELD)));
		ENEMY(SHADOW_BLOB) = OR(I(SWORD), I(MAGIC_POWDER));
		ENEMY(SHADOW_DETHL) = OR(I(BOMB), I(BOW), I(BOOMERANG), r->sword_beam);
		ENEMY(BUZZ_BLOB) = OR(I(BOMB), I(BOW), I(MAGIC_ROD), I(BOOMERANG), r->sword_beam);
	}

	ENEMY(FINAL_NIGHTMARE) = AND(ENEMY(SHADOW_BLOB), ENEMY(SHADOW_AGAHNIM), ENEMY(SHADOW_MOLDORM),
				     ENEMY(SHADOW_GANON), ENEMY(SHADOW_LANMOLA), ENEMY(SHADOW_DETHL));
	r->enemy_present[LADX_ENEMY_FINAL_NIGHTMARE] = true;
}
