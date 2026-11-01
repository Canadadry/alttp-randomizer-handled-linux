#include "rando_playthrough.h"

#include <assert.h>
#include <string.h>

/* A pseudo-location is a world location id, or RANDO_LOCATION_COUNT + shop * RANDO_SHOP_SLOTS + slot.
 * VT's equipment slots only ever add the pre-collected items, so they get no id. Arrays are indexed
 * by it; walks go through pseudo_at, which skips the locations the world does not have. */
#define PSEUDO_COUNT RANDO_PLAYTHROUGH_ENTRY_MAX

typedef struct Playthrough {
	const RandoWorld *world;
	RandoWorld shadow;
	uint8_t shadow_shop_items[RANDO_SHOP_COUNT][RANDO_SHOP_SLOTS];
	uint16_t sphere_locations[PSEUDO_COUNT];
	int sphere_start[PSEUDO_COUNT + 2];
	int nspheres;
	bool in_sphere[PSEUDO_COUNT];
	bool required[PSEUDO_COUNT];
	uint16_t required_location[PSEUDO_COUNT], required_sphere[PSEUDO_COUNT];
	int nrequired;
	uint16_t required_keys[PSEUDO_COUNT];
	int nrequired_keys;
	bool in_order[PSEUDO_COUNT];
} Playthrough;

static const RandoItemId junk_items[] = {
	RANDO_ITEM_BlueShield, RANDO_ITEM_Boomerang, RANDO_ITEM_MirrorShield, RANDO_ITEM_RedBoomerang,
	RANDO_ITEM_RedShield, RANDO_ITEM_BombUpgrade5, RANDO_ITEM_BombUpgrade10, RANDO_ITEM_BombUpgrade50,
	RANDO_ITEM_ArrowUpgrade5, RANDO_ITEM_ArrowUpgrade10, RANDO_ITEM_ArrowUpgrade70, RANDO_ITEM_RedPotion,
	RANDO_ITEM_Bee, RANDO_ITEM_TenArrows, RANDO_ITEM_Bomb, RANDO_ITEM_ThreeBombs,
	RANDO_ITEM_OneRupee, RANDO_ITEM_FiveRupees, RANDO_ITEM_TwentyRupees, RANDO_ITEM_FiftyRupees,
	RANDO_ITEM_OneHundredRupees, RANDO_ITEM_ThreeHundredRupees, RANDO_ITEM_Heart, RANDO_ITEM_Rupoor,
};

static bool is_junk(RandoItemId item)
{
	size_t i;

	for (i = 0; i < sizeof junk_items / sizeof junk_items[0]; i++)
		if (junk_items[i] == item)
			return true;
	return false;
}

static bool is_shop(int p)
{
	return p >= RANDO_LOCATION_COUNT;
}

static RandoShopId shop_of(int p)
{
	return (RandoShopId)((p - RANDO_LOCATION_COUNT) / RANDO_SHOP_SLOTS);
}

static int slot_of(int p)
{
	return (p - RANDO_LOCATION_COUNT) % RANDO_SHOP_SLOTS;
}

static int pseudo_count(const Playthrough *pt)
{
	return rando_layout_location_count(pt->world->variant) + RANDO_SHOP_COUNT * RANDO_SHOP_SLOTS;
}

/* The k-th pseudo-location in VT's walk: the world's locations in getLocations() order, then the
 * shop slots. */
static int pseudo_at(const Playthrough *pt, int k)
{
	int n = rando_layout_location_count(pt->world->variant);

	return k < n ? (int)rando_layout_location(pt->world->variant, k) : RANDO_LOCATION_COUNT + (k - n);
}

static RandoItemId shadow_item(const Playthrough *pt, int p)
{
	return (RandoItemId)(is_shop(p) ? pt->shadow_shop_items[shop_of(p)][slot_of(p)] : pt->shadow.location_items[p]);
}

