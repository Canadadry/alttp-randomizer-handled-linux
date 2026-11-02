#include "postfill_draw.h"
#include "rando_text.h"
#include "text_strings.h"

#include <assert.h>
#include <string.h>

#define SEED_TEXT_MAX 1024

typedef struct Str {
	char text[SEED_TEXT_MAX];
	size_t n;
} Str;

static void str_add(Str *s, const char *text)
{
	size_t n = strlen(text);

	assert(n < sizeof s->text - s->n);
	memcpy(s->text + s->n, text, n + 1);
	s->n += n;
}

static void str_int(Str *s, int value)
{
	char digits[12];
	int i = (int)sizeof digits - 1;
	unsigned u = value < 0 ? 0u - (unsigned)value : (unsigned)value;

	digits[i] = '\0';
	do {
		digits[--i] = (char)('0' + u % 10);
		u /= 10;
	} while (u != 0);
	if (value < 0)
		digits[--i] = '-';
	str_add(s, digits + i);
}

static const char *region_name(const RandoGen *gen, RandoLocationId location)
{
	assert(location < RANDO_LOCATION_COUNT);
	return rando_region_info(rando_layout_location_region(gen->world.variant, location))->spoiler_name;
}

static const char *item_region(const RandoGen *gen, RandoItemId item)
{
	return region_name(gen, rando_first_with(&gen->world, item));
}

static void bow_hint(const RandoGen *gen, RandoLocationId location, Str *s)
{
	const char *region = region_name(gen, location);

	str_add(s, "Did you find\nthe arrows in\n");
	str_add(s, strcmp(region, "Ganons Tower") == 0 ? "My tower?" : region);
}

static void silvers_hint(const RandoGen *gen, int which, Str *s)
{
	const RandoTexts *t = &gen->post.texts;

	switch (t->bow_hint) {
	case RANDO_BOW_HINT_ALTERNATE:
		bow_hint(gen, t->bow_locations[which], s);
		break;
	case RANDO_BOW_HINT_SILVERS:
		bow_hint(gen, t->silvers_location, s);
		break;
	case RANDO_BOW_HINT_FAKE:
		if (gen->world.config.item_pool == RANDO_ITEM_POOL_CROWD_CONTROL) {
			str_add(s, "Chat said no\nto Silvers.\nIt's over Hero");
		} else {
			assert(t->fake_silvers < RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS);
			str_add(s, rando_strings_ganon_phase_3_no_silvers[t->fake_silvers]);
		}
		break;
	}
}

static const char *const dingus = "You think you\nare ready to\nface me?\n\nI will not die\n\nunless you\ncomplete your\n"
				  "goals. Dingus!";

static bool sign_ganon(const RandoConfig *c, Str *s)
{
	const char *count = c->crystals_ganon == 1 ? " Crystal" : " Crystals";

	switch (c->goal) {
	case RANDO_GOAL_GANON:
		str_add(s, "To beat Ganon you must collect ");
		str_int(s, c->crystals_ganon);
		str_add(s, count);
		str_add(s, " and defeat his minion at the top of his tower.");
		return true;
	case RANDO_GOAL_FAST_GANON:
		str_add(s, "You need ");
		str_int(s, c->crystals_ganon);
		str_add(s, count);
		str_add(s, " to beat Ganon.");
		return true;
	case RANDO_GOAL_GANONHUNT:
		str_add(s, "To beat Ganon you must collect ");
		str_int(s, c->item_goal_required);
		str_add(s, " Triforce Pieces.");
		return true;
	case RANDO_GOAL_PEDESTAL:
		str_add(s, "You need to get to the pedestal... Dingus!");
		return true;
	case RANDO_GOAL_TRIFORCE_HUNT:
		str_add(s, "Go find the Triforce pieces... Dingus!");
		return true;
	case RANDO_GOAL_DUNGEONS:
		str_add(s, "You need to defeat all of Ganon's bosses.");
		return true;
	case RANDO_GOAL_COMPLETIONIST:
		str_add(s, "You need to collect EVERY item and defeat EVERY boss.");
		return true;
	}
	return false;
}

static void ganon_fall_in_alt(const RandoConfig *c, Str *s)
{
	switch (c->goal) {
	case RANDO_GOAL_PEDESTAL:
		str_add(s, "You cannot\nkill me. You\nshould go for\nyour real goal\nIt's on the\npedestal.\n\nYou dingus!\n");
		break;
	case RANDO_GOAL_TRIFORCE_HUNT:
		str_add(s, "So you thought\nyou could come\nhere and beat\nme? I have\nhidden the\nTriforce\npieces well.\n"
			   "Without them,\nyou can't win!");
		break;
	default:
		str_add(s, dingus);
		break;
	}
}

