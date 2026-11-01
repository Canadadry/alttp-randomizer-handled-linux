#include "rando_spoiler.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define KEY_MAX 96
#define WORLD_SUFFIX ":1"

typedef struct Entry {
	char key[KEY_MAX];
	RandoItemId item;
} Entry;

static int entry_cmp(const void *a, const void *b)
{
	return strcmp(((const Entry *)a)->key, ((const Entry *)b)->key);
}

static int name_cmp(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void indent(FILE *out, int level)
{
	fprintf(out, "%*s", 4 * level, "");
}

/* json_encode with JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE; suffix is appended inside the quotes. */
static void string(FILE *out, const char *s, const char *suffix)
{
	fputc('"', out);
	for (; *s; s++) {
		assert((unsigned char)*s >= 0x20 && "spoiler: control characters in names are not encoded");
		if (*s == '"' || *s == '\\')
			fputc('\\', out);
		fputc(*s, out);
	}
	fputs(suffix, out);
	fputc('"', out);
}

static void key(FILE *out, int level, const char *s)
{
	indent(out, level);
	string(out, s, "");
	fputs(": ", out);
}

static bool spoiler_skips(RandoLocationClass location_class)
{
	return rando_location_is_a(location_class, RANDO_LOCATION_CLASS_PRIZE_EVENT) ||
	       rando_location_is_a(location_class, RANDO_LOCATION_CLASS_TRADE);
}

/* VT's getTarget()->getName(). */
static const char *target_name(RandoItemId item)
{
	const RandoItemInfo *info = rando_item_info(item);

	return info->item_class == RANDO_ITEM_CLASS_ALIAS ? rando_item_info(info->alias_target)->name : info->name;
}

/* How a region section names a placed item, suffix included: under rom.genericKeys an Item\Key is
 * `Key` with no suffix (an alias such as ShopKey is not an Item\Key). Writes into buf, returns it. */
static const char *placed_name(const RandoWorld *world, RandoItemId item, char *buf, size_t size)
{
	int len;

	if (world->config.rom_generic_keys && rando_item_info(item)->item_class == RANDO_ITEM_CLASS_KEY)
		return "Key";
	len = snprintf(buf, size, "%s" WORLD_SUFFIX, target_name(item));
	assert(len > 0 && (size_t)len < size && "spoiler: raise the item name buffer");
	(void)len;
	return buf;
}

/* Every region named spoiler_name merges into one object. */
static void write_region_section(FILE *out, const RandoWorld *world, const char *spoiler_name)
{
	static Entry entries[RANDO_LOCATION_COUNT];
	char name[KEY_MAX];
	int n = 0, r, i;

	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		const RandoRegionInfo *region = rando_region_info((RandoRegionId)r);

		if (strcmp(region->spoiler_name, spoiler_name) != 0)
			continue;
		for (i = 0; i < rando_layout_region_location_count(world->variant, (RandoRegionId)r); i++) {
			RandoLocationId id = rando_layout_region_location(world->variant, (RandoRegionId)r, i);
			const RandoLocationInfo *location = rando_location_info(id);
			int len;

			if (spoiler_skips(location->location_class))
				continue;
			len = snprintf(entries[n].key, KEY_MAX, "%s" WORLD_SUFFIX, location->name);
			assert(len > 0 && len < KEY_MAX && "spoiler: raise KEY_MAX for this location name");
			entries[n++].item = rando_world_item(world, id);
		}
	}
	if (n == 0) {
		fputs("[]", out);
		return;
	}
	qsort(entries, (size_t)n, sizeof entries[0], entry_cmp);
	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 2, entries[i].key);
		if (entries[i].item == RANDO_ITEM_NONE)
			string(out, "Nothing", "");
		else
			string(out, placed_name(world, (RandoItemId)entries[i].item, name, sizeof name), "");
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	indent(out, 1);
	fputc('}', out);
}

static bool any_shop_active(const RandoWorld *world)
{
	int s;

	for (s = 0; s < RANDO_SHOP_COUNT; s++)
		if (rando_world_shop_active(world, (RandoShopId)s))
			return true;
	return false;
}