static void set_shadow_item(Playthrough *pt, int p, RandoItemId item)
{
	if (is_shop(p))
		pt->shadow_shop_items[shop_of(p)][slot_of(p)] = (uint8_t)item;
	else
		pt->shadow.location_items[p] = (uint8_t)item;
}

/* $world->getCollectableLocations()[name]->getItem(): a shop slot reads the real world's cached slot. */
static RandoItemId real_item(const Playthrough *pt, int p)
{
	const RandoWorld *world = pt->world;

	if (!is_shop(p))
		return rando_world_item(world, (RandoLocationId)p);
	return (RandoItemId)(world->collectable_computed ? world->collectable_shop_items[shop_of(p)][slot_of(p)]
							 : world->shop_items[shop_of(p)][slot_of(p)]);
}

/* A shadow location checked against locations. A shop slot's region is the real world's, so its
 * access always reads the real world. */
static bool can_access(const Playthrough *pt, const RandoWorld *locations, int p, const RandoItems *items)
{
	if (is_shop(p))
		return rando_world_can_access_shop(pt->world, shop_of(p), items);
	return rando_world_can_access_with(&pt->shadow, locations, (RandoLocationId)p, items);
}

/* The shadow's getCollectableLocations(); its collected state is always empty. */
static bool is_collectable(int p)
{
	RandoLocationClass c;

	if (is_shop(p))
		return true;
	c = rando_location_info((RandoLocationId)p)->location_class;
	return c != RANDO_LOCATION_CLASS_MEDALLION && c != RANDO_LOCATION_CLASS_FOUNTAIN;
}

/* The equipment slots, then every sphere. */
static void collectable_items(const Playthrough *pt, RandoItems *items)
{
	int i;

	rando_items_clear(items);
	for (i = 0; i < rando_world_pre_collected_count(&pt->shadow); i++)
		rando_items_add(items, rando_world_pre_collected_item(&pt->shadow, i));
	for (i = 0; i < pt->sphere_start[pt->nspheres]; i++)
		if (shadow_item(pt, pt->sphere_locations[i]) != RANDO_ITEM_NONE)
			rando_items_add(items, shadow_item(pt, pt->sphere_locations[i]));
}

/* World::getLocationSpheres on the shadow; sphere 0 holds only the equipment slots. */
static void build_spheres(Playthrough *pt)
{
	RandoItems my_items = pt->shadow.pre_collected, found;
	int k, p, n, start;

	memset(pt->in_sphere, 0, sizeof pt->in_sphere);
	pt->nspheres = 1;
	pt->sphere_start[0] = pt->sphere_start[1] = 0;
	do {
		start = n = pt->sphere_start[pt->nspheres];
		for (k = 0; k < pseudo_count(pt); k++) {
			p = pseudo_at(pt, k);
			if (!is_collectable(p) || shadow_item(pt, p) == RANDO_ITEM_NONE || pt->in_sphere[p]
			    || !can_access(pt, &pt->shadow, p, &my_items))
				continue;
			pt->sphere_locations[n++] = (uint16_t)p;
		}
		rando_items_clear(&found);
		for (p = start; p < n; p++) {
			pt->in_sphere[pt->sphere_locations[p]] = true;
			rando_items_add(&found, shadow_item(pt, pt->sphere_locations[p]));
		}
		rando_items_merge(&my_items, &my_items, &found);
		assert(pt->nspheres + 1 < PSEUDO_COUNT + 2);
		pt->sphere_start[++pt->nspheres] = n;
	} while (n > start);
}

static void add_required(Playthrough *pt, int p, int sphere)
{
	int k;

	assert(!pt->required[p]);
	pt->required[p] = true;
	pt->required_location[pt->nrequired] = (uint16_t)p;
	pt->required_sphere[pt->nrequired++] = (uint16_t)sphere;
	for (k = 0; k < pt->nrequired_keys && pt->required_keys[k] != sphere; k++)
		;
	if (k == pt->nrequired_keys)
		pt->required_keys[pt->nrequired_keys++] = (uint16_t)sphere;
}

