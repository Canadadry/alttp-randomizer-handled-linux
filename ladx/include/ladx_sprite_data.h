#ifndef LADX_SPRITE_DATA_H
#define LADX_SPRITE_DATA_H

#define LADX_SPRITE_PAIRS 4
#define LADX_SPRITE_VALUES 2

typedef enum LadxSpriteKind {
	LADX_SPRITE_ABSENT = 0,
	LADX_SPRITE_NONE,
	LADX_SPRITE_DATA
} LadxSpriteKind;

typedef struct LadxSpritePair {
	int slot;
	int count;
	int values[LADX_SPRITE_VALUES];
} LadxSpritePair;

typedef struct LadxSpriteData {
	int count;
	LadxSpritePair pair[LADX_SPRITE_PAIRS];
} LadxSpriteData;

LadxSpriteKind ladx_sprite_data(int entity, int room, int evilshop, LadxSpriteData *out);

#endif
