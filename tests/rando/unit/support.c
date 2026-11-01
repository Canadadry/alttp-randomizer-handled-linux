#include "support.h"

#include "rando.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int test_fixtures_mode(const char *test, int argc, char **argv)
{
	if (argc == 1)
		return 0;
	if (argc == 2 && strcmp(argv[1], "--fixtures") == 0)
		return 1;
	fprintf(stderr, "usage: %s [--fixtures]\n", test);
	exit(2);
}

void test_fail(const char *test, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: FAIL ", test);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

void fixture_open(Fixture *fx, const char *path)
{
	memset(fx, 0, sizeof *fx);
	fx->path = path;
	fx->file = fopen(path, "r");
	if (!fx->file)
		test_fail("fixture", "cannot open %s", path);
}

int fixture_next(Fixture *fx)
{
	while (fgets(fx->line, sizeof fx->line, fx->file)) {
		size_t len = strlen(fx->line);
		char *p;

		fx->line_no++;
		if (len > 0 && fx->line[len - 1] == '\n')
			fx->line[--len] = '\0';
		else if (!feof(fx->file))
			test_fail("fixture", "%s:%ld: line longer than %d bytes", fx->path, fx->line_no,
				  FIXTURE_LINE_MAX - 2);
		if (fx->line[0] == '\0' || fx->line[0] == '#')
			continue;
		memcpy(fx->record, fx->line, len + 1);
		fx->nfields = 0;
		for (p = fx->line;;) {
			if (fx->nfields == FIXTURE_FIELDS_MAX)
				fixture_fail(fx, "too many fields");
			fx->field[fx->nfields++] = p;
			p = strchr(p, '\t');
			if (!p)
				break;
			*p++ = '\0';
		}
		return 1;
	}
	if (ferror(fx->file))
		test_fail("fixture", "%s: read error", fx->path);
	return 0;
}

void fixture_fail(const Fixture *fx, const char *why)
{
	fprintf(stderr, "fixture: FAIL %s:%ld: %s\n  %s\n", fx->path, fx->line_no, why, fx->record);
	exit(1);
}

void fixture_close(Fixture *fx)
{
	if (fx->file)
		fclose(fx->file);
	fx->file = NULL;
}

void fixture_parse_view(const Fixture *fx, const char *view, uint16_t out[RANDO_ITEM_COUNT],
			const uint16_t all[RANDO_ITEM_COUNT])
{
	char name[64];
	const char *p = view + 1;

	if (view[0] == 'E')
		memset(out, 0, RANDO_ITEM_COUNT * sizeof out[0]);
	else if (view[0] == 'A' && all)
		memcpy(out, all, RANDO_ITEM_COUNT * sizeof out[0]);
	else
		fixture_fail(fx, "view base is neither E nor a usable A");
	if (*p == '\0')
		return;
	if (*p != ':')
		fixture_fail(fx, "view base not followed by ':'");
	while (*p) {
		const char *eq = strchr(++p, '=');
		char *end;
		long count;
		RandoItemId id;

		if (!eq || (size_t)(eq - p) >= sizeof name)
			fixture_fail(fx, "malformed view entry");
		memcpy(name, p, (size_t)(eq - p));
		name[eq - p] = '\0';
		id = rando_item_find(name);
		if (id == RANDO_ITEM_NONE)
			fixture_fail(fx, "unknown item in view");
		count = strtol(eq + 1, &end, 10);
		if (end == eq + 1 || count < 0 || count > UINT16_MAX || (*end != '|' && *end != '\0'))
			fixture_fail(fx, "malformed view count");
		out[id] = (uint16_t)count;
		p = end;
	}
}

void fixture_parse_items(const Fixture *fx, const char *counts, const char *values, RandoItems *out)
{
	RandoItems all;

	rando_items_all(&all);
	fixture_parse_view(fx, counts, out->counts, all.counts);
	fixture_parse_view(fx, values, out->values, all.values);
}

#define PRED_COLUMNS_MAX 128
#define PRED_CONFIGS_MAX 8
#define PRED_MULTISETS_MAX 2048

typedef struct NamedConfig {
	char name[32];
	RandoConfig config;
} NamedConfig;

typedef struct EnumValue {
	const char *json;
	int value;
} EnumValue;

static const EnumValue states[] = {
	{"\"standard\"", RANDO_STATE_STANDARD}, {"\"open\"", RANDO_STATE_OPEN},
	{"\"inverted\"", RANDO_STATE_INVERTED}, {"\"retro\"", RANDO_STATE_RETRO}, {NULL, 0},
};
static const EnumValue weapons[] = {
	{"\"randomized\"", RANDO_WEAPONS_RANDOMIZED}, {"\"assured\"", RANDO_WEAPONS_ASSURED},
	{"\"vanilla\"", RANDO_WEAPONS_VANILLA}, {"\"swordless\"", RANDO_WEAPONS_SWORDLESS}, {NULL, 0},
};
static const EnumValue placements[] = {
	{"\"basic\"", RANDO_PLACEMENT_BASIC}, {"\"advanced\"", RANDO_PLACEMENT_ADVANCED}, {NULL, 0},
};
static const EnumValue logics[] = {
	{"\"NoGlitches\"", RANDO_LOGIC_NO_GLITCHES}, {"\"OverworldGlitches\"", RANDO_LOGIC_OVERWORLD_GLITCHES},
	{"\"HybridMajorGlitches\"", RANDO_LOGIC_HYBRID_MAJOR_GLITCHES},
	{"\"MajorGlitches\"", RANDO_LOGIC_MAJOR_GLITCHES}, {"\"NoLogic\"", RANDO_LOGIC_NO_LOGIC}, {NULL, 0},
};
static const EnumValue goals[] = {
	{"\"ganon\"", RANDO_GOAL_GANON}, {"\"fast_ganon\"", RANDO_GOAL_FAST_GANON},
	{"\"dungeons\"", RANDO_GOAL_DUNGEONS}, {"\"pedestal\"", RANDO_GOAL_PEDESTAL},
	{"\"triforce-hunt\"", RANDO_GOAL_TRIFORCE_HUNT}, {"\"ganonhunt\"", RANDO_GOAL_GANONHUNT},
	{"\"completionist\"", RANDO_GOAL_COMPLETIONIST}, {NULL, 0},
};
static const EnumValue accessibilities[] = {
	{"\"items\"", RANDO_ACCESSIBILITY_ITEMS}, {"\"locations\"", RANDO_ACCESSIBILITY_LOCATIONS},
	{"\"none\"", RANDO_ACCESSIBILITY_NONE}, {NULL, 0},
};
static const EnumValue dungeon_items[] = {
	{"\"standard\"", RANDO_DUNGEON_ITEMS_STANDARD}, {"\"mc\"", RANDO_DUNGEON_ITEMS_MC},
	{"\"mcs\"", RANDO_DUNGEON_ITEMS_MCS}, {"\"full\"", RANDO_DUNGEON_ITEMS_FULL}, {NULL, 0},
};
static const EnumValue enemy_healths[] = {
	{"\"default\"", RANDO_ENEMY_HEALTH_DEFAULT}, {"\"easy\"", RANDO_ENEMY_HEALTH_EASY},
	{"\"hard\"", RANDO_ENEMY_HEALTH_HARD}, {"\"expert\"", RANDO_ENEMY_HEALTH_EXPERT}, {NULL, 0},
};

static int enum_value(const Fixture *fx, const EnumValue *values, const char *json)
{
	for (; values->json; values++)
		if (strcmp(values->json, json) == 0)
			return values->value;
	fixture_fail(fx, "unknown config value");
	return 0;
}

static bool bool_value(const Fixture *fx, const char *json)
{
	if (strcmp(json, "true") == 0)
		return true;
	if (strcmp(json, "false") != 0)
		fixture_fail(fx, "config value is not a JSON boolean");
	return false;
}

/* VT compares these numerically, so the baseline's "7" is 7. */
static int int_value(const Fixture *fx, const char *json)
{
	const char *p = json[0] == '"' ? json + 1 : json;
	char *end;
	long v = strtol(p, &end, 10);

	if (end == p || (json[0] == '"' ? strcmp(end, "\"") : strcmp(end, "")) != 0)
		fixture_fail(fx, "config value is not an integer");
	return (int)v;
}

typedef struct BoolKey {
	const char *key;
	size_t offset;
} BoolKey;

#define BOOL_KEY(k, field) {k, offsetof(RandoConfig, field)}

static const BoolKey bool_keys[] = {
	BOOL_KEY("ignoreCanKillEscapeThings", ignore_can_kill_escape_things),
	BOOL_KEY("region.wildKeys", region_wild_keys),
	BOOL_KEY("region.wildBigKeys", region_wild_big_keys),
	BOOL_KEY("region.wildMaps", region_wild_maps),
	BOOL_KEY("region.wildCompasses", region_wild_compasses),
	BOOL_KEY("region.bossNormalLocation", region_boss_normal_location),
	BOOL_KEY("region.cantTakeDamage", region_cant_take_damage),
	BOOL_KEY("region.forceSkullWoodsKey", region_force_skull_woods_key),
	BOOL_KEY("region.requireBetterBow", region_require_better_bow),
	BOOL_KEY("region.requireBetterSword", region_require_better_sword),
	BOOL_KEY("region.swordsInPool", region_swords_in_pool),
	BOOL_KEY("rom.CatchableFairies", rom_catchable_fairies),
	BOOL_KEY("rom.rupeeBow", rom_rupee_bow),
	BOOL_KEY("rom.genericKeys", rom_generic_keys),
	BOOL_KEY("rom.mapOnPickup", rom_map_on_pickup),
	BOOL_KEY("rom.vanillaKeys", rom_vanilla_keys),
	BOOL_KEY("rom.vanillaBigKeys", rom_vanilla_big_keys),
	BOOL_KEY("rom.vanillaMaps", rom_vanilla_maps),
	BOOL_KEY("rom.vanillaCompasses", rom_vanilla_compasses),
	BOOL_KEY("canSuperBunny", can_super_bunny),
	BOOL_KEY("canFakeFlipper", can_fake_flipper),
	BOOL_KEY("canSuperSpeed", can_super_speed),
	BOOL_KEY("canBootsClip", can_boots_clip),
	BOOL_KEY("canMirrorClip", can_mirror_clip),
	BOOL_KEY("canWaterWalk", can_water_walk),
	BOOL_KEY("canDungeonRevive", can_dungeon_revive),
	BOOL_KEY("canMirrorWrap", can_mirror_wrap),
	BOOL_KEY("canOWYBA", can_owyba),
	BOOL_KEY("canOneFrameClipOW", can_one_frame_clip_ow),
	BOOL_KEY("canOneFrameClipUW", can_one_frame_clip_uw),
	BOOL_KEY("canTransitionWrapped", can_transition_wrapped),
	BOOL_KEY("canBunnySurf", can_bunny_surf),
	BOOL_KEY("canBunnyRevive", can_bunny_revive),
};

void fixture_apply_config(const Fixture *fx, RandoConfig *c, const char *key, const char *json)
{
	size_t i;

	if (strcmp(json, "null") == 0)
		return;
	for (i = 0; i < sizeof bool_keys / sizeof bool_keys[0]; i++)
		if (strcmp(key, bool_keys[i].key) == 0) {
			*(bool *)((char *)c + bool_keys[i].offset) = bool_value(fx, json);
			return;
		}
	if (strcmp(key, "mode.state") == 0)
		c->mode_state = (RandoState)enum_value(fx, states, json);
	else if (strcmp(key, "mode.weapons") == 0)
		c->mode_weapons = (RandoWeapons)enum_value(fx, weapons, json);
	else if (strcmp(key, "itemPlacement") == 0)
		c->item_placement = (RandoItemPlacement)enum_value(fx, placements, json);
	else if (strcmp(key, "logic") == 0)
		c->logic = (RandoLogic)enum_value(fx, logics, json);
	else if (strcmp(key, "goal") == 0)
		c->goal = (RandoGoal)enum_value(fx, goals, json);
	else if (strcmp(key, "accessibility") == 0)
		c->accessibility = (RandoAccessibility)enum_value(fx, accessibilities, json);
	else if (strcmp(key, "dungeonItems") == 0)
		c->dungeon_items = (RandoDungeonItems)enum_value(fx, dungeon_items, json);
	else if (strcmp(key, "enemizer.enemyHealth") == 0)
		c->enemizer_enemy_health = (RandoEnemyHealth)enum_value(fx, enemy_healths, json);
	else if (strcmp(key, "crystals.ganon") == 0)
		c->crystals_ganon = int_value(fx, json);
	else if (strcmp(key, "crystals.tower") == 0)
		c->crystals_tower = int_value(fx, json);
	else if (strcmp(key, "item.Goal.Required") == 0)
		c->item_goal_required = int_value(fx, json);
	else if (strcmp(key, "item.require.Lamp") == 0)
		c->item_require_lamp = int_value(fx, json);
	else if (strcmp(key, "item.overflow.count.Shield") == 0)
		c->item_overflow_count_shield = int_value(fx, json);
	else if (strcmp(key, "rom.BottleFill.Magic") == 0)
		c->rom_bottle_fill_magic = int_value(fx, json);
	else if (strcmp(key, "rom.freeItemMenu") == 0)
		(void)int_value(fx, json);
	else
		fixture_fail(fx, "unknown config key");
}

static const RandoConfig *find_config(const Fixture *fx, const NamedConfig *configs, int n, const char *name)
{
	int i;

	for (i = 0; i < n; i++)
		if (strcmp(configs[i].name, name) == 0)
			return &configs[i].config;
	fixture_fail(fx, "unknown config name");
	return NULL;
}

int fixture_replay_predicates(const char *test, const PredicateColumn *columns, int ncolumns, long *values)
{
	static RandoItems multisets[PRED_MULTISETS_MAX];
	NamedConfig configs[PRED_CONFIGS_MAX];
	int column_of[PRED_COLUMNS_MAX];
	int nconfigs = 0, nmultisets = 0, ncols = -1, records = 0, i;
	Fixture fx;

	*values = 0;
	fixture_open(&fx, FIXTURE_PREDICATE_VECTORS);
	while (fixture_next(&fx)) {
		const char *kind = fx.field[0];

		if (strcmp(kind, "vector_config") == 0) {
			if (nconfigs == PRED_CONFIGS_MAX || strlen(fx.field[1]) >= sizeof configs[0].name)
				fixture_fail(&fx, "too many configs or name too long");
			strcpy(configs[nconfigs].name, fx.field[1]);
			rando_config_init(&configs[nconfigs++].config);
		} else if (strcmp(kind, "config") == 0) {
			RandoConfig *c = (RandoConfig *)find_config(&fx, configs, nconfigs, fx.field[1]);

			fixture_apply_config(&fx, c, fx.field[2], fx.field[3]);
		} else if (strcmp(kind, "columns") == 0) {
			char *p = fx.field[1];
			int found = 0;

			for (ncols = 0; p; ncols++) {
				char *bar = strchr(p, '|');

				if (ncols == PRED_COLUMNS_MAX)
					fixture_fail(&fx, "too many columns");
				if (bar)
					*bar = '\0';
				column_of[ncols] = -1;
				for (i = 0; i < ncolumns; i++)
					if (strcmp(columns[i].name, p) == 0) {
						column_of[ncols] = i;
						found++;
					}
				p = bar ? bar + 1 : NULL;
			}
			if (found != ncolumns)
				test_fail(test, "%d of the %d columns tested are missing from the fixture",
					  ncolumns - found, ncolumns);
		} else if (strcmp(kind, "multiset") == 0) {
			if (atoi(fx.field[1]) != nmultisets || nmultisets == PRED_MULTISETS_MAX)
				fixture_fail(&fx, "multiset index out of order or too many multisets");
			fixture_parse_items(&fx, fx.field[3], fx.field[4], &multisets[nmultisets++]);
		} else if (strcmp(kind, "pred") == 0) {
			int m = atoi(fx.field[1]);
			const RandoConfig *c = find_config(&fx, configs, nconfigs, fx.field[2]);
			char *p = fx.field[3];

			if (m < 0 || m >= nmultisets || ncols < 0)
				fixture_fail(&fx, "pred before its multiset or the columns record");
			for (i = 0; i < ncols; i++) {
				char *end;
				long want = strtol(p, &end, 10);
				int got;

				if (end == p)
					fixture_fail(&fx, "malformed value");
				p = end;
				if (column_of[i] < 0)
					continue;
				got = columns[column_of[i]].eval(&multisets[m], c);
				if (got != want) {
					fprintf(stderr, "%s: %s: VT %ld, C %d\n", test, columns[column_of[i]].name,
						want, got);
					fixture_fail(&fx, "predicate differs from VT");
				}
				(*values)++;
			}
			if (*p != '\0')
				fixture_fail(&fx, "more values than columns");
			records++;
		} else {
			fixture_fail(&fx, "unknown record kind");
		}
	}
	fixture_close(&fx);
	if (records == 0)
		test_fail(test, "no pred record replayed");
	return records;
}

static RandoLocationId location_named(const Fixture *fx, const char *name)
{
	RandoLocationId id = rando_location_find(name);

	if (id == RANDO_LOCATION_NONE)
		fixture_fail(fx, "unknown location");
	return id;
}

static RandoItemId item_named(const Fixture *fx, const char *name)
{
	RandoItemId id = rando_item_find(name);

	if (id == RANDO_ITEM_NONE)
		fixture_fail(fx, "unknown item");
	return id;
}

/* Splits s in place at each sep; returns the number of parts, "-" giving none. */
static int split(const Fixture *fx, char *s, char sep, char **parts, int max)
{
	int n = 0;

	if (strcmp(s, "-") == 0)
		return 0;
	for (;;) {
		char *next = strchr(s, sep);

		if (n == max)
			fixture_fail(fx, "too many list entries");
		parts[n++] = s;
		if (!next)
			return n;
		*next = '\0';
		s = next + 1;
	}
}

static char *value_of(const Fixture *fx, char *entry)
{
	char *eq = strchr(entry, '=');

	if (!eq)
		fixture_fail(fx, "list entry without '='");
	*eq = '\0';
	return eq + 1;
}

static void apply_overrides(const Fixture *fx, RandoConfig *config, char **parts, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		char *json = value_of(fx, parts[i]);

		fixture_apply_config(fx, config, parts[i], json);
	}
}