/* VT's `break 2` leaves only the two inner loops: "put all higher items back" still runs for that
 * check sphere, and the later check spheres are skipped because p is now required. Keep it so. */
static void bubble(Playthrough *pt, int p, int sphere)
{
	RandoItems items;
	int nkeys = pt->nrequired_keys, k, r, nr;

	for (k = nkeys - 1; k >= 0; k--) {
		int check = pt->required_keys[k];

		if (check == sphere || pt->required[p])
			continue;
		for (r = 0; r < pt->nrequired; r++)
			if (pt->required_sphere[r] >= check)
				set_shadow_item(pt, pt->required_location[r], RANDO_ITEM_NONE);
		nr = pt->nrequired;
		for (r = 0; r < nr; r++) {
			int q = pt->required_location[r];
			RandoItemId temp;

			if (pt->required_sphere[r] != check)
				continue;
			temp = shadow_item(pt, q);
			set_shadow_item(pt, q, RANDO_ITEM_NONE);
			collectable_items(pt, &items);
			if (!can_access(pt, pt->world, q, &items)) {
				set_shadow_item(pt, p, real_item(pt, p));
				add_required(pt, p, sphere);
				break;
			}
			set_shadow_item(pt, q, temp);
		}
		for (r = 0; r < pt->nrequired; r++)
			set_shadow_item(pt, pt->required_location[r], real_item(pt, pt->required_location[r]));
	}
}

static void prune(Playthrough *pt)
{
	const RandoConfig *c = &pt->world->config;
	RandoItems items;
	int sphere, i;

	memset(pt->required, 0, sizeof pt->required);
	pt->nrequired = pt->nrequired_keys = 0;
	for (sphere = pt->nspheres - 1; sphere >= 1; sphere--) {
		for (i = pt->sphere_start[sphere]; i < pt->sphere_start[sphere + 1]; i++) {
			int p = pt->sphere_locations[i];
			RandoItemId pulled = shadow_item(pt, p);
			RandoItemClass item_class;

			if (pulled == RANDO_ITEM_NONE)
				continue;
			set_shadow_item(pt, p, RANDO_ITEM_NONE);
			item_class = rando_item_info(pulled)->item_class;
			if (((item_class == RANDO_ITEM_CLASS_MAP || item_class == RANDO_ITEM_CLASS_COMPASS)
			     && rando_dungeon_item_confined(c, item_class))
			    || is_junk(pulled))
				continue;
			collectable_items(pt, &items);
			if (!rando_world_win_condition(&pt->shadow, &items)) {
				set_shadow_item(pt, p, real_item(pt, p));
				add_required(pt, p, sphere);
				continue;
			}
			bubble(pt, p, sphere);
		}
	}
}

static bool round_skips(const RandoConfig *c, RandoItemId item)
{
	RandoItemClass item_class = rando_item_info(item)->item_class;

	return ((c->rom_generic_keys || !c->region_wild_keys) && item_class == RANDO_ITEM_CLASS_KEY)
		|| item_class == RANDO_ITEM_CLASS_MAP || item_class == RANDO_ITEM_CLASS_COMPASS
		|| item == RANDO_ITEM_RescueZelda;
}

static bool is_trade(int p)
{
	return !is_shop(p) && rando_location_info((RandoLocationId)p)->location_class == RANDO_LOCATION_CLASS_TRADE;
}

static void push_entry(const Playthrough *pt, RandoPlaythrough *out, int round, int p)
{
	RandoPlaythroughEntry *e;

	assert(out->nentries < RANDO_PLAYTHROUGH_ENTRY_MAX);
	e = &out->entries[out->nentries++];
	e->round = (uint16_t)round;
	e->region = is_shop(p) ? rando_shop_info(shop_of(p))->region
			       : rando_layout_location_region(pt->world->variant, (RandoLocationId)p);
	e->location = is_shop(p) ? RANDO_LOCATION_NONE : (RandoLocationId)p;
	e->shop = is_shop(p) ? shop_of(p) : RANDO_SHOP_NONE;
	e->slot = (uint8_t)(is_shop(p) ? slot_of(p) : 0);
	e->item = shadow_item(pt, p);
}