/* Keys item_<slot>, location, type are already in strcmp order. Shop items are written with
 * getName(), not getTarget(), as VT's shop loop does. */
static void write_shops(FILE *out, const RandoWorld *world)
{
	bool first = true;
	int s, slot;

	fputs("[\n", out);
	for (s = 0; s < RANDO_SHOP_COUNT; s++) {
		RandoShopId id = (RandoShopId)s;
		const RandoShopInfo *shop = rando_shop_info(id);

		if (!rando_world_shop_active(world, id))
			continue;
		if (!first)
			fputs(",\n", out);
		first = false;
		indent(out, 2);
		fputs("{\n", out);
		for (slot = 0; slot < RANDO_SHOP_SLOTS; slot++) {
			RandoItemId item = rando_world_shop_item(world, id, slot);
			char name[16];

			if (item == RANDO_ITEM_NONE)
				continue;
			snprintf(name, sizeof name, "item_%d", slot);
			key(out, 3, name);
			fputs("{\n", out);
			key(out, 4, "item");
			string(out, rando_item_info(item)->name, WORLD_SUFFIX);
			fputs(",\n", out);
			key(out, 4, "price");
			fprintf(out, "%d\n", rando_world_shop_price(world, id, slot));
			indent(out, 3);
			fputs("},\n", out);
		}
		key(out, 3, "location");
		string(out, shop->name, "");
		fputs(",\n", out);
		key(out, 3, "type");
		string(out, shop->kind == RANDO_SHOP_KIND_TAKE_ANY ? "Take Any" : "Shop", "");
		fputc('\n', out);
		indent(out, 2);
		fputc('}', out);
	}
	fputc('\n', out);
	indent(out, 1);
	fputc(']', out);
}

typedef struct BossKey {
	const char *key;
	RandoRegionId region;
	RandoBossSlot slot;
	const char *literal;
} BossKey;

/* VT's getSpoiler Bosses keys, already in strcmp order. Hyrule Castle, Ganons Tower and Ganon are
 * literals in VT, not world reads. */
static const BossKey boss_keys[] = {
	{"Desert Palace", RANDO_REGION_DESERT_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Eastern Palace", RANDO_REGION_EASTERN_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Ganon", RANDO_REGION_NONE, RANDO_BOSS_SLOT_BOSS, "Ganon"},
	{"Ganons Tower", RANDO_REGION_NONE, RANDO_BOSS_SLOT_BOSS, "Agahnim 2"},
	{"Ganons Tower Basement", RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_BOTTOM, NULL},
	{"Ganons Tower Middle", RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_MIDDLE, NULL},
	{"Ganons Tower Top", RANDO_REGION_GANONS_TOWER, RANDO_BOSS_SLOT_TOP, NULL},
	{"Hyrule Castle", RANDO_REGION_NONE, RANDO_BOSS_SLOT_BOSS, "Agahnim"},
	{"Ice Palace", RANDO_REGION_ICE_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Misery Mire", RANDO_REGION_MISERY_MIRE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Palace Of Darkness", RANDO_REGION_PALACE_OF_DARKNESS, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Skull Woods", RANDO_REGION_SKULL_WOODS, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Swamp Palace", RANDO_REGION_SWAMP_PALACE, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Thieves Town", RANDO_REGION_THIEVES_TOWN, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Tower Of Hera", RANDO_REGION_TOWER_OF_HERA, RANDO_BOSS_SLOT_BOSS, NULL},
	{"Turtle Rock", RANDO_REGION_TURTLE_ROCK, RANDO_BOSS_SLOT_BOSS, NULL},
};

static const char *boss_key_name(const RandoWorld *world, const BossKey *k)
{
	RandoBossId boss;

	if (k->region == RANDO_REGION_NONE)
		return k->literal;
	boss = rando_world_boss(world, k->region, k->slot);
	assert(boss != RANDO_BOSS_NONE && "spoiler: Bosses written before placeBosses filled every slot");
	return rando_boss_name(boss);
}