static const RandoTextId tile_texts[RANDO_TILE_COUNT] = {
	[RANDO_TILE_EASTERN_PALACE] = RANDO_TEXT_telepathic_tile_eastern_palace,
	[RANDO_TILE_TOWER_OF_HERA_FLOOR_4] = RANDO_TEXT_telepathic_tile_tower_of_hera_floor_4,
	[RANDO_TILE_SPECTACLE_ROCK] = RANDO_TEXT_telepathic_tile_spectacle_rock,
	[RANDO_TILE_SWAMP_ENTRANCE] = RANDO_TEXT_telepathic_tile_swamp_entrance,
	[RANDO_TILE_THIEVES_TOWN_UPSTAIRS] = RANDO_TEXT_telepathic_tile_thieves_town_upstairs,
	[RANDO_TILE_MISERY_MIRE] = RANDO_TEXT_telepathic_tile_misery_mire,
	[RANDO_TILE_PALACE_OF_DARKNESS] = RANDO_TEXT_telepathic_tile_palace_of_darkness,
	[RANDO_TILE_DESERT_BONK_TORCH_ROOM] = RANDO_TEXT_telepathic_tile_desert_bonk_torch_room,
	[RANDO_TILE_CASTLE_TOWER] = RANDO_TEXT_telepathic_tile_castle_tower,
	[RANDO_TILE_ICE_LARGE_ROOM] = RANDO_TEXT_telepathic_tile_ice_large_room,
	[RANDO_TILE_TURTLE_ROCK] = RANDO_TEXT_telepathic_tile_turtle_rock,
	[RANDO_TILE_ICE_ENTRACE] = RANDO_TEXT_telepathic_tile_ice_entrace,
	[RANDO_TILE_ICE_STALFOS_KNIGHTS_ROOM] = RANDO_TEXT_telepathic_tile_ice_stalfos_knights_room,
	[RANDO_TILE_TOWER_OF_HERA_ENTRANCE] = RANDO_TEXT_telepathic_tile_tower_of_hera_entrance,
	[RANDO_TILE_SOUTH_EAST_DARKWORLD_CAVE] = RANDO_TEXT_telepathic_tile_south_east_darkworld_cave,
};

static void hint_name(Str *s, const RandoHintName *hint, const char *kind, const char *name, int index)
{
	if (hint->names == NULL) {
		str_add(s, "hint.");
		str_add(s, kind);
		str_add(s, ".");
		str_add(s, name);
		return;
	}
	assert(index == 0 || index < hint->count);
	str_add(s, hint->names[index]);
}

static void location_hint(const RandoLocationHint *hint, Str *s)
{
	assert(hint->item < RANDO_ITEM_COUNT && hint->location < RANDO_LOCATION_COUNT);
	hint_name(s, &rando_hint_items[hint->item], "item", rando_item_info(hint->item)->name, hint->item_name);
	str_add(s, " ");
	hint_name(s, &rando_hint_locations[hint->location], "location", rando_location_info(hint->location)->name,
		  hint->location_name);
	if (s->text[0] >= 'a' && s->text[0] <= 'z')
		s->text[0] = (char)(s->text[0] - 'a' + 'A');
}

static bool tile_hint(const RandoGen *gen, RandoTextId id, Str *s)
{
	const RandoHints *h = &gen->post.hints;
	const RandoTileHint *hint;
	int tile;

	for (tile = 0; tile < RANDO_TILE_COUNT && tile_texts[tile] != id; tile++)
		;
	if (tile == RANDO_TILE_COUNT || !h->on)
		return false;
	hint = &h->tiles[tile];
	assert(hint->kind != RANDO_TILE_HINT_NONE);
	if (hint->kind == RANDO_TILE_HINT_JOKE) {
		assert(hint->joke < RANDO_STRINGS_HINT);
		str_add(s, rando_strings_hint[hint->joke]);
	} else {
		location_hint(&hint->location, s);
	}
	return true;
}

typedef struct ItemText {
	RandoTextId id;
	RandoLocationClass location_class;
	const char *const *texts;
} ItemText;