/* Returns the value the four rom.vanilla* flags share, -1 when they differ. */
static int vanilla_dungeon_items(const RandoConfig *c)
{
	if (c->rom_vanilla_keys != c->rom_vanilla_big_keys || c->rom_vanilla_keys != c->rom_vanilla_compasses
	    || c->rom_vanilla_keys != c->rom_vanilla_maps)
		return -1;
	return c->rom_vanilla_keys;
}

static RandoRegionId region_named(const Fixture *fx, const char *key)
{
	RandoRegionId region = rando_region_find(key);

	if (region == RANDO_REGION_NONE)
		fixture_fail(fx, "unknown region");
	return region;
}

/* RegionTest's bare Region has no region items, so any C region without them answers canFill as it does. */
static RandoRegionId bare_region(const Fixture *fx)
{
	if (rando_region_info(RANDO_REGION_NORTH_EAST_LIGHT_WORLD)->nitems != 0)
		fixture_fail(fx, "the region standing for VT's bare Region has region items");
	return RANDO_REGION_NORTH_EAST_LIGHT_WORLD;
}

enum {
	ROW_FILE = 1, ROW_METHOD, ROW_PROVIDER, ROW_INDEX, ROW_VARIANT, ROW_CONFIG, ROW_PRESET, ROW_COUNTS,
	ROW_VALUES, ROW_QUERY, ROW_TARGET, ROW_ITEM, ROW_EXPECTED, ROW_ANSWER, ROW_FIELDS
};