static void write_bosses(FILE *out, const RandoWorld *world)
{
	size_t i, n = sizeof boss_keys / sizeof boss_keys[0];

	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 2, boss_keys[i].key);
		string(out, boss_key_name(world, &boss_keys[i]), "");
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	indent(out, 1);
	fputc('}', out);
}

/* The playthrough's slot 0 also skips Upgrade\Health (rando_playthrough). */
static bool equipped_skips(RandoItemId item)
{
	RandoItemClass c = rando_item_info(item)->item_class;

	return c == RANDO_ITEM_CLASS_UPGRADE_ARROW || c == RANDO_ITEM_CLASS_UPGRADE_BOMB || c == RANDO_ITEM_CLASS_EVENT;
}

static int equipped_count(const RandoWorld *world)
{
	int i, n = 0;

	for (i = 0; i < rando_world_pre_collected_count(world); i++)
		n += !equipped_skips(rando_world_pre_collected_item(world, i));
	return n;
}

static void write_equipment_slots(FILE *out, int level, const RandoItemId *items, int n, bool target)
{
	static Entry entries[RANDO_PRE_COLLECTED_MAX];
	int i;

	assert(n > 0 && n <= RANDO_PRE_COLLECTED_MAX);
	for (i = 0; i < n; i++) {
		snprintf(entries[i].key, KEY_MAX, "Equipment Slot %d", i + 1);
		entries[i].item = items[i];
	}
	qsort(entries, (size_t)n, sizeof entries[0], entry_cmp);
	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, level + 1, entries[i].key);
		string(out, target ? target_name(entries[i].item) : rando_item_info(entries[i].item)->name, WORLD_SUFFIX);
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	indent(out, level);
	fputc('}', out);
}

static void write_equipped(FILE *out, const RandoWorld *world)
{
	RandoItemId items[RANDO_PRE_COLLECTED_MAX];
	int i, n = 0;

	for (i = 0; i < rando_world_pre_collected_count(world); i++) {
		RandoItemId item = rando_world_pre_collected_item(world, i);

		if (!equipped_skips(item))
			items[n++] = item;
	}
	write_equipment_slots(out, 1, items, n, true);
}

static void entry_key(const RandoPlaythroughEntry *e, char key[KEY_MAX])
{
	int len = e->location == RANDO_LOCATION_NONE
			  ? snprintf(key, KEY_MAX, "%s - %d" WORLD_SUFFIX, rando_shop_info(e->shop)->name, e->slot)
			  : snprintf(key, KEY_MAX, "%s" WORLD_SUFFIX, rando_location_info(e->location)->name);

	assert(len > 0 && len < KEY_MAX && "spoiler: raise KEY_MAX for this location name");
}

/* entries[first, end) are one round. */
static void write_round(FILE *out, const RandoPlaythrough *playthrough, int first, int end)
{
	const char *regions[RANDO_REGION_COUNT];
	static Entry entries[RANDO_PLAYTHROUGH_ENTRY_MAX];
	int nregions = 0, i, j, r, n;

	for (i = first; i < end; i++) {
		const char *name = rando_playthrough_entry_region(&playthrough->entries[i]);

		for (j = 0; j < nregions && strcmp(regions[j], name) != 0; j++)
			;
		if (j == nregions)
			regions[nregions++] = name;
	}
	qsort(regions, (size_t)nregions, sizeof regions[0], name_cmp);
	fputs("{\n", out);
	for (r = 0; r < nregions; r++) {
		n = 0;
		for (i = first; i < end; i++) {
			if (strcmp(rando_playthrough_entry_region(&playthrough->entries[i]), regions[r]) != 0)
				continue;
			entry_key(&playthrough->entries[i], entries[n].key);
			entries[n++].item = playthrough->entries[i].item;
		}
		qsort(entries, (size_t)n, sizeof entries[0], entry_cmp);
		key(out, 3, regions[r]);
		fputs("{\n", out);
		for (i = 0; i < n; i++) {
			key(out, 4, entries[i].key);
			string(out, rando_item_info(entries[i].item)->name, WORLD_SUFFIX);
			fputs(i + 1 < n ? ",\n" : "\n", out);
		}
		indent(out, 3);
		fputs(r + 1 < nregions ? "},\n" : "}\n", out);
	}
	indent(out, 2);
	fputc('}', out);
}