static const ItemText item_texts[] = {
	{RANDO_TEXT_tablet_ether_book, RANDO_LOCATION_CLASS_DROP_ETHER, rando_text_ether},
	{RANDO_TEXT_tablet_bombos_book, RANDO_LOCATION_CLASS_DROP_BOMBOS, rando_text_bombos},
	{RANDO_TEXT_mastersword_pedestal_translated, RANDO_LOCATION_CLASS_PEDESTAL, rando_text_pedestal},
};

static RandoItemId class_item(const RandoWorld *world, RandoLocationClass location_class)
{
	int n = rando_layout_location_count(world->variant), i;

	for (i = 0; i < n; i++) {
		RandoLocationId location = rando_layout_location(world->variant, i);

		if (rando_location_info(location)->location_class == location_class) {
			RandoItemId item = rando_world_item(world, location);

			return item == RANDO_ITEM_NONE ? RANDO_ITEM_Nothing : item;
		}
	}
	assert(!"no location of the class");
	return RANDO_ITEM_Nothing;
}

static bool item_text(const RandoGen *gen, RandoTextId id, Str *s)
{
	size_t i;

	for (i = 0; i < sizeof item_texts / sizeof item_texts[0]; i++)
		if (item_texts[i].id == id) {
			RandoItemId item = class_item(&gen->world, item_texts[i].location_class);
			const char *text;

			if (gen->world.config.rom_generic_keys && rando_item_info(item)->item_class == RANDO_ITEM_CLASS_KEY)
				item = RANDO_ITEM_KeyGK;
			text = item_texts[i].texts[item];
			str_add(s, text != NULL ? text : "Don't waste\nyour time!");
			return true;
		}
	return false;
}

typedef struct InvertedText {
	RandoTextId id;
	const char *text;
	bool pause;
} InvertedText;

static const InvertedText inverted_texts[] = {
	{RANDO_TEXT_sign_path_to_death_mountain, "\xE2\x86\x92 Bumper Cave\nYou need Cape and Mirror, but not Hookshot", true},
	{RANDO_TEXT_sign_bumper_cave, "Cave to lost, old man.\nGood luck.", true},
	{RANDO_TEXT_sign_east_of_bomb_shop, "\n\xE2\x86\x90 Your House", true},
	{RANDO_TEXT_sign_east_of_links_house, "\n\xE2\x86\x90 Bomb Shoppe", true},
	{RANDO_TEXT_kiki_leaving_screen, "{NOTEXT}", false},
	{RANDO_TEXT_dark_sanctuary, "{NOTEXT}", false},
	{RANDO_TEXT_dark_sanctuary_yes, "{NOTEXT}", false},
	{RANDO_TEXT_dark_sanctuary_no, "If you want that healing you're gonna need 20 rupees.", true},
	{RANDO_TEXT_menu_start_2, "{MENU}\n{SPEED0}\n\xE2\x89\xA5@'s House\n Dark Chapel\n{CHOICE3}", false},
	{RANDO_TEXT_menu_start_3, "{MENU}\n{SPEED0}\n\xE2\x89\xA5@'s House\n Dark Chapel\n Dark Mountain\n{CHOICE2}", false},
	{RANDO_TEXT_intro_main,
	 "{INTRO}\n Episode  III\n{PAUSE3}\n A Link to\n   the Past\n{PAUSE3}\nInverted\n  Randomizer\n{PAUSE3}\n"
	 "After mostly disregarding what happened in the first two games,\n{PAUSE3}\n"
	 "Link has been transported to the Dark World\n{PAUSE3}\nWhile he was slumbering,\n{PAUSE3}\n"
	 "Whatever will happen?\n{PAUSE3}\n{CHANGEPIC}\nGanon has moved around all the items in Hyrule.\n{PAUSE7}\n"
	 "You will have to find all the items necessary to beat Ganon.\n{PAUSE7}\n"
	 "This is your chance to be a hero.\n{PAUSE3}\n{CHANGEPIC}\n"
	 "You must get the 7 crystals to beat Ganon.\n{PAUSE9}\n{CHANGEPIC}",
	 false},
};

static bool inverted_text(const RandoGen *gen, RandoTextId id, Str *s, bool *pause)
{
	size_t i;

	if (gen->world.config.mode_state != RANDO_STATE_INVERTED)
		return false;
	for (i = 0; i < sizeof inverted_texts / sizeof inverted_texts[0]; i++)
		if (inverted_texts[i].id == id) {
			str_add(s, inverted_texts[i].text);
			*pause = inverted_texts[i].pause;
			return true;
		}
	return false;
}

