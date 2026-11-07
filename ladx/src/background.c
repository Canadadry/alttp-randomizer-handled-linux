#include "ladx_background.h"

#include <string.h>

static int present(const LadxBackground *bg, uint32_t addr)
{
	return addr < LADX_BACKGROUND_ADDR_SPAN && (bg->present[addr / 8] >> (addr % 8) & 1);
}

int ladx_background_get(const LadxBackground *bg, uint32_t addr, uint8_t *value)
{
	if (!present(bg, addr))
		return 0;
	*value = bg->tile[addr];
	return 1;
}

int ladx_background_set(LadxBackground *bg, uint32_t addr, uint8_t value)
{
	if (addr >= LADX_BACKGROUND_ADDR_SPAN)
		return 0;
	if (!present(bg, addr)) {
		bg->present[addr / 8] |= (uint8_t)(1u << (addr % 8));
		if (bg->count == 0 || addr < bg->low)
			bg->low = addr;
		if (bg->count == 0 || addr > bg->high)
			bg->high = addr;
		bg->count++;
	}
	bg->tile[addr] = value;
	return 1;
}

int ladx_background_decode(LadxBackground *bg, const uint8_t *data, size_t n)
{
	size_t idx = 0;
	uint32_t addr, amount, k;
	int repeat, vertical;

	memset(bg->present, 0, sizeof bg->present);
	bg->count = 0;
	bg->low = bg->high = 0;
	for (;;) {
		if (idx >= n)
			return 0;
		if (data[idx] == 0x00)
			return 1;
		if (idx + 3 > n)
			return 0;
		addr = (uint32_t)data[idx] << 8 | data[idx + 1];
		amount = (data[idx + 2] & 0x3Fu) + 1;
		repeat = (data[idx + 2] & 0x40) != 0;
		vertical = (data[idx + 2] & 0x80) != 0;
		idx += 3;
		for (k = 0; k < amount; k++) {
			if (idx >= n)
				return 0;
			ladx_background_set(bg, addr, data[idx]);
			if (!repeat)
				idx++;
			addr += vertical ? 0x20u : 0x01u;
		}
		if (repeat)
			idx++;
	}
}

size_t ladx_background_encode(const LadxBackground *bg, uint8_t *out, size_t cap)
{
	uint32_t low, high, diff, same, k;
	size_t len = 0;

	if (bg->count == 0)
		return 0;
	low = bg->low;
	high = bg->high + 1;
	while (low < high) {
		if (!present(bg, low)) {
			low++;
			continue;
		}
		if (low > 0xFFFFu)
			return 0;
		diff = 1;
		while (present(bg, low + diff) && diff < 0x40)
			diff++;
		same = 1;
		while (present(bg, low + same) && bg->tile[low] == bg->tile[low + same] && same < 0x40)
			same++;
		if (same + 4 > diff && same > 2) {
			if (len + 4 > cap)
				return 0;
			out[len++] = (uint8_t)(low >> 8);
			out[len++] = (uint8_t)(low & 0xFF);
			out[len++] = (uint8_t)((same - 1) | 0x40);
			out[len++] = bg->tile[low];
			low += same;
		} else {
			if (len + 3 + diff > cap)
				return 0;
			out[len++] = (uint8_t)(low >> 8);
			out[len++] = (uint8_t)(low & 0xFF);
			out[len++] = (uint8_t)(diff - 1);
			for (k = 0; k < diff; k++)
				out[len++] = bg->tile[low + k];
			low += diff;
		}
	}
	if (len + 1 > cap)
		return 0;
	out[len++] = 0x00;
	return len;
}