/* VT's accessibility "full" is a legacy value VT reads as "items". */
static void set_prepared_option(const Fixture *fx, RandoOptions *options, const char *key, const char *json)
{
	static const char *const keys[][2] = {
		{"mode.weapons", "weapons"}, {"goal", "goal"}, {"accessibility", "accessibility"},
	};
	char value[FIXTURE_LINE_MAX];
	size_t i, len = strlen(json);

	if (len < 2 || json[0] != '"' || json[len - 1] != '"')
		fixture_fail(fx, "config value is not a JSON string");
	memcpy(value, json + 1, len - 2);
	value[len - 2] = '\0';
	if (strcmp(key, "accessibility") == 0 && strcmp(value, "full") == 0)
		strcpy(value, "items");
	for (i = 0; i < sizeof keys / sizeof keys[0]; i++)
		if (strcmp(key, keys[i][0]) == 0) {
			if (!rando_options_set(options, keys[i][1], value))
				fixture_fail(fx, "unknown option value");
			return;
		}
	fixture_fail(fx, "config key without a generator option");
}

static RandoGen prepared_gen;

static int prepared_item(const Fixture *fx, char **overrides, int n)
{
	RandoOptions options;
	int i;

	if (strcmp(fx->field[ROW_VARIANT], "standard") != 0)
		fixture_fail(fx, "prepared_item on a variant other than standard");
	rando_options_init(&options);
	options.state = RANDO_OPTION_STATE_STANDARD;
	for (i = 0; i < n; i++) {
		char *json = value_of(fx, overrides[i]);

		set_prepared_option(fx, &options, overrides[i], json);
	}
	if (rando_generate_until(&prepared_gen, &options, 1, 0, NULL, NULL, RANDO_STAGE_PREPARE_WORLD) != RANDO_INCOMPLETE)
		fixture_fail(fx, "the generator did not stop after prepareWorld");
	return rando_world_item(&prepared_gen.world, location_named(fx, fx->field[ROW_TARGET]))
	       == item_named(fx, fx->field[ROW_ITEM]);
}

