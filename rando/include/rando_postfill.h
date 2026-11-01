#ifndef RANDO_POSTFILL_H
#define RANDO_POSTFILL_H

#include "rando_prng.h"
#include "rando_world.h"

#include <stdint.h>

/* The alternatives of VT's strings/<file>.txt at 219fcafd, as Randomizer::getTextArray splits them. */
#define RANDO_STRINGS_UNCLE 55
#define RANDO_STRINGS_TAVERN_MAN 51
#define RANDO_STRINGS_BLIND 77
#define RANDO_STRINGS_GANON_1 31
#define RANDO_STRINGS_GANON_PHASE_3_NO_SILVERS 36
#define RANDO_STRINGS_TRIFORCE 39

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

/* Caller-owned, no pointers: every post-fill choice, as ids and indices, for the ROM writer. */
typedef struct RandoPostFill {
	RandoTexts texts;
} RandoPostFill;

void rando_postfill_clear(RandoPostFill *post);

/* Randomizer::setTexts with spoil.BootsLocation off. Draws 55 + 77 + 51 + 31, then min(2, progressive
 * bows in the world), then 36 on the fake branch, then 39. */
void rando_set_texts(const RandoWorld *world, RandoPrng *prng, RandoPostFill *post);

#endif