typedef enum PlaythroughKeyKind { KEY_EQUIPPED, KEY_ROUND, KEY_CHAIN, KEY_REGIONS } PlaythroughKeyKind;

typedef struct PlaythroughKey {
	char key[24];
	PlaythroughKeyKind kind;
	int first, end;
} PlaythroughKey;

static int playthrough_key_cmp(const void *a, const void *b)
{
	return strcmp(((const PlaythroughKey *)a)->key, ((const PlaythroughKey *)b)->key);
}

/* Keys: "0" (slot 0's Equipped), one per round with an entry, longest_item_chain, regions_visited. */
static void write_playthrough(FILE *out, const RandoPlaythrough *playthrough)
{
	static PlaythroughKey keys[RANDO_PLAYTHROUGH_ENTRY_MAX + 3];
	int n = 0, i;

	if (playthrough->nequipped > 0) {
		snprintf(keys[n].key, sizeof keys[n].key, "0");
		keys[n++].kind = KEY_EQUIPPED;
	}
	for (i = 0; i < playthrough->nentries; i++) {
		if (i > 0 && playthrough->entries[i - 1].round == playthrough->entries[i].round)
			continue;
		assert(playthrough->entries[i].round > 0);
		snprintf(keys[n].key, sizeof keys[n].key, "%d", playthrough->entries[i].round);
		keys[n].kind = KEY_ROUND;
		keys[n].first = i;
		keys[n].end = i + 1;
		while (keys[n].end < playthrough->nentries
		       && playthrough->entries[keys[n].end].round == playthrough->entries[i].round)
			keys[n].end++;
		n++;
	}
	snprintf(keys[n].key, sizeof keys[n].key, "longest_item_chain");
	keys[n++].kind = KEY_CHAIN;
	snprintf(keys[n].key, sizeof keys[n].key, "regions_visited");
	keys[n++].kind = KEY_REGIONS;
	qsort(keys, (size_t)n, sizeof keys[0], playthrough_key_cmp);

	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 2, keys[i].key);
		if (keys[i].kind == KEY_EQUIPPED) {
			fputs("{\n", out);
			key(out, 3, "Equipped");
			write_equipment_slots(out, 3, playthrough->equipped, playthrough->nequipped, false);
			fputc('\n', out);
			indent(out, 2);
			fputc('}', out);
		} else if (keys[i].kind == KEY_CHAIN) {
			fprintf(out, "%d", playthrough->longest_item_chain);
		} else if (keys[i].kind == KEY_REGIONS) {
			fprintf(out, "%d", playthrough->regions_visited);
		} else {
			write_round(out, playthrough, keys[i].first, keys[i].end);
		}
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	indent(out, 1);
	fputc('}', out);
}

typedef enum MetaKind { META_STRING, META_INT, META_BOOL } MetaKind;

typedef struct Meta {
	const char *key;
	MetaKind kind;
	const char *s;
	int i;
} Meta;

static int meta_cmp(const void *a, const void *b)
{
	return strcmp(((const Meta *)a)->key, ((const Meta *)b)->key);
}

static const char *const accessibility_names[] = {
	[RANDO_ACCESSIBILITY_ITEMS] = "items", [RANDO_ACCESSIBILITY_LOCATIONS] = "locations",
	[RANDO_ACCESSIBILITY_NONE] = "none"};
static const char *const dungeon_items_names[] = {
	[RANDO_DUNGEON_ITEMS_STANDARD] = "standard", [RANDO_DUNGEON_ITEMS_MC] = "mc",
	[RANDO_DUNGEON_ITEMS_MCS] = "mcs", [RANDO_DUNGEON_ITEMS_FULL] = "full"};
