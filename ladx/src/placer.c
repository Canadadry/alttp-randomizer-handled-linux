#include "ladx_randomizer.h"
#include "randomizer_internal.h"

#include <stdio.h>
#include <string.h>

static bool value_is(const LadxSettings *s, LadxSettingId id, const char *name)
{
	return strcmp(ladx_setting_value_name(id, s->value[id]), name) == 0;
}

void ladx_placer_init(LadxPlacer *p, LadxLogic *logic, const LadxSettings *validated)
{
	p->logic = logic;
	p->beaks_first = (value_is(validated, LADX_SETTING_OWLSTATUES, "both") ||
			  value_is(validated, LADX_SETTING_OWLSTATUES, "dungeon")) &&
			 !value_is(validated, LADX_SETTING_DUNGEON_BEAKS, "keysanity");
	p->keys_first = !value_is(validated, LADX_SETTING_DUNGEON_KEYS, "keysanity");
	p->nightmare_keys_first = !value_is(validated, LADX_SETTING_NIGHTMARE_KEYS, "keysanity");
	p->goal_only = value_is(validated, LADX_SETTING_ACCESSIBILITY, "goal");
	ladx_itempool_clear(&p->pool);
	p->spot_count = 0;
	p->broken = false;
}

bool ladx_placer_add_item(LadxPlacer *p, int item, int count)
{
	return ladx_itempool_add(&p->pool, item, count);
}

bool ladx_placer_add_spot(LadxPlacer *p, int spot)
{
	if (spot < 0 || spot >= ladx_logic_spot_count(p->logic) || p->spot_count == LADX_LOGIC_SPOT_CAP)
		return false;
	p->spots[p->spot_count++] = spot;
	return true;
}

static bool has_option(const LadxPlacer *p, int spot, int item)
{
	return (p->option_set[spot][item / 32] >> (item % 32)) & 1u;
}

static int options_of(LadxPlacer *p, int spot)
{
	int n = ladx_logic_spot_options(p->logic, spot, p->options, LADX_PLACER_OPTION_CAP);

	if (n > LADX_PLACER_OPTION_CAP) {
		p->broken = true;
		return -1;
	}
	return n;
}

static void count_options(LadxPlacer *p, int spot, int delta)
{
	int n = options_of(p, spot), i;

	for (i = 0; i < n; i++)
		p->item_spot_count[p->options[i]] += delta;
}

static bool prepare(LadxPlacer *p)
{
	int spots = ladx_logic_spot_count(p->logic), i, j, n;

	for (i = 0; i < spots; i++) {
		memset(p->option_set[i], 0, sizeof p->option_set[i]);
		n = options_of(p, i);
		if (n < 0)
			return false;
		p->option_count[i] = n;
		for (j = 0; j < n; j++)
			p->option_set[i][p->options[j] / 32] |= 1u << (p->options[j] % 32);
		p->name_rank[i] = 0;
	}
	for (i = 0; i < spots; i++)
		for (j = 0; j < spots; j++)
			if (strcmp(ladx_logic_spot(p->logic, j)->name_id, ladx_logic_spot(p->logic, i)->name_id) < 0)
				p->name_rank[i]++;
	memset(p->item_spot_count, 0, sizeof p->item_spot_count);
	for (i = 0; i < p->spot_count; i++)
		count_options(p, p->spots[i], 1);
	return !p->broken;
}

static int pool_sum(const LadxPlacer *p)
{
	int i, item, count, sum = 0;

	for (i = 0; ladx_itempool_entry(&p->pool, i, &item, &count); i++)
		sum += count;
	return sum;
}

static bool has_new_places_to_explore(LadxPlacer *p)
{
	const LadxLogic *logic = p->logic;
	int i, k;

	ladx_explorer_init(&p->explorer);
	ladx_explorer_visit(&p->explorer, logic, ladx_logic_start(logic));
	for (i = 0; i < ladx_logic_location_count(logic); i++) {
		const LadxLocation *loc = ladx_logic_location(logic, i);

		if (!ladx_explorer_reached(&p->explorer, i))
			continue;
		for (k = 0; k < loc->spot_count; k++)
			if (ladx_logic_spot(logic, loc->spot_first + k)->item == LADX_ITEM_NONE)
				return true;
	}
	return false;
}

static unsigned long spot_score(const LadxPlacer *p, int spot)
{
	const LadxSpot *s = ladx_logic_spot(p->logic, spot);

	return ladx_logic_location(p->logic, s->location)->dungeon >= 0 ? 0 : (unsigned long)p->option_count[spot];
}