static int replay_row(Fixture *fx)
{
	char *parts[32];
	char config_field[FIXTURE_LINE_MAX], preset_field[FIXTURE_LINE_MAX];
	const char *query = fx->field[ROW_QUERY], *target = fx->field[ROW_TARGET];
	RandoVariant variant;
	RandoConfig config;
	RandoWorld world;
	RandoItems items;
	int n, i;

	if (strcmp(fx->field[ROW_VARIANT], "standard") == 0)
		variant = RANDO_VARIANT_STANDARD;
	else if (strcmp(fx->field[ROW_VARIANT], "open") == 0)
		variant = RANDO_VARIANT_OPEN;
	else if (strcmp(fx->field[ROW_VARIANT], "inverted") == 0)
		variant = RANDO_VARIANT_INVERTED;
	else if (strcmp(fx->field[ROW_VARIANT], "retro") == 0)
		variant = RANDO_VARIANT_RETRO;
	else
		fixture_fail(fx, "unknown variant");
	rando_config_init(&config);
	strcpy(config_field, fx->field[ROW_CONFIG]);
	n = split(fx, config_field, ',', parts, 32);
	if (strcmp(query, "prepared_item") == 0)
		return prepared_item(fx, parts, n);
	if (strcmp(query, "vanilla_dungeon_items") == 0) {
		apply_overrides(fx, &config, parts, n);
		rando_world_init(&world, variant, &config);
		return vanilla_dungeon_items(&world.config);
	}
	for (i = 0; i < n; i++)
		if (strncmp(parts[i], "logic=", 6) == 0) {
			/* apply_overrides splits its entry in place; the pass after init needs it whole. */
			char logic[FIXTURE_LINE_MAX], *entry = logic;

			strcpy(logic, parts[i]);
			apply_overrides(fx, &config, &entry, 1);
		}
	rando_world_init(&world, variant, &config);
	apply_overrides(fx, &world.config, parts, n);
	strcpy(preset_field, fx->field[ROW_PRESET]);
	n = split(fx, preset_field, '|', parts, 32);
	for (i = 0; i < n; i++) {
		char *item = value_of(fx, parts[i]);

		if (strncmp(parts[i], "boss:", 5) == 0) {
			RandoBossId boss = rando_boss_find(item);

			rando_world_set_boss(&world, region_named(fx, parts[i] + 5), RANDO_BOSS_SLOT_BOSS,
					     boss == RANDO_BOSS_NONE ? RANDO_BOSS_RULELESS : boss);
			continue;
		}
		if (!rando_world_set_item(&world, location_named(fx, parts[i]), item_named(fx, item)))
			fixture_fail(fx, "preset item refused by its location");
	}
	fixture_parse_items(fx, fx->field[ROW_COUNTS], fx->field[ROW_VALUES], &items);

	if (strcmp(query, "access") == 0)
		return rando_world_can_access(&world, location_named(fx, target), &items);
	if (strcmp(query, "fill") == 0)
		return rando_world_fill(&world, location_named(fx, target), item_named(fx, fx->field[ROW_ITEM]), &items);
	if (strcmp(query, "region_can_fill") == 0)
		return rando_world_region_can_fill(&world,
						   strcmp(target, "Region") == 0 ? bare_region(fx) : region_named(fx, target),
						   item_named(fx, fx->field[ROW_ITEM]));
	if (strcmp(query, "enter") == 0)
		return rando_world_region_can_enter(&world, region_named(fx, target), &items);
	if (strcmp(query, "complete") == 0)
		return rando_world_region_can_complete(&world, region_named(fx, target), &items);
	fixture_fail(fx, "unknown query");
	return 0;
}