static const char *const goal_names[] = {
	[RANDO_GOAL_GANON] = "ganon", [RANDO_GOAL_FAST_GANON] = "fast_ganon", [RANDO_GOAL_DUNGEONS] = "dungeons",
	[RANDO_GOAL_PEDESTAL] = "pedestal", [RANDO_GOAL_TRIFORCE_HUNT] = "triforce-hunt",
	[RANDO_GOAL_GANONHUNT] = "ganonhunt", [RANDO_GOAL_COMPLETIONIST] = "completionist"};
static const char *const functionality_names[] = {
	[RANDO_ITEM_FUNCTIONALITY_NORMAL] = "normal", [RANDO_ITEM_FUNCTIONALITY_HARD] = "hard",
	[RANDO_ITEM_FUNCTIONALITY_EXPERT] = "expert"};
static const char *const placement_names[] = {[RANDO_PLACEMENT_BASIC] = "basic",
					      [RANDO_PLACEMENT_ADVANCED] = "advanced"};
static const char *const item_pool_names[] = {
	[RANDO_ITEM_POOL_NORMAL] = "normal", [RANDO_ITEM_POOL_HARD] = "hard", [RANDO_ITEM_POOL_EXPERT] = "expert",
	[RANDO_ITEM_POOL_CROWD_CONTROL] = "crowd_control"};
static const char *const logic_names[] = {
	[RANDO_LOGIC_NO_GLITCHES] = "NoGlitches", [RANDO_LOGIC_OVERWORLD_GLITCHES] = "OverworldGlitches",
	[RANDO_LOGIC_HYBRID_MAJOR_GLITCHES] = "HybridMajorGlitches", [RANDO_LOGIC_MAJOR_GLITCHES] = "MajorGlitches",
	[RANDO_LOGIC_NO_LOGIC] = "NoLogic"};
static const char *const state_names[] = {
	[RANDO_STATE_STANDARD] = "standard", [RANDO_STATE_OPEN] = "open", [RANDO_STATE_INVERTED] = "inverted",
	[RANDO_STATE_RETRO] = "retro"};
static const char *const weapons_names[] = {
	[RANDO_WEAPONS_RANDOMIZED] = "randomized", [RANDO_WEAPONS_ASSURED] = "assured",
	[RANDO_WEAPONS_VANILLA] = "vanilla", [RANDO_WEAPONS_SWORDLESS] = "swordless"};

/* NULL for an UNSET value: VT's absent key, written as null. */
static const char *named(const char *const *names, size_t n, int value)
{
	assert(value >= 0 && (size_t)value < n);
	return names[value];
}

#define NAMED(names, value) named(names, sizeof names / sizeof names[0], (int)(value))

/* A drawn count is the integer VT resolved; a fixed one stays the config's string. */
static Meta crystals_meta(const char *k, RandoOptionKey option, RandoCrystals value, int resolved)
{
	Meta m = {k, META_STRING, NULL, 0};

	if (value == RANDO_CRYSTALS_RANDOM) {
		m.kind = META_INT;
		m.i = resolved;
	} else {
		m.s = rando_option_value_name(option, (int)value);
	}
	return m;
}

