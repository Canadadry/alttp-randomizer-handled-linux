#ifndef LADX_SPRITE_DATA_H
#define LADX_SPRITE_DATA_H

/*
 * entityData.py's SPRITE_DATA at the pin (tests/ladx/README.md,
 * "Pipeline", enemizer): for an entity in a room, the sprite slots it
 * needs, each with the values it accepts, lambdas resolved on the room.
 */

#define LADX_SPRITE_PAIRS 4
#define LADX_SPRITE_VALUES 2

typedef enum LadxSpriteKind {
	LADX_SPRITE_ABSENT = 0, /* not a SPRITE_DATA key */
	LADX_SPRITE_NONE,	/* None, or a lambda returning None */
	LADX_SPRITE_DATA
} LadxSpriteKind;

typedef struct LadxSpritePair {
	int slot;
	int count; /* 1 or 2 values */
	int values[LADX_SPRITE_VALUES];
} LadxSpritePair;

typedef struct LadxSpriteData {
	int count; /* pairs, in LADXR's order */
	LadxSpritePair pair[LADX_SPRITE_PAIRS];
} LadxSpriteData;

/* evilshop: addEvilShop's SPRITE_DATA[0x77] = SPRITE_DATA[0x4D]. *out set only for LADX_SPRITE_DATA. */
LadxSpriteKind ladx_sprite_data(int entity, int room, int evilshop, LadxSpriteData *out);

#endif
