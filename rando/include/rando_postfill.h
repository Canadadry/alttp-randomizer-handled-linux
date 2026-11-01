#ifndef RANDO_POSTFILL_H
#define RANDO_POSTFILL_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

/* The alternatives of VT's strings/<file>.txt at 219fcafd, as Randomizer::getTextArray splits them. */
#define RANDO_STRINGS_UNCLE 55
#define RANDO_STRINGS_TAVERN_MAN 51
#define RANDO_STRINGS_BLIND 77
#define RANDO_STRINGS_GANON_1 31
#define RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS 36
#define RANDO_STRINGS_TRIFORCE 39
/* HintService's joke hints. */
#define RANDO_STRINGS_HINT 37

/* The alternatives of each Randomizer::randomizeCredits pick at 219fcafd. */
#define RANDO_CREDITS_CASTLE 3
#define RANDO_CREDITS_SANCTUARY 4
#define RANDO_CREDITS_KAKARIKO 49
#define RANDO_CREDITS_LUMBERJACKS 7
#define RANDO_CREDITS_BRIDGE 3
#define RANDO_CREDITS_WOODS 3
#define RANDO_CREDITS_WELL 8

typedef enum RandoBowHint {
	RANDO_BOW_HINT_ALTERNATE,
	RANDO_BOW_HINT_SILVERS,
	RANDO_BOW_HINT_FAKE,
} RandoBowHint;

/* setTexts' choices; each text is an index into its string file's alternatives. */
typedef struct RandoTexts {
	uint8_t uncle;
	uint8_t blind;
	uint8_t tavern_man;
	uint8_t ganon_1;
	uint8_t triforce;
	RandoBowHint bow_hint;
	/* ALTERNATE only, in VT's pop order; RANDO_LOCATION_NONE otherwise. */
	RandoLocationId bow_locations[2];
	/* SILVERS only; RANDO_LOCATION_NONE otherwise. */
	RandoLocationId silvers_location;
	/* FAKE only: drawn even under crowd_control, whose text overrides it. */
	uint8_t fake_silvers;
	/* The location whose ProgressiveBow VT swaps for the 0x65 alternate; the world keeps the plain bow.
	 * RANDO_LOCATION_NONE unless ALTERNATE. */
	RandoLocationId alternate_bow;
} RandoTexts;

/* randomizeCredits' choices; each is an index into its pick's alternatives, in VT's order. */
typedef struct RandoCredits {
	uint8_t castle;
	uint8_t sanctuary;
	/* The name before "'s homecoming". */
	uint8_t kakariko;
	uint8_t lumberjacks;
	uint8_t bridge;
	uint8_t woods;
	uint8_t well;
	/* The coin drew 1: "the dwarven breadsmiths"; otherwise the scene keeps its default text. */
	bool smithy_breadsmiths;
} RandoCredits;

/* hint.item.<raw name> or hint.location.<name> at 219fcafd has no key: __() returns the key, with no draw. */
#define RANDO_HINT_MISSING (-1)

/* The alternatives of hint.item.<raw name>: 0 for a plain string, else the array's length. Asserts the
 * id is below the count. */
int rando_hint_item_alternatives(RandoItemId item);
/* Likewise for hint.location.<name>. */
int rando_hint_location_alternatives(RandoLocationId location);

/* One Location::getHint, as ids and indices. */
typedef struct RandoLocationHint {
	RandoLocationId location;
	/* The item whose hint.item text names it: KeyGK for a key under rom.genericKeys, an alias's target. */
	RandoItemId item;
	/* The picked alternative of hint.item and of hint.location; 0 when that text is no array. */
	uint8_t item_name;
	uint8_t location_name;
} RandoLocationHint;

/* Location::getHint: draws the item's alternatives, then the location's, each only when it has some
 * (rando_hint_*_alternatives above 0). False, with no draw and out unchanged, when location holds no item. */
bool rando_location_hint(const RandoWorld *world, RandoLocationId location, RandoPrng *prng, RandoLocationHint *out);

typedef enum RandoCollectionHint {
	RANDO_COLLECTION_HINT_NONE,
	RANDO_COLLECTION_HINT_ONE,
	RANDO_COLLECTION_HINT_MANY,
} RandoCollectionHint;

/* LocationCollection::getHint over locations[0..n), in order. NONE, no draw: no location holds an item.
 * ONE: exactly one does, and out is its rando_location_hint. MANY, no draw: VT's name lookups use
 * getName(), whose ":<world id>" suffix misses every key; out->location is the first location holding an
 * item, out->item RANDO_ITEM_NONE and both indices 0. out is unchanged on NONE. */
RandoCollectionHint rando_collection_hint(const RandoWorld *world, const RandoLocationId *locations, int n,
					  RandoPrng *prng, RandoLocationHint *out);

