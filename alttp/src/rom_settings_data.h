#ifndef RANDO_ROM_SETTINGS_DATA_H
#define RANDO_ROM_SETTINGS_DATA_H

#include "rando_world.h"

#include <stdint.h>

#define WISHING_WELL_ROOM_SIZE 205

extern const uint8_t rando_wishing_well_room[WISHING_WELL_ROOM_SIZE];

typedef struct RomShop {
	uint8_t config;
	uint8_t shopkeeper;
	uint16_t room_id;
	uint8_t door_id;
	uint32_t extra_address;
	uint8_t extra_byte;
} RomShop;

#define UNCLE_ROW_SIZE 6
#define UNCLE_SWORD_ROWS 10
#define UNCLE_SHIELD_ROWS 7

typedef struct RomRow {
	uint32_t address;
	uint8_t bytes[UNCLE_ROW_SIZE];
} RomRow;

extern const RomRow rando_uncle_sword_rows[UNCLE_SWORD_ROWS];
extern const RomRow rando_uncle_shield_rows[UNCLE_SHIELD_ROWS];

#define RETRO_DIG_PRIZES_SIZE 64

extern const uint8_t rando_retro_dig_prizes[RETRO_DIG_PRIZES_SIZE];

extern const RomShop rando_rom_shops[RANDO_SHOP_COUNT];

#define INVERTED_WRITES 193
#define INVERTED_BYTES 510

typedef struct RomWrite {
	uint32_t address;
	uint16_t offset;
	uint8_t size;
} RomWrite;

extern const RomWrite rando_inverted_writes[INVERTED_WRITES];
extern const uint8_t rando_inverted_bytes[INVERTED_BYTES];

#endif
