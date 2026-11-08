#ifndef LADX_BACKGROUND_H
#define LADX_BACKGROUND_H

#include <stddef.h>
#include <stdint.h>

#define LADX_BACKGROUND_ADDR_SPAN 0x10800u

typedef struct LadxBackground {
	uint8_t tile[LADX_BACKGROUND_ADDR_SPAN];
	uint8_t present[LADX_BACKGROUND_ADDR_SPAN / 8];
	uint32_t low, high;
	uint32_t count;
} LadxBackground;

int ladx_background_decode(LadxBackground *bg, const uint8_t *data, size_t n);

int ladx_background_set(LadxBackground *bg, uint32_t addr, uint8_t value);

int ladx_background_get(const LadxBackground *bg, uint32_t addr, uint8_t *value);

size_t ladx_background_encode(const LadxBackground *bg, uint8_t *out, size_t cap);

#endif