static void write_meta(FILE *out, const RandoWorld *world, const RandoOptions *options)
{
	const RandoConfig *c = &world->config;
	Meta meta[] = {
		{"accessibility", META_STRING, NAMED(accessibility_names, c->accessibility), 0},
		{"allow_quickswap", META_BOOL, NULL, 1},
		crystals_meta("crystals_ganon", RANDO_OPTION_CRYSTALS_GANON, options->crystals_ganon, c->crystals_ganon),
		crystals_meta("crystals_tower", RANDO_OPTION_CRYSTALS_TOWER, options->crystals_tower, c->crystals_tower),
		{"dungeon_items", META_STRING, NAMED(dungeon_items_names, c->dungeon_items), 0},
		{"enemizer.boss_shuffle", META_STRING, "none", 0},
		{"enemizer.enemy_damage", META_STRING, "default", 0},
		{"enemizer.enemy_health", META_STRING, "default", 0},
		{"enemizer.enemy_shuffle", META_STRING, "none", 0},
		{"enemizer.pot_shuffle", META_STRING, "off", 0},
		{"entry_crystals_ganon", META_STRING,
		 rando_option_value_name(RANDO_OPTION_CRYSTALS_GANON, (int)options->crystals_ganon), 0},
		{"entry_crystals_tower", META_STRING,
		 rando_option_value_name(RANDO_OPTION_CRYSTALS_TOWER, (int)options->crystals_tower), 0},
		{"goal", META_STRING, NAMED(goal_names, c->goal), 0},
		{"hints", META_STRING, "on", 0},
		{"item_functionality", META_STRING, NAMED(functionality_names, c->item_functionality), 0},
		{"item_placement", META_STRING, NAMED(placement_names, c->item_placement), 0},
		{"item_pool", META_STRING, NAMED(item_pool_names, c->item_pool), 0},
		{"logic", META_STRING, NAMED(logic_names, c->logic), 0},
		{"mode", META_STRING, NAMED(state_names, c->mode_state), 0},
		{"pseudoboots", META_BOOL, NULL, 0},
		{"rom_mode", META_STRING, NAMED(logic_names, c->logic), 0},
		{"size", META_INT, NULL, 2},
		{"spoilers", META_STRING, "on", 0},
		{"tournament", META_BOOL, NULL, 0},
		{"weapons", META_STRING, NAMED(weapons_names, c->mode_weapons), 0},
		{"world_id", META_INT, NULL, 1},
		{"worlds", META_INT, NULL, 1},
	};
	size_t i, n = sizeof meta / sizeof meta[0];

	qsort(meta, n, sizeof meta[0], meta_cmp);
	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 2, meta[i].key);
		if (meta[i].kind == META_STRING && !meta[i].s)
			fputs("null", out);
		else if (meta[i].kind == META_STRING)
			string(out, meta[i].s, "");
		else if (meta[i].kind == META_INT)
			fprintf(out, "%d", meta[i].i);
		else
			fputs(meta[i].i ? "true" : "false", out);
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	indent(out, 1);
	fputc('}', out);
}

bool rando_spoiler_write_json(FILE *out, const RandoWorld *world, const RandoOptions *options,
			      const RandoPlaythrough *playthrough)
{
	static const char shops[] = "Shops", bosses[] = "Bosses", equipped[] = "Equipped", meta[] = "meta",
			  playthrough_key[] = "playthrough";
	const char *sections[RANDO_REGION_COUNT + 5];
	int n = 0, r, i;

	for (r = 0; r < RANDO_REGION_COUNT; r++) {
		const char *name = rando_region_info((RandoRegionId)r)->spoiler_name;

		for (i = 0; i < n && strcmp(sections[i], name) != 0; i++)
			;
		if (i == n)
			sections[n++] = name;
	}
	if (any_shop_active(world))
		sections[n++] = shops;
	sections[n++] = bosses;
	if (equipped_count(world) > 0)
		sections[n++] = equipped;
	sections[n++] = meta;
	if (playthrough)
		sections[n++] = playthrough_key;
	qsort(sections, (size_t)n, sizeof sections[0], name_cmp);

	fputs("{\n", out);
	for (i = 0; i < n; i++) {
		key(out, 1, sections[i]);
		if (sections[i] == shops)
			write_shops(out, world);
		else if (sections[i] == bosses)
			write_bosses(out, world);
		else if (sections[i] == equipped)
			write_equipped(out, world);
		else if (sections[i] == meta)
			write_meta(out, world, options);
		else if (sections[i] == playthrough_key)
			write_playthrough(out, playthrough);
		else
			write_region_section(out, world, sections[i]);
		fputs(i + 1 < n ? ",\n" : "\n", out);
	}
	fputs("}\n", out);
	return fflush(out) == 0 && !ferror(out);
}

bool rando_spoiler_write_pool(FILE *out, const RandoWorld *world, const RandoPools *pools)
{
	char name[KEY_MAX];
	int list, i;

	for (list = 0; list < RANDO_POOL_LIST_COUNT; list++)
		for (i = 0; i < pools->count[list]; i++)
			fprintf(out, "%s\n", placed_name(world, (RandoItemId)pools->items[list][i], name, sizeof name));
	return fflush(out) == 0 && !ferror(out);
}
