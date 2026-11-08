#include "sdl_rotate.h"

#include <stdint.h>

bool sdl_rotate_180(SDL_Surface *src, SDL_Surface *dst)
{
	int x, y;

	if (src == NULL || dst == NULL || src == dst || src->w != dst->w || src->h != dst->h ||
	    src->format->BytesPerPixel != 4 || dst->format->BytesPerPixel != 4)
		return false;
	if (SDL_MUSTLOCK(src) && SDL_LockSurface(src) != 0)
		return false;
	if (SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) != 0) {
		if (SDL_MUSTLOCK(src))
			SDL_UnlockSurface(src);
		return false;
	}
	for (y = 0; y < src->h; y++) {
		const uint32_t *in = (const uint32_t *)((const uint8_t *)src->pixels + (size_t)y * src->pitch);
		uint32_t *out = (uint32_t *)((uint8_t *)dst->pixels + (size_t)(dst->h - 1 - y) * dst->pitch);

		for (x = 0; x < src->w; x++)
			out[dst->w - 1 - x] = in[x];
	}
	if (SDL_MUSTLOCK(dst))
		SDL_UnlockSurface(dst);
	if (SDL_MUSTLOCK(src))
		SDL_UnlockSurface(src);
	return true;
}
