#ifndef RANDO_ITEMS_H
#define RANDO_ITEMS_H

#include "rando_config.h"
#include "rando_item_ids.h"

#include <stdbool.h>
#include <stdint.h>

/* One value per concrete VT item class; every VT item class extends Item directly. */
typedef enum RandoItemClass {
	RANDO_ITEM_CLASS_PLAIN,
	RANDO_ITEM_CLASS_SWORD,
	RANDO_ITEM_CLASS_SHIELD,
	RANDO_ITEM_CLASS_BOW,
	RANDO_ITEM_CLASS_BOTTLE_CONTENTS,
	RANDO_ITEM_CLASS_MEDALLION,
	RANDO_ITEM_CLASS_BOTTLE,
	RANDO_ITEM_CLASS_HEALTH,
	RANDO_ITEM_CLASS_CRYSTAL,
	RANDO_ITEM_CLASS_ARMOR,
	RANDO_ITEM_CLASS_KEY,
	RANDO_ITEM_CLASS_COMPASS,
	RANDO_ITEM_CLASS_BIG_KEY,
	RANDO_ITEM_CLASS_MAP,
	RANDO_ITEM_CLASS_PENDANT,
	RANDO_ITEM_CLASS_ARROW,
	RANDO_ITEM_CLASS_UPGRADE_BOMB,
	RANDO_ITEM_CLASS_UPGRADE_ARROW,
	RANDO_ITEM_CLASS_UPGRADE_MAGIC,
	RANDO_ITEM_CLASS_PROGRAMMABLE,
	RANDO_ITEM_CLASS_EVENT,
	RANDO_ITEM_CLASS_ALIAS,
	RANDO_ITEM_CLASS_COUNT,
} RandoItemClass;

typedef struct RandoItemInfo {
	const char *name;
	RandoItemClass item_class;
	uint8_t power_quarters;
	RandoItemId alias_target;
	bool key_prefixed;
} RandoItemInfo;

/* Asserts id < RANDO_ITEM_COUNT. */
const RandoItemInfo *rando_item_info(RandoItemId id);
/* Raw names only; RANDO_ITEM_NONE when unknown. */
RandoItemId rando_item_find(const char *name);
/* True for a key, big key, map or compass class whose region_wild_* flag is off. */
bool rando_dungeon_item_confined(const RandoConfig *c, RandoItemClass item_class);

/* VT's ItemCollection keeps two views that can disagree: counts is item_counts (has, count),
 * values is cached_values as a multiset (every filter-based predicate). unset and diff touch
 * counts only; merge adds the argument's counts to both views. */
typedef struct RandoItems {
	uint16_t counts[RANDO_ITEM_COUNT];
	uint16_t values[RANDO_ITEM_COUNT];
} RandoItems;

void rando_items_clear(RandoItems *items);
/* Item::all(world)->copy()->manyKeys(): VT's allItems base. */
void rando_items_all(RandoItems *items);
void rando_items_add(RandoItems *items, RandoItemId id);
/* removeItem: a no-op when the item has no count. */
void rando_items_remove(RandoItems *items, RandoItemId id);
/* offsetUnset. */
void rando_items_unset(RandoItems *items, RandoItemId id);
/* out may alias a. */
void rando_items_merge(RandoItems *out, const RandoItems *a, const RandoItems *b);
/* out may alias a. */
void rando_items_diff(RandoItems *out, const RandoItems *a, const RandoItems *b);

bool rando_items_has(const RandoItems *items, RandoItemId id);
/* RANDO_CONFIG_NULL is VT's has($key, null): false. */
bool rando_items_has_n(const RandoItems *items, RandoItemId id, int at_least);
int rando_items_count(const RandoItems *items);

/* A NULL config is VT's call with no world. */
bool rando_items_can_lift_rocks(const RandoItems *items);
bool rando_items_can_lift_dark_rocks(const RandoItems *items);
bool rando_items_can_light_torches(const RandoItems *items);
bool rando_items_can_melt_things(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_fly(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_spin_speed(const RandoItems *items);
bool rando_items_can_acquire_fairy(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_bunny_revive(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_shoot_arrows(const RandoItems *items, const RandoConfig *config, int min_level);
bool rando_items_can_block_lasers(const RandoItems *items);
bool rando_items_can_extend_magic(const RandoItems *items, const RandoConfig *config, int bars);
bool rando_items_glitched_link_in_dark_world(const RandoItems *items);
bool rando_items_has_health(const RandoItems *items, int minimum_hearts);
/* In quarter hearts, initial included. */
int rando_items_heart_count(const RandoItems *items, int initial_quarters);
bool rando_items_can_kill_escape_things(const RandoItems *items, const RandoConfig *config);
bool rando_items_can_kill_most_things(const RandoItems *items, const RandoConfig *config, int enemies);
bool rando_items_can_bomb_things(const RandoItems *items);
bool rando_items_can_get_good_bee(const RandoItems *items);
bool rando_items_has_sword(const RandoItems *items, int min_level);
bool rando_items_has_armor(const RandoItems *items, int min_level);
bool rando_items_has_bottle(const RandoItems *items, int at_least);
int rando_items_bottle_count(const RandoItems *items);
bool rando_items_has_a_bottle(const RandoItems *items);

#endif
