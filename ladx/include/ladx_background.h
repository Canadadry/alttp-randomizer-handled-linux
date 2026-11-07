#ifndef LADX_BACKGROUND_H
#define LADX_BACKGROUND_H

#include <stddef.h>
#include <stdint.h>

/*
 * LADXR's BackgroundEditor (backgroundEditor.py at the pin): a background
 * entry's runs decoded into a tile per VRAM address, edited, and encoded
 * back by its greedy store encoder. Caller-owned, no allocation.
 */

/* Past the highest address a run reaches: 0xFFFF plus 63 vertical steps of 0x20. */
#define LADX_BACKGROUND_ADDR_SPAN 0x10800u

typedef struct LadxBackground {
	uint8_t tile[LADX_BACKGROUND_ADDR_SPAN];
	uint8_t present[LADX_BACKGROUND_ADDR_SPAN / 8];
	uint32_t low, high; /* the lowest and highest present address, when count > 0 */
	uint32_t count;
} LadxBackground;

/*
 * Clears bg and reads data's runs until their 0x00 terminator. 0 when a
 * read passes data[n-1] (LADXR's IndexError).
 */
int ladx_background_decode(LadxBackground *bg, const uint8_t *data, size_t n);

/* addr below LADX_BACKGROUND_ADDR_SPAN (else nothing changes and 0). */
int ladx_background_set(LadxBackground *bg, uint32_t addr, uint8_t value);

/* tiles[addr] into *value; 0 when addr is absent (Python's KeyError). */
int ladx_background_get(const LadxBackground *bg, uint32_t addr, uint8_t *value);

/*
 * BackgroundEditor.store's bytes into out, terminator included: their
 * count, or 0 when bg is empty (LADXR's min() of an empty dict), an
 * address over 0xFFFF starts a run (bytearray's ValueError), or over cap.
 */
size_t ladx_background_encode(const LadxBackground *bg, uint8_t *out, size_t cap);

#endif
