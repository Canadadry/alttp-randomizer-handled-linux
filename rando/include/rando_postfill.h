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

/* Caller-owned, no pointers: every post-fill choice, as ids and indices, for the ROM writer. */
typedef struct RandoPostFill {
	RandoTexts texts;
	RandoCredits credits;
} RandoPostFill;

void rando_postfill_clear(RandoPostFill *post);

/* Randomizer::setTexts with spoil.BootsLocation off. Draws 55 + 77 + 51 + 31, then min(2, progressive
 * bows in the world), then 36 on the fake branch, then 39. */
void rando_set_texts(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post);

/* Randomizer::randomizeCredits: 3 + 4 + 49 + 7, the smithy coin, then 3 + 3 + 8; 78 draws, always. */
void rando_randomize_credits(RandoPrng *prng, RandoPostFill *post);

#endif
