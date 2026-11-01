#ifndef RANDO_POSTFILL_H
#define RANDO_POSTFILL_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdbool.h>
#include <stdint.h>

#define RANDO_STRINGS_UNCLE 55
#define RANDO_STRINGS_TAVERN_MAN 51
#define RANDO_STRINGS_BLIND 77
#define RANDO_STRINGS_GANON_1 31
#define RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS 36
#define RANDO_STRINGS_TRIFORCE 39
#define RANDO_STRINGS_HINT 37

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

typedef struct RandoTexts {
	uint8_t uncle;
	uint8_t blind;
	uint8_t tavern_man;
	uint8_t ganon_1;
	uint8_t triforce;
	RandoBowHint bow_hint;
	RandoLocationId bow_locations[2];
	RandoLocationId silvers_location;
	uint8_t fake_silvers;
	RandoLocationId alternate_bow;
} RandoTexts;

typedef struct RandoCredits {
	uint8_t castle;
	uint8_t sanctuary;
	uint8_t kakariko;
	uint8_t lumberjacks;
	uint8_t bridge;
	uint8_t woods;
	uint8_t well;
	bool smithy_breadsmiths;
} RandoCredits;

#define RANDO_HINT_MISSING (-1)

int rando_hint_item_alternatives(RandoItemId item);
int rando_hint_location_alternatives(RandoLocationId location);

typedef struct RandoLocationHint {
	RandoLocationId location;
	RandoItemId item;
	uint8_t item_name;
	uint8_t location_name;
} RandoLocationHint;

bool rando_location_hint(const RandoWorld *world, RandoLocationId location, RandoPrng *prng, RandoLocationHint *out);

typedef enum RandoCollectionHint {
	RANDO_COLLECTION_HINT_NONE,
	RANDO_COLLECTION_HINT_ONE,
	RANDO_COLLECTION_HINT_MANY,
} RandoCollectionHint;

RandoCollectionHint rando_collection_hint(const RandoWorld *world, const RandoLocationId *locations, int n,
					  RandoPrng *prng, RandoLocationHint *out);

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
	RandoLocationHint location;
	uint8_t joke;
} RandoTileHint;

typedef struct RandoHints {
	bool on;
	RandoTileHint tiles[RANDO_TILE_COUNT];
} RandoHints;

#define RANDO_PRIZE_MUSIC_MAX 11

typedef struct RandoPrizeMusic {
	uint8_t count;
	RandoLocationId locations[RANDO_PRIZE_MUSIC_MAX];
	uint8_t music[RANDO_PRIZE_MUSIC_MAX];
} RandoPrizeMusic;

#define RANDO_RNG_BLOCK_SIZE 1024

typedef struct RandoPostFill {
	RandoTexts texts;
	RandoCredits credits;
	RandoHints hints;
	RandoPrizeMusic prize_music;
	uint8_t digging_game;
	uint8_t rng_block[RANDO_RNG_BLOCK_SIZE];
} RandoPostFill;

void rando_postfill_clear(RandoPostFill *post);

void rando_set_texts(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post);

void rando_randomize_credits(RandoPrng *prng, RandoPostFill *post);

void rando_apply_hints(const RandoWorld *world, RandoPrng *prng, const uint8_t *advancement, int n,
		       RandoPostFill *post);

void rando_prize_music(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post);

void rando_digging_game(RandoPrng *prng, RandoPostFill *post);

void rando_rng_block(RandoPrng *prng, RandoPostFill *post);

void rando_prize_packs_to_rom(RandoWorld *world, RandoPrng *prng);

#endif