/* HintService's telepathic tiles, in the order VT lists them. */
typedef enum RandoTile {
	RANDO_TILE_EASTERN_PALACE,
	RANDO_TILE_TOWER_OF_HERA_FLOOR_4,
	RANDO_TILE_SPECTACLE_ROCK,
	RANDO_TILE_SWAMP_ENTRANCE,
	RANDO_TILE_THIEVES_TOWN_UPSTAIRS,
	RANDO_TILE_MISERY_MIRE,
	RANDO_TILE_PALACE_OF_DARKNESS,
	RANDO_TILE_DESERT_BONK_TORCH_ROOM,
	RANDO_TILE_CASTLE_TOWER,
	RANDO_TILE_ICE_LARGE_ROOM,
	RANDO_TILE_TURTLE_ROCK,
	RANDO_TILE_ICE_ENTRACE,
	RANDO_TILE_ICE_STALFOS_KNIGHTS_ROOM,
	RANDO_TILE_TOWER_OF_HERA_ENTRANCE,
	RANDO_TILE_SOUTH_EAST_DARKWORLD_CAVE,
	RANDO_TILE_COUNT,
} RandoTile;

typedef enum RandoTileHintKind {
	RANDO_TILE_HINT_NONE,
	RANDO_TILE_HINT_LOCATION,
	RANDO_TILE_HINT_JOKE,
} RandoTileHintKind;

typedef struct RandoTileHint {
	RandoTileHintKind kind;
	/* LOCATION only. */
	RandoLocationHint location;
	/* JOKE only: an index into strings/hint.txt's alternatives. */
	uint8_t joke;
} RandoTileHint;

/* applyHints' choices. Hints on fills every tile; off leaves every tile NONE. */
typedef struct RandoHints {
	bool on;
	RandoTileHint tiles[RANDO_TILE_COUNT];
} RandoHints;

/* The prize locations of the regions whose VT class has music_addresses: ten dungeons and Agahnim 2. */
#define RANDO_PRIZE_MUSIC_MAX 11

/* writeItem's prize music under rom.mapOnPickup, in getRegions() order; count 0 without it, where the
 * music follows the prize with no draw. */
typedef struct RandoPrizeMusic {
	uint8_t count;
	RandoLocationId locations[RANDO_PRIZE_MUSIC_MAX];
	/* 0x11 or 0x16, for every music address of the location's region. */
	uint8_t music[RANDO_PRIZE_MUSIC_MAX];
} RandoPrizeMusic;

/* Rom::writeRNGBlock's bytes at 0x178000. */
#define RANDO_RNG_BLOCK_SIZE 1024

/* Caller-owned, no pointers: every post-fill choice, as ids and indices, for the ROM writer. */
typedef struct RandoPostFill {
	RandoTexts texts;
	RandoCredits credits;
	RandoHints hints;
	RandoPrizeMusic prize_music;
	/* setDiggingGameRng's prize count, 1 to 30. */
	uint8_t digging_game;
	/* Each byte is pack('C') of a draw in 0..256: a drawn 256 is stored as 0. */
	uint8_t rng_block[RANDO_RNG_BLOCK_SIZE];
} RandoPostFill;

void rando_postfill_clear(RandoPostFill *post);

/* Randomizer::setTexts with spoil.BootsLocation off. Draws 55 + 77 + 51 + 31, then min(2, progressive
 * bows in the world), then 36 on the fake branch, then 39. */
void rando_set_texts(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post);

/* Randomizer::randomizeCredits: 3 + 4 + 49 + 7, the smithy coin, then 3 + 3 + 8; 78 draws, always. */
void rando_randomize_credits(RandoPrng *prng, RandoPostFill *post);

/* HintService::applyHints over advancement[0..n), the list as prepareWorld left it; no draw with
 * spoil.Hints off. Call after rando_set_texts: its alternate bow is no ProgressiveBow to VT. A pair pick
 * never hints (VT's getName() suffix); random() over no location draws nothing. */
void rando_apply_hints(const RandoWorld *world, RandoPrng *prng, const uint8_t *advancement, int n,
		       RandoPostFill *post);

/* Prize::writeItem over World::writeToRom's walk: under rom.mapOnPickup, 2 draws per prize location of a
 * region with music addresses, Agahnim 2's event included, empty or not; no draw otherwise. */
void rando_prize_music(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post);

/* World::writeToRom's setDiggingGameRng(get_random_int(1, 30)): 1 draw. */
void rando_digging_game(RandoPrng *prng, RandoPostFill *post);

/* World::writeToRom's writeRNGBlock over get_random_int(0, 0x100): 1024 draws. */
void rando_rng_block(RandoPrng *prng, RandoPostFill *post);

/* World::writePrizePacksToRom's draws: getDropsPool's fy_shuffle of all 63 drops at VT's drop counts, even
 * when fewer slots are empty; then the empty slots, in getPrizePacks() order, take the pool from its start.
 * 63 draws. */
void rando_prize_packs_to_rom(RandoWorld *world, RandoPrng *prng);

#endif
