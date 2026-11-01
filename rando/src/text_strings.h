#ifndef RANDO_TEXT_STRINGS_H
#define RANDO_TEXT_STRINGS_H

#include "rando_postfill.h"

/* Each holds exactly its RANDO_STRINGS_* count of entries, checked at compile time where defined. */
extern const char *const rando_strings_uncle[];
extern const char *const rando_strings_tavern_man[];
extern const char *const rando_strings_blind[];
extern const char *const rando_strings_ganon_1[];
extern const char *const rando_strings_ganon_phase_3_no_silvers[];
extern const char *const rando_strings_triforce[];
extern const char *const rando_strings_hint[];

/* hint.<kind>.<name> in hint.php: names NULL for a missing key, else count 0 for a plain string (names[0])
 * or the array's length. */
typedef struct RandoHintName {
	const char *const *names;
	int count;
} RandoHintName;

extern const RandoHintName rando_hint_items[RANDO_ITEM_COUNT];
extern const RandoHintName rando_hint_locations[RANDO_LOCATION_COUNT];

/* Each item's tablet or pedestal text; NULL for VT's default. */
extern const char *const rando_text_ether[RANDO_ITEM_COUNT];
extern const char *const rando_text_bombos[RANDO_ITEM_COUNT];
extern const char *const rando_text_pedestal[RANDO_ITEM_COUNT];

#endif