int fixture_replay_rows(const char *test, const char *path, RowFilter filter, int *total)
{
	Fixture fx;
	int run = 0;

	*total = 0;
	fixture_open(&fx, path);
	while (fixture_next(&fx)) {
		int want, got;

		if (strcmp(fx.field[0], "row") != 0)
			continue;
		if (fx.nfields != ROW_FIELDS)
			fixture_fail(&fx, "row record without its 15 fields");
		(*total)++;
		if (!filter(fx.field[ROW_FILE], fx.field[ROW_METHOD]))
			continue;
		want = atoi(fx.field[ROW_ANSWER]);
		got = replay_row(&fx);
		if (got != want) {
			fprintf(stderr, "%s: %s::%s provider %s row %s: %s(%s%s%s) VT %d, C %d (test expects %s)\n", test,
				fx.field[ROW_FILE], fx.field[ROW_METHOD], fx.field[ROW_PROVIDER], fx.field[ROW_INDEX],
				fx.field[ROW_QUERY], fx.field[ROW_TARGET], strcmp(fx.field[ROW_ITEM], "-") ? ", " : "",
				strcmp(fx.field[ROW_ITEM], "-") ? fx.field[ROW_ITEM] : "", want, got,
				fx.field[ROW_EXPECTED]);
			fixture_fail(&fx, "row answer differs from VT");
		}
		run++;
	}
	fixture_close(&fx);
	return run;
}