static bool seed_string(const RandoGen *gen, RandoTextId id, Str *s, bool *pause)
{
	const RandoTexts *t = &gen->post.texts;
	const RandoConfig *c = &gen->world.config;

	*pause = true;
	switch (id) {
	case RANDO_TEXT_uncle_leaving_text:
		assert(t->uncle < RANDO_STRINGS_UNCLE);
		str_add(s, rando_strings_uncle[t->uncle]);
		return true;
	case RANDO_TEXT_sahasrahla_bring_courage:
		str_add(s, "Want something\nfor free? Go\nearn the green\npendant in\n");
		str_add(s, item_region(gen, RANDO_ITEM_PendantOfCourage));
		str_add(s, "\nand I'll give\nyou something.");
		return true;
	case RANDO_TEXT_bomb_shop:
		str_add(s, "bring me the\ncrystals from\n");
		str_add(s, item_region(gen, RANDO_ITEM_Crystal5));
		str_add(s, "\nand\n");
		str_add(s, item_region(gen, RANDO_ITEM_Crystal6));
		str_add(s, "\nso I can make\na big bomb!");
		return true;
	case RANDO_TEXT_ganon_phase_3_no_silvers:
		silvers_hint(gen, 0, s);
		return true;
	case RANDO_TEXT_ganon_phase_3_no_silvers_alt:
		silvers_hint(gen, 1, s);
		return true;
	case RANDO_TEXT_sign_ganons_tower:
		if (c->crystals_tower >= 7)
			return false;
		str_add(s, "You need ");
		str_int(s, c->crystals_tower);
		str_add(s, c->crystals_tower == 1 ? " crystal to enter." : " crystals to enter.");
		return true;
	case RANDO_TEXT_sign_ganon:
		return sign_ganon(c, s);
	case RANDO_TEXT_ganon_fall_in_alt:
		ganon_fall_in_alt(c, s);
		return true;
	case RANDO_TEXT_murahdahla:
		if (c->goal != RANDO_GOAL_TRIFORCE_HUNT)
			return false;
		str_add(s, "Hello @. I\nam Murahdahla, brother of\nSahasrahla and Aginah. Behold the power of\n"
			   "invisibility.\n\n\n\n\xE2\x80\xA6 \xE2\x80\xA6 \xE2\x80\xA6\n\nWait! you can see me? I knew I "
			   "should have\nhidden in  a hollow tree. If you bring\n");
		str_int(s, c->item_goal_required);
		str_add(s, " triforce pieces, I can reassemble it.");
		return true;
	case RANDO_TEXT_blind_by_the_light:
		assert(t->blind < RANDO_STRINGS_BLIND);
		str_add(s, rando_strings_blind[t->blind]);
		return true;
	case RANDO_TEXT_kakariko_tavern_fisherman:
		assert(t->tavern_man < RANDO_STRINGS_TAVERN_MAN);
		str_add(s, rando_strings_tavern_man[t->tavern_man]);
		return true;
	case RANDO_TEXT_ganon_fall_in:
		assert(t->ganon_1 < RANDO_STRINGS_GANON_1);
		str_add(s, rando_strings_ganon_1[t->ganon_1]);
		return true;
	case RANDO_TEXT_ganon_phase_3_alt:
		str_add(s, "Got wax in\nyour ears?\nI cannot die!");
		return true;
	case RANDO_TEXT_end_triforce:
		assert(t->triforce < RANDO_STRINGS_TRIFORCE);
		str_add(s, "{NOBORDER}\n");
		str_add(s, rando_strings_triforce[t->triforce]);
		return true;
	case RANDO_TEXT_sign_north_of_links_house:
		if (gen->post.hints.on)
			return false;
		str_add(s, "Randomizer v31\n\n>    -veetorp");
		return true;
	default:
		return tile_hint(gen, id, s) || item_text(gen, id, s) || inverted_text(gen, id, s, pause);
	}
}

size_t rando_text_seed(const RandoGen *gen, RandoTextId id, uint8_t out[RANDO_TEXT_ENTRY_MAX], bool *seed_set)
{
	Str s;
	bool pause, set;

	assert(id < RANDO_TEXT_COUNT);
	s.n = 0;
	s.text[0] = '\0';
	set = seed_string(gen, id, &s, &pause);
	if (seed_set != NULL)
		*seed_set = set;
	return set ? rando_text_encode(s.text, pause, out) : rando_text_default(id, out);
}