const char *rando_playthrough_entry_region(const RandoPlaythroughEntry *e)
{
	return rando_region_info(e->region)->spoiler_name;
}

/* VT counts the rounds, then takes one off for each round left with no entry once Trade locations
 * are filtered: that is the number of rounds holding an entry. */
static void count_rounds_and_regions(RandoPlaythrough *out)
{
	int i, j;

	out->longest_item_chain = 0;
	out->regions_visited = out->nequipped > 0;
	for (i = 0; i < out->nentries; i++) {
		const RandoPlaythroughEntry *e = &out->entries[i];

		if (i == 0 || out->entries[i - 1].round != e->round)
			out->longest_item_chain++;
		for (j = i - 1; j >= 0 && out->entries[j].round == e->round; j--)
			if (strcmp(rando_playthrough_entry_region(&out->entries[j]), rando_playthrough_entry_region(e)) == 0)
				break;
		out->regions_visited += j < 0 || out->entries[j].round != e->round;
	}
}

static void replay(Playthrough *pt, RandoPlaythrough *out)
{
	const RandoConfig *c = &pt->world->config;
	RandoItems my_items = pt->shadow.pre_collected, found;
	int longest = 1, round_size = 0, k, p, nfound;
	bool available[PSEUDO_COUNT];

	memset(pt->in_order, 0, sizeof pt->in_order);
	out->nentries = 0;
	do {
		if (round_size > 0)
			longest++;
		round_size = 0;
		nfound = 0;
		rando_items_clear(&found);
		for (k = 0; k < pseudo_count(pt); k++) {
			p = pseudo_at(pt, k);
			available[p] = is_collectable(p) && shadow_item(pt, p) != RANDO_ITEM_NONE
				       && !pt->in_order[p] && can_access(pt, pt->world, p, &my_items);
			if (available[p]) {
				rando_items_add(&found, shadow_item(pt, p));
				nfound++;
			}
		}
		for (k = 0; k < pseudo_count(pt); k++) {
			p = pseudo_at(pt, k);
			if (!available[p])
				continue;
			pt->in_order[p] = true;
			if (round_skips(c, shadow_item(pt, p)))
				continue;
			round_size++;
			if (!is_trade(p))
				push_entry(pt, out, longest, p);
		}
		rando_items_merge(&my_items, &my_items, &found);
	} while (nfound > 0);
	count_rounds_and_regions(out);
}

void rando_playthrough(const RandoWorld *world, RandoPlaythrough *out)
{
	Playthrough pt;
	int i;

	pt.world = world;
	rando_world_copy(&pt.shadow, world);
	memcpy(pt.shadow_shop_items, pt.shadow.shop_items, sizeof pt.shadow_shop_items);
	for (i = 0; i < rando_layout_location_count(world->variant); i++) {
		RandoLocationId p = rando_layout_location(world->variant, i);

		if (is_junk(shadow_item(&pt, p)))
			set_shadow_item(&pt, p, RANDO_ITEM_NONE);
	}
	build_spheres(&pt);
	prune(&pt);

	out->nequipped = 0;
	for (i = 0; i < rando_world_pre_collected_count(&pt.shadow); i++) {
		RandoItemId item = rando_world_pre_collected_item(&pt.shadow, i);
		RandoItemClass c = rando_item_info(item)->item_class;

		if (c != RANDO_ITEM_CLASS_UPGRADE_ARROW && c != RANDO_ITEM_CLASS_UPGRADE_BOMB
		    && c != RANDO_ITEM_CLASS_HEALTH && c != RANDO_ITEM_CLASS_EVENT)
			out->equipped[out->nequipped++] = item;
	}
	replay(&pt, out);
}