static bool can_still_place_item_pool(LadxPlacer *p)
{
	int n = p->spot_count, orders = 0, i, j, k, item, count;

	if (p->broken)
		return false;

	/* spots.sort(key=scoreSpot), stable: the list position breaks ties. */
	for (i = 0; i < n; i++) {
		unsigned long key = (spot_score(p, p->spots[i]) << 20) | ((unsigned long)p->name_rank[p->spots[i]] << 10) |
				    (unsigned long)i;

		for (j = i; j > 0 && p->sort_key[j - 1] > key; j--) {
			p->sort_key[j] = p->sort_key[j - 1];
			p->sorted[j] = p->sorted[j - 1];
		}
		p->sort_key[j] = key;
		p->sorted[j] = p->spots[i];
	}

	/* item_priority_order: the pool's keys, stable-sorted by spot count. */
	for (i = 0; ladx_itempool_entry(&p->pool, i, &item, &count); i++) {
		p->remaining[item] = count;
		for (j = orders; j > 0 && p->item_spot_count[p->priority[j - 1]] > p->item_spot_count[item]; j--)
			p->priority[j] = p->priority[j - 1];
		p->priority[j] = item;
		orders++;
	}

	for (i = 0; i < n; i++) {
		int spot = p->sorted[i];

		for (j = 0; j < orders && !has_option(p, spot, p->priority[j]); j++)
			;
		if (j == orders)
			return false;
		item = p->priority[j];
		if (--p->remaining[item] == 0) {
			for (k = j; k + 1 < orders; k++)
				p->priority[k] = p->priority[k + 1];
			orders--;
			if (orders > 0 && p->item_spot_count[p->priority[0]] == n)
				return true;
		}
	}
	return true;
}

static bool logic_still_valid(LadxPlacer *p)
{
	const LadxLogic *logic = p->logic;
	int i, item, count, reached = 0;

	if (p->spot_count > 0 && !has_new_places_to_explore(p))
		return false;
	if (!can_still_place_item_pool(p))
		return false;
	ladx_explorer_init(&p->explorer);
	for (i = 0; ladx_itempool_entry(&p->pool, i, &item, &count); i++)
		ladx_explorer_add_item(&p->explorer, item, count);
	ladx_explorer_visit(&p->explorer, logic, ladx_logic_start(logic));
	if (p->goal_only)
		return ladx_explorer_reached(&p->explorer, ladx_logic_windfish(logic));
	for (i = 0; i < ladx_logic_location_count(logic); i++)
		reached += ladx_explorer_reached(&p->explorer, i);
	return reached == ladx_logic_location_count(logic);
}

static bool remove_item(LadxPlacer *p, int item, LadxOutcome *outcome)
{
	if (!ladx_itempool_contains(&p->pool, item)) {
		ladx_outcome_key_error(outcome, item);
		return false;
	}
	ladx_itempool_remove(&p->pool, item, 1);
	return true;
}

static void remove_spot(LadxPlacer *p, int spot)
{
	int i;

	for (i = 0; i < p->spot_count && p->spots[i] != spot; i++)
		;
	for (; i + 1 < p->spot_count; i++)
		p->spots[i] = p->spots[i + 1];
	p->spot_count--;
	count_options(p, spot, -1);
}

static bool try_place(LadxPlacer *p, int spot, int item, LadxOutcome *outcome, bool *placed)
{
	ladx_logic_place(p->logic, spot, item);
	if (!remove_item(p, item, outcome))
		return false;
	remove_spot(p, spot);
	*placed = logic_still_valid(p);
	if (!*placed) {
		ladx_logic_place(p->logic, spot, LADX_ITEM_NONE);
		ladx_itempool_add(&p->pool, item, 1);
		p->spots[p->spot_count++] = spot;
		count_options(p, spot, 1);
	}
	return !p->broken;
}

static void index_error(LadxOutcome *outcome)
{
	outcome->kind = LADX_OUTCOME_CRASH;
	outcome->type = "IndexError";
	outcome->message = "Cannot choose from an empty sequence";
}

static bool place_specific_item(LadxPlacer *p, int item, LadxRng *rnd, LadxOutcome *outcome, bool *placed)
{
	int n = 0, i;
	size_t index;

	for (i = 0; i < p->spot_count; i++)
		if (has_option(p, p->spots[i], item))
			p->pick[n++] = p->spots[i];
	if (ladx_rng_choice(rnd, LADX_STAGE_PLACER_PLACE_SPECIFIC_ITEM, (size_t)n, &index) != LADX_RNG_OK) {
		index_error(outcome);
		return false;
	}
	return try_place(p, p->pick[index], item, outcome, placed);
}

static bool place_item(LadxPlacer *p, LadxRng *rnd, LadxOutcome *outcome, bool *placed)
{
	int spot, n, m = 0, i;
	size_t index;

	if (ladx_rng_choice(rnd, LADX_STAGE_PLACER_PLACE_ITEM, (size_t)p->spot_count, &index) != LADX_RNG_OK) {
		index_error(outcome);
		return false;
	}
	spot = p->spots[index];
	n = options_of(p, spot);
	if (n < 0)
		return false;
	for (i = 0; i < n; i++)
		if (ladx_itempool_contains(&p->pool, p->options[i]))
			p->options[m++] = p->options[i];
	*placed = false;
	if (m == 0)
		return true;
	ladx_rng_choice(rnd, LADX_STAGE_PLACER_PLACE_ITEM, (size_t)m, &index);
	return try_place(p, spot, p->options[index], outcome, placed);
}

static bool starts_with(int item, const char *prefix)
{
	return strncmp(ladx_item_name(item), prefix, strlen(prefix)) == 0;
}

static bool gather_keys(LadxPlacer *p, const char *prefix)
{
	int i, item, count;

	for (i = 0; ladx_itempool_entry(&p->pool, i, &item, &count); i++) {
		if (!starts_with(item, prefix))
			continue;
		for (; count > 0; count--) {
			if (p->dungeon_key_count == LADX_RNG_MAX_LIST)
				return false;
			p->dungeon_keys[p->dungeon_key_count++] = item;
		}
	}
	return true;
}

static void assertion(LadxOutcome *outcome, const char *message)
{
	outcome->kind = LADX_OUTCOME_CRASH;
	outcome->type = "AssertionError";
	outcome->message = message;
}

static LadxRandomizeStatus end(const LadxPlacer *p)
{
	return p->broken ? LADX_RANDOMIZE_BROKEN : LADX_RANDOMIZE_OK;
}

LadxRandomizeStatus ladx_placer_run(LadxPlacer *p, LadxRng *rnd, LadxOutcome *outcome)
{
	int bail_counter = 0, sum = pool_sum(p);
	bool placed;

	if (!prepare(p))
		return LADX_RANDOMIZE_BROKEN;
	if (sum != p->spot_count) {
		snprintf(outcome->text, sizeof outcome->text, "%d != %d", sum, p->spot_count);
		assertion(outcome, outcome->text);
		return LADX_RANDOMIZE_OK;
	}
	if (!logic_still_valid(p)) {
		if (p->broken)
			return LADX_RANDOMIZE_BROKEN;
		assertion(outcome, "Sanity check failed: False");
		return LADX_RANDOMIZE_OK;
	}

	p->dungeon_key_count = 0;
	if ((p->beaks_first && !gather_keys(p, "STONE_BEAK")) || (p->keys_first && !gather_keys(p, "KEY")) ||
	    (p->nightmare_keys_first && !gather_keys(p, "NIGHTMARE_KEY")))
		return LADX_RANDOMIZE_BROKEN;
	if (ladx_rng_shuffle(rnd, LADX_STAGE_PLACER_RUN, p->dungeon_keys, (size_t)p->dungeon_key_count,
			     sizeof p->dungeon_keys[0]) != LADX_RNG_OK)
		return LADX_RANDOMIZE_BROKEN;

	while (ladx_itempool_size(&p->pool) > 0) {
		if (pool_sum(p) != p->spot_count) {
			assertion(outcome, "");
			return LADX_RANDOMIZE_OK;
		}
		if (p->dungeon_key_count > 0) {
			if (!place_specific_item(p, p->dungeon_keys[0], rnd, outcome, &placed))
				return end(p);
			if (placed) {
				p->dungeon_key_count--;
				memmove(p->dungeon_keys, p->dungeon_keys + 1,
					(size_t)p->dungeon_key_count * sizeof p->dungeon_keys[0]);
			}
		} else if (!place_item(p, rnd, outcome, &placed)) {
			return end(p);
		}
		if (!placed) {
			if (++bail_counter > 10) {
				outcome->kind = LADX_OUTCOME_ERROR;
				outcome->type = NULL;
				outcome->message = "Failed to place an item for a bunch of retries";
				return LADX_RANDOMIZE_OK;
			}
		} else {
			bail_counter = 0;
		}
	}
	outcome->kind = LADX_OUTCOME_OK;
	outcome->type = NULL;
	outcome->message = NULL;
	return LADX_RANDOMIZE_OK;
}
